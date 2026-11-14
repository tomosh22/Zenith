# RPROD design — rendering production

This is the entry document of the RPROD design family: the design of the production renderer machinery of Flux — the render graph's front end (API, culling, ordering, placement, barrier synthesis, merging, the performance-rule checks), render views, two-phase HiZ culling and GPU-driven indirect draw compaction, the offline shader compiler and the runtime shader library, PSO keys, precaching and the missing-PSO fallback, frame pacing, per-pass timestamps, per-platform quality tiers, the composition of the full frame of every shading path, the GPU performance lints, and the parity suites. The specification it elaborates is `Zenith2/Docs/Rendering.md` and its section files; where they disagree the specification wins and this family is corrected in the same diff ([Z2-N-REND-001]). A worker implements against this document, `Zenith2/Docs/WorkerGuide.md` and at most three cited sections.

## Purpose and scope

### What RPROD owns

| Owned | Where | Specified in |
|---|---|---|
| the render graph front end: public API, setup, compile steps 1, 2, 4 and 5, merging, the rule checks, the first-write rule and poisoning, timestamp indices, pass contexts, record-time checks, front-end dump keys | `Zenith2/Engine/ZenithFlux/` (graph headers), `Internal/Graph/` (RPROD files) | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` |
| culling: HiZ pyramids and builds, two-phase instance culling, draw keys and command regions, the bucket registry, transparent commands, shadow-view and meshlet culling, the CPU reference, counters, the HiZ overlay | `Internal/Culling/`, `Zenith2/Shaders/Passes/Culling/`, `…/HiZ/`, `Zenith2/Shaders/Common/HiZTest.slang` | `Zenith2/Docs/Design/RPROD/Culling.md` |
| render views: types, registry and histories, view list, depth modes, shading-path selection, the capability matrix data, advanced multi-view, view extensions | `Flux_View.h`, `Flux_ViewContext.h`, `Flux_DepthMode.h`, `Flux_ShadingPath.h`, `Flux_ViewExtension.h`, `Internal/Views/`, `Internal/Paths/` | `Zenith2/Docs/Design/RPROD/Views.md` |
| `Flux_Renderer`, the configuration keys, render-world consumption, the render environment, the frame sequence, quality tiers, the frame pacer | `Flux_Renderer.h`, `Flux_Quality.h`, `Flux_Config.h`, `Internal/Renderer/`, `Internal/Quality/`, `Internal/Pacing/` | `Zenith2/Docs/Design/RPROD/Renderer.md` |
| the render-instance key and delta and the other additions to `Flux_RenderWorld.h`; the engine render components `Zenith.Render.MeshInstance` and `Zenith.Render.Camera` (Q48 ruled (a)); the four engine extractors `Render.ExtractInstances`, `Render.ExtractLights`, `Render.ExtractCameras` and `Render.ExtractEnvironment` | `Flux_RenderWorld.h`, `Flux_RenderComponents.h`; `Zenith2/Engine/ZenithRuntime/Internal/Zenith_EngineExtractors.cpp` (RPROD's file in `Zenith2/Docs/Design/MOD.md → "ZenithRuntime files"`), `Internal/Extraction/`, `Internal/Zenith_RenderTypes.h/.cpp`, `Tests/EngineExtractors/` | `Zenith2/Docs/Design/RPROD/Extractors.md` |
| the feature-function surface, the four path functions, pass ownership, RPROD's passes | `Flux_Features.h`, `Internal/Passes/` | `Zenith2/Docs/Design/RPROD/Passes.md` |
| the offline shader compiler, `ZenithShadersGenerated`, the shader tree's structure, the runtime shader library, PSO keys, the PSO table, precache sets and cook step, the creation schedule, the missing-PSO fallback, the shader and PSO lints, the permutation limit | `Zenith2/Tools/ZenithShaderCompiler/`, `Zenith2/Engine/ZenithShadersGenerated/`, `Zenith2/Shaders/`, `Flux_PSO.h`, `Flux_PSOPrecache.h`, `Internal/PSO/`, `Zenith2/Tools/ZenithCook/Internal/Steps/PSO/` | `Zenith2/Docs/Design/RPROD/PSO.md` |
| diagnostics | as above | `Zenith2/Docs/Design/RPROD/Diagnostics.md` |
| tests, parity suites and scenes, the RPROD budget harness, the shading-path harness, the D-TBDR harness, the rendering workload games W-Indoor and W-Outdoor | `Tests/`, `Zenith2/Benchmarks/{Budgets/RPROD,ShadingPaths,TBDR}/`, `Zenith2/Games/WorkloadIndoor/`, `Zenith2/Games/WorkloadOutdoor/` | `Zenith2/Docs/Design/RPROD/Tests.md` |

The division of ZenithFlux code with RHI is `Zenith2/Docs/Design/RHI.md → "Division of Flux code with RPROD"`, adopted as it stands (reconciliation row R1), with one realisation choice recorded as row RP1 ("Reconciliation items"; X98 applied): split barriers are emitted as candidates at step 5 and demoted by RHI's steps 6 and 7 (`Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md → "Split-barrier candidates"`). The ownership of `Zenith2/Tools/ZenithShaderCompiler/`, `ZenithShadersGenerated`, `Shaders/` and the rendering workload games is Architecture's table (`Zenith2/Docs/Architecture.md → "Repository layout"`); SHIP owns the Slang and DXC pins and DEVT displays what RPROD publishes.

### Explicit exclusions

| Not in RPROD | Owner or classification | Decision |
|---|---|---|
| compile steps 3, 6 and 7, recording orchestration, submission, the imported-state table, the dump writer, GPU lifetime, upload, readback, descriptors, the GPU scene tables, persistent resources, recovery, the persistent pipeline-cache store | RHI | Z2-D-056, Z2-D-058 |
| the vertex format, meshes, materials, textures, skinning, LOD selection, decals, the permutation cook step | GEOM | Z2-D-062 |
| lighting, shadows, fog, AO, SSR, TAA and upscaling, exposure, post-processing passes | LIGHT | Z2-D-035 |
| the on-screen profiler and graph viewer | DEVT | Z2-D-005 |
| editor viewports, gizmos and the debug UI | EDIT | Z2-D-079 |
| tile-local merging | not implemented until D-TBDR (open, Stage 5) | Z2-D-036 |
| MSAA | not provided | Z2-D-035 |
| stereo views | deferred to Stage 7 (SCOPE), view signatures XR-ready | Z2-D-072 |
| hardware ray tracing, virtualized geometry | deferred to Stage 7 (ADVGFX) | Z2-D-043 |
| runtime shader compilation | forbidden | Z2-D-025 |
| split-screen and per-player views | excluded | Z2-D-076 |

### Inventory items covered

| Item | Summary | Design heading |
|---|---|---|
| Z2-I-RPROD-01 | two-phase HiZ at instance level | `Zenith2/Docs/Design/RPROD/Culling.md → "Two-phase sequence per view"` |
| Z2-I-RPROD-02 | the RG16F linear pyramid, one SPD dispatch | Culling.md "HiZ pyramid resources", "HiZ build dispatch" |
| Z2-I-RPROD-03 | the occlusion test | Culling.md "Culling files", "CPU reference implementation" |
| Z2-I-RPROD-04 | shadow-view HiZ | Culling.md "Shadow view culling realisation" |
| Z2-I-RPROD-05 | HiZ diagnostics | Culling.md "Culling counters readback", "HiZ debug overlay" |
| Z2-I-RPROD-06 | meshlet-level HiZ (S5) | Culling.md "Meshlet culling realisation" |
| Z2-I-RPROD-07 | indirect draw compaction | Culling.md "Instance culling dispatch sequence", "Draw keys and command regions", "Transparent collection and emission" |
| Z2-I-RPROD-08 | basic views (S2) | `Zenith2/Docs/Design/RPROD/Views.md → "View registry"`, "View list construction" |
| Z2-I-RPROD-09 | advanced multi-view (S5) | Views.md "Advanced multi-view" |
| Z2-I-RPROD-10 | automatic barrier synthesis | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md → "Step 5 barrier synthesis realisation"` |
| Z2-I-RPROD-11, -12 | transient aliasing and its toggle | RenderGraphFrontEnd.md "Step 4 placement realisation", "Physical resource caches" |
| Z2-I-RPROD-13, -14, -15 | offline compilation, enumerated permutations, limits | `Zenith2/Docs/Design/RPROD/PSO.md → "Compiler pipeline"`, "Permutation limit" |
| Z2-I-RPROD-16, -17 | PSO precaching, the fallback | PSO.md "Precache cook step", "Missing-PSO fallback realisation" |
| Z2-I-RPROD-18 | frame pacing | `Zenith2/Docs/Design/RPROD/Renderer.md → "Frame pacer"` |
| Z2-I-RPROD-19 | per-pass GPU timestamps | RenderGraphFrontEnd.md "Timestamp insertion and attribution" |
| Z2-I-RPROD-20 | per-platform quality tiers (S5) | Renderer.md "Quality settings", "Quality tier selection" |
| Z2-I-RPROD-21 to -24 | the full frame of every path | `Zenith2/Docs/Design/RPROD/Passes.md → "Path frame functions"` |
| Z2-I-RPROD-25 to -32 | the GPU performance rules and lints | RenderGraphFrontEnd.md "Compile-time rule checks", "Pass merging algorithm"; PSO.md "Shader lint implementation", "PSO lints" |

### Document family

| File | Holds | Its `##` headings |
|---|---|---|
| `Zenith2/Docs/Design/RPROD.md` | this entry: scope, dependencies, layout, the API summary, threading, errors, failure handling, test strategy, budgets, stages, decisions, the requirement index | the fifteen skeleton headings, then "Reconciliation items" |
| `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` | the graph front end | Scope of this file · Graph public headers · Flux_RenderGraph header · Flux_RasterTargets header · Flux_PassContext header · Flux_GraphSettings header · Flux_GraphCaches header · Flux_GraphErrors header · Internal graph files · Graph internal entry points · Setup tables · Field-table walk · Flux_CompiledGraph · Compile driver · Step 1 culling and clear folding · Step 2 ordering through ZenithGraph · First-access widening · Step 4 placement realisation · Physical resource caches · Step 5 barrier synthesis realisation · Split-barrier candidates · Barrier verifier · Pass merging algorithm · Compile-time rule checks · First-write rule and poisoning · Timestamp insertion and attribution · Pass context implementation · Record-time checks · Front-end frame dump keys · Graph limits complexity and memory |
| `Zenith2/Docs/Design/RPROD/Culling.md` | culling | Scope of this file · Culling files · HiZ pyramid resources · HiZ build dispatch · Culling records · Bucket registry · Draw keys and command regions · Instance culling dispatch sequence · Two-phase sequence per view · Shadow view culling realisation · Meshlet culling realisation · Transparent collection and emission · CPU reference implementation · Culling counters readback · HiZ debug overlay · Tables of the passes this file adds · Culling failure behaviour |
| `Zenith2/Docs/Design/RPROD/Views.md` | views and paths | Scope of this file · Flux_View header · Flux_ViewContext header · Flux_DepthMode header · Flux_ShadingPath header · Capability matrix data file · View registry · View list construction · Shadowed-light selection · Round-robin scheduler · Advanced multi-view · View extensions |
| `Zenith2/Docs/Design/RPROD/Renderer.md` | the renderer | Scope of this file · Flux_Renderer header · Configuration keys · Render world consumption · Render environment · Render frame sequence · Quality settings · Quality tier selection · Frame pacer |
| `Zenith2/Docs/Design/RPROD/Extractors.md` | the engine extractors and the render-instance delta | Scope of this file · Extractor files · Flux_RenderWorld header additions · Engine render components · Extractor registration · Engine extraction state · Delta tracking · Instance extractor · Light extractor · Camera extractor · Environment extractor · Full-state extraction · Determinism of extraction · Slot-full handling · Extraction counters and zones · Extractor tests · Stage placement of the extractors |
| `Zenith2/Docs/Design/RPROD/Passes.md` | passes and frames | Scope of this file · Flux_Features header · Prologue feature functions · Path frame functions · Pass ownership and files · Pass implementation rules · Shared feature input types · Async-compute flags · Present pass realisation |
| `Zenith2/Docs/Design/RPROD/PSO.md` | shaders and PSOs | Scope of this file · ZenithShaderCompiler files · Compiler pipeline · Compiler error codes · Compiler additions adopted from GEOM · Shader blob loading · Flux_PSO header · Fixed state tables · PSO table and states · PSO builder · Precache API · Precache cook step · Precache file reader · PSO creation schedule realisation · Missing-PSO fallback realisation · PSO recording in development · Shader lint implementation · PSO lints · Permutation limit |
| `Zenith2/Docs/Design/RPROD/Diagnostics.md` | diagnostics | Scope of this file · Counters · On-screen profiler rows · Log categories · Console commands and debug UI · Debug views · Frame dump front-end content · Telemetry · Profiling zones |
| `Zenith2/Docs/Design/RPROD/Tests.md` | tests | Scope of this file · Test files · Graph unit tests · View tests · Culling tests · Path and pass tests · PSO and shader tests · Lint fixtures · Frame pacing tests · Quality tier tests · Renderer tests · Render tests · Parity suites · Parity scenes · Sync validation · Benchmarks and harnesses · Device steps |

## Dependencies

### Libraries and edges

