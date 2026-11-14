# DEVT design: developer tools and quality

This is the entry document of the DEVT design family (Developer tools & quality). It states what DEVT owns, the libraries and files its code lives in, the public API index, the contracts every DEVT type obeys, the threading, lifetime and error contract, the diagnostics, failure handling, test strategy, budget rows, stage plan and decisions, and the index from every `Z2-R-DEVT-NNN` requirement to the headings that specify it. The split files under `Zenith2/Docs/Design/DEVT/` hold the full specification of each subsystem; their headings are listed in § "Document family".

Conventions: names are Zenith Hungarian (Z2-D-034); no function carries `noexcept` or `[[nodiscard]]`, and every fallible one returns a `Zenith_Result<T, E>` (`Zenith2/Docs/Contracts.md` → "API boundary"); every public type carries one contract block (`Zenith2/Docs/Contracts.md` → "Contract blocks per type"); code blocks use tabs. A line `<!-- elaboration -->` marks a detail designed here beyond the programme plan; an `<!-- assumption: pending <AREA> -->` block states an interface another area's document does not yet provide, recorded in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` (rows `DV1` onward). Obligations in this family are traced to the catalogue `Zenith2/Docs/Requirements/DEVT.md`.

## Purpose and scope

### What DEVT owns

| Capability | Where it is specified |
|---|---|
| `ZenithProfiling`, the layer-1 engine library: the profiler-hook implementation (zones, zone values, counters, lock and blocking-wait events, per-site lock statistics), per-thread event rings, thread lanes, frame assembly and history, GPU timing ingestion, the capture format and writer, Tracy forwarding | `Zenith2/Docs/Design/DEVT/Profiler.md` |
| the on-screen profiler (Z2-D-005): its state machine and runtime toggle, the timeline, GPU, frame-history, memory, counter, Luau and startup views, the panel catalogue and its expression language, panel providers, the overlay input path, the in-editor overlay | `Zenith2/Docs/Design/DEVT/Overlay.md` |
| the development console `Zenith_DevConsole` (registration, grammar, built-in commands, configuration variables, output), the development file writer, the runtime development commands and the DEVT frame dump | `Zenith2/Docs/Design/DEVT/Console.md` |
| the debug-draw recorder `Zenith_DebugDraw`, its render-world records `Flux_RenderDebugDraw`, the `DebugOverlay` pass (`Flux_AddDebugOverlay`) and its programs, the debug font | `Zenith2/Docs/Design/DEVT/DebugDraw.md` |
| structured-log tooling: the JSON Lines file sink, the record schema, the log viewer | `Zenith2/Docs/Design/DEVT/Console.md` |
| remote diagnostics: the development server on PLAT's loopback listener, its protocol, remote console, log streaming, capture streaming and file fetch, the host client | `Zenith2/Docs/Design/DEVT/Console.md` |
| `Zenith_CrashHandler` (the X37 noun): crash capture on Windows and Android (Linux is a test host without capture), the crash helper and its security contract, the crash report format, annotations, attachments, breadcrumbs, fatal recognition, next-launch finalization, the script hang report, symbolization; the D-SEL-CRASH (open, Stage 2) selection | `Zenith2/Docs/Design/DEVT/Crash.md` |
| the evidence-fingerprint specification (Stage 1 deliverable 13, Z2-D-065), the workload runner `Run-Workload.ps1`, row source bindings, the budget-run record `Zenith_BudgetRunRecord` and the evidence record writer, run telemetry, regression dashboards | `Zenith2/Docs/Design/DEVT/Evidence.md` |
| the `linux-tsan` WSL2 host and its LLVM pin (Stage 1 deliverable 4), fuzzing conventions and the campaign runner, the clang-tidy configuration, content-validation reports, asset profiling, the `ZenithDevTool` host tool, the libgraph and complexity gates as DEVT requirements | `Zenith2/Docs/Design/DEVT/QualityTools.md` |
| every named DEVT test | `Zenith2/Docs/Design/DEVT/Tests.md` |

### What DEVT does not own

| Capability | Owner | Reference |
|---|---|---|
| the hook table `Zenith_ProfilerHooks`, its installation, `g_xProfilerHooks`, the lock-order checker and its Debug thread state | CORE (declared by Contracts) | `Zenith2/Docs/Contracts.md` → "No mutable global state" |
| the log router, `Zenith_LogSink`, the console and ring sinks, the failure procedure and `Zenith_TerminateProcess` | CORE | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log router", "Failure procedure" |
| every counter, zone and on-screen row's *data*: each area publishes its own counters and zones and lists its rows | the publishing area | the "Counters" and "On-screen profiler rows" headings of each design |
| the GPU timestamp queries, their calibration and `Flux_PassTimingPublisher` | RPROD, RHI | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Timestamp insertion and attribution" |
| GPU fault breadcrumbs and the device-loss report | RHI | `Zenith2/Docs/Design/RHI/Recovery.md` → "Breadcrumbs", "Device-loss report" |
| RenderDoc and PIX captures (`r.capture`) | RHI | `Zenith2/Docs/Design/RHI/Recovery.md` → "Captures" |
| the render and CPU frame dumps (`zenith2.graph-dump`, `zenith2.frame-dump`, the Flux graph dump) | GRAPH, RHI, RPROD | `Zenith2/Docs/Design/GRAPH.md` → "Graph dump schema zenith2.graph-dump version 1" |
| the loopback listener, the file watcher and the `Engine.Dev` input context | PLAT | `Zenith2/Docs/Design/PLAT/Platform.md` → "Development services" |
| the MP-MEM S24 sampler | PLAT | `Zenith2/Docs/Design/PLAT.md` → "The Stage 1 PLAT harness" |
| the gate step scripts (`Libgraph`, `Complexity`, `Sanitizers`, `Coverage`) and their activation | PROC (protected) | `Zenith2/Docs/Contracts.md` → "Enforceability matrix" |
| symbol retention and the licence inventory | SHIP | Z2-I-SHIP inventory items |
| the Luau DAP debugger and the script sampling profiler | SCRIPT | `Zenith2/Docs/Design/SCRIPT/Debugger.md` → "Sampling profiler implementation" |
| sanitizer instrumentation of the heap, arenas and pools | CORE | `Zenith2/Docs/Contracts/Memory.md` → "Sanitizer instrumentation" |
| the editor's own panels and ImGui | EDIT | `Zenith2/Docs/Architecture.md` → "Repository layout" |

