# Zenith2 Architecture

The architecture of Zenith2: what lives where, what depends on what, how the engine is built, how a frame is scheduled and owned, how data-defined components, the ECS, worlds and the composition root work, and the platform, memory, security, determinism and multiplayer foundations underneath. Normative statements are tagged `[Z2-N-ARCH-NNN]`; the numbering is shared by this entry document and its split files under `Zenith2/Docs/Architecture/`.

## Purpose and scope

### What this document binds

This document is the architecture specification that every Zenith2 design document (`Zenith2/Docs/Design/<CODE>.md`), requirement catalogue and ticket builds on. The engineering contract every type obeys is `Zenith2/Docs/Contracts.md`; rendering is `Zenith2/Docs/Rendering.md`; scripting is `Zenith2/Docs/Scripting.md`; numbers are `Zenith2/Docs/Budgets.md`. Where this document names a type, function, file, flag, format or test, that name is the one implementations use.

- [Z2-N-ARCH-001] Zenith2 code conforms to this document family (this entry document and every file under `Zenith2/Docs/Architecture/`); a ticket, design document or code change that contradicts it is reported as a contradiction and changes nothing that picks a side until the document is revised in the same diff as the change (Zenith2/Docs/Contracts.md → "Review and change management").
- [Z2-N-ARCH-002] A change to any contract, API, format or rule specified here edits the affected section of this document family in the same diff, and a new or changed ruling is appended to `Zenith2/Docs/DecisionLog.md` in that diff with an explicit `Z2-D-NNN` (Z2-D-085, Z2-D-097).

### Document family

| File | Section it specifies | Its `##` headings |
|---|---|---|
| `Zenith2/Docs/Architecture.md` (this file) | every section below; the split sections carry a contract summary here | the headings of this file |
| `Zenith2/Docs/Architecture/BuildSystem.md` | Build system and presets | Settled rules governing the build · Top-level tree and helper modules · The zenith_add_library helper · Compile and link flags · Presets · Toolchain pins and the setup script · Third-party integration · Games in CMake · The linux-tsan host · Gate-budget rules that shape the build · Hygiene gates fed by the build |
| `Zenith2/Docs/Architecture/ZenithGraph.md` | ZenithGraph shared core | Role of the graph core · Public API · Hazard derivation · Cycle detection and diagnostics · Ordering, priorities and dependency counts · Lifetime intervals · Structure hash · Recompilation and caching support · JSON dump · Complexity and capacity · Conformance suite |
| `Zenith2/Docs/Architecture/CPUFrameGraph.md` | CPU frame graph and scheduler | Settled rules and ownership · Frame structure · Job system · Dynamic sub-jobs and continuations · Frame graph builder · Schedule compilation and caching · Execution · Render extraction and pipelining · Worlds in the graph · Cancellation and shutdown · Profiling · Required tests |
| `Zenith2/Docs/Architecture/FramePhases.md` | Frame phases and authority | Settled rules and ownership · The twelve phases · Sync points and the tick boundary · Per-field authority · Commands · Arbitration rules · Schedule-compile errors · Required tests |
| `Zenith2/Docs/Architecture/LifecycleCommands.md` | Lifecycle commands | Settled rules and ownership · Lifecycle command summary · Listener interface · Spawn · Destroy · Teleport · Ragdoll enter and recover · Cinematic start, interrupt and skip · World transition · Shutdown · Scenario tests |
| `Zenith2/Docs/Architecture/DataDefinedComponents.md` | Data-defined components | Settled rules and ownership · The schema file · Registration at game load · Storage in every backend · Generated Luau types · Migration · Required tests |
| `Zenith2/Docs/Architecture/ECSModel.md` | ECS model | Settled rules and ownership · Worlds and backends · Entity identity · Command buffers · Sync points and the structural epoch · Views · Queries · Deterministic reductions · Type-erased API surface · World registry · Scheduling contract · Script lifetimes on the ECS side · Diagnostics · Required tests |
| `Zenith2/Docs/Architecture/CompositionRoot.md` | Composition root and Player | Settled rules and ownership · Player layout · The generated composition table · Startup sequence · The frame loop · Android packaging · Required tests |
| `Zenith2/Docs/Architecture/PlatformLayer.md` | Platform layer | Settled rules and ownership · SDL3 responsibilities · Android lifecycle and surface recovery · Thermal adaptation · Virtual filesystem · Asynchronous I/O · Configuration layering · Account changes · The headless test host · Required tests |

Normative ID ranges: 001–019 this section; 021–069 "Repository layout"; 071–119 "Library dependency graph"; 121–149 "Third-party dependencies"; 151–259 BuildSystem.md; 261–309 "Engine modules and game projects"; 311–379 ZenithGraph.md; 381–469 CPUFrameGraph.md; 471–539 FramePhases.md; 541–589 LifecycleCommands.md; 591–639 DataDefinedComponents.md; 641–679 "Reflection and runtime type registration"; 681–759 ECSModel.md; 761–779 "Worlds and isolation"; 781–809 "Multiplayer foundations"; 811–839 "Memory governor"; 841–880 CompositionRoot.md; 881–909 "Determinism guarantee and mechanisms"; 911–949 PlatformLayer.md; 951–979 "Security architecture"; 981–999 "Named selection decisions".

### Scope

Zenith2 is a general-purpose engine for single-player production first. Every scope boundary is a recorded decision:

| Boundary | Classification | Decision |
|---|---|---|
| Windows (Vulkan and D3D12, chosen at run time) and Android arm64, validated on the Galaxy S24 Ultra alone; other Adreno devices best-effort; Mali dropped; minSdk 34; 16 KB pages | required | Z2-D-010 |
| C++20 | required | Z2-D-009 |
| 2D, first class | required | Z2-D-078 |
| online-services adapter with an offline implementation (Stage 4); Steam and Google Play backends (Stage 6) | required / deferred | Z2-D-074 |
| consoles | deferred to Stage 7, gated on SDK access | Z2-D-071 |
| VR/XR through OpenXR; multi-view and input kept XR-ready | deferred to Stage 7 | Z2-D-072 |
| networked multiplayer and dedicated servers, with Stage 1 foundations and a Stage 3 loopback proof | deferred to Stage 7 | Z2-D-073 |
| modding | excluded | Z2-D-075 |
| local multiplayer and split-screen | excluded | Z2-D-076 |
| quest, dialogue, inventory and interaction systems | game-side Luau modules in OpenWorldSlice | Z2-D-077 |
| hair and groom | excluded | Z2-D-078 |
| visual authoring for animation, terrain, UI and cinematics; code authoring with previews for materials and VFX | required | Z2-D-079 |
| three equally binding flagship slices: OpenWorldSlice, CinematicSlice, Game2D | required | Z2-D-021 |

- [Z2-N-ARCH-003] No engine library implements a capability classified excluded above, and no engine library implements quest, dialogue, inventory or interaction rules; those exist as Luau gameplay modules inside `Games/OpenWorldSlice/` (Z2-D-077).
- [Z2-N-ARCH-004] Deferred capabilities are designed now so that their stage adds code without changing the Stage 1–6 contracts: render views keep a stereo view kind reserved (Zenith2/Docs/Rendering.md → "Render views"), input goes through actions and contexts, and simulation is fixed-tick with replayable commands and network IDs (§ "Multiplayer foundations").

### Conventions used here

- Normative statements are one-obligation tagged bullets; rationale, comparisons and prior art sit in informative blocks; tables and code blocks restate obligations and introduce none of their own (Z2-D-066).
- A line `<!-- elaboration -->` marks a detail designed for Zenith2 beyond what the approved programme plan stated; reviewers look at those first.
- Settled decisions are cited as `Z2-D-NNN`; open decisions by name with "(open, Stage N)".
- Other core documents are cited by path and exact `##` heading, e.g. Zenith2/Docs/Contracts.md → "Ownership and allocation". Headings of this family are cited the same way, with the split file's path.
- Error types take the two forms of Zenith2/Docs/Contracts.md → "Errors and results": an enumeration numbered from 1, or a record of at most 32 bytes holding the code and fixed-size context (an index, an ID, a count) and no text; where one failure yields several diagnostics with text, the record carries the first code and the count, and the diagnostics are read from the object that reported them, as its declaration states.
- Names follow Zenith Hungarian (Z2-D-034): types `Zenith_*`, renderer types `Flux_*`, members `m_`, type prefixes `x p px a u ul i b f str sz e pfn`, enums `CATEGORY_VALUE`, constants `uFOO`.

### Assumed types