| Target | Declared direct dependencies | Third party |
|---|---|---|
| `ZenithFlux` (RPROD files) | Architecture's ZenithFlux row (`Zenith2/Docs/Architecture.md → "Library dependency graph"`): ZenithCore, ZenithGraph, ZenithJobs, ZenithProfiling, ZenithRHI, ZenithVertexFormat, ZenithAssets, ZenithShadersGenerated; `TEST_DEPENDS ZenithPathTracer ZenithRHI_Vulkan ZenithRHI_D3D12` | none; tests add the `flip` header (row RP11) |
| `ZenithShadersGenerated` | ZenithCore, ZenithVertexFormat, ZenithRHI (`KIND GENERATED`, `SHADERS Zenith2/Shaders/`) | none |
| `ZenithShaderCompiler` (host tool) | ZenithCore, ZenithVertexFormat | `slang`, `dxc` |
| the PSO cook step inside `ZenithCook` | ZenithCook's set (ASSET's tool) | none |

Flux includes no ZenithScene, ZenithECS or ZenithScript header ([Z2-N-REND-005], [Z2-N-ARCH-420]); it reads configuration through the `const Zenith_ConfigStore&` the composition root passes (a ZenithCore type), so it needs no ZenithPlatform edge.

### Headings this design relies on

| Need | Citation |
|---|---|
| the specification | `Zenith2/Docs/Rendering.md` (every skeleton heading RPROD owns); `Zenith2/Docs/Rendering/RenderGraph.md`, `Culling.md`, `FrameAnatomy.md`, `ShaderSystem.md`, `PerPass/*.md` |
| the graph core | `Zenith2/Docs/Architecture.md → "ZenithGraph shared core"`; `Zenith2/Docs/Design/GRAPH.md → "Zenith_Graph.h"`; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md → "Step 6 culling"`, "Step 2 hazard derivation per resource"; `Zenith2/Docs/Design/GRAPH/Tests.md → "The Flux builder mapping"` |
| the back end | `Zenith2/Docs/Design/RHI.md → "Division of Flux code with RPROD"`; `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` (every heading) |
| RHI API | `Zenith2/Docs/Design/RHI/PublicAPIDevice.md → "Zenith_RHIBarrier header"`, "Zenith_RHISwapchain header"; `Zenith2/Docs/Design/RHI/PublicAPI.md → "Zenith_FormatFeatures header"`, "Zenith_RHICapabilities header"; `Zenith2/Docs/Design/RHI/Recovery.md → "Frame pacing hooks"`, "Pipeline cache store"; `Zenith2/Docs/Design/RHI/Memory.md → "Flux_GPUScene"` |
| jobs and the frame graph | `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Frame structure"`, "Render extraction and pipelining"; `Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobContext"` |
| core types | `Zenith2/Docs/Design/CORE/Containers.md → "Zenith_Array"`, "Zenith_HashMap and Zenith_HashSet"; `Zenith2/Docs/Design/CORE/Maths.md → "Matrix types"`, "Zenith_Hash64"; `Zenith2/Docs/Design/CORE/Diagnostics.md → "Configuration store"`, "Structured logging" |
| geometry | `Zenith2/Docs/Design/GEOM/Meshes.md → "LOD selection"`; `Zenith2/Docs/Design/GEOM/Materials.md → "Transparency"`, "Permutation limit check and report"; `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Program stream-set specialisation"`, "The exact arithmetic kernel" |
| configuration and tiers | `Zenith2/Docs/Design/PLAT/Configuration.md → "Quality tier resolution"`, "Quality tier changes" |
| budgets | `Zenith2/Docs/Budgets.md → "Rendering rows"`, "Measured in every workload run", "Production-quality acceptance" |
| lighting | `Zenith2/Docs/Design/LIGHT.md → "Flux_LightSettings header"`, "Flux_Lighting header", "Configuration keys", "Light component schemas"; `Zenith2/Docs/Design/LIGHT/Shadows.md → "Shadowed-light budget"`, "Atlas allocation", "Flux_Shadows header"; `Zenith2/Docs/Design/LIGHT/Temporal.md → "Upscaler selection"`, "Dynamic-resolution controller"; `Zenith2/Docs/Design/LIGHT/QualityTiers.md → "Tier list"` |
| environment | `Zenith2/Docs/Design/ENV.md → "Render-world records"`, "Frame-graph placement"; `Zenith2/Docs/Design/ENV/Atmosphere.md → "Blocks ENV writes"`; `Zenith2/Docs/Design/ENV/WaterWeather.md → "Submersion test"` |
| the render world and runtime files | `Zenith2/Docs/Rendering.md → "Purpose and scope"` (headings "The render world hand-off", "Render world resync"); `Zenith2/Docs/Design/MOD.md → "ZenithRuntime files"`, "Frame-graph nodes registered by ZenithRuntime" |
| extraction inputs (the engine extractors, in ZenithRuntime) | `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Render extraction and pipelining"`; `Zenith2/Docs/Design/ECS/Scheduling.md → "Epilogue and extractors"`; `Zenith2/Docs/Design/ECS/Queries.md → "Chunk iteration contract"`, "Change-filtered queries", "Change versions and merge versions"; `Zenith2/Docs/Design/ECS/WorldAPI.md → "Zenith_ChunkView header"`, "Zenith_ECSObservers header", "Zenith_World header"; `Zenith2/Docs/Design/SCENE/Transforms.md → "Transform components"`, "Teleport", "Interpolation data"; `Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_TransformHierarchy header"`, "Zenith_LifecycleListener header"; `Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetRegistry"`; `Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_SimulationClock header"` |

LIGHT and ENV are cited by heading; ENV's revision 1 declares the environment blocks resource RPROD reads (`Zenith2/Docs/Design/ENV.md → "Render-world records"`), and a citation of LIGHT text that the render-environment ruling still has to conform sits in an `<!-- assumption: pending … LIGHT … -->` block (row RP25). Pending designs cited by path: VFX, UI, DEVT, EDIT, TWOD, SCENE, STREAM, SHIP, RESEARCH (`Zenith2/Docs/Design/<CODE>.md`); each dependency on them is an `<!-- assumption -->` block in the split file that needs it and a reconciliation row of "Reconciliation items".

### Upward-callback interfaces

| Interface | Declared in | Implemented by |
|---|---|---|
| `Flux_ViewExtension` | `ZenithFlux/Flux_ViewExtension.h` | the editor (overlays), Zenith2D (sprites), other modules (`Zenith2/Docs/Design/RPROD/Views.md → "View extensions"`) |

RPROD's engine extraction state in ZenithRuntime implements SCENE's `Zenith_LifecycleListener` (`Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_LifecycleListener header"`) for teleport and destroy events (`Zenith2/Docs/Design/RPROD/Extractors.md → "Engine extraction state"`). Flux implements `Zenith_MemoryConsumer` through RHI's `Flux_MemoryConsumer`; the pressure actions RPROD applies (graph caches, view histories, quality overrides, the round-robin interval) are called from it.

## Library and file layout

### ZenithFlux target

```cmake
# Zenith2/Engine/ZenithFlux/CMakeLists.txt
zenith_add_library(ZenithFlux DEPENDS ZenithCore ZenithGraph ZenithJobs ZenithProfiling ZenithRHI ZenithVertexFormat ZenithAssets ZenithShadersGenerated
	SUPPORTED ZENITH_GPU
	GENERATES Flux_Justified32BitFormats.generated.h Flux_CapabilityMatrix.generated.h
	TEST_DEPENDS ZenithPathTracer ZenithRHI_Vulkan ZenithRHI_D3D12)
```

RPROD owns this call; the RHI, GEOM, LIGHT and other areas add files to the library, never a second call. The two RHI backends are `TEST_DEPENDS` so the device-backed tests in `ZenithFlux_Tests` create their WARP and lavapipe devices through `Zenith_CreateRHI_D3D12` and `Zenith_CreateRHI_Vulkan`; a preset that lacks a backend (`ZenithRHI_D3D12` on `android-arm64`) links the test target without it, and the test sources select the backends present with `#if ZENITH_TEST_LINKS_ZENITHRHI_D3D12` and `#if ZENITH_TEST_LINKS_ZENITHRHI_VULKAN` (`Zenith2/Docs/Architecture/BuildSystem.md` [Z2-N-ARCH-172], cross-doc item X197, applied).

### RPROD public root headers

`Flux_RenderGraph.h`, `Flux_RasterTargets.h`, `Flux_PassContext.h`, `Flux_GraphSettings.h`, `Flux_GraphCaches.h`, `Flux_GraphErrors.h`, `Flux_View.h`, `Flux_ViewContext.h`, `Flux_DepthMode.h`, `Flux_ShadingPath.h`, `Flux_ViewExtension.h`, `Flux_Features.h`, `Flux_PSO.h`, `Flux_PSOPrecache.h`, `Flux_Renderer.h`, `Flux_Quality.h`, `Flux_Config.h`, `Flux_RenderComponents.h` (the two engine render component structs), and `Flux_RenderWorld.h` (Rendering's declaration of the render world and its writer, which RPROD's diff lands with the view-request type, the render-instance key, flags and delta, and the render environment; `Zenith2/Docs/Design/RPROD/Extractors.md → "Flux_RenderWorld header additions"`).

### RPROD internal directories

| Directory (`Zenith2/Engine/ZenithFlux/`) | Contents |
|---|---|
| `Internal/Graph/` (RPROD files) | RenderGraphFrontEnd.md "Internal graph files" |
| `Internal/Culling/` | Culling.md "Culling files" |
| `Internal/Views/`, `Internal/Paths/` (with `CapabilityMatrix.json` and its generator) | Views.md |
| `Internal/Renderer/`, `Internal/Quality/`, `Internal/Pacing/`, `Internal/Config/Quality/` (tier files and tier lists) | Renderer.md |
| `Internal/Passes/<Pass>/` (RPROD's passes; other areas' directories beside them) | Passes.md "Pass ownership and files" |
| `Internal/PSO/` (`Flux_ShaderLibrary`, `Flux_PSOTable`, `Flux_PSOBuilder`, `Flux_FixedStates.h`, `Flux_PSOPrecacheFile`, `Flux_PSORecorder`) | PSO.md |
| `Internal/Diagnostics/` (`Flux_PassTimingPublisher`, `Flux_RenderCounters`) | Diagnostics.md |
| `Tests/Graph/`, `Culling/`, `Views/`, `Paths/`, `Quality/`, `Pacing/`, `PSO/`, `Renderer/`, `Render/`, `Parity/`, `Examples/` | Tests.md "Test files" |

RPROD's ZenithRuntime files (`Zenith2/Engine/ZenithRuntime/`): `Internal/Zenith_EngineExtractors.h/.cpp`, `Internal/Extraction/` (the extraction state, the delta tracker, the four extractor bodies), `Internal/Zenith_RenderTypes.h/.cpp` and `Tests/EngineExtractors/` (`Zenith2/Docs/Design/RPROD/Extractors.md → "Extractor files"`; MOD's tables gain them by row RP26).

No RPROD file lives under `Internal/Windows|Android|Linux/`: RPROD code is platform-neutral; the shader compiler's watch mode is in `Zenith2/Tools/ZenithShaderCompiler/Internal/Windows/`.

### Other RPROD targets

`ZenithShadersGenerated` (`Zenith2/Engine/ZenithShadersGenerated/CMakeLists.txt`, the one helper call of [Z2-N-REND-108]); `ZenithShaderCompiler` (PSO.md "ZenithShaderCompiler files"); `Zenith_Bench_Budgets_RPROD`, `Zenith_Bench_ShadingPaths`, `Zenith_Bench_TBDR` (`zenith_add_benchmark`); the workload games `Zenith2/Games/WorkloadIndoor/` and `Zenith2/Games/WorkloadOutdoor/` (`zenith_add_game`, no C++).

### Supported matrix

| Target | `SUPPORTED` |
|---|---|
| `ZenithFlux`, `ZenithShadersGenerated` | `ZENITH_GPU` (excludes `linux-tsan`, [Z2-N-ARCH-173]) |
| `ZenithShaderCompiler` | `ZENITH_TOOLS_HOST` |
| `Zenith_Bench_Budgets_RPROD`, `Zenith_Bench_ShadingPaths` | `win`, `android-arm64` |
| `Zenith_Bench_TBDR` | `android-arm64`, `win` (smoke on WARP) |
| `ZenithFlux_Tests` | registered in `win`, `win-asan` (built on Android, never run there) |

## Public API

### Header summary

| Header | Key types | Detail |
|---|---|---|
| `Flux_RenderGraph.h`, `Flux_RasterTargets.h`, `Flux_PassContext.h`, `Flux_GraphSettings.h`, `Flux_GraphCaches.h`, `Flux_GraphErrors.h` | the graph | RenderGraphFrontEnd.md, the headings of the same names |
| `Flux_View.h`, `Flux_ViewContext.h`, `Flux_DepthMode.h`, `Flux_ShadingPath.h`, `Flux_ViewExtension.h` | views and paths | Views.md |
| `Flux_Features.h` | every per-view feature function and the path functions | Passes.md "Flux_Features header" |
| `Flux_PSO.h`, `Flux_PSOPrecache.h` | PSO keys, the precache API | PSO.md "Flux_PSO header", "Precache API" |
| `Flux_Renderer.h`, `Flux_Quality.h`, `Flux_Config.h` | the renderer, tiers, keys | Renderer.md |
| `Flux_RenderWorld.h`, `Flux_RenderComponents.h` | the render-instance key, flags and delta, the render environment, the engine render components | Extractors.md "Flux_RenderWorld header additions", "Engine render components"; Renderer.md "Render environment" |

### Rules across the API