Excluded, with decision: a crash-report upload server and any hosted service (no paid or hosted service; reports stay on the device's user mount and are fetched by the developer, Z2-D-022 governs any challenge); remote tools in Shipping builds (`Zenith2/Docs/Architecture.md` → "Security architecture"); a build-flag profiler (Z2-D-005).

### Inventory items covered

Every item of `Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1", section "DEVT Developer tools & quality", is covered: Z2-I-DEVT-01 to Z2-I-DEVT-26. The table "Coverage of inventory items" of `Zenith2/Docs/Requirements/DEVT.md` maps each item to its requirements. DEVT also owns the requirements of the `linux-tsan` host (Stage 1 deliverable 4), naming the CORE item Z2-I-CORE-06 it serves, and the evidence-fingerprint specification (Stage 1 deliverable 13).

### Document family

| File | Its headings |
|---|---|
| `Zenith2/Docs/Design/DEVT.md` (this file) | the skeleton below and its subsections |
| `Zenith2/Docs/Design/DEVT/Profiler.md` | Scope of this file · Profiler public headers · Zenith_ProfilerTypes header · Zenith_Profiler header · Zenith_ProfileZone header · Hook implementation · Thread slots and the thread-local variable · Event records · Per-thread event rings · Name tables · Counter registry · Thread lanes · Frame assembly · Frame history and detailed frames · Zone statistics · GPU timing ingestion · Lock and wait events · Lock statistics · Runtime toggle and modes · Tracy forwarding · Capture format · Capture writer and the flush thread · Memory layout and accounting · Creation and shutdown · Profiler cost model |
| `Zenith2/Docs/Design/DEVT/Overlay.md` | Scope of this file · Overlay public headers · Zenith_ProfilerOverlay header · Zenith_ProfilerPanels header · Overlay state machine · Overlay layout · Top bar · Timeline view · GPU view · Frame history view · Panels · Expression language · Built-in panel catalogue · Panel providers · Overlay input · Overlay drawing · Freeze and frame selection · Graph view · Editor overlay · S24 overlay |
| `Zenith2/Docs/Design/DEVT/Console.md` | Scope of this file · Zenith_DevConsole header · Zenith_DevFileWriter header · Console grammar · Command registry · Command execution · Built-in commands · Configuration variables · Console output and history · On-screen console · Runtime development commands · DEVT frame dump · Log file sink · Log record schema · Remote diagnostics server · Remote protocol · Remote messages · Remote client · Shipping absence of development services |
| `Zenith2/Docs/Design/DEVT/DebugDraw.md` | Scope of this file · Zenith_DebugDraw header · Recorder records · Shape expansion · Recorder rings and drain · Durations and world tags · Flux_DebugDraw header · Debug-draw extractor · DebugOverlay pass · DebugLines program · DebugText program · Debug font · Overlay upload budget · Debug-draw failure handling · Recorder slot alternative |
| `Zenith2/Docs/Design/DEVT/Crash.md` | Scope of this file · Zenith_CrashHandler header · Crash handler lifecycle · Crash handler anchor · Crash report files · Crash report schema · Annotations and breadcrumbs · Attachments · Crash log mirror · Fatal failures · Windows capture · Crash helper security contract · Android capture · Linux capture · Sanitizer builds · Hang reports · Thread registration · Next-launch finalization · Symbolization · D-SEL-CRASH candidates and acceptance · Crashpad realisation · Custom realisation |
| `Zenith2/Docs/Design/DEVT/Evidence.md` | Scope of this file · Evidence fingerprint specification · Fingerprint algorithm · Zenith2Evidence module · Fingerprint invalidation tests · Workload runner · Row source bindings · Budget-run inputs · Zenith_BudgetRunRecord header · Budget-run sampling · Statistics · Evidence record writer · Per-frame capture CSV · Measured interval and run length · Area row emitters · Run telemetry · Frame telemetry · Regression dashboards |
| `Zenith2/Docs/Design/DEVT/QualityTools.md` | Scope of this file · The linux-tsan host · WSL distro provisioning · Kernel setting · Toolchain and dependency mirror · The linux-tsan toolchain file · Host runner · TSan smoke suite · Host verification · Gates in CI · Fuzzing · Fuzz campaign runner · Static analysis · Content validation reports · Asset profiling · ZenithDevTool · Capture conversion |
| `Zenith2/Docs/Design/DEVT/Tests.md` | Scope of this file · Test targets · ZenithProfiling unit tests · Overlay tests · Console tests · Debug-draw tests · Crash tests · Evidence tests · Quality-tool tests · Integration suites · Render tests · ShippingTest cases · Death cases · Stress and sanitizer runs · Device and acceptance runs · Fixtures |

## Dependencies

### Libraries DEVT code lives in

| Library | Layer | Declared dependencies (owned by `Zenith2/Docs/Architecture.md` → "Library dependency graph") | DEVT content |
|---|---|---|---|
| `ZenithProfiling` | 1 | ZenithCore | the whole library (DEVT owns it, `Zenith2/Docs/Architecture.md` → "Repository layout") |
| `ZenithPlatform` | 2 | ZenithCore, ZenithProfiling | `Zenith_CrashHandler.h` and `Internal/CrashHandler/**` with their platform files (PLAT owns the library; DEVT owns these files, `Zenith2/Docs/Design/PLAT.md` table "What PLAT does not own": "the crash handler … DEVT") |
| `ZenithFlux` | 5 | ZenithCore, ZenithGraph, ZenithJobs, ZenithProfiling, ZenithRHI, ZenithVertexFormat, ZenithAssets, ZenithShadersGenerated | `Flux_DebugDraw.h`, `Internal/Debug/**` (the `DebugOverlay` pass, which `Zenith2/Docs/Design/RPROD/Passes.md` assigns to DEVT) |
| `ZenithRuntime` | 6 | as Architecture lists it | `Internal/DevTools/**`: the wiring of every DEVT object into the composition root, the debug-draw extractor, the remote server, the user-mount writer, the input adapter, the budget-run driver |
| `ZenithDevTool` (tool) | — | ZenithCore, ZenithProfiling, ZenithPlatform | the host command-line tool (`Zenith2/Tools/DevTool/`); assumption: pending ARCH (row DV1) |

ZenithProfiling depends on ZenithCore alone: it reads the monotonic clock (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Clocks"), uses the heap, arenas, containers (`Zenith_BoundedQueue` for every multi-producer queue, and CORE's `Zenith_SpscRing<T>` for every single-producer ring, ruling Q80, row DV23), `Zenith_Thread`, `Zenith_SemaphoreNoProfiling`, `Zenith_MutexNoProfiling` (the unprofiled variants alone: ZenithProfiling names no profiled primitive, Profiler.md "Lock and wait events"), `Zenith_Hash64`, `Zenith_StringView`, `Zenith_Format`, the maths types (`Zenith_Vector3`, `Zenith_Quaternion`, `Zenith_Matrix4`), the JSON writer and reader, the configuration schema and store types and the memory-consumer interface. It never includes a ZenithPlatform, ZenithJobs or ZenithFlux header: everything above layer 1 reaches it through types it declares (§ "Upward interfaces DEVT declares").

Third party: `Tracy` (owner `ZenithProfiling`, `cmake-subproject`, BSD-3-Clause, `Zenith2/Docs/Architecture.md` → "Third-party dependencies"). The manifest entry's platforms are `windows, android`; ZenithProfiling builds in `linux-tsan` too, so DEVT asks for `linux` to join the entry's platforms (row DV2) and configures Tracy there with every feature off (§ "ZenithProfiling platform files"). The crash-handler library that D-SEL-CRASH (open, Stage 2) may admit (Crashpad) is owned by `ZenithPlatform` (`Zenith2/Docs/Architecture.md` → "Third-party dependencies", table "Dependencies admitted by open selections").

### Headings DEVT relies on

| Need | Heading |
|---|---|
| the hook table, counter kinds, the thread-local exemption | `Zenith2/Docs/Contracts.md` → "No mutable global state" |
| the layering rules for upward interfaces | `Zenith2/Docs/Contracts.md` → "Layering DAG" |
| bounded queues, the lock table (row 940), shutdown rows 6, 7, 11 | `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract" |
| result and error forms | `Zenith2/Docs/Contracts.md` → "Errors and results" |
| tests, death suites, fuzz targets, forbidden constructs | `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates" |
| startup steps 1b, 2, 7, 9, 13, 15, 18 and the frame loop step 3 | `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence", "The frame loop" |
| zones per node and sub-job, the contention rule, the dump | `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Profiling" |
| presets, the `linux-tsan` rules, the manifest | `Zenith2/Docs/Architecture/BuildSystem.md` → "The linux-tsan host", "Presets", "Toolchain pins and the setup script" |
| the development-service absence and the loopback rule | `Zenith2/Docs/Architecture.md` → "Security architecture" |
| the render-world hand-off and its writer | `Zenith2/Docs/Rendering.md` → "Purpose and scope" |
| the `DebugOverlay` per-pass table | `Zenith2/Docs/Rendering/PerPass/Common.md` → "DebugOverlay" |
| GPU timestamps | `Zenith2/Docs/Rendering/RenderGraph.md` → "GPU timestamps" |
| evidence records, the procedure catalogue, workload definitions | `Zenith2/Docs/Budgets.md` → "Frame-rate definitions and measurement procedure", "Workload definitions" |
| the log router, sinks, the failure procedure, clocks | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log router", "Failure procedure", "Clocks" |
| rings and bounded queues | `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_RingBuffer", "Zenith_BoundedQueue"; the single-producer ring `Zenith_SpscRing<T>`, specified by AUDIO pending CORE: `Zenith2/Docs/Design/AUDIO.md` → "Assumed interfaces pending other areas" |
| maths and colour types | `Zenith2/Docs/Design/CORE/Maths.md` → "Vector types", "Zenith_Quaternion", "Zenith_Colour" |
| the failure procedure DEVT reads fatals from | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Failure procedure" |
| the manifest hashes the fingerprint reuses | `Zenith2/Docs/Design/SHIP/Manifest.md` → "Canonicalisation and hashing", "Evidence invalidation" |
| the configuration dump the workload runner reads | `Zenith2/Docs/Design/PLAT/Configuration.md` → "The configuration dump" |
| the one declaration of `Flux_AddDebugOverlay` | `Zenith2/Docs/Design/RPROD/Passes.md` → "Flux_Features header" |
| the timestamp publisher's caller | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Timestamp insertion and attribution" |
| threads | `Zenith2/Docs/Design/CORE/Threading.md` → "Threads" |
| JSON writer and formatting | `Zenith2/Docs/Design/CORE/Text.md` → "JSON writer", "Formatting" |
| the executor's timings and timeline | `Zenith2/Docs/Design/GRAPH.md` → "Zenith_FrameExecutor.h" |
| the JOBS zones, lanes and contention rows | `Zenith2/Docs/Design/JOBS.md` → "Zones", "On-screen profiler rows" |
| the loopback listener and Shipping exclusion | `Zenith2/Docs/Design/PLAT/Platform.md` → "Loopback socket", "Shipping exclusion" |
| the engine development actions | `Zenith2/Docs/Design/PLAT/Input.md` → "Engine actions and contexts" |
| user-mount atomic writes and the evidence directory grant | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Atomic writes", "Command-line host files" |
| the Player main | `Zenith2/Docs/Design/MOD.md` → "Player headers", "Frame loop driver" |

### Consumer needs and the DEVT answer

Every need another area's design or ticket records against DEVT, under the names and signatures that area already uses, and the DEVT heading that answers it. Where DEVT answers with a different shape, the row says why; the reconciliation rows are in the cross-area notes.

| Consumer (row) | Need as written | DEVT answer |
|---|---|---|
| JOBS (JB10), ECS (EB4), PLAT (PB17), RHI (RB8), ASSET (AB2), SCENE (ScB7), SCRIPT (SB13), ENV (EnB17) | the `ZenithProfiling` library target named in `DEPENDS`; only `class Zenith_Profiler;` (JOBS) | `ZenithProfiling` with its API-only headers (§ "ZenithProfiling target"; Profiler.md "Zenith_Profiler header"), Stage 1 sprint by pull-forward, fix version `Z2 Stage 2` (Z2-R-DEVT-001, Z2-R-DEVT-002) |
| JOBS (JB11, J10; Z2T-JOBS-028) | `Zenith_ProfilerLaneID RegisterThreadLane(uint32 uLaneIndex, const char* szName)`, once per lane at `Create` step 9, a failed registration logged once and otherwise ignored; the zone-begin hook copies `m_szName` at the call | exactly that declaration (the null lane on a full table); JOBS's flow needs no other call, because a registered lane binds the thread whose OS name equals it and index 0 binds the caller (Profiler.md "Thread lanes"); names are copied at the call (Profiler.md "Hook implementation") (Z2-R-DEVT-004, Z2-R-DEVT-008). The one JOBS change is the field type: `m_pxProfilerLane` ("opaque lane handle … or null", a pointer) becomes a `Zenith_ProfilerLaneID` value (row DV10); Z2T-JOBS-028 is otherwise built as written |
| JOBS (J22), JOBS TestCatalogue | `Test_Profiler_ContentionRowsFromCounters` asserting interval sums only; `Test_FrameGraph_ZonesForEveryNode` | both tests in the `Zenith2/Tests/DevTools/` integration suite (Tests.md "Integration suites") (Z2-R-DEVT-037, Z2-R-DEVT-115) |
| GRAPH (GR11, G16), ECS (EB5), SCENE (ScB7), RPROD (RPB8), SCRIPT (SB13) | the development console registration API: a named command with a handler and user data, a one-line help text, main thread, an output call; `Zenith_DevConsole` | `Zenith_DevConsole::RegisterCommand(const Zenith_DevCommandDesc&)` returning a move-only `Zenith_DevCommandToken`, the handler `Zenith_PfnDevCommand`, the reply object `Zenith_DevCommandOutput` (Console.md "Zenith_DevConsole header") (Z2-R-DEVT-047). The console is a concrete class with no virtual member, so a registrant's tests (Z2T-GRAPH-050's executor tests among them) construct the real console, which runs without a device, and use its test-facing members `Execute`, `HasCommand` and `GetCommandCount` instead of "a test console implementing DEVT's registration interface" (row DV25) |
| GRAPH (GR11), MOD (Z2T-MOD-061) | `jobs.dump <path>` writes through `Zenith_FrameExecutorDesc::m_pfnDumpWrite`, which MOD-061 implements over the user mount | adopted unchanged: `jobs.dump` is not on DEVT's file-writer path, and DEVT's integration test asserts GRAPH's callback receives the files (Tests.md "Integration suites"); no change to GRAPH or MOD |
| ECS (EB5; Z2T-ECS-069) | `ecs.dump <worldTag> <path>` | adopted unchanged: DEVT's runtime command file registers that exact form, writing to a user-mount `<path>` (Console.md "Runtime development commands"); no change to ECS |
| SCRIPT (SB13) | a development file grant for `script.profiler.dump` | `Zenith_DevFileWriter`, an interface ZenithProfiling declares and ZenithRuntime implements over `user:/devtools/` (Console.md "Zenith_DevFileWriter header") (Z2-R-DEVT-053) |
| SCENE (ScB7) | debug primitives for the hierarchy view; the debug-draw sink passed in by the composition root | `Zenith_DebugDraw` (DebugDraw.md "Zenith_DebugDraw header") (Z2-R-DEVT-057) |
| RPROD (RPB9, Renderer.md `m_axDebugDraws`), Rendering per-pass table | `Flux_AddDebugOverlay` and the element type of `m_axDebugDraws` | `Flux_RenderDebugDraw` in `Flux_DebugDraw.h` and the pass (DebugDraw.md "Flux_DebugDraw header", "DebugOverlay pass") (Z2-R-DEVT-060, Z2-R-DEVT-061) |
| LIGHT (LB22) | the debug overlay pass draws the `r.debug.lighting` views | a different shape: `Flux_AddDebugOverlay` draws lines, rectangles and glyphs only; LIGHT's views are passes LIGHT adds at RPROD's `FLUX_EXTENSION_POINT_OVERLAY`, which runs immediately before `DebugOverlay` and keeps `LightingDebug.slang` LIGHT's; DEVT draws their legend text (row DV7) |
| LIGHT (LB22, L41) | the Player budget-run mode passes every `render.quality.*` value as `--set` | the runner `Run-Workload.ps1` learns every registered `render.quality.*` key from a `--config-dump` pre-launch and passes each exactly once, the workload's value for the four mapped `graphics` fields and the tier's non-user value for the rest (Evidence.md "Workload runner") (Z2-R-DEVT-089) |
| GEOM (GB25; Z2T-GEOM-118) | `Flux_EmitGeomBudgetRows(Zenith_BudgetRunRecord&)` called at frame end | the driver-called emitter shape, registered by ZenithRuntime and called at frame-loop step 3 (Evidence.md "Area row emitters") (Z2-R-DEVT-094) |
| ENV (EnB17; Z2T-ENV-070) | `Zenith_EmitEnvironmentBudgetRows(Zenith_BudgetRunRecord&, const Zenith_EnvironmentFrameTimings&)` called by `Environment.ExtractEnvironment`; `Flux_EmitEnvironmentGpuRows(Zenith_BudgetRunRecord&, const Flux_PassTimings&)` called on `Flux.BuildGraph` | the node-side emitter shape: `FindRow` and `AddSample` are ANY_THREAD, the module reaches the record through `Zenith_ModuleStartContext::GetBudgetRunRecord()` and Flux through `Flux_RendererDesc::m_pxBudgetRunRecord` (Evidence.md "Area row emitters") (Z2-R-DEVT-091, Z2-R-DEVT-094); ENV's signatures stand; row DV26 notes that ENV's GPU rows can bind as `gpuPassSum` instead of being emitted |
| ENV (EnB17), GEOM (GB25) | debug variables; the profiler panel API; console variables | debug views are configuration keys set through the console's `set` (Console.md "Configuration variables"); panels through the panel catalogue (Overlay.md "Built-in panel catalogue") (Z2-R-DEVT-036, Z2-R-DEVT-050) |
| GEOM (GB25; Z2T-GEOM-015), COMPRESS (Z2T-COMPRESS-016), SCENE (Z2T-SCENE-052, -082), SCRIPT (Z2T-SCRIPT-058) | the `linux-tsan` WSL host and its build tree for the clang golden recordings | the Stage 1 `needs-human` TASK "Bring up the linux-tsan WSL host", which every one of these recording tickets blocks on (QualityTools.md "Host verification"; § "Stage 1") (Z2-R-DEVT-098 to Z2-R-DEVT-102, Z2-R-DEVT-132); rows DV28 and DV30 ask the four areas to link it. Z2T-PLAT-027 is PLAT's in-tree headless Linux platform mode, not the WSL host, and is not this need; SCRIPT-058's link to it does not provide the host (A5 row SB13's "RESOLVED" is corrected by row DV28) |
| ECS (094), CORE (034), JOBS (032, JB15) | `linux-tsan` sanitizer runs in Stage 2 | the same host, through PROC's `Sanitizers -Preset linux-tsan` step |
| MOD (MB14, Z2T-MOD-030) | the on-screen profiler sink and the profiler sink types the Player constructs at step 2 | the Player constructs one `Zenith_Profiler` (Tracy and the on-screen capture inside it) and calls `FillHooks` (Profiler.md "Creation and shutdown") (Z2-R-DEVT-003, Z2-R-DEVT-020); MOD-030's "only Tracy until the on-screen sink exists" no longer applies, and row DV12 names its rewrite |
| MOD (MB14, MOD-022), JOBS (J14), EDIT | the crash reporter created at step 7 and its thread registration through `m_pfnThreadLifecycle`; every `main` that creates a handler | `Zenith_CrashHandler::Create` at step 7, the JOBS-signature thunk, and `Zenith_RunCrashHelperIfRequested` directly after `Zenith_InstallRootHeap(&xHeap)` (which stays the first statement, [Z2-N-CONTRACT-142]) in the Player's, the editor's and ZenithPlatform's crash-test `main`; the one process-wide anchor that the OS crash callbacks reach the handler through (ruling Q88 (a), X273; Crash.md "Crash handler anchor") (Crash.md "Crash handler lifecycle", "Thread registration") (Z2-R-DEVT-075, Z2-R-DEVT-084, Z2-R-DEVT-120, Z2-R-DEVT-133); rows DV11, DV27 and DV29 (b) |
| SCRIPT (SB20) | the crash-handler attachment of the watchdog's hang report | `Zenith_CrashHandler::WriteHangReport` (Crash.md "Hang reports") (Z2-R-DEVT-083) |
| RHI (RB8, R15) | `r.capture` and the profiler's capture button; the crash-report attachment of `zenith2.device-loss`; the run telemetry record (losses, recoveries, cold-PSO flag) | the GPU capture button runs `r.capture 1` (Overlay.md "GPU view"); the crash report associates device-loss reports by time (Crash.md "Attachments"); `Zenith_RunTelemetry::RecordEvent` (Evidence.md "Run telemetry") (Z2-R-DEVT-046, Z2-R-DEVT-081, Z2-R-DEVT-096) |
| RPROD (Diagnostics.md; Z2T-RPROD-029), RHI (Z2T-RHI-075) | timestamps published "to the profiler hooks and Tracy" by the static, stateless `Flux_PassTimingPublisher::Publish(const Flux_FrameTimestampRecord&, Zenith_Span<const uint64>, uint32)` that RHI's `PrepareNextFrame` step 4 calls | a different shape, kept: the hook table has no GPU member ([Z2-N-REND-312] already says "published to the on-screen profiler and Tracy"), so the publisher calls `Zenith_ProfilerGpuSink::SubmitGpuTimings`. The stateless publisher reaches it as a registered sink, not through a profiler pointer: the renderer registers `Flux_RendererDesc::m_pxGpuTimingSink` with `Flux_FrameLifetime::SetGpuTimingSink`, and RHI's caller passes the registered sink as `Publish`'s fourth parameter (Profiler.md "GPU timing ingestion") (Z2-R-DEVT-014, Z2-R-DEVT-124). Rows DV5 (RPROD) and DV24 (RHI) carry the two halves and the revalidation of Z2T-RPROD-029 and Z2T-RHI-075, whose tests read `Zenith_TestProfilerGpuSink` instead of a capturing hook table |
| ASSET (AS15, AB2), SCRIPT (zones with values) | zone arguments (asset GUID, section index; entity count or simulation ID) | `Zenith_ProfileZoneValue` and `Zenith_ProfileScope::SetValue` (Profiler.md "Zenith_ProfileZone header") (Z2-R-DEVT-005) |
| COMPRESS (CM12, 024) | a "Compression" counter group summing `COUNTER_KIND_DELTA` values | the Compression panel (Overlay.md "Built-in panel catalogue") (Z2-R-DEVT-036) |
| CORE (CORE.md "On-screen profiler rows") | the memory and configuration panels | Overlay.md "Built-in panel catalogue", "Panel providers" (Z2-R-DEVT-033, Z2-R-DEVT-039) |
| MOD (MOD-027) | the startup panel reads `Zenith_Runtime::GetStartupReport` | the startup panel provider (Overlay.md "Panel providers") (Z2-R-DEVT-039) |
| GRAPH (G16) | dump readers refuse a newer `schemaVersion` with `Zenith_GraphDumpVersionMessage`; the profiler draws the frame timeline and node timings | Overlay.md "Graph view", "Timeline view" (Z2-R-DEVT-030, Z2-R-DEVT-043) |
| PLAT (PLAT.md "Frame-dump and config-dump entries") | the frame dump (DEVT's) includes the frame's platform events, input commands and I/O depths | the DEVT frame dump with its platform section provider (Console.md "DEVT frame dump") (Z2-R-DEVT-056) |
| ASSET (AS15), ASSET (048, 079, 080) | asset rows; dashboards read the cook report; content tooling reads the validation report | Overlay.md "Built-in panel catalogue"; QualityTools.md "Content validation reports", "Asset profiling" (Z2-R-DEVT-108, Z2-R-DEVT-109) |
| ASSET (017, 078), MOD (012) | the fingerprint reads the content hash and `composition.json` | Evidence.md "Fingerprint algorithm" (Z2-R-DEVT-087) |
| CORE (048) | the crash report attaches `Zenith_LogRingSink::CopyRecentLines` output | a different shape, kept: `CopyRecentLines` takes the sink lock, which a crashing thread cannot take, so the crash path reads DEVT's lock-free `Zenith_CrashLogMirror`; the on-screen console reads `CopyRecentLines` (Crash.md "Crash log mirror") (Z2-R-DEVT-080). CORE's API and tests are unchanged; row DV29 (a) asks CORE to correct `Zenith2/Docs/Design/CORE.md`'s sentence that the ring is "attached to crash reports by DEVT" and Z2T-CORE-048's cross-system line |
| CORE (failure procedure) | none recorded; DEVT's crash path must recognise `ZENITH_FATAL` | from CORE's existing behaviour alone (the step-5 `LOG_LEVEL_FATAL` record seen by the mirror sink, and `Zenith_TerminateProcess`'s exception code or trap), with no new CORE hook (Crash.md "Fatal failures") (Z2-R-DEVT-122) |
| CORE (CB8) | DEVT's first shedding case row in `axMEMORY_SHEDDING_CASES` | DEVT's consumer is `MEMORY_SUBSYSTEM_OTHER`, whose shedding row is "none / none"; its case row asserts no change (Profiler.md "Memory layout and accounting") (Z2-R-DEVT-018) |

### Upward interfaces DEVT declares

ZenithProfiling sits at layer 1, so every service above it reaches the profiler only through interfaces ZenithProfiling declares and the composition root passes down ([Z2-N-CONTRACT-175]):

| Interface (ZenithProfiling) | Implemented by | Purpose |
|---|---|---|
| `Zenith_ProfilerOutput` | ZenithRuntime (`Internal/DevTools/Zenith_DevToolsOutput.cpp`, over `Zenith_AsyncIO::WriteAtomic`) | capture segments, telemetry chunks and log chunks written to the user mount |
| `Zenith_ProfilerGpuSink` | `Zenith_Profiler` itself; tests (`Zenith_TestProfilerGpuSink`) | GPU pass timings from ZenithFlux's stateless publisher, registered with the frame lifetime |
| `Zenith_EvidenceFileWriter` | ZenithRuntime's budget-run driver (over PLAT's `Zenith_HostFile`); tests | the append-only evidence record and frames CSV |
| `Zenith_DevFileWriter` | ZenithRuntime (`Internal/DevTools/Zenith_DevFileWriterUserMount.cpp`) | console command output files under `user:/devtools/` |
| `Zenith_ProfilerPanelProvider` | each area's own provider (MOD's startup report, GRAPH's frame graph, RPROD's render graph and transient memory, PLAT's platform; CORE's configuration panel through its adapter TU), in the area's own ticket (ruling DEVT-AREA-REGISTRATION) | panels whose content is an object snapshot rather than counters |
| `Zenith_DevFrameDumpSection` | each area's own section (PLAT, ECS, ASSET, UI, VFX, …), in the area's own ticket | sections of the DEVT frame dump |
| `Zenith_PfnRegisterDevToolsArea` over `Zenith_DevToolsAreaServices` (`ZenithProfiling/Zenith_DevToolsAreaServices.h`) | each engine library's registration function, listed one line per area in ZenithRuntime's `Internal/DevTools/Zenith_DevToolsAreaList.cpp`; each module through `Zenith_ModuleStartContext` | the area-registration hand-off (below) |

#### Area registration

Ruling DEVT-AREA-REGISTRATION (2026-10-10) with its 2026-10-16 refinement DEVT-AREA-REGISTRATION-2: DEVT owns the console, overlay, panel, frame-dump and `RowSources.json` frameworks and their APIs, and every area registers its own commands, overlay tabs, panel providers, frame-dump sections and row sources in its own tickets. The interface lives in ZenithProfiling, the lowest library DEVT owns and the one every registering engine library except ZenithCore already depends on:

```cpp
// Zenith2/Engine/ZenithProfiling/Zenith_DevToolsAreaServices.h
class Zenith_DevConsole; class Zenith_DevFrameDump; class Zenith_ProfilerOverlay; class Zenith_DevFileWriter; class Zenith_DebugDraw;
struct Zenith_DevToolsAreaServices
{
	Zenith_DevConsole* m_pxConsole = nullptr;           // null in Shipping
	Zenith_DevFrameDump* m_pxFrameDump = nullptr;       // null in Shipping
	Zenith_ProfilerOverlay* m_pxOverlay = nullptr;      // present in Shipping, hidden
	Zenith_DevFileWriter* m_pxFileWriter = nullptr;     // null in Shipping
	Zenith_DebugDraw* m_pxDebugDraw = nullptr;
};
inline constexpr uint32 uDEVTOOLS_AREA_MAX_COMMANDS = 16;
inline constexpr uint32 uDEVTOOLS_AREA_MAX_CALLBACKS = 8;   // tabs, providers and frame-dump sections
class Zenith_DevToolsAreaTokens
{
public:
	// Takes ownership; false (the token is destroyed, which unregisters it) and one log line
	// "devtools: area <name> exceeds <n> commands" (or callbacks) when full.
	bool AddCommand(Zenith_DevCommandToken&& xToken);
	bool AddCallback(Zenith_CallbackToken&& xToken);
	uint32 GetCommandCount() const;
	uint32 GetCallbackCount() const;
	void SetAreaName(const char* szArea);       // by Zenith_DevTools before the registration function runs
private:
	Zenith_DevCommandToken m_axCommands[uDEVTOOLS_AREA_MAX_COMMANDS];
	Zenith_CallbackToken m_axCallbacks[uDEVTOOLS_AREA_MAX_CALLBACKS];
	uint32 m_uCommandCount = 0;
	uint32 m_uCallbackCount = 0;
	const char* m_szAreaName = "";
};
// pContext is the area's own context struct (declared in the area's header), returned by the list entry's getter.
using Zenith_PfnRegisterDevToolsArea = void (*)(const Zenith_DevToolsAreaServices& xServices, void* pContext,
	Zenith_DevToolsAreaTokens& xTokensOut);
```

```cpp
// Zenith2/Engine/ZenithRuntime/Internal/DevTools/Zenith_DevToolsAreaList.h
// The live objects areas read, filled by the composition root at step 13 (every pointer non-owning, null when absent).
struct Zenith_DevToolsRuntimeObjects
{
	Zenith_Runtime* m_pxRuntime = nullptr;               // startup report, world list
	Zenith_FrameExecutor* m_pxFrameExecutor = nullptr;
	Flux_Renderer* m_pxRenderer = nullptr;
	Zenith_ConfigStore* m_pxConfig = nullptr;
	Zenith_MemoryGovernor* m_pxGovernor = nullptr;
	Zenith_WorldRegistry* m_pxWorlds = nullptr;
	Zenith_SceneRegistry* m_pxScenes = nullptr;
	Zenith_AssetRegistry* m_pxAssets = nullptr;
};
struct Zenith_DevToolsAreaEntry
{
	const char* m_szArea;                                                         // "ECS", "ASSET", "CORE", …
	Zenith_PfnRegisterDevToolsArea m_pfnRegister;
	void* (*m_pfnGetContext)(const Zenith_DevToolsRuntimeObjects&);      // returns the area's context; may be null
};
```

- Engine libraries at or above ZenithProfiling (JOBS, GRAPH, ECS, SCENE, ASSET, PLAT, RHI, RPROD, ENV, …) define their registration function and their context struct in their own library, and add one entry `{ "<AREA>", &<register>, &<getter> }` to the `constexpr` array of `Zenith2/Engine/ZenithRuntime/Internal/DevTools/Zenith_DevToolsAreaList.cpp`, each in its own ticket (the "call sites other areas add" rule); the getter is a ZenithRuntime function in the same file that returns the area's context built from `Zenith_DevToolsRuntimeObjects` (a world registry for ECS's `ecs.dump`, the asset registry for ASSET's `asset.dump`, the store and the governor for CORE's commands, the runtime for MOD's `startup.report` and `world.list`). `Zenith_DevTools` calls every entry at startup step 13 with the services, the entry's context and a fresh `Zenith_DevToolsAreaTokens` (its area name set), and keeps the tokens until shutdown, so the registrations end before the console and overlay are destroyed. A registrant adds every token through `AddCommand` or `AddCallback`; the capacity is a refusal with one log line, never an assert.
- An area below ZenithProfiling (ZenithCore: the `memory` tab, `config.dump`, `memory.stats`, the configuration panel) registers through one adapter translation unit under ZenithRuntime, `Zenith2/Engine/ZenithRuntime/Internal/DevTools/Areas/Zenith_<Area>DevTools.cpp`, written in that area's own ticket and calling only the area's public query API; its list entry names the adapter's function and getter.
- Modules register from `Start` through `Zenith_ModuleStartContext::GetDevConsole`, `GetDebugDraw`, `GetProfilerOverlay` and `GetDevFrameDump` (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces"), which MOD's Stage 2 accessor ticket implements from `Zenith_DevTools`; a module's context is its own state; the module host journals and undoes each registration with the module.
- The declarations above land first, as their own API slice, so every area's registration ticket blocks on that slice rather than on the runtime aggregate.
- DEVT itself registers only its own commands (`help`, `set`, `get`, `keys`, `echo`, `history`, `exec`, `profiler.*`, `devtools.*`, `crash.*`) and its own tabs (`frame`, `gpu`, `history`, `counters`, `devtools`).

ZenithPlatform's crash handler declares no upward interface; the script watchdog, the composition root and the remote server call it directly.

### Needs recorded for other areas

`C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` rows `DV1` to `DV35` carry each area need in full. Every change to a core document goes through the cross-document queue `C:/dev/Zenith2-drafts/reviews/CROSS_DOC_ITEMS.md` rows X245 to X248, X273, X282 and X308, which the core-doc batch applies; a DV row only names its X row, never routes core wording on its own. The DEVT sections that wait on each core-document change:

| X row | Core document change | DEVT sections waiting on it | DV rows (area halves) |
|---|---|---|---|
| X245 (a) | `Architecture/CompositionRoot.md` → "Startup sequence": the one `Zenith_Profiler` at step 2; `Zenith_RuntimeMain` and `Zenith_Runtime::Create` take `Zenith_Profiler&`; step 7's `Zenith_DevTools`, `Zenith_LogFileSink`, `Zenith_CrashHandler`; a budget-run input error at step 7 is exit code 2 | Profiler.md "Creation and shutdown"; Evidence.md "Workload runner" | DV3, DV12 |
| X245 (b) | "The frame loop" step 3 is `Zenith_DevTools::EndFrame` (profiler frame, overlay, console queue, remote poll, telemetry, budget-run sampling) | § "Frame placement"; Console.md "Command execution" | DV4 |
| X245 (c) | "Module interfaces": `Zenith_ModuleStartContext` gains `GetDevConsole()`, `GetDebugDraw()`, `GetProfilerOverlay()`, `GetBudgetRunRecord()` and `RegisterBudgetEmitter(const char*, void (*)(Zenith_BudgetRunRecord&))` | Evidence.md "Area row emitters"; Overlay.md "Panel providers" | DV12, DV21 |
| X245 (d) | "Command-line options": the Windows `--zenith-crash-helper` exception to [Z2-N-ARCH-860], dispatched by every main that creates a crash handler; Architecture.md "Security architecture" records the helper's trust rules | Crash.md "Windows capture", "Crash helper security contract" | DV11, DV27, DV29 (b) |
| X245 (e) | Contracts.md shutdown table step 8 and MOD's step 6: every DEVT output is detached before `Zenith_AsyncIO` is destroyed | § "Cancellation and shutdown"; Console.md "Log file sink" | DV12 |
| X245 (f), (g), (h), (i), (j) | the `Tools/DevTool/` tool row; Tracy's `linux` platform; the Tracy exception to [Z2-N-ARCH-959]; the evidence-directory grant admitting the read of `rows.json` and `fingerprint.json`; CPUFrameGraph "Required tests" placing `Test_FrameGraph_ZonesForEveryNode` in `Zenith2/Tests/DevTools/` | § "Shaders tools scripts and integration suites"; Profiler.md "Tracy forwarding"; Evidence.md "Budget-run inputs"; Tests.md "Integration suites" | DV1, DV2, DV9, DV13 (b) |
| X246 | `Rendering.md` → "Render-world record types" gains `Flux_RenderDebugDraw`; `Rendering/PerPass/Common.md` → "DebugOverlay" gains `m_uQuadBase` and `m_uReversedZ` (the view's depth mode per Z2-D-061) | DebugDraw.md "Flux_DebugDraw header", "DebugOverlay pass" | DV8 |
| X247 | Contracts.md [Z2-N-CONTRACT-144] (pending Q79), the `z2-sanitizer-defect` TSan and UBSan patterns of [Z2-N-CONTRACT-413], and the `Zenith2Memory` paths gaining `Zenith2/Engine/ZenithProfiling/**`, `Zenith2/Engine/ZenithPlatform/Internal/CrashHandler/**`, `Zenith2/Tests/DevTools/**`, `Zenith2/Tests/TsanSmoke/**` | Profiler.md "Thread slots and the thread-local variable"; QualityTools.md "TSan smoke suite"; Tests.md "Stress and sanitizer runs" | DV15 (a), DV17 (d) |
| X248 | `Scripting/PublicTypes.md`: `Zenith_ScriptWatchdogDesc::m_pxCrashHandler` | Crash.md "Hang reports" | DV14 |
| X273 with its amendment X282, completed by X308 | ruling Q88 (a): Z2-D-046's dated extension in `DecisionLog.md` (the anchor kept under either realisation; under Crashpad it serves only `CRASH_HANDLER_ERROR_ALREADY_INSTALLED`); Contracts [Z2-N-CONTRACT-141] and the allowlist entries of [Z2-N-CONTRACT-152] gain `g_pxCrashHandlerAnchor`; CompositionRoot [Z2-N-ARCH-866]; the anchor is written by `Create`, by the destructor, by the crash path's one claim and by the same trampoline's unclaim; X308: Contracts' `g_` naming row ("the five allowlisted variables") and WorkerGuide's forbidden-constructs row | Crash.md "Crash handler anchor" | DV17 (e), DV17 (g) (the protected `Zenith2/CLAUDE.md` naming bullet and non-negotiable 3, PROC `needs-human`), DV35 (CORE: `Design/CORE.md`'s count of the allowlisted variables) |

The area-only rows: ARCH and SHIP DV2 (the manifest entry); GRAPH DV32 (Z2T-GRAPH-050's console tests follow the `ShippingTest` rule); RPROD DV33 (the on-screen profiler toggle `r.graph.timestamps` follows in Shipping is `devtools.profiler.enabled`); CORE DV34 (`Zenith_BoundedQueue::GetAllocatedBytes`); RPROD DV5 (the desc members, the publisher's sink parameter, Z2T-RPROD-029's revalidation) and DV6 (the font atlas); LIGHT DV7 (LIGHT rewords its "Debug views": its lighting views are LIGHT's own passes at `FLUX_EXTENSION_POINT_OVERLAY`, not drawn by DEVT's pass); JOBS DV10 (the lane field's type); PLAT DV13 (six `Engine.Dev` actions, two gesture recognisers, the evidence read-open, the memory sampler, the input-script loop, the Android exit records); SHIP DV16 (the WSL step order with the LLVM install, `Test-LinuxTsanHost.ps1 -Native`, `-Package`); PROC DV17 (the gate requirements, the assert scripts, the Shipping symbols, the `Zenith2Memory` gate list, the crash anchor's allowlist entry, running `win-shipping-test` in `Test`); GEOM DV18; ECS, ASSET, SCENE, MOD DV19 (runtime commands, ECS's `ecs.dump <worldTag> <path>` form adopted unchanged); CORE DV20 (the shedding case row), DV22 (change dispatch for console-set values), DV23 (the `Zenith_SpscRing` constraints of ruling Q80) DV29 (the stale CORE.md crash sentence, a test target's own forwarding main, the pool-gauge convention) and DV35 (`Design/CORE.md` → "Headings this area relies on" counting five allowlisted variables, with X308); STREAM, GAME, AUDIO, VFX, ANIM, UI DV21; RHI DV24 (the frame lifetime's sink and Z2T-RHI-075's revalidation); GRAPH DV25 (Z2T-GRAPH-050's tests use the real console); ENV DV26 (node-side emission confirmed; GPU rows as `gpuPassSum`); EDIT DV27 (the editor main dispatches the crash helper); SCRIPT DV28 (Z2T-SCRIPT-058 blocks on the host bring-up TASK, not Z2T-PLAT-027); COMPRESS, GEOM, SCENE DV30 (their recording tickets block on the TASK); every area whose rows the panel catalogue carries DV31 (who writes the rows).
Lock-profiling needs. The profiler's lock hooks, its unprofiled primitives and its dispatch calls rest on the following in other areas, now stated in `Zenith2/Docs/Contracts.md` ([Z2-N-CONTRACT-143], [Z2-N-CONTRACT-589] to [Z2-N-CONTRACT-591]) and `Zenith2/Docs/Design/PROC/StructuralGates.md` (rule F26). CORE: `Zenith_PfnOnLockAcquire` gains `uint64 ulWaitTicks` (the wait in monotonic nanoseconds, 0 for an uncontended acquire or a `TryLock` success) in `Zenith_ProfilerHooks.h`; every platform threading class is a template on one `bool` with the aliases `Zenith_Mutex` / `Zenith_MutexNoProfiling`, `Zenith_ConditionVariable` / `Zenith_ConditionVariableNoProfiling` and `Zenith_Semaphore` / `Zenith_SemaphoreNoProfiling`; the six dispatch functions `Zenith_DispatchZoneBegin`, `Zenith_DispatchZoneEnd`, `Zenith_DispatchCounter`, `Zenith_DispatchLockAcquire`, `Zenith_DispatchLockRelease` and `Zenith_DispatchBlockingWait` and the thread-local hook depth `m_uHookDepth` of `g_xDebugThreadState`; everything the hooks reach (the clock, `Zenith_SpscRing`, the debug thread state, the log router) uses the unprofiled variants. PROC: `ForbiddenCheck` rule F26 (and its `profilerReachable` list), which refuses a profiled `Zenith_Mutex`, `Zenith_ConditionVariable` or `Zenith_Semaphore` token in the non-test sources of `Zenith2/Engine/ZenithProfiling/**`, of `Zenith2/Engine/ZenithCore/Internal/Profiling/**` and of the list, and a direct call of a `m_pfnOn…` hook member outside `Zenith_ProfilerHooks.h`, with failing and passing fixtures; test files are exempt, so DEVT's contended-lock and hammer cases may lock a profiled mutex. JOBS, ECS, ASSET, RHI and every other caller of a hook member convert to the dispatch functions in a separate pass. PROC's `profilerReachable` list gains `Zenith2/Engine/ZenithPlatform/Internal/CrashHandler/Zenith_CrashLogMirror.cpp` and `.h`, the one DEVT log sink whose `Write` runs on the logging thread, possibly inside a hook (`Zenith_LogFileSink::Write` does nothing, and ZenithProfiling is covered by directory). The blocking-wait hook takes `uint64 ulWaitTicks` and is dispatched after the wait returns, so SCRIPT's `"debugger-stop"` call passes its measured wait.

## Library and file layout

### ZenithProfiling target

`Zenith2/Engine/ZenithProfiling/CMakeLists.txt` is exactly:

```cmake
zenith_add_library(ZenithProfiling DEPENDS ZenithCore THIRD_PARTY tracy TEST_LABELS memory)
```

with the default `SUPPORTED ZENITH_ALL` (every preset including `linux-tsan` and both Android ABIs). `TEST_LABELS memory` puts every `ZenithProfiling_Tests` case under the `Zenith2Memory` category's `Test -Label memory` line, because the hook path, the name table, the recorder and the queues are concurrency code. A label does not trigger the category's gates: [Z2-N-CONTRACT-357] runs the stress tests through the `Sanitizers` step of `Zenith2Memory`, whose paths (Contracts → "Enforceability matrix": ZenithCore, the job graph, ZenithScript's VM glue) do not yet include ZenithProfiling, so X247 (c) adds `Zenith2/Engine/ZenithProfiling/**`, `Zenith2/Engine/ZenithPlatform/Internal/CrashHandler/**`, `Zenith2/Tests/DevTools/**` and `Zenith2/Tests/TsanSmoke/**` to the matrix row, and row DV17 (d) hands the matching protected gate-list edit to PROC. With ruling Q80 the per-thread ring is CORE's `Zenith_SpscRing`, which is already inside ZenithCore's paths; ZenithProfiling stays in the request for the name table, the recorder and the hook path.

### ZenithProfiling public headers

| Header | Declares | Specified in |
|---|---|---|
| `Zenith_ProfilerTypes.h` | `Zenith_ProfilerLaneID`, `Zenith_ProfilerSiteID`, `Zenith_ProfilerCounterID`, `Zenith_ProfilerMode`, `Zenith_ProfilerLaneKind`, `Zenith_ProfilerUnit`, `Zenith_ProfilerError`, the capacity constants | Profiler.md "Zenith_ProfilerTypes header" |
| `Zenith_Profiler.h` | `Zenith_ProfilerDesc`, `Zenith_ProfilerSettings`, `Zenith_ProfilerOutput`, `Zenith_ProfilerFrameInputs`, `Zenith_ProfilerGpuPass`, `Zenith_ProfilerGpuSubmission`, `Zenith_ProfilerFrameSummary`, `Zenith_ProfilerZone`, `Zenith_ProfilerFrameView`, `Zenith_ProfilerCounterView`, `Zenith_ProfilerZoneStat`, `Zenith_ProfilerLaneInfo`, `Zenith_Profiler` | Profiler.md "Zenith_Profiler header" |
| `Zenith_ProfileZone.h` | `Zenith_ProfileScope`, `Zenith_ProfileZoneValue`, `ZENITH_PROFILE_SCOPE`, `ZENITH_PROFILE_SCOPE_NAMED` | Profiler.md "Zenith_ProfileZone header" |
| `Zenith_ProfilerCapture.h` | the capture format constants and `Zenith_ProfilerCaptureReader` | Profiler.md "Capture format" |
| `Zenith_ProfilerPanels.h` | `Zenith_ProfilerRowDesc`, `Zenith_ProfilerPanelDesc`, `Zenith_ProfilerExpression`, `Zenith_ProfilerPanelBuilder`, `Zenith_ProfilerPanelProvider` | Overlay.md "Zenith_ProfilerPanels header" |
| `Zenith_ProfilerOverlay.h` | `Zenith_ProfilerOverlayInput`, `Zenith_ProfilerOverlayState`, `Zenith_ProfilerOverlayDesc`, `Zenith_ProfilerOverlay` | Overlay.md "Zenith_ProfilerOverlay header" |
| `Zenith_DevConsole.h` | `Zenith_DevCommandArgs`, `Zenith_DevCommandOutput`, `Zenith_PfnDevCommand`, `Zenith_DevCommandDesc`, `Zenith_DevCommandToken`, `Zenith_DevConsoleDesc`, `Zenith_DevConsole` | Console.md "Zenith_DevConsole header" |
| `Zenith_DevFileWriter.h` | `Zenith_DevFileWriter`, `Zenith_DevFileError` | Console.md "Zenith_DevFileWriter header" |
| `Zenith_DevFrameDump.h` | `Zenith_DevFrameDumpSection`, `Zenith_DevFrameDump` | Console.md "DEVT frame dump" |
| `Zenith_DevToolsAreaServices.h` | `Zenith_DevToolsAreaServices`, `Zenith_DevToolsAreaTokens`, `Zenith_PfnRegisterDevToolsArea` | § "Area registration" |
| `Zenith_DebugDraw.h` | `Zenith_DebugDrawFlags`, `Zenith_DebugDrawDesc`, `Zenith_DebugDrawStats`, `Zenith_DebugDraw`, `Zenith_DebugDrawBatchView` | DebugDraw.md "Zenith_DebugDraw header" |
| `Zenith_LogFileSink.h` | `Zenith_LogFileSinkDesc`, `Zenith_LogFileSink` | Console.md "Log file sink" |
| `Zenith_BudgetRunRecord.h` | `Zenith_BudgetRowSpec`, `Zenith_BudgetRunInputs`, `Zenith_BudgetRunError`, `Zenith_BudgetRunRecord` | Evidence.md "Zenith_BudgetRunRecord header" |
| `Zenith_Telemetry.h` | `Zenith_TelemetryEventKind`, `Zenith_TelemetryEvent`, `Zenith_RunTelemetryDesc`, `Zenith_RunTelemetry` | Evidence.md "Run telemetry" |
| `Zenith_DevToolsConfig.h` | `Zenith_RegisterDevToolsConfigKeys`, the key-name constants | § "Configuration keys" |

### ZenithProfiling internal files

| File | Content |
|---|---|
| `Internal/Hooks/Zenith_ProfilerHooksImpl.cpp`, `.h` | the six hook functions, the thread-slot claim, the per-thread name cache |
| `Internal/Hooks/Zenith_ProfilerThreadSlot.cpp`, `.h` | the one allowlisted `thread_local Zenith_ProfilerThreadSlot g_xProfilerThreadBuffers` ([Z2-N-CONTRACT-152]) |
| `Internal/Hooks/Zenith_ProfilerThreadState.h` | the thread state: the CORE `Zenith_SpscRing<Zenith_ProfilerEvent>`, depth, the captured and Tracy masks, value counts, caches, the open-zone stack, the lane, the lock state |
| `Internal/Hooks/Zenith_ProfilerLockBank.h` | the per-thread lock state: `Zenith_ProfilerLockEntry`, `Zenith_ProfilerLockBank`, `Zenith_ProfilerHeldLock`, `Zenith_ProfilerThreadLockState` (Profiler.md "Lock statistics") |
| `Internal/Hooks/Zenith_ProfilerLockStats.cpp`, `.h` | the lock hooks' statistics update, the bank handover and the end-of-frame merge |
| `Internal/Capture/Zenith_ProfilerSync.h` | `Zenith_ProfilerSyncTestAccess`: the tests' view of the profiler's own unprofiled primitives (Profiler.md "Lock and wait events") |
| `Internal/Capture/Zenith_ProfilerNameTable.cpp`, `.h` | the lock-free insert-only name table (sites and counters), with `Zenith_ProfilerNameTableTestAccess` |
| `Internal/Capture/Zenith_ProfilerCounters.cpp`, `.h` | the counter table, per-frame snapshot, history |
| `Internal/Capture/Zenith_ProfilerLanes.cpp`, `.h` | the lane table |
| `Internal/Frames/Zenith_ProfilerFrameAssembler.cpp`, `.h` | frame assembly, the open-zone carry, zone statistics |
| `Internal/Frames/Zenith_ProfilerFrameStore.cpp`, `.h` | the frame history ring and the detailed-frame ring |
| `Internal/Frames/Zenith_ProfilerGpu.cpp`, `.h` | the GPU submission queue and GPU lanes |
| `Internal/Capture/Zenith_ProfilerCaptureWriter.cpp`, `.h` | segment encoding, the flush thread `Zenith.ProfFlush` |
| `Internal/Capture/Zenith_ProfilerCaptureReader.cpp` | the reader behind `Zenith_ProfilerCapture.h` |
| `Internal/Tracy/Zenith_TracyBridge.h` | `Zenith_TracySourceLocation`, the bridge table `Zenith_TracyBridgeTable`, `Zenith_GetTracyBridgeTable` and the tests' seam `Zenith_ProfilerTestAccess::SetTracyBridge` (Profiler.md "Tracy forwarding") |
| `Internal/Tracy/Zenith_ProfilerTestAccess.cpp` | the definition of `Zenith_ProfilerTestAccess::SetTracyBridge`; it includes no Tracy header, so a `ShippingTest` test that calls it never pulls `Zenith_TracyBridge.cpp` into a link without Tracy |
| `Internal/Tracy/Zenith_TracyBridge.cpp` | the bridge over Tracy's C API (`TracyC.h`), the only file including a Tracy header |
| `Internal/Overlay/Zenith_ProfilerOverlay.cpp`, `Zenith_OverlayLayout.cpp`, `Zenith_OverlayTimeline.cpp`, `Zenith_OverlayLocks.cpp`, `Zenith_OverlayHistory.cpp`, `Zenith_OverlayPanels.cpp`, `.h` files | the overlay |
| `Internal/Panels/Zenith_ProfilerExpression.cpp`, `.h` | the expression compiler and evaluator |
| `Internal/Panels/Zenith_PanelCatalogue.cpp` and one `Zenith_Panels<Area>.cpp` per area of Overlay.md "Built-in panel catalogue" | the constexpr panel tables |
| `Internal/Console/Zenith_DevConsole.cpp`, `Zenith_DevConsoleParser.cpp`, `Zenith_DevConsoleBuiltins.cpp`, `.h` | the console |
| `Internal/Console/Zenith_DevConsoleExecute.cpp` | the development-only translation unit defining `Zenith_DevConsole::ExecuteStatement`, the console's Shipping-forbidden symbol (Console.md "Shipping absence of development services") |
| `Internal/DevTools/Zenith_DevFrameDump.cpp`, `Zenith_DevFrameDumpWrite.cpp` | the frame dump; the second file is development-only and defines `Zenith_DevFrameDump::WriteFrame` |
| `Internal/DebugDraw/Zenith_DebugDraw.cpp`, `Zenith_DebugDrawShapes.cpp`, `Zenith_DebugDrawSlots.cpp`, `.h` | the recorder; the slots file is the one that differs between the two paths of ruling Q79 (DebugDraw.md "Recorder slot alternative") |
| `Internal/Logging/Zenith_LogFileSink.cpp`, `Zenith_LogJson.cpp`, `.h` | the file sink and the record encoder |
| `Internal/Evidence/Zenith_BudgetRunRecord.cpp`, `Zenith_EvidenceWriter.cpp`, `Zenith_BudgetStatistics.cpp`, `.h` | the budget-run record |
| `Internal/Evidence/Zenith_RunTelemetry.cpp`, `.h` | run and frame telemetry |
| `Internal/Zenith_DevToolsConfig.cpp`, `Internal/Zenith_DevToolsLog.h` | key registration; the DEVT log categories |

### ZenithProfiling platform files

| File | Content |
|---|---|
| `Internal/Windows/Zenith_ProfilerPlatform_Windows.cpp` | `GetThreadDescription` for automatic lane names, `GetCurrentProcessorNumber` for the lane's last core, `Zenith_ProfilerPlatformThreadNameLimit()` returning 63 |
| `Internal/Android/Zenith_ProfilerPlatform_Android.cpp` | `pthread_getname_np`, `sched_getcpu`, `Zenith_ProfilerPlatformThreadNameLimit()` returning 15 |
| `Internal/Linux/Zenith_ProfilerPlatform_Linux.cpp` | `pthread_getname_np`, `sched_getcpu`, `Zenith_ProfilerPlatformThreadNameLimit()` returning 15 |

Tracy is compiled on every platform of the manifest entry; in `linux-tsan` the bridge is linked but its `m_pfnStartup` is never called, because `devtools.tracy.enabled` is refused there (`Zenith_IsSanitizerBuild()`), and the entry's options turn off every Tracy feature that starts a thread at load (§ "Configuration keys"; Profiler.md "Tracy forwarding").

### ZenithProfiling test files

| File | Content |
|---|---|
| `Tests/Zenith_Profiler_Tests.cpp` | hooks, rings, lanes, counters, frames, GPU, captures, stress |
| `Tests/Zenith_ProfileZone_Tests.cpp` | the zone macros (two scopes in one block, nested scopes) |
| `Tests/Zenith_ProfilerNameTable_Tests.cpp` | the one custom container, with its oracle, property, size-overflow and stress tests ([Z2-N-CONTRACT-288] to [Z2-N-CONTRACT-290], [Z2-N-CONTRACT-357]) |
| `Tests/Zenith_ProfilerOverlay_Tests.cpp`, `Tests/Zenith_ProfilerExpression_Tests.cpp`, `Tests/Zenith_ProfilerPanels_Tests.cpp` | the overlay |
| `Tests/Zenith_DevConsole_Tests.cpp` | the console |
| `Tests/Zenith_DebugDraw_Tests.cpp` | the recorder |
| `Tests/Zenith_LogFileSink_Tests.cpp` | the file sink |
| `Tests/Zenith_BudgetRunRecord_Tests.cpp`, `Tests/Zenith_RunTelemetry_Tests.cpp` | evidence and telemetry |
| `Tests/Zenith_Profiler_DefectTests.cpp` | death tests, each in the suite its macro selects (`z2-death-assert` or `z2-death-check`, Tests.md "Death cases") |
| `Tests/Fuzz/ProfilerCapture.cpp`, `Tests/Fuzz/DevConsoleLine.cpp`, `Tests/Fuzz/BudgetRunRows.cpp` | the three parsers of untrusted input ([Z2-N-ARCH-952]) |
| `Tests/Support/Zenith_ProfilerTestSupport.h` | header-only helpers ([Z2-N-ARCH-255], X184): `Zenith_ScopedProfilerInstall`, `Zenith_TestProfilerOutput`, `Zenith_TestDevFileWriter`, `Zenith_TestProfilerGpuSink`, `Zenith_TestEvidenceFileWriter`, `Zenith_TestEmitFrame` (Tests.md "Test targets") |
| `Tests/Examples/Zenith_Profiler_Example.cpp`, `Zenith_DevConsole_Example.cpp`, `Zenith_DebugDraw_Example.cpp` | compiled examples cited by the contract blocks |
| `Tests/Fixtures/Capture/`, `Tests/Fixtures/Expressions/`, `Tests/Fixtures/BudgetRun/` | committed text fixtures (Tests.md "Fixtures") |

### DEVT files in other libraries

| Library | Files |
|---|---|
| `ZenithPlatform` | `Zenith_CrashHandler.h`; `Internal/CrashHandler/Zenith_CrashHandlerAnchor.cpp`, `.h` (the one allowlisted variable `g_pxCrashHandlerAnchor` and its four transition functions, Crash.md "Crash handler anchor"), `Zenith_CrashHandler.cpp`, `Zenith_CrashReport.cpp`, `.h` (the report writer and the signal-path `Zenith_CrashJsonTail`, Crash.md "Signal-safe report tail"), `Zenith_CrashAnnotations.cpp`, `Zenith_CrashLogMirror.cpp`, `.h` (the shared walk and `Zenith_CrashLogMirrorTestAccess`), `Zenith_CrashFatal.cpp`, `Zenith_CrashFinalize.cpp`, `.h`; `Internal/Windows/Zenith_CrashHandler_Windows.cpp`, `Zenith_CrashHelper_Windows.cpp`, `.h` (`Zenith_RunCrashHelperIfRequested`, `Zenith_CrashHelperValidate`, `Zenith_CrashHelperBuildLaunch`, `Zenith_CrashHelperSection`, Crash.md "Crash helper section layout"); `Internal/Android/Zenith_CrashHandler_Android.cpp`, `Zenith_CrashExitInfo_Android.cpp`; `Internal/Linux/Zenith_CrashHandler_Linux.cpp` (the unsupported stub, Crash.md "Linux capture"); `Internal/Windows/Zenith_CrashHandlerTestAccess_Windows.cpp` and its `Internal/Android/`, `Internal/Linux/` counterparts (the test-access bodies through which the tests reach the Windows realisation, Tests.md "Test targets"); `Tests/Zenith_CrashHandler_Tests.cpp`, `Tests/Zenith_CrashHandlerDefect_Tests.cpp` (the crash cases and the skipped child case `Test_CrashChild_RunScenario`, launched through ZenithTestSupport's `Zenith_TestProcess::LaunchSelf`, ruling X349 (a)), `Tests/Zenith_PlatformTestMain.cpp` (the target's own `main`: `Zenith_ApplyCrtMathMode()`, the root heap, the helper dispatch, then `Zenith_RunTestMain`), `Tests/Fuzz/CrashReport.cpp` and its corpus; no `Tests/<Platform>/` directory ([Z2-N-ARCH-162]) |
| `ZenithFlux` | `Flux_DebugDraw.h`; `Internal/Debug/Flux_DebugOverlay.cpp` (the definition of RPROD's `Flux_AddDebugOverlay` declaration in `Flux_Features.h`), `.h`, `Flux_DebugFont.cpp`, `Flux_DebugFontGlyphs.h`; `Tests/Debug/Flux_DebugOverlay_Tests.cpp` |
| `ZenithRuntime` | `Internal/DevTools/Zenith_DevTools.cpp`, `.h` (the aggregate the runtime owns, with `Zenith_DevToolsMemoryConsumer`), `Zenith_DevToolsOutput.cpp`, `Zenith_DevFileWriterUserMount.cpp`, `.h`, `Zenith_DebugDrawExtractor.cpp`, `Zenith_OverlayInputAdapter.cpp`, `Zenith_DevToolsServer.cpp`, `.h`, `Zenith_DevToolsProtocol.cpp`, `.h`, `Zenith_RuntimeDevCommands.cpp` (`exec`), `Zenith_DevToolsAreaList.cpp` (the area-registration list, one line per area, § "Area registration"), `Zenith_BudgetRunDriver.cpp`; the `Internal/DevTools/Areas/Zenith_<Area>DevTools.cpp` adapter files are the registering areas' own; `Tests/DevTools/Zenith_DevTools_Tests.cpp`; `Tests/Fuzz/RemoteFrame.cpp` and its corpus |
| `ZenithDevTool` (tool) | `Tools/DevTool/Internal/Zenith_DevToolProcess.h`, `Internal/Windows/Zenith_DevToolProcess_Windows.cpp` (Console.md "Remote client"; the remote client's socket is PLAT's `Zenith_DevClient`, so the tool has no socket file), the subcommand files, the tool's main; `Tools/DevTool/Tests/` |
| shaders | `Zenith2/Shaders/Passes/Debug/DebugLines.slang`, `Zenith2/Shaders/Passes/Debug/DebugText.slang`, `Zenith2/Shaders/Common/DebugDrawRecords.slang` (the shared record layout module) |

### Shaders tools scripts and integration suites

| Path | Kind | Content |
|---|---|---|
| `Zenith2/Tools/DevTool/` | `zenith_add_tool(ZenithDevTool DEPENDS ZenithCore ZenithProfiling ZenithPlatform)`, `SUPPORTED ZENITH_TOOLS_HOST` | the host CLI of QualityTools.md "ZenithDevTool" |
| `Zenith2/Benchmarks/Run-Workload.ps1` | PowerShell | the workload runner (Evidence.md "Workload runner") |
| `Zenith2/Benchmarks/Evidence/Zenith2Evidence.psm1` | PowerShell module | the fingerprint functions (Evidence.md "Zenith2Evidence module") |
| `Zenith2/Benchmarks/Evidence/Tests/Test-Zenith2Evidence.ps1`, `Zenith2/Benchmarks/Evidence/Tests/Fixtures/**` | assert script and fixtures | Evidence.md "Fingerprint invalidation tests" |
| `Zenith2/Benchmarks/Workloads/RowSources.json` | data | Evidence.md "Row source bindings" |
| `Zenith2/Benchmarks/Fuzz/Run-FuzzCampaign.ps1`, `Zenith2FuzzCampaign.psm1`, `Tests/Test-FuzzCampaign.ps1` | PowerShell, module and assert script | QualityTools.md "Fuzz campaign runner" |
| `Zenith2/Benchmarks/ProfilerOverhead/` | `zenith_add_benchmark(ProfilerOverhead DEPENDS ZenithProfiling ZenithCore)` | Profiler.md "Profiler cost model" (including the name table's justification cases) |
| `Zenith2/Benchmarks/CrashPrototype/` | the D-SEL-CRASH harness | Crash.md "D-SEL-CRASH candidates and acceptance" |
| `Zenith2/Tools/DevTool/Scripts/Invoke-LinuxTsan.ps1`, `Test-LinuxTsanHost.ps1`, `Zenith2LinuxTsan.psm1`, `Tests/Test-Zenith2LinuxTsan.ps1`; `Zenith2/Tools/DevTool/Wsl/wsl.conf` | PowerShell and data | QualityTools.md "The linux-tsan host"; the distro, its packages, the LLVM pin and the kernel setting are provisioned by SHIP's `Setup-Zenith2.ps1` (row DV16) |
| `Zenith2/.clang-tidy` | configuration | QualityTools.md "Static analysis" |
| `Zenith2/Tests/DevTools/` | `zenith_add_integration_tests(DevTools DEPENDS ZenithProfiling ZenithJobs ZenithGraph ZenithCore SUPPORTED ZENITH_HOST)` | CPU integration tests (Tests.md "Integration suites") |
| `Zenith2/Tests/DevToolsRender/` | `zenith_add_integration_tests(DevToolsRender DEPENDS ZenithRuntime ZenithFlux ZenithProfiling SUPPORTED ZENITH_WINDOWS)` | WARP and lavapipe render tests (Tests.md "Render tests") |
| `Zenith2/Tests/TsanSmoke/` | `zenith_add_integration_tests(TsanSmoke DEPENDS ZenithCore SUPPORTED ZENITH_HOST)` | QualityTools.md "TSan smoke suite" |

### Supported matrices

| Target | `SUPPORTED` | Reason |
|---|---|---|
| `ZenithProfiling` | `ZENITH_ALL` | every library above it runs in every preset |
| DEVT files in `ZenithPlatform` | its `ZENITH_ALL`; platform files narrowed by `Internal/<Platform>/` | the crash handler exists on every platform; Linux is the test host |
| DEVT files in `ZenithFlux`, `ZenithRuntime` | their `ZENITH_GPU` | GPU libraries exclude `linux-tsan` ([Z2-N-ARCH-173]) |
| `ZenithDevTool` | `ZENITH_TOOLS_HOST` | a host tool |
| `Zenith_Integration_DevTools`, `Zenith_Integration_TsanSmoke` | `ZENITH_HOST` | they run under `win-asan` and `linux-tsan` |
| `Zenith_Integration_DevToolsRender` | `ZENITH_WINDOWS` | WARP and lavapipe on the Windows host |

## Public API

### API rules

- Every DEVT public type has one contract block; no DEVT function is `noexcept` or `[[nodiscard]]`; every fallible call returns a `Zenith_Result<T, E>` with a DEVT error enumeration of § "Error enumerations".
- No DEVT public header includes an STL header, a Tracy header, a platform SDK header or another library's `Internal/` header ([Z2-N-CONTRACT-026], [Z2-N-CONTRACT-043]).
- No DEVT object is reachable through global state except the hook table the Player installs: the composition root passes the `Zenith_Profiler`, `Zenith_DevConsole`, `Zenith_DebugDraw`, `Zenith_CrashHandler` and `Zenith_RunTelemetry` to every consumer by reference or pointer ([Z2-N-CONTRACT-146]). Two exceptions exist, each one allowlisted variable: `Zenith_ProfileZoneValue` reaches the calling thread's slot in the thread-local variable, as the hook implementations do ([Z2-N-CONTRACT-144]); and the OS crash callbacks, which carry no user data, reach the crash handler through the process-wide anchor `g_pxCrashHandlerAnchor` (ruling Q88 (a), recorded by X273 as a dated extension of Z2-D-046; Crash.md "Crash handler anchor"). No consumer reads the anchor: only the handler's own trampolines and its `Create` and destructor touch it.
- Every development service (the console, the remote server, Tracy, the file writer, the frame dump) is present in every configuration except Shipping, where its entry symbols are absent (Console.md "Shipping absence of development services"); the profiler capture engine, the overlay, the debug-draw recorder, the counter registry and the crash handler exist in Shipping too, the overlay disabled by default (Z2-D-005).
- Every public class holds its state in private members declared in its own header by the landing ticket; DEVT uses no implementation class behind a pointer. Each Shipping-forbidden symbol is a declared member (or a declared ZenithRuntime internal class's member) defined in a development-only translation unit (Console.md "Shipping absence of development services").
- Behaviour that differs in Shipping is driven by data the composition root passes where it can be (`Zenith_BuildConfiguration` in `Zenith_RegisterDevToolsConfigKeys`, `Zenith_ResolveProfilerSettings`, `Zenith_GetPlayerProfilerDesc` and `Zenith_LogFileSinkDesc`), so the ordinary `Test` configurations exercise both branches. Behaviour that only a Shipping-flag build can show (a `Create` refusing under `if constexpr (Zenith_IsShippingBuild())`, Tracy never starting, crash capture with every Shipping flag) is a `ShippingTest` case run by the `win-shipping-test` preset ([Z2-N-ARCH-231]; Tests.md "ShippingTest cases"); the absence of a symbol from a Player's link closure is proven by the `Libgraph` Shipping symbol check, which alone reads that closure. Every DEVT test that exercises a development service follows the `ShippingTest` rule of Tests.md "ShippingTest cases": the two-branch form `if constexpr (Zenith_IsShippingBuild()) { <the refusal asserts> } else { <the test> }`, so the test itself is a discarded statement in `ShippingTest` (no odr-use of a development-only call, no unreachable code under `/W4 /WX`).

### Header index

The full declarations, with contract blocks, are in the split files named by § "ZenithProfiling public headers" and § "DEVT files in other libraries". The DEVT types other libraries name:

| Type | Header | Used by |
|---|---|---|
| `Zenith_Profiler` | `ZenithProfiling/Zenith_Profiler.h` | Player (construction), ZenithRuntime, ZenithJobs (lanes) |
| `Zenith_ProfilerGpuSink`, `Zenith_ProfilerGpuSubmission` | `ZenithProfiling/Zenith_Profiler.h` | ZenithFlux (the timestamp publisher and the frame lifetime) |
| `Zenith_ProfilerLaneID` | `ZenithProfiling/Zenith_ProfilerTypes.h` | ZenithJobs |
| `Zenith_ProfileScope`, `Zenith_ProfileZoneValue` | `ZenithProfiling/Zenith_ProfileZone.h` | every library declaring `ZenithProfiling` |
| `Zenith_DevConsole`, `Zenith_DevCommandDesc`, `Zenith_DevCommandToken` | `ZenithProfiling/Zenith_DevConsole.h` | ZenithJobs (executor), ZenithFlux (RPROD), ZenithECS, ZenithScene, ZenithAssets, ZenithScript, ZenithRuntime, modules |
| `Zenith_DevFileWriter` | `ZenithProfiling/Zenith_DevFileWriter.h` | command handlers |
| `Zenith_DebugDraw`, `Zenith_DebugDrawRecord`, `Zenith_DebugDrawRgba` | `ZenithProfiling/Zenith_DebugDraw.h` | ZenithScene, modules, the overlay, ZenithRuntime (the extractor reads the public record) |
| `Zenith_BudgetRunRecord`, `Zenith_EvidenceFileWriter` | `ZenithProfiling/Zenith_BudgetRunRecord.h` | ZenithRuntime, ZenithFlux (GEOM, ENV emitters), ZenithEnvironment |
| `Zenith_DevFrameDump`, `Zenith_DevFrameDumpSection` | `ZenithProfiling/Zenith_DevFrameDump.h` | ZenithRuntime (owner and sections), libraries above ZenithProfiling (sections) |
| `Zenith_RunTelemetry` | `ZenithProfiling/Zenith_Telemetry.h` | ZenithRuntime, ZenithFlux (RHI recovery) |
| `Flux_RenderDebugDraw` | `ZenithFlux/Flux_DebugDraw.h` | the render world, `Flux_AddDebugOverlay` |
| `Zenith_CrashHandler`, `Zenith_RunCrashHelperIfRequested` | `ZenithPlatform/Zenith_CrashHandler.h` | ZenithRuntime, ZenithScript (watchdog), the Player's, the editor's and ZenithPlatform's crash-test `main` |

### Configuration keys

`Zenith_DevToolsConfig.h` declares:

```cpp
// Zenith2/Engine/ZenithProfiling/Zenith_DevToolsConfig.h
#pragma once
#include "ZenithCore/Zenith_BuildInfo.h"
#include "ZenithCore/Zenith_ConfigStore.h"
#include "ZenithProfiling/Zenith_Profiler.h"

/// @contract functions DevToolsConfig
/// Ownership: Registers keys into the caller's schema; holds no state.
/// Thread affinity: MAIN_THREAD. Called by the composition root at startup step 6, in library-layer order.
/// Invalidation: None.
/// Units: As each key's name states (Ms milliseconds, Bytes bytes).
/// Errors: Zenith_ConfigError from Zenith_ConfigSchema::Register (CONFIG_ERROR_DUPLICATE_KEY, CONFIG_ERROR_SCHEMA_FROZEN,
///   CONFIG_ERROR_OUT_OF_MEMORY); the first failure is returned and the keys after it are not registered.
/// Version: since Z2 Stage 2. Key names are stable; a rename is a project-settings migration.
/// @endcontract
// eConfiguration: the composition root passes Zenith_GetBuildConfiguration(); tests pass any value, so the Shipping masks are
// unit-tested in an ordinary build.
Zenith_Result<void, Zenith_ConfigError> Zenith_RegisterDevToolsConfigKeys(Zenith_ConfigSchema& xSchema,
	Zenith_BuildPlatform ePlatform, Zenith_BuildConfiguration eConfiguration);
// The profiler settings the runtime applies at startup step 7 and on every change of a devtools.profiler.* or
// devtools.tracy.enabled key: each member from its key's resolved value, except that with BUILD_CONFIGURATION_SHIPPING
// m_bGpuTimings is the resolved value of devtools.profiler.enabled, the on-screen profiler toggle ([Z2-N-REND-313];
// Profiler.md "Runtime toggle and modes"). MAIN_THREAD; never fails (a key absent from the store resolves to its default).
Zenith_ProfilerSettings Zenith_ResolveProfilerSettings(const Zenith_ConfigStore& xStore,
	Zenith_BuildConfiguration eConfiguration);

inline constexpr const char* szDEVTOOLS_KEY_PROFILER_ENABLED = "devtools.profiler.enabled";
inline constexpr const char* szDEVTOOLS_KEY_PROFILER_VISIBLE = "devtools.profiler.visible";
inline constexpr const char* szDEVTOOLS_KEY_PROFILER_PANEL = "devtools.profiler.panel";
inline constexpr const char* szDEVTOOLS_KEY_PROFILER_HITCH_MS = "devtools.profiler.hitchThresholdMs";
inline constexpr const char* szDEVTOOLS_KEY_PROFILER_LOCKS = "devtools.profiler.captureLocks";
inline constexpr const char* szDEVTOOLS_KEY_PROFILER_GPU_TIMINGS = "devtools.profiler.gpuTimings";
inline constexpr const char* szDEVTOOLS_KEY_PROFILER_SCALE = "devtools.profiler.uiScale";
inline constexpr const char* szDEVTOOLS_KEY_TRACY_ENABLED = "devtools.tracy.enabled";
inline constexpr const char* szDEVTOOLS_KEY_CONSOLE_ENABLED = "devtools.console.enabled";
inline constexpr const char* szDEVTOOLS_KEY_CONSOLE_VERBOSE = "devtools.console.verbose";
inline constexpr const char* szDEVTOOLS_KEY_CONSOLE_EXEC = "devtools.console.exec";
inline constexpr const char* szDEVTOOLS_KEY_REMOTE_ENABLED = "devtools.remote.enabled";
inline constexpr const char* szDEVTOOLS_KEY_REMOTE_PORT = "devtools.remote.port";
inline constexpr const char* szDEVTOOLS_KEY_LOG_FILE = "devtools.log.file";
inline constexpr const char* szDEVTOOLS_KEY_TELEMETRY_ENABLED = "devtools.telemetry.enabled";
inline constexpr const char* szDEVTOOLS_KEY_TELEMETRY_COUNTERS = "devtools.telemetry.counters";
inline constexpr const char* szDEVTOOLS_KEY_CRASH_ENABLED = "crash.enabled";
inline constexpr const char* szDEVTOOLS_KEY_CRASH_MAX_REPORTS = "crash.maxReports";
inline constexpr const char* szDEVTOOLS_KEY_BUDGET_RUN_LOOP = "devtools.budgetRun.loop";
```

<!-- elaboration -->
| Key | Type | Default | Layers | Restart | Meaning |
|---|---|---|---|---|---|
| `devtools.profiler.enabled` | BOOL | true; Shipping false | every | no | zone, lock and GPU capture on (Profiler.md "Runtime toggle and modes"); counters are aggregated regardless |
| `devtools.profiler.visible` | ENUM {hidden, compact, full} | hidden | engine default, user, command line | no | the overlay's initial state (Overlay.md "Overlay state machine") |
| `devtools.profiler.panel` | STRING | `frame` | engine default, user, command line | no | the initial panel ID |
| `devtools.profiler.hitchThresholdMs` | FLOAT 1.0 … 1000.0 | 33.3 | every | no | the hitch marker threshold (the MP-HITCH boundary of `Zenith2/Docs/Budgets.md` → "Frame-rate definitions and measurement procedure") |
| `devtools.profiler.captureLocks` | BOOL | false | engine default, user, command line; non-Shipping | no | record lock and blocking-wait events and the per-site lock statistics (Profiler.md "Lock statistics"); the overlay's `[locks]` toggle sets it for the session through `set` |
| `devtools.profiler.gpuTimings` | BOOL | true; Shipping false (engine default alone) | every; Shipping engine default | no | ingest GPU pass timings in either profiler mode (Profiler.md "GPU timing ingestion"); the runner never changes it, so budget runs measure MP-GPUPASS rows; in Shipping `Zenith_ResolveProfilerSettings` ignores it and GPU timings follow `devtools.profiler.enabled` |
| `devtools.profiler.uiScale` | INT 0 … 4 | 0 (automatic from the display scale) | every | no | the overlay's integer pixel scale |
| `devtools.tracy.enabled` | BOOL | false | engine default, command line; non-Shipping and non-sanitizer builds alone | yes | start the Tracy client (Profiler.md "Tracy forwarding") |
| `devtools.console.enabled` | BOOL | true | engine default, command line; non-Shipping | no | accept console input |
| `devtools.console.verbose` | BOOL | false | engine default, command line; non-Shipping | no | the on-screen console shows `TRACE` and `DEBUG` lines (Console.md "On-screen console") |
| `devtools.console.exec` | STRING | empty | command line; non-Shipping | yes | a `;`-separated console line run once at the first `ExecuteQueued` (Console.md "On-screen console") |
| `devtools.remote.enabled` | BOOL | false | engine default, command line; non-Shipping | yes | start the remote server (Console.md "Remote diagnostics server") |
| `devtools.remote.port` | INT 1024 … 65535 | 27852 | engine default, command line; non-Shipping | yes | the listener port |
| `devtools.log.file` | BOOL | true; Shipping false | engine default, user, command line | yes | the JSON Lines file sink (Console.md "Log file sink") |
| `devtools.telemetry.enabled` | BOOL | true; Shipping false (and refused) | engine default, command line | yes | run and frame telemetry (Evidence.md "Run telemetry") |
| `devtools.telemetry.counters` | STRING | the list of Evidence.md "Frame telemetry" | engine default, game, command line | yes | the counters written per frame |
| `crash.enabled` | BOOL | true; false in sanitizer builds | engine default, command line | yes | install the crash handler (Crash.md "Sanitizer builds") |
| `crash.maxReports` | INT 1 … 256 | 16 | engine default, command line | yes | reports kept on the user mount |
| `devtools.budgetRun.loop` | BOOL | false | command line | yes | loop the scripted input until the run's minimums are met (Evidence.md "Measured interval and run length") |

A key marked non-Shipping is registered in every build so the schema is identical, and when `eConfiguration` is `BUILD_CONFIGURATION_SHIPPING` its layer mask admits the engine-default layer alone and the default is the off value.

## Data structures and formats

| Structure or format | Size or version | Specified in |
|---|---|---|
| `Zenith_ProfilerEvent` (per-thread ring record) | 16 bytes | Profiler.md "Event records" |
| `Zenith_ProfilerZone` (assembled zone) | 32 bytes | Profiler.md "Frame assembly" |
| `Zenith_ProfilerFrameSummary` | 64 bytes | Profiler.md "Frame history and detailed frames" |
| `Zenith_ProfilerLockStat` (frame lock statistics entry) | 48 bytes | Profiler.md "Lock statistics" |
| per-thread lock state (two banks of 64 entries, 16 held locks) | 8,896 bytes per thread state | Profiler.md "Lock statistics" |
| per-thread ring (CORE's `Zenith_SpscRing<Zenith_ProfilerEvent>`) | 16,384 events, 256 KiB (Android 8,192, 128 KiB) | Profiler.md "Per-thread event rings" |
| name tables | 16,384 sites, 4,096 counters, 1 MiB of name text | Profiler.md "Name tables" |
| detailed frame | 32,768 zones and 4,096 GPU zones, 1,179,648 bytes | Profiler.md "Frame history and detailed frames" |
| debug-draw main ring, other rings, staging | 16,384, 1,024 and 32,768 records of 48 bytes | DebugDraw.md "Recorder rings and drain" |
| capture segment `.zprof` | format version 1, 96-byte header, typed chunks | Profiler.md "Capture format" |
| `Zenith_DebugDrawRecord` (recorder ring record) | 48 bytes | DebugDraw.md "Recorder records" |
| `Flux_DebugLineSegment`, `Flux_DebugQuad` (GPU) | 32 bytes each | DebugDraw.md "Flux_DebugDraw header" |
| debug font | 95 glyphs of 8 × 8, atlas `R8_UNORM` 128 × 48 | DebugDraw.md "Debug font" |
| console line | at most 1,024 bytes, at most 32 tokens | Console.md "Console grammar" |
| remote protocol message | 8-byte header, at most 1 MiB payload | Console.md "Remote protocol" |
| log record line `zenith2.log-record` | version 1, JSON Lines | Console.md "Log record schema" |
| DEVT frame dump `zenith2.devtools-dump` | version 1, JSON | Console.md "DEVT frame dump" |
| crash report `zenith2.crash-report` | version 1, JSON | Crash.md "Crash report schema" |
| hang report text | version 1 | Crash.md "Hang reports" |
| evidence record `zenith2.evidence.v1` | Budgets' block, exactly | Evidence.md "Evidence record writer" |
| budget-run rows file `zenith2.budget-run-rows` | version 1, JSON | Evidence.md "Budget-run inputs" |
| fingerprint file `zenith2.evidence-fingerprint` | version 1, JSON | Evidence.md "Fingerprint algorithm" |
| row sources `zenith2.row-sources` | version 1, JSON | Evidence.md "Row source bindings" |
| run telemetry `zenith2.run-telemetry` | version 1, JSON; frames CSV | Evidence.md "Run telemetry", "Frame telemetry" |
| dashboard `zenith2.dashboard` | version 1, JSON plus one HTML file | Evidence.md "Regression dashboards" |

Every multi-byte binary field is little-endian; every JSON file is UTF-8 without a byte-order mark, written with CORE's `Zenith_JsonWriter` (`JSON_WRITE_STYLE_PRETTY` for files a person reads, `JSON_WRITE_STYLE_COMPACT` for JSON Lines); every reader of a DEVT format refuses a `schemaVersion` or format version above its own with a recoverable error naming both versions, and skips unknown JSON members and unknown binary chunk kinds by length.

## Algorithms and state machines

| Algorithm or state machine | Specified in |
|---|---|
| zone begin, end and value hooks; counter hook; lock and wait hooks | Profiler.md "Hook implementation" |
| thread-slot claim and generation check | Profiler.md "Thread slots and the thread-local variable" |
| SPSC ring use (CORE's primitive) | Profiler.md "Per-thread event rings" |
| zone begin and end balance across mode and Tracy changes | Profiler.md "Hook implementation" |
| lock-site statistics: per-thread banks, the writer-count handover and the end-of-frame merge | Profiler.md "Lock statistics" |
| lane binding by thread name | Profiler.md "Thread lanes" |
| fatal recognition and the crash helper's validation | Crash.md "Fatal failures", "Crash helper security contract" |
| the seqlock and log-mirror protocols | Crash.md "Annotations and breadcrumbs", "Crash log mirror" |
| name-table lookup and lock-free insert | Profiler.md "Name tables" |
| counter snapshot per frame by kind | Profiler.md "Counter registry" |
| frame assembly with open-zone carry and truncation repair | Profiler.md "Frame assembly" |
| GPU submission and lane alignment | Profiler.md "GPU timing ingestion" |
| capture segment encoding and flush | Profiler.md "Capture writer and the flush thread" |
| overlay state machine | Overlay.md "Overlay state machine" |
| expression compilation and evaluation | Overlay.md "Expression language" |
| console tokenisation, parsing and execution | Console.md "Console grammar", "Command execution" |
| debug-draw shape expansion, drain and batching | DebugDraw.md "Shape expansion", "Recorder rings and drain", "Debug-draw extractor" |
| remote server connection state machine | Console.md "Remote diagnostics server" |
| crash capture per platform and next-launch finalization | Crash.md "Windows capture", "Android capture", "Next-launch finalization" |
| fingerprint computation | Evidence.md "Fingerprint algorithm" |
| nearest-rank statistics and the evidence record | Evidence.md "Statistics", "Evidence record writer" |
| budget-run measured interval | Evidence.md "Measured interval and run length" |

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| hook functions (`m_pfnOnZoneBegin`, …) | `ANY_THREAD`; they take no mutex, condition variable or semaphore (profiled or not) and allocate nothing, so they run under any held lock, in the OOM handler and inside the log router's sink lock |
| `Zenith_Profiler` | `ANY_THREAD_EXCEPT(Create, ApplySettings, RegisterThreadLane, EndFrame, StartCapture, StopCapture, SetOutput, Get*)`: those run on the main thread; `BindCurrentThread` on the calling thread; `SubmitGpuTimings` (the `Zenith_ProfilerGpuSink` member) on any thread, one caller at a time |
| `Zenith_ProfileScope`, `Zenith_ProfileZoneValue` | `ANY_THREAD` (the calling thread's own slot) |
| `Zenith_ProfilerOverlay` | `MAIN_THREAD` |
| `Zenith_DevConsole` | `MAIN_THREAD`; `QueueLine` is `ANY_THREAD` |
| `Zenith_DebugDraw` | `ANY_THREAD` for the drawing calls and `GetFreeRecords`; `MAIN_THREAD` for `Create` and `SetEnabled`; `DrainInto` on the drain node `DevTools.DrainDebugDraws@W0`; `GetBatches` on the extractor nodes after it |
| `Zenith_LogFileSink` | as `Zenith_LogSink`: `Write` on logging threads, `Output` and `Flush` on the drain thread |
| `Zenith_BudgetRunRecord`, `Zenith_RunTelemetry` | `MAIN_THREAD` except `AddSample`, `FindRow`, `GetRows` and `RecordEvent`, which are `ANY_THREAD` (CORE `Zenith_BoundedQueue`s and immutable rows) |
| `Zenith_CrashHandler` | `MAIN_THREAD` for `Create`, `Finalize` and destruction; `ANY_THREAD` for `SetAnnotation`, `AddBreadcrumb`, `RegisterCurrentThread`, `UnregisterCurrentThread`, `WriteHangReport`, `AddAttachment` and `GetResidentBytes` (the contract block of Crash.md "Zenith_CrashHandler header") |
| the remote server (ZenithRuntime) | `MAIN_THREAD` (polled in frame-loop step 3) |

### Frame placement

| Work | Where | Declared access |
|---|---|---|
| hooks | wherever the instrumented code runs | none (no frame-graph resource) |
| `Zenith_DevTools::EndFrame` (profiler `EndFrame`, overlay build, console queue, remote poll, telemetry, budget-run sampling) | frame-loop step 3, main thread, between iterations, when no job runs ([Z2-N-ARCH-870]; row DV4 adds the console queue to step 3's text) | none |
| `DevTools.DrainDebugDraws` (group 0, addressed `DevTools.DrainDebugDraws@W0`), then `DevTools.ExtractDebugDraws@W<tag>` per world | `FRAME_SUBGRAPH_EPILOGUE` stage 12, `JOB_AFFINITY_ANY`; each extractor declares the explicit orderings `After("DevTools.DrainDebugDraws@W0", true)` and `Before("Render.Seal@W0", true)`, group-0 nodes ordering against world nodes through explicit orderings alone (`Zenith2/Docs/Architecture/CPUFrameGraph.md` group rule 7; DebugDraw.md "Debug-draw extractor"); neither is registered in headless runs | the drain node none; each extractor its world's render-world writer partition, and no resource shared with the drain node |
| `Flux_AddDebugOverlay` | the common epilogue of every main and editor view inside `Flux.BuildGraph` | the pass's parameter struct and targets (DebugDraw.md "DebugOverlay pass") |
| `Zenith.ProfFlush` | its own thread | capture segments (the JSON Lines file sink writes its chunks from the log router's drain thread, Console.md "Log file sink") |
| crash capture | the crashing thread, then the helper process (Windows) or the OS (Android) | none |

### Lock-order entries

DEVT uses the existing row 940 `uLOCK_LEVEL_PROFILER_BUFFERS` (ZenithProfiling, short-hold, any thread; `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract") for the capture-segment queue between `EndFrame` and the flush thread, and no other lock. Every other DEVT structure shared between threads is lock-free with atomic storage (Profiler.md "Per-thread event rings", "Name tables"; DebugDraw.md "Recorder rings and drain"; Crash.md "Annotations and breadcrumbs" (the resolve handshake `seq_cst`), "Crash log mirror" (each slot single-writer through its header's `WRITING` claim, a copied line kept only when its headers are unchanged); the multi-producer queues are CORE's `Zenith_BoundedQueue`), because the hooks run under arbitrary held locks, including leaf levels above 940, which no lock they took could order against. The queue's mutex is a `Zenith_MutexNoProfiling`: it keeps its row 940 and the lock-order checker still orders it, but it never calls a hook, so no hook can fire from the profiler's own lock; `m_bInProfiler` remains as defence in depth (Profiler.md "Lock and wait events").

### Cancellation and shutdown

| Object | Created | Destroyed | Threads |
|---|---|---|---|
| `Zenith_Profiler` | Player startup step 2 (before the hooks are installed) | after the hooks are uninstalled, in shutdown row 11 | `Zenith.ProfFlush`, joined by its destructor (row 11 "profiler flush thread") |
| `Zenith_DevTools` (ZenithRuntime aggregate: console, debug-draw recorder, overlay, file writer, frame dump, telemetry, budget-run driver, remote server, the memory consumer) | startup step 7, after the log sinks | shutdown step 10b, with the log sinks and the crash reporter; its outputs detached earlier, at MOD step 6 | none |
| `Zenith_LogFileSink` | step 7 | detached (`Detach`: removed from the router, last chunk written) at MOD step 6; destroyed at step 10b | none |
| `Zenith_CrashHandler` | step 7 | step 10b, last of the step-7 objects (crash capture stays armed through shutdown) | the Windows helper process, terminated by the destructor |

Shutdown, in MOD's order (`Zenith2/Docs/Design/MOD.md` → "Shutdown sequence"), which every DEVT writer obeys so nothing writes through `Zenith_AsyncIO` after Contracts step 8 destroys it:

1. MOD step 1 (Contracts step 4, outstanding operations cancelled and drained): `Zenith_DevTools::BeginShutdown` stops the remote server, stops any capture (writing its last segment synchronously into the output), requests cancellation of every pending development-file write (`Zenith_DevFileWriterUserMount::CancelPending`, which covers `ecs.dump`'s `WriteUserPath` writes too) and of a pending `exec` read, flushes the telemetry and writes the budget-run record and the last CSV lines (`Zenith_BudgetRunRecord::Write`, through PLAT host files, which `Zenith_AsyncIO` does not carry).
2. MOD step 6, after `Zenith_Config::FlushUserLayer()` and `Zenith_InputSystem::FlushUserBindings()` and before `Zenith_AsyncIO` is destroyed (Contracts step 8): `Zenith_DevTools::DetachOutputs()` runs, in order, `Zenith_LogFileSink::Detach(xRouter)` (removed from the router, which waits until the drain thread is outside it; its last chunk handed to the output), `Zenith_RunTelemetry::Detach()` (a last `Flush`, then no output), `Zenith_Profiler::SetOutput(nullptr)` (waits until `Zenith.ProfFlush` is idle), and finally `Zenith_DevToolsOutput::WaitIdle()`, `Zenith_DevFileWriterUserMount::WaitIdle()` and the exec slot's wait (its read completes, cancelled or not, and its buffer is freed), which wait (at most 2 s each, a timeout logged under `devtools.telemetry`) for every write submitted above. After it no DEVT object holds an output, so a record logged by MOD's step 7 (`Runtime: shutdown complete`) reaches the console, ring and crash-mirror sinks and never the file, and a late telemetry event is dropped and counted.
3. Contracts step 8 destroys `Zenith_AsyncIO`; MOD step 7 then destroys the step-7 objects (crash handler last).

X245 (e) records step 2 in Contracts' shutdown table; `Test_DevTools_DetachOutputsBeforeAsyncIO` and `Test_LogFileSink_NoWriteAfterDetach` assert it.

### Error enumerations

```cpp
// Zenith2/Engine/ZenithProfiling/Zenith_ProfilerTypes.h (excerpt; the full header is Profiler.md "Zenith_ProfilerTypes header")
enum Zenith_ProfilerError : uint8
{
	PROFILER_ERROR_INVALID_DESC = 1,      // a desc field outside the range its comment gives
	PROFILER_ERROR_OUT_OF_MEMORY,         // a Create allocation failed
	PROFILER_ERROR_THREAD_CREATE,         // Zenith.ProfFlush could not start
	PROFILER_ERROR_LANES_FULL,            // RegisterThreadLane beyond uPROFILER_MAX_LANES
	PROFILER_ERROR_CAPTURE_ACTIVE,        // StartCapture while a capture runs
	PROFILER_ERROR_NO_OUTPUT,             // StartCapture with no Zenith_ProfilerOutput set
	PROFILER_ERROR_GPU_QUEUE_FULL,        // SubmitGpuTimings found the submission queue full (the frame is dropped)
	PROFILER_ERROR_CAPTURE_CORRUPT,       // the capture reader found a malformed segment
	PROFILER_ERROR_CAPTURE_VERSION,       // the capture reader found a format version above its own
	PROFILER_ERROR_EXPRESSION,            // an expression failed to compile (Overlay.md "Expression language")
	PROFILER_ERROR_NOT_IN_SHIPPING,       // Zenith_DevFrameDump::Create or Zenith_RunTelemetry::Create in a Shipping build
};

// Zenith2/Engine/ZenithProfiling/Zenith_DevConsole.h (excerpt)
enum Zenith_DevConsoleError : uint8
{
	DEV_CONSOLE_ERROR_INVALID_NAME = 1,   // a command name outside the grammar of Console.md "Command registry"
	DEV_CONSOLE_ERROR_DUPLICATE_NAME,
	DEV_CONSOLE_ERROR_REGISTRY_FULL,      // beyond uDEV_CONSOLE_MAX_COMMANDS
	DEV_CONSOLE_ERROR_NOT_IN_SHIPPING,    // any call in a Shipping build
	DEV_CONSOLE_ERROR_LINE_TOO_LONG,
	DEV_CONSOLE_ERROR_PARSE,              // an unterminated quote or a bad escape
	DEV_CONSOLE_ERROR_UNKNOWN_COMMAND,
	DEV_CONSOLE_ERROR_QUEUE_FULL,         // QueueLine beyond the 64-line queue
	DEV_CONSOLE_ERROR_OUT_OF_MEMORY,
};

// Zenith2/Engine/ZenithProfiling/Zenith_DevFileWriter.h (excerpt)
enum Zenith_DevFileError : uint8
{
	DEV_FILE_ERROR_BAD_NAME = 1,          // a relative name failing Console.md "Zenith_DevFileWriter header" rules
	DEV_FILE_ERROR_TOO_LARGE,             // above uDEV_FILE_MAX_BYTES (64 MiB)
	DEV_FILE_ERROR_BUSY,                  // eight writes already pending
	DEV_FILE_ERROR_IO,                    // the submission was refused by the I/O service
	DEV_FILE_ERROR_NOT_IN_SHIPPING,
};

// Zenith2/Engine/ZenithProfiling/Zenith_BudgetRunRecord.h (excerpt)
enum Zenith_BudgetRunErrorCode : uint8
{
	BUDGET_RUN_ERROR_ROWS_MISSING = 1,    // the rows file is absent from the evidence directory
	BUDGET_RUN_ERROR_ROWS_PARSE,          // the rows file fails its schema
	BUDGET_RUN_ERROR_ROWS_VERSION,        // schemaVersion above 1
	BUDGET_RUN_ERROR_FINGERPRINT_MISSING,
	BUDGET_RUN_ERROR_FINGERPRINT_PARSE,
	BUDGET_RUN_ERROR_WORKLOAD_MISMATCH,   // the rows file names another workload or version than the command line
	BUDGET_RUN_ERROR_UNKNOWN_SOURCE,      // a row's source kind is not one of Evidence.md "Row source bindings"
	BUDGET_RUN_ERROR_TOO_MANY_ROWS,       // above uBUDGET_RUN_MAX_ROWS (128)
	BUDGET_RUN_ERROR_OUT_OF_MEMORY,
	BUDGET_RUN_ERROR_WRITE,               // the evidence record or the capture could not be created
};

// Zenith2/Engine/ZenithPlatform/Zenith_CrashHandler.h (excerpt)
enum Zenith_CrashHandlerErrorCode : uint8
{
	CRASH_HANDLER_ERROR_ALREADY_INSTALLED = 1,   // a second Create in one process (the anchor is held, Crash.md "Crash handler anchor")
	CRASH_HANDLER_ERROR_DISABLED,                // crash.enabled is false (not a failure: the composition root continues)
	CRASH_HANDLER_ERROR_DIRECTORY,               // user:/crash/ could not be created
	CRASH_HANDLER_ERROR_HELPER_START,            // Windows: the helper process did not start or did not report ready in 5 s
	CRASH_HANDLER_ERROR_ATTACHMENTS_FULL,        // beyond uCRASH_MAX_ATTACHMENTS
	CRASH_HANDLER_ERROR_ANNOTATIONS_FULL,        // beyond uCRASH_MAX_ANNOTATIONS
	CRASH_HANDLER_ERROR_OUT_OF_MEMORY,
	CRASH_HANDLER_ERROR_UNSUPPORTED,             // a call the platform's realisation does not provide
};
```

Each enumeration has its `Zenith_ToString` and `Zenith_GetErrorCategory` overloads in the same header ([Z2-N-CONTRACT-466]; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Results and the error taxonomy"). `Zenith_BudgetRunError` is the record `{ Zenith_BudgetRunErrorCode m_eCode; uint16 m_uRowIndex; uint32 m_uLine; }` (8 bytes, [Z2-N-CONTRACT-480]).

### Luau errors and asserts

DEVT exposes no Luau binding, so it raises no Luau error and holds no block of `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges" (a block is taken only by an owner of catalogued binding errors, [Z2-N-SCRIPT-926]); it uses no async-source value of SCRIPT's registry either. Console input, remote messages, capture files, rows files and fingerprint files are untrusted: every parser returns a DEVT error and never asserts ([Z2-N-CONTRACT-472]). Programmer errors (an end hook with no open zone on a thread whose slot is owned, a `RegisterCommand` off the main thread, a drain from two threads at once) are `ZENITH_ASSERT`; a corrupted internal invariant that would write out of bounds (a ring index beyond capacity) is `ZENITH_CHECK`.

### Determinism

Nothing DEVT records, draws or computes is simulation-visible: no component value, script-visible value, replayed quantity or schedule depends on whether the profiler, the overlay, the console or the crash handler is enabled. DEVT's own commands change configuration keys only through `Console.md "Configuration variables"`, which refuses simulation-affecting keys, so a replay is never perturbed by a typed `set` (Z2-D-019). Every registrant's command obeys the same rule through `Zenith_DevCommandDesc`'s contract (Console.md "Zenith_DevConsole header"): a handler never writes simulation-visible state directly; a command that must change a simulation submits an input command through that world's recorded command stream, which a replay reproduces, or refuses itself while the world records or replays. Review enforces the rule for every registrant (Z2-R-DEVT-141).

## Diagnostics and profiling

### Counters

DEVT's own counters, published by `Zenith_Profiler::EndFrame` through its own counter path:

| Counter | Kind | Meaning |
|---|---|---|
| `profiler.events_dropped_delta` | DELTA | ring records dropped this frame (all threads) |
| `profiler.rings.full`, `profiler.rings.dropped`, `profiler.rings.high_water` | CUMULATIVE | the [Z2-N-CONTRACT-345] triple of the per-thread rings (high water is the largest ring occupancy in records) |
| `profiler.sites`, `profiler.counters`, `profiler.lanes` | GAUGE | name-table and lane occupancy |
| `profiler.sites_full_delta`, `profiler.counters_full_delta` | DELTA | names refused because a table was full |
| `profiler.lanes_refused`, `profiler.counter_kind_conflicts` | CUMULATIVE | lane registrations refused (lane table full); counter calls with a second kind for one name |
| `profiler.assemble_us` | GAUGE | `EndFrame` assembly time (the cost row's measurement) |
| `devtools.memory_bytes` | GAUGE | DEVT's resident bytes, the source of Z2-B-DEVT-001 (Profiler.md "Memory layout and accounting") |
| `profiler.zones` | GAUGE | zones assembled in the frame |
| `profiler.lock_sites` | GAUGE | lock sites in the frame's lock statistics (Profiler.md "Lock statistics") |
| `profiler.lock_stats_dropped_delta`, `profiler.lock_banks_deferred_delta` | DELTA | lock-statistic updates refused (a full per-thread table, a full held-lock stack, a release with no acquire, a full frame table); per-thread lock banks whose merge waited a frame |
| `profiler.gpu_frames_dropped_delta` | DELTA | GPU submissions refused |
| `profiler.memory_bytes` | GAUGE | the profiler's resident bytes |
| `profiler.capture.segments_delta`, `profiler.capture.bytes_delta` | DELTA | capture output |
| `profiler.capture_queue.full`, `.dropped`, `.high_water` | CUMULATIVE | the segment queue's triple |
| `devtools.debugdraw.lines`, `devtools.debugdraw.quads` | GAUGE | records drained in the frame |
| `devtools.debugdraw.dropped_delta` | DELTA | records dropped (ring full, persistent table full, frame cap, slot full, non-finite) |
| `devtools.debugdraw.upload_refused_delta` | DELTA | frames whose `DebugOverlay` upload the ring refused |
| `devtools.debugdraw.main_ring.full`, `.dropped`, `.high_water` | CUMULATIVE | the main ring's triple (the overlay and the console) |
| `devtools.debugdraw.rings.full`, `.dropped`, `.high_water` | CUMULATIVE | the other recorder rings' triple, summed |
| `devtools.debugdraw.staging_high_water` | GAUGE | the largest staging fill |
| `devtools.evidence.samples_dropped_delta` | DELTA | emitter samples refused by a full queue |
| `telemetry.events.full`, `.dropped`, `.high_water` | CUMULATIVE | the telemetry event queue's triple |
| `devtools.console.commands_delta`, `devtools.console.errors_delta` | DELTA | executed lines and failed lines |
| `devtools.console.queue.full`, `.dropped`, `.high_water` | CUMULATIVE | the 64-line queue |
| `devtools.remote.connections` | GAUGE | authenticated connections |
| `devtools.remote.bytes_sent_delta`, `devtools.remote.bytes_received_delta` | DELTA | traffic |
| `devtools.files.pending` | GAUGE | development-file writes in flight |
| `devtools.log_file.chunks_delta`, `devtools.log_file.dropped_delta` | DELTA | file-sink chunks written and lines dropped |
| `telemetry.frames_delta` | DELTA | frame telemetry rows written |
| `crash.reports_pending` | GAUGE | finalized reports not yet fetched |
| `crash.annotations_dropped` | CUMULATIVE | annotation writes dropped (a full table, or the resolve's bounded retries exhausted; Crash.md "Annotations and breadcrumbs") |
| `crash.log_mirror.dropped` | CUMULATIVE | log lines the crash log mirror abandoned (a writer overtaken, or a slot held by another writer; Crash.md "Crash log mirror") |
| `crash.altstacks_refused` | CUMULATIVE | Android thread registrations beyond the 32 alternate signal stacks (Crash.md "Thread registration") |

### Log categories

`devtools.profiler`, `devtools.console`, `devtools.debugdraw`, `devtools.remote` (beside PLAT's `devtools` category for the listener), `devtools.evidence`, `devtools.telemetry`, and `crash`, each declared with `ZENITH_LOG_CATEGORY` and compiled minimum `LOG_LEVEL_TRACE` (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Structured logging"). Fixed messages are quoted where each split file specifies the event; every warning about a capacity (a full ring, a full table, a dropped GPU frame) is logged once per run per capacity.

### Profiling zones

`DevTools.EndFrame` (main thread, frame-loop step 3) with children `Profiler.Assemble`, `Profiler.Overlay`, `DevTools.Console`, `DevTools.Remote`, `DevTools.Telemetry`, `DevTools.BudgetRun`; `DevTools.DrainDebugDraws` and `DevTools.ExtractDebugDraws@W<tag>` (the epilogue nodes); `Profiler.FlushSegment` (the flush thread); `Flux.DebugOverlay` (inside `Flux.BuildGraph`). The profiler never records zones of its own hooks. Its own zones are emitted through `Zenith_DispatchZoneBegin` and `Zenith_DispatchZoneEnd` like everyone's.

### On-screen rows debug views and dumps

DEVT's own panel `devtools` shows the counters above. The DEVT frame dump (Console.md "DEVT frame dump") is the one DEVT dump; the capture (Profiler.md "Capture format") is the deep record; DEVT contributes no entry to the render graph's frame dump.

### Telemetry

DEVT owns the run telemetry record and the frame telemetry stream (Evidence.md "Run telemetry", "Frame telemetry"); its own entries are the profiler's dropped events and the overlay state per frame.

## Authoring and cooking

DEVT has no authoring document, importer or cook step and no editor surface of its own beyond the overlay drawn into editor views (Overlay.md "Editor overlay"). Its committed data are the panel catalogue (C++ constexpr tables), the debug font glyph table (a C++ constexpr table), `Zenith2/Benchmarks/Workloads/RowSources.json` (edited by each area's budget-emission ticket), `Zenith2/.clang-tidy`, the WSL setup data files and the test fixtures; none is cooked, and none is migrated: a change to `RowSources.json`'s shape is a `schemaVersion` bump with the runner and the record reader changed in the same diff. Configuration keys are registered at startup step 6 and follow the project-settings migration rules of `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind".

## Failure handling

| Failure | Defined behaviour |
|---|---|
| `Zenith_Profiler::Create` fails at Player step 2 | the Player writes `Zenith2: profiler creation failed: <error>` with `Zenith_WriteDiagnosticLine`, installs no hook, destroys the log router, uninstalls the root heap and returns exit code 4; there is no stand-in object (Profiler.md "Creation and shutdown") |
| a per-thread ring full | the event is dropped (`QUEUE_FULL_POLICY_DROP_NEWEST`), `profiler.events_dropped_delta` counts it, and frame assembly repairs the pairing (Profiler.md "Frame assembly") |
| more threads than thread states | events of the extra threads are dropped and counted; one warning names the thread ID |
| a name table full | the name maps to site 0 `<names full>` or counter 0 `<counters full>`, counted |
| a GPU submission queue full | the GPU frame is dropped, counted; the GPU view shows a gap |
| the capture output refuses a segment | the capture stops, `devtools.profiler: capture stopped: output refused segment <n>` |
| a console line unparsable or a command unknown | the reply carries the error text; nothing runs |
| a command handler fails | the handler writes its error through `Zenith_DevCommandOutput::Error`; the console counts it |
| a development file write fails | `DEV_FILE_ERROR_*` to the caller; `devtools.console: write '<name>' failed: <error>` |
| a debug-draw ring full or the upload cap reached | records dropped, counted, one warning per run; the overlay checks the main ring's free records before each view and draws merged runs instead of overflowing (DebugDraw.md "Recorder rings and drain") |
| shutdown reaches `Zenith_AsyncIO`'s destruction | every DEVT output was detached at MOD step 6; log lines after it skip the file sink, telemetry events after it are dropped (§ "Cancellation and shutdown") |
| the crash helper is started by another program, or its section is forged | validation fails, exit 3, nothing written (Crash.md "Crash helper security contract") |
| the remote listener cannot bind | the server is absent for the run, one error line; the game continues |
| a remote peer sends a malformed message | the connection is closed and logged; the server continues |
| crash handler creation fails | startup step 7 fails with exit code 4 when `crash.enabled` is true (MOD's step-7 row "a log sink or the crash reporter could not be created"); `CRASH_HANDLER_ERROR_DISABLED` is not a failure |
| a crash during crash capture | the second fault terminates without a report (Crash.md "Windows capture", "Android capture") |
| a corrupt report on the user mount at finalization | it is renamed `.corrupt` and skipped |
| a budget-run input missing or malformed | startup step 7 fails with exit code 2 and the `BUDGET_RUN_ERROR_*` named (an input error, like every budget-run switch error; row DV12 adds the row to MOD's step-failure table, X245 (a)) |
| a budget run cannot write its record | exit code 1 after the run, the error logged |
| memory pressure | the profiler sheds nothing (its subsystem's row is "none / none", `Zenith2/Docs/Architecture.md` → "Memory governor") |
| allocation failure inside a hook | impossible by construction: hooks never allocate |

## Test strategy

`Zenith2/Docs/Design/DEVT/Tests.md` names every test, its file, the case it asserts and the gate step that runs it. In summary:

| Group | Location | Gate step |
|---|---|---|
| ZenithProfiling unit tests, including the ring and name-table oracle and property tests | `Engine/ZenithProfiling/Tests/` | `Test` (win-debug, win-release), `Test -Label memory`, `Sanitizers -Preset win-asan`, `Sanitizers -Preset linux-tsan` |
| death tests | `Engine/ZenithProfiling/Tests/Zenith_Profiler_DefectTests.cpp`, `Zenith_ProfilerNameTable_Tests.cpp` | `Test`: `z2-death-assert` (Debug, Sanitize, Hwasan) for the cases that die by `ZENITH_ASSERT`, `z2-death-check` (every configuration) for the others (Tests.md "Death cases") |
| `ShippingTest` cases | every DEVT test target the `win` tree supports | `Test` with `-Preset win-shipping-test` once PROC adds it (row DV17 (f)); kept out of ticket Definitions of Done until then (Tests.md "ShippingTest cases") |
| fuzz targets | `Engine/ZenithProfiling/Tests/Fuzz/` | `Sanitizers -Preset win-asan` (60-second smoke); stage acceptance (campaign) |
| crash handler tests | `Engine/ZenithPlatform/Tests/` | `Test` and `Sanitizers` for the platform-neutral cases (the anchor protocol, the seqlocks, the log mirror, the signal-safe escaper, the decode and finalization); the crash cases are ordinary cases that crash a child process (the skipped case `Test_CrashChild_RunScenario`, launched through `Zenith_TestProcess::LaunchSelf`, [Z2-N-ARCH-162]) and inspect the files carrying the child's PID, in every `win` configuration including `ShippingTest`, with an in-process `CRASH_HANDLER_ERROR_DISABLED` branch in `win-asan` (Tests.md "Crash tests", Crash.md "Sanitizer builds") |
| debug overlay pass tests | `Engine/ZenithFlux/Tests/Debug/` | `Test` with WARP and lavapipe |
| CPU integration | `Tests/DevTools/` | `Test`, `Sanitizers -Preset linux-tsan` |
| render integration | `Tests/DevToolsRender/` | `Test` (WARP, lavapipe) |
| TSan smoke | `Tests/TsanSmoke/` | `Sanitizers -Preset linux-tsan`; the Stage 1 acceptance run |
| PowerShell assert scripts | `Benchmarks/Evidence/Tests/`, `Tools/DevTool/Scripts/Tests/` | the coverage gate step (PROC wiring, row DV17); `Setup-Zenith2.ps1 -Check` |
| device runs | the S24 overlay, remote and crash runs | `needs-human` device steps of the Stage 3 acceptance |

No DEVT test doubles a concrete engine class (FINAL-DOUBLES; `Zenith_Profiler` is `final`, and `Zenith_DevConsole` and `Zenith_DebugDraw` have no base and no virtual member): tests construct the real `Zenith_Profiler`, `Zenith_DevConsole` and `Zenith_DebugDraw` (all three run without a device or a frame loop) and install hooks for the test's duration through `Zenith_ScopedProfilerInstall`; the test helpers implement DEVT's own interfaces only (Tests.md "Test targets"), and other areas' tests of DEVT consumers construct the real console and use `Zenith_TestProfilerGpuSink` (rows DV5, DV24, DV25). No ticket asks a worker to run a build or a test or to record a count or digest (WORKER-EVIDENCE); every proof is a registered CTest, an integration suite or an assert script that a gate step runs, every expected value a test compares against is either computed at test time or a hand-written committed golden (GOLDEN-FILES: the trace-event golden, the dump fixture), and device evidence is recorded only by `needs-human` tickets. Every DEVT test name is unique programme-wide (TEST-NAME-UNIQUE); the two DEVT-owned tests other areas' designs name (`Test_FrameGraph_ZonesForEveryNode`, `Test_Profiler_ContentionRowsFromCounters`) are owned here.

## Budget rows

### Rows DEVT owns

| Row | Name | Procedure | Measured on | Proposed value (adopted by the Stage 1 per-system budget ticket for DEVT) |
|---|---|---|---|---|
| Z2-B-DEVT-001 | Profiler and telemetry buffers | MP-SUBMEM | the GAUGE `devtools.memory_bytes`: the resident bytes of DEVT's two `MEMORY_SUBSYSTEM_OTHER` consumers (the profiler; `Zenith_DevToolsMemoryConsumer` for the recorder, console, overlay, file sink, telemetry and crash handler), the attribution rule for a component inside a shared subsystem (Evidence.md "Row source bindings"); maximum over the run. Procedure note: buffers allocated only on demand are excluded and absent from budget runs (the capture arena, development-file writes, the remote rings, the budget-run record itself) | Win 44 MB; S24 29 MB. Derived in Profiler.md "Memory layout and accounting": 40,844,376 bytes = 40.84 MB on Windows and 27,005,016 bytes = 27.01 MB on the S24, in MB of 10^6 bytes ([Z2-N-BUDGET-141]), with about 7% headroom |
| Z2-B-DEVT-002 | On-screen profiler overhead CPU | MP-CPUZONE | the zone `DevTools.EndFrame` plus `DevTools.DrainDebugDraws` plus every `DevTools.ExtractDebugDraws@W<tag>`, per frame, with the overlay in its `full` state; the hook cost is reported beside it from the enabled-versus-disabled run pair ([Z2-N-BUDGET-107]) | Win 0.40 ms; S24 0.90 ms |
| Z2-B-DEVT-003 | Debug overlays and on-screen profiler GPU | MP-GPUPASS | the `DebugOverlay` pass of every view (and `HiZDebugOverlay`, which RPROD's culling design places under the same row), from the GPU zones the profiler builds in either mode; the run is the overhead pair's enabled half, with the overlay in `full`, and the counters-only half still measures every other MP-GPUPASS row because GPU ingestion follows `devtools.profiler.gpuTimings`, not the zone mode (Profiler.md "GPU timing ingestion") | Win 0.15 ms; S24 0.40 ms |

The producer is "Stage 1 per-system budget ticket for DEVT", blocked by the CORE, JOBS and RPROD rulings (`Zenith2/Docs/Budgets.md` → "Row index"); DEVT has no Stage 1 harness, so the ruling cites the reference engine's profiler cost and published Tracy figures (2–5 ns per zone event on the reference CPU class) and the derivations named above.

### Rows DEVT is measured against or consumes

| Row | How DEVT relates |
|---|---|
| Z2-B-CORE-004, Z2-B-CORE-005 | DEVT's arenas are its own blocks, counted in Z2-B-DEVT-001, never in the frame arenas |
| Z2-B-RHI-006 (upload ring hard cap) | the overlay's vertex upload is at most 3 MiB per frame within it (DebugDraw.md "Overlay upload budget") |
| Z2-B-SHIP-001 and the frame-rate rows | the budget run measures them; the overlay is off during them ([Z2-N-BUDGET-107]) |
| Z2-B-PROC-026 (`linux-tsan` step timeout) | the TSan smoke suite runs inside it |
| every area's MP-GPUPASS row | measured from the profiler's GPU zones in budget runs, which disable only the profiler's CPU zones and overlay |

### Pending row needs

None beyond the three DEVT rows, which are pending until the Stage 1 DEVT ruling.

## Stage plan

### Stage 0

- Z2-I-DEVT-12 and Z2-I-DEVT-13: the libgraph front end and the `max-file-loc` key are Stage 0 PROC tickets; DEVT owns the two requirements they list (QualityTools.md "Gates in CI"; row DV17).

### Stage 1

| Item | Earliest consumer | Tickets |
|---|---|---|
| the `linux-tsan` WSL host and LLVM pin: the `wsl.conf` data file and DEVT's provisioning requirements on SHIP's step, `Zenith2LinuxTsan.psm1` with `Invoke-LinuxTsan.ps1` and its assert script, the TSan smoke suite, `Test-LinuxTsanHost.ps1` with both modes | Z2T-COMPRESS-016, Z2T-GEOM-015 (Stage 1 sprint), Z2T-SCENE-052, Z2T-SCENE-082, Z2T-SCRIPT-058 (golden recordings); Z2T-CORE-034, Z2T-ECS-094, Z2T-JOBS-032 (Stage 2 sanitizer runs) | four ordinary tickets (the provisioning data and module, the runner, the smoke suite, the verification script), then the `needs-human` TASK "Bring up the linux-tsan WSL host" in the Stage 1 sprint, blocked by those four and by SHIP's setup-script ticket, which every recording ticket listed blocks on (QualityTools.md "Host verification"); the Stage 1 acceptance re-runs `Test-LinuxTsanHost.ps1` and does not host the bring-up |
| the evidence-fingerprint specification and `Zenith2Evidence.psm1` with its invalidation tests | Stage 1 budget allocation evidence records (`allocation.json`), the Stage 1 acceptance's coverage step | one specification ticket (this design's Evidence.md is the specification; the ticket commits the module and tests) |
| the Stage 1 per-system budget ticket for DEVT | every DEVT budget citation | `needs-human` ruling, blocked by the CORE, JOBS and RPROD rulings |
| pull-forward into the Stage 1 sprint (fix version `Z2 Stage 2`): the `ZenithProfiling` target and the API-only headers `Zenith_ProfilerTypes.h`, `Zenith_Profiler.h`, `Zenith_ProfileZone.h`, `Zenith_DevConsole.h`, `Zenith_DevFileWriter.h`, `Zenith_DebugDraw.h` | JOBS 005 (pulled forward by X131), ECS 030, PLAT 005, RHI 013, ASSET 005, SCENE 005 and 023, SCRIPT 028 | one API-only ticket blocked by the four CORE API-only tickets whose headers it includes: Z2T-CORE-001 (foundation headers up to `Zenith_ProfilerHooks.h`), Z2T-CORE-014 (`Zenith_ConfigStore.h`), Z2T-CORE-015 (`Zenith_MemoryGovernor.h`) and Z2T-CORE-007 (the maths headers `Zenith_DebugDraw.h` includes); the `Zenith_SpscRing` header is not needed by the API slice (it is internal), so CORE's ring blocks the Stage 2 hook ticket instead (Profiler.md "Profiler public headers") |

### Stage 2

In dependency order (each line one or more tickets of the §9 decomposition):

1. the hook implementation, thread slots, rings (blocked by CORE's `Zenith_SpscRing` ticket, row DV23), name tables, lanes, counters (Profiler.md), unblocking JOBS 028 and every consumer's counter tests;
2. frame assembly, history, zone statistics, lock events and lock statistics, the runtime toggle, the Shipping mode;
3. the console and the development file writer (Console.md), unblocking GRAPH 050, ECS 069, SCENE 050, RPROD 047, 071, 083, SCRIPT 063;
4. the debug-draw recorder and `Flux_RenderDebugDraw` (API first: `Flux_DebugDraw.h` and the no-pass definition of `Flux_AddDebugOverlay`), then the extractor, the `DebugOverlay` pass and the font, unblocking RPROD 046, 058, 075 and SCENE 050; the recorder ticket implements the recorder slot of whichever Q79 path is ruled (Profiler.md "Thread slots and the thread-local variable");
5. GPU timing ingestion: `Zenith_ProfilerGpuSink` lands in the item-1 slice; the ingestion ticket follows RPROD's `Flux_PassTimingPublisher` ticket (Z2T-RPROD-029, revalidated through row DV5) and RHI's caller (Z2T-RHI-075, row DV24);
6. the overlay core: state machine, top bar, timeline, frame history, memory and counter panels, the expression language, and the panel catalogue with the Stage 2 areas' rows, which DEVT's catalogue tickets write (one ticket per few areas, transcribing each area's "On-screen profiler rows", Overlay.md "Built-in panel catalogue", row DV31);
7. Tracy forwarding; the capture format, writer and reader; `ZenithDevTool capture`;
8. the JSON Lines file sink, the log viewer, run and frame telemetry (RHI's telemetry events land with it);
9. `Zenith_CrashHandler` API-only header (the MOD step-7 slot, the SCRIPT watchdog field and `Zenith_RunCrashHelperIfRequested`, which returns false until Stage 3), and the D-SEL-CRASH harness and ruling;
10. fuzzing conventions and the campaign runner with its module and assert script; the `.clang-tidy` configuration;
11. the Stage 2 integration: `Test_FrameGraph_ZonesForEveryNode`, `Test_Profiler_ContentionRowsFromCounters`, the overlay render test on WARP and lavapipe.

D-SEL-CRASH (open, Stage 2) gates the crash implementation tickets of Stage 3, never the API-only header.

### Stage 3

The crash handler realisation chosen by D-SEL-CRASH on Windows and Android (with the anchor and the helper's security contract when the custom writer is chosen; the anchor's allowlist entry is PROC's `needs-human` edit, row DV17 (e), which blocks the implementing ticket), reports, annotations, attachments, fatal recognition, finalization with the Android exit-record mapping, the hang report, symbolization; the remote diagnostics server, protocol and client; the Luau panel (SCRIPT lands Luau gameplay in Stage 3); the overlay on the S24 and in the editor; the workload runner, row sources, the budget-run record and evidence writer (the Stage 3 focused workloads need them); the GPU capture button; regression dashboards; the Stage 3 device runs (S24 overlay, remote, crash) as `needs-human` steps.

### Stage 4

Content-validation reports and the asset-profiling view (ASSET's Stage 4 pipeline).

### Stages 5 6 and 7

No new DEVT capability: Stage 5 adds the panel rows of the areas whose features land then (each area's row list joins the catalogue in that area's ticket, Overlay.md "Built-in panel catalogue"); Stage 6 runs the fuzz campaign for its full duration and the Shipping symbol check as security evidence (`Zenith2/Docs/Architecture.md` → "Security architecture"); Stage 7 has no DEVT item.

### API-only slices

The Stage 1 pull-forward slice above; the `Zenith_CrashHandler` header (Stage 2, item 9); `Flux_DebugDraw.h` with `Flux_RenderDebugDraw` and the no-pass definition of RPROD's `Flux_AddDebugOverlay` in `Internal/Debug/Flux_DebugOverlay.cpp` (Stage 2, item 4) before the pass (A5 clarification (d) item 5); `Zenith_BudgetRunRecord.h` with `Zenith_EvidenceFileWriter` (Stage 2 sprint, fix version `Z2 Stage 3`) before ENV 070 and GEOM 118.

## Decisions

### Settled decisions

| ID | Governs |
|---|---|
| Z2-D-005 | the on-screen profiler in game and editor, on Windows and the S24, a runtime toggle, Tracy for deep capture, the core in Stage 2 |
| Z2-D-007 | libgraph and the complexity gate as hard CI gates (Z2-I-DEVT-12, -13) |
| Z2-D-008 | the manifest pins the WSL distro and the Linux LLVM |
| Z2-D-011 | `linux-tsan` under WSL2 with TSan and UBSan, dependencies from source, suppressions only with a decision |
| Z2-D-012 | the DEVT budget rows and the budget-run evidence |
| Z2-D-019 | development tools are never simulation-visible |
| Z2-D-028 | the 60-second fuzz smoke and the TSan suite within the gate budget |
| Z2-D-040, Z2-D-041 | ownership; the one custom container's justification record (the name table; the ring is CORE's by ruling Q80) |
| Z2-D-061 | standard Z by default: `DebugOverlay`'s `m_uReversedZ` is the view's depth mode, and `Frustum` is depth-mode independent |
| Z2-D-017 | exceptions only in the Luau firewall's `/EHs` compile groups (the clang-tidy rationale) |
| Z2-D-045, Z2-D-047 | results, asserts and contract blocks |
| Z2-D-046 | the profiler hooks and the one thread-local variable as the only process-wide state, and, as extended by ruling Q88 (a) (the dated extension X273 and X282 append, with X308 and rows DV17 (g) and DV35 rewording the statements that count the allowlisted variables), the crash handler anchor `g_pxCrashHandlerAnchor` |
| Z2-D-050 | the overlay and debug draws appear through the pipelined render world |
| Z2-D-055, Z2-D-057 | GPU per-pass timings from the graph; transient memory with and without aliasing in the memory panel |
| Z2-D-065 | the evidence fingerprint |
| Z2-D-082 | Tracy pinned and confined to `ZenithProfiling/Internal/` |
| Z2-D-084 | the D-SEL-CRASH harness and ruling split |
| Z2-D-097 | rulings appended to the DecisionLog in their ticket's diff |

### Open decisions

- D-SEL-CRASH (open, Stage 2): Crashpad versus a custom minidump writer. DEVT owns the harness and its report; the acceptance rule is the core documents' and DEVT restates it without change: `Zenith2/Docs/Architecture.md` → "Named selection decisions" ("Crashpad unless it cannot meet the 16 KB page and minSdk 34 constraints or the package budget", over Windows and Android coverage, symbol-server integration, size and maintenance) together with `Zenith2/Docs/DecisionLog.md`'s D-SEL-CRASH acceptance clauses (out-of-process capture on Windows and Android, minidumps symbolised against the retained symbols, no exceptions in engine code, nothing reachable remotely in Shipping). Crash.md "D-SEL-CRASH candidates and acceptance" lists the measured inputs to that rule and records that the custom candidate's Android path fails the minidump clause.
- Q79 (human-open, folded with Q11): whether the debug-draw recorder may cache its ring in the profiler's thread slot. DEVT specifies the reuse as the primary path inside an `<!-- assumption: pending Q79/Q11 -->` block and the alternative in full (DebugDraw.md "Recorder slot alternative"), so either answer is implementable.
- Q88 (orchestrator-ruled (a) 2026-10-09, human may override with the fallback (c)): the crash handler's OS callbacks reach it through one allowlisted atomic anchor; Crash.md "Crash handler anchor" fences the design in an `<!-- assumption: pending X273 -->` block until the core batch applies X273, X282 and X308 and PROC lands rows DV17 (e) and (g). The anchor has four writes (the claim by `Create`, the release by the destructor, the crash path's claim and the same trampoline's unclaim) and exists under either D-SEL-CRASH realisation; Q88 stays flagged for human attention.
- D-SEL-HASH (open, Stage 1): the name tables hash names with `Zenith_Hash64`; the design is independent of the outcome.
- D-SEL-JSON (open, Stage 1): every DEVT JSON reader uses CORE's reader; independent of the outcome.
- D-SEL-HEAP (open, Stage 1): no effect on DEVT (all its memory is reserved at creation).

### Selection decisions DEVT owns

D-SEL-CRASH alone. Its harness ticket builds `Zenith2/Benchmarks/CrashPrototype/` (both candidates' capture paths on Windows and Android and the six measured inputs of Crash.md "D-SEL-CRASH candidates and acceptance": coverage, out-of-process and minidump, symbol-server integration, size and the 16 KB, minSdk 34 and package-budget constraints, the exception and remote-reachability checks, maintenance) with a CTest `--smoke` that runs the Windows custom capture against a child process; its `needs-human` ruling runs both on the reference desktop and the S24, commits `Zenith2/Docs/Research/DSelCrash/Report.md`, applies the rule of § "Open decisions" verbatim (Crashpad unless disqualified; a custom selection records its Android minidump deviation) and appends the ruling to `Zenith2/Docs/DecisionLog.md` with the next free `Z2-D-NNN` in its own diff (Z2-D-084, Z2-D-097).

### Justification records DEVT adds

One record. By ruling Q80 the single-producer single-consumer ring is CORE's `Zenith_SpscRing<T>` (its record, `Custom threading primitive: Zenith_SpscRing`, is CORE's, appended by CORE's landing ticket in the shape AUDIO specifies), so DEVT adds no ring and no record for one. The name table's record is appended to `Zenith2/Docs/DecisionLog.md` with an explicit `Z2-D-NNN` by the diff that adds the table ([Z2-N-CONTRACT-278], Z2-D-097), provisionally, every field of [Z2-N-CONTRACT-279]'s template filled:

```markdown
### Z2-D-NNN Custom container: Zenith_ProfilerNameTable

- Status: Provisional
- Kind: container
- Standard alternative: CORE's Zenith_HashMap<uint64, uint32> guarded by a Zenith_Mutex
- Improvement claimed: lock-free lookup and insert-only publication, callable from a profiler hook while any lock of any
  level is held (a mutex at any level would violate the lock order when a hook fires under a higher leaf lock, such as
  uLOCK_LEVEL_HEAP_LEAK_TABLE 990), with no allocation after Init
- Measured evidence: pending — Zenith_Bench_ProfilerOverhead run recorded by the ruling <the DEVT name-table ruling ticket>
- Maintenance cost: about 400 lines (Internal/Capture/Zenith_ProfilerNameTable.cpp, .h); the tests
  Test_ProfilerNameTable_MatchesOracle, _Properties, _SizeOverflow_IsRejected, _StressConcurrentInsert; linux-tsan and
  win-asan coverage through the Zenith2Memory sanitizer lines (X247 (c)); no platform code; owner DEVT's profiler epic
- Acceptance criteria: NameTable_Lookup_Resident median ≤ 20 ns on the reference desktop and ≤ 50% of
  HashMapMutex_Lookup_Resident's median on the reference desktop and the S24 at 1, 2, 8 and all threads;
  NameTable_Insert_Distinct ≤ HashMapMutex_Insert_Distinct; NameTable_LookupUnderHeldLock reports zero lock-order
  violations (while HashMapMutex_LookupUnderHeldLock reports them); re-run on each dependency upgrade
- Contract: Zenith2/Engine/ZenithProfiling/Internal/Capture/Zenith_ProfilerNameTable.h (an Internal header; the table is not
  public, and its contract block carries the container fields of Zenith2/Docs/Design/DEVT/Profiler.md "Name tables")
- Decision: adopt provisionally
```

The `needs-human` ruling ticket runs `Zenith_Bench_ProfilerOverhead` (`Zenith2/Benchmarks/ProfilerOverhead/`) on the reference desktop and the S24 and completes the record in place ([Z2-N-CONTRACT-292]). The harness measures exactly the record's criteria: the cases `NameTable_Lookup_Resident`, `NameTable_Insert_Distinct` and `NameTable_LookupUnderHeldLock` against their alternatives `HashMapMutex_Lookup_Resident`, `HashMapMutex_Insert_Distinct` and `HashMapMutex_LookupUnderHeldLock` under identical names, thread counts and held locks (Profiler.md "Profiler cost model"); the zone-hook cases of the same harness serve the cost model and Z2-B-DEVT-002, not the record.

## Requirement index

Every requirement of `Zenith2/Docs/Requirements/DEVT.md` and the design headings that specify it (paths relative to `Zenith2/Docs/Design/`):

| Requirement | Title | Design headings |
|---|---|---|
| Z2-R-DEVT-001 | ZenithProfiling library target | DEVT.md → "ZenithProfiling target"; DEVT.md → "Libraries DEVT code lives in" |
| Z2-R-DEVT-002 | API-only headers and forward-declarable types | DEVT/Profiler.md → "Profiler public headers"; DEVT.md → "ZenithProfiling public headers" |
| Z2-R-DEVT-003 | Hook table filling | DEVT/Profiler.md → "Hook implementation" |
| Z2-R-DEVT-004 | Zone names copied at the call | DEVT/Profiler.md → "Hook implementation"; DEVT/Profiler.md → "Name tables" |
| Z2-R-DEVT-005 | Zone values | DEVT/Profiler.md → "Zenith_ProfileZone header" |
| Z2-R-DEVT-006 | Per-thread event rings | DEVT/Profiler.md → "Per-thread event rings"; DEVT/Profiler.md → "Event records" |
| Z2-R-DEVT-007 | Hooks lock-free and allocation-free | DEVT/Profiler.md → "Hook implementation"; DEVT.md → "Lock-order entries" |
| Z2-R-DEVT-008 | Thread lanes | DEVT/Profiler.md → "Thread lanes" |
| Z2-R-DEVT-009 | Counter registry honours counter kinds | DEVT/Profiler.md → "Counter registry" |
| Z2-R-DEVT-010 | Frame assembly | DEVT/Profiler.md → "Frame assembly" |
| Z2-R-DEVT-011 | Frame history and hitch detection | DEVT/Profiler.md → "Frame history and detailed frames" |
| Z2-R-DEVT-012 | Detailed frames and freeze | DEVT/Profiler.md → "Frame history and detailed frames"; DEVT/Overlay.md → "Freeze and frame selection" |
| Z2-R-DEVT-013 | Per-frame zone statistics | DEVT/Profiler.md → "Zone statistics" |
| Z2-R-DEVT-014 | GPU timing ingestion | DEVT/Profiler.md → "GPU timing ingestion" |
| Z2-R-DEVT-015 | Runtime capture toggle | DEVT/Profiler.md → "Runtime toggle and modes" |
| Z2-R-DEVT-016 | Shipping profiler configuration | DEVT/Profiler.md → "Runtime toggle and modes"; DEVT.md → "Configuration keys" |
| Z2-R-DEVT-017 | Tracy forwarding | DEVT/Profiler.md → "Tracy forwarding" |
| Z2-R-DEVT-018 | Profiler memory preallocation and accounting | DEVT/Profiler.md → "Memory layout and accounting" |
| Z2-R-DEVT-019 | Lock and blocking-wait events | DEVT/Profiler.md → "Lock and wait events" |
| Z2-R-DEVT-020 | Profiler creation unwinding and errors | DEVT/Profiler.md → "Creation and shutdown"; DEVT.md → "Error enumerations" |
| Z2-R-DEVT-021 | Profiler shutdown and the flush thread | DEVT/Profiler.md → "Creation and shutdown"; DEVT/Profiler.md → "Capture writer and the flush thread" |
| Z2-R-DEVT-022 | Capture file format | DEVT/Profiler.md → "Capture format" |
| Z2-R-DEVT-023 | Capture start stop and segment output | DEVT/Profiler.md → "Capture writer and the flush thread" |
| Z2-R-DEVT-024 | Memory snapshots in captures | DEVT/Profiler.md → "Capture format" |
| Z2-R-DEVT-025 | Name-table justification record | DEVT.md → "Justification records DEVT adds"; DEVT/Profiler.md → "Name tables" |
| Z2-R-DEVT-026 | Profiler test support header | DEVT/Tests.md → "Test targets"; DEVT.md → "ZenithProfiling test files" |
| Z2-R-DEVT-027 | DEVT configuration keys | DEVT.md → "Configuration keys" |
| Z2-R-DEVT-028 | DEVT counters and log categories | DEVT.md → "Counters"; DEVT.md → "Log categories" |
| Z2-R-DEVT-029 | Overlay states and the runtime toggle | DEVT/Overlay.md → "Overlay state machine" |
| Z2-R-DEVT-030 | CPU frame-graph timeline view | DEVT/Overlay.md → "Timeline view" |
| Z2-R-DEVT-031 | GPU pass timeline view | DEVT/Overlay.md → "GPU view" |
| Z2-R-DEVT-032 | Frame-time history with hitch markers | DEVT/Overlay.md → "Frame history view" |
| Z2-R-DEVT-033 | Memory panel with aliasing savings | DEVT/Overlay.md → "Built-in panel catalogue" |
| Z2-R-DEVT-034 | Counter panel | DEVT/Overlay.md → "Panels" |
| Z2-R-DEVT-035 | Panel expression language | DEVT/Overlay.md → "Expression language" |
| Z2-R-DEVT-036 | Built-in panel catalogue | DEVT/Overlay.md → "Built-in panel catalogue" |
| Z2-R-DEVT-037 | Contention rows from JOBS counters | DEVT/Overlay.md → "Built-in panel catalogue"; DEVT/Tests.md → "Integration suites" |
| Z2-R-DEVT-038 | Luau time and GC rows | DEVT/Overlay.md → "Built-in panel catalogue" |
| Z2-R-DEVT-039 | Panel providers | DEVT/Overlay.md → "Panel providers" |
| Z2-R-DEVT-040 | Overlay input and pointer claims | DEVT/Overlay.md → "Overlay input" |
| Z2-R-DEVT-041 | Overlay drawing through debug-draw primitives | DEVT/Overlay.md → "Overlay drawing"; DEVT/Overlay.md → "Overlay layout" |
| Z2-R-DEVT-042 | Overlay freeze and frame selection | DEVT/Overlay.md → "Freeze and frame selection" |
| Z2-R-DEVT-043 | Graph view and dump version refusal | DEVT/Overlay.md → "Graph view" |
| Z2-R-DEVT-044 | On-screen profiler on the S24 | DEVT/Overlay.md → "S24 overlay" |
| Z2-R-DEVT-045 | In-editor on-screen profiler | DEVT/Overlay.md → "Editor overlay" |
| Z2-R-DEVT-046 | Overlay GPU capture button | DEVT/Overlay.md → "GPU view" |
| Z2-R-DEVT-047 | Console command registry | DEVT/Console.md → "Zenith_DevConsole header"; DEVT/Console.md → "Command registry" |
| Z2-R-DEVT-048 | Console grammar | DEVT/Console.md → "Console grammar" |
| Z2-R-DEVT-049 | Built-in console commands | DEVT/Console.md → "Built-in commands" |
| Z2-R-DEVT-050 | Configuration variables through the console | DEVT/Console.md → "Configuration variables" |
| Z2-R-DEVT-051 | Console output and history | DEVT/Console.md → "Console output and history" |
| Z2-R-DEVT-052 | On-screen console view | DEVT/Console.md → "On-screen console" |
| Z2-R-DEVT-053 | Development file writer | DEVT/Console.md → "Zenith_DevFileWriter header" |
| Z2-R-DEVT-054 | Console absent in Shipping | DEVT/Console.md → "Shipping absence of development services" |
| Z2-R-DEVT-055 | Runtime development commands | DEVT/Console.md → "Runtime development commands" |
| Z2-R-DEVT-056 | DEVT frame dump | DEVT/Console.md → "DEVT frame dump" |
| Z2-R-DEVT-057 | Debug-draw recorder API | DEVT/DebugDraw.md → "Zenith_DebugDraw header"; DEVT/DebugDraw.md → "Shape expansion" |
| Z2-R-DEVT-058 | Debug-draw rings and drain | DEVT/DebugDraw.md → "Recorder rings and drain"; DEVT/DebugDraw.md → "Recorder records" |
| Z2-R-DEVT-059 | Debug-draw durations world tags and view filtering | DEVT/DebugDraw.md → "Durations and world tags" |
| Z2-R-DEVT-060 | Render-world debug-draw records | DEVT/DebugDraw.md → "Flux_DebugDraw header" |
| Z2-R-DEVT-061 | DebugOverlay pass | DEVT/DebugDraw.md → "DebugOverlay pass"; DEVT/DebugDraw.md → "DebugLines program"; DEVT/DebugDraw.md → "DebugText program" |
| Z2-R-DEVT-062 | Debug font atlas | DEVT/DebugDraw.md → "Debug font" |
| Z2-R-DEVT-063 | Debug-draw extractor | DEVT/DebugDraw.md → "Debug-draw extractor" |
| Z2-R-DEVT-064 | Debug overlay GPU cost | DEVT/DebugDraw.md → "Overlay upload budget"; DEVT.md → "Rows DEVT owns" |
| Z2-R-DEVT-065 | JSON Lines file log sink | DEVT/Console.md → "Log file sink" |
| Z2-R-DEVT-066 | Log record schema | DEVT/Console.md → "Log record schema" |
| Z2-R-DEVT-067 | Log viewer | DEVT/QualityTools.md → "ZenithDevTool" |
| Z2-R-DEVT-068 | Remote diagnostics server | DEVT/Console.md → "Remote diagnostics server" |
| Z2-R-DEVT-069 | Remote protocol framing | DEVT/Console.md → "Remote protocol" |
| Z2-R-DEVT-070 | Remote console and log streaming | DEVT/Console.md → "Remote messages" |
| Z2-R-DEVT-071 | Remote capture and file fetch | DEVT/Console.md → "Remote messages" |
| Z2-R-DEVT-072 | Remote client and the adb workflow | DEVT/Console.md → "Remote client" |
| Z2-R-DEVT-073 | Remote server absent in Shipping | DEVT/Console.md → "Shipping absence of development services" |
| Z2-R-DEVT-074 | D-SEL-CRASH selection | DEVT/Crash.md → "D-SEL-CRASH candidates and acceptance"; DEVT.md → "Selection decisions DEVT owns" |
| Z2-R-DEVT-075 | Crash handler API-only header | DEVT/Crash.md → "Zenith_CrashHandler header" |
| Z2-R-DEVT-076 | Windows out-of-process crash capture | DEVT/Crash.md → "Windows capture"; DEVT/Crash.md → "Crash helper section layout"; DEVT/Tests.md → "Crash tests" |
| Z2-R-DEVT-077 | Android crash capture | DEVT/Crash.md → "Android capture"; DEVT/Crash.md → "Signal-safe report tail" |
| Z2-R-DEVT-078 | Linux host without capture and sanitizer behaviour | DEVT/Crash.md → "Linux capture"; DEVT/Crash.md → "Sanitizer builds" |
| Z2-R-DEVT-079 | Crash report format | DEVT/Crash.md → "Crash report files"; DEVT/Crash.md → "Crash report schema"; DEVT/Crash.md → "Signal-safe report tail" |
| Z2-R-DEVT-080 | Crash annotations and breadcrumbs | DEVT/Crash.md → "Annotations and breadcrumbs"; DEVT/Crash.md → "Crash log mirror" |
| Z2-R-DEVT-081 | Crash attachments and device-loss association | DEVT/Crash.md → "Attachments" |
| Z2-R-DEVT-082 | Next-launch finalization and retention | DEVT/Crash.md → "Next-launch finalization" |
| Z2-R-DEVT-083 | Script hang report | DEVT/Crash.md → "Hang reports" |
| Z2-R-DEVT-084 | Thread registration | DEVT/Crash.md → "Thread registration" |
| Z2-R-DEVT-085 | Crash symbolization | DEVT/Crash.md → "Symbolization" |
| Z2-R-DEVT-086 | Crash capture in Shipping without remote exposure | DEVT/Crash.md → "Crash handler lifecycle"; DEVT/Console.md → "Shipping absence of development services" |
| Z2-R-DEVT-087 | Evidence fingerprint specification | DEVT/Evidence.md → "Evidence fingerprint specification"; DEVT/Evidence.md → "Fingerprint algorithm" |
| Z2-R-DEVT-088 | Fingerprint invalidation tests | DEVT/Evidence.md → "Fingerprint invalidation tests"; DEVT/Evidence.md → "Zenith2Evidence module" |
| Z2-R-DEVT-089 | Workload runner | DEVT/Evidence.md → "Workload runner" |
| Z2-R-DEVT-090 | Row source bindings | DEVT/Evidence.md → "Row source bindings" |
| Z2-R-DEVT-091 | Budget-run record and statistics | DEVT/Evidence.md → "Zenith_BudgetRunRecord header"; DEVT/Evidence.md → "Budget-run sampling"; DEVT/Evidence.md → "Statistics" |
| Z2-R-DEVT-092 | Evidence record writer | DEVT/Evidence.md → "Evidence record writer"; DEVT/Evidence.md → "Per-frame capture CSV" |
| Z2-R-DEVT-093 | Budget-run measured interval and run length | DEVT/Evidence.md → "Measured interval and run length" |
| Z2-R-DEVT-094 | Area row emitters | DEVT/Evidence.md → "Area row emitters" |
| Z2-R-DEVT-095 | Profiler disabled during budget runs | DEVT/Evidence.md → "Budget-run sampling" |
| Z2-R-DEVT-096 | Run telemetry and frame telemetry | DEVT/Evidence.md → "Run telemetry"; DEVT/Evidence.md → "Frame telemetry" |
| Z2-R-DEVT-097 | Regression dashboards | DEVT/Evidence.md → "Regression dashboards" |
| Z2-R-DEVT-098 | linux-tsan WSL distro provisioning | DEVT/QualityTools.md → "WSL distro provisioning"; DEVT/QualityTools.md → "Kernel setting" |
| Z2-R-DEVT-099 | linux-tsan toolchain and dependency mirror | DEVT/QualityTools.md → "Toolchain and dependency mirror"; DEVT/QualityTools.md → "The linux-tsan toolchain file" |
| Z2-R-DEVT-100 | linux-tsan host runner | DEVT/QualityTools.md → "Host runner" |
| Z2-R-DEVT-101 | TSan smoke suite | DEVT/QualityTools.md → "TSan smoke suite" |
| Z2-R-DEVT-102 | linux-tsan host verification | DEVT/QualityTools.md → "Host verification" |
| Z2-R-DEVT-103 | libgraph gate in CI | DEVT/QualityTools.md → "Gates in CI" |
| Z2-R-DEVT-104 | Complexity gate in CI | DEVT/QualityTools.md → "Gates in CI" |
| Z2-R-DEVT-105 | Fuzzing conventions and regression inputs | DEVT/QualityTools.md → "Fuzzing" |
| Z2-R-DEVT-106 | Fuzz campaign runner | DEVT/QualityTools.md → "Fuzz campaign runner" |
| Z2-R-DEVT-107 | clang-tidy configuration | DEVT/QualityTools.md → "Static analysis" |
| Z2-R-DEVT-108 | Content validation reports | DEVT/QualityTools.md → "Content validation reports" |
| Z2-R-DEVT-109 | Asset profiling view | DEVT/QualityTools.md → "Asset profiling" |
| Z2-R-DEVT-110 | ZenithDevTool host tool | DEVT/QualityTools.md → "ZenithDevTool" |
| Z2-R-DEVT-111 | Capture conversion to trace events | DEVT/QualityTools.md → "Capture conversion" |
| Z2-R-DEVT-112 | Profiler and telemetry memory within its row | DEVT.md → "Rows DEVT owns"; DEVT/Profiler.md → "Memory layout and accounting" |
| Z2-R-DEVT-113 | Profiler CPU overhead within its row | DEVT.md → "Rows DEVT owns"; DEVT/Profiler.md → "Profiler cost model" |
| Z2-R-DEVT-114 | Stage 1 DEVT budget allocation | DEVT.md → "Rows DEVT owns"; DEVT.md → "Stage 1" |
| Z2-R-DEVT-115 | Frame-graph zones integration test | DEVT/Tests.md → "Integration suites" |
| Z2-R-DEVT-116 | Stage 2 on-screen profiler integration | DEVT/Tests.md → "Render tests"; DEVT.md → "Stage 2" |
| Z2-R-DEVT-117 | Stage 3 device and editor integration | DEVT/Tests.md → "Device and acceptance runs"; DEVT.md → "Stage 3" |
| Z2-R-DEVT-118 | Sanitizer runs of DEVT code | DEVT/Tests.md → "Stress and sanitizer runs" |
| Z2-R-DEVT-119 | Shipping forbidden symbols for DEVT services | DEVT/Console.md → "Shipping absence of development services" |
| Z2-R-DEVT-120 | Crash handler behaviour | DEVT/Crash.md → "Crash handler lifecycle" |
| Z2-R-DEVT-121 | Crash helper security contract | DEVT/Crash.md → "Crash helper security contract"; DEVT/Crash.md → "Windows capture" |
| Z2-R-DEVT-122 | Fatal recognition without a CORE hook | DEVT/Crash.md → "Fatal failures" |
| Z2-R-DEVT-123 | Crash structures race-free under TSan | DEVT/Crash.md → "Annotations and breadcrumbs"; DEVT/Crash.md → "Crash log mirror" |
| Z2-R-DEVT-124 | GPU timing sink path | DEVT/Profiler.md → "GPU timing ingestion"; DEVT/Profiler.md → "Zenith_Profiler header" |
| Z2-R-DEVT-125 | GPU timings independent of the zone mode | DEVT/Profiler.md → "GPU timing ingestion"; DEVT/Profiler.md → "Runtime toggle and modes" |
| Z2-R-DEVT-126 | Zone macros expand the line number | DEVT/Profiler.md → "Zenith_ProfileZone header" |
| Z2-R-DEVT-127 | Zone balance across mode and Tracy changes | DEVT/Profiler.md → "Hook implementation" |
| Z2-R-DEVT-128 | Outputs detached before the async IO service is destroyed | DEVT.md → "Cancellation and shutdown"; DEVT/Console.md → "Log file sink" |
| Z2-R-DEVT-129 | Overlay never overflows the main ring | DEVT/DebugDraw.md → "Recorder rings and drain"; DEVT/Overlay.md → "Overlay drawing" |
| Z2-R-DEVT-130 | Recorder slot per ruling Q79 | DEVT/Profiler.md → "Thread slots and the thread-local variable"; DEVT/DebugDraw.md → "Recorder slot alternative" |
| Z2-R-DEVT-131 | Evidence files created once and appended | DEVT/Evidence.md → "Zenith_BudgetRunRecord header"; DEVT/Evidence.md → "Per-frame capture CSV" |
| Z2-R-DEVT-132 | linux-tsan host bring-up task | DEVT/QualityTools.md → "Host verification"; DEVT.md → "Stage 1" |
| Z2-R-DEVT-133 | Crash handler anchor | DEVT/Crash.md → "Crash handler anchor"; DEVT/Crash.md → "Crash handler lifecycle" |
| Z2-R-DEVT-134 | Shipping behaviour proven in ShippingTest | DEVT/Tests.md → "ShippingTest cases"; DEVT.md → "API rules" |
| Z2-R-DEVT-135 | Development-service tests correct in ShippingTest | DEVT/Tests.md → "ShippingTest cases"; DEVT/Console.md → "Shipping absence of development services" |
| Z2-R-DEVT-136 | Crash helper launch handles and access rights | DEVT/Crash.md → "Windows capture"; DEVT/Crash.md → "Crash helper security contract" |
| Z2-R-DEVT-137 | Profiler shared state race-free under TSan | DEVT/Profiler.md → "Thread slots and the thread-local variable"; DEVT/Profiler.md → "Thread lanes"; DEVT/Profiler.md → "GPU timing ingestion" |
| Z2-R-DEVT-138 | Name arena exhaustion never spins | DEVT/Profiler.md → "Name tables" |
| Z2-R-DEVT-139 | Shipping GPU timings follow the on-screen profiler toggle | DEVT/Profiler.md → "Runtime toggle and modes"; DEVT.md → "Configuration keys" |
| Z2-R-DEVT-140 | Budget-run record excluded from memory samples | DEVT/Evidence.md → "Zenith_BudgetRunRecord header"; DEVT/Evidence.md → "Row source bindings" |
| Z2-R-DEVT-141 | Console handlers never write simulation state | DEVT/Console.md → "Zenith_DevConsole header"; DEVT.md → "Determinism" |
| Z2-R-DEVT-142 | Death cases classified by their macro | DEVT/Tests.md → "Death cases" |
| Z2-R-DEVT-143 | Profiling brackets accumulate per site per frame | DEVT/Profiler.md → "Zone statistics"; DEVT/Profiler.md → "Zenith_ProfileZone header" |
| Z2-R-DEVT-144 | Lock statistics accumulate per lock site per frame | DEVT/Profiler.md → "Lock statistics"; DEVT/Profiler.md → "Lock and wait events"; DEVT/Profiler.md → "Capture format"; DEVT/Overlay.md → "Timeline view" |
| Z2-R-DEVT-145 | Lock statistics are collected race-free | DEVT/Profiler.md → "Lock statistics"; DEVT/Profiler.md → "Thread slots and the thread-local variable" |
| Z2-R-DEVT-146 | The profiler cannot recurse through its own primitives | DEVT/Profiler.md → "Lock and wait events"; DEVT.md → "Lock-order entries" |
| Z2-R-DEVT-147 | The profiler's own code reaches hooks only through the dispatch functions | DEVT/Profiler.md → "Hook implementation"; DEVT/Profiler.md → "Zenith_ProfileZone header"; DEVT/Profiler.md → "Capture writer and the flush thread" |