<!-- elaboration -->
This family uses these types without declaring them. Zenith2/Docs/Contracts.md declares `Zenith_PfnMigrationStep`, `Zenith_Result`, `Zenith_Owned`, `Zenith_Arena`, `Zenith_Heap`, `Zenith_ByteSpan`, `Zenith_MutableByteSpan`, `Zenith_PersistentID`, `Zenith_EntityHandle`, `Zenith_AssetHandle`, `Zenith_OperationHandle`, `Zenith_Semaphore`, `Zenith_FaultInjector` and the threading primitives; the rest are declared by the design documents named (`Zenith2/Docs/Design/CORE.md` also carries every governor type's full contract block, algorithms and counters; this document states the contract):

| Type | Semantics assumed here | Declared by |
|---|---|---|
| `Zenith_Span<T>` | a non-owning (pointer, count) view over contiguous `T`, no allocation | `Zenith2/Docs/Design/CORE.md` |
| `Zenith_StringView` | a non-owning (pointer, byte length) view over UTF-8 text, not NUL-terminated | `Zenith2/Docs/Design/CORE.md` |
| `Zenith_JsonWriter` | a streaming JSON writer producing byte-stable output (backing per D-SEL-JSON, open, Stage 1) | `Zenith2/Docs/Design/CORE.md` |
| `Zenith_Hash64` | the 64-bit hash function selected by D-SEL-HASH (open, Stage 1), seed 0 | `Zenith2/Docs/Design/CORE.md` |
| `Zenith_StringID` | a 4-byte interned-string identifier, a plain value (Architecture/DataDefinedComponents.md → "The schema file") | `Zenith2/Docs/Design/CORE.md` |
| `Zenith_Vector3`, `Zenith_Quaternion` | `f32` vector and quaternion value types | `Zenith2/Docs/Design/CORE.md` |
| `Zenith_Transform` | local position, rotation, scale value type | `Zenith2/Docs/Design/SCENE.md` |
| `Zenith_Config` | the layered-configuration facade over the configuration sources | this family: Architecture/PlatformLayer.md → "Configuration layering" |
| `Zenith_ConfigValue`, `Zenith_ConfigKeyDesc`, `Zenith_ConfigProvenance`, `Zenith_ConfigLayer`, `Zenith_ConfigKey`, `Zenith_ConfigError`, `Zenith_ConfigSchema`, `Zenith_ConfigStore` | the configuration value types, key descriptors, provenance, layers, errors, key schema and layered store (ZenithCore) | `Zenith2/Docs/Design/CORE.md` |
| `Zenith_Scene`, `Zenith_Prefab` | the scene and prefab asset types used as `Zenith_AssetHandle` arguments | `Zenith2/Docs/Design/SCENE.md` |
| `Zenith_AnimationClip` | the animation-clip asset type | `Zenith2/Docs/Design/ANIM.md` |
| `Zenith_MemoryGovernorStats`, `Zenith_OOMPressureFlag` | the governor's per-frame statistics (level, process, device-local and unattributed bytes, per-subsystem usage, reserved peaks, event counts); the relay the OOM handler raises and the governor consumes as a CRITICAL source | `Zenith2/Docs/Design/CORE.md` |
| `Zenith_StringTable`, `Zenith_LogRouter` | the process interned-string table; the structured-log router (created by the Player in startup step 1b, `uLOG_RING_BYTES_DEFAULT`, `FillHooks`, `StartDrainThread`) | `Zenith2/Docs/Design/CORE.md` |
| `Zenith_JsonWriteError` | the error of a `Zenith_JsonWriter` call (stream failure, depth exceeded) | `Zenith2/Docs/Design/CORE.md` |
| `Zenith_TypeLayout`, `Zenith_SerializedFieldSet`, `Zenith_MigrationChain`, `Zenith_MigrationContext`, `Zenith_MigrationError` | a type's computed in-memory layout; a property-ID-keyed value set; a type's ordered migration steps; the context and error of one migration | `Zenith2/Docs/Design/ECS.md` (Reflect) |
| `Zenith_AssetRegistry` | the asset system's registry of shared asset handles | `Zenith2/Docs/Design/ASSET.md` |
| `Zenith_AssetLoader`, `Zenith_ArchiveReaderDesc`, `Zenith_CreateArchiveMountSource`, `Zenith_RegisterAssetsConfigKeys`, `Zenith_RegisterAssetTypes`, `Zenith_GetAssetHandleValueKindHooks`, `Zenith_ResolveAssetReferenceDefault` | the asset loader (`ServiceFrame`, `ServiceUntilComplete`), the `.zpak` archive reader's factory and descriptor, and ZenithAssets' startup entries | `Zenith2/Docs/Design/ASSET.md` |
| `Zenith_SceneLoader`, `Zenith_SceneLoadResult` | the scene loader (`StepActivations`, `BeginPrepare`) and the 8-byte additive-load publication payload | `Zenith2/Docs/Design/SCENE.md` |
| `Zenith_ScriptRuntime` (including `ApplyPendingReloads(Zenith_TypeRegistry&)` and `CommitPendingReloads()`, both called in frame-prologue step 6, the second in every frame) | the Luau runtime of all worlds | Zenith2/Docs/Scripting.md → "VM model and threading" |
| `Zenith_ScriptBindingTable`, `Zenith_ScriptBindingError`, `Zenith_SideEffectKind`, `Zenith_ScriptSideEffectConsumer` | the Luau-free binding table (`ZenithReflect/Zenith_ScriptBinding.h`), its `Seal` error, the side-effect kinds and the consumer interface of SCRIPT-held side effects | Zenith2/Docs/Scripting.md → "Modules, bindings and generated types" |
| `Zenith_AsyncResultTable` | the per-world table of asynchronous results (completion IDs, source records, tick-boundary publication), owned by each `Zenith_ScriptWorld` (`ZenithScript/Zenith_ScriptAsync.h`) | Zenith2/Docs/Scripting.md → "Merge-time capacity and asynchronous results" |
| `Flux_Renderer`, `Flux_RenderWorldWriter`, `Zenith_RHIDevice`, `Zenith_RHIDeviceDesc`, `Zenith_RHIError` | the renderer (including `RunPrologue()`, called in frame-prologue step 4, and the polled `GetDeviceLossState()`); the per-extractor render-world writer partition; the RHI device, its descriptor and error | Zenith2/Docs/Rendering.md → "Purpose and scope" and → "GPU baseline and profiles" |
| `Flux_DeviceLossState` | the device-loss handler's state (`FLUX_DEVICE_LOSS_STATE_NORMAL`, …), declared in `ZenithFlux/Flux_QueueValues.h` | Zenith2/Docs/Rendering.md → "Recovery" |
| `Zenith_Compression`, `Zenith_CompressionDesc`, `Zenith_CompressionCodecSet` | the compression interface, its descriptor, and the codec set through which readers select a codec from the data (`Zenith_CompressionError` is Contracts') | `Zenith2/Docs/Design/COMPRESS.md` |
| `Zenith_OnlineBackendEntry`, `Zenith_PfnCreateOnlineServices` | one linked online-services backend (`{ const char* m_szName; Zenith_PfnCreateOnlineServices m_pfnCreate; }`) and its factory type, declared in `ZenithOnline/Zenith_OnlineServices.h` | `Zenith2/Docs/Design/ONLINE.md` |
| `Zenith_ScriptRecoveryPolicy` | the per-game script recovery policy | Zenith2/Docs/Scripting.md → "Quarantine and hot reload" |
| `Zenith_Physics`, `Zenith_PhysicsDesc`, `Zenith_PhysicsError`, `Zenith_PhysicsWorldResource` | the physics interface, its descriptor and error, and the world resource systems declare | `Zenith2/Docs/Design/PHYS.md` |
| `Zenith_AudioCommandQueue` | the per-world audio command world resource read by `Audio.Update` | `Zenith2/Docs/Design/AUDIO.md` |
| `Zenith_NetworkIDMap` | a world's map from network ID to entity handle | `Zenith2/Docs/Design/NET.md` |
| `Zenith_Platform`, `Zenith_PlatformDesc`, `Zenith_PlatformEvent`, `Zenith_PlatformEventKind`, `Zenith_ConfigDesc`, `Zenith_ConfigSourceError`, `Zenith_CommandLineFileGrant`, `Zenith_OpenCommandLineHostFile`, `Zenith_Window`, `Zenith_VirtualFileSystem`, `Zenith_ThermalMonitor`, `Zenith_PlatformAccountPayload`, `Zenith_PfnAccountConsumer` | the platform layer's objects (the account consumer type is `void (*)(const Zenith_PlatformAccountPayload& xAccount, void* pUserData)`); this family relies on `Zenith_Platform::Create(const Zenith_PlatformDesc&)` (startup step 4), `Zenith_Platform::Pump()` (prologue step 1) and `Zenith_ThermalMonitor::GetLatestSample()` (prologue step 5) | `Zenith2/Docs/Design/PLAT.md` |
| `Zenith_Cinematic`, `Zenith_CinematicBinding`, `Zenith_CinematicTicket`, `Zenith_CinematicInstanceID` | the cinematic asset and its instance identities | `Zenith2/Docs/Design/CINE.md` |

- [Z2-N-ARCH-005] A type this family uses and does not declare is either declared by Zenith2/Docs/Contracts.md or listed in the table "Assumed types" with the design document that declares it, and the owning design document adopts the name and semantics given there or raises a contradiction.

## Repository layout

### The tree

```text
Zenith2/
  CLAUDE.md                     binding rules for everything under Zenith2/ (protected)
  CMakeLists.txt                the root build (Architecture/BuildSystem.md → "Top-level tree and helper modules")
  CMakePresets.json             every configure, build and test preset
  .gitignore                    ignores out/
  .clang-format  .clang-tidy    pinned formatting and static-analysis configuration
  CMake/                        build helpers and toolchain files
  ThirdParty/                   manifest.json, patches/, <name>/CMakeLists.txt wrappers
  Engine/
    ZenithCore/                 ownership, heap/arenas/pools, containers, strings, maths, threading, results,
                                asserts, logging, clocks, random streams, hashing, JSON, the configuration
                                value types, schema and store, the memory governor
    ZenithGraph/                the shared dependency-graph core
    ZenithProfiling/            zones, counters, the profiler hooks, the on-screen profiler data model, Tracy
    ZenithReflect/              the type registry, runtime type registration, schemas, phases, migration
    ZenithDocument/             the authoring-document migration framework and canonical serialization (no host file I/O)
    ZenithVertexFormat/         the CPU vertex codec and generated layout-table types
    ZenithCompression/          the neutral compression API
    ZenithCompression_Zlib/     zlib backend
    ZenithCompression_Zstd/     zstd backend
    ZenithJobs/                 job system, CPU frame graph, simulation clock
    ZenithPlatform/             SDL3 platform layer, virtual filesystem, async I/O, configuration sources, thermal
    ZenithRHI/                  the neutral RHI API
    ZenithRHI_Vulkan/           Vulkan backend (volk, Vulkan-Profiles, VMA)
    ZenithRHI_D3D12/            D3D12 backend (Agility SDK, D3D12MA)
    ZenithAssets/               asset registry, handles, cooked-format readers, residency, streaming requests
    ZenithPackage/              packages, update installs, the verified cache, Play Asset Delivery, the package service
    ZenithECS/                  the ECS API, world simulation, commands, queries, views, systems
    ZenithECS_Custom/           custom archetype backend
    ZenithECS_Flecs/            flecs backend
    ZenithECS_EnTT/             EnTT backend with the dynamic-storage adapter
    ZenithScene/                transforms, hierarchy, scenes, prefabs, lifecycle listeners and commands
    ZenithShadersGenerated/     generated shader headers, blobs and layout tables (INTERFACE target; no sources)
    ZenithFlux/                 the renderer: render graph, render world, features, GPU scene
    ZenithSave/                 saves, checkpoints, the replay log format
    ZenithScript/               Luau runtime, the exception firewall, bindings
    ZenithScriptCompiler/       the Luau compiler and the D-LUAU-STATELESS (open, Stage 1) check (tool side; never in a Player)
    ZenithRuntime/              the composition root: startup, frame loop, worlds, module interface
    Zenith2D/                   module: sprites, tilemaps, 2D cameras, 2D physics per D-SEL-2DPHYS (open, Stage 2); the 2D GPU side is TWOD's files in ZenithFlux (`Flux_Sprite2D.h`, `Internal/Sprite2D/`)
    ZenithPhysics/              module: the neutral physics API and its ECS integration
    ZenithPhysics_Jolt/         Jolt backend
    ZenithPhysics_PhysX/        PhysX 5 backend
    ZenithAnimation/            module: skeletons, graphs, IK, root motion
    ZenithAudio/                module: miniaudio-based audio
    ZenithAI/                   module: navigation, perception, agents
    ZenithUI/                   module: retained UI, text, localization, accessibility
    ZenithOnline/               module: the online-services adapter API
    ZenithOnline_Offline/       the offline online-services implementation
    ZenithOnline_SteamCore/     the Steam backend's store-independent logic and scripted seams (Stage 6)
    ZenithOnline_GooglePlayCore/ the Google Play backend's store-independent logic and scripted seams (Stage 6)
    ZenithOnline_Steam/         Steam backend over the Steamworks SDK (Stage 6)
    ZenithOnline_GooglePlay/    Google Play Games and Play Billing backend (Stage 6)
    ZenithNet/                  module: network IDs, authority, the loopback transport
    ZenithStreaming/            module: world partitioning and cell streaming
    ZenithGameplay/             module: cameras, tags, timers, ability and state primitives
    ZenithCinematics/           module: timelines, tracks, cinematic arbitration
    ZenithVFX/                  module: particle and effect components (GPU side in ZenithFlux)
    ZenithEnvironment/          module: terrain, foliage, water, atmosphere, weather components
    ZenithXR/                   module: OpenXR integration (Stage 7, Windows PC VR)
  Player/                       the shared Player main and the Android Gradle template
  Shaders/                      Slang sources, compiled through ZenithShadersGenerated: Common/ and one directory per consuming library (Flux, each rendering module, the path tracer, and `Editor/` for ZenithEditor)
  Tools/
    ZenithEditor/               the editor
    EditorCore/                 ZenithEditorCore, the editor's session, documents, commands, history, selection and projection (tool library, no ImGui)
    Authoring/                  ZenithAuthoring, the authoring-side code ZenithCook and the editor both call (tool library; one Internal/<Dir>/ per owner)
    DevTool/                    ZenithDevTool, the development host CLI (DEVT)
    ZenithCook/                 the cook: importers, packers, schema and type generation
    ContentCore/                ZenithContentCore, the source parsers and decoders, the merge engine and journal codecs (tool library)
    ZenithContent/              the content command-line tool: merge, batch operations, storage and cache administration
    Package/                    PACK's packaging scripts (PowerShell and CMake script mode)
    ZenithShaderCompiler/       the offline Slang/DXC compiler
    NewGame/                    ZenithNewGame, the game scaffolding CLI
    NewGameScaffold/            ZenithNewGameScaffold, the scaffolding library the CLI and the editor wizard link
    ZenithPathTracer/           the compute path tracer for physical-accuracy validation (test-only static library)
    DocumentHost/               ZenithDocumentHost, the host-filesystem document storage the tools link (tool library)
    Gates/                      gate step scripts, profiles, baselines, allowlists (protected)
    Setup/                      Setup-Zenith2.ps1, Enter-Zenith2Env.ps1, Zenith2Env.psm1
    Backlog/                    File-Backlog.ps1 and the backlog tooling
    Maintenance/                engine versioning, changelog, API surface, ownership and rehearsal scripts and data (MAINT)
    Security/                   security registries, triage and ceremony scripts and the key ceremony tool (no compiled source, no gate logic)
  Benchmarks/<Harness>/         measurement harnesses (research, budgets, decision harness tickets)
  Benchmarks/Budgets/<AREA>/    the per-area budget harnesses (the group directory Budgets holds no CMakeLists.txt)
  Benchmarks/Ed25519/           the Ed25519 implementation selection harness (SEC)
  Tests/<Suite>/                integration suites that link the runtime and modules (Lifecycle, Determinism, Security, …)
  Games/
    WorkloadIndoor/  WorkloadOutdoor/  WorkloadCharacters/  WorkloadDense/  WorkloadUI2D/
    OpenWorldSlice/  CinematicSlice/  Game2D/  Minimal/
    <Name>/                     game.json, CMakeLists.txt, Scripts/, Scenes/, Schemas/, Assets/
  Docs/
    Architecture.md  Architecture/  Rendering.md  Scripting.md  Contracts.md  Budgets.md
    Roadmap.md  DecisionLog.md  Coverage.md  WorkerGuide.md  QualityMatrix.md  ContentSpecs.md
    Design/<CODE>.md            Requirements/{Inventory.md, Normative.md, <CODE>.md}
    Research/                   committed raw data and reports of measurement rulings (elaboration: the plan's "Docs/Research/")
    Board/                      Vault records only
  out/                          build trees, generated files, cooked content, gate reports (ignored)
```

<!-- elaboration -->
The plan's library list is kept whole. `ZenithShadersGenerated` (the owner of generated shader code) and `ZenithScriptCompiler` (the compile-time half of the Luau firewall) are added by the Rendering and Scripting documents. Eight more libraries are added so that every area code has a home: `ZenithSave` (SAVE), `ZenithStreaming` (STREAM), `ZenithGameplay` (GAME), `ZenithCinematics` (CINE), `ZenithVFX` (VFX), `ZenithEnvironment` (ENV), `ZenithPackage` (PACK) and `ZenithOnline_Offline` (the offline implementation of ONLINE as its own backend target, following the one-library-per-backend rule of Z2-D-001). `Tests/` holds integration suites that need several modules at once, and `CMake/` the build helpers.

### Ownership per directory

| Directory | Kind | Owning area |
|---|---|---|
| `Engine/ZenithCore/` | engine library | CORE (`Zenith_EngineVersion.h`, `Internal/Zenith_EngineVersion.cpp`, `Tests/Zenith_EngineVersion_Tests.cpp`: MAINT) |
| `Engine/ZenithGraph/` | engine library | GRAPH |
| `Engine/ZenithProfiling/` | engine library | DEVT |
| `Engine/ZenithReflect/` | engine library | ECS |
| `Engine/ZenithDocument/` | engine library | SCENE (the `.meta` document type and its importer-settings chains: ASSET) |
| `Engine/ZenithVertexFormat/` | engine library | GEOM |
| `Engine/ZenithCompression/`, `_Zlib/`, `_Zstd/` | engine API library and backends | COMPRESS |
| `Engine/ZenithJobs/` | engine library | JOBS (the CPU frame graph within it: GRAPH) |
| `Engine/ZenithPlatform/` | engine library | PLAT |
| `Engine/ZenithRHI/`, `_Vulkan/`, `_D3D12/` | engine API library and backends | RHI |
| `Engine/ZenithAssets/` | engine library | ASSET |
| `Engine/ZenithPackage/` | engine library | PACK |
| `Engine/ZenithECS/`, `_Custom/`, `_Flecs/`, `_EnTT/` | engine API library and backends | ECS |
| `Engine/ZenithScene/` | engine library | SCENE |
| `Engine/ZenithShadersGenerated/` | generated-code target | RPROD |
| `Engine/ZenithFlux/` | engine library | RPROD (render graph and GPU lifetime: RHI; lighting: LIGHT; geometry: GEOM; environment GPU side `Flux_Environment.h`, `Internal/Environment/`: ENV; VFX GPU side: VFX) |
| `Engine/ZenithSave/` | engine library | SAVE |
| `Engine/ZenithScript/`, `Engine/ZenithScriptCompiler/` | engine library, tool-side library | SCRIPT |
| `Engine/ZenithRuntime/` | engine library | MOD |
| `Engine/Zenith2D/` | module | TWOD |
| `Engine/ZenithPhysics/`, `_Jolt/`, `_PhysX/` | module and backends | PHYS |
| `Engine/ZenithAnimation/` | module | ANIM |
| `Engine/ZenithAudio/` | module | AUDIO |
| `Engine/ZenithAI/` | module | AI |
| `Engine/ZenithUI/` | module | UI |
| `Engine/ZenithOnline/`, `Engine/ZenithOnline_Offline/` | module and backend | ONLINE |
| `Engine/ZenithOnline_SteamCore/`, `Engine/ZenithOnline_GooglePlayCore/`, `Engine/ZenithOnline_Steam/`, `Engine/ZenithOnline_GooglePlay/` | engine libraries and backends (Stage 6) | ONLINE |
| `Engine/ZenithNet/` | module | NET |
| `Engine/ZenithStreaming/` | module | STREAM |
| `Engine/ZenithGameplay/` | module | GAME |
| `Engine/ZenithCinematics/` | module | CINE |
| `Engine/ZenithVFX/` | module | VFX |
| `Engine/ZenithEnvironment/` | module | ENV |
| `Engine/ZenithXR/` | module (Stage 7) | SCOPE |
| `Player/` | Player library | MOD |
| `Shaders/` | shader sources | RPROD |
| `Tools/ZenithEditor/` | tool | EDIT |
| `Tools/EditorCore/` | tool library | EDIT |
| `Tools/Authoring/` | tool library | EDIT (the library file, root header and contract); each owner's `Internal/<Dir>/`, root header `Zenith_Authoring<Area>.h`, `Tests/<Dir>/` and fixtures (Zenith2/Docs/Design/EDIT.md → "ZenithAuthoring owners") |
| `Tools/DevTool/` | tool | DEVT |
| `Tools/ZenithCook/` | tool | ASSET (importers, `Internal/Importers/` and `Internal/Content/`: CONTENT; packaging, `Internal/Package/`: PACK) |
| `Tools/ContentCore/` | tool library | CONTENT |
| `Tools/ZenithContent/` | tool | CONTENT |
| `Tools/Package/` | scripts | PACK |
| `Tools/ZenithShaderCompiler/` | tool | RPROD |
| `Tools/NewGame/` | tool | MOD |
| `Tools/NewGameScaffold/` | tool library | MOD |
| `Tools/DocumentHost/` | tool library | SCENE |
| `Tools/ZenithPathTracer/` | test-only tool library | ADVGFX (the path-traced reference, Z2-D-043; validation consumer: LIGHT, Zenith2/Docs/Rendering.md → "Validation roles") |
| `Tools/Gates/`, `Tools/Backlog/` | scripts (protected, respectively ordinary) | PROC |
| `Tools/Maintenance/` | scripts and data | MAINT |
| `Tools/Security/` | registries and scripts (no compiled source; the gate logic is in the protected `Tools/Gates/Security/`) | SEC |
| `Tools/Setup/`, `ThirdParty/`, `CMake/` | scripts, manifest, build helpers | SHIP |
| `Benchmarks/` | harnesses | RESEARCH (per harness: the area that owns the measured subsystem, for example `Benchmarks/Ed25519/`: SEC; each child directory is owned by its own `subsystem_owners.json` row) |
| `Tests/` | integration suites | the area of each suite, for example Lifecycle: GAME; Determinism: ECS; Package: PACK; ContentPipeline: CONTENT; Security: SEC (each child directory is owned by its own `subsystem_owners.json` row) |
| `Games/Workload*/` | workload games | RPROD (rendering workloads), ECS (WorkloadDense), UI (WorkloadUI2D), ANIM (WorkloadCharacters) |
| `Games/OpenWorldSlice/`, `CinematicSlice/`, `Game2D/` | flagship slices | GAME, CINE, TWOD respectively |
| `Games/Minimal/` | the Stage 2 proving game | MOD |
| `Docs/` | documentation | PROC (core docs), each area (its design doc and catalogue) |

- [Z2-N-ARCH-021] Every compiled-language source under `Engine/`, `Player/`, `Tools/`, `Benchmarks/` and `Tests/` belongs to exactly one target by its directory (Architecture/BuildSystem.md → "The zenith_add_library helper"), and the `SourceLayout` gate step fails on a source in a directory no target globs.
- [Z2-N-ARCH-022] Each engine library is one directory directly under `Engine/` whose name equals its CMake target, and a library is never nested inside another library's directory; the one exception is `ZenithPlayer`, whose directory is `Zenith2/Player/` (Architecture/BuildSystem.md → "The zenith_add_library helper").
- [Z2-N-ARCH-023] A library's public API is the set of headers at its directory root; everything else is under `Internal/`, platform code under `Internal/Windows/`, `Internal/Android/` or `Internal/Linux/`, and tests under `Tests/` (Zenith2/Docs/Contracts.md → "API boundary").
- [Z2-N-ARCH-024] Games live only under `Zenith2/Games/<Name>/`, in this repository, and no compiled-language source is authored or tracked under `Games/<Name>/` (Z2-D-002); the one generated TU of each game lives under `out/` (bullet 206).
- [Z2-N-ARCH-025] Everything a build, cook, test or gate step produces is written under `Zenith2/out/` (or the `linux-tsan` tree under `$HOME/.zenith2/out/`), which the nested `Zenith2/.gitignore` ignores, so a gate run never changes the tracked or untracked tree outside `out/`.
- [Z2-N-ARCH-026] `Zenith2/Docs/Board/` holds Vault records only; no document, script or tool output is written there by anything other than the Vault writer.
- [Z2-N-ARCH-027] `Zenith2/Tools/Gates/**` and `Zenith2/CLAUDE.md` are protected paths edited only by PROC `needs-human` tickets; ordinary tickets run gate steps by name and never edit them.
- [Z2-N-ARCH-028] Engine assets that every game needs (default configuration, engine shaders' cooked blobs, fallback textures) are produced by the cook from `Zenith2/Engine/*` libraries' declared content and `Zenith2/Shaders/`, never copied into a game's directory.

### Games in the repository

| Game | Purpose | Workload or slice |
|---|---|---|
| `WorkloadIndoor` | 200 lights (32 shadowed), 1M visible triangles | W-Indoor |
| `WorkloadOutdoor` | 4 km² terrain, 100k foliage instances, 2 km view, 10 m/s traversal | W-Outdoor |
| `WorkloadCharacters` | 100 skinned characters at 60 bones, 100 AI agents | W-Characters |
| `WorkloadDense` | dense simulation (scale set by its Stage 1 workload ticket) | W-Dense |
| `WorkloadUI2D` | UI-heavy 2D project (scale set by its Stage 1 workload ticket) | W-UI2D |
| `OpenWorldSlice` | streaming, world authoring, AI at scale, persistent saves, the Luau quest, dialogue and inventory samples | W-OpenWorld (flagship) |
| `CinematicSlice` | character and animation quality, cinematics, lighting, audio, set-pieces | W-Cinematic (flagship) |
| `Game2D` | 2D, UI, accessibility flows, localization | W-Game2D (flagship) |
| `Minimal` | the smallest game exercising the whole game model; the Stage 2 proving game | none |

The workload definitions and their numbers are in Zenith2/Docs/Budgets.md → "Workload definitions".

- [Z2-N-ARCH-029] Every game under `Games/` builds in every preset its `game.json` platforms allow on every change, because every game is part of the one CMake tree; a change that breaks any game's build fails the Build step.

## Library dependency graph

### Declared direct dependencies

The graph below is the declared intent libgraph checks real use against. Layer 0 has no dependency; a library's layer is one more than the highest layer among its declared dependencies.

| Library | Kind | Layer | Declared direct dependencies |
|---|---|---|---|
| ZenithCore | engine | 0 | — |
| ZenithGraph | engine | 1 | ZenithCore |
| ZenithProfiling | engine | 1 | ZenithCore |
| ZenithReflect | engine | 1 | ZenithCore |
| ZenithVertexFormat | engine | 1 | ZenithCore |
| ZenithCompression | engine (API) | 1 | ZenithCore |
| ZenithDocument | engine | 2 | ZenithCore, ZenithReflect, ZenithProfiling |
| ZenithCompression_Zlib | backend | 2 | ZenithCompression, ZenithCore |
| ZenithCompression_Zstd | backend | 2 | ZenithCompression, ZenithCore |
| ZenithJobs | engine | 2 | ZenithCore, ZenithGraph, ZenithProfiling |
| ZenithPlatform | engine | 2 | ZenithCore, ZenithProfiling |
| ZenithRHI | engine (API) | 3 | ZenithCore, ZenithProfiling, ZenithPlatform |
| ZenithAssets | engine | 3 | ZenithCore, ZenithReflect, ZenithPlatform, ZenithCompression, ZenithJobs, ZenithProfiling |
| ZenithECS | engine (API) | 3 | ZenithCore, ZenithReflect, ZenithGraph, ZenithJobs, ZenithProfiling |
| ZenithRHI_Vulkan | backend | 4 | ZenithRHI, ZenithPlatform, ZenithProfiling, ZenithCore |
| ZenithRHI_D3D12 | backend | 4 | ZenithRHI, ZenithPlatform, ZenithProfiling, ZenithCore |
| ZenithECS_Custom | backend | 4 | ZenithECS, ZenithReflect, ZenithCore |
| ZenithECS_Flecs | backend | 4 | ZenithECS, ZenithReflect, ZenithCore |
| ZenithECS_EnTT | backend | 4 | ZenithECS, ZenithReflect, ZenithCore |
| ZenithPackage | engine | 4 | ZenithCore, ZenithProfiling, ZenithPlatform, ZenithAssets, ZenithJobs |
| ZenithScene | engine | 4 | ZenithCore, ZenithReflect, ZenithECS, ZenithAssets, ZenithJobs, ZenithProfiling, ZenithDocument, ZenithCompression |
| ZenithShadersGenerated | generated (INTERFACE) | 4 | ZenithCore, ZenithVertexFormat, ZenithRHI |
| ZenithFlux | engine | 5 | ZenithCore, ZenithGraph, ZenithJobs, ZenithProfiling, ZenithRHI, ZenithVertexFormat, ZenithAssets, ZenithShadersGenerated, ZenithCompression (VT tile decode and the texture streamer call `Zenith_DecompressSection`) |
| ZenithSave | engine | 5 | ZenithCore, ZenithReflect, ZenithECS, ZenithScene, ZenithPlatform, ZenithCompression, ZenithProfiling |
| ZenithScript | engine | 5 | ZenithCore, ZenithReflect, ZenithECS, ZenithScene, ZenithAssets, ZenithJobs, ZenithPlatform, ZenithProfiling |
| ZenithScriptCompiler | tool-side engine library | 6 | ZenithScript, ZenithReflect, ZenithCore |
| ZenithRuntime | engine | 6 | ZenithCore, ZenithGraph, ZenithJobs, ZenithProfiling, ZenithPlatform, ZenithReflect, ZenithCompression, ZenithAssets, ZenithECS, ZenithScene, ZenithSave, ZenithScript, ZenithRHI, ZenithFlux, ZenithPackage |
| ZenithPlayer | Player | 7 | ZenithRuntime, ZenithCore, ZenithProfiling, ZenithPlatform |
| ZenithPhysics | module | 7 | ZenithRuntime, ZenithSave, ZenithScene, ZenithECS, ZenithReflect, ZenithAssets, ZenithPlatform, ZenithJobs, ZenithCore, ZenithProfiling |
| ZenithAudio | module | 7 | ZenithRuntime, ZenithPlatform, ZenithAssets, ZenithScene, ZenithECS, ZenithReflect, ZenithJobs, ZenithCore, ZenithProfiling |
| ZenithUI | module | 7 | ZenithRuntime, ZenithFlux, ZenithShadersGenerated, ZenithRHI, ZenithVertexFormat, ZenithPlatform, ZenithAssets, ZenithScene, ZenithECS, ZenithReflect, ZenithCore, ZenithProfiling |
| Zenith2D | module | 7 | ZenithRuntime, ZenithFlux, ZenithShadersGenerated, ZenithAssets, ZenithScene, ZenithECS, ZenithReflect, ZenithSave, ZenithJobs, ZenithProfiling, ZenithVertexFormat, ZenithCore (plus the 2D-physics dependency D-SEL-2DPHYS rules, open, Stage 2) |
| ZenithOnline | module (API) | 7 | ZenithRuntime, ZenithPlatform, ZenithJobs, ZenithCore, ZenithSave, ZenithReflect, ZenithECS, ZenithAssets, ZenithProfiling, ZenithScript (Stage 4), ZenithPackage (Stage 6) |
| ZenithNet | module | 7 | ZenithRuntime, ZenithSave, ZenithScene, ZenithECS, ZenithReflect, ZenithJobs, ZenithCore, ZenithPlatform, ZenithProfiling, ZenithAssets |
| ZenithStreaming | module | 7 | ZenithRuntime, ZenithFlux, ZenithShadersGenerated, ZenithAssets, ZenithScene, ZenithECS, ZenithReflect, ZenithJobs, ZenithPlatform, ZenithCore, ZenithProfiling |
| ZenithGameplay | module | 7 | ZenithRuntime, ZenithScene, ZenithECS, ZenithReflect, ZenithCore |
| ZenithVFX | module | 7 | ZenithRuntime, ZenithFlux, ZenithShadersGenerated, ZenithAssets, ZenithScene, ZenithECS, ZenithReflect, ZenithJobs, ZenithProfiling, ZenithVertexFormat, ZenithCore |
| ZenithEnvironment | module | 7 | ZenithRuntime, ZenithFlux, ZenithShadersGenerated, ZenithAssets, ZenithScene, ZenithECS, ZenithReflect, ZenithJobs, ZenithPlatform, ZenithCompression, ZenithProfiling, ZenithCore |
| ZenithXR | module (Stage 7; `SUPPORTED ZENITH_WINDOWS`) | 7 | ZenithRuntime, ZenithFlux, ZenithRHI, ZenithPlatform, ZenithScript, ZenithReflect, ZenithECS, ZenithProfiling, ZenithJobs, ZenithCore |
| ZenithPhysics_Jolt | backend | 8 | ZenithPhysics, ZenithJobs, ZenithCore, ZenithProfiling |
| ZenithPhysics_PhysX | backend | 8 | ZenithPhysics, ZenithJobs, ZenithCore, ZenithProfiling |
| ZenithOnline_Offline | backend | 8 | ZenithOnline, ZenithPlatform, ZenithCore |
| ZenithOnline_SteamCore | engine (Stage 6; `ZENITH_WINDOWS`) | 8 | ZenithOnline, ZenithPlatform, ZenithCore |
| ZenithOnline_GooglePlayCore | engine (Stage 6; `ZENITH_GPU`) | 8 | ZenithOnline, ZenithPlatform, ZenithCore |
| ZenithAI | module | 8 | ZenithRuntime, ZenithPhysics, ZenithSave, ZenithPlatform, ZenithScene, ZenithECS, ZenithReflect, ZenithAssets, ZenithJobs, ZenithCore, ZenithProfiling |
| ZenithAnimation | module | 8 | ZenithRuntime, ZenithPhysics, ZenithFlux, ZenithPlatform, ZenithSave, ZenithAssets, ZenithScene, ZenithECS, ZenithReflect, ZenithJobs, ZenithCore, ZenithProfiling |
| ZenithCinematics | module | 9 | ZenithRuntime, ZenithAnimation, ZenithSave, ZenithAssets, ZenithScene, ZenithECS, ZenithReflect, ZenithCore, ZenithFlux, ZenithJobs, ZenithProfiling |
| ZenithOnline_Steam | backend (Stage 6; `ZENITH_WINDOWS`) | 9 | ZenithOnline_SteamCore, ZenithOnline, ZenithPlatform, ZenithCore (third party `steamworks-sdk`) |
| ZenithOnline_GooglePlay | backend (Stage 6; `ZENITH_ANDROID`) | 9 | ZenithOnline_GooglePlayCore, ZenithOnline, ZenithPlatform, ZenithCore |

Tools and their declared dependencies (tools are leaves; nothing depends on a tool):

| Tool | Declared direct dependencies | Third party |
|---|---|---|
| ZenithShaderCompiler | ZenithCore, ZenithVertexFormat | Slang, DXC (host tools) |
| ZenithCook | ZenithCore, ZenithReflect, ZenithCompression, ZenithCompression_Zlib, ZenithCompression_Zstd, ZenithAssets, ZenithScene, ZenithVertexFormat, ZenithJobs, ZenithPlatform, ZenithScriptCompiler, ZenithRuntime, every module library (Zenith2D, ZenithPhysics, ZenithPhysics_Jolt and ZenithPhysics_PhysX (the backend shape sections, written through each backend's `BakeShape`; [Z2-N-CONTRACT-050]), ZenithAnimation, ZenithAudio, ZenithAI, ZenithUI, ZenithOnline, ZenithNet, ZenithStreaming, ZenithGameplay, ZenithCinematics, ZenithVFX, ZenithEnvironment), ZenithShadersGenerated (a tools-only edge: the packer reads the generated stream-set layout tables), ZenithDocument, ZenithDocumentHost, ZenithAuthoring (added by the first owner move that lands), ZenithContentCore, ZenithPackage (PACK's shared format code, linked in its tests and verify mode) | fastgltf, meshoptimizer (as a consumer of ZenithContentCore's entries), KTX-Software, MikkTSpace |
| ZenithEditor | ZenithEditorCore, ZenithAuthoring, the engine libraries it includes directly (ZenithCore, ZenithGraph, ZenithJobs, ZenithProfiling, ZenithPlatform, ZenithReflect, ZenithDocument, ZenithDocumentHost, ZenithCompression, ZenithECS, ZenithScene, ZenithAssets, ZenithSave, ZenithRHI, ZenithScript, ZenithScriptCompiler, ZenithRuntime), ZenithShadersGenerated, ZenithFlux, ZenithVertexFormat, every module, every backend, ZenithNewGameScaffold (`Zenith2/Docs/Design/EDIT.md` → "Libraries and third-party dependencies") | ImGui |
| ZenithEditorCore (static tool library, `Tools/EditorCore/`; no ImGui and no shell; `SUPPORTED ZENITH_WINDOWS`; consumers ZenithEditor, ZenithContent and the integration suites `EditorMigration` and `EditorSchema`) | ZenithCore, ZenithReflect, ZenithDocument, ZenithDocumentHost, ZenithScene, ZenithECS, ZenithAssets, ZenithJobs, ZenithProfiling, ZenithPlatform, ZenithScript, ZenithRuntime, ZenithAuthoring | — |
| ZenithAuthoring (static tool library, `Tools/Authoring/`, `zenith_add_tool` with no main; `SUPPORTED ZENITH_WINDOWS`, so its hosted parsers' fuzz targets build in `win-asan`; outside every Shipping link closure; consumers ZenithCook, ZenithEditorCore, ZenithEditor, ZenithContent, `Zenith_Integration_ContentPipeline` and `Zenith_Integration_WorldAuthoring`) | created with ZenithCore alone; each owner's move appends the libraries its own files include (`Zenith2/Docs/Design/EDIT.md` → "ZenithAuthoring contract rules"); the target state is ZenithCore, ZenithReflect, ZenithDocument, ZenithAssets, ZenithScene, ZenithCompression, ZenithJobs, ZenithAnimation, ZenithAudio, ZenithEnvironment, ZenithUI, ZenithVFX, ZenithAI, Zenith2D, ZenithCinematics, ZenithStreaming, ZenithPhysics (CHAR's `static_assert`s against PHYS's controller and component headers) and ZenithContentCore, never ZenithRuntime, ZenithEditorCore or ZenithCook | — |
| ZenithContentCore (static tool library, `Tools/ContentCore/`; `SUPPORTED ZENITH_WINDOWS`; outside every Shipping link closure; consumers ZenithCook, ZenithAuthoring, ZenithContent and `Zenith_Integration_ContentPipeline`) | ZenithCore, ZenithDocument, ZenithScene (`Zenith_GltfSource.h` uses `Zenith_Transform`) | fastgltf (with its `dependsOn` simdjson), meshoptimizer, stb-image, dr-wav, dr-flac, dr-mp3, stb-vorbis |
| ZenithContent (`Tools/ZenithContent/`, `SUPPORTED ZENITH_TOOLS_HOST`) | ZenithContentCore, ZenithAuthoring, ZenithEditorCore, ZenithDocument, ZenithDocumentHost, ZenithScene, ZenithAssets, ZenithCore | — |
| ZenithDevTool (`Tools/DevTool/`, `SUPPORTED ZENITH_TOOLS_HOST`) | ZenithCore, ZenithProfiling, ZenithPlatform | — |
| ZenithNewGame (and its library ZenithNewGameScaffold, `SUPPORTED ZENITH_WINDOWS`) | ZenithCore, ZenithPlatform, ZenithReflect, ZenithDocument, ZenithDocumentHost | — |
| ZenithDocumentHost (tool library, `SUPPORTED ZENITH_HOST`; outside every Shipping link closure) | ZenithDocument, ZenithCore | — |
| ZenithPathTracer (static library; `SUPPORTED ZENITH_WINDOWS`: the `win-debug`, `win-release`, `win-tools` and `win-asan` build presets) | ZenithCore, ZenithRHI, ZenithVertexFormat, ZenithCompression, ZenithFlux, ZenithShadersGenerated (the scene-document parser is a header-only test-support helper whose callers declare ZenithDocument) | — |

<!-- elaboration -->
The plan states three dependency chains (Core → Graph → Jobs; Core → Profiling → Platform → RHI → Flux, with Flux also using Graph; Core → Reflect → ECS → Scene → Script, with ECS scheduling using Jobs). The table keeps all three and completes the graph: modules sit above `ZenithRuntime` because they register through its module interface, backends sit one layer above their API, and `ZenithAssets` sits below `ZenithFlux` because Flux implements the asset system's loader interface for GPU-backed asset types.

- [Z2-N-ARCH-082] `ZenithFlux` declares `ZenithJobs` (its record node fans command-list recording out as sub-jobs) and `ZenithAssets` (it implements the asset system's loader interface for textures, meshes and materials, and the render world carries asset handles); an edge to `ZenithAssets` is not an edge to `ZenithScene`, so it keeps "Flux never sees Scene"; Zenith2/Docs/Rendering.md's library table follows this table's `ZenithFlux` row.
- [Z2-N-ARCH-083] Generated shader headers use only ZenithCore types, ZenithRHI descriptor and view value types and ZenithVertexFormat layout-table types, so `ZenithShadersGenerated` has no dependency above layer 3 and every library including a generated header declares `ZenithShadersGenerated`.
- [Z2-N-ARCH-084] `ZenithScriptCompiler` is a tool-side library: `ZenithCook` and `ZenithEditor` depend on it, no Player links it in any configuration, and the Shipping link-closure check of Architecture/BuildSystem.md → "The zenith_add_library helper" fails when it appears there.
- [Z2-N-ARCH-085] The Luau-free binding ABI is the public header `ZenithReflect/Zenith_ScriptBinding.h`; a library or module that owns script bindings depends on `ZenithReflect` for them and never on `ZenithScript`, and the bindings of libraries below `ZenithReflect` live in `ZenithRuntime/Internal/ScriptBindings/` (Zenith2/Docs/Scripting.md → "Modules, bindings and generated types").
- [Z2-N-ARCH-087] `ZenithDocument` performs no host file I/O: every document read and write goes through its `Zenith_DocumentStorage` interface, and the host-filesystem implementation is the tool library `ZenithDocumentHost`, which the editor, the cook and `ZenithNewGameScaffold` link and no Player links, so a runtime target that links `ZenithDocument` gains no path outside the virtual filesystem (Architecture/PlatformLayer.md bullet 923).
- [Z2-N-ARCH-086] `ZenithPathTracer` is linked by render-test and physical-accuracy test targets alone and never appears in any Player's link closure; the Shipping link-closure check covers it as a tool target (Architecture/BuildSystem.md → "The zenith_add_library helper").

- [Z2-N-ARCH-071] Every library's `zenith_add_library(… DEPENDS …)` lists exactly the declared direct dependencies of the table above, and a change to a library's dependencies edits the table in the same diff.
- [Z2-N-ARCH-072] The declared graph is acyclic, and every edge points from a higher layer to a strictly lower layer.
- [Z2-N-ARCH-073] No engine library, module or backend depends on a tool, a game, the Player or a test target, and no library below `ZenithRuntime` depends on a module.
- [Z2-N-ARCH-074] A module may depend on another module only as the table declares (AI on Physics, Animation on Physics, Cinematics on Animation), and a game selecting a module selects its module dependencies too (§ "Engine modules and game projects").
- [Z2-N-ARCH-075] A backend depends on its API library and on libraries at or below the API's layer, never on another backend or a sibling API's backend; it may also depend on one engine library of its own family that depends only on the family's API library and libraries at or below the API's layer, sits strictly between the API's layer and the backend's, is never itself a backend (no `BACKEND_OF`), and is linked into a game only through that backend's `DEPENDS` (and into tests through `TEST_DEPENDS`), as `ZenithOnline_SteamCore` and `ZenithOnline_GooglePlayCore` are.

### Flux never sees Scene

- [Z2-N-ARCH-076] `ZenithFlux` declares no dependency on, includes no header of, and links no symbol of `ZenithScene`, `ZenithECS`, `ZenithScript`, `ZenithSave`, `ZenithRuntime` or any module; `ZenithRuntime` extracts render state into `Flux_RenderWorld` and modules contribute through registered extractors (Architecture/CPUFrameGraph.md → "Render extraction and pipelining").
- [Z2-N-ARCH-077] `ZenithRHI` and its backends know nothing of the render graph's passes, of assets, or of any simulation library; they expose devices, resources, queues and command recording to `ZenithFlux`.

### Upward notifications

- [Z2-N-ARCH-078] A lower library that needs to notify a higher one declares an interface that the higher library implements and the composition root passes down — `Zenith_LifecycleListener` (ZenithScene), `Zenith_MemoryConsumer` (ZenithCore), the asset loader interface (ZenithAssets, implemented by ZenithFlux), `Zenith_ModuleInstance` (ZenithRuntime) — and never stores a pointer into a higher library's code otherwise (Zenith2/Docs/Contracts.md → "Layering DAG").

### What libgraph enforces

The `Libgraph` gate step runs libgraph's CMake File API front end over the `win-debug` build (built with `/Gy-`), reading `declared_deps.json` for declared edges (Architecture/BuildSystem.md → "The zenith_add_library helper").

The first six rows are the base list of Zenith2/Docs/Contracts.md → "Layering DAG"; the rows after them are marked elaboration.

| Rule | Evidence | Result on violation |
|---|---|---|
| used-but-undeclared edge | include resolution and link-symbol use against `declared_deps.json` | fail |
| cycle (declared or real) | the declared graph; the include and symbol graph | fail |
| `Internal/` encapsulation break | an include of `<Lib>/Internal/**` from outside `<Lib>` and its test targets | fail |
| leakage | a third-party or `Internal/` header reachable from a public header | fail |
| shader compiler in a runtime target | Slang or DXC symbols in any runtime target's link surface | fail |
| (elaboration) third-party header outside the owner's `Internal/` | the include graph against the manifest `owner` field | fail |
| (elaboration) `INTERFACE_LIBRARY` targets | the File API's INTERFACE targets (`ZenithShadersGenerated`): declared edges and include directories; no symbol stage | fail on an undeclared include edge |
| (elaboration) Shipping link closure | the target set of Architecture/BuildSystem.md bullet 240 against the forbidden targets of bullet 241 | fail |
| (elaboration) development services in a Shipping executable | symbols listed in `Zenith2/Tools/Gates/shipping_forbidden_symbols.json` in any Shipping game executable (§ "Security architecture") | fail |
| inbound-surface growth | the delta against `Zenith2/Tools/Gates/libgraph_baseline.json` | fail unless an API review record covers it |
| swap contract | per backend family (ECS, RHI, physics, compression, and — elaboration — online), the symbols and types consumers use | report (the swappability evidence) |

- [Z2-N-ARCH-079] The rules of the table "What libgraph enforces" marked fail are hard failures of the `Libgraph` gate step (Z2-D-007, Z2-D-042), and a rule exception exists only as an allowlist entry in a protected gate file with a decision ID.
- [Z2-N-ARCH-080] Libgraph writes the swap-contract report for every backend family to `${ZENITH_OUT}/gates/libgraph/swap_<family>.json` on every run, and a backend whose consumers use a symbol or type outside the family's API library fails the swap-contract check.

### Generated code attribution

| Generated file | Produced by | Attributed to |
|---|---|---|
| shader parameter headers `Flux/Shaders/<Program>.generated.h`, blobs, reflection, vertex layout tables | ZenithShaderCompiler, through the `SHADERS` option of `ZenithShadersGenerated` | `ZenithShadersGenerated`, the one library that declares `SHADERS` (includes attach to it; compiled symbols to the including library) |
| files listed in an ordinary library's `GENERATES` option | that library's own custom commands | the declaring library |
| a game's `Zenith_GameComposition.generated.cpp` | `zenith_add_game` | the game's executable target |
| `.d.luau`, `.luaurc` | ZenithCook's `luau-types` step | the game (not compiled) |
| `HeaderSelfContained` TUs | the gate step | the header's library (checked, never linked) |

- [Z2-N-ARCH-081] Generated code is attributed to the target whose build produces it, recorded in `declared_deps.json`, and libgraph counts its includes as that target's edges (Zenith2/Docs/Contracts.md → "Layering DAG"); the symbol rule for the INTERFACE target `ZenithShadersGenerated` is Architecture/BuildSystem.md bullet 243.

## Third-party dependencies

All third-party dependencies are pinned in `Zenith2/ThirdParty/manifest.json`, integrated as released and confined to the owning library's `Internal/` directory (Z2-D-082). The manifest format and the integration mechanics are in Architecture/BuildSystem.md → "Third-party integration".

### Selected dependencies

| Dependency | Manifest name | Purpose | Owner | Integration | Licence (SPDX, recorded in the manifest; licence-file hash checked by `Setup-Zenith2.ps1 -Check`) | Platforms | Usage |
|---|---|---|---|---|---|---|---|
| SDL3 | `sdl3` | window, input, Android lifecycle, filesystem | ZenithPlatform | cmake-subproject | Zlib | windows, android, linux | runtime |
| volk | `volk` | Vulkan loading and device-level dispatch (Z2-D-026) | ZenithRHI_Vulkan | zenith-wrapper | MIT | windows, android | runtime |
| Vulkan-Headers, Vulkan-Profiles | `vulkan-headers`, `vulkan-profiles` | Vulkan API and the baseline profile (the profile generator `scripts/gen_profiles_solution.py` and its pinned Python are developer-only toolchain pins; the generated files are committed) | ZenithRHI_Vulkan | header-only | Apache-2.0 | windows, android | runtime |
| renderdoc-app (`renderdoc_app.h`) | `renderdoc-app` | the RenderDoc in-application API for development-build captures | ZenithRHI_Vulkan, ZenithRHI_D3D12 (a header-only entry shared by the two backends of one API) | header-only | MIT | windows, android | runtime (non-Shipping use) |
| VMA | `vma` | Vulkan memory | ZenithRHI_Vulkan | zenith-wrapper | MIT | windows, android | runtime |
| D3D12 Agility SDK | `d3d12-agility-sdk` | D3D12 runtime (headers linked; runtime DLL `sanitizerExempt`) | ZenithRHI_D3D12 | prebuilt | Microsoft Agility SDK licence | windows | runtime |
| D3D12MA | `d3d12ma` | D3D12 memory | ZenithRHI_D3D12 | zenith-wrapper | MIT | windows | runtime |
| Slang | `slang` | shader compilation to SPIR-V and DXIL, offline only | ZenithShaderCompiler | host-tool | Apache-2.0 WITH LLVM-exception | windows | tools |
| DXC | `dxc` | DXIL compilation, offline only | ZenithShaderCompiler | host-tool | NCSA | windows | tools |
| Jolt Physics | `jolt` | physics backend | ZenithPhysics_Jolt | cmake-subproject | MIT | windows, android, linux | runtime |
| PhysX 5 | `physx` | physics backend (Android per D-SEL-PHYSX-ANDROID, open, Stage 1) | ZenithPhysics_PhysX | zenith-wrapper | BSD-3-Clause | windows, (android) | runtime |
| zlib | `zlib` | compression backend | ZenithCompression_Zlib | zenith-wrapper (upstream 1.3.1's CMake renames `zconf.h` in its source directory, which would modify the hash-verified dependency cache) | Zlib | windows, android, linux | runtime |
| zstd | `zstd` | compression backend | ZenithCompression_Zstd | cmake-subproject (`cmakeSourceDir` `build/cmake`) | BSD-3-Clause | windows, android, linux | runtime |
| Luau | `luau` | scripting: `Luau.VM` owned by ZenithScript; `Luau.Ast` and `Luau.Compiler` owned by ZenithScriptCompiler | ZenithScript, ZenithScriptCompiler (per target) | cmake-subproject | MIT | windows, android, linux | runtime (VM), tools (compiler) |
| flecs | `flecs` | ECS backend | ZenithECS_Flecs | cmake-subproject | MIT | windows, android, linux | runtime |
| EnTT | `entt` | ECS backend | ZenithECS_EnTT | header-only | MIT | windows, android, linux | runtime |
| miniaudio | `miniaudio` | audio output device layer (WASAPI, AAudio, null), plus format conversion when the endpoint is not 48 kHz; decoding, node graph, engine and resource manager compiled out | ZenithAudio | cmake-subproject | MIT-0 | windows, android | runtime |
| ImGui (docking) | `imgui` | editor UI | ZenithEditor | zenith-wrapper | MIT | windows | tools |
| fastgltf | `fastgltf` | glTF import (tool targets that call it directly, ZenithCook among them, list it as consumers) | ZenithContentCore | cmake-subproject | MIT | windows | tools |
| meshoptimizer | `meshoptimizer` | mesh processing and meshlets (tool targets that call it directly, ZenithCook among them, list it as consumers) | ZenithContentCore | cmake-subproject | MIT | windows | tools |
| stb_image | `stb-image` | image source decoding | ZenithContentCore | header-only | dual MIT or public domain (`licenceElected` in the manifest) | windows | tools |
| dr_wav, dr_flac, dr_mp3 | `dr-wav`, `dr-flac`, `dr-mp3` | audio source decoding (WAV, FLAC, MP3) | ZenithContentCore | header-only | dual public domain or MIT-0 (`licenceElected` in the manifest) | windows | tools |
| stb_vorbis | `stb-vorbis` | audio source decoding (Ogg Vorbis) | ZenithContentCore | header-only | dual MIT or public domain (`licenceElected` in the manifest) | windows | tools |
| MikkTSpace (`mikktspace.c`) | `mikktspace` | tangent generation for imported meshes | ZenithCook | zenith-wrapper | Zlib | windows | tools |
| KTX-Software (basisu) | `ktx-software` | texture compression (BC7, ASTC) | ZenithCook | cmake-subproject | Apache-2.0 | windows | tools |
| OpenXR loader (Khronos OpenXR SDK) | `openxr-loader` | the OpenXR loader (Stage 7): a shared library linked through its import library and staged beside the executable | ZenithXR | prebuilt | Apache-2.0 | windows | runtime |
| Tracy | `tracy` | deep profiling capture | ZenithProfiling | cmake-subproject | BSD-3-Clause | windows, android, linux | runtime (non-Shipping) |
| doctest | `doctest` | tests | test targets | header-only | MIT | windows, android, linux | tests |
| nanobench | `nanobench` | benchmarks | benchmark targets | header-only | MIT | windows, android, linux | tests |
| WARP (NuGet) | `warp` | headless D3D12 rendering in CI (`sanitizerExempt`) | test runs | prebuilt | Microsoft WARP licence | windows | tests |
| lavapipe (Mesa) | `lavapipe` | headless Vulkan rendering in CI (`sanitizerExempt`; the pinned build includes the Win32 WSI, because swapchain tests create a hidden window) | test runs | prebuilt | MIT | windows, linux | tests |

### Dependencies admitted by open selections

| Selection | Candidates it admits when ruled |
|---|---|
| D-SEL-JSON (open, Stage 1) | yyjson or simdjson, owned by ZenithCore |
| D-SEL-HASH (open, Stage 1) | xxHash, owned by ZenithCore |
| D-SEL-HEAP (open, Stage 1) | mimalloc, owned by ZenithCore, or none (the system allocator) |
| D-SEL-NAV (open, Stage 1/2) | Recast/Detour, owned by ZenithAI, or none |
| D-SEL-TEXT (open, Stage 1/2) | HarfBuzz, FreeType, msdfgen, SheenBidi or an ICU4X subset, owned by ZenithUI |
| D-SEL-UIRT (open, Stage 1/2) | RmlUi, owned by ZenithUI, or none |
| D-SEL-CODEC (open, Stage 2) | Opus or Vorbis, owned by ZenithAudio |
| D-SEL-VIDEO (open, Stage 2) | FFmpeg, owned by ZenithCinematics, or the platform decoders (no dependency) |
| D-SEL-CRASH (open, Stage 2) | Crashpad, owned by ZenithPlatform, or none |
| D-SEL-2DPHYS (open, Stage 2) | Box2D v3, owned by Zenith2D, or none (Jolt in 2D-constrained mode) |
| D-SEL-FBX (open, Stage 4) | FBX2glTF, a Blender headless conversion, or the Autodesk FBX SDK, owned by ZenithCook, or none (glTF-only) |
| D-SEL-UPSCALER (open, Stage 5) | AMD FidelityFX Super Resolution 2 or 3 (shader source compiled offline), NVIDIA DLSS or Intel XeSS, owned by ZenithFlux (the LIGHT area's code), or none |
| the Stage 1 security-architecture ticket | an Ed25519 verification implementation (candidates: the pinned Monocypher 4.0.2 and libsodium 1.0.20, or a Zenith-owned constant-time implementation), owned by ZenithPlatform |

- [Z2-N-ARCH-121] A third-party dependency is used only when it has a manifest entry with exact version, source URL, SHA-256, SPDX licence, integration mode and owner. A selection candidate that a harness measures before its ruling has an entry with `owner: "benchmark"` (linked into benchmark targets alone, Architecture/BuildSystem.md → "The manifest dependencies section"), and the ticket that implements the ruling moves the admitted candidate's `owner` to the library and its `usage` to `runtime` in the same diff; a dependency that does not build with exceptions and RTTI off is not admitted unless a recorded decision amends ForbiddenCheck rule F17 for it (Zenith2/Docs/Contracts.md → "ForbiddenCheck rules").
- [Z2-N-ARCH-122] Third-party headers (EnTT, flecs, Jolt, PhysX, zlib, zstd, Vulkan, D3D12, SDL3, Luau and every other entry) never reach a public header of any Zenith2 library; libgraph's leakage rule enforces it (Zenith2/Docs/Contracts.md → "API boundary").
- [Z2-N-ARCH-123] Slang and DXC are offline tools: no runtime target links either, and the runtime loads only precompiled SPIR-V and DXIL blobs (Z2-D-025, Zenith2/Docs/Rendering.md → "Shader system and offline compilation").
- [Z2-N-ARCH-124] Tracy is compiled into non-Shipping configurations; the Shipping configuration links the profiler's counter-only sink and no Tracy symbol, which the Shipping symbol check of § "Security architecture" verifies.
- [Z2-N-ARCH-125] The SHIP licence inventory is generated from the manifest's `licence` fields and shipped with every packaged build; a dependency whose licence the inventory cannot state blocks packaging.
- [Z2-N-ARCH-126] A dependency upgrade changes its manifest entry (version, URL, SHA-256) in one diff that also passes every conformance suite of the libraries owning it; the manifest hash change invalidates every evidence record that depends on it (Z2-D-065).
- [Z2-N-ARCH-127] The redistributables the OS or driver loader loads and nothing links (the D3D12 Agility SDK runtime, WARP, lavapipe) are the only `sanitizerExempt` entries, each with its decision ID in the manifest (Architecture/BuildSystem.md → "Third-party integration").
- [Z2-N-ARCH-128] Luau's targets have per-target owners: `Luau.VM` (and `Luau.CodeGen`) belong to `ZenithScript`, `Luau.Ast` and `Luau.Compiler` to `ZenithScriptCompiler`, so no runtime target reaches the Luau compiler.

## Build system and presets

The full specification is `Zenith2/Docs/Architecture/BuildSystem.md`; its headings are listed in § "Document family". The contract in summary (each row restates the tagged obligations of that file):

| Topic | Contract | Normative IDs |
|---|---|---|
| generator and sources | CMake presets, Ninja Multi-Config, globbed sources, no source lists, Ninja's re-run the one regeneration | 151–153, 156–157, 226 |
| libraries | `zenith_add_library(<Name> …)`: root headers public, `Internal/` private, `Internal/Windows|Android|Linux` filtered per platform, `Tests/` into test targets, declared direct `DEPENDS`, `declared_deps.json`; `zenith_add_subdirectories(<Dir> GROUPS …)` for `Benchmarks/Budgets/<AREA>/` | 156, 161–170, 182–184, 227, 238, 243 (INTERFACE attribution), TEST_DEPENDS, 251 (TEST_LABELS) |
| tests | doctest discovery; the test registry `CMake/Tests/ZenithCMakeTests.cmake` (compile-fail `*_CompileFail.cpp.in`, death and sanitizer-defect suites, fuzz targets); `ZENITH_TEST_FIXTURE_ROOT`, `ZENITH_REPO_ROOT` | 178–181, 228–229, 249–252 |
| matrix | every target declares `ZENITH_SUPPORTED`; GPU libraries exclude `linux-tsan` | 171–173 |
| exceptions | off everywhere; `/EHs` (never `/EHsc`) and `-fexceptions` on `ZenithScript_EH`, `ZenithScriptCompiler_EH` and the Luau targets alone | 174–177 |
| flags | warnings as errors, `/Gy-` and `/Z7` in `win-debug`, no RTTI, no floating-point contraction; every configuration define 0 or 1 in every target, tested by value | 185–189, 230–231, 247–248 |
| presets | configure presets `win`, `win-asan`, `linux-tsan`, `android-arm64`, `android-x86_64`; build presets `win-debug`, `win-release`, `win-shipping`, `win-tools`, `win-asan`, `linux-tsan`, `android-arm64` (+ `-debug`, `-shipping`, `-hwasan`), `android-x86_64` | 190–193 |
| reproducibility | one manifest pins toolchain and dependencies; `Setup-Zenith2.ps1`; sccache shared by every lane | 194–198, 232–233, 239 |
| third party | integrated as released; confined to the owner (a header-only entry may be shared by sibling backends); built from source in sanitizer builds; no Slang or DXC in runtime; stream-set tables generated before the cook compiles | 199–204, 234, 237 (sanitizer-exempt redistributables; per-platform options), 253 |
| games | `zenith_add_game(<Name>)` reads `game.json` with `string(JSON)`; one executable per game; no C++ in a game; generated files per configure preset | 205–211, 246 (allowed game-directory entries) |
| Android | NDK r27c, minSdk 34, 16 KB pages, AGP ≥ 8.5.1, `ApkCheck`, the HWASan build type, `ZENITH_HOST_TOOLS_DIR` | 212–216, 235–236 (SDL_main export), 244–245 |
| linux-tsan | WSL2 through `wsl.exe -d <distro> -e bash -lc`, Linux-filesystem `binaryDir`, `vm.mmap_rnd_bits ≤ 28`, CI native | 217–221 |
| gate budget | 30-minute warm phase cap, cold detection, per-step cold timeouts | 222–224 |

## Engine modules and game projects

### Settled rules

Optional engine modules are static libraries selected per game in `game.json` and registered by the composition root when present; there are no dynamic plugins, no installed SDK, no out-of-repo projects and no mod loading (Z2-D-024, Z2-D-002, Z2-D-075, Z2-D-081).

- [Z2-N-ARCH-261] A module is a `KIND MODULE` static library; it is linked into a game only when the game's `game.json` selects it, and it is registered only by the composition root through its two entry points.
- [Z2-N-ARCH-262] No Zenith2 binary loads code at run time: no `LoadLibrary`, `dlopen`, plugin directory, script-native extension or downloadable executable content exists in any runtime target.
- [Z2-N-ARCH-274] `ForbiddenCheck` rejects `LoadLibrary*` and `dlopen` calls outside `ZenithPlatform/Internal/` and `ZenithRHI_*/Internal/`, where the OS graphics loader and volk use them.
- [Z2-N-ARCH-263] There is no installed SDK and no out-of-repository game project: every game is a directory under `Zenith2/Games/` built by the one CMake tree, so every gate (libgraph, complexity, sanitizers, coverage) covers every game.
- [Z2-N-ARCH-264] Third-party mod loading does not exist (Z2-D-075); no runtime path mounts, loads or executes content from outside the package, patch, loose and user mounts, and the user mount never provides content (Architecture/PlatformLayer.md → "Virtual filesystem").

### The module list

| `MODULE_NAME` | Library | Module dependencies | Backend family | Without a device (headless) | Area |
|---|---|---|---|---|---|
| `2D` | Zenith2D | — (2D physics per D-SEL-2DPHYS, open, Stage 2) | — | simulation runs; no sprite passes | TWOD |
| `Physics` | ZenithPhysics | — | Jolt, PhysX | unchanged | PHYS |
| `Animation` | ZenithAnimation | Physics | — | unchanged (GPU skinning skipped) | ANIM |
| `Audio` | ZenithAudio | — | — | a null output device; voices, events and budgets still run | AUDIO |
| `AI` | ZenithAI | Physics | — | unchanged | AI |
| `UI` | ZenithUI | — | — | layout and logic run; no UI passes | UI |
| `Online` | ZenithOnline | — | Offline (Steam, GooglePlay in Stage 6); every supported backend linked, chosen by configuration | unchanged | ONLINE |
| `Net` | ZenithNet | — | — | unchanged (the dedicated-server shape of Stage 7) | NET |
| `Streaming` | ZenithStreaming | — | — | unchanged | STREAM |
| `Gameplay` | ZenithGameplay | — | — | unchanged | GAME |
| `Cinematics` | ZenithCinematics | Animation | — | unchanged | CINE |
| `VFX` | ZenithVFX | — | — | emitter logic runs; no GPU simulation | VFX |
| `Environment` | ZenithEnvironment | — | — | components and collision run; no terrain passes | ENV |
| `XR` | ZenithXR (Windows only, `SUPPORTED ZENITH_WINDOWS`; Stage 7) | no module dependency | no backend family | the provider is created disabled and the module starts in DISABLED | SCOPE |

Every game links the engine libraries of the dependency graph up to `ZenithRuntime`, one ECS backend, both compression backends and its platform's RHI backends; modules are the optional part.

### Module entry points

Each module's root header `Engine/<Library>/Zenith_Module_<MODULE_NAME>.h` declares exactly:

```cpp
Zenith_Result<void, Zenith_ModuleError> Zenith_Module_<MODULE_NAME>_RegisterTypes(Zenith_TypeRegistry& xTypes);

// A module without a backend family:
Zenith_Result<Zenith_Owned<Zenith_ModuleInstance>, Zenith_ModuleError> Zenith_Module_<MODULE_NAME>_Start(
	Zenith_ModuleStartContext& xContext);

// Physics takes the selected backend's factory:
Zenith_Result<Zenith_Owned<Zenith_ModuleInstance>, Zenith_ModuleError> Zenith_Module_Physics_Start(
	Zenith_ModuleStartContext& xContext, Zenith_PfnCreatePhysics pfnCreateBackend);

// Online takes the list of every linked backend and chooses one by configuration:
Zenith_Result<Zenith_Owned<Zenith_ModuleInstance>, Zenith_ModuleError> Zenith_Module_Online_Start(
	Zenith_ModuleStartContext& xContext, Zenith_Span<const Zenith_OnlineBackendEntry> axBackends);
```

`Zenith_OnlineBackendEntry` is declared by `ZenithOnline/Zenith_OnlineServices.h` (Architecture.md → "Purpose and scope", table "Assumed types").

`Zenith_ModuleInstance`, `Zenith_ModuleStartContext` and `Zenith_WorldAttachContext` are declared by `ZenithRuntime` (Architecture/CompositionRoot.md → "Startup sequence").

- [Z2-N-ARCH-265] A module registers, through the contexts the composition root passes it, exactly its reflection types, its systems, world resources, lifecycle listeners, render extractors, epilogue stages, memory consumers, command kinds and script bindings; it never reaches another module's objects except through declared world resources and system access.
- [Z2-N-ARCH-266] Module dependencies are declared in CMake (`DEPENDS`) and checked by libgraph like any other edge; a module that uses another module without declaring it fails the `Libgraph` step.
- [Z2-N-ARCH-267] `zenith_add_game` fails the configure when a selected module's module dependency is not also selected, with `Games/<Name>/game.json: module '<A>' requires module '<B>'; add "<B>" to modules`, and never adds a module implicitly.
- [Z2-N-ARCH-268] A module with a backend family receives its backend factory (Physics) or the list of its linked backends' factories (Online) only as the `Start` argument the generated composition passes, and its API library exports no factory (Zenith2/Docs/Contracts.md → "API boundary"); a module never names a backend: Online chooses among the backends the composition passes, in the order passed, the first that names `online.backend` or else the first that starts, and the generated composition passes Online's list in platform preference order (the store backend first, Offline last; MOD's backend resolution), so ZenithOnline contains no backend name.

### Backend selection

| Family | Selected by | Linked into a game | Chosen at |
|---|---|---|---|
| ECS | `backends.ecs` | the selected backend | link time |
| Physics | `backends.physics` (per platform allowed) | the selected backend | link time |
| Compression | none | zlib and zstd, always both | per asset: the cooked header records the codec (Z2-D-004) |
| RHI | none | Windows: Vulkan and D3D12; Android: Vulkan | run time on Windows (`rhi.backend`) |
| Online | none | Offline (Stage 4); Steam on Windows and GooglePlay on Android (Stage 6); every backend the preset supports is linked | configuration (`online.backend`) |

- [Z2-N-ARCH-269] The conformance executables (`ZenithECS_Conformance`, `ZenithPhysics_Conformance`, `ZenithCompression_Conformance`, `ZenithRHI_Conformance`, `ZenithOnline_Conformance`) link every backend of their family that the preset supports and run every scenario on each (Architecture/BuildSystem.md → "The zenith_add_library helper").
- [Z2-N-ARCH-270] Physics determinism is promised per backend: each physics backend, run through the shared conformance suite, produces bit-identical results across worker counts for the same inputs, and no result is compared across backends (Z2-D-003).
- [Z2-N-ARCH-271] Compression output is deterministic per codec and codec version, and every cooked-format header records the codec ID so a reader selects the codec from the data, never from configuration (Z2-D-004).
- [Z2-N-ARCH-275] A backend factory may write a third-party library's process-wide hook pointers (Jolt's allocation and trace hooks, `JPH::Factory::sInstance`) only inside the construction and destruction of its backend object, under a one-instance-per-process refusal (`PHYSICS_ERROR_BACKEND_ALREADY_ACTIVE` for the physics backends); this is a recorded exception to Z2-D-046 for third-party state, which the writable-data scan does not see because third-party source roots are outside its scope (Zenith2/Docs/Contracts.md [Z2-N-CONTRACT-151] step 1; Zenith2/Docs/Design/PHYS/Backends.md).

### Creating a game

`Tools/NewGame/` builds `ZenithNewGame`, a host CLI; its scaffolding code is the static library `ZenithNewGameScaffold` in `Tools/NewGameScaffold/`, which the editor's new-project wizard links, so both produce identical results.

```text
ZenithNewGame create <Name> [--template <Blank|3D|2D>] [--modules <A,B,…>] [--ecs <Custom|Flecs|EnTT>]
                            [--physics <Jolt|PhysX>] [--platforms <windows,android>] [--application-id <id>]
ZenithNewGame validate <Name>          # runs the game.json and schema validators without configuring
ZenithNewGame --migrate <Name>         # upgrades game.json to the current schemaVersion (preview, backup, journaled write)
```

`create` writes:

| File | Content |
|---|---|
| `Games/<Name>/CMakeLists.txt` | `zenith_add_game(<Name>)` |
| `Games/<Name>/game.json` | the schema of Architecture/BuildSystem.md → "Games in CMake", filled from the arguments |
| `Games/<Name>/Scenes/Main.scene.json` | a data-only scene: an entity `Main` with its transform (Blank), entities `Camera` and `Sun` with transforms (3D), or an entity `Camera` with a transform (2D); no behaviour attachment at Stage 2 |
| `Games/<Name>/Scripts/.keep` (Stage 2), `Games/<Name>/Scripts/Main.luau` (from Stage 3) | an empty script directory at Stage 2; from Stage 3, when the script runtime exists, a behaviour attached to `Main` (Blank) or `Camera` (3D, 2D), declaring Early scripts |
| `Games/<Name>/Schemas/.keep`, `Games/<Name>/Assets/Config/game.config.json` | an empty schema directory and the game configuration layer |
| `Games/<Name>/Tests/Boot.gametest.json` | the created game's headless boot test, 300 ticks |

- [Z2-N-ARCH-272] Project creation writes no C++ and no file outside `Games/<Name>/`; the created game configures, builds and boots its empty scene headless on the first run, which `Test_NewGame_CreatedGameBoots` (in `Tests/NewGame/`) verifies for each template.
- [Z2-N-ARCH-273] `ZenithNewGame create` refuses an existing directory, a name failing `^[A-Z][A-Za-z0-9]{0,31}$`, or a name equal to an engine library or tool, with exit code 2 and the reason.

## ZenithGraph shared core

The full specification is `Zenith2/Docs/Architecture/ZenithGraph.md`. The contract in summary:

| Topic | Contract | Normative IDs |
|---|---|---|
| role | one graph engine for the CPU frame graph (ZenithJobs) and the GPU render graph (ZenithFlux); backend meaning is opaque integers | 311–312 |
| API | `Zenith_Graph`, `Zenith_GraphNodeHandle`, versioned `Zenith_GraphResourceHandle`, `Zenith_GraphAccessMode` (`READ`, `WRITE`, `READ_WRITE`, `WRITE_DISCARD`), `AddNode`, `AddResource`, `DeclareRead`, `DeclareWrite`, `DeclareAccess`, `AddOrdering`, `MarkOutput`, `Compile` → `Zenith_GraphSchedule`, `WriteJson`; arena-only, no exceptions, sticky declaration errors | 313–321 |
| hazards | RAW, WAR, WAW over the version chain and overlapping subranges; version forks are errors; deterministic | 322–328 |
| cycles | Tarjan components; one named cycle per component with each edge's reason, resource and version; every error in one pass | 329–331, 345–346 (front-end error mapping), 348 (detail-line forms), 349 (string-free error record; diagnostics in a caller-owned report) |
| schedule | Kahn order by critical-path priority then declaration index; bottom-level priorities; transitive reduction; dependency counts | 332–335 |
| lifetimes | first and last position per resource; the one input for GPU aliasing and CPU arena reuse | 336–337 |
| caching | structure hash excluding costs; reprioritisation without recompile | 338–339 |
| first writes | whole-resource or per-subresource first-write-discard flags; the GPU front end declares transients per subresource | 346, 350 |
| dump | `zenith2.graph-dump` version 1, deterministic | 340–341, 347 (front-end dumps embed the core under "core") |
| capacity and conformance | the schedule recompile within the row "CPU frame-graph schedule recompile" (the 10,000-node graph is a benchmark reference); one conformance suite run through the core, the CPU front end and the GPU front end | 342–344 |

## CPU frame graph and scheduler

The full specification is `Zenith2/Docs/Architecture/CPUFrameGraph.md`. The contract in summary:

| Topic | Contract | Normative IDs |
|---|---|---|
| layering | ZenithJobs schedules stages over opaque resources; ECS and Runtime translate systems and engine stages | 381 |
| no fibers, no blocking waits | edges, sub-job joins and continuations only; the development-build debugger stop the one bracketed exception | 382, 388, 394, 398 |
| frame structure | prologue, T fixed ticks, epilogue with extraction, and RENDER(k−1) concurrently; never more than one frame ahead; the engine stage nodes (`Assets.Service`, `Render.Seal`, the Flux render nodes) | 383–386, 437–438 (the frame prologue), 441 (`Assets.Service`) |
| job system | work-stealing pool, main-thread and render-submit affinity, bounded queues with declared full policies, background jobs in 2 ms slices | 387–394, 434–435 (audio update) |
| sub-jobs | `ParallelFor` joined before node completion; deterministic merges; continuations | 395–398 |
| access declaration | `Zenith_SystemBuilder` component and property read/write sets, world resources, `Before`/`After`, per-property expansion | 399–401 |
| hazards | stage order, same-stage reader after writer unless `Before`, arbitration nodes, groups per world | 402–405 |
| schedule | compiled once, cached by structure hash, recompiled on the listed triggers, swapped in the prologue | 406–410, 440 (string-free compile error; diagnostics from the builder) |
| execution and clock | priority-ordered ready pushes, cancellable instances, fixed ticks, bounded catch-up, interpolation | 411–415, 436 (catch-up from the JOBS row) |
| pipelining | `Flux_RenderWorld` hand-off: copied values only, immutable after seal, two-slot ring, one frame of CPU latency accounted in the input-latency row | 416–422 |
| worlds, shutdown, profiling, tests | per-world groups; orderly drain and join; a zone per node and sub-job; the test table | 423–430, 431–433 |

## Frame phases and authority

The full specification is `Zenith2/Docs/Architecture/FramePhases.md`. The contract (Z2-D-080; D-PHASES refinement, open, Stage 1) in summary:

| # | Phase | Owns | Publishes at its end |
|---|---|---|---|
| 1 | Input | action state, per-player input contexts | input commands (replay-logged) |
| 2 | Early scripts | gameplay intent: desired velocity, requested actions | command and event buffers, merged deterministically |
| 3 | Animation | poses, root-motion extraction (not application) | root-motion deltas |
| 4 | Movement arbitration | the character-controller move: root motion, script intent and constraints under the arbitration rules | kinematic targets |
| 5 | Physics | dynamic-body transforms, contacts and triggers | collision and trigger events |
| 6 | Post-physics | ragdoll ↔ animation blending, physical-animation sync | final poses |
| 7 | AI and navigation | agent decisions and path requests (asynchronous results at tick boundaries) | AI commands |
| 8 | Transform propagation | world transforms from local transforms and hierarchy | transform change bits |
| 9 | Events | delivery of the merged event queue | — |
| 10 | Late scripts | reactions to events and final gameplay state | command buffers (applied at the next sync point) |
| 11 | Save-snapshot point | the consistent world snapshot for saves and replays | — |
| 12 | Render extraction | the immutable `Flux_RenderWorld` | the hand-off to rendering (pipelined) |

| Topic | Contract | Normative IDs |
|---|---|---|
| phases | fixed order every tick; parallel within a phase by access; behaviours in 2 or 10; phases 11 and 12 read-only | 471–478, 501–504 (chunk systems Early/Late in v1) |
| sync points | between every pair of phases; playback in merge order; compaction only at sync points | 479–481, 505 (tick boundary steps), 507 (paused worlds publish nothing), 508 (property-only tick-boundary access) |
| authority | one owning phase (or `CommandsOnly`) per field, per-field checks at schedule compile, writes outside the owner through commands, two direct writers an error without arbitration | 482–488 |
| commands | applied at the next sync point; last in merge order wins; stale handles dropped with a counter | 489–492 |
| arbitration | default precedence cinematic > ragdoll > physics > animation > script; a deterministic merge node per arbitrated field | 493–497 |
| errors | fixed message formats; all errors in one pass; diagnostics held by the system registry; checks in every configuration | 498–500, 506 |

## Lifecycle commands

The full specification is `Zenith2/Docs/Architecture/LifecycleCommands.md`. The contract in summary:

| Command | Publication point | Test | Normative IDs |
|---|---|---|---|
| Spawn | the sync point after the issuing phase; visible to the next phase; IDs in merge order | `Test_Lifecycle_Spawn_VisibleToNextPhase` | 545–546 |
| Destroy | marked at that sync point; removed at the end of the tick; handles stale from the next tick; the parent relation is ECS's | `Test_Lifecycle_Destroy_DeferredToTickEnd` | 547–549, 569–571 |
| Teleport | that sync point; resets physics velocity, interpolation, animation root and motion-vector history | `Test_Lifecycle_Teleport_ResetsAllHistories` | 550–551, the next phase of the same tick |
| Ragdoll enter and recover | that sync point; ownership blended animation ↔ physics in phase 6 | `Test_Lifecycle_Ragdoll_EnterAndRecoverBlend` | 552–553, 564–565; phase 5 of the same tick when issued in phases 1–4, else of the next tick |
| Cinematic start, interrupt and skip | that sync point; precedence while playing; gameplay state restored from the pre-cinematic snapshot at the next tick boundary | `Test_Lifecycle_Cinematic_SkipRestoresGameplayState` | 554–556 |
| World transition | the new world loads in isolation; its load completion is a logged publication; swap in prologue step 7 recorded as `WORLD_SWAP`; stale async results discarded | `Test_Lifecycle_WorldTransition_SwapsAtTickBoundary` | 557–559, swap in the frame prologue, 566–567 |
| Shutdown | when the current frame-loop iteration returns: every tick it started drains, no further tick starts, flush, tear down in reverse phase order (the asynchronous result table last) | `Test_Lifecycle_Shutdown_ReversePhaseTeardown` | 560–561 |

The five scenario tests — teleport during a ragdoll, skipping a cinematic mid-blend, a spawn and a destroy in the same tick, a world transition with in-flight asynchronous results, and saving at the snapshot point during streaming with animation and physics active — are specified in Architecture/LifecycleCommands.md → "Scenario tests" (normative IDs 562–563); listeners and their dispatch order are 541–544.

## Data-defined components

The full specification is `Zenith2/Docs/Architecture/DataDefinedComponents.md`. The contract (Z2-D-020) in summary:

| Topic | Contract | Normative IDs |
|---|---|---|
| one front end | `Games/<Name>/Schemas/*.schema.json`; indistinguishable from C++-registered types to every consumer; engine and module components are C++-registered structs | 591–592, 614 |
| identity | type ID = `Zenith_Hash64(stableName)`; field IDs explicit and never reused (`retiredFieldIds`); schema `version` | 593–594 |
| types and limits | primitives, vectors, quaternions, colours, interned strings, persistent IDs, entity and asset handles, fixed arrays, nested structs, out-of-line containers; 128 fields, 1,024 inline bytes, depth 4 | 595–597 |
| registration | cooked `schemas.zreg`, validated again at load, registered after engine and module types, before `Freeze`; diagnostics held by the type registry | 598–600, 613 |
| storage | Custom per-field columns; flecs runtime components; EnTT dynamic-storage adapter (a flagged cost); generated lifecycle hooks | 601–602, 609 (interned strings are plain values) |
| Luau | generated `.d.luau` from the frozen registry; names verbatim | 603–604 |
| migration | an authoring-data migration: declarative ops, fixtures per version, saves migrated forward | 605–607 |
| tests | the test table | 608, 610 (event schemas) |

## Reflection and runtime type registration

### One reflection system

`ZenithReflect` is the one reflection system (Zenith2/Docs/Contracts.md → "Reflection"): explicit registration, stable type and property IDs, schema versions, and runtime type registration so that data-defined components are ordinary registered types.

- [Z2-N-ARCH-641] Every reflected type — engine and module types registered from C++, and game types registered from schemas — lives in one `Zenith_TypeRegistry` owned by the composition root, and every consumer reads types only through it.
- [Z2-N-ARCH-642] Registration is explicit and ordered (engine libraries, then modules in canonical order, then schemas) and completes with `Freeze`; nothing registers through static initialisation, and after `Freeze` the registry is immutable except through the hot-reload window of bullet 650 and safe to read from any thread (Architecture/CompositionRoot.md → "Startup sequence").

### Identifiers

| Identifier | Type | Derivation | Stability rule |
|---|---|---|---|
| type ID | `Zenith_TypeID` (64-bit) | `Zenith_Hash64` of the registered stable name (`"Zenith.Transform"`, `"OpenWorldSlice.Stats"`), never of the C++ type name | the stable name never changes; a collision is a registration error naming both types |
| property ID | `Zenith_PropertyID` (32-bit) | declared explicitly per property (a `uPROP_*` constant in C++, `id` in a schema) | never reused within its type, including after removal |
| schema version | `uint32` | declared per type | incremented by any change to the property set or a property's type; each increment ships a migration step |

### Registry types

```cpp
// Engine/ZenithReflect/Zenith_TypeInfo.h
enum Zenith_TypeKind : uint8
{
	TYPE_KIND_COMPONENT,          // stored in ECS worlds
	TYPE_KIND_STRUCT,             // nested value type
	TYPE_KIND_EVENT,              // emitted through event buffers
	TYPE_KIND_RESOURCE,           // a world resource (physics world, navmesh, audio mixer, …)
	TYPE_KIND_BEHAVIOUR_STATE,    // a Luau behaviour's declared state table (Zenith2/Docs/Scripting.md → "Behaviour state model and recovery")
	TYPE_KIND_CONFIG,             // a configuration block
	TYPE_KIND_ASSET,              // an asset type's reflected header data
};

enum Zenith_TypeOrigin : uint8 { TYPE_ORIGIN_CPP, TYPE_ORIGIN_SCHEMA };

enum Zenith_PropertyValueType : uint8
{
	PROPERTY_TYPE_BOOL, PROPERTY_TYPE_I8, PROPERTY_TYPE_I16, PROPERTY_TYPE_I32, PROPERTY_TYPE_I64,
	PROPERTY_TYPE_U8, PROPERTY_TYPE_U16, PROPERTY_TYPE_U32, PROPERTY_TYPE_U64, PROPERTY_TYPE_F32, PROPERTY_TYPE_F64,
	PROPERTY_TYPE_VEC2, PROPERTY_TYPE_VEC3, PROPERTY_TYPE_VEC4, PROPERTY_TYPE_QUAT, PROPERTY_TYPE_COLOUR,
	PROPERTY_TYPE_STRING_ID, PROPERTY_TYPE_PERSISTENT_ID, PROPERTY_TYPE_ENTITY, PROPERTY_TYPE_ASSET,
	PROPERTY_TYPE_FIXED_ARRAY, PROPERTY_TYPE_STRUCT, PROPERTY_TYPE_CONTAINER,
	PROPERTY_TYPE_ENUM,           // value 23: a reflected C++ enumeration stored as its underlying integer (C++ types only)
};

struct Zenith_PropertyInfo
{
	Zenith_PropertyID m_xID;
	const char* m_szName;                  // the script-visible name
	Zenith_PropertyValueType m_eType;
	Zenith_TypeID m_xElementType;          // struct, container element, asset kind; zero otherwise
	uint32 m_uOffset;                     // in the type's in-memory layout
	uint32 m_uSize;
	uint32 m_uAlignment;
	uint32 m_uArrayCount;                 // fixed arrays; 1 otherwise
	uint32 m_uMaxCount;                   // containers
	Zenith_Phase m_eAuthority;             // Architecture/FramePhases.md → "Per-field authority"
	uint32 m_uFlags;                      // PROPERTY_FLAG_SERIALIZED, _REPLICATED, _RESTORE_AFTER_CINEMATIC, _EDITOR_HIDDEN, …
	Zenith_ByteSpan m_xDefault;            // default value bytes
};

struct Zenith_TypeLifecycleHooks
{
	void (*m_pfnConstruct)(void* pDestination, uint32 uCount, void* pUserData);
	void (*m_pfnDestroy)(void* pObjects, uint32 uCount, void* pUserData);
	void (*m_pfnCopy)(void* pDestination, const void* pSource, uint32 uCount, void* pUserData);
	void (*m_pfnMove)(void* pDestination, void* pSource, uint32 uCount, void* pUserData);
	void* m_pUserData;                     // passed back as every hook's pUserData; stored verbatim, never dereferenced
};

// Contract: IMMUTABLE after Zenith_TypeRegistry::Freeze. Members non-const: the registry's builders write them after
// construction, during registration and in a hot-reload window.
struct Zenith_TypeInfo
{
	Zenith_TypeID m_xID;
	const char* m_szStableName;
	const char* m_szScriptName;
	uint32 m_uVersion;
	Zenith_TypeKind m_eKind;
	Zenith_TypeOrigin m_eOrigin;
	uint32 m_uSize;
	uint32 m_uAlignment;
	Zenith_Span<const Zenith_PropertyInfo> m_axProperties;   // ascending property ID
	Zenith_TypeLifecycleHooks m_xHooks;                      // trivial hooks are null
	Zenith_Phase m_eResourceOwningPhase;                     // TYPE_KIND_RESOURCE
	Zenith_NetworkAuthority m_eNetworkAuthority;             // § "Multiplayer foundations"
	const Zenith_ArbitrationDecl* m_pxArbitration;           // null when none
	const Zenith_MigrationChain* m_pxMigrations;
};
```

- [Z2-N-ARCH-985] Every caller of a `Zenith_TypeLifecycleHooks` member passes that hooks value's `m_pUserData` as the hook's last parameter, `pUserData`; the reflection registry stores `m_pUserData` verbatim and never dereferences it. C++ types' hooks carry a null `m_pUserData`; a schema type's hooks are the four constant generic functions of ZenithReflect with `m_pUserData` naming the type's registry-owned record, so the schema hooks need no per-type thunk slot table and no `ForbiddenCheck` writable-data allowlist entry (Zenith2/Docs/Contracts.md → "No mutable global state").

### Registration API

```cpp
// Engine/ZenithReflect/Zenith_TypeRegistry.h
// Contract: MAIN_THREAD until Freeze; ANY_THREAD and IMMUTABLE afterwards except inside a hot-reload window
// (bullet 650), which ZenithRuntime alone opens in frame-prologue step 6. Owned by the composition root. Members
// non-const: registration and hot reload write them after construction.
class Zenith_TypeRegistry
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_TypeRegistry>, Zenith_ReflectError> Create(Zenith_Heap& xHeap);

	template<typename T>
	Zenith_TypeBuilder& BeginComponent(const char* szStableName, uint32 uVersion);
	template<typename T>
	Zenith_TypeBuilder& BeginType(Zenith_TypeKind eKind, const char* szStableName, uint32 uVersion);

	Zenith_Result<void, Zenith_SchemaError> RegisterSchemaJson(Zenith_ByteSpan xJsonBytes, Zenith_StringView strSourcePath);
	Zenith_Result<void, Zenith_SchemaError> RegisterSchemaBlob(Zenith_ByteSpan xBlobBytes);
	Zenith_Span<const Zenith_SchemaDiagnostic> GetSchemaDiagnostics() const;   // MAIN_THREAD; of the latest registration

	// Startup (Architecture/CompositionRoot.md → "Startup sequence", steps 6 and 10); MAIN_THREAD.
	void AttachConfigSchema(Zenith_ConfigSchema& xSchema);       // step 6, before engine and module declarations
	Zenith_ConfigSchema& GetConfigSchema();        // the attached schema, until it is frozen
	void BindStringTable(Zenith_StringTable& xStrings);          // step 10: interns every string default declared so far
	void InstallValueKindHooks(Zenith_PropertyValueType eType, const Zenith_ValueKindHooks& xHooks);   // step 10, before the first schema
	// Step 11 (and the cook after its Freeze), FROZEN, before any world: binds the value kind's resolver and resolves every
	// non-null reference default of that kind; an unresolvable default is SCHEMA_ERROR_BAD_DEFAULT (startup exit code 3).
	Zenith_Result<void, Zenith_SchemaError> ResolveReferenceDefaults(Zenith_PropertyValueType eType,
		Zenith_PfnConstructFromPersistentID pfnConstruct, void* pUserData);
	bool AreReferenceDefaultsResolved() const;    // a world desc is refused while false

	Zenith_Result<void, Zenith_ReflectError> Freeze();
	bool IsFrozen() const;

	// The hot-reload window (main thread, frame prologue): replaces TYPE_KIND_BEHAVIOUR_STATE types, and in the editor
	// between play sessions TYPE_ORIGIN_SCHEMA types, by publishing a new registry revision.
	Zenith_Result<void, Zenith_ReflectError> BeginHotReload();
	Zenith_Result<uint64, Zenith_ReflectError> EndHotReload();     // validates, publishes; returns the revision
	void AbandonHotReload();                                                      // discards every change of the window
	uint64 GetRevision() const;

	const Zenith_TypeInfo* FindType(Zenith_TypeID xID) const;               // null if absent
	const Zenith_TypeInfo* FindTypeByStableName(Zenith_StringView strName) const;
	Zenith_Span<const Zenith_TypeInfo* const> GetTypes() const;           // byte order of stable name
};

// Contract: OWNER_THREAD; valid until End().
class Zenith_TypeBuilder
{
public:
	template<typename TOwner, typename TField>
	Zenith_PropertyBuilder& Property(Zenith_PropertyID xID, const char* szName, TField TOwner::* pMember);
	Zenith_TypeBuilder& DefaultAuthority(Zenith_Phase ePhase);
	Zenith_TypeBuilder& NetworkAuthority(Zenith_NetworkAuthority eAuthority);
	Zenith_TypeBuilder& Arbitration(Zenith_Span<const Zenith_PropertyID> axProperties, Zenith_ArbitrationRule eRule,
		Zenith_Span<const Zenith_WriterSource> axOrder);
	Zenith_TypeBuilder& ArbitrationCustom(Zenith_Span<const Zenith_PropertyID> axProperties, Zenith_PfnArbitrationMerge pfnMerge);
	Zenith_TypeBuilder& Migration(uint32 uFromVersion, Zenith_PfnMigrationStep pfnStep);
	Zenith_TypeBuilder& ScriptName(const char* szName);
	void End();
};

// Contract: OWNER_THREAD; valid until its type builder's End().
class Zenith_PropertyBuilder
{
public:
	Zenith_PropertyBuilder& Authority(Zenith_Phase ePhase);          // overrides the type's default authority
	Zenith_PropertyBuilder& Flags(uint32 uPropertyFlags);           // PROPERTY_FLAG_*
	Zenith_PropertyBuilder& Default(Zenith_ByteSpan xValue);         // in the property's reflected value type
	Zenith_PropertyBuilder& Range(float fMin, float fMax);            // numeric properties
	Zenith_PropertyBuilder& MaxCount(uint32 uMaxCount);             // containers
	Zenith_PropertyBuilder& ScriptName(const char* szName);
	Zenith_PropertyBuilder& RestoreAfterCinematic();
	Zenith_TypeBuilder& EndProperty();
};
```

- [Z2-N-ARCH-643] Every consumer of type metadata — serialization, the inspector, migration, `.d.luau` generation, chunk-system column access, schedule authority checks, network replication and render extraction — works from `Zenith_TypeInfo` and `Zenith_PropertyInfo` alone and never branches on `m_eOrigin` (Z2-D-020).
- [Z2-N-ARCH-644] Serialization reads and writes property values by property ID through the registry, never by copying an object's memory layout, so a layout change never changes a serialized format (Z2-D-038).
- [Z2-N-ARCH-645] A type whose properties carry lifetime (asset handles and containers; interned string IDs are plain values) registers non-null lifecycle hooks, and every store of such a type (ECS columns, shadows, snapshots, save buffers) copies, moves and destroys through them.
- [Z2-N-ARCH-646] `Freeze` validates the whole registry in one pass — ID uniqueness, stable-name uniqueness, missing authorities, arbitration validity, migration-chain completeness, lifecycle-hook presence — and reports every error; a frozen registry never contains a type that failed validation.
- [Z2-N-ARCH-647] Looking up type metadata by `Zenith_TypeID` is data lookup and is allowed; the registry never returns a service object, and no subsystem uses it to find another subsystem (Zenith2/Docs/Contracts.md → "No mutable global state").
- [Z2-N-ARCH-648] The editor re-registers changed schemas through the hot-reload window between play sessions, after migrating every open document; a running play world never observes a schema-type change.
- [Z2-N-ARCH-650] After `Freeze`, the registry changes only inside a hot-reload window that `ZenithRuntime` opens on the main thread in step 6 of the frame prologue (Architecture/CPUFrameGraph.md → "Frame structure"; Architecture/CompositionRoot.md → "The frame loop"; Zenith2/Docs/Contracts.md → "Reflection"): the window admits replacement of `TYPE_KIND_BEHAVIOUR_STATE` types during play (Zenith2/Docs/Scripting.md → "Quarantine and hot reload") and of schema types in the editor between play sessions, and nothing else.
- [Z2-N-ARCH-651] `EndHotReload` validates the changed types as `Freeze` does and publishes a new registry revision atomically; `Zenith_TypeInfo` pointers of the previous revision stay valid for two further frames and are then retired, and a validation failure publishes nothing and leaves the previous revision in effect.
- [Z2-N-ARCH-652] Every reflected component, struct and event type that a module registers from C++ sets an explicit script name with `ScriptName`, unique programme-wide and derived from its C++ type name by rule 1 of Zenith2/Docs/Scripting/Bindings.md → "Name mapping from Zenith Hungarian" (for example `Zenith_RigidBody2DComponent` → `RigidBody2D`), so that two modules' types never fail `Freeze` with `REFLECT_ERROR_BAD_NAME` in a registry that holds both; MOD's integration suite registers the engine types and every module's types into one registry and freezes it, so a clash fails a gate instead of a game's start (ruling X353-SCRIPTNAMES, 2026-10-10; cross-doc item X353).

### Reflection error codes

```cpp
enum Zenith_ReflectErrorCode : uint16
{
	REFLECT_ERROR_TYPE_ID_COLLISION = 1,
	REFLECT_ERROR_DUPLICATE_STABLE_NAME,
	REFLECT_ERROR_DUPLICATE_PROPERTY_ID,
	REFLECT_ERROR_MISSING_AUTHORITY,
	REFLECT_ERROR_BAD_AUTHORITY,
	REFLECT_ERROR_ARBITRATION_TYPE,
	REFLECT_ERROR_MIGRATION_CHAIN,
	REFLECT_ERROR_MISSING_HOOKS,
	REFLECT_ERROR_FROZEN,                 // a registration after Freeze outside a hot-reload window
	REFLECT_ERROR_HOT_RELOAD_KIND,        // a hot-reload window change to a type kind it does not admit
	REFLECT_ERROR_UNKNOWN_ELEMENT_TYPE,
	REFLECT_ERROR_LAYOUT,                 // value 12: a component above 1,024 bytes or 16-byte alignment, or overlapping C++ properties
	REFLECT_ERROR_BAD_NAME,               // a stable or script name that fails its grammar
	REFLECT_ERROR_OUT_OF_MEMORY,
	REFLECT_ERROR_BAD_DEFAULT,            // value 15: a default that is not a value of its property's type
};

// The error record of [Z2-N-CONTRACT-480]. The text naming both parties of a collision, or the missing piece,
// is logged at the point of failure ([Z2-N-CONTRACT-464]), not carried.
struct Zenith_ReflectError
{
	Zenith_ReflectErrorCode m_eCode;
	Zenith_PropertyID m_xProperty;        // zero when not applicable
	Zenith_TypeID m_xType;                // zero when not applicable
};
```

- [Z2-N-ARCH-649] The tests `Test_TypeRegistry_EachErrorCodeReported`, `Test_TypeRegistry_FrozenRejectsRegistration`, `Test_TypeRegistry_ConcurrentReadsAfterFreeze` (under `linux-tsan`), `Test_TypeRegistry_SerializationByPropertyID` and `Test_TypeRegistry_HooksBalanceReferences` exist in `Engine/ZenithReflect/Tests/` and assert what their names state.

## ECS model

The full specification is `Zenith2/Docs/Architecture/ECSModel.md`. The contract (Z2-D-039, Z2-D-040, Z2-D-051) in summary:

| Topic | Contract | Normative IDs |
|---|---|---|
| API and backends | one type-erased API keyed by reflection IDs; `Zenith_CreateWorld_Custom`, `_Flecs`, `_EnTT`; no backend type outside its `Internal/` | 681–686, 705 |
| identity | entity handles, stable simulation IDs, persistent IDs and network IDs are distinct; simulation IDs order everything | 687–688 |
| command buffers | per invocation, arena-backed, merged by (system order, logical invocation ID, sequence), reserved during the invocation, a merge node for every command-, event- or reduction-emitting and every script system, merge chains in the stage's cost-independent topological order | 689–692 |
| epochs and versions | structural epoch per chunk bumped only by structural playback; change versions per column set to precomputed merge versions V(t, m) at merge or by playback stamps; a commit never invalidates another invocation's view | 693–695, 723, 728–729 |
| views | scoped to their invocation ID; declared columns only; handle, string and container fields read-only or absent in chunk views | 696–699, 720–721 (per-row change bits) |
| queries | cached, refreshed at sync points, stable within a phase; storage order never observable | 700–702 |
| reductions | contributions folded in merge order through fixed-order trees | 703–704 |
| world registry, scheduling, script lifetimes | registry object owned by the root; entity-boundary rollback support; own-row rule; provisional spawn IDs; phase 12 rejected by `AddSystem`; resources own phases 1–11; the merge context and the design-declared members | 706–712, 722–723 (system registry, CommitRowChanges), 730–732 |
| diagnostics and tests | per-world counters; the conformance table on all three backends, with 1, 2, 8 and all workers, forced compaction and two capacities | 713–715, 716–719 (playback access, system context) |

## Worlds and isolation

A world is one independent simulation: its own ECS storage, physics world, script VMs, event and command queues, simulation clock state, random seed and frame-graph nodes. The editor runs an edit world and any number of play worlds; a world transition loads a new world beside the old one.

| State | Per world | Shared across worlds |
|---|---|---|
| ECS storage, entity handles, simulation-ID counter | yes | — |
| physics world, navigation queries, audio emitters and listeners | yes | the physics, AI and audio module services that own them |
| Luau VMs (the main VM, per-worker chunk-system VMs) | yes | the bytecode cache |
| event queues, command buffers, merge state | yes | — |
| tick index, pause, time scale, world seed | yes | the frame loop and its simulation clock rate |
| frame-graph nodes and resources | yes (one group per world) | engine stage nodes |
| asset handles and GPU resources | — | the asset registry and the RHI device (reference-counted, Zenith2/Docs/Contracts.md → "Handle kinds") |
| type registry | — | one, frozen |
| render world | one section per extracted world | one `Flux_RenderWorld` per frame |

- [Z2-N-ARCH-761] Worlds are isolated: no entity handle, component handle, chunk view, command buffer, script value or physics body of one world is valid in another, and resolving one in the wrong world fails with the wrong-world error in every configuration (Zenith2/Docs/Contracts.md → "Handle kinds").
- [Z2-N-ARCH-762] Worlds share no mutable simulation state; the only cross-world data paths are serialization by persistent ID (world transition carry-over, editor copy and paste between edit and play worlds) performed at tick boundaries.
- [Z2-N-ARCH-763] Each world is created and destroyed on the main thread at a frame boundary, receives a world tag from the `Zenith_WorldRegistry`, and contributes its own node group to the frame schedule (Architecture/CPUFrameGraph.md → "Worlds in the graph").
- [Z2-N-ARCH-764] A failure in one world (a script error, a quarantined behaviour, a merge rejection) affects no other world's tick, and pausing one world pauses no other.
- [Z2-N-ARCH-765] Destroying a world releases its references to shared assets and GPU resources through their reference counts and never forces their destruction; assets and resources outlive every world (Zenith2/Docs/Contracts.md → "Handle kinds").
- [Z2-N-ARCH-766] Editor play worlds are independent: starting play copies the edit world by serialization into a new world, stopping play destroys the play world, and nothing the play world did reaches the edit world.
- [Z2-N-ARCH-767] The test `Test_Worlds_TwoWorldsTickIndependently` (in `Tests/Determinism/`) runs two worlds of the same content with different pause and time-scale settings concurrently for 600 ticks and asserts that each world's tick hashes equal a solo run of that world.

## Multiplayer foundations

Networked multiplayer and dedicated servers are deferred to Stage 7 (Z2-D-073); their foundations are architecture now and are proven by a Stage 3 loopback. Local multiplayer and split-screen are excluded (Z2-D-076). The NET area owns `ZenithNet`; the command format is shared with input recording (SAVE).

### Network identity

<!-- elaboration -->
```cpp
// Engine/ZenithNet/Zenith_NetworkID.h
// A session-scoped replicated identity, distinct from world handles and persistent IDs.
struct Zenith_NetworkID
{
	uint64 m_ulValue = 0;   // bits 48–63: the allocating peer (0 = the server); bits 0–47: that peer's counter

	constexpr bool IsValid() const { return m_ulValue != 0; }
};

// Engine/ZenithReflect/Zenith_NetworkAuthority.h
enum Zenith_NetworkAuthority : uint8
{
	NETWORK_AUTHORITY_SERVER,        // the default: the server simulates; clients receive
	NETWORK_AUTHORITY_OWNER_CLIENT,  // the owning client proposes; the server validates and rebroadcasts
	NETWORK_AUTHORITY_LOCAL,         // never replicated (presentation-only state)
};
```

- [Z2-N-ARCH-781] Network IDs are distinct from entity handles, stable simulation IDs and persistent IDs; a world keeps a `Zenith_NetworkIDMap` from network ID to entity handle, and network messages carry network IDs only, never handles (Zenith2/Docs/Contracts.md → "Handle kinds").
- [Z2-N-ARCH-782] The authority model is server-authoritative by default: every component declares a network authority (`networkAuthority` in schemas, `NetworkAuthority` in C++ registration), defaulting to `NETWORK_AUTHORITY_SERVER`, and each replicated field declares `replicated`.

### Fixed ticks and replayable commands

Simulation runs on fixed ticks (Architecture/CPUFrameGraph.md → "Execution"), and gameplay input enters simulation as input commands produced by phase 1 and in no other form (Architecture/FramePhases.md → "The twelve phases").

<!-- elaboration -->
The replay log format `ZRPL`, owned by `ZenithSave` (`Engine/ZenithSave/Zenith_ReplayLog.h`) and shared by input recording, replays, the determinism tests and network command transport:

```cpp
// Engine/ZenithSave/Zenith_ReplayLog.h
enum Zenith_BackendID : uint8
{
	BACKEND_ID_NONE = 0,
	BACKEND_ID_ECS_CUSTOM = 1, BACKEND_ID_ECS_FLECS = 2, BACKEND_ID_ECS_ENTT = 3,
	BACKEND_ID_PHYSICS_JOLT = 1, BACKEND_ID_PHYSICS_PHYSX = 2,     // physics values share the numeric space of their own field
};

enum Zenith_ReplayRecordKind : uint8
{
	REPLAY_RECORD_WORLD_BEGIN = 1,   // a world session starts (the startup world, or the new world of a swap)
	REPLAY_RECORD_TICK = 2,          // one tick of one world
	REPLAY_RECORD_WORLD_SWAP = 3,    // a world transition was performed after the named tick
	REPLAY_RECORD_CHECKPOINT = 4,    // a phase-11 snapshot of one world after the named tick
};

enum Zenith_ReplayTickFlags : uint16
{
	REPLAY_TICK_FLAG_NONE = 0,
	REPLAY_TICK_FLAG_NON_REPLAYABLE = 1u << 0,   // any fatal, non-replayable diagnostic event in this tick (Scripting.md → "Budgets safepoints and the watchdog" lists them): a replay stops before it
};

// The values are assigned only by Zenith2/Docs/Scripting.md → "Asynchronous source kind registry" ([Z2-N-SCRIPT-924]);
// this declaration lists exactly its assigned and provisional rows and changes only in the diff that adds a row there.
enum Zenith_AsyncSourceKind : uint8
{
	ASYNC_SOURCE_MODULE = 0,         // any other module operation
	ASYNC_SOURCE_PATHFINDING = 1,
	ASYNC_SOURCE_STREAMING = 2,
	ASYNC_SOURCE_PHYSICS_QUERY = 3,
	ASYNC_SOURCE_ONLINE = 4,         // a mixed kind: script requests and ONLINE's prologue-allocated notifications
	ASYNC_SOURCE_SAVE = 5,
	ASYNC_SOURCE_WORLD_LOAD = 6,
	ASYNC_SOURCE_PLATFORM = 7,       // a world-visible platform event (lifecycle, thermal, account, device, disk, locale, configuration)
	ASYNC_SOURCE_SCENE_LOAD = 8,     // an additive scene load: the 8-byte Zenith_SceneLoadResult payload in both publication kinds
	ASYNC_SOURCE_ANIMATION = 9,      // ANIM's asset bindings and clip-chunk arrivals
	ASYNC_SOURCE_AUDIO = 10,         // provisional: AUDIO's script notifications
	ASYNC_SOURCE_UI = 11,            // UI events published as recorded results
	ASYNC_SOURCE_CINEMATICS = 12,    // CINE's sequence and video bindings
	ASYNC_SOURCE_NAVIGATION = 13,    // AI's navigation-file loads
	ASYNC_SOURCE_PACKAGE = 14,       // PACK's package, update and DLC events (prologue, never under --replay)
	ASYNC_SOURCE_NETWORK = 15,       // NET's snapshot fragments and session events (prologue, never under --replay)
	ASYNC_SOURCE_XR = 16,            // ZenithXR's notifications (Stage 7; prologue, never under --replay)
};
inline constexpr uint32 uASYNC_SOURCE_KIND_CAPACITY = 32;   // the source records of each world's Zenith_AsyncResultTable

enum Zenith_PublicationKind : uint8
{
	PUBLICATION_KIND_GENERIC_RECOMPUTED = 0,   // the result is recomputed deterministically on replay; no payload is recorded
	PUBLICATION_KIND_GENERIC_RECORDED = 1,     // the result depends on the outside world (I/O, online); its payload is recorded
	PUBLICATION_KIND_WORLD_LOADED = 2,         // a transition target finished loading (payload: the operation's uint64 issue ID)
	PUBLICATION_KIND_PLATFORM_RECORDED = 3,    // a platform event: its 36-byte payload is recorded and a replay publishes it
};

// One publication inside a REPLAY_RECORD_TICK record: this 16-byte header, then m_uPayloadBytes of payload.
// Its publish tick is the enclosing TICK record's tick index. Serialized field by field, little-endian.
struct Zenith_ReplayPublicationHeader
{
	uint64 m_ulCompletionID;
	uint8 m_uKind;                  // Zenith_PublicationKind
	uint8 m_uSourceKind;            // Zenith_AsyncSourceKind
	uint16 m_uReserved;             // 0
	uint32 m_uPayloadBytes;         // 0 for PUBLICATION_KIND_GENERIC_RECOMPUTED
};
```

The file header:

| Field | Type | Meaning |
|---|---|---|
| magic | `uint32` | `ZRPL` |
| format version | `uint16` | 1 |
| flags | `uint16` | bit 0: the log contains `REPLAY_RECORD_CHECKPOINT` records (set when the run records with `replay.checkpointIntervalTicks` above 0, and then at least one checkpoint record follows) |
| build ID | 16 bytes | the executable's build ID (not the composition's `m_ulBuildIdentity`) |
| composition hash | `uint64` | `Zenith_GameComposition::m_ulCompositionHash` |
| content hash | `uint64` | the cooked-content hash |
| ECS backend, physics backend | `uint8` each | `Zenith_BackendID`; replays are compared per backend |
| tick rate | `uint32` | `sim.tickHz` at recording |
| simulation-affecting configuration | length-prefixed key/value list | every configuration key registered as simulation-affecting |

Then records, each `{ uint8 kind; uint32 byteLength; payload }`:

| Record | Payload |
|---|---|
| `REPLAY_RECORD_WORLD_BEGIN` | world tag `uint16`; world seed `uint64`; scene persistent ID (16 bytes); scene content hash `uint64`; the new world's tick index starts at 0. A `WORLD_BEGIN` immediately followed, before any `TICK` of the same world tag, by a `REPLAY_RECORD_CHECKPOINT` of that tag at tick t means the world was created from a save, a resume or a seek restore: the checkpoint is a one-world `SAVE_KIND_REPLAY_CHECKPOINT` image captured out of tick after the restore completed (codec NONE, sequence 0, UTC seconds 0), the world's first `TICK` is t + 1, and a replay restores the checkpoint instead of loading the scene |
| `REPLAY_RECORD_TICK` | world tag `uint16`; flags `uint16` (`Zenith_ReplayTickFlags`); tick index `uint64` (strictly increasing per world); next completion ID `uint64` (the world's `Zenith_AsyncResultTable` counter at the tick boundary, after every prologue allocation of the frame and before the tick's first merge-chain allocation); input command count `uint16` then commands `{ uint16 player slot; uint32 action ID; uint32 sequence; 8 bytes quantised value }` in (player slot, sequence) order; publication count `uint16` then publications (`Zenith_ReplayPublicationHeader` then payload) in completion-ID order; tick hash `uint64` (Zenith2/Docs/Scripting.md → "Determinism rules and randomness") |
| `REPLAY_RECORD_WORLD_SWAP` | old world tag `uint16`; old world's last executed tick `uint64`; new world tag `uint16`; new world's scene content hash `uint64`; followed in the log by the new world's `WORLD_BEGIN` |
| `REPLAY_RECORD_CHECKPOINT` | world tag `uint16`; tick index `uint64` (the snapshot is of the state after that tick); snapshot length `uint32`; the phase-11 snapshot bytes (the save format of ZenithSave) |

All integers are little-endian; a reader skips unknown record kinds and unknown trailing bytes of a record by its byte length. One log covers one Player run: the startup world's `WORLD_BEGIN` opens it, and every transition appends a `WORLD_SWAP` and the next `WORLD_BEGIN` (Architecture/LifecycleCommands.md → "World transition").

The world's pending-result set and its monotonic completion-ID counter belong to `Zenith_AsyncResultTable` (ZenithScript, `Zenith_ScriptAsync.h`), one per `Zenith_ScriptWorld`, so every world has one: sources are declared per `Zenith_AsyncSourceKind` (`DeclareSource`), completion IDs are allocated per kind (`AllocateCompletionID`), results arrive from any thread through `Complete`, the tick boundary publishes them in completion-ID order through each kind's handler, the replay recorder observes publications and the replay reader supplies them (Zenith2/Docs/Scripting.md → "Merge-time capacity and asynchronous results"). An additive scene load publishes as `ASYNC_SOURCE_SCENE_LOAD`: success as `PUBLICATION_KIND_GENERIC_RECOMPUTED` (a replay recomputes the preparation, which yields the same instance ID; nothing is recorded), failure as `PUBLICATION_KIND_GENERIC_RECORDED` with the `Zenith_SceneLoadResult` `{ uint32 m_uInstance; uint8 m_uCode; uint8 m_uDetail; uint16 m_uReserved; }` recorded, so a replay publishes the recorded failure even when its own preparation succeeded (`Zenith2/Docs/Design/SCENE.md`).

- [Z2-N-ARCH-792] Every asynchronous result that reaches a world is completed into that world's `Zenith_AsyncResultTable` under a declared `Zenith_AsyncSourceKind`, and no other object publishes into a world; the source-kind values of the enumeration are assigned only by Zenith2/Docs/Scripting.md → "Asynchronous source kind registry" ([Z2-N-SCRIPT-924], [Z2-N-SCRIPT-925]), never renumbered, and at most `uASYNC_SOURCE_KIND_CAPACITY` (32) kinds exist.
- [Z2-N-ARCH-791] A world-visible platform event reaches simulation only as an `ASYNC_SOURCE_PLATFORM` publication of kind `PUBLICATION_KIND_PLATFORM_RECORDED` at a tick boundary; its 36-byte little-endian payload (`uint16` the `Zenith_PlatformEventKind`, `uint16` 0, then 32 kind-specific bytes with platform-scoped IDs zeroed; the layout is `Zenith2/Docs/Design/PLAT.md` → "Threading lifetime and error contract") is recorded in the log, and a replay publishes the recorded payloads at the recorded ticks and ignores live platform events: under `--replay` the PLAT bridge allocates no completion ID for a live event, and the world's counter follows the recorded next completion ID of each `TICK` record ([Z2-N-ARCH-794]).
- [Z2-N-ARCH-794] Every `REPLAY_RECORD_TICK` record carries the world's next completion ID at the tick boundary (after every frame-prologue allocation of the frame and before the tick's first merge-chain allocation); under `--replay` the world's `Zenith_AsyncResultTable` adopts the recorded value at that point, and a live counter above the recorded value is a replay divergence that halts the world with a diagnostic naming the tick, the recorded and the live value (Zenith2/Docs/Scripting.md [Z2-N-SCRIPT-341]).
- [Z2-N-ARCH-783] Gameplay input reaches simulation only as replayable input commands logged at phase 1, and asynchronous results reach it only as publications logged at tick boundaries, so a replay of the log reproduces the run bit for bit on the same build and backend (Z2-D-019, Z2-D-053).
- [Z2-N-ARCH-784] Replaying a log whose build ID, composition hash, content hash, backends or tick rate (SAVE's `REPLAY_IDENTITY_FIELD_TICK_RATE`, the header's `sim.tickHz`) differ from the running game is refused with exit code 5 and a message naming the mismatched field.
- [Z2-N-ARCH-789] A position in a replay log is the pair (ordinal of the `WORLD_BEGIN` record counted from 0 at the start of the file, tick index); a save written during a recorded run stores that pair in its header, and resuming a replay from the save seeks to it; a replay resumed from an EXACT save (no behaviour suspended mid-`Run`, no quarantined instance or instance of a disabled or disabling module, no in-flight additive load, every participant EXACT) compares its tick hashes with the recording, while one resumed from a COHERENT save (a behaviour suspended mid-`Run`, a quarantined or disabled instance, a loading additive instance, PhysX) runs without hash comparison and reports that, the save's coherence being checked by SAVE's coherence assertions (`Zenith2/Docs/Design/SAVE.md` → "Reproducibility boundaries").
- [Z2-N-ARCH-790] A replay stops before a `TICK` record whose flags carry `REPLAY_TICK_FLAG_NON_REPLAYABLE`, reports that tick, and exits with code 5.
- [Z2-N-ARCH-788] A world-transition load completion is logged as a `PUBLICATION_KIND_WORLD_LOADED` publication at the tick boundary that publishes it, and the swap is logged as a `REPLAY_RECORD_WORLD_SWAP` naming the old world's last executed tick; a replay swaps after exactly that tick.

### Headless execution

- [Z2-N-ARCH-785] A headless runtime configuration runs every simulation phase without Flux, the audio output device or platform windowing (Architecture/CompositionRoot.md → "Startup sequence"), and it is the configuration the loopback server, the device-free CI integration smoke and the determinism suites use.
- [Z2-N-ARCH-793] GPU test executables and render tests are not headless runs: they create an RHI device directly on WARP (D3D12) or lavapipe (Vulkan, selected through the pinned ICD manifest), and those that need a swapchain first create a hidden window through `Zenith_Window` (`Zenith_WindowDesc::m_bHidden = true`, realised by the platform layer with `SDL_WINDOW_HIDDEN`), while every other GPU test creates no window; "boots headless on WARP and lavapipe" therefore means these test executables, never the headless configuration of bullet 785.

### The Stage 3 loopback proof

The proof is the integration suite `Zenith2/Tests/NetLoopback/` driving two processes of the game `Games/WorkloadCharacters` in its Stage 3 selection (the Animation, Gameplay, Net and Physics modules; AI joins at Stage 4).

| Process | Command line |
|---|---|
| server | `WorkloadCharacters --headless --assets-root <cooked WorkloadCharacters> --set content.startupScene=content:/Scenes/NetLoopback.scene.json --set net.proof.mode=true --set sim.maxCatchUpTicks=8 --timeout-seconds 120 --set net.role=server --set net.port=<p> --record <server.zrpl> --exit-after-ticks 600` |
| client | `WorkloadCharacters --headless --assets-root <cooked WorkloadCharacters> --set content.startupScene=content:/Scenes/NetLoopback.scene.json --set net.proof.mode=true --set sim.maxCatchUpTicks=8 --timeout-seconds 120 --set net.role=client --set net.connect=127.0.0.1:<p> --record <client.zrpl> --input-script <game>/Tests/NetLoopback.input.json --exit-after-ticks 600` |

1. The server spawns 64 characters with network IDs allocated by peer 0; the client possesses one of them.
2. Each tick the client sends, over the loopback UDP transport of `ZenithNet/Internal` (unreliable datagrams, sequence-numbered), one INPUT per tick carrying that tick's block, up to three previous blocks and the digests of the newest four complete snapshot ticks.
3. The server applies client commands at the tick they name (a command arriving after its tick has been simulated is applied at the next tick and counted in `net.late_commands`), simulates, and sends each tick a snapshot of every replicated field of every character, quantised (positions at 1 mm, rotations as smallest-three 10-bit quaternions; a proof fixture, superseded by the Stage 7 replication specification).
4. The client applies snapshots to its replicated entities (no prediction in Stage 3).
5. After 600 ticks both processes write their logs and exit.

- [Z2-N-ARCH-786] `Test_Net_LoopbackReplicatesTransformsAndCommands` passes when: the server's received command log equals the client's sent command log for every tick after reordering by tick and sequence; for every tick T for which the client holds a snapshot, every replicated transform equals the server's authoritative transform at T after the same quantisation; `net.late_commands` is 0 on an idle machine; and both processes exit with code 0.

<!-- informative -->
The transform criterion of [Z2-N-ARCH-786] is checked in-process for every tick the client holds completely: the server checks each capture against the quantised authoritative state, verifies every complete tick the client reports against its own capture and that none went unreported, and the client checks that each proxy holds the decoded values; the results are reported through the exit codes. The command criterion is the verifier's log comparison. The test is a CTest entry of the test registry running `Zenith2/Tests/NetLoopback/Run-NetLoopback.ps1` (`Zenith2/Docs/Design/NET/Loopback.md` → "The loopback suite").
<!-- /informative -->
- [Z2-N-ARCH-787] The loopback transport, snapshot format and possession exist to prove the foundations and are marked Stage 3 foundations in the NET design document; transport sessions, prediction, reconciliation, interest management and dedicated server builds are Stage 7 work specified there (Z2-D-073).

## Memory governor

### Model

<!-- elaboration -->
`Zenith_MemoryGovernor` (ZenithCore, `Engine/ZenithCore/Zenith_MemoryGovernor.h`) is the cross-system memory governor: it holds per-subsystem budgets from `Zenith2/Docs/Budgets.md`, derives a pressure level from measured usage and OS signals, tells registered consumers to shed, budgets transition peaks explicitly, and grants subsystem-scoped reservations for single operations that are not transitions (§ "Subsystem reservations"). It is owned by the CORE area.

```cpp
enum Zenith_MemorySubsystem : uint8
{
	MEMORY_SUBSYSTEM_RENDERING,      // GPU heaps, render targets, the transient pool, particle pools, CPU render data
	MEMORY_SUBSYSTEM_STREAMING,      // resident streamed cells, textures and meshes in flight
	MEMORY_SUBSYSTEM_AUDIO,          // banks, decoded buffers, streams
	MEMORY_SUBSYSTEM_SCRIPTS,        // Luau heaps (all VMs), snapshots, checkpoints
	MEMORY_SUBSYSTEM_EDITOR_CACHES,  // thumbnails, previews, derived-data caches in memory (editor only)
	MEMORY_SUBSYSTEM_ASSETS,         // CPU-side decoded asset data outside streaming, fonts and glyph atlases, cinematic data
	MEMORY_SUBSYSTEM_SIMULATION,     // ECS storage, scene data, physics, navigation, animation, save buffers
	MEMORY_SUBSYSTEM_OTHER,          // the general heap's unattributed residual, frame and graph arenas, profiler buffers, job-system storage
	MEMORY_SUBSYSTEM_COUNT,
};

enum Zenith_MemoryPressureLevel : uint8
{
	MEMORY_PRESSURE_NORMAL,
	MEMORY_PRESSURE_WARNING,
	MEMORY_PRESSURE_CRITICAL,
};

enum Zenith_TransitionPeakKind : uint8
{
	TRANSITION_PEAK_STREAMING_DOUBLE_RESIDENCY,   // old and new versions of streamed data resident together
	TRANSITION_PEAK_WORLD_TRANSITION,             // two worlds resident during a transition
	TRANSITION_PEAK_TEXTURE_SWAP,                 // reallocate-and-swap of a streamed texture
	TRANSITION_PEAK_COUNT,
};

enum Zenith_MemoryErrorCode : uint8
{
	MEMORY_ERROR_PEAK_REFUSED = 1,   // the reservation would exceed its transition-peak row
	MEMORY_ERROR_BAD_BUDGETS,        // a budget or threshold is zero or inconsistent at Create
	MEMORY_ERROR_OUT_OF_MEMORY,      // Create could not allocate the governor
	MEMORY_ERROR_SUBSYSTEM_REFUSED,  // the reservation would exceed its subsystem's budget (§ "Subsystem reservations")
};

struct Zenith_MemoryError
{
	Zenith_MemoryErrorCode m_eCode;
	Zenith_TransitionPeakKind m_ePeak;   // for MEMORY_ERROR_PEAK_REFUSED
	Zenith_MemorySubsystem m_eSubsystem; // for MEMORY_ERROR_SUBSYSTEM_REFUSED
	uint64 m_ulRequestedBytes;
	uint64 m_ulAvailableBytes;
};

// Contract: implemented by subsystems; registered by the composition root or a module start.
// Reentrancy: NONE. Notifications arrive on the main thread in the frame prologue.
class Zenith_MemoryConsumer
{
public:
	virtual ~Zenith_MemoryConsumer() = default;
	virtual Zenith_MemorySubsystem GetSubsystem() const = 0;
	virtual uint64 GetResidentBytes() const = 0;     // ANY_THREAD, relaxed reads
	virtual uint64 GetDeviceLocalBytes() const { return 0; }   // ANY_THREAD; the device-local share
	virtual void OnPressureLevelChanged(Zenith_MemoryPressureLevel eOld, Zenith_MemoryPressureLevel eNew) = 0;
};

struct Zenith_MemoryThresholds
{
	uint32 m_uWarningPercent;                 // memory.threshold.warningPercent: the row "Memory pressure warning threshold"
	uint32 m_uCriticalPercent;                // memory.threshold.criticalPercent: the row "Memory pressure critical threshold"
	uint32 m_uSubsystemCriticalPercent;       // memory.subsystemCriticalPercent
	uint32 m_uHysteresisPercent;              // memory.hysteresisPercent
	uint32 m_uHysteresisMs;                   // memory.hysteresisMs
	uint32 m_uAndroidLowMemoryHoldMs;         // memory.androidLowMemoryHoldMs
};

struct Zenith_MemoryBudgets
{
	uint64 m_aulSubsystemBytes[MEMORY_SUBSYSTEM_COUNT];
	uint64 m_ulProcessBytes;                  // S24: process RSS; Windows: RAM
	uint64 m_ulDeviceLocalBytes;              // Windows VRAM; 0 on unified-memory devices
	uint64 m_aulTransitionPeakBytes[TRANSITION_PEAK_COUNT];
	Zenith_MemoryThresholds m_xThresholds;
	uint32 m_uPendingSubsystemMask;           // bit per Zenith_MemorySubsystem whose budget includes a pending row
};

// Contract: move-only; releases its bytes to the governor in its destructor. Thread affinity: ANY_THREAD.
class Zenith_TransitionReservation
{
public:
	Zenith_TransitionReservation(Zenith_TransitionReservation&& xOther);
	Zenith_TransitionReservation& operator=(Zenith_TransitionReservation&& xOther);
	Zenith_TransitionReservation(const Zenith_TransitionReservation&) = delete;
	~Zenith_TransitionReservation();
	uint64 GetBytes() const;
	void Shrink(uint64 ulNewBytes);                                    // release part early
private:
	Zenith_MemoryGovernor* m_pxGovernor = nullptr;
	Zenith_TransitionPeakKind m_eKind = TRANSITION_PEAK_COUNT;
	uint64 m_ulBytes = 0;
};

// Contract: move-only; releases its bytes to the governor in its destructor. Thread affinity: ANY_THREAD.
class Zenith_SubsystemReservation
{
public:
	Zenith_SubsystemReservation(Zenith_SubsystemReservation&& xOther);
	Zenith_SubsystemReservation& operator=(Zenith_SubsystemReservation&& xOther);
	Zenith_SubsystemReservation(const Zenith_SubsystemReservation&) = delete;
	~Zenith_SubsystemReservation();
	uint64 GetBytes() const;
	void Shrink(uint64 ulNewBytes);                                    // release part early
private:
	Zenith_MemoryGovernor* m_pxGovernor = nullptr;
	Zenith_MemorySubsystem m_eSubsystem = MEMORY_SUBSYSTEM_COUNT;
	uint64 m_ulBytes = 0;
};

// Contract: MAIN_THREAD except ReserveTransitionPeak, ReserveSubsystemBytes, ReportOSPressure,
// GetReservedPeakBytes and GetReservedSubsystemBytes (ANY_THREAD).
class Zenith_MemoryGovernor
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_MemoryGovernor>, Zenith_MemoryError> Create(
		const Zenith_MemoryBudgets& xBudgets, Zenith_Heap& xHeap,
		Zenith_FaultInjector* pxFaultInjector = nullptr);   // tests inject failure into the non-allocation steps
	void RegisterConsumer(Zenith_MemoryConsumer& xConsumer);
	void UnregisterConsumer(Zenith_MemoryConsumer& xConsumer);
	void ReportOSPressure(Zenith_MemoryPressureLevel eLevel);           // from the platform layer
	void ReportProcessUsage(uint64 ulProcessBytes, uint64 ulDeviceLocalBytes);
	Zenith_Result<Zenith_TransitionReservation, Zenith_MemoryError> ReserveTransitionPeak(
		Zenith_TransitionPeakKind eKind, uint64 ulBytes);
	Zenith_Result<Zenith_SubsystemReservation, Zenith_MemoryError> ReserveSubsystemBytes(
		Zenith_MemorySubsystem eSubsystem, uint64 ulBytes);          // § "Subsystem reservations"
	void Update();                                                      // frame prologue
	void ForceLevel(Zenith_MemoryPressureLevel eLevel);                // development builds and tests
	void ClearForcedLevel();                                            // undoes ForceLevel
	void BindOOMPressureFlag(Zenith_OOMPressureFlag& xFlag);           // once, before the first Update: the OOM relay
	Zenith_MemoryPressureLevel GetLevel() const;
	Zenith_MemoryGovernorStats GetStats() const;         // the memory panel's data (bullet 818)
	uint64 GetReservedPeakBytes(Zenith_TransitionPeakKind eKind) const;   // ANY_THREAD
	uint64 GetReservedSubsystemBytes(Zenith_MemorySubsystem eSubsystem) const;   // ANY_THREAD
};

// The composition root's two helpers (bullets 813 and 819).
Zenith_Result<void, Zenith_MemoryError> Zenith_ReadMemoryBudgets(const Zenith_ConfigStore& xConfig,
	Zenith_MemoryBudgets& xOut);                                        // every field from the keys of the table "Budgets"
uint64 Zenith_QueryProcessResidentBytes();             // the OS measurement of bullet 813
```

### Budgets

The composition root fills `Zenith_MemoryBudgets` from configuration keys whose engine-default values (layer 1 of Architecture/PlatformLayer.md → "Configuration layering") come from a table generated at configure time from the rows below (Zenith2/Docs/Budgets.md → "Row index"; the generator is specified by `Zenith2/Docs/Design/CORE.md`), so no budget number is written in engine code or in a platform configuration file; the budget keys follow `memory.budget.<name>Bytes` (for example `memory.budget.renderingBytes`, `memory.budget.processBytes`) and `memory.peak.<name>Bytes` (for example `memory.peak.worldTransitionBytes`); the threshold and hysteresis keys are named individually in the table.

| Budget | Budgets row(s) | Status and producer |
|---|---|---|
| process total, S24 | Z2-B-CORE-001 "S24 process RSS" (at most 3.0 GB) | adopted (Zenith2/Docs/Budgets.md → "Memory budgets") |
| process total, Windows | Z2-B-CORE-002 "Windows process RAM" (8 GB), Z2-B-CORE-003 "Windows VRAM" (6 GB) | adopted |
| `MEMORY_SUBSYSTEM_RENDERING` | Z2-B-RHI-001 "Persistent render targets and view histories", Z2-B-RHI-002 "Bindless descriptor heaps", Z2-B-RHI-006 "Upload ring hard cap", Z2-B-RHI-010 "Transient memory peak", Z2-B-RHI-011 "GPU readback pool size", Z2-B-GEOM-001 "Unified vertex and index pools", Z2-B-GEOM-002 "Texture residency pool", Z2-B-RPROD-001 "GPU scene tables", Z2-B-RPROD-002 "PSO cache and pipeline objects", Z2-B-ADVGFX-001 "Virtual texturing physical cache and page tables", Z2-B-VFX-001 "Particle pools and CPU particle data"; Z2-B-RHI-003 "GPU residency budget per heap" is a per-heap cap the RHI enforces and is not summed | pending: the Stage 1 per-system budget tickets for RHI, GEOM, RPROD, ADVGFX and VFX |
| `MEMORY_SUBSYSTEM_STREAMING` | Z2-B-STREAM-001 "Streaming CPU-side resident and in-flight cell data" | pending: STREAM |
| `MEMORY_SUBSYSTEM_AUDIO` | Z2-B-AUDIO-001 "Audio banks, streams and mixer" | pending: AUDIO |
| `MEMORY_SUBSYSTEM_SCRIPTS` | Z2-B-SCRIPT-009 "World main VM memory cap", Z2-B-SCRIPT-011 "VM memory times workers per world", Z2-B-SCRIPT-013 "Snapshot memory per tick"; Z2-B-SCRIPT-010 "Per-worker chunk-system VM memory cap" and Z2-B-SCRIPT-015 "Checkpoint reserved memory per behaviour instance" are covered by Z2-B-SCRIPT-011 and the checkpoint arenas, are enforced by the script runtime and reported through `GetResidentBytes`, and are not summed | pending: SCRIPT |
| `MEMORY_SUBSYSTEM_EDITOR_CACHES` | Z2-B-EDIT-001 "Editor caches" (editor builds; 0 in a Player) | pending: EDIT |
| `MEMORY_SUBSYSTEM_ASSETS` | Z2-B-ASSET-001 "Asset system resident data and decode buffers", Z2-B-UI-001 "UI, fonts and glyph atlases", Z2-B-CINE-001 "Cinematic timelines and video decode buffers" | pending: ASSET, UI, CINE |
| `MEMORY_SUBSYSTEM_SIMULATION` | Z2-B-ECS-001 "Component storage per world", Z2-B-SCENE-001 "Scene and prefab runtime data", Z2-B-PHYS-001 "Physics world memory", Z2-B-AI-001 "Navigation mesh and pathfinding memory", Z2-B-ANIM-001 "Animation clips, skeletons and pose buffers", Z2-B-SAVE-001 "Save serialization buffers" | pending: ECS, SCENE, PHYS, AI, ANIM, SAVE |
| `MEMORY_SUBSYSTEM_OTHER` | Z2-B-CORE-004 "General heap unattributed residual", Z2-B-CORE-005 "Frame and graph arenas", Z2-B-DEVT-001 "Profiler and telemetry buffers", Z2-B-JOBS-004 "Job system resident memory" | pending: CORE, DEVT, JOBS |
| transition peaks | Z2-B-CORE-006 "Streaming double-residency peak", Z2-B-CORE-007 "World-transition peak", Z2-B-CORE-008 "Texture-swap peak" | pending: CORE |
| pressure thresholds (keys `memory.threshold.warningPercent`, `memory.threshold.criticalPercent`) | Z2-B-CORE-009 "Memory pressure warning threshold", Z2-B-CORE-010 "Memory pressure critical threshold" (% of platform total) | pending: the Stage 1 per-system budget ticket for CORE |
| subsystem critical factor, hysteresis margin and dwell, Android low-memory hold | configuration keys `memory.subsystemCriticalPercent`, `memory.hysteresisPercent`, `memory.hysteresisMs`, `memory.androidLowMemoryHoldMs` | values set by the same Stage 1 per-system budget ticket for CORE, together with Z2-B-CORE-009/010 |

- [Z2-N-ARCH-819] The composition root fills every field of `Zenith_MemoryBudgets` from the configuration keys named in the table "Budgets", and `Zenith_MemoryGovernor::Create` fails with `MEMORY_ERROR_BAD_BUDGETS` (startup exit code 2, naming the key) when any budget or threshold is zero, when the warning threshold is not below the critical threshold, or when the per-subsystem budgets outside `m_uPendingSubsystemMask` exceed the process total.
- [Z2-N-ARCH-823] The table "Budgets" is the single source of the mapping from subsystem to Budgets rows: each subsystem's budget is the sum, for the build platform, of exactly the rows its row lists (host and device-local values alike), a row it names as not summed contributes to no budget, and every other document that maps a subsystem to rows cites this table; which bound a row counts in is Zenith2/Docs/Budgets.md → "Purpose versioning and change rule" (table "Allocation sums").
- [Z2-N-ARCH-824] A subsystem whose rows include a pending row takes the platform's process total as its budget (Z2-B-CORE-003 for its device-local share on Windows) and its bit in `m_uPendingSubsystemMask`, never sheds because of its own budget, and is reported as pending on the profiler's memory panel until the row is allocated.
- [Z2-N-ARCH-820] No threshold, hysteresis value or budget in the governor is a compiled-in number: each comes from a Budgets row or from a configuration key whose value the Stage 1 per-system budget ticket for CORE sets.

### Pressure levels

`Update` computes the level each frame as the maximum of:

| Source | WARNING when | CRITICAL when |
|---|---|---|
| process usage against the process budget | ≥ the row "Memory pressure warning threshold" | ≥ the row "Memory pressure critical threshold" |
| any subsystem against its budget | ≥ its budget | ≥ its budget × `memory.subsystemCriticalPercent` / 100 |
| Windows: `QueryMemoryResourceNotification` on the low-memory handle; the RHI's device-local usage query | low-memory signalled, or device-local usage ≥ the warning threshold row applied to the VRAM total (elaboration: the same two rows serve RAM and VRAM) | device-local usage ≥ the critical threshold row applied to the VRAM total |
| Android: `SDL_EVENT_LOW_MEMORY` (the platform's low-memory callback) | — | signalled, held for `memory.androidLowMemoryHoldMs` |
| `ForceLevel` (until `ClearForcedLevel`) | forced | forced |
| the OOM relay bound by `BindOOMPressureFlag` (raised by the heap's OOM handler) | — | raised since the last `Update` |

The level drops once every source has stayed below the lower level's threshold minus `memory.hysteresisPercent` percentage points for `memory.hysteresisMs` milliseconds (hysteresis).

- [Z2-N-ARCH-811] The governor derives the pressure level from the table "Pressure levels" and the hysteresis rule, applies level changes in the frame prologue, and notifies every registered consumer of every change in registration order on the main thread.
- [Z2-N-ARCH-812] Shedding changes caches, residency and presentation quality only, never simulation-visible state: no entity, component value, script-visible value or replayed quantity depends on the pressure level, so memory pressure cannot break the determinism guarantee.
- [Z2-N-ARCH-813] Process usage is measured every frame from the OS (`GetProcessMemoryInfo` working set on Windows, `/proc/self/statm` resident pages on Android) and from each consumer's `GetResidentBytes`.
- [Z2-N-ARCH-818] The measured process usage and every consumer's resident bytes are published each frame to the on-screen profiler's memory panel.

### Shedding per subsystem

<!-- elaboration -->
This table is the single specification of what each subsystem sheds; Zenith2/Docs/Rendering.md → "GPU lifetime and synchronisation" cites it and adds renderer-internal mechanics (which heaps, which passes) and no different response.

| Subsystem | At WARNING | At CRITICAL |
|---|---|---|
| Rendering | the texture-streaming pool target drops by 25%; cached physical transients unused for 2 frames are released (graph-cache retirement age 2 frames); transient heaps shrink at the next graph compile instead of after 300 frames; upload-ring chunks above the initial capacity are retired as soon as they are free; the streamed-texture mip bias rises by 1; capture and texture-view update intervals double; admitted GPU particle capacity, sorted particles and CPU particle capacity × 0.75, and particle pool regions stop growing; the virtual-texture request mip bias rises by 1 (the VT physical cache and page-table pool are fixed allocations that pressure never resizes) | in addition: every unused cached physical resource is released and group heaps are reallocated at the next graph compile; streamed textures outside the current visible set drop to their minimum resident mips (the mip bias rises by 2 as the mechanism); the quality overrides disable SSR, depth of field and motion blur and drop volumetric-fog resolution one step; the dynamic-resolution scale goes to its floor; capture and preview view histories are dropped; no new persistent render target is created until the level falls; admitted GPU particle capacity, sorted particles and CPU particle capacity × 0.5, pool ranges of emitters culled for more than 120 frames are freed, and the detached-effect cap halves; the virtual-texture request mip bias rises by 2 and the VT read and upload limits halve |
| Streaming | halve the prefetch radius; cancel `IO_PRIORITY_BACKGROUND` requests | cancel every prefetch; release presentation-held cell residency (prefetched cell data, HLOD meshes, boosters) outside the required band; no simulation cell is unloaded by memory pressure; admit only `IO_PRIORITY_CRITICAL` requests |
| Audio | reduce stream prebuffers to their minimum; purge the decoded-sample cache to 50% | stop virtual voices; release AUDIO-held bank references (retention cache, always-loaded banks other than the master bank) with no playing voice; component-held banks stay |
| Scripts | run GC steps at 4× the per-world step budget | run a full collection of every VM at the next frame boundary (a VM memory-cap breach stays fatal, Zenith2/Docs/Scripting.md → "Budgets safepoints and the watchdog") |
| Cinematics | presentation only: every video player's decode-ahead trims to 2 frames | stop the video players of sections not visible this frame; never a cinematic sequence, a preload pin or any state simulation reads (memory pressure is not a recorded input, Z2-D-019) |
| Editor caches | purge thumbnail and preview caches to 50% (least recently used first) | purge them entirely; drop in-memory derived-data caches |
| Assets | release zero-reference assets pending deferred deletion immediately | also drop decoded CPU copies of GPU-resident assets |
| Simulation | none (simulation memory is budgeted, not shed) | none; a simulation subsystem at CRITICAL raises the diagnostic `memory.simulation_over_budget` |
| Other | none | none |

- [Z2-N-ARCH-814] Every consumer implements the shedding its row of the table "Shedding per subsystem" specifies for each level, and each consumer's design document lists the counters that prove its shedding ran.
- [Z2-N-ARCH-821] A consumer applies its response within one frame of the governor's level change and reverts it one frame after the level falls below that level.
- [Z2-N-ARCH-822] The table "Shedding per subsystem" is the single source of shedding behaviour: another document that describes a subsystem's response to memory pressure cites this table and adds implementation detail only.

### Transition peaks

- [Z2-N-ARCH-815] Streaming double residency, world transitions and texture swaps reserve their peak from the governor before they allocate; a refused reservation (`MEMORY_ERROR_PEAK_REFUSED`) defers the operation to a following frame (presentation prefetch of streaming, texture swap) or fails the request (world transition, Architecture/LifecycleCommands.md → "World transition"), and never allocates past the peak row; a simulation-required cell load never defers or waits: it reserves without deferral and holds the reservation for the whole load (until the cell holds content or the load ends), and on a refusal the consumer releases at once the presentation residency that doubles it (the displayed HLOD proxy), so no deferrable allocation exceeds the peak row.
- [Z2-N-ARCH-816] A reservation is released by its `Zenith_TransitionReservation` owner's destructor, and the governor reports every reservation still held at shutdown.

### Subsystem reservations

- [Z2-N-ARCH-825] An operation that is not a transition and must not allocate past its subsystem's budget (a SCENE preparation whose decoded size exceeds `scene.prepareReserveThresholdBytes`, `Zenith2/Docs/Design/SCENE/Loading.md` → "Preparation operation") reserves its bytes with `ReserveSubsystemBytes(eSubsystem, ulBytes)` before it allocates. The governor grants the reservation when the subsystem's resident bytes at the last `Update` (the sum of its consumers' `GetResidentBytes`), plus the subsystem's outstanding reservations, plus `ulBytes`, stay within the subsystem's budget of the table "Budgets" (the process total for a subsystem in `m_uPendingSubsystemMask`, [Z2-N-ARCH-824]); otherwise it refuses with `MEMORY_ERROR_SUBSYSTEM_REFUSED` (with `m_eSubsystem`, the requested and the available bytes) and the operation fails or defers as its own design states (SCENE: `SCENE_ERROR_MEMORY_REFUSED`), never allocating past the budget. A world transition keeps the transition-peak reservation of [Z2-N-ARCH-815] and [Z2-N-ARCH-559] and takes no subsystem reservation for the same bytes.
- [Z2-N-ARCH-826] A `Zenith_SubsystemReservation` is released by its owner's destructor (for SCENE, with the preparation's arena) or earlier through `Shrink`; outstanding subsystem reservations count toward their subsystem's source and the process source of the table "Pressure levels" exactly as transition reservations count toward the process source, the double count while the reserved bytes are also reported by a consumer being conservative by design; the governor reports every subsystem reservation still held at shutdown, as [Z2-N-ARCH-816] does for transition reservations; and `Test_MemoryGovernor_SubsystemReservationRefusedAtBudget` (reservations up to the budget succeed, the next is refused with the available bytes, `Shrink` and destruction release) exists in `Engine/ZenithCore/Tests/`.

### Forced-level tests

- [Z2-N-ARCH-817] The tests `Test_MemoryGovernor_ForcedWarningShedsEachConsumer`, `Test_MemoryGovernor_ForcedCriticalShedsEachConsumer` (each subsystem's consumer, through its counters), `Test_MemoryGovernor_HysteresisReturnsToNormal`, `Test_MemoryGovernor_TransitionPeakRefusedAtCap`, `Test_MemoryGovernor_ShedsNoSimulationState` (tick hashes identical with and without forced CRITICAL), `Test_MemoryGovernor_OSSignalMapsToLevel` and `Test_MemoryGovernor_BadBudgetsRejected` exist in `Engine/ZenithCore/Tests/` or, for cross-system cases, `Tests/Memory/`, and assert what their names state; the S24 memory-pressure run is a `needs-human` acceptance device step.

## Composition root and Player

The full specification is `Zenith2/Docs/Architecture/CompositionRoot.md`. The contract (Z2-D-081, Z2-D-046, Z2-D-024) in summary:

| Topic | Contract | Normative IDs |
|---|---|---|
| wiring | `ZenithRuntime` constructs every subsystem in the startup order with injected dependencies and destroys them in reverse | 841–842 |
| generated composition | a constant table and backend thunks per game and configure preset, types in ZenithRuntime; canonical module order; factory symbols from `FACTORY`; interface-version and module-dependency checks; a per-preset build identity beside the composition hash | 843–845, 872–874 |
| startup | root heap and profiler hooks first; registration before `Freeze`; module registrations only through contexts; headless skips device steps; the log router between steps 1 and 2; type and configuration-key declarations before the configuration is built; a string-free error with the detail in the startup report; the Player's router and OOM flag passed to the runtime; the budget-run checks | 846–851, 858–861 (Android entry export, command-line options, binding ABI), 871 (startup report), 875 (log drain thread), 876 (router and flag), 877–878 (budget run), 879 (account consumer) |
| exit codes and frame loop | a fixed exit-code table (6: device lost); the main thread runs the loop; no render subgraph while the device is not `NORMAL` | 852–853, 863 (hot-reload caller and `CommitPendingReloads`), 880 |
| Android | one native library from the CMake output; pinned Gradle, JDK and SDK; cooked content in `assets/` | 854–856, 862 |
| tests | the composition test table | 857 |

## Determinism guarantee and mechanisms

### The guarantee

For the same backend and the same build, identical content and inputs give identical simulation results, regardless of worker count, chunk layout or scheduling, including ticks where scripts fail or exhaust their budgets; cross-backend and cross-platform equality is not promised (Z2-D-019).

- [Z2-N-ARCH-881] Every simulation result — every component value, entity set, event sequence, command sequence, behaviour checkpoint and the simulation-ID counter after every tick — is bit-identical across runs with the same build, ECS backend, physics backend, content and replay log, for any worker count, chunk capacity, compaction schedule and thread interleaving.
- [Z2-N-ARCH-882] Nothing promises equality across ECS backends, physics backends, platforms or builds, and no test or acceptance criterion compares simulation results across them.

### Mechanisms

| Mechanism | What it removes | Specified in |
|---|---|---|
| the entity as the transaction boundary; per-row restore; per-entity side-effect sub-buffers | failure outcomes that depend on chunk layout | Zenith2/Docs/Scripting.md → "Transaction scope and the entity boundary"; Architecture/ECSModel.md → "Script lifetimes on the ECS side" (Z2-D-051) |
| merge order (system order, logical invocation ID, sequence) for commands, spawn IDs and events | completion-order dependence | Architecture/ECSModel.md → "Command buffers"; Zenith2/Docs/Scripting.md → "Determinism rules and randomness" |
| stable simulation IDs assigned at playback in merge order | slot and storage-index dependence | Architecture/ECSModel.md → "Entity identity" |
| fixed-order reduction trees; contributions instead of shared in-place writes | worker-count dependence of sums and shared structures | Architecture/ECSModel.md → "Deterministic reductions" |
| cross-entity read rule (own row for same-phase written columns) | observation of in-flight writes | Architecture/ECSModel.md → "Script lifetimes on the ECS side" |
| counter-based script randomness keyed by (world seed, source, tick, simulation ID, draw counter) | shared RNG races and replay drift | Zenith2/Docs/Scripting.md → "Determinism rules and randomness" |
| safepoint-count script budgets; wall clock as a fatal watchdog alone | timing-dependent rollbacks | Zenith2/Docs/Scripting.md → "Budgets safepoints and the watchdog" (Z2-D-052) |
| merge-time shared-capacity acceptance in merge order | first-come capacity races | Zenith2/Docs/Scripting.md → "Merge-time capacity and asynchronous results" |
| asynchronous results published at tick boundaries in completion-ID order, logged | wall-clock arrival dependence | Zenith2/Docs/Scripting.md → "Merge-time capacity and asynchronous results" (Z2-D-053); § "Multiplayer foundations" |
| fixed phases, per-field authority, deterministic arbitration | ownership decided by compile order | Architecture/FramePhases.md → "Per-field authority", "Arbitration rules" |
| sync-point playback and compaction in merge order only | layout changes mid-phase | Architecture/FramePhases.md → "Sync points and the tick boundary" |
| fixed ticks; simulation reads tick time, never wall-clock time | frame-rate dependence | Architecture/CPUFrameGraph.md → "Execution" |
| replayable input commands and the replay log | input timing dependence | § "Multiplayer foundations" |
| schedule priorities affect wall-clock order only | cost-estimate dependence | Architecture/ZenithGraph.md → "Ordering, priorities and dependency counts" |
| no RTTI, no floating-point contraction; identical per-entity instruction sequences | SIMD-lane versus scalar-tail differences | Architecture/BuildSystem.md → "Compile and link flags" |
| CORE's fixed-algorithm transcendental functions instead of the C runtime's; the CRT's FMA3 dispatch switched off in every Windows `main` | CPU-dependent CRT transcendental results on one build (the x64 Universal CRT selects FMA3 implementations at run time) | bullets 888 and 889; Zenith2/Docs/Design/CORE/Maths.md → "Deterministic transcendental functions" (ruling X314-FMA3) |
| per-backend physics determinism across worker counts | physics scheduling dependence | § "Engine modules and game projects" (Z2-D-003) |
| memory shedding and thermal adaptation touch presentation only | device-state dependence | § "Memory governor"; Architecture/PlatformLayer.md → "Thermal adaptation" |

- [Z2-N-ARCH-883] Vectorised simulation kernels produce, for each entity, exactly the result the scalar path produces for that entity (the same operations in the same order per lane), so an entity's result never depends on its position in a chunk or batch.
- [Z2-N-ARCH-884] Simulation code never orders, hashes or keys by a memory address, pointer value, slot index, chunk index or worker index; hashed containers used by simulation iterate in an order defined by their keys, never by their bucket layout.
- [Z2-N-ARCH-885] Simulation code reads no clock, environment variable, file timestamp, thread identity or OS random source; time comes from the tick index and randomness from the counter-based streams (Zenith2/Docs/Contracts.md → "Tests and code hygiene gates", rule F14).
- [Z2-N-ARCH-886] The determinism suites (`Tests/Determinism/`, the ECS conformance determinism tests and the script fault matrix) replay the same logs with 1, 2, 8 and all workers, with forced compaction and two chunk capacities, on each backend, and compare per-tick world-state hashes.
- [Z2-N-ARCH-887] A determinism-suite hash difference fails the suite and reports the first differing tick, entity and property.
- [Z2-N-ARCH-888] Simulation-affecting code, and every computation whose output is tracked in source control or baked (cook steps, authoring tools, generators and bakes), never calls a C runtime transcendental function (`<cmath>` `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `exp`, `exp2`, `log`, `log2`, `log10`, `pow`, `cbrt`); it calls CORE's `Zenith_Sin`, `Zenith_Cos`, `Zenith_SinCos`, `Zenith_Tan`, `Zenith_Asin`, `Zenith_Acos`, `Zenith_Atan`, `Zenith_Atan2`, `Zenith_Exp`, `Zenith_Exp2`, `Zenith_Ln`, `Zenith_Log2`, `Zenith_Log10`, `Zenith_Pow`, `Zenith_Cbrt` or `Zenith_Sqrt` (`ZenithCore/Zenith_Transcendental.h`, binary32 and binary64), whose result bits are a pure function of the argument bits on every CPU, compiler and platform; `std::sqrt` of a non-negative argument stays allowed, because IEEE 754 rounds it correctly everywhere, and so do `+ − × ÷` without contraction, `floor`, `ceil` and `trunc`, which are exact or correctly rounded and outside the rule; a cube root is `Zenith_Cbrt`, which CORE's set provides (orchestrator ruling X314-FMA3, human may override; Zenith2/Docs/Design/CORE/Maths.md → "Deterministic transcendental functions").
- [Z2-N-ARCH-889] Every `main` and `SDL_main` of Zenith2 (the Player's entries, every tool, test, benchmark and integration-suite entry) calls `Zenith_ApplyCrtMathMode()` as its first statement, before the root heap is installed and before any thread starts; on Windows x64 it calls `_set_FMA3_enable(0)`, so third-party code that calls a CRT transcendental takes the non-FMA path on every CPU, and elsewhere it does nothing; it is defence in depth, and bullet 888's guarantee never depends on it (Zenith2/Docs/Design/CORE/Maths.md → "The CRT maths mode").

## Platform layer

The full specification is `Zenith2/Docs/Architecture/PlatformLayer.md`. The contract (Z2-D-010) in summary:

| Topic | Contract | Normative IDs |
|---|---|---|
| SDL3 | confined to `ZenithPlatform/Internal/`; the event pump on the main thread in the prologue; engine threads from `Zenith_Multithreading` | 911–913 |
| Android lifecycle | pause at a tick boundary, release the surface before the callback returns, recreate on foreground; a bounded 2,000 ms handshake | 914–916, 939 |
| process death | a resume state written on every background entry, restored on matching identities | 917–918, 934–935 |
| thermal | headroom drives dynamic resolution and quality tier; presentation only | 919 |
| virtual filesystem | mounts package < patch < loose < user; `content:/` and `user:/`; the user mount never shadows content; path validation against traversal | 920–924 |
| async I/O | priorities, bounded queues, cancellation, atomic writes, low-disk refusal surfaced to the game | 925–927, 940–942 |
| configuration | six layers (engine default, platform, game, quality tier, user, command line) with provenance; the value types, schema and store are ZenithCore's, the sources and the `Zenith_Config` facade this layer's | 928–930, 937 |
| account changes, headless host, tests | account events at tick boundaries; the Linux headless implementation; the test table | 931–933, 936, 938 |

## Security architecture

### Trust boundaries

| Boundary | Input | Trust | Controls |
|---|---|---|---|
| shipped content | cooked archives in the package mount | trusted source, untrusted bytes | versioned headers, parser limits, bounds-checked readers, cook-time validation |
| patches and downloaded content | patch archives, Play Asset Delivery packs | authenticated before mounting | signed update manifests (below), whose signature is checked at every start; Play signing for Play-delivered packs; each file verified once and re-hashed when its stamps change ([Z2-N-ARCH-966]) |
| user files | saves, user configuration, resume state | untrusted | parser limits, checksums, versioned migration, no executable content |
| scripts | Luau bytecode from the package or patch mounts; source in development builds | trusted source, sandboxed execution | the sandbox, budgets, memory caps and watchdog (Zenith2/Docs/Scripting.md → "Sandboxing and security"); bytecode only from authenticated mounts |
| remote tools | the DAP debugger, the remote profiler, the editor live link | development builds only | loopback binding by default, a per-session token, absent from Shipping |
| network | loopback transport (Stage 3), sessions (Stage 7) | untrusted | parser limits on every message, sequence and size checks; CRC-32C trailer; 64-packet replay window; 8 malformed packets in 1 s disconnect the peer |
| online services | responses from the adapter's backends | untrusted | parser limits; `Zenith2/Docs/Design/ONLINE.md` → "Security assumptions" |
| tool inputs | DCC sources, authoring documents, the shared DDC, the clipboard, child-process output, command-line host paths | the person's choice, not the bytes | importer limits and crash isolation, document validation, command-line grants; never reaches a Player |
| process IPC | the Windows crash helper | inherited handles only | [Z2-N-ARCH-965] |

<!-- informative -->
The boundary records (inputs, trust, controls, tiers and residual risks of each boundary) are `Zenith2/Docs/Design/SEC/TrustBoundaries.md`.
<!-- /informative -->

- [Z2-N-ARCH-951] Every parser of external bytes (cooked formats, JSON authoring documents, saves, configuration, replay logs, schema blobs, network messages, online responses, image and audio decoders) treats its input as untrusted: it validates every header, size, count and offset against the input length with overflow-checked arithmetic before use, and returns a `Zenith_Result` error, never an assert or undefined behaviour, on invalid input (Zenith2/Docs/Contracts.md → "Errors and results").
- [Z2-N-ARCH-952] Every such parser has a libFuzzer target under its library's `Tests/Fuzz/` run as a smoke in the `win-asan` preset and for its tier's duration in stage acceptance (`Zenith2/Docs/Design/SEC/Fuzzing.md` → "Stage acceptance campaigns"; the full duration is the Stage 6 tier duration).

### Parser limits

| Parser | Limits (a violation is a recoverable error) |
|---|---|
| JSON (authoring documents, configuration, `game.json`, schemas) | file 64 MiB (schemas 1 MiB), nesting depth 64, string 1 MiB, array 1,048,576 elements, object 65,536 keys, numbers within `f64` range |
| cooked archive (`.zpak`) | header magic and version checked; table-of-contents entries at most 1,048,576; every entry's offset + size within the file; compressed size at most 4 GiB; decompressed size declared and enforced |
| textures | width and height at most 16,384, mips at most 15, array layers at most 2,048, declared byte size equal to the computed size |
| meshes | vertices at most 16,777,216 per mesh, indices 16-bit per sub-mesh (Zenith2/Docs/Rendering.md → "Vertex data format"), sub-meshes at most 65,536 |
| audio | channels at most 8, sample rate at most 192 kHz, declared length checked against the payload |
| saves | file 256 MiB, every section length-prefixed and checksummed (CRC-32C), version migration before use; at most 1,024 sections and 16 worlds; one decoded section at most 256 MiB and the decoded total at most 1 GiB, checked before any decode; per world section at most 16,777,216 entities and 256 components per entity; events per world at most `ecs.maxEventsPerTick`; at most 4,096 resources per world section; at most 65,536 asynchronous entries per list; the journal exactly 176 bytes; the last-profile record exactly 28 bytes; the listing prefix 128 KiB (`Zenith2/Docs/Design/SAVE/Formats.md` → "Format limits") |
| replay logs | tick blocks length-prefixed; at most 4,096 commands and 4,096 publications per tick; one record at most 256 MiB (`uREPLAY_MAX_RECORD_BYTES`); one checkpoint image at most min(256 MiB − 14, half the writer's commit queue), a larger one skipped by the writer; the whole log at most `replay.maxLogBytes` (512 MiB by default), refused by the reader before allocating and stopping the writer before the limit |
| cooked UI document (`.zui`) | element records 32,768; string pool 4 MiB, strings 65,536; style blocks 65,536, properties per block 128; binding records 256; TYPEDATA 4 MiB; EVENTS entries 5 × 32,768; every index checked against its section before use (`Zenith2/Docs/Design/UI/Documents.md` → "Parser limits") |
| cooked font (`.zfont`) and its `ZMSD` page payload | faces 8; pages 64 per file; glyph records 65,535 per face; coverage ranges 65,536 per face; program 64 MiB per face; page size 512, 1024 or 2048; the page payload's header rules |
| cooked localization table (`.zloc`) | messages 262,144; pool 64 MiB; message bytes 65,536; plural programs 256 bytes each; nesting 8 |
| UI binding manifest (`UI/BindingManifest.zbin`) | types 4,096; properties 65,536 in total |
| UI publication payload | 448 bytes; names 64 bytes; text 256 bytes |
| streaming index (`.zstream`) | grids 8; cells 65,536; dependencies 32 per cell; references 1,048,576 per world and 4,096 per cell per direction; persistent entries 1,048,576 per world and 1,024 per cell; HLOD nodes 65,536, 3 levels, 4 meshes per node; entities per cell 16,384, and at cook at most the configured `ecs.maxSpawnsPerTick` (Z2-B-ECS-003) when that is lower (`Zenith2/Docs/Design/STREAM/Formats.md` → "Limits") |
| streaming cell delta | 65,535 bytes (the ECS container heap's element cap); 1,024 entity records; at most 32 component types per entity record, present plus absent (1 ≤ M + A ≤ 32) |
| animation assets | skeletons: 1,024 joints, 512 curves, 256 sockets, 64 chains, 32 IK definitions, names 63 bytes; clips: 432,001 frames, 27,001 segments, 4,096 events and 4,096 markers, event names 127 bytes, duration 3,600 s; graphs: 4,096 nodes, 256 parameters, 256 state machines, 256 states and 2,048 transitions per machine, 8 conditions per transition, 8 layers, 8 slots, 64 masks, 32 sync groups, 1,024 clips, 64 blend samples, 65,536 bytes of instance memory, a pose stack of 16; intermediates: 65,536 channels per clip, 16,777,216 keys per channel; import: 65,536 nodes and 64 skeletons per source; documents 64 MiB (`Zenith2/Docs/Design/ANIM/Formats.md` → "Parser limits") |
| schema blobs | the schema limits of Architecture/DataDefinedComponents.md → "The schema file" |
| network messages (Stage 3) | datagram 24 … 1,200 bytes, body 1,176; commands per datagram 64; INPUT blocks 4 and digest pairs 4; snapshot fragment body 1,020 bytes; fragments per tick 32; destroys per fragment 64 and per capture 256; replicated entities per capture 512; replication table 32 entries; proxy table 16; ULEB128 10 bytes; publication payload 1,024 bytes; datagrams received per frame 512; NET save section exactly 24 bytes (`Zenith2/Docs/Design/NET/Loopback.md` → "Wire limits") |
| network messages (Stage 7) | message 65,536 bytes; 32 messages per datagram; table 1,024 entries; property 1,024 bytes; string and asset tables 4,096 entries; credential 1,024 bytes; publication pieces of 1,020 bytes, 65 per message (`Zenith2/Docs/Design/NET/Stage7.md` → "Serialization limits") |
| cooked asset file | sections at most 4,096; file at most 16 GiB; header plus section table at most 163,904 bytes; a section's stored and decoded sizes at most 4 GiB each (decoded through the compression codec set) |
| asset catalogue | entries at most 1,048,576; file entries at most 1,048,576; dependencies at most 16,777,216; file at most 512 MiB; fragments at most 256; hard dependencies per asset at most 4,096 |
| `.meta` sidecar | file at most 1 MiB; the JSON limits above; duplicate keys rejected |
| DDC entry | payloads at most 256; step ID at most 64 bytes; entry at most 4 GiB |
| asset database file | records at most 4,194,304; edges at most 67,108,864; file at most 1 GiB |
| asset load result payload | exactly 24 bytes |
| 2D atlas document | sources 4,096; frames 65,535; pages 64; page extent 4,096; grid count 4,096 per source |
| 2D animation document | parameters 16; states 64; transitions 256; conditions 4 per transition; clips 256; frames 1,024 per clip; events 8 per frame and 256 per skeletal clip; keys 4,096 per track; tracks 384 bone tracks + 768 slot tracks per clip |
| 2D tileset document | tiles 65,535; polygon vertices 8; animation frames 64 per tile; terrains 64; rules 256 per terrain |
| 2D tilemap document | layers 16; chunks 16,384 per map (every layer); chunk coordinate magnitude 2^15; cells base64 exactly 5,464 characters |
| 2D rig document | bones 128; slots 256; skins 64; attachments 1,024 per skin |
| cooked TWOD files | the counts of the five 2D documents; strings 1,048,576 bytes; names 255 bytes each |
| TWOD save section | overlay chunks 65,536; body records 65,536; shape records 262,144; joint records 65,536; map records 65,536; layers 16 per map record; chain index entries 262,144; chains 262,144 in total; unit edges 8,192 per chain; pair records 262,144; one-way disabled pairs 262,144; pending operations 65,536; collision dirty keys 65,536; posed entities 65,536; bones 128 and slots 256 per posed entity; backend blob 256 MiB |
| collision shapes (`.zcol`) | the cooked asset file limits; convex vertices ≤ 256, faces ≤ 512, indices ≤ 4,096; mesh triangles ≤ 2,097,152; heightfield samples per side 2 … 4,097 with samplesX = samplesZ; heightfield palette ≤ 127; other palettes ≤ 256; compound children ≤ 65,535; buoyancy samples ≤ 32; backend payload ≤ 512 MiB |
| physics materials (`.zpmat`) | interactions ≤ 64; string lengths 1 … 63 |
| ragdolls (`.zragdoll`) | parts ≤ 64; convex vertices per part ≤ 256 |
| physics cook intermediates (`ZPHI`, `ZPCI`) | samples 257 per side; `ZPHI` palette ≤ 127, `ZPCI` palette ≤ 256; compound records ≤ 65,535 |
| physics save section | the save limits; body, constraint and pair counts ≤ their configuration capacities; slot-table capacities equal to the world's and high-water marks ≤ them |
| physics layer configuration strings | ≤ 32 names of 1 … 31 bytes; ≤ 1,024 pair tokens; ≤ 8 KiB |
| navigation files (`.znav`) | the cooked asset file limits; INFO 64 bytes; classes ≤ 6; areas = 64; profiles ≤ 8; tile vertices ≤ 16,384, polygons ≤ 4,096, detail vertices ≤ 127 and detail triangles ≤ 255 per polygon; layers W, H ≤ 2,048, spans ≤ 16,777,216, area volumes ≤ 256; manifest entries ≤ 1,048,576; volume nodes ≤ 4,194,304, levels ≤ 10 |
| navigation source intermediate (`ZNSG`) | vertices ≤ 263,169, triangles ≤ 524,288 |
| navigation tile bake intermediate (`ZNTJ`) | scenes ≤ 4,096, sources ≤ 4,096, water volumes ≤ 256 |
| navigation and navigation-settings documents | the JSON limits above; `scenes` and `terrains` ≤ 4,096 entries; `volumes`, `carveRegions`, `waterVolumes` ≤ 256 |
| AI save section | the save limits; the counts and index rules of `Zenith2/Docs/Design/AI/Formats.md` → "Save section layout" (search states at most `ai.path.maxActiveRequests` × `ai.path.maxSearchNodes` × 52 bytes plus heads) |
| script path requests and results | exactly 64 and 416 bytes |
| package manifest (`package.zpkm`) | units 4,096, archives 65,536, strings 1 MiB; at most 256 units without the PRESENT flag (check M12); file at most 5,374,016 bytes (64 + 32 × 4,096 + 64 × 65,536 + 1,048,576) (`Zenith2/Docs/Design/PACK/Formats.md` → "Package manifest") |
| update and DLC manifests (`update.zman`, `dlc.zman`) | files 65,536, strings 1 MiB; file 225 … 96 + 64 × 65,536 + 1,048,576 + 64 bytes (`Zenith2/Docs/Design/PACK/Formats.md` → "Update manifest") |
| PACK install and delivery state | install journal: moves 0 … 1,024, strings 64 KiB; install state file exactly 48 bytes; install record exactly 192 bytes; delivery state file: packs 256, stamps 25,600 of 48 bytes, strings 16 KiB; verified stamp file (`verified.zinv`): directories 1,024, files 131,072, at most 8,437,792 bytes (`Zenith2/Docs/Design/PACK/Formats.md` → "Install journal", "Install state file", "Install record", "Delivery state file", "Verified stamp file") |
| `Package.*` event payload | exactly 96 bytes |
| CHAR documents (tool-side inputs, never read by a Player) | every document 64 MiB; assembly 16 regions, 16 slots, 64 parts, 32 loadouts, 64 parts per loadout, 64 root components; lip-sync 4,096 lines, 16 locales, 256 overrides per entry, 65,536 keys per override, 1,024 phrases per entry, 600 s of audio per entry; visemes file 65,536 cues; pose book 255 poses, 1,024 joints and 512 curves per pose; control set 256 controls, 32 groups; project 16 voice locales, 16 classes; pose snapshot 1,024 joints, 512 curves; scenario 4,096 steps, 256 assertions, 600 s (`Zenith2/Docs/Design/CHAR.md` → "Character document limits") |
| WORLD documents (tool-side inputs, never read by a Player) | world document 64 MiB; 32 layers; 65,536 regions; 64 generators; 16 terrains; `layerGridOrder` entries at most 8 − the base grid count; 16,777,216 entities of a world's resolved content; 256 light pages per unit; payloads 268,435,456 bytes decompressed; 65,536 generated entities per (generator, region) (`Zenith2/Docs/Design/WORLD/Documents.md` → "Document limits") |
| content source importers (tool-side, out of process) | glTF: file and each external buffer 2 GiB, JSON chunk 64 MiB, decoded `data:` URI 256 MiB, URI 4,096 bytes, nodes 65,536 at depth 64, meshes 16,384, primitives per mesh 255, vertices 16,777,216 per primitive and per combined mesh, accessors and buffer views 1,048,576, buffers, materials, textures, images and samplers 4,096, skins 1,024 with 1,024 joints, morph targets 256 per mesh, animations 4,096 with 65,536 channels and 16,777,216 keys per channel, cameras and lights 4,096, instances per node 65,536, scenes 256; image sources: 16,384 per axis, decoded 2 GiB, file 1 GiB; audio sources: channels 1 … 8, sample rate 8 … 192 kHz, decoded 2 GiB, file 2 GiB, 64 markers kept, 4,096 WAV/AIFF chunks, 1,024 FLAC metadata blocks, Ogg Vorbis decoder working memory 64 MiB (`Zenith2/Docs/Design/CONTENT/Importers.md` → "Parser limits") |
| online catalogue document | the JSON limits above; 1,024 achievements; 256 entitlements; IDs 64 bytes; native IDs 128 bytes (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Format limits") |
| cooked online catalogue | section body ≤ 24 + 24 × 1,024 + 16 × 256 + 1,048,576 bytes (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Format limits") |
| online payloads | script request payload 80 bytes; result and notification payloads 256 bytes; backend completion payload: CLOUD_LIST ≤ 1,024 objects (`uONLINE_MAX_CLOUD_LIST_OBJECTS`), CLOUD_READ ≤ `online.cloud.maxTransferBytes` (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Format limits") |
| online cloud object name | 69 bytes (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Format limits") |
| online cloud envelope | 320 + 268,435,456 bytes (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Format limits") |
| online cloud index | 2,112 + 256 × 128 = 34,880 bytes (a 2,112-byte header with a 128-pair commit log, 64 entries and 64 deletion records); commit log ≤ 128 pairs (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Format limits") |
| online sync manifest | 16 + 104 × 64 = 6,672 bytes (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Format limits") |
| offline achievement file | 16 + 72 × 1,024 bytes (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Format limits") |
| offline cloud object | 64 + `ulONLINE_MAX_CLOUD_OBJECT_BYTES` bytes (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Format limits") |
| online device record | 64 bytes (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Format limits") |
| virtual texture (`.zvt`) | extent per axis 256 … 16,384 (powers of two); tiles 21,845; tile index 873,800 bytes; the other limits of `Zenith2/Docs/Design/ADVGFX/VTCook.md` → "Parser limits" |
| path-traced reference (`.zref`) | extents 16 … 1,024; decoded planes at most 6,291,456 bytes (`Zenith2/Docs/Design/ADVGFX/PathTracer.md` → "Reference files") |
| UTF-8 validation of every text field read from outside | the limits of `Zenith2/Docs/Design/CORE/Text.md` → "JSON reader" |
| persistent-ID text and binary forms in saves and documents | the limits of `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_PersistentID functions" |
| the cooked bytecode container header and its load-time verification fields | the limits of `Zenith2/Docs/Scripting.md` → "Shipping bytecode compatibility" |
| `Zenith_ScriptModuleManifest::Load` | the limits of `Zenith2/Docs/Scripting.md` → "Shipping bytecode compatibility" |
| the behaviour checkpoint decoder (checkpoints in saves and replays) | the limits of `Zenith2/Docs/Scripting.md` → "Sandboxing and security" ([Z2-N-SCRIPT-485]) |
| the Luau source compiler under the source limits | the limits of `Zenith2/Docs/Scripting.md` → "Sandboxing and security" ([Z2-N-SCRIPT-487]) |
| the DAP transport framing and request decoder | the limits of `Zenith2/Docs/Design/SCRIPT/Debugger.md` → "Transport and framing" |
| scene and prefab documents and their validator | the limits of `Zenith2/Docs/Design/SCENE/Format.md` → "Document limits" |
| the cooked-scene decoder | the limits of `Zenith2/Docs/Design/SCENE/Format.md` → "Document validation" |
| the `.zmat` reader | the limits of `Zenith2/Docs/Design/GEOM/Materials.md` → "Material reader validation" |
| stream-set codes and canonical names | the limits of `Zenith2/Docs/Design/GEOM/VertexFormat.md` → "Layout validation and hashing" |
| the `.zenv` static environment reader | the limits of `Zenith2/Docs/Design/LIGHT.md` → "Data structures and formats" |
| the `.zlut` grading LUT reader | the limits of `Zenith2/Docs/Design/LIGHT.md` → "Data structures and formats" |
| the `.zgiv` irradiance-volume reader | the limits of `Zenith2/Docs/Design/LIGHT/GI.md` → "Irradiance volume cooked format" |
| the `.zlmp` lightmap-set reader | the limits of `Zenith2/Docs/Design/LIGHT/GI.md` → "Lightmap set cooked format" |
| the `.zprb` reflection-probe reader | the limits of `Zenith2/Docs/Design/LIGHT/Reflections.md` → "Reflection probe cooked format" |
| the `.cube` LUT source reader of `Cook_GradingLUT` | the limits of `Zenith2/Docs/Design/LIGHT.md` → "Data structures and formats" |
| the RGBE (`.hdr`) source reader of `Cook_Environment` | the limits of `Zenith2/Docs/Design/LIGHT.md` → "Data structures and formats" |
| the Zenith2 header of a persistent pipeline-cache file (`ZPCV`, `ZPCD`) and its payload hash | the limits of `Zenith2/Docs/Rendering/ShaderSystem.md` → "Persistent pipeline caches" |
| the `.zbank` parser | the limits of `Zenith2/Docs/Design/AUDIO/Formats.md` → "Bank file checks" |
| trigger command payloads (recorded commands) | the limits of `Zenith2/Docs/Design/AUDIO/Formats.md` → "Sound file checks" |
| UI, theme, font family, localization project and string table documents | the limits of `Zenith2/Docs/Design/UI/Documents.md` → "Parser limits" |
| `Zenith_CompileMessage` over message sources | the limits of `Zenith2/Docs/Design/UI/Documents.md` → "Parser limits" |
| `Zenith_CompilePluralRule` over CLDR rule strings | the limits of `Zenith2/Docs/Design/UI/Documents.md` → "Parser limits" |
| `Zenith_ReadXliffImport` | the limits of `Zenith2/Docs/Design/UI/Documents.md` → "Parser limits" |
| terrain and tile documents through `Load` | the limits of `Zenith2/Docs/Design/ENV/Bake.md` → "Reader validation" |
| foliage, placement, atmosphere, weather, water, terrain layer and cloud-noise documents | the limits of `Zenith2/Docs/Design/ENV/Bake.md` → "Reader validation" |
| cooked terrain tile files | the limits of `Zenith2/Docs/Design/ENV/Bake.md` → "Reader validation" |
| cooked foliage cell files | the limits of `Zenith2/Docs/Design/ENV/Bake.md` → "Reader validation" |
| the three cooked environment parameter files | the limits of `Zenith2/Docs/Design/ENV/Bake.md` → "Reader validation" |
| the cooked cloud-noise file | the limits of `Zenith2/Docs/Design/ENV/Bake.md` → "Reader validation" |
| the cooked effect reader | the limits of `Zenith2/Docs/Design/VFX/EffectFormat.md` → "Format limits" |
| the effect document validator | the limits of `Zenith2/Docs/Design/VFX/EffectFormat.md` → "Document limits" |
| the cinematic sequence loader | the limits of `Zenith2/Docs/Design/CINE/Formats.md` → "Load validation" |
| the cooked video container loader | the limits of `Zenith2/Docs/Design/CINE/Formats.md` → "Video load validation" |
| the `.zprof` capture reader | the limits of `Zenith2/Docs/Design/DEVT/Profiler.md` → "Profiler public headers" |
| the console tokeniser and parser | the limits of `Zenith2/Docs/Design/DEVT/Console.md` → "Console grammar" |
| the budget-run `rows.json` and `fingerprint.json` readers | the limits of `Zenith2/Docs/Design/DEVT/Evidence.md` → "Fingerprint invalidation tests" |
| the remote protocol frame decoder | the limits of `Zenith2/Docs/Design/DEVT/Console.md` → "Remote protocol" |
| the next-launch crash report finalization parser | the limits of `Zenith2/Docs/Design/DEVT/Crash.md` → "Crash report schema" |
| three-way merge inputs | the limits of `Zenith2/Docs/Design/CONTENT/Merge.md` → "Merge determinism and limits" |
| batch journals | the limits of `Zenith2/Docs/Design/CONTENT/Importers.md` → "Parser limits" |
| the content validation report reader | the limits of `Zenith2/Docs/Design/CONTENT/Importers.md` → "Parser limits" |

- [Z2-N-ARCH-953] The limits of the table "Parser limits" are constants in each parser's library, tested at the limit and at limit + 1 by that parser's tests, and changed only with the table in the same diff.

### Path traversal

- [Z2-N-ARCH-954] Every runtime file access goes through a validated `Zenith_VirtualPath` confined to its mount root after link resolution (Architecture/PlatformLayer.md → "Virtual filesystem"), except the named exceptions of [Z2-N-ARCH-923]: the command-line grants and ZenithPackage's install file system and directory sources, each confined to its own root; no path taken from content, saves, configuration, scripts or the network is ever passed to an OS file API unvalidated.

### Authenticated update manifests

<!-- elaboration -->
A patch for an installed Windows build, or a downloaded content update, ships an update manifest `update.zman`: format version, release line, target build ID, the SHA-256 and size of every file it adds or replaces, and an Ed25519 signature over all preceding bytes. The public verification key is compiled into the Player as `constexpr` data; the private key is kept out of the repository. The signature primitive's implementation is a pinned, audited library admitted to the manifest by the Stage 1 security-architecture ticket.

- [Z2-N-ARCH-955] The patch mount accepts an archive only after its update manifest's signature verifies against the compiled-in public key, the manifest's release line and target build match the running build, and every listed file's SHA-256 and size match, verified once, re-hashed when its stamps change: a file whose identity stamp (size, last-write time, change time and file identity) equals the stamp the applier recorded in `.zenith2-install/verified.zinv` when it verified the file at commit is not re-hashed, and a file whose stamp differs is re-hashed in full; the signature, release line and target build are checked at every start (human ruling Q90 (a), 2026-10-16; the same rule covers Windows DLC).
- [Z2-N-ARCH-961] Any update-verification failure rejects the whole update, leaves the previous content mounted, and logs the reason.
- [Z2-N-ARCH-956] Play-delivered bundles and asset packs rely on Play's signing and are verified against the cooked-content hash recorded in the base module before mounting, verified once, re-hashed when their stamps change (a pack whose recorded verification, Play pack version, manifest hash, location and per-archive stamps all still match is not re-hashed; human ruling Q90 (a), 2026-10-16); a mismatch leaves the pack unmounted and reports it to the game.
- [Z2-N-ARCH-966] The stamp record is a cache inside the trust boundary "patches and downloaded content", not a new boundary, and the stamp skip trusts `.zenith2-install/verified.zinv` (and, for packs, the delivery state file) exactly as much as the files it covers: the records are unauthenticated user state, so anyone able to write the install root (Windows) or the app-private storage (Android) can forge a record matching a rewritten file's new stamp together with the genuine signed manifest's digest, and that party can already replace the executable, which the boundary does not defend against. The update manifest's Ed25519 signature stays checked at every start but does not bind a file's bytes once its record is trusted; the cooked files' own header checks remain, and `package.reverifyOnStartup` turns every skip off for a run. The Stage 1 security-architecture ticket confirms (a) the stamp set, (b) that the records are unauthenticated and protected only by the install root's write permission, and (c) the escalation, a keyed record (a MAC under a secret kept outside the install root), if a distribution layout ever makes `.zenith2-install/` more writable than the executable (`Zenith2/Docs/Design/PACK/Algorithms.md` → "Verified cache", `Zenith2/Docs/Design/PACK/Formats.md` → "Verified stamp file").
- [Z2-N-ARCH-957] Script bytecode is loaded only from the package and patch mounts in Shipping builds, because Luau bytecode is not safe to load from untrusted sources.
- [Z2-N-ARCH-962] In development builds the loose mount supplies bytecode that the tools (ZenithCook, ZenithEditor) compiled through `ZenithScriptCompiler`; no Player compiles script source in any configuration.

### No debugger or remote tools in shipping

<!-- elaboration -->
- [Z2-N-ARCH-958] Shipping builds contain no DAP debugger server, no remote-profiler listener, no editor live link, no development console, no loose mount and no cheat command.
- [Z2-N-ARCH-963] The development services live in development-only translation units whose entry symbols are listed in `Zenith2/Tools/Gates/shipping_forbidden_symbols.json`.
- [Z2-N-ARCH-964] The `Libgraph` step fails when any symbol listed in `shipping_forbidden_symbols.json` appears in a Shipping game executable; the symbol check is complemented by the forbidden-string scan and the listener-import scan, each with a positive control (`Zenith2/Docs/Design/SEC/ShippingExposure.md` → "Audit layers").
- [Z2-N-ARCH-959] In development builds every remote-tool server binds to `127.0.0.1` by default, requires the per-session token printed in the local log to connect, and can be bound to another interface only with the explicit configuration key `devtools.bindAddress` set on the command line; the received token is compared in constant time, five failed handshakes within 10 s stop the listener accepting connections for 60 s (`devtools.listener.lockouts`), and a non-loopback bind logs a warning at every start (`Zenith2/Docs/Design/SEC/TrustBoundaries.md` → "Remote-tool session rules", rules T3, T4 and T6). The one exception is the Tracy client inside ZenithProfiling: it listens on the loopback interface only, starts only when `devtools.tracy.enabled` is set on the command line, carries no session token (Tracy's protocol has none), and is absent from Shipping ([Z2-N-ARCH-124]).
- [Z2-N-ARCH-965] The Windows crash helper (`--zenith-crash-helper`, Architecture/CompositionRoot.md bullet 860) trusts only inherited handles: the parent process, an unnamed section and three events, and no PID and no path argument. The parent-process handle carries exactly `PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE`, never `PROCESS_DUP_HANDLE`; the section and the events are created non-inheritable and only inheritable duplicates exist, for the one `CreateProcessW` call (`bInheritHandles = TRUE` with a `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` of exactly those five), and are closed as soon as it returns; any other Zenith2 code that starts a child with `bInheritHandles = TRUE` passes its own handle list. The helper validates the section (magic, version, a 128-bit nonce, and the parent PID against the inherited handle), derives the crash directory itself, bounds and escapes every section string, and exits with code 3 on any failed check (Zenith2/Docs/Design/DEVT/Crash.md → "Crash helper security contract").

### Security evidence

- [Z2-N-ARCH-960] Stage 6 acceptance includes the security hardening evidence: every fuzz target run for its full duration without a finding, a tampered-patch test (flipped byte, wrong key, wrong build, replayed older manifest) rejected in each case, and the Shipping symbol check clean for every game.

## Named selection decisions

Selection and classification decisions are Stage 1 or Stage 2 (or Stage 3) decisions that block their consumers. A decision that needs measurement is split into an ordinary harness ticket, which builds the measurement under `Zenith2/Benchmarks/`, and a `needs-human` ruling ticket, which runs it on the reference hardware, commits the report under `Zenith2/Docs/Research/` and appends the ruling to `Zenith2/Docs/DecisionLog.md` in its own diff (Z2-D-084, Z2-D-097).

| Selection | Candidates | Criteria | Acceptance rule | Stage | Consumers blocked |
|---|---|---|---|---|---|
| D-SEL-NAV | Recast/Detour vs custom (the bake stages and the runtime navmesh storage the queries read; the search loop and queries are ZenithAI code over either storage, ruling AI-R1; Zenith2/Docs/DecisionLog.md → "D-SEL-NAV") | tiled navmesh baking and streaming, dynamic obstacles, off-mesh links, agent sizes, non-ground navigation, async query cost, determinism per build | the candidate that passes the AI navigation conformance scenarios with query cost within the AI budget rows and the lower maintenance estimate; Recast/Detour wins ties | 1/2 | AI navigation, ENV collision and navigation generation, WORLD navigation baking |
| D-SEL-TEXT | HarfBuzz + FreeType + msdfgen vs alternatives; bidi through SheenBidi vs an ICU4X subset | complex-script shaping, font fallback, bidirectional text, SDF quality at UI scales, binary size on the S24, licences | passes the UI localization stress corpus (pseudo-localization, long strings, RTL, CJK, Arabic, Devanagari) with golden captures within FLIP thresholds and fits the package budget rows | 1/2 | UI text, localization, accessibility captions |
| D-SEL-UIRT | RmlUi vs a custom retained UI | retained layout, navigation and focus, accessibility semantics, visual authoring integration, render-graph integration, maintenance | passes the UI conformance scenarios and the Game2D menu walkthrough within the UI budget rows; the custom option is chosen only when RmlUi fails a scenario that cannot be fixed by integration | 1/2 | UI runtime, UI editor, Game2D, WorkloadUI2D |
| D-SEL-HEAP | the system allocator vs mimalloc for the general heap (sanitizer builds always use the system allocator) | allocation throughput and fragmentation on the workloads, RSS on the S24, sanitizer compatibility, maintenance | mimalloc is adopted only when its measured median improvement on the W-Dense and W-OpenWorld allocation traces exceeds the justification record's threshold with 95% confidence; otherwise the system allocator | 1 | the `Zenith_Heap` backing (Zenith2/Docs/Contracts.md → "Ownership and allocation") |
| D-SEL-PHYSX-ANDROID | PhysX 5 on arm64 Android, or Jolt as the Android backend | PhysX builds with NDK r27c and 16 KB pages, passes the physics conformance suite on the S24, fits the physics budget rows | PhysX is an Android backend only when all three hold; otherwise the capability matrix makes Jolt the Android backend and `game.json` rejects PhysX for Android | 1 | Android physics, `ZenithPhysics_PhysX` matrix, games' `backends.physics` |
| D-VTX-TANGENTLESS | an octahedral normal with a screen-space-derivative tangent frame vs QTangent | FLIP against the QTangent reference on the test asset set, derivative artefacts (including the visibility-buffer path), ALU cost against bandwidth saved on the S24 and the RTX 3070 | adopt per quality tier, per material class, or not at all; QTangent stays the default until the ruling | 3 | GEOM vertex streams, material shaders (Zenith2/Docs/Rendering.md → "Vertex data format") |
| D-SEL-JSON | yyjson vs simdjson | parse and write throughput on the authoring corpus, writer support, determinism of output, parser-limit enforcement, binary size | the candidate with a writer that produces byte-stable output and the higher median parse throughput within the security limits; a candidate without a writer is paired with a Zenith writer only if still faster | 1 | every JSON reader and writer (`Zenith_JsonWriter`), the cook, the editor |
| D-SEL-HASH | xxHash (XXH3-64 and XXH3-128) | speed, distribution quality, stability of the output across versions and platforms | adopted when its output is bit-identical across Windows and Android for the test vectors and its version is pinned; the hash of stable names, graph structures and script random keys all use it | 1 | type IDs, structure hashes, script randomness, hashed containers, evidence fingerprints |
| D-SEL-CODEC | Opus vs Vorbis for streamed audio | quality at target bitrates, decode cost on the S24, seeking, licence | the codec meeting the audio quality listening bar at the lower decode cost within the audio CPU rows | 2 | AUDIO streaming and banks, CINE dialogue |
| D-SEL-VIDEO | platform decoders (Media Foundation, MediaCodec) vs FFmpeg | licensing, codec coverage, hardware decode on the S24, integration with the render graph | the platform decoders unless a required cinematic format cannot be decoded by them on either platform; FFmpeg only with a licence review recorded in the decision | 2 | CINE video playback |
| D-SEL-CRASH | Crashpad vs a custom minidump writer | Windows and Android coverage, symbol-server integration, size, maintenance | Crashpad unless it cannot meet the 16 KB page and minSdk 34 constraints or the package budget | 2 | DEVT crash dumps and symbols, SHIP symbol retention |
| D-SEL-2DPHYS | Box2D v3 vs Jolt in 2D-constrained mode | 2D feature coverage (joints, one-way platforms, continuous collision), determinism per build, cost on WorkloadUI2D and Game2D | the neutral total rule of Zenith2/Docs/DecisionLog.md → "D-SEL-2DPHYS" (ruling Q91 (a)): the scenario suite minus `ExactSave` plus per-build determinism gate; then Z2-B-TWOD-004 and Z2-B-TWOD-005 at the Stage 1 committed scales (both meet → Jolt-2D; one meets → it; neither → the lower Game2D-scale p95 step time); neither passing the gates leaves the ruling Blocked and Stage 2 acceptance waiting | 2 | Zenith2D physics, Game2D |
| D-SEL-MOTIONMATCH | motion matching in scope, or blend trees only | CinematicSlice and OpenWorldSlice locomotion quality, authoring cost, memory and CPU on the S24 | in scope only when a harness shows it within the ANIM budget rows on the S24 and the flagship quality review requires it | 3 | ANIM locomotion, CHAR workflow |
| D-CLASS-SIM | cloth, destruction and soft bodies: each required, deferred or excluded, with an owner and a library candidate (Jolt soft bodies, custom) | flagship-slice needs, cost on the S24, available library support | each capability is classified with a recorded owner and rationale; a required capability names its implementation candidate and budget rows | 2 | PHYS extended capabilities, VFX and CINE set-pieces |
| D-SEL-UPSCALER | a third-party temporal upscaler behind `FLUX_UPSCALER_EXTERNAL`: AMD FidelityFX Super Resolution 2 or 3 (MIT; shader source compiled offline through the pinned toolchain; Vulkan and D3D12; mobile-capable), NVIDIA DLSS (proprietary SDK, desktop NVIDIA only), Intel XeSS (Windows), or none | licence compatible with shipping; both RHIs; no runtime shader compilation (Z2-D-025); FLIP against a 4× supersampled reference on the TAA temporal scenes at equal GPU time; S24 availability | adopted for a tier only when it beats `FLUX_UPSCALER_TAAU` on FLIP at equal or lower `Z2-B-LIGHT-010` GPU time on that tier's reference hardware over 10 repetitions; owned by LIGHT | 5 | the Stage 5 external-upscaler tickets (LIGHT temporal upscaling) |
| D-SEL-FBX | FBX import through a conversion path (FBX2glTF, Blender headless, or the Autodesk FBX SDK) vs glTF-only | round-trip fidelity across the supported-DCC matrix (skins, morphs, animation, materials, cameras, lights; axis, handedness and unit conversion), licence, out-of-process crash isolation, maintenance | the conversion path is adopted only when it reproduces the DCC matrix's reference assets within the CONTENT fidelity thresholds; otherwise glTF-only | 4 | CONTENT importers, ZenithCook FBX import |

- [Z2-N-ARCH-981] Every consumer of a selection in the table above is `blocks`-linked from that selection's ruling ticket and cannot be implemented before the ruling; its design references the selection by name with "(open, Stage N)" until the ruling lands.
- [Z2-N-ARCH-982] A selection ruling states the chosen candidate, the measurements against each criterion with their report path under `Zenith2/Docs/Research/`, and the acceptance rule's outcome, and it never silently changes a default: a changed default is itself the ruling's recorded consequence.
- [Z2-N-ARCH-983] Research measurements for selections run under controlled conditions (locked clocks, fixed-performance mode where available, at least 10 repetitions, medians with 95% confidence intervals, raw data committed), kept separate from shipping qualification.

<!-- informative -->
Flagged proposal P1 (not an accepted decision) suggests a further ruling, D-SEL-EnTT-SHIPPING, after the Stage 5 research report, on whether EnTT stays a shipping ECS backend or becomes research-only. Nothing in this document assumes that outcome: EnTT is a full backend (Z2-D-039) until the human approves or rejects the proposal.
<!-- /informative -->