- Every fallible function returns `Zenith_Result<T, E>` with an RPROD error enumeration numbered from 1 ([Z2-N-CONTRACT-465]); graph declaration calls never fail visibly and report at compile.
- Every public type carries one contract block ([Z2-N-CONTRACT-111]); generated headers carry theirs ([Z2-N-CONTRACT-114]).
- No public header includes a Vulkan, D3D12, Slang or DXC header ([Z2-N-CONTRACT-043]); pass contexts expose no command list ([Z2-N-REND-300]).
- `Flux_Renderer` and `Flux_ViewRegistry` are concrete types created by static factories; `Flux_ViewExtension` is the one RPROD interface ([Z2-N-CONTRACT-035]).

### Handle kinds used

| Kind | Types |
|---|---|
| frame-scoped graph handles | `Flux_RGTexture`, `Flux_RGBuffer`, `Flux_RGPass`, view values in parameter fields ([Z2-N-CONTRACT-095]) |
| device handles | `Flux_TextureHandle`, `Flux_BufferHandle`, `Flux_PipelineHandle` (RHI's aliases) |
| operation handles | PSO group requests (`Flux_PSOPrecacher::RequestGroup`), readbacks (RHI's) |
| identities, not handles | `Flux_ViewID`, `Zenith_RHIShaderProgramID`, `Zenith_RHIPermutationKey`, bucket indices, draw keys |

## Data structures and formats

### Data structure index

| Structure | Bytes | Specified in |
|---|---|---|
| `Flux_RGResourceRecord`, `Flux_RGViewRecord`, `Flux_RGPassRecord`, `Flux_RGAccessRecord` | 96, 32, 128, 40 | RenderGraphFrontEnd.md "Setup tables" |
| `Flux_CompiledGraph`, `Flux_CompiledPass`, `Flux_ResourceLifetime`, `Flux_PlacedTransient` | arena spans | RenderGraphFrontEnd.md "Flux_CompiledGraph" |
| `Flux_BarrierState`, `Flux_BarrierRecord` | 12, 40 | RenderGraphFrontEnd.md "Step 5 barrier synthesis realisation" |
| `Flux_GraphErrorList` | 24 | RenderGraphFrontEnd.md "Flux_GraphErrors header" |
| `Flux_SlotCommandRecord`, `Flux_CullSurvivor`, `Flux_CullCounters`, `Flux_TransparentEntry` | 16, 8, 32, 16 | Culling.md "Culling records" |
| `Flux_BucketKey` | 48 | Culling.md "Bucket registry" |
| `Flux_PSOKey` | 56 | PSO.md "Flux_PSO header" |
| `Flux_RenderInstanceDelta`, `Flux_RenderInstanceKey` | 184, 8 | Extractors.md "Flux_RenderInstanceDelta layout", "Instance keys" |
| `Flux_ViewPersistentState` resources | per kind | Views.md "Persistent state by kind" |

### File formats

| File | Format | Specified in |
|---|---|---|
| `.zshader` container | versioned binary; this design defines the permutation `flags` bits | `Zenith2/Docs/Rendering/ShaderSystem.md → "Shader blob container"`; PSO.md "Shader blob loading" |
| `pso_precache.zbin`, `recorded.psoset.json`, `<Game>.misses.psoset.json` | as the specification | ShaderSystem.md "PSO precache sets"; PSO.md "Precache cook step" |
| reflection JSON, material reflection JSON, lint JSON, reload journal | as the specification and GEOM | ShaderSystem.md "Reflection JSON", "Watch mode and editor reload" |
| `CapabilityMatrix.json` | schema version 1 | Views.md "Capability matrix data file" |
| `Justified32BitFormats.json` | schema version 1 | RenderGraphFrontEnd.md "Compile-time rule checks" |
| quality tier files `<TierID>.config.json`, tier lists | PLAT's configuration formats | Renderer.md "Quality settings" |
| `zenith2.flux-graph-dump` version 1 | front-end keys | RenderGraphFrontEnd.md "Front-end frame dump keys" |
| parity `Tolerances.json`, parity scenes | JSON | Tests.md "Parity suites", "Parity scenes" |

Every binary format is little-endian and serialized field by field (Z2-D-038).

## Algorithms and state machines

### Algorithm index

| Algorithm | Heading |
|---|---|
| setup and the field-table walk | RenderGraphFrontEnd.md "Field-table walk" |
| the compile driver | RenderGraphFrontEnd.md "Compile driver" |
| culling and clear folding (step 1) | RenderGraphFrontEnd.md "Step 1 culling and clear folding" |
| ordering (step 2), the first-write check | RenderGraphFrontEnd.md "Step 2 ordering through ZenithGraph", "First-access widening" |
| aliasing placement (step 4), caches | RenderGraphFrontEnd.md "Step 4 placement realisation", "Physical resource caches" |
| barrier synthesis (step 5), splits, the verifier | RenderGraphFrontEnd.md "Step 5 barrier synthesis realisation", "Split-barrier candidates", "Barrier verifier" |
| pass merging | RenderGraphFrontEnd.md "Pass merging algorithm" |
| two-phase culling, compaction, the HiZ build | Culling.md "Instance culling dispatch sequence", "Two-phase sequence per view", "HiZ build dispatch" |
| view list, shadowed lights, round robin, depth mode | Views.md "View list construction", "Shadowed-light selection", "Round-robin scheduler", "Flux_DepthMode header" |
| quality tier selection | Renderer.md "Quality tier selection" |
| the frame pacer | Renderer.md "Frame pacer" |
| the missing-PSO fallback per build type | PSO.md "Missing-PSO fallback realisation" |
| permutation enumeration and the limit | PSO.md "Compiler pipeline", "Permutation limit" |

### State machines

| Machine | Heading |
|---|---|
| PSO entry (`Flux_PSOState`: `READY`, `PENDING`, `ABSENT`, `FAILED` with a `Flux_PSOFailReason` `BLOB` or `DEVICE`; `FAILED` per [Z2-N-REND-208]) | PSO.md "PSO table and states" |
| extraction delta tracking (tracked, held, moving, pending removal) | Extractors.md "Delta tracking" |
| frame pacer (`WARMUP`, `PACING`, `SUSPENDED`) | Renderer.md "Frame pacer" |
| view persistent state (created, valid, invalid, retired) | Views.md "View registry" |

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| `Flux_RenderGraph`, `Flux_RasterTargets`, feature functions, `Flux_ViewRegistry`, `Flux_BucketRegistry` | `OWNER_THREAD` (the Flux.BuildGraph node) |
| `Flux_CompiledGraph` | `IMMUTABLE` after compile; read by record jobs |
| `Flux_RasterContext`, `Flux_ComputeContext` | `OWNER_THREAD` (one record job) |
| `Flux_GraphCaches` | `OWNER_THREAD`; the imported-state commit on the render-submit thread, sequenced after the build node |
| `Flux_PSOTable` | readers lock-free from record jobs; writers creation jobs and the build node |
| `Flux_PSOPrecacher` | `ANY_THREAD` |
| `Flux_FramePacer` | `RENDER_SUBMIT_THREAD` |
| `Flux_Renderer` | as its contract block (Renderer.md "Flux_Renderer header") |

### Frame-graph nodes and declared access

| Node (`FRAME_SUBGRAPH_RENDER`) | Affinity | RPROD work |
|---|---|---|
| `Flux.BuildGraph` | `JOB_AFFINITY_ANY` | render-world consumption, view list, draw keys, setup, compile steps 0–2, 4, 5, merging, 8–10 |
| `Flux.Record` | `JOB_AFFINITY_ANY`, `ParallelFor` over partitions | pass bodies and contexts (never on the render-submit thread, cross-doc item X53) |
| `Flux.Submit` | `JOB_AFFINITY_RENDER_SUBMIT` | none (RHI's) |
| `Flux.Present` | `JOB_AFFINITY_RENDER_SUBMIT` | the frame pacer and the latency record |

Four RPROD nodes run outside the render subgraph, per world: the engine extractors (`Zenith2/Docs/Design/RPROD/Extractors.md`), each a `FRAME_SUBGRAPH_EPILOGUE` stage-12 node of phase 12, registered by ZenithRuntime's attach step through `Zenith_WorldAttachContext::AddExtractor` after the modules' extractors, each declaring reads only and writing only its own `Flux_RenderWorldWriter` partition ([Z2-N-ARCH-419]), and none declaring an ordering: `Zenith_WorldSimulation::EmitInto` supplies `Before("Render.Seal@W0", true)` for each (Q40 ruled (a)).

| Node (`FRAME_SUBGRAPH_EPILOGUE`, stage 12) | Declared reads | Writes |
|---|---|---|
| `Render.ExtractInstances` | `Zenith.Render.MeshInstance` 1–15, `Zenith.WorldTransform` 1–7, `Zenith.PreviousWorldTransform` 1–3 | instance deltas, history resets |
| `Render.ExtractLights` | LIGHT's five light components, the two transform components | light deltas |
| `Render.ExtractCameras` | `Zenith.Render.Camera` 1–15, the two transform components, `Zenith.Lighting.CameraLighting` (optional) | view requests |
| `Render.ExtractEnvironment` | `Zenith.Lighting.WorldLighting` 1–7, ENV's `PHASE_INPUT` resource `Zenith.Environment.RenderBlocks` when registered | the world's one `SetEnvironment` |

The render subgraph of iteration k reads only the sealed render world k−1 and runs concurrently with simulation k ([Z2-N-ARCH-385], Z2-D-050); the node bodies are registered by ZenithRuntime (`Zenith2/Docs/Design/MOD.md → "Frame-graph nodes registered by ZenithRuntime"`). Background jobs: PSO creation (high priority for the boot group, low otherwise), shader-container loads, dump writes.

### Lock-order entries

RPROD adds no lock. The PSO table and the bucket registry are lock-free or single-threaded as above; RHI's row 480 guards the persistent-resource bookkeeping RPROD calls.

### Cancellation and shutdown

PSO group requests are operation handles: destroying one cancels its group's creations no other request shares, each creation job returning at its next check ([Z2-N-ARCH-425]). At shutdown step 2 the in-flight render subgraph completes; the renderer is destroyed in step 6 after every queue went idle (cross-doc item X76): PSO jobs are drained, the view registry and graph caches release through the retirement queue, the frame pacer holds nothing.

### Error enumerations

| Enumeration | Header | Enumerators |
|---|---|---|
| `Flux_RGError` | `Flux_GraphErrors.h` | RenderGraphFrontEnd.md "Flux_GraphErrors header" |
| `Flux_RecordCheck`, `Flux_RGInfo` | `Flux_GraphErrors.h` | as above (`Flux_RecordCheck` 4 is `FLUX_RECORD_CHECK_DRAW_PARAMS_EXCEEDED`, [Z2-N-REND-496]) |
| `Flux_ViewError` | `Flux_View.h` | `KIND_UNAVAILABLE`, `INVALID_REQUEST`, `DEPENDENCY_CYCLE`, `REGISTRY_FULL`, `OUT_OF_MEMORY` |
| `Flux_RendererError` | `Flux_Renderer.h` | `OUT_OF_MEMORY`, `DEVICE`, `SHADER_LIBRARY`, `PRECACHE_FILE`, `CONFIGURATION`, `EXTENSION_LIMIT` |
| `Flux_PSOError` | `Flux_PSOPrecache.h` | `UNKNOWN_GROUP`, `QUEUE_FULL`, `DEVICE_LOST` |
| `Flux_ShaderError` | `Internal/PSO/Flux_ShaderLibrary.h` | `CONTAINER_VERSION`, `CORRUPT_BLOB`, `MISSING_PROGRAM`, `MISSING_PERMUTATION`, `IO` |
| `Zenith_ZscError` | the compiler's internal header | PSO.md "Compiler error codes" |

Each has its `Zenith_ToString` overload ([Z2-N-CONTRACT-466]); errors crossing into RHI or GEOM types are mapped at the boundary ([Z2-N-CONTRACT-463]).

### Luau errors and asserts

RPROD exposes no Luau binding; script reaches rendering through components that extractors copy. A graph compile error is a programmer error (logged, then `ZENITH_CHECK` in development builds); cooked input (containers, the precache file, tier files) is untrusted and produces results and logs, never asserts ([Z2-N-CONTRACT-472]).

## Diagnostics and profiling

`Zenith2/Docs/Design/RPROD/Diagnostics.md` lists every counter, profiler row, log category, console command, debug view, dump entry, telemetry field and zone. In summary: per-pass GPU timestamps feed DEVT's GPU timeline; culling counters per view and phase; transient memory with and without aliasing and the savings; PSO creation, cache and miss counters; pacing interval, deviation and latency; the frame dump's front-end keys; the HiZ overlay.

## Authoring and cooking

RPROD has no editor surface of its own: the editor viewports, the debug UI and the material previews are EDIT's (`Zenith2/Docs/Design/EDIT.md`, pending), which reach views through `Flux_ViewRequest` and passes through `Flux_ViewExtension`. RPROD's authored and cooked artefacts are:

| Artefact | Authored | Cooked or generated |
|---|---|---|
| Slang programs of RPROD's passes | code under `Zenith2/Shaders/Passes/` | `ZenithShaderCompiler` at build time (PSO.md "Compiler pipeline") |
| `recorded.psoset.json` per game | recorded by development runs, committed | the PSO cook step (PSO.md "Precache cook step") |
| quality tier files and tier lists | JSON under `Internal/Config/Quality/` | cooked into `content:/Engine/Config/Quality/` by ASSET's configuration merge |
| `CapabilityMatrix.json`, `Justified32BitFormats.json` | JSON with the Rendering tables | generated headers at build time |
| parity scenes | JSON under `Tests/Parity/Scenes/` | loaded by the parity runner |

Migration: the recorded PSO set is an authoring document (schema version, unknown fields preserved, [Z2-D-048]); the other files are engine data changed with the code.

## Failure handling

| Failure | Behaviour |
|---|---|
| a graph compile error | every diagnostic logged; `ZENITH_CHECK` in development builds; Shipping skips the frame, counts `Flux.Graph.CompileFailures`, keeps presenting the last image |
| an arena or table capacity breach | `FLUX_RG_ERROR_CAPACITY` naming the table; the frame fails as above |
| a transient heap or placed-resource allocation failure | RHI's device-memory exhaustion response runs (`Zenith2/Docs/Design/RHI/Memory.md → "Device memory exhaustion"`), then `FLUX_RG_ERROR_CAPACITY` |
| a missing PSO | the fallback per build type (PSO.md "Missing-PSO fallback realisation"); never a synchronous creation |
| a corrupt blob or a missing permutation | `FLUX_PSO_STATE_FAILED` with reason `BLOB` ([Z2-N-REND-208]): logged once, the `FAILED` fallback, never re-queued for the run; a shader reload returns it to `ABSENT` |
| a device-side PSO creation failure (`PIPELINE_CREATE`, `SHADER_BLOB_INVALID`) | `FLUX_PSO_STATE_FAILED` with reason `DEVICE`: logged once, the `FAILED` fallback, never re-queued on the same device; device-loss recovery returns it to `ABSENT` (a recreated device or updated driver may create it); no in-device retry (PSO.md "PSO table and states") |
| a missing or refused shader container or precache file | `FLUX_RENDERER_ERROR_SHADER_LIBRARY` / `PRECACHE_FILE` at creation in Shipping (startup exit code 4); in development the misses are recorded |
| an invalid view request | dropped, logged once per owner, counted |
| a view registry at capacity | the view renders with an invalid history |
| an out-of-range quality value | the previous value is kept and logged |
| a bucket registry or draw-key overflow | the slot is not drawn and counted |
| a meshlet overflow | the fallback of Culling.md "Culling failure behaviour" |
| a frame whose upload need (transparent entries and every other build-node frame data) the ring cannot serve within the cap | RHI's frame deferral (`Zenith2/Docs/Design/RHI/Memory.md → "Frame deferral"`); nothing of the frame is written (Culling.md "Build-node upload data") |
| a frame whose upload need exceeds `Z2-B-RHI-006` (a view's transparent entries, which have no cap of their own, [Z2-N-REND-660]) | RHI's frame failure (`Zenith2/Docs/Design/RHI/Memory.md → "Frame failures"`): `FLUX_FRAME_FAILURE_UPLOAD_OVER_CAP`, nothing recorded, submitted or presented, RHI's log naming the owner `Flux.Transparent`, RPROD's `flux.culling` warning naming the view; 30 consecutive failed frames exit with `EXIT_CODE_DEVICE_LOST` (6) and "The renderer could not fit a frame's uploads into memory." (Culling.md "Transparent collection and emission") |
| a render-world record refused with `FLUX_RENDER_WORLD_ERROR_SLOT_FULL` | counted in `Flux.RenderWorld.Dropped` by the writer; an engine extractor makes one refused call per frame, keeps every remaining record for the next frame, rotates each write list's start so persistent overflow starves no record, and never asserts (Extractors.md "Slot-full handling") |
| an unresolvable mesh or material of a renderable entity, an invalid light, an invalid camera lens | the entity is held, retried only when the resolver's availability epoch moves (never in a headless run), or refused and counted, and logged once (Extractors.md "Delta tracking", "Instance extractor", "Light extractor", "Camera extractor") |
| a latency-slot timeout | logged once per second; the frame continues |
| device loss | RHI's recover-or-exit policy (`Zenith2/Docs/Design/RHI/Recovery.md → "Recover-or-clean-exit policy"`); RPROD rebuilds the PSO table, the view registry histories (invalid) and the bucket and draw-key tables from the full-state resync |
| cancellation of a PSO group request | its creations stop at their next check; nothing leaks |
| a budget breach | the row's breach class: GPU pass rows BR-ACCEPT, `Z2-B-RPROD-013`/`-022`/`-023` BR-DEFER, `-024` BR-COOK, `-025` BR-ACCEPT, `-029` BR-FALLBACK, `-031` BR-TEST, memory rows BR-GOVERNOR |

## Test strategy

### Test targets and gate steps

| Target or suite | Gate step | Presets |
|---|---|---|
| `ZenithFlux_Tests` (graph, culling, views, paths, quality, pacing, PSO, renderer, render, aliasing and async parity) | `Test` | `win-debug`, `win-release` on WARP and lavapipe |
| the same | `Sanitizers -Preset win-asan` | stage acceptance and `Zenith2Memory` diffs |
| `ZenithShaderCompiler_Tests`, lint fixtures | `Test`, `ShaderChecks` | `win` |
| `ZenithRuntime_Tests` (`Tests/EngineExtractors/`, the engine extractors, row RP26) | `Test`; `Sanitizers -Preset win-asan` at stage acceptance | `win-debug`, `win-release` (headless worlds, no GPU; the fixture resolver of `Zenith2/Docs/Design/RPROD/Extractors.md → "Extraction resolver"` resolves meshes and materials, which the headless asset registry never does) |
| `ZenithCook_Tests` PSO cases | `Test` | `win` |
| backend parity cases | `BackendParity` (Stage 3) | `win-release` |
| path parity cases | `PathParity` (Stage 5) | `win-release` |
| the three harness smoke runs | `Test` | `win` |
| the Flux graph conformance builder (GRAPH's scenarios through RHI's test access) | `Test` (`ZenithGraph_Conformance`) | `win` |

`Zenith2/Docs/Design/RPROD/Tests.md` names every test and the case it asserts. Every GPU test runs with synchronization validation and GPU-based validation clean (Tests.md "Sync validation").

### Required test groups

The graph unit tests including the barrier-blocked merge (`Test_RenderGraph_MergeBlockedByBarrier`); every graph error fixture; the render test in which a fully occluded object records zero draws (`Test_HiZ_FullyOccludedZeroDraws`, `Test_Render_FullyOccludedZeroDraws`); aliasing on/off bit-identical parity with poisoning (`Test_AliasingParity_<Scene>`); every lint's failing fixture; backend parity from Stage 3 and path parity from Stage 5; the frame-pacing tests.

## Budget rows

### Rows RPROD owns

| Row | Status | How RPROD meets or measures it |
|---|---|---|
| Z2-B-RPROD-001 GPU scene tables | pending | RHI's `GetTableBytes`; RPROD's draw-key buffers count here |
| Z2-B-RPROD-002 PSO cache and pipeline objects | pending | the PSO table and driver pipeline memory |
| Z2-B-RPROD-003 Render extraction CPU | pending | the per-frame sum over every world of the four engine extractor node zones `Render.ExtractInstances`, `Render.ExtractLights`, `Render.ExtractCameras`, `Render.ExtractEnvironment` (Diagnostics.md "Profiling zones") |
| Z2-B-RPROD-004 GPU-scene scatter upload GPU | pending | the scatter pass timestamps (RHI's pass, RPROD's row) |
| Z2-B-RPROD-005, -008 Instance culling phase 1 and 2 GPU | pending | the cull passes |
| Z2-B-RPROD-006 Depth velocity normal prepass GPU | pending | both prepass phases |
| Z2-B-RPROD-007 HiZ pyramid build GPU | pending | each build (twice per view) |
| Z2-B-RPROD-009, -010 Forward opaque, Transparent forward GPU | pending | the passes |
| Z2-B-RPROD-011, -012, -013 per-object lights, per-object opaque, per-object light cap | pending | the Forward path; the cap is `render.quality.perObjectLightCap` |
| Z2-B-RPROD-014 to -019 G-buffer, meshlet culling, visibility raster, classification, resolve, velocity | pending | the Stage 5 passes |
| Z2-B-RPROD-020 Frame pacing present-interval deviation | pending | the pacer's deviation (MP-FRAME) |
| Z2-B-RPROD-021 CPU render lag | adopted (1 frame) | the pipelining structure |
| Z2-B-RPROD-022, -023 view caps per tier | pending | the view list |
| Z2-B-RPROD-024 Shader permutation limit per game | pending | the compiler and the cook |
| Z2-B-RPROD-025 Missing-PSO events per qualification run | adopted (0) | the PSO miss counters |
| Z2-B-RPROD-026 HiZ mid-frame depth store and reload bandwidth | pending | the S24 harness |
| Z2-B-RPROD-027 Input-to-photon latency | pending | the latency attribution |
| Z2-B-RPROD-028 GPU frame allocation headroom | pending | MP-ALLOC over every GPU row |
| Z2-B-RPROD-029 D24 depth-precision tolerance | pending (proposed 0.25 m) | `r.depth.precisionToleranceMetres` |
| Z2-B-RPROD-030 On-disk pipeline-cache size | pending | MP-DISKCACHE (RHI's store enforces the cap) |
| Z2-B-RPROD-031 Per-scene parity tolerances | pending | the parity `Tolerances.json` |

Every pending row's producer is the Stage 1 per-system budget ticket for RPROD, blocked by the RPROD harness and by the CORE and JOBS rulings; every other area's ruling is blocked by RPROD's, because MP-ALLOC checks the GPU headroom row RPROD adopts ([Z2-N-BUDGET-023]).

### Rows RPROD is measured against

`Z2-B-SHIP-001`, `-002` frame time; `Z2-B-SHIP-011` dynamic-resolution floor; `Z2-B-SHIP-015` to `-018` hitch rows (cold PSO caches count, [Z2-N-BUDGET-065]); `Z2-B-RHI-001` persistent render targets and view histories; `Z2-B-RHI-004` graph build and compile CPU; `Z2-B-RHI-005` recording and submit CPU; `Z2-B-RHI-010` transient memory peak; `Z2-B-EDIT-003` shader blob reload; `Z2-B-LIGHT-002` shadow views total GPU (the shadow cull part); `Z2-B-PROC-024`, `-027`, `-029` the ShaderChecks, BackendParity and PathParity step timeouts.

Two pending rows other areas propose measure RPROD passes: "VT branch cost in VT-free material passes (S24)" (ADVGFX's, an MP-GPUPASS A/B of `ForwardPlusOpaque`, cross-doc item X422) and "Video conversion and composite GPU" (CINE's, MP-GPUPASS over `VideoConvert` and `VideoComposite`, cross-doc items X294 and X455); neither is RPROD's to allocate.

### Pending row needs

None beyond the rows above: every fixed capacity of this design (65,535 buckets, 256 persistent views, 128 views per frame, 32 extensions, 64 pending PSO group requests, the graph limits) is derived in its contract from bounded inputs ([Z2-N-CONTRACT-342]).

### Measurement harness

`Zenith_Bench_Budgets_RPROD` (`Zenith2/Benchmarks/Budgets/RPROD/`, the Stage 1 per-system budget harness for RPROD) measures on the RTX 3070 and the S24, 10 repetitions, medians with 95% confidence intervals, raw data under `Zenith2/Docs/Research/Budgets/RPROD/`: the reference engine's Flux culling, prepass and forward passes (Budgets' basis for RPROD), a synthetic GPU-driven scene at W-Indoor and W-Outdoor scale for the pass rows, the D24 far step of each workload view, the PSO counts and pipeline memory of the workloads' predicted sets, the pacing deviation of a paced empty frame, the S24 store-and-reload bandwidth, the four engine extractor zones per world (`Z2-B-RPROD-003`), the transparent entry bytes per view of W-Indoor and W-Outdoor (`16 × n`, the largest view and the frame total), which the RHI budget ticket for `Z2-B-RHI-006` Upload ring hard cap reads as an input, and the full-state render-world bytes of W-Indoor and W-Outdoor: 184 bytes per instance delta, 112 per light delta and the view requests of the frame, the engine extractors' part of a full-state world (18.4 MB of instance deltas alone at W-Outdoor's 100,000 instances). A full-state world is the sizing case of the render-world slot arenas ([Z2-N-REND-017]), so those bytes are an input of the budget ticket for `Z2-B-CORE-005` Frame and graph arenas; `--smoke` runs one repetition in the `Test` step.

## Stage plan

### Stage 1

| Ticket | Kind | Blocks |
|---|---|---|
| the RPROD budget harness | harness | the RPROD budget ruling |
| the Stage 1 per-system budget ticket for RPROD | budget (needs-human) | every ticket citing a pending RPROD row; every other area's per-system ruling |
| the Stage 1 workload scale tickets for W-Indoor and W-Outdoor | decision (needs-human) | the workload games |
| D-SLANG (shared with RHI) | decision (needs-human) | the generated resource fields |
| the RPROD Stage 2 revalidation (RPROD, Stage 2, {D-SLANG, D-SEL-HASH}) | revalidation (needs-human) | Stage 2 RPROD tickets depending on them |

### Stage 1 pull-forward and API-only slices

Pulled into the Stage 1 sprint with fix version `Z2 Stage 2`: the API-only graph headers (`Flux_RenderGraph.h`, `Flux_RasterTargets.h`, `Flux_PassContext.h`, `Flux_GraphSettings.h`, `Flux_GraphErrors.h`) with their contract blocks and compile-only tests; `Flux_View.h`, `Flux_DepthMode.h` with `Flux_SelectDepthMode` and its analytic tests; `Flux_PSO.h` and the fixed state tables; the capability matrix data and generator. Everything that hashes (view IDs, PSO key hashes, program IDs) waits for D-SEL-HASH (open, Stage 1).

### Stage 2

The render graph front end complete (setup, steps 1, 2, 4, 5, merging, checks, poisoning, timestamps, contexts, record checks, dump keys) with its unit tests and aliasing parity; `ZenithShaderCompiler` (pipeline, headers, shared types, container, lints, `--check`), `ZenithShadersGenerated`, the runtime shader library; PSO keys and the builder; basic views (identity, registry, histories, depth mode, view list); `Flux_Renderer` creation and the frame sequence; the render-world additions (the instance key, flags and delta), the engine render components and the instance and camera extractors (`Zenith2/Docs/Design/RPROD/Extractors.md → "Stage placement of the extractors"`), without which the Stage 2 renderer draws no instance through the hand-off; the stream-set compiler surface GEOM needs; the GPU rule checks of Z2-I-RPROD-25, -27, -28, -32. Gate: the base list plus ShaderChecks.

### Stage 3

Forward+ on Vulkan, D3D12 and the S24 (Z2-I-RPROD-21) with backend parity in the gates (a PROC ticket activates BackendParity); two-phase HiZ with shadow-view HiZ, compaction with draw keys, the bucket registry, transparent commands; PSO precaching (cook step, file, schedule, API), the missing-PSO fallback, PSO recording; frame pacing and latency attribution; the permutation limit; the default quality tier per platform; view extensions, shadowed-light selection, caps and round robin; the early-Z, attachment, half-precision and back-face lints; the workload games W-Indoor and W-Outdoor; watch mode; the light and environment extractors; the procedural records' consumption and the null vertex stream binding. The D-VTX-TANGENTLESS (open, Stage 3) harness is GEOM's (Z2-R-GEOM-133 and -134): RPROD's Forward+ opaque, G-buffer and resolve programs accept its development-only variant, and no default changes.

### Stage 4

No RPROD inventory item; RPROD supports the VT pool consumers and streaming-cell PSO groups that STREAM and ADVGFX land.

### Stage 5

Forward, Deferred and visibility-buffer frames with meshlet culling (Z2-I-RPROD-06, -22 to -24); path parity (a PROC ticket activates PathParity); advanced multi-view (Z2-I-RPROD-09); per-platform quality tiers (Z2-I-RPROD-20); the shading-path research harness for RESEARCH's report; the D-TBDR harness and ruling.

### Stage 6 and Stage 7

Stage 6: the physical input-to-photon confirmation and the qualification runs' zero-miss evidence. Stage 7: `VIEW_KIND_STEREO` as a two-view group (SCOPE, Z2-D-072).

### Core-doc dependencies of the stage plan

Every core-doc item this stage plan depended on is applied: X94, X97 to X105, X108, X125, X127, X128, X134, X135, X138, X141 and X144 in core-doc batch 4, X178, X183 and X185 in core-doc patch 2026-10-02b, X195, X197, X198, X209 and X230 in core-doc patch 2026-10-03c, and X250 (the Forward path's program names, `Passes/Forward/ForwardOpaquePerObject.slang`, and Forward+ opaque's `Flux_OpaqueParams`) in core-doc patch 2026-10-03d (X103's remaining rows wait on X106, which no RPROD design text uses). No RPROD ticket is held by a core-doc item. The cross-area rows that remain open (RP21, RP24, RP26, RP27, RP28, RP33) are the area-producer needs the tickets name; their blocks below stay fenced.

## Decisions

### Settled decisions governing the area

| Decision | Effect here |
|---|---|
| Z2-D-014 Universal GPU performance rules and their lints | the checks, lints and fixtures |
| Z2-D-025 Offline shader compilation; PSOs precached | the compiler, the library, precaching, the fallback |
| Z2-D-035 Clustered forward+ by default | the four paths and the capability matrix |
| Z2-D-036 No tile-local merging until D-TBDR | merging never reads tile-locally |
| Z2-D-054 Fully bindless binding model | draw IDs, bucket record bases, the push constant |
| Z2-D-055 The render graph rebuilt per frame | the arena graph, single declaration, feature functions |
| Z2-D-056 Automatic barrier synthesis | step 5, no barrier API |
| Z2-D-057 Transient aliasing with a toggle | step 4, the first-write rule, poisoning |
| Z2-D-058 GPU scene tables | draw keys over the tables |
| Z2-D-059 Render views | identity, histories, kinds |
| Z2-D-060 Two-phase HiZ | the pyramid and the culling sequence |
| Z2-D-061 Depth mode | `Flux_SelectDepthMode` |
| Z2-D-062 Vertex data | generated layout tables in the PSO builder |
| Z2-D-063 Validation roles | the parity suites |
| Z2-D-064 Research methodology | the shading-path harness |
| Z2-D-005, Z2-D-012, Z2-D-015, Z2-D-050, Z2-D-072, Z2-D-084, Z2-D-085, Z2-D-097 | profiler data, budgets, generated structs, pipelining, XR readiness, harness plus ruling, living docs, rulings in the ticket's diff |

### Open decisions

D-SLANG (open, Stage 1): the Vulkan `DescriptorHandle<T>` lowering; RPROD's generated fields use the D-SLANG-neutral surface until it rules ([Z2-N-REND-970]). D-SEL-HASH (open, Stage 1): view IDs, program IDs, PSO key hashes, cache keys. D-VTX-TANGENTLESS (open, Stage 3): GEOM's ruling; RPROD's programs host the variant. D-TBDR (open, Stage 5): owned here (below).

### Selection decisions RPROD owns

D-TBDR (open, Stage 5) is owned here. Its authority is the open-decision entry `Zenith2/Docs/DecisionLog.md → "Open decisions"` (heading "D-TBDR"): its alternatives, its acceptance rule and its harness-and-ruling split (Z2-D-084) govern, and this section only maps them onto what `Zenith_Bench_TBDR` measures on the S24.

<!-- elaboration -->
| DecisionLog alternative | Measured configuration of the Forward+ S24 frame |
|---|---|
| (1) keep no tile-local merging | the current design: every merge of "Pass merging algorithm" and no tile-local read; the baseline |
| (2) tile-local reads for specific pass pairs on Android | `ForwardPlusOpaque`, `SkyAtmosphere` and `TransparentForward` in one rendering scope, the second and third passes reading colour and depth tile-locally through `VK_KHR_dynamic_rendering_local_read` where the Adreno driver exposes it |
| (3) tile-local merging wherever the graph finds a compatible pair | "Pass merging algorithm" with condition 5 relaxed so that a same-pixel attachment read between two adjacent compatible raster passes becomes a tile-local read instead of a blocking barrier, applied to every such pair of the frame; on the Forward+ frame this adds prepass phase 2 kept on tile ahead of the opaque scope, with the final HiZ build (a compute pass, which cannot join a rendering scope) moved after the merged scope |

| Element | Content |
|---|---|
| criteria | DecisionLog's: each configuration measured on the S24 on W-Indoor and W-Outdoor under the research procedure (controlled conditions, at least 10 repetitions, 95% confidence intervals): median main-view GPU frame time with its interval, backend-parity and path-parity images against (1), sync validation; the Adreno bandwidth counters and the two-phase culling store-and-reload row `Z2-B-RPROD-026` reported as evidence |
| acceptance rule | DecisionLog's, verbatim: a configuration (2) or (3) passes only when all of these hold against (1): its median main-view GPU frame time is at least 5% lower than (1)'s; its 95% confidence interval and (1)'s do not overlap; its backend-parity and path-parity images are bit-identical to (1)'s; and sync validation is clean. Among passing configurations the one with the lowest median GPU frame time is adopted; when none passes, (1) stays. Bandwidth is reported evidence and is not itself an acceptance criterion. The harness reports one verdict under this rule, and the ruling is appended to `Zenith2/Docs/DecisionLog.md` in the ruling ticket's diff (Z2-D-097) |

The rule above is the one the DecisionLog entry carries since cross-doc item X102 was applied (batch 4); there is no second verdict.

### Rulings this design records

<!-- elaboration -->
Recorded by the tickets that land them: the draw key (material slot, LOD) as the compaction unit ([Z2-N-REND-650], X99 applied); every GPU transient declared to the core per subresource (`GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD_PER_SUBRESOURCE`, [Z2-N-ARCH-346], row RP16), with `Test_RenderGraph_FirstWriteAddsNoEdge` proving first writes add no ordering to any declared frame; split candidates demoted by the back end ([Z2-N-REND-393], [Z2-N-REND-394], X98 applied); `FLUX_RG_ERROR_DEPTH_WRITABLE_UNUSED` detected at record (X101 (e) applied); `FLUX_PSO_STATE_FAILED` with its two reasons, a blob failure standing for the run and a device failure cleared by device-loss recovery with no in-device retry ([Z2-N-REND-208], X101 and X144 applied); the engine render components `Zenith.Render.MeshInstance` and `Zenith.Render.Camera` declared with the extractors (Q48 ruled (a)); the extractors' own change reference (`GetChangeVersion()` at the end of the previous run, a version at or newer than it counting as changed so out-of-tick writes are seen, discarded whenever `GetPreviousReadVersion()` is 0) over one unfiltered query per tracker instead of per-component `QUERY_TERM_CHANGED` slots; the light trackers' joint sweep for the `WITHOUT` precedence; held entities retried only when the extraction resolver's availability epoch moves; eight inline material overrides per instance delta; the view extension interface; the pacer's swap-interval rule; the quality tier as an index into PLAT's tier list; `HiZTest.slang` under `Common/` ([Z2-N-REND-642], X100 applied); the render environment's single writer (X125).

## Requirement index

The table is generated from `Zenith2/Docs/Requirements/RPROD.md`.

| Requirement | Title | Design headings |
|---|---|---|
| Z2-R-RPROD-001 | ZenithFlux RPROD layout | `Zenith2/Docs/Design/RPROD.md` → "ZenithFlux target"; `Zenith2/Docs/Design/RPROD.md` → "RPROD internal directories"; `Zenith2/Docs/Design/RPROD.md` → "Supported matrix" |
| Z2-R-RPROD-002 | Graph public API | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Flux_RenderGraph header"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Flux_RasterTargets header"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Flux_GraphErrors header" |
| Z2-R-RPROD-003 | Graph rebuilt per frame in an arena | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Flux_RenderGraph header"; `Zenith2/Docs/Design/RPROD/Renderer.md` → "Flux.BuildGraph" |
| Z2-R-RPROD-004 | Resources declared once through fields | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Field-table walk"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Setup tables" |
| Z2-R-RPROD-005 | Versioned ordering | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 2 ordering through ZenithGraph"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "First-access widening" |
| Z2-R-RPROD-006 | Compile driver order | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Compile driver" |
| Z2-R-RPROD-007 | Every error reported | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Flux_GraphErrors header"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Diagnostic text" |
| Z2-R-RPROD-008 | Graph error codes | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Flux_GraphErrors header"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 2 ordering through ZenithGraph" |
| Z2-R-RPROD-009 | Settings read once per compile | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Flux_GraphSettings header" |
| Z2-R-RPROD-010 | Empty raster passes culled | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 1 culling and clear folding" |
| Z2-R-RPROD-011 | Unused passes culled | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 1 culling and clear folding" |
| Z2-R-RPROD-012 | Clears folded into load ops | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 1 culling and clear folding" |
| Z2-R-RPROD-013 | Merge only barrier-free compatible passes | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Pass merging algorithm" |
| Z2-R-RPROD-014 | No tile-local merging | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Pass merging algorithm"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Field-table walk" |
| Z2-R-RPROD-015 | Barrier state machine | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 5 barrier synthesis realisation" |
| Z2-R-RPROD-016 | Compatible reads merged | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 5 barrier synthesis realisation" |
| Z2-R-RPROD-017 | One batch per pass | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 5 barrier synthesis realisation" |
| Z2-R-RPROD-018 | First-use discards | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 5 barrier synthesis realisation"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 4 placement realisation" |
| Z2-R-RPROD-019 | Cross-queue records | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 5 barrier synthesis realisation" |
| Z2-R-RPROD-020 | Split-barrier candidates | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Split-barrier candidates" |
| Z2-R-RPROD-021 | Independent barrier verifier | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Barrier verifier" |
| Z2-R-RPROD-022 | No barrier API in pass contexts | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Flux_PassContext header"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Pass context implementation" |
| Z2-R-RPROD-023 | Validation clean | `Zenith2/Docs/Design/RPROD/Tests.md` → "Sync validation" |
| Z2-R-RPROD-024 | First-fit aliased placement | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 4 placement realisation" |
| Z2-R-RPROD-025 | Sharing only when ordered | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 4 placement realisation" |
| Z2-R-RPROD-026 | Physical resource caches | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Physical resource caches" |
| Z2-R-RPROD-027 | Aliasing toggle | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 4 placement realisation"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Flux_GraphSettings header" |
| Z2-R-RPROD-028 | First-write rule | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "First-access widening"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "First-write rule and poisoning" |
| Z2-R-RPROD-029 | Poison on first use | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "First-write rule and poisoning" |
| Z2-R-RPROD-030 | Aliasing parity | `Zenith2/Docs/Design/RPROD/Tests.md` → "Parity suites" |
| Z2-R-RPROD-031 | Transient memory reporting | `Zenith2/Docs/Design/RPROD/Diagnostics.md` → "Counters"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Flux_GraphCaches header" |
| Z2-R-RPROD-032 | Per-pass timestamps | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Timestamp insertion and attribution" |
| Z2-R-RPROD-033 | Timings published per pass | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Timestamp insertion and attribution"; `Zenith2/Docs/Design/RPROD/Diagnostics.md` → "On-screen profiler rows" |
| Z2-R-RPROD-034 | Timestamps in shipping follow the profiler | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Flux_GraphSettings header" |
| Z2-R-RPROD-035 | Front-end frame dump | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Front-end frame dump keys"; `Zenith2/Docs/Design/RPROD/Diagnostics.md` → "Frame dump front-end content" |
| Z2-R-RPROD-036 | D32 needs a reason | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Compile-time rule checks" |
| Z2-R-RPROD-037 | Justified 32-bit targets only | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Compile-time rule checks" |
| Z2-R-RPROD-038 | Unwritten attachments rejected | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Compile-time rule checks"; `Zenith2/Docs/Design/RPROD/PSO.md` → "Permutation flags bits" |
| Z2-R-RPROD-039 | Record-time checks | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Record-time checks" |
| Z2-R-RPROD-040 | Format features at compile | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Compile-time rule checks" |
| Z2-R-RPROD-041 | Persistent and table writes rejected | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Compile-time rule checks" |
| Z2-R-RPROD-042 | No MSAA | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Compile-time rule checks" |
| Z2-R-RPROD-043 | View types and contracts | `Zenith2/Docs/Design/RPROD/Views.md` → "Flux_View header"; `Zenith2/Docs/Design/RPROD/Views.md` → "Flux_ViewContext header"; `Zenith2/Docs/Design/RPROD/Views.md` → "Flux_AtlasTile include" |
| Z2-R-RPROD-044 | Stable view identity | `Zenith2/Docs/Design/RPROD/Views.md` → "Flux_View header" |
| Z2-R-RPROD-045 | Independent per-view histories | `Zenith2/Docs/Design/RPROD/Views.md` → "View registry"; `Zenith2/Docs/Design/RPROD/Views.md` → "Persistent state by kind" |
| Z2-R-RPROD-046 | History invalidation | `Zenith2/Docs/Design/RPROD/Views.md` → "View registry" |
| Z2-R-RPROD-047 | Persistent state retired | `Zenith2/Docs/Design/RPROD/Views.md` → "View registry" |
| Z2-R-RPROD-048 | Depth mode selection | `Zenith2/Docs/Design/RPROD/Views.md` → "Flux_DepthMode header" |
| Z2-R-RPROD-049 | Depth-mode operations single-sourced | `Zenith2/Docs/Design/RPROD/Views.md` → "Flux_DepthMode header" |
| Z2-R-RPROD-050 | Deterministic view list | `Zenith2/Docs/Design/RPROD/Views.md` → "View list construction" |
| Z2-R-RPROD-051 | Render extent and dynamic-resolution floor | `Zenith2/Docs/Design/RPROD/Views.md` → "View list construction" |
| Z2-R-RPROD-052 | Feature masks | `Zenith2/Docs/Design/RPROD/Views.md` → "View list construction" |
| Z2-R-RPROD-053 | Shadowed-light selection | `Zenith2/Docs/Design/RPROD/Views.md` → "Shadowed-light selection" |
| Z2-R-RPROD-054 | View caps and round-robin | `Zenith2/Docs/Design/RPROD/Views.md` → "Round-robin scheduler"; `Zenith2/Docs/Design/RPROD/Views.md` → "View list construction" |
| Z2-R-RPROD-055 | Views share one graph | `Zenith2/Docs/Design/RPROD/Passes.md` → "Path frame functions"; `Zenith2/Docs/Design/RPROD/Passes.md` → "Pass implementation rules" |
| Z2-R-RPROD-056 | View extensions | `Zenith2/Docs/Design/RPROD/Views.md` → "View extensions" |
| Z2-R-RPROD-057 | Advanced multi-view | `Zenith2/Docs/Design/RPROD/Views.md` → "Advanced multi-view" |
| Z2-R-RPROD-058 | Stereo stays deferred | `Zenith2/Docs/Design/RPROD/Views.md` → "Advanced multi-view" |
| Z2-R-RPROD-059 | Shading path selection | `Zenith2/Docs/Design/RPROD/Views.md` → "Flux_ShadingPath header" |
| Z2-R-RPROD-060 | Capability matrix data | `Zenith2/Docs/Design/RPROD/Views.md` → "Capability matrix data file" |
| Z2-R-RPROD-061 | Feature function surface | `Zenith2/Docs/Design/RPROD/Passes.md` → "Flux_Features header"; `Zenith2/Docs/Design/RPROD/Passes.md` → "Prologue feature functions" |
| Z2-R-RPROD-062 | Frame declaration order | `Zenith2/Docs/Design/RPROD/Passes.md` → "Path frame functions" |
| Z2-R-RPROD-063 | Forward+ full frame | `Zenith2/Docs/Design/RPROD/Passes.md` → "Path frame functions"; `Zenith2/Docs/Design/RPROD/Tests.md` → "Render tests" |
| Z2-R-RPROD-064 | Forward full frame | `Zenith2/Docs/Design/RPROD/Passes.md` → "Path frame functions"; `Zenith2/Docs/Design/RPROD/Tests.md` → "Parity suites" |
| Z2-R-RPROD-065 | Deferred full frame | `Zenith2/Docs/Design/RPROD/Passes.md` → "Path frame functions" |
| Z2-R-RPROD-066 | Visibility-buffer full frame | `Zenith2/Docs/Design/RPROD/Passes.md` → "Path frame functions" |
| Z2-R-RPROD-067 | Early-Z order in every path | `Zenith2/Docs/Design/RPROD/Passes.md` → "Pass implementation rules" |
| Z2-R-RPROD-068 | Async flags exactly as the tables | `Zenith2/Docs/Design/RPROD/Passes.md` → "Async-compute flags" |
| Z2-R-RPROD-069 | Pass ownership | `Zenith2/Docs/Design/RPROD/Passes.md` → "Pass ownership and files"; `Zenith2/Docs/Design/RPROD/Passes.md` → "Pass implementation rules" |
| Z2-R-RPROD-070 | Present pass | `Zenith2/Docs/Design/RPROD/Passes.md` → "Present pass realisation" |
| Z2-R-RPROD-071 | Recording never on the submit thread | `Zenith2/Docs/Design/RPROD/Renderer.md` → "Flux.Record"; `Zenith2/Docs/Design/RPROD.md` → "Frame-graph nodes and declared access" |
| Z2-R-RPROD-072 | HiZ in every path and view | `Zenith2/Docs/Design/RPROD/Culling.md` → "Two-phase sequence per view" |
| Z2-R-RPROD-073 | Phase 1 inputs | `Zenith2/Docs/Design/RPROD/Culling.md` → "Instance culling dispatch sequence" |
| Z2-R-RPROD-074 | Phase 2 inputs | `Zenith2/Docs/Design/RPROD/Culling.md` → "Instance culling dispatch sequence" |
| Z2-R-RPROD-075 | Visibility bits union | `Zenith2/Docs/Design/RPROD/Culling.md` → "Instance culling dispatch sequence" |
| Z2-R-RPROD-076 | Invalid history culling | `Zenith2/Docs/Design/RPROD/Culling.md` → "Instance culling dispatch sequence" |
| Z2-R-RPROD-077 | Pyramid format | `Zenith2/Docs/Design/RPROD/Culling.md` → "HiZ pyramid resources"; `Zenith2/Docs/Design/RPROD/Culling.md` → "HiZ build dispatch" |
| Z2-R-RPROD-078 | Single-pass downsampler | `Zenith2/Docs/Design/RPROD/Culling.md` → "HiZ build dispatch" |
| Z2-R-RPROD-079 | Persistent pyramid halves | `Zenith2/Docs/Design/RPROD/Culling.md` → "HiZ pyramid resources" |
| Z2-R-RPROD-080 | Occlusion test | `Zenith2/Docs/Design/RPROD/Culling.md` → "Culling files"; `Zenith2/Docs/Design/RPROD/Culling.md` → "CPU reference implementation" |
| Z2-R-RPROD-081 | CPU reference matches the GPU | `Zenith2/Docs/Design/RPROD/Culling.md` → "CPU reference implementation" |
| Z2-R-RPROD-082 | Shadow-view culling | `Zenith2/Docs/Design/RPROD/Culling.md` → "Shadow view culling realisation" |
| Z2-R-RPROD-083 | Culling counters | `Zenith2/Docs/Design/RPROD/Culling.md` → "Culling counters readback"; `Zenith2/Docs/Design/RPROD/Diagnostics.md` → "Counters" |
| Z2-R-RPROD-084 | HiZ debug overlay | `Zenith2/Docs/Design/RPROD/Culling.md` → "HiZ debug overlay" |
| Z2-R-RPROD-085 | Zero-draw occlusion render test | `Zenith2/Docs/Design/RPROD/Tests.md` → "Culling tests"; `Zenith2/Docs/Design/RPROD/Tests.md` → "Render tests" |
| Z2-R-RPROD-086 | Mobile store-and-reload budget | `Zenith2/Docs/Design/RPROD/Culling.md` → "Two-phase sequence per view"; `Zenith2/Docs/Design/RPROD/Tests.md` → "Benchmarks and harnesses" |
| Z2-R-RPROD-087 | Meshlet two-phase culling | `Zenith2/Docs/Design/RPROD/Culling.md` → "Meshlet culling realisation" |
| Z2-R-RPROD-088 | Meshlet overflow | `Zenith2/Docs/Design/RPROD/Culling.md` → "Meshlet culling realisation" |
| Z2-R-RPROD-089 | Indirect compaction | `Zenith2/Docs/Design/RPROD/Culling.md` → "Instance culling dispatch sequence" |
| Z2-R-RPROD-090 | Draw keys | `Zenith2/Docs/Design/RPROD/Culling.md` → "Draw keys and command regions" |
| Z2-R-RPROD-091 | Command regions sized on the CPU | `Zenith2/Docs/Design/RPROD/Culling.md` → "Draw keys and command regions"; `Zenith2/Docs/Design/RPROD/Culling.md` → "BucketScatter" |
| Z2-R-RPROD-092 | Deterministic compaction | `Zenith2/Docs/Design/RPROD/Culling.md` → "Instance culling dispatch sequence"; `Zenith2/Docs/Design/RPROD/Culling.md` → "CullSort" |
| Z2-R-RPROD-093 | Bucket registry | `Zenith2/Docs/Design/RPROD/Culling.md` → "Bucket registry" |
| Z2-R-RPROD-094 | Transparent command path | `Zenith2/Docs/Design/RPROD/Culling.md` → "Transparent collection and emission" |
| Z2-R-RPROD-095 | Compiler tool target | `Zenith2/Docs/Design/RPROD/PSO.md` → "ZenithShaderCompiler files"; `Zenith2/Docs/Design/RPROD/PSO.md` → "Ownership of the shader toolchain" |
| Z2-R-RPROD-096 | Offline compilation only | `Zenith2/Docs/Design/RPROD/PSO.md` → "Compiler pipeline" |
| Z2-R-RPROD-097 | Deterministic compiler | `Zenith2/Docs/Design/RPROD/PSO.md` → "Compiler pipeline" |
| Z2-R-RPROD-098 | Generated headers | `Zenith2/Docs/Design/RPROD/PSO.md` → "Compiler pipeline" |
| Z2-R-RPROD-099 | Shared GPU structs | `Zenith2/Docs/Design/RPROD/PSO.md` → "Compiler pipeline" |
| Z2-R-RPROD-100 | Compiler errors and exit codes | `Zenith2/Docs/Design/RPROD/PSO.md` → "Compiler error codes" |
| Z2-R-RPROD-101 | Stale output check | `Zenith2/Docs/Design/RPROD/PSO.md` → "Shader lint implementation" |
| Z2-R-RPROD-102 | Watch mode and reload journal | `Zenith2/Docs/Design/RPROD/PSO.md` → "ZenithShaderCompiler files"; `Zenith2/Docs/Design/RPROD/PSO.md` → "PSO table and states" |
| Z2-R-RPROD-103 | Runtime shader library | `Zenith2/Docs/Design/RPROD/PSO.md` → "Shader blob loading"; `Zenith2/Docs/Design/RPROD/PSO.md` → "Permutation flags bits" |
| Z2-R-RPROD-104 | GEOM compiler additions | `Zenith2/Docs/Design/RPROD/PSO.md` → "Compiler additions adopted from GEOM"; `Zenith2/Docs/Design/RPROD/PSO.md` → "Compiler error codes" |
| Z2-R-RPROD-105 | Enumerated permutations | `Zenith2/Docs/Design/RPROD/PSO.md` → "Compiler pipeline" |
| Z2-R-RPROD-106 | Permutation limit | `Zenith2/Docs/Design/RPROD/PSO.md` → "Permutation limit" |
| Z2-R-RPROD-107 | PSO keys | `Zenith2/Docs/Design/RPROD/PSO.md` → "Flux_PSO header" |
| Z2-R-RPROD-108 | Fixed state tables | `Zenith2/Docs/Design/RPROD/PSO.md` → "Fixed state tables" |
| Z2-R-RPROD-109 | PSO table states | `Zenith2/Docs/Design/RPROD/PSO.md` → "PSO table and states" |
| Z2-R-RPROD-110 | PSO builder | `Zenith2/Docs/Design/RPROD/PSO.md` → "PSO builder" |
| Z2-R-RPROD-111 | Precache cook step | `Zenith2/Docs/Design/RPROD/PSO.md` → "Precache cook step" |
| Z2-R-RPROD-112 | Precache file reader | `Zenith2/Docs/Design/RPROD/PSO.md` → "Precache file reader" |
| Z2-R-RPROD-113 | Creation schedule | `Zenith2/Docs/Design/RPROD/PSO.md` → "PSO creation schedule realisation"; `Zenith2/Docs/Design/RPROD/PSO.md` → "Precache API" |
| Z2-R-RPROD-114 | Precache API | `Zenith2/Docs/Design/RPROD/PSO.md` → "Precache API" |
| Z2-R-RPROD-115 | PSO misses recorded in development | `Zenith2/Docs/Design/RPROD/PSO.md` → "PSO recording in development" |
| Z2-R-RPROD-116 | Missing-PSO fallback per build | `Zenith2/Docs/Design/RPROD/PSO.md` → "Missing-PSO fallback realisation" |
| Z2-R-RPROD-117 | Magenta fallback never misses | `Zenith2/Docs/Design/RPROD/PSO.md` → "Precache cook step" |
| Z2-R-RPROD-118 | Zero misses in qualification | `Zenith2/Docs/Design/RPROD/PSO.md` → "Missing-PSO fallback realisation" |
| Z2-R-RPROD-119 | PSO counters | `Zenith2/Docs/Design/RPROD/Diagnostics.md` → "Counters" |
| Z2-R-RPROD-120 | Early-Z lints | `Zenith2/Docs/Design/RPROD/PSO.md` → "Shader lint implementation"; `Zenith2/Docs/Design/RPROD/PSO.md` → "PSO lints" |
| Z2-R-RPROD-121 | Conditional output lint | `Zenith2/Docs/Design/RPROD/PSO.md` → "Shader lint implementation"; `Zenith2/Docs/Design/RPROD/PSO.md` → "PSO lints" |
| Z2-R-RPROD-122 | Half-precision lints | `Zenith2/Docs/Design/RPROD/PSO.md` → "Shader lint implementation" |
| Z2-R-RPROD-123 | Back-face PSO lint | `Zenith2/Docs/Design/RPROD/PSO.md` → "PSO lints" |
| Z2-R-RPROD-124 | Binding and vocabulary lints | `Zenith2/Docs/Design/RPROD/PSO.md` → "Shader lint implementation" |
| Z2-R-RPROD-125 | Lint allowlist needs decisions | `Zenith2/Docs/Design/RPROD/PSO.md` → "Shader lint implementation" |
| Z2-R-RPROD-126 | Vertex input from generated tables | `Zenith2/Docs/Design/RPROD/PSO.md` → "PSO builder" |
| Z2-R-RPROD-127 | Swap interval from refresh | `Zenith2/Docs/Design/RPROD/Renderer.md` → "Frame pacer" |
| Z2-R-RPROD-128 | Desired present times | `Zenith2/Docs/Design/RPROD/Renderer.md` → "Frame pacer" |
| Z2-R-RPROD-129 | Pacer state machine | `Zenith2/Docs/Design/RPROD/Renderer.md` → "Frame pacer" |
| Z2-R-RPROD-130 | Latency slot bounds run-ahead | `Zenith2/Docs/Design/RPROD/Renderer.md` → "Frame pacer" |
| Z2-R-RPROD-131 | Pacing deviation measured | `Zenith2/Docs/Design/RPROD/Renderer.md` → "Frame pacer"; `Zenith2/Docs/Design/RPROD/Diagnostics.md` → "Counters" |
| Z2-R-RPROD-132 | Input-to-photon attribution | `Zenith2/Docs/Design/RPROD/Renderer.md` → "Input-to-photon attribution" |
| Z2-R-RPROD-133 | Quality settings struct | `Zenith2/Docs/Design/RPROD/Renderer.md` → "Quality settings" |
| Z2-R-RPROD-134 | Default tier per platform | `Zenith2/Docs/Design/RPROD/Renderer.md` → "Quality settings" |
| Z2-R-RPROD-135 | Per-platform quality tiers | `Zenith2/Docs/Design/RPROD/Renderer.md` → "Quality settings" |
| Z2-R-RPROD-136 | Tier changes at the next frame | `Zenith2/Docs/Design/RPROD/Renderer.md` → "Quality tier selection" |
| Z2-R-RPROD-137 | Per-view overrides | `Zenith2/Docs/Design/RPROD/Renderer.md` → "Quality tier selection" |
| Z2-R-RPROD-138 | Renderer creation and unwinding | `Zenith2/Docs/Design/RPROD/Renderer.md` → "Create sequence" |
| Z2-R-RPROD-139 | Renderer configuration keys | `Zenith2/Docs/Design/RPROD/Renderer.md` → "Configuration keys" |
| Z2-R-RPROD-140 | Render world consumption | `Zenith2/Docs/Design/RPROD/Renderer.md` → "Render world consumption" |
| Z2-R-RPROD-141 | Render frame sequence | `Zenith2/Docs/Design/RPROD/Renderer.md` → "Render frame sequence" |
| Z2-R-RPROD-142 | One frame of CPU render lag | `Zenith2/Docs/Design/RPROD.md` → "Frame-graph nodes and declared access" |
| Z2-R-RPROD-143 | Counters and profiler rows | `Zenith2/Docs/Design/RPROD/Diagnostics.md` → "Counters"; `Zenith2/Docs/Design/RPROD/Diagnostics.md` → "On-screen profiler rows" |
| Z2-R-RPROD-144 | Console commands | `Zenith2/Docs/Design/RPROD/Diagnostics.md` → "Console commands and debug UI" |
| Z2-R-RPROD-145 | Backend parity suite | `Zenith2/Docs/Design/RPROD/Tests.md` → "Parity suites"; `Zenith2/Docs/Design/RPROD/Tests.md` → "Parity scenes" |
| Z2-R-RPROD-146 | Path parity suite | `Zenith2/Docs/Design/RPROD/Tests.md` → "Parity suites" |
| Z2-R-RPROD-147 | Parity reproducibility and artefacts | `Zenith2/Docs/Design/RPROD/Tests.md` → "Parity suites" |
| Z2-R-RPROD-148 | Parity scenes cover shared passes | `Zenith2/Docs/Design/RPROD/Tests.md` → "Parity scenes" |
| Z2-R-RPROD-149 | Shading-path research harness | `Zenith2/Docs/Design/RPROD/Tests.md` → "Benchmarks and harnesses" |
| Z2-R-RPROD-150 | D-TBDR harness | `Zenith2/Docs/Design/RPROD/Tests.md` → "Benchmarks and harnesses"; `Zenith2/Docs/Design/RPROD.md` → "Selection decisions RPROD owns" |
| Z2-R-RPROD-151 | D-TBDR ruling | `Zenith2/Docs/Design/RPROD.md` → "Selection decisions RPROD owns" |
| Z2-R-RPROD-152 | RPROD budget harness | `Zenith2/Docs/Design/RPROD.md` → "Measurement harness" |
| Z2-R-RPROD-153 | RPROD budget rows allocated | `Zenith2/Docs/Design/RPROD.md` → "Rows RPROD owns" |
| Z2-R-RPROD-154 | Rendering workload games | `Zenith2/Docs/Design/RPROD.md` → "Stage 3" |
| Z2-R-RPROD-155 | On-disk pipeline cache bound | `Zenith2/Docs/Design/RPROD.md` → "Rows RPROD owns" |
| Z2-R-RPROD-156 | GPU scene memory measured | `Zenith2/Docs/Design/RPROD.md` → "Rows RPROD is measured against" |
| Z2-R-RPROD-157 | Widened first write adds no ordering | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "First-access widening"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Consequence of the widening"; `Zenith2/Docs/Design/RPROD/Tests.md` → "Widened first write test" |
| Z2-R-RPROD-158 | Render environment single writer | `Zenith2/Docs/Design/RPROD/Renderer.md` → "Render environment"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Environment extractor" |
| Z2-R-RPROD-159 | Per-instance cull distance | `Zenith2/Docs/Design/RPROD/Culling.md` → "Per-instance cull distance" |
| Z2-R-RPROD-160 | Culling pass tables | `Zenith2/Docs/Design/RPROD/Culling.md` → "Tables of the passes this file adds"; `Zenith2/Docs/Design/RPROD/Culling.md` → "CullClear"; `Zenith2/Docs/Design/RPROD/Culling.md` → "CullSort"; `Zenith2/Docs/Design/RPROD/Culling.md` → "BucketScatter"; `Zenith2/Docs/Design/RPROD/Culling.md` → "HiZDebugOverlay" |
| Z2-R-RPROD-161 | Build-node upload data declared first | `Zenith2/Docs/Design/RPROD/Culling.md` → "Build-node upload data"; `Zenith2/Docs/Design/RPROD/Culling.md` → "Transparent collection and emission"; `Zenith2/Docs/Design/RPROD.md` → "Measurement harness" |
| Z2-R-RPROD-162 | Unpaced present modes | `Zenith2/Docs/Design/RPROD/Renderer.md` → "Frame pacer" |
| Z2-R-RPROD-163 | Environment and SSGI frame slots | `Zenith2/Docs/Design/RPROD/Passes.md` → "Environment and lighting passes in the path functions" |
| Z2-R-RPROD-164 | LIGHT additions to RPROD passes | `Zenith2/Docs/Design/RPROD/Passes.md` → "LIGHT additions to RPROD's pass tables" |
| Z2-R-RPROD-165 | Render instance key and delta | `Zenith2/Docs/Design/RPROD/Extractors.md` → "Instance keys"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Instance flags"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Flux_RenderInstanceDelta layout" |
| Z2-R-RPROD-166 | Engine render component types | `Zenith2/Docs/Design/RPROD/Extractors.md` → "Flux_RenderComponents header"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Render component registration" |
| Z2-R-RPROD-167 | Engine extractors registered without ordering | `Zenith2/Docs/Design/RPROD/Extractors.md` → "Extractor registration" |
| Z2-R-RPROD-168 | Instance deltas exact | `Zenith2/Docs/Design/RPROD/Extractors.md` → "Delta tracking"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Instance extractor"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Engine extraction state"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Extraction resolver" |
| Z2-R-RPROD-169 | Interpolated and settled instance transforms | `Zenith2/Docs/Design/RPROD/Extractors.md` → "Delta tracking"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Instance extractor" |
| Z2-R-RPROD-170 | Teleport history resets | `Zenith2/Docs/Design/RPROD/Extractors.md` → "Instance extractor"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Engine extraction state" |
| Z2-R-RPROD-171 | Light extraction | `Zenith2/Docs/Design/RPROD/Extractors.md` → "Light extractor"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Light conversions" |
| Z2-R-RPROD-172 | Camera view requests | `Zenith2/Docs/Design/RPROD/Extractors.md` → "Camera extractor"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "View request fill"; `Zenith2/Docs/Design/RPROD/Views.md` → "View list construction" |
| Z2-R-RPROD-173 | Camera cuts | `Zenith2/Docs/Design/RPROD/Extractors.md` → "Camera cuts"; `Zenith2/Docs/Design/RPROD/Views.md` → "Flux_View header" |
| Z2-R-RPROD-174 | Environment read from ENV's blocks resource | `Zenith2/Docs/Design/RPROD/Extractors.md` → "Environment extractor"; `Zenith2/Docs/Design/RPROD/Renderer.md` → "Render environment" |
| Z2-R-RPROD-175 | Full-state extraction | `Zenith2/Docs/Design/RPROD/Extractors.md` → "Full-state extraction" |
| Z2-R-RPROD-176 | Extraction determinism | `Zenith2/Docs/Design/RPROD/Extractors.md` → "Determinism of extraction"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Delta tracking" |
| Z2-R-RPROD-177 | Render-world slot-full retry | `Zenith2/Docs/Design/RPROD/Extractors.md` → "Slot-full handling" |
| Z2-R-RPROD-178 | Extraction zones and counters | `Zenith2/Docs/Design/RPROD/Extractors.md` → "Extraction counters and zones"; `Zenith2/Docs/Design/RPROD/Diagnostics.md` → "Profiling zones" |
| Z2-R-RPROD-179 | Procedural instances consumed | `Zenith2/Docs/Design/RPROD/Renderer.md` → "Render world consumption"; `Zenith2/Docs/Design/RPROD/Renderer.md` → "Flux.BuildGraph"; `Zenith2/Docs/Design/RPROD/Renderer.md` → "Flux_Renderer header"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Procedural instance records" |
| Z2-R-RPROD-180 | Null vertex stream bound | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Pass context implementation"; `Zenith2/Docs/Design/RPROD/PSO.md` → "PSO builder" |
| Z2-R-RPROD-181 | Stream-set compiler surface | `Zenith2/Docs/Design/RPROD/PSO.md` → "Compiler additions adopted from GEOM"; `Zenith2/Docs/Design/RPROD/PSO.md` → "Compiler error codes"; `Zenith2/Docs/Design/RPROD/PSO.md` → "Compiler pipeline" |
| Z2-R-RPROD-182 | Transparent over-cap failure attributable | `Zenith2/Docs/Design/RPROD/Culling.md` → "Transparent collection and emission"; `Zenith2/Docs/Design/RPROD/Culling.md` → "Build-node upload data" |
| Z2-R-RPROD-183 | Held entities wait for asset availability | `Zenith2/Docs/Design/RPROD/Extractors.md` → "Extraction resolver"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Delta tracking"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Instance extraction algorithm" |
| Z2-R-RPROD-184 | LIGHT frame data and frame end | `Zenith2/Docs/Design/RPROD/Renderer.md` → "Flux.BuildGraph" |
| Z2-R-RPROD-185 | Rect and disc light extraction | `Zenith2/Docs/Design/RPROD/Extractors.md` → "Light extractor"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Light conversions" |

## Reconciliation items

Each row is a cross-area row RPROD raised or answered; the X column names the cross-doc item that carries a core-doc change, and every core-doc deviation of this family sits in an `<!-- assumption: pending … -->` block naming it. The Where column is the heading of this family that carries the assumption or the elaboration.

| # | Item | To | X | Where |
|---|---|---|---|---|
| RP1 | split barriers: step 5 emits candidates, RHI steps 6 and 7 demote them through `Flux_DemoteSplit` | RHI, REND | X98 | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md → "Split-barrier candidates"` |
| RP2 | `FLUX_RG_ERROR_DEPTH_WRITABLE_UNUSED` is a record-time check in development builds and tests | REND | X101 (e) | RenderGraphFrontEnd.md "Compile-time rule checks", "Record-time checks" |
| RP3 | resolved by RHI (row R20): the back-end entry signatures of steps 3, 6 and 7 in `Flux_GraphQueues.h` and `Flux_GraphPartition.h`; `FLUX_RG_ERROR_SWAPCHAIN_QUEUE` in step 6d; steps 6e and 7 calling `Flux_DemoteSplit` (`Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Ownership of the compile steps"`, "Step 6 queue assignment", "Step 7 command-list partitions") | RHI | — | RenderGraphFrontEnd.md "Compile driver" |
| RP4 | resolved by RHI (row R20): `PrepareNextFrame` step 4 calls `Flux_PassTimingPublisher::Publish` once per queue (`Zenith2/Docs/Design/RHI.md → "Frame-in-flight rotation"`) | RHI | — | RenderGraphFrontEnd.md "Timestamp insertion and attribution" |
| RP5 | `HiZTest.slang` under `Zenith2/Shaders/Common/` | REND | X100 (a) | `Zenith2/Docs/Design/RPROD/Culling.md → "Culling files"` |
| RP6 | compaction per draw key, not per primitive; `m_xPrimitiveCounters` renamed `m_xCommandCounters` | REND | X99 | Culling.md "Draw keys and command regions", "Instance culling dispatch sequence" |
| RP7 | LIGHT's shadow planner: withdrawn, replaced by RP18 | LIGHT | — | `Zenith2/Docs/Design/RPROD/Views.md → "Shadowed-light selection"` |
| RP8 | resolved by RHI (row R20): `SetDesiredPresentTime` and `SetSwapInterval` on the swapchain (`Zenith2/Docs/Design/RHI/PublicAPIDevice.md → "Zenith_RHISwapchain header"`, `Zenith2/Docs/Design/RHI/Recovery.md → "Frame pacing hooks"`) | RHI | — | `Zenith2/Docs/Design/RPROD/Renderer.md → "Frame pacer"` |
| RP9 | the default tier IDs in QualityMatrix | BUDGET | — | Renderer.md "Quality settings" |
| RP10 | parity CTest labels | MOD, PROC | — | `Zenith2/Docs/Design/RPROD/Tests.md → "Test files"` |
| RP11 | the `flip` manifest row | ARCH, SHIP | — | Tests.md "Parity suites" |
| RP12 | reply to GM5 and GM6 | GEOM | — | Culling.md "Bucket registry" |
| RP13 | culling counters reach the profiler with at most `r.framesInFlight` frames of latency | REND | X100 (c) | Culling.md "Culling counters readback" |
| RP14 | superseded by RP16 (it was filed as information only) | GRAPH | — | RenderGraphFrontEnd.md "First-access widening" |
| RP15 | resolved: Rendering carries the resync as `m_bFullState` ("Render world resync"); RPROD rebuilds the draw keys from it | REND, MOD | — | Renderer.md "Render world consumption" |
| RP16 | resolved: the per-subresource first-write flag `GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD_PER_SUBRESOURCE` in the graph core ([Z2-N-ARCH-346]), declared for every GPU transient; GRAPH implements the flag in Stage 2 | GRAPH, ARCH | X134 | RenderGraphFrontEnd.md "First-access widening" |
| RP17 | the render environment: one `SetEnvironment` per world per frame, one writer (the engine environment extractor), LIGHT's type names, ENV's blocks read by the extractor, explicit sun and moon slots (answers EN5, EN6) | REND, LIGHT, ENV | X125 | Renderer.md "Render environment" |
| RP18 | resolved by LIGHT (rows L29, L38): LIGHT's `Flux_ShadowAtlasAllocator`, `Flux_AtlasTile` and `PlanShadows` are the one shape; `Flux_AtlasTile`, `Flux_ShadowViewKind`, `Flux_ShadowViewPlan` and `Flux_RenderLightKey` live in the leaf header `Flux_ShadowAtlas.h`, which `Flux_View.h` and `Flux_ViewContext.h` include; the tiles reach the records through `StageLightRecord` from `PlanShadows` | LIGHT | — | Views.md "Shadowed-light selection", "Flux_AtlasTile include" |
| RP19 | resolved by LIGHT (rows L28, L38; `m_xLight` without output or cluster members, the WIN_DEFAULT froxel grid 128 × 72 × 48 in three INT keys, the pressure floor gated by `HasDynamicResolution`): the quality seam: `Flux_QualitySettings::m_xLight` of LIGHT's type, LIGHT's one upscaler enumeration, the dynamic-resolution scale written by LIGHT's controller alone, LIGHT's tier files named `<TierID>.config.json`, the shadow depth-bias entry named `FLUX_DEPTH_BIAS_SHADOW` with LIGHT's values (answers L2, L6, L23, L26) | LIGHT | X103 (b) | Renderer.md "Quality settings"; PSO.md "Fixed state tables" |
| RP20 | ENV's material vertex modifier through GEOM's `MeshVertex.slang`, ENV's frame slots and capability rows, LIGHT's SSGI slot (answers EN1, EN11, L15) | REND, GEOM, ENV, LIGHT | X127 (1), (3); X105 | `Zenith2/Docs/Design/RPROD/Passes.md → "Environment and lighting passes in the path functions"`, "Pass implementation rules" |
| RP21 | per-instance cull distance through `FLUX_PRIMITIVE_FLAG_CUSTOM_CULL_DISTANCE` (answers EN10) | REND, GEOM | X127 (2) | Culling.md "Per-instance cull distance" |
| RP22 | LIGHT's additions to RPROD's pass tables and to `Flux_ViewPersistentState`, `Flux_FrameShared`, `Flux_ViewTargets` (answers L5, L7, L10, L12) | REND | X103 (a), (b), (e), (h), (i) | Passes.md "LIGHT additions to RPROD's pass tables"; Views.md "Persistent state by kind" |
| RP23 | the history-validity narrowing on a dynamic-resolution step (answers L18) | REND | X104 | Views.md "View registry"; Culling.md "HiZ pyramid resources" |
| RP24 | the per-pass tables of `CullClear`, `CullSort`, `BucketScatter` and `HiZDebugOverlay` join `Zenith2/Docs/Rendering/PerPass/Common.md` | REND | X135 | Culling.md "Tables of the passes this file adds" |
| RP25 | the environment read: `Render.ExtractEnvironment` reads ENV's `PHASE_INPUT` resource `Zenith.Environment.RenderBlocks` (answered by ENV's revision 1, row EN17); LIGHT conforms `Zenith2/Docs/Design/LIGHT/Volumetrics.md` ("Atmosphere ownership", "Underwater medium override") to the per-view medium records being ENV's `Flux_RenderEnvironmentView`, not resource members; ENV's module instance keys set bit 63 (`Flux_MakeModuleInstanceKey`); ENV's submersion records key the main view with the camera extractor's identity (owner the world tag, sub-index 0) | ENV, LIGHT | X125 | Renderer.md "Render environment"; Extractors.md "Environment extractor", "Camera extractor" |
| RP26 | MOD's "ZenithRuntime files" gains `Internal/Zenith_EngineExtractors.h`, `Internal/Extraction/`, `Internal/Zenith_RenderTypes.h/.cpp` and `Tests/EngineExtractors/` in `ZenithRuntime_Tests`; startup step 6 calls `Zenith_RegisterRenderComponentTypes`; the runtime's world attach step calls `Zenith_AttachEngineExtractors` after the modules' `AttachWorld`; the scaffold's template scenes use `Zenith.Render.Camera` and `Zenith.Render.MeshInstance` (answers M18 for cameras) | MOD | — | Extractors.md "Extractor files", "Render component registration", "Extractor registration" |
| RP27 | the GPU scene's application of `Flux_RenderInstanceDelta`: `SKINNED` and `TRANSPARENT` derived at application from the resolved mesh and the effective materials, an ADD of a held key applied as UPDATE and an UPDATE of an unknown key as ADD, the matrix's top three rows as the record's float3x4; `Flux_UploadRing::GetCapBytes()` for the per-view transparent share | RHI | — | Extractors.md "Instance flags", "Flux_RenderInstanceDelta layout"; Culling.md "Transparent collection and emission" |
| RP28 | answers GM21 to GM25 (rows below); asks GEOM's "render-world input" row to name the eight inline material overrides, and a render-target texture asset kind for Stage 5 texture cameras | GEOM | — | Extractors.md "Flux_RenderInstanceDelta layout", "Flux_RenderComponents header" |
| RP29 | skinned poses: `Flux_RenderWorldWriter::AddSkinnedPose` is written by ANIM's extractor from its pose buffers, keyed with `Flux_MakeEntityInstanceKey` of the same entity, not by an engine extractor (`Zenith2/Docs/Design/GEOM/Skinning.md` "the runtime's extractor") | ANIM, GEOM | — | Extractors.md "Scope of this file" |
| RP30 | answers L17 (the camera cut on a teleport of the camera's entity or an ancestor) and LIGHT's extraction asks: `Flux_MakeEntityLightKey` keys, one light per entity by the query precedence point, spot, directional, rect, disc, the tint normalisation, `FLT_MAX` directional range, `Flux_ValidateLightDelta` in the extractor | LIGHT | — | Extractors.md "Light extractor", "Camera cuts" |
| RP31 | the read version of a stage-12 epilogue or extractor invocation, and out-of-tick writes (stamped `GetChangeVersion()` = V(t, M), the end-of-tick sync point's version) being invisible to a stage-12 `QUERY_TERM_CHANGED` term; RPROD's trackers keep their own reference with the reset rule meanwhile | ECS | — | Extractors.md "Delta tracking" |
| RP32 | the runtime's attach context exempt from the `<MODULE_NAME>.` prefix rule for its `Render.*` extractor names; `Render.Seal` registered in every configuration, headless included ([Z2-N-ARCH-439]) | MOD | — | Extractors.md "Extractor files", "Extractor tests" |
| RP33 | `Zenith_AssetRegistry::GetAvailabilityEpoch()`: a registry-wide counter moved with a release store after every activation that publishes a non-null object, never moved by a headless registry | ASSET | — | Extractors.md "Extraction resolver" |
| RP34 | ENV's submersion test selects the main camera by calling `Flux_SelectMainCamera` (extends RP25 (d)) | ENV | X156 | Extractors.md "View request fill", "Flux_RenderComponents header" |
| RP35 | the unit property of area lights: 4 `IntensityUnit`'s meaning for rect and disc lights, and one of the rect schema's two unit properties (4, 19) retired | LIGHT | — | Extractors.md "Light conversions" |
| RP36 | a mesh-level accessor telling whether any primitive of a `Flux_MeshResource` carries `FLUX_PRIMITIVE_FLAG_CUSTOM_CULL_DISTANCE` (extends RP21) | GEOM | X127 (2) | Extractors.md "Extraction resolver", "Instance extraction algorithm" |
| RP37 | answers L28, L29, L36 and L38: applied in Renderer.md and Views.md, with `WriteFrameData` after `ExtendReservation` and `EndFrame` on every path of `Flux.BuildGraph` | LIGHT | X148 | Renderer.md "Flux.BuildGraph", "Quality settings", "Quality tier selection"; Views.md "View list construction" |
| GM21 | answered: the shadow bucket keys the shadow-depth program's position-only specialisation of the mesh's raster code | GEOM | — | Culling.md "Bucket registry" |
| GM22 | answered: `m_axProceduralInstances` consumed by `Flux_ProceduralApplyRecords` before the deltas (step 3b), `Flux_ProceduralBuildFrame` before the prologue (step 7b), `Flux_Renderer::GetProceduralGeometry`, the section and writer members carried in `Flux_RenderWorld.h` | GEOM, REND | X141 | Renderer.md "Render world consumption", "Flux.BuildGraph", "Flux_Renderer header"; Extractors.md "Procedural instance records" |
| GM23 | RHI's call surface for record images; no RPROD change | RHI | — | — |
| GM24 | answered: the stream-set compiler surface (`[StreamSet]`, `--stream-sets`, the seven codes, the null stream, the constants, the procedural shared structs) | GEOM, REND | X138 | PSO.md "Compiler additions adopted from GEOM", "Compiler error codes" |
| GM25 | answered: `Flux_NullVertexStream` bound at binding 7 by the raster context when the PSO's table has the null stream | GEOM, RHI | — | RenderGraphFrontEnd.md "Pass context implementation"; PSO.md "PSO builder" |

Other rows RPROD answers without a change of its own: L3, L4, L9, L14, L16, L17, L20 and L27 name tables and modules LIGHT, GEOM or RHI own (X103 (c), (d), (f), (g); X106), and RPROD's passes follow whatever lands; EN7 (d) blocks RPROD's W-Outdoor ticket on ENV's pulled-forward terrain work; AS13 (ASSET's device-loss reload) needs nothing of RPROD, whose shader containers stay mapped cooked bytes across a device loss. X97 (the graph API additions), X99, X100, X101, X102 and X144 are listed in the rows above or in "Selection decisions RPROD owns"; X94 (the bucket stream-set code) is cited in Culling.md "Bucket registry"; RHI's row R19 (two `Flux_Renderer` forwarding members, X128) is answered in Renderer.md "Flux_Renderer header".
