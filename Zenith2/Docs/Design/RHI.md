# RHI design — the render hardware interface and the render graph's GPU back end

This is the entry document of the RHI design family: the library-level design of `ZenithRHI`, `ZenithRHI_Vulkan` and `ZenithRHI_D3D12`, and of the parts of `ZenithFlux` that own GPU lifetime, synchronisation back end, allocation, presentation and recovery. The specification it elaborates is `Zenith2/Docs/Rendering.md` and its section files; where this family and that specification disagree, the specification wins and this family is corrected in the same diff ([Z2-N-REND-001]). A worker implements against this document, `Zenith2/Docs/WorkerGuide.md` and at most three cited sections.

## Purpose and scope

### What RHI owns

| Owned | Where | Specified in |
|---|---|---|
| the neutral RHI API: device, queues and timelines, command lists (the graph's recording surface), barriers, resources and device handles, views and descriptor heaps, retirement, pipelines and pipeline caches, swapchains, queries, validation, captures, breadcrumbs, fault injection, configuration keys | `Zenith2/Engine/ZenithRHI/` | `Zenith2/Docs/Design/RHI/PublicAPI.md` |
| the Vulkan backend (volk, Vulkan-Profiles, VMA, the `VP_ZENITH2_baseline` profile with `shaderFloat16`, timeline semaphores, `vkCmdPipelineBarrier2`, dynamic rendering, descriptor-indexing heaps, `DrawIndex`, pipeline caches) | `Zenith2/Engine/ZenithRHI_Vulkan/` | `Zenith2/Docs/Design/RHI/Vulkan.md` |
| the D3D12 backend (Agility SDK, D3D12MA, SM 6.6, Enhanced Barriers, the root-constant draw ID, the PSO library) | `Zenith2/Engine/ZenithRHI_D3D12/` | `Zenith2/Docs/Design/RHI/D3D12.md` |
| the render graph's back end: compile steps 3, 6 and 7, recording orchestration, submission, the imported-state table, queue-family ownership transfers, the frame dump writer, the graph conformance test access | `Zenith2/Engine/ZenithFlux/Internal/Graph/` (the files of "Division of Flux code with RPROD") | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` |
| GPU lifetime (frames in flight, retirement, deferred deletion), the upload ring, readbacks, descriptor slot allocation, residency reporting, memory-pressure response, the GPU scene tables and pools, persistent resources | `Zenith2/Engine/ZenithFlux/Internal/{Lifetime,Upload,Readback,Bindless,Memory,GPUScene}/`, `ZenithFlux/Flux_QueueValues.h`, `Flux_PersistentResources.h`, `Flux_Readback.h` | `Zenith2/Docs/Design/RHI/Memory.md` |
| presentation, swapchain recreation, device-loss detection, breadcrumbs, the recover-or-clean-exit policy, the persistent pipeline-cache store, captures | `Zenith2/Engine/ZenithFlux/Internal/{Recovery,PipelineCache}/` | `Zenith2/Docs/Design/RHI/Recovery.md` |

`Zenith2/Docs/Architecture.md → "Repository layout"` (table "Ownership per directory") gives RHI the three RHI libraries and, inside `ZenithFlux` (an RPROD library), the render graph and GPU lifetime.

### Division of Flux code with RPROD

| Flux code | Area | Why |
|---|---|---|
| `Flux_RenderGraph` public API, setup, culling (step 1), ordering (step 2), placement (step 4), barrier synthesis and the verifier (step 5), pass merging, pass contexts, timestamp insertion, the graph errors | RPROD | Z2-I-RPROD-10, -11, -12, -19, -25 |
| steps 3, 6, 7, record orchestration, `Flux_ToRHIBarrier`, submission, the End phase, the imported-state table, the frame dump writer, `Flux_RenderGraphTestAccess` | RHI | Z2-I-RHI-07, -16, -18 |
| `Flux_FrameLifetime`, `Flux_DeferredDeletion`, `Flux_DescriptorAllocator`, `Flux_UploadRing`, `Flux_ReadbackPool`, `Flux_GpuMemoryReporter`, `Flux_MemoryConsumer`, `Flux_GPUScene`, `Flux_PersistentResources`, `Flux_SwapchainManager`, `Flux_DeviceLossHandler`, `Flux_DeviceLossReportWriter`, `Flux_PipelineCacheStore`, `Flux_CaptureController` | RHI | Z2-I-RHI-01 to -06, -08 to -13, -17, -19 to -22 |
| PSO keys, precache sets, creation schedule and the missing-PSO fallback; frame pacing; views; culling; every pass | RPROD (passes: LIGHT, GEOM, VFX and the rest) | Z2-I-RPROD |

The interface between the halves is `Flux_CompiledGraph` (`Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Ownership of the compile steps"`); `Zenith2/Docs/Design/RPROD.md` specifies the front half.

### Explicit exclusions

| Not in RHI | Owner or classification | Decision |
|---|---|---|
| barrier synthesis and transient placement algorithms | RPROD | Z2-D-056, Z2-D-057 |
| the shader compiler, generated headers, permutations, PSO precaching and the missing-PSO fallback | RPROD | Z2-D-025, Z2-D-015 |
| frame pacing (the controller) | RPROD; the RHI provides the hooks ("Present modes and formats" of Recovery.md) | Z2-I-RHI-13 note |
| any public barrier, layout-transition, ownership-transfer or aliasing-barrier API | forbidden: only `ZenithFlux/Internal/Graph` reaches barriers | Z2-D-056 |
| runtime shader compilation | forbidden | Z2-D-025 |
| MSAA | not provided | Z2-D-035 |
| tile-local reads and subpass merging | not implemented until D-TBDR (open, Stage 5) | Z2-D-036 |
| hardware ray tracing | deferred to Stage 7 (ADVGFX) | Z2-D-043 |
| stereo and multi-view rendering | deferred to Stage 7 (SCOPE), view identity kept XR-ready by RPROD | Z2-D-072 |
| console RHI backends | deferred to Stage 7, gated on SDK access (SCOPE) | Z2-D-071 |
| a GPU backend on the `linux-tsan` host | none: GPU libraries exclude it ([Z2-N-ARCH-173]) | Z2-D-010 |
| runtime-created samplers | none: the static sampler table | Z2-D-054 |

### Inventory items covered

| Item | Summary | Design heading |
|---|---|---|
| Z2-I-RHI-01 | timelines per queue, retirement rule | `Zenith2/Docs/Design/RHI/Memory.md → "Retirement queue"`; this file "Retirement rule" |
| Z2-I-RHI-02 | descriptor slot reuse after retirement | `Zenith2/Docs/Design/RHI/Memory.md → "Descriptor heap and slot allocation"` |
| Z2-I-RHI-03 | frames in flight 2–3 | this file "Frame-in-flight rotation" |
| Z2-I-RHI-04, Z2-I-RHI-05 | upload ring stall, cap, split or refuse, counter | `Zenith2/Docs/Design/RHI/Memory.md → "Upload ring internals"` |
| Z2-I-RHI-06 | readbacks as operation handles | `Zenith2/Docs/Design/RHI/Memory.md → "Readback pool"` |
| Z2-I-RHI-07 | tracked resource kinds | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Tracked resource kinds realised"` |
| Z2-I-RHI-08 | GPU scene tables and pools | `Zenith2/Docs/Design/RHI/Memory.md → "GPU scene implementation"` |
| Z2-I-RHI-09 | allocation | `Zenith2/Docs/Design/RHI/Memory.md → "Memory classes and allocation"` |
| Z2-I-RHI-10 | residency | `Zenith2/Docs/Design/RHI/Memory.md → "Residency reporting"` |
| Z2-I-RHI-11 | heap alignment and placement | `Zenith2/Docs/Design/RHI/Memory.md → "Heap alignment and placement"` |
| Z2-I-RHI-12 | memory pressure | `Zenith2/Docs/Design/RHI/Memory.md → "Memory pressure responder"` |
| Z2-I-RHI-13 | presentation | `Zenith2/Docs/Design/RHI/Recovery.md → "Swapchain model"` |
| Z2-I-RHI-14 | capability negotiation | this file "Device creation sequence"; `Zenith2/Docs/Design/RHI/Vulkan.md → "Baseline profile check"`; `Zenith2/Docs/Design/RHI/D3D12.md → "Baseline checks"` |
| Z2-I-RHI-15 | validation | this file "Validation layers and the clean requirement" |
| Z2-I-RHI-16 | graph inspection | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Frame dump back-end sections"` |
| Z2-I-RHI-17 | captures | `Zenith2/Docs/Design/RHI/Recovery.md → "Captures"` |
| Z2-I-RHI-18 | queue scheduling | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Step 6 queue assignment"` |
| Z2-I-RHI-19 | recovery diagnostics | `Zenith2/Docs/Design/RHI/Recovery.md → "Device-loss report"` |
| Z2-I-RHI-20 | swapchain recreation | `Zenith2/Docs/Design/RHI/Recovery.md → "Swapchain recreation"` |
| Z2-I-RHI-21 | device loss, breadcrumbs, recover or exit | `Zenith2/Docs/Design/RHI/Recovery.md → "Recover-or-clean-exit policy"` |
| Z2-I-RHI-22 | pipeline-cache invalidation | `Zenith2/Docs/Design/RHI/Recovery.md → "Pipeline cache store"` |

### Relationship to the specification

This family adds, and marks with `<!-- elaboration -->` where the choice is new: the neutral barrier vocabulary and its split between Flux and the backends; the retirement queue and descriptor free lists as ZenithRHI types; the upload ring's predicted reservation with compile-time extension, the build-node upload order, frame deferral and frame failure; the retirement drain headroom and the retirement listener; the render-idle points and the renderer's prologue hook they assume; the frame-failure exit; the back-end compile entries, the timestamp hand-off, the two pacing hooks and the HDR metadata call other areas asked for; the `platform=` token of `FormatFeatures.table`; the device-creation step list; the device-loss state machine with its exit code; `Flux_RenderGraphTestAccess`; the lock rows and ForbiddenCheck rule F20. Each addition that touches another document is listed in "Reconciliation items".

### Document family

| File | Holds | Its `##` headings |
|---|---|---|
| `Zenith2/Docs/Design/RHI.md` | this entry: scope, dependencies, layout, the top-level contracts, threading, errors, diagnostics, failure handling, tests, budgets, stages, decisions, the requirement index | the fifteen skeleton headings, then "Reconciliation items" |
| `Zenith2/Docs/Design/RHI/PublicAPI.md` | the public value, resource, descriptor, lifetime, pipeline and queue declarations with their contract blocks | Header list · Zenith_RHITypes header · Zenith_RHIError header · Zenith_DeviceHandle header · Zenith_RHICapabilities header · Zenith_FormatFeatures header · Zenith_RHIShaderInterface header · Zenith_RHIResources header · Zenith_RHIDescriptors header · Zenith_RHILifetime header · Zenith_RHIPipelines header · Zenith_RHIQueue header |
| `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` | the recording, barrier, presentation, query, debug, fault-injection, configuration and device declarations, the backend factory headers and the RHI-owned ZenithFlux public headers | Zenith_RHICommandList header · Zenith_RHIBarrier header · Zenith_RHISwapchain header · Zenith_RHIQueries header · Zenith_RHIDebug header · Zenith_RHIFaultInjection header · Zenith_RHIConfig header · Zenith_RHIDevice header · Backend factory headers · RHI-owned ZenithFlux public headers |
| `Zenith2/Docs/Design/RHI/Vulkan.md` | the Vulkan backend | Scope of this file · Internal file map · Instance creation · Baseline profile check · Physical device selection · Queue families · Logical device and volk · Memory through VMA · Descriptor heap realisation · Pipeline layout and pipelines · Command recording mapping · Barrier translation · Submission and timelines · Swapchain and presentation · Pipeline cache · Debug utils and validation · Device loss and fault reports · Timestamps and calibration · RenderDoc capture on Vulkan · Android specifics |
| `Zenith2/Docs/Design/RHI/D3D12.md` | the D3D12 backend | Scope of this file · Internal file map · Agility SDK loading · Adapter selection · Baseline checks · Queues and fences · Descriptor heaps · Root signature and command signatures · Pipelines and the PSO library · Command recording mapping · Enhanced barrier translation · Memory through D3D12MA · Swapchain and presentation · Debug layer and validation · Device removal and DRED · Timestamps and calibration · PIX and RenderDoc capture |
| `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` | the graph's back end | Scope of this file · Ownership of the compile steps · Internal file map · Step 3 queues lifetimes and reachability · Step 6 queue assignment · Step 7 command-list partitions · Neutral barrier translation · Recording · Submission · Imported-state table · Tracked resource kinds realised · Queue-family ownership transfers · Frame dump back-end sections · Flux_RenderGraphTestAccess · Back-end tests |
| `Zenith2/Docs/Design/RHI/Memory.md` | allocation, lifetime, upload, readback, residency, pressure, GPU scene | Scope of this file · Memory classes and allocation · Device resource table · Heap alignment and placement · Retirement queue · Descriptor heap and slot allocation · Upload ring internals · Readback pool · Residency reporting · Memory pressure responder · Device memory exhaustion · GPU scene implementation |
| `Zenith2/Docs/Design/RHI/Recovery.md` | presentation, recovery, pipeline caches, captures | Scope of this file · Swapchain model · Present modes and formats · Acquire and present · Frame pacing hooks · Swapchain recreation · Device loss detection · Breadcrumbs · Device-loss report · Recover-or-clean-exit policy · Pipeline cache store · Captures |
| `Zenith2/Docs/Design/RHI/Tests.md` | every test | Test files · RHI unit tests · RHI conformance suite · Format feature and profile tests · Bindless probe · Half-precision check · Lifetime and upload tests · Readback tests · Descriptor tests · GPU scene tests · Memory tests · Presentation and recovery tests · Validation tests · Graph back-end tests · Failure-injection tests · Stress and sanitizer runs · Device steps · Benchmarks |

## Dependencies

### Libraries

| Library | Declared direct dependencies | Third party (manifest names) |
|---|---|---|
| `ZenithRHI` (engine API, layer 3) | ZenithCore, ZenithProfiling, ZenithPlatform | none |
| `ZenithRHI_Vulkan` (backend, layer 4) | ZenithRHI, ZenithPlatform, ZenithProfiling, ZenithCore | `vulkan-headers`, `volk`, `vulkan-profiles`, `vma`, `renderdoc-app` |
| `ZenithRHI_D3D12` (backend, layer 4, Windows) | ZenithRHI, ZenithPlatform, ZenithProfiling, ZenithCore | `d3d12-agility-sdk`, `d3d12ma`, `renderdoc-app` |
| developer-only tools (no library links them) | — | `python` (pinned, runs `Generate-Profiles.ps1` and the `vulkan-profiles` generator `scripts/gen_profiles_solution.py`; no build or gate step runs it, because the generated profile files are committed with the JSON SHA-256 and the generator version in their first line) |
| `ZenithFlux` (the RHI-owned files) | as `Zenith2/Docs/Architecture.md → "Library dependency graph"` lists ZenithFlux; the RHI files use ZenithCore, ZenithGraph, ZenithJobs, ZenithProfiling, ZenithRHI and ZenithAssets of that set | none |

These rows equal the table of `Zenith2/Docs/Architecture.md → "Library dependency graph"`; the manifest entry `renderdoc-app` (the MIT single header `renderdoc_app.h`) is a new manifest row, as are the developer-only `python` and `vulkan-profiles` generator pins ("Reconciliation items", R7; cross-document item X78, pending).

### Headings this design relies on

| Need | Citation |
|---|---|
| the specification of everything rendering | `Zenith2/Docs/Rendering.md → "GPU baseline and profiles"`, "Binding model", "Render graph", "Barrier synthesis", "Transient aliasing", "GPU lifetime and synchronisation", "GPU scene tables and pools", "Decision D-SLANG", "Recovery"; `Zenith2/Docs/Rendering/GpuResources.md`, `Rendering/RenderGraph.md`, `Rendering/ShaderSystem.md` (their `##` headings as cited in each split file) |
| results, error records and categories | `Zenith2/Docs/Contracts.md → "Errors and results"`; `Zenith2/Docs/Design/CORE/Diagnostics.md → "Results and the error taxonomy"` |
| owned pointers, heap, arenas | `Zenith2/Docs/Contracts.md → "Ownership and allocation"`; `Zenith2/Docs/Design/CORE/Memory.md → "Arena internals"` |
| containers | `Zenith2/Docs/Design/CORE/Containers.md → "Zenith_Array"`, "Zenith_HashMap and Zenith_HashSet", "Zenith_BoundedQueue", "Zenith_Span" |
| operation handles | `Zenith2/Docs/Contracts.md → "Handle kinds"`; `Zenith2/Docs/Design/CORE/Threading.md → "Zenith_OperationHandle implementation"` |
| device handles | `Zenith2/Docs/Contracts.md → "Handle kinds"` |
| mutexes, lock order, bounded queues, shutdown | `Zenith2/Docs/Contracts.md → "Threading and concurrency contract"`; `Zenith2/Docs/Design/CORE/Threading.md → "Mutexes and lock ordering"` |
| logging and configuration | `Zenith2/Docs/Design/CORE/Diagnostics.md → "Structured logging"`, "Configuration store", "Clocks" |
| clip space | `Zenith2/Docs/Design/CORE/Maths.md → "Coordinate systems"` |
| the memory governor | `Zenith2/Docs/Architecture.md → "Memory governor"`; `Zenith2/Docs/Design/CORE/MemoryGovernor.md → "Consumer registration"` |
| the graph core the back end compiles through | `Zenith2/Docs/Architecture.md → "ZenithGraph shared core"`; `Zenith2/Docs/Design/GRAPH.md → "Zenith_Graph.h"`, "Graph dump schema zenith2.graph-dump version 1"; `Zenith2/Docs/Design/GRAPH/Tests.md → "The Flux builder mapping"` |
| parallel recording, render-submit affinity | `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Frame structure"`, "Job system"; `Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobContext"`; `Zenith2/Docs/Design/JOBS/Algorithms.md → "Render-submit thread"` |
| startup, shutdown, exit codes, composition | `Zenith2/Docs/Architecture/CompositionRoot.md → "Startup sequence"`, "The generated composition table" |
| backend libraries and the build helper | `Zenith2/Docs/Architecture/BuildSystem.md → "The zenith_add_library helper"`; `Zenith2/Docs/Contracts.md → "API boundary"` |
| the platform window, surface and lifecycle | `Zenith2/Docs/Architecture/PlatformLayer.md → "SDL3 responsibilities"`, "Android lifecycle and surface recovery" |
| budgets | `Zenith2/Docs/Budgets.md → "Rendering rows"`, "Memory budgets", "Row index" |

<!-- assumption: pending PLAT -->
The PLAT design is not yet signed off; this family relies on its window and lifecycle names as drafted: `Zenith_Window::CreateVulkanSurface(void*) → Zenith_Result<uint64, Zenith_PlatformError>`, `Zenith_Window::DestroyVulkanSurface`, `Zenith_Window::GetNativeHandle() → Zenith_NativeWindowHandle` (`NATIVE_WINDOW_KIND_WIN32`, `_ANDROID`), `Zenith_Window::IsSurfaceAvailable()`, `Zenith_GetVulkanInstanceProcAddr(Zenith_Platform&)`, `Zenith_GetVulkanInstanceExtensions(Zenith_Platform&)`, and `Zenith_AppLifecycleListener::OnReleaseSurface`/`OnRecreateSurface` (`Zenith2/Docs/Design/PLAT/Platform.md` → "Windows"; `Zenith2/Docs/Design/PLAT/Android.md` → "Surface-loss handshake").
<!-- /assumption -->

### Upward-callback interfaces RHI declares

| Interface | Declared in | Implemented by | Purpose |
|---|---|---|---|
| `Zenith_RHIValidationSink` | ZenithRHI | ZenithRuntime (logs), the test support sink (fails tests) | validation messages from layers and driver threads |
| `Zenith_RHIFaultInjector` | ZenithRHI | test code alone | failure injection with no shipping switch ([Z2-N-CONTRACT-415]) |
| `Zenith_RHIRetirementListener` | ZenithRHI | `Flux_DeferredDeletion` (ZenithFlux) | every table resource the device destroys, before its slot is freed, so Flux removes its imported state |
| `Flux_GPUSceneRangeListener` | ZenithFlux (`Internal/GPUScene/Flux_GPUScene.h`, RHI-owned) | RPROD's `Flux_MaterialRangeForwarder` (ZenithFlux), installed by `Flux_Renderer::Create` step 7 through `Flux_GPUScene::SetRangeListener` | every material-slot range the GPU scene creates or frees, so GEOM's `Flux_MaterialSlotCache` fills its records and owns its buckets (`Zenith2/Docs/Design/RHI/Memory.md → "Material-slot range listener"`; cross-area row RPB23); an interface within one library, not an upward edge |

The RHI files of ZenithFlux implement two interfaces declared below them: `Zenith_MemoryConsumer` (ZenithCore, as `Flux_MemoryConsumer`) and, through the runtime's forwarding, the surface members of `Zenith_AppLifecycleListener` (ZenithPlatform). Every other upward path is a return value or a polled status ([Z2-N-ARCH-078]).

### Names other documents assume

| Name | Assumed by | Ruling here |
|---|---|---|
| `Zenith_CreateRHI_Vulkan`, `Zenith_CreateRHI_D3D12` | Rendering, CompositionRoot, MOD | the factories; `BACKEND_NAME` `Vulkan` and `D3D12` |
| `Zenith_PfnCreateDevice` in `ZenithRHI/Zenith_RHIDevice.h` | CompositionRoot, MOD | kept under this name (the MOD question of its reconciliation R4 is answered: no rename) |
| `Zenith_RHIDevice`, `Zenith_RHIDeviceDesc`, `Zenith_RHIError` | Architecture ("Assumed types") | `Zenith2/Docs/Design/RHI/PublicAPIDevice.md → "Zenith_RHIDevice header"`, `Zenith2/Docs/Design/RHI/PublicAPI.md → "Zenith_RHIError header"` |
| `uZENITH_D3D12_AGILITY_SDK_VERSION`, `szZENITH_D3D12_AGILITY_SDK_PATH` in `ZenithRHI_D3D12/Zenith_RHI_D3D12.h`, and the staged DLLs | MOD | `Zenith2/Docs/Design/RHI/PublicAPIDevice.md → "Backend factory headers"` |
| `Zenith_RHISRV<T>`, `Zenith_RHIUAV<T>`, `Zenith_RHIDescriptorHandle`, `Zenith_RHIShaderInterface.h`, `Zenith_FormatFeatures.h`, `Zenith_RHIFormat` | Rendering, DecisionLog Z2-D-015 nouns | `Zenith2/Docs/Design/RHI/PublicAPI.md` |
| `Flux_RenderGraphTestAccess` | GRAPH | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Flux_RenderGraphTestAccess"` |
| `Flux_TextureHandle`, `Flux_BufferHandle`, `Flux_SamplerHandle`, `Flux_PipelineHandle` | Contracts, Rendering | aliases of device handles in `Zenith_DeviceHandle.h`, identical to the `Zenith_RHI*Handle` names; their tag types are `Zenith_RHITextureTag` and the rest (X73, pending) |
| `Flux_CapabilityTier` | Rendering ("Capability negotiation", `Flux_SelectShadingPath`) | the ZenithRHI type is `Zenith_RHICapabilityTier` (`ZENITH_RHI_CAPABILITY_TIER_DESKTOP/MOBILE`); Flux keeps `using Flux_CapabilityTier = Zenith_RHICapabilityTier` in `Flux_QueueValues.h` (X73, pending; "Reconciliation items", R13) |
| `Flux_GraphBuildQueuePlan`, `Flux_GraphAssignQueues`, `Flux_GraphPartition` and the call of `Flux_DemoteSplit` from steps 6 and 7 | RPROD (`Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md → "Compile driver"`, row RP3) | declared as RPROD's driver calls them: `Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Ownership of the compile steps"`; the demotions in "Step 6 queue assignment" row 6e and "Step 7 command-list partitions" |
| `Flux_PassTimingPublisher::Publish(const Flux_FrameTimestampRecord&, Zenith_Span<const uint64>, uint32)` | RPROD (`RenderGraphFrontEnd.md → "Timestamp insertion and attribution"`, row RP4) | called once per queue from `PrepareNextFrame` step 4 ("Frame-in-flight rotation") and from a frames-in-flight change for the old slots ("Changing the frames in flight"); `Flux_FrameTimestampRecord` is declared here |
| `Zenith_RHISwapchain::SetDesiredPresentTime(uint64, uint64)`, `SetSwapInterval(uint32)` | RPROD (`Zenith2/Docs/Design/RPROD/Renderer.md → "Frame pacer"`, row RP8) | declared in `Zenith2/Docs/Design/RHI/PublicAPIDevice.md → "Zenith_RHISwapchain header"`, realised in `Zenith2/Docs/Design/RHI/Recovery.md → "Frame pacing hooks"` |
| the HDR metadata call and the per-surface colour-space report | LIGHT (`Zenith2/Docs/Design/LIGHT/ColourManagement.md → "HDR calibration and metadata"`, "Output mode selection", row L21) | `Zenith_RHISwapchain::SetHDRMetadata(const Zenith_RHIHDRMetadata&)`, `Zenith_RHISwapchainInfo::m_uOutputColourSpacesSupported`, `m_bDisplayHdr`, `m_fDisplayMaxLuminanceNits`, `Zenith_RHICapabilities::m_bHdrMetadata`; `Flux_SwapchainManager::SetHDRMetadata(const Flux_HDRMetadata&)` takes LIGHT's value (Recovery.md "Present modes and formats") |
| `Flux_GPUSceneRangeListener` (`OnMaterialRangeCreated`, `OnMaterialRangeFreed`), `Flux_GPUScene::SetRangeListener` | RPROD (`Zenith2/Docs/Design/RPROD/Renderer.md → "Create sequence"` step 7, row RPB23), GEOM (`Zenith2/Docs/Design/GEOM/Materials.md → "Material slots and constants"`, row GM23) | declared with these names and signatures in `Zenith2/Docs/Design/RHI/Memory.md → "Material-slot range listener"` |
| `Flux_Renderer::RunPrologue`, `Flux_Renderer::GetDeviceLossState`, `Flux_DeviceLossState` | this design, of MOD, ARCH and RPROD | "Render-idle points" (assumption pending MOD, ARCH, X128; cross-area note R19) |

### Areas RHI depends on for integration

| Area | Integration |
|---|---|
| RPROD | the front half of the graph and its compile driver, which calls the three back-end entries (RP3); PSO creation through `CreateGraphicsPipeline`; frame pacing through the swapchain hooks (RP8); the timestamp publisher RHI calls (RP4); the two `Flux_Renderer` members `RunPrologue` and `GetDeviceLossState`, which forward to RHI's components ("Render-idle points", assumption pending MOD, ARCH (X128)) |
| MOD | startup step 12 and 13, the composition table, Agility exports and staging, exit codes 4 (factory refusal) and 6 (`EXIT_CODE_DEVICE_LOST`, X72); the frame-loop driver's prologue call `Flux_Renderer::RunPrologue()` in step 4 after `Zenith_MemoryGovernor::Update`, and the `bRunRender` term `renderer.GetDeviceLossState() == FLUX_DEVICE_LOSS_STATE_NORMAL` in `RunFrame` step 2 (`Zenith2/Docs/Design/MOD.md → "Frame loop driver"`; assumption pending MOD, ARCH (X128), cross-area note R19) |
| ARCH | [Z2-N-ARCH-437] and "The frame prologue" step 4 admitting the renderer's prologue call; `Zenith2/Docs/Architecture/CompositionRoot.md → "The frame loop"` step 2 gaining the device-loss term (X128) |
| LIGHT | the HDR metadata `Flux_HDRMetadata` passed to the swapchain manager and the per-surface colour-space report its output-mode selection reads (L21); a fifth GPU-scene table in Stage 5 (L14, X106 pending) |
| GEOM | the `R16_UNORM` sampled row of the format-feature table (GM8, X130 pending) |
| PLAT | the window, native handle, surfaces, the lifecycle handshake |
| CORE | results, containers, operation tables, the governor, configuration, logging |
| JOBS, GRAPH | the render subgraph's nodes, parallel recording, the graph core, the Flux conformance builder |
| ASSET | the loader interface Flux implements (persistent resource creation and release, reload after device loss) |
| SAVE | the autosave request of the clean-exit path |
| DEVT | the on-screen profiler rows, the crash report, the frame-dump viewer |
| SHIP | the manifest pins (Agility, D3D12MA, VMA, volk, Vulkan-Profiles, WARP, lavapipe, RenderDoc header), the shared gate environment's `VK_DRIVER_FILES` |

## Library and file layout

### ZenithRHI target

```cmake
# Zenith2/Engine/ZenithRHI/CMakeLists.txt
zenith_add_library(ZenithRHI DEPENDS ZenithCore ZenithProfiling ZenithPlatform
	SUPPORTED ZENITH_GPU
	GENERATES Zenith_FormatFeatures.h)
```

| Path (`Zenith2/Engine/ZenithRHI/`) | Kind | Contents |
|---|---|---|
| the nineteen headers of `Zenith2/Docs/Design/RHI/PublicAPI.md → "Header list"` | public | the API |
| `Internal/FormatFeatures.table`, `Internal/CMake/GenerateFormatFeatures.cmake` | committed data and the generator ([Z2-N-REND-038]; placement per X69, R6); the generated `Zenith_FormatFeatures.h` stays a public root header | the format-feature table |
| `Internal/Zenith_RHIDeviceBase.h/.cpp` | private; the one ZenithRHI header a backend includes (X68) | the backend-neutral device half: resource tables, retirement queue, descriptor lists, report text, fault-injector calls |
| `Internal/Zenith_RHIHandleCodec.h` | private; included by `Zenith_RHIDeviceBase.h`, never by a backend directly | handle bit encoding |
| `Internal/Zenith_RHIResourceTable.h` | private; included by `Zenith_RHIDeviceBase.h`, never by a backend directly | `Zenith_RHIResourceTable<TRecord>` |
| `Internal/Zenith_RHIRetirementQueue.cpp`, `Internal/Zenith_RHIDescriptorHeap.cpp` | private | the two lifetime types |
| `Internal/Zenith_RHILocks.h` | private | the lock-level constants of "Lock-order rows added by RHI" |
| `Internal/Zenith_RHIConfig.cpp`, `Internal/Zenith_RHIStrings.cpp` | private | configuration keys, error strings |
| `Tests/…` | tests | `Zenith2/Docs/Design/RHI/Tests.md → "Test files"` |
| `Tests/Conformance/…` | `ZenithRHI_Conformance` | the backend conformance suite |

The backends include `ZenithRHI/Internal/Zenith_RHIDeviceBase.h` and no other ZenithRHI `Internal/` header; the codec and the resource table reach them through it. A backend is the implementation of its API library, and the RHI's backend-neutral half is shared by both; the Libgraph gate step's `Internal/` rule ([Z2-N-CONTRACT-024]) is satisfied through the exemption cross-document item X68 rules: `KIND BACKEND` libraries may include their `BACKEND_OF` library's `Internal/Zenith_*Base.h` headers, and nothing else of it ("Reconciliation items", R5).

### ZenithRHI_Vulkan target

`Zenith2/Docs/Design/RHI/Vulkan.md → "Internal file map"` lists every file, the `zenith_add_library` call (`KIND BACKEND`, `SUPPORTED ZENITH_GPU`) and the committed profile files.

### ZenithRHI_D3D12 target

`Zenith2/Docs/Design/RHI/D3D12.md → "Internal file map"` lists every file and the call (`KIND BACKEND`, `SUPPORTED ZENITH_WINDOWS`); every source is under `Internal/Windows/`.

### RHI-owned files in ZenithFlux

| Path (`Zenith2/Engine/ZenithFlux/`) | Contents |
|---|---|
| `Flux_QueueValues.h`, `Flux_PersistentResources.h`, `Flux_Readback.h` | public headers |
| `Internal/Lifetime/Flux_FrameLifetime.h/.cpp`, `Flux_DeferredDeletion.h/.cpp` | frames in flight, deferred deletion |
| `Internal/Upload/Flux_UploadRing.h/.cpp`, `Flux_UploadRingInternal.h` | the upload ring and its operation |
| `Internal/Readback/Flux_ReadbackPool.h/.cpp` | readbacks |
| `Internal/Bindless/Flux_DescriptorAllocator.h/.cpp` | descriptor wrapper |
| `Internal/Memory/Flux_GpuMemoryReporter.h/.cpp`, `Flux_MemoryConsumer.h/.cpp` | residency and pressure |
| `Internal/GPUScene/Flux_GPUScene.h/.cpp`, `Flux_TableSlotAllocator.h`, `Flux_PoolAllocator.h/.cpp`, `Flux_PersistentResources.cpp` | the GPU scene |
| `Internal/Graph/Flux_GraphQueues.*`, `Flux_GraphPartition.*`, `Flux_ToRHIBarrier.*`, `Flux_GraphRecorder.*`, `Flux_GraphSubmitter.*`, `Flux_ImportedStateTable.*`, `Flux_FrameDumpWriter.*` | the graph back end |
| `Internal/Recovery/Flux_SwapchainManager.*`, `Flux_DeviceLossHandler.*`, `Flux_DeviceLossReportWriter.*`, `Flux_CaptureController.*` | presentation and recovery |
| `Internal/PipelineCache/Flux_PipelineCacheStore.h/.cpp` | the persistent cache |
| `Tests/Lifetime/`, `Upload/`, `Readback/`, `Bindless/`, `Memory/`, `Recovery/`, `PipelineCache/`, `Graph/Backend/`, `GPUScene/`, `GraphConformance/Flux_RenderGraphTestAccess.h` | tests |

### Test targets and files

`ZenithRHI_Tests`, `ZenithRHI_Vulkan_Tests`, `ZenithRHI_D3D12_Tests`, `ZenithRHI_Conformance`, `ZenithFlux_Tests` (the RHI directories), `ZenithGraph_Conformance` (the Flux builder through the test access), `Zenith_Bench_BindlessProbe` and `Zenith_Bench_Budgets_RHI`; every file and test is in `Zenith2/Docs/Design/RHI/Tests.md`.

### Supported matrix

| Target | `SUPPORTED` | Why |
|---|---|---|
| `ZenithRHI`, `ZenithRHI_Vulkan` | `ZENITH_GPU` (`win`, `win-asan`, `android-arm64`, `android-x86_64`) | [Z2-N-ARCH-173] |
| `ZenithRHI_D3D12` | `ZENITH_WINDOWS` | [Z2-N-REND-026] |
| `Zenith_Bench_BindlessProbe` | `win`, `android-arm64` | the probe's RTX, WARP, lavapipe and S24 runs |
| `Zenith_Bench_Budgets_RHI` | `win`, `android-arm64` | the reference hardware |

## Public API

### Rules across the API

- Every fallible function returns `Zenith_Result<T, Zenith_RHIError>` (or a Flux error of the RHI-owned Flux headers); no public header includes a Vulkan, D3D12, DXGI, volk, VMA, D3D12MA or RenderDoc header ([Z2-N-CONTRACT-043]).
- `Zenith_RHIDevice`, `Zenith_RHICommandList`, `Zenith_RHISwapchain`, `Zenith_RHIPipelineCache`, `Zenith_RHICaptureTool`, `Zenith_RHIValidationSink` and `Zenith_RHIFaultInjector` are interfaces; every other type is concrete ([Z2-N-CONTRACT-035]).
- The API library exports no factory; each backend exports exactly one ([Z2-N-CONTRACT-038], [Z2-N-ARCH-169]).
- ForbiddenCheck rule F20 (added by the ticket that lands `Zenith_RHIBarrier.h`, "Reconciliation items" R4): an `#include` of `ZenithRHI/Zenith_RHICommandList.h` outside `Zenith2/Engine/ZenithFlux/Internal/**`, the ZenithRHI backends and ZenithRHI's own sources and tests fails; an `#include` of `ZenithRHI/Zenith_RHIBarrier.h` outside `Zenith2/Engine/ZenithFlux/Internal/Graph/**`, the backends and ZenithRHI tests fails ([Z2-N-REND-006], Z2-D-056). `Zenith_RHIDevice.h` forward-declares the command list, so including the device header gives no recording surface.
- Every resource is destroyed through the retirement queue; the public surface has no immediate destroy ([Z2-N-CONTRACT-088]). Four destructions are immediate by name, each after a proof that no GPU reference remains: `SetFramesInFlight` destroys the device's old per-slot objects (backend objects with no handle) right after `WaitIdle`; `ReleaseSurface` destroys the swapchain's images, the swapchain and the surface right after the wait that completed every frame that used them ([Z2-N-ARCH-914] requires them gone before the callback returns); a D3D12 resize destroys the back-buffer entries right after the direct-queue wait (DXGI requires every reference released before `ResizeBuffers`); and destroying a lost device releases every object without a wait ("Device status state machine"). Each calls the retirement listener for every table resource it destroys.

### Header summary

| Header | Key types | Detail |
|---|---|---|
| `Zenith_RHIDevice.h` | `Zenith_RHIDevice`, `Zenith_RHIDeviceDesc`, `Zenith_PfnCreateDevice` | `Zenith2/Docs/Design/RHI/PublicAPIDevice.md → "Zenith_RHIDevice header"` |
| `Zenith_RHICommandList.h`, `Zenith_RHIBarrier.h` | the recording surface and the neutral barrier vocabulary | `Zenith2/Docs/Design/RHI/PublicAPIDevice.md → "Zenith_RHICommandList header"`, "Zenith_RHIBarrier header" |
| `Zenith_RHIQueue.h` | `Zenith_RHITimeline`, `Zenith_RHISubmitBatch` | `Zenith2/Docs/Design/RHI/PublicAPI.md → "Zenith_RHIQueue header"` |
| `Zenith_RHIResources.h`, `Zenith_DeviceHandle.h` | descs, requirements, handles | `Zenith2/Docs/Design/RHI/PublicAPI.md → "Zenith_RHIResources header"`, "Zenith_DeviceHandle header" |
| `Zenith_RHIDescriptors.h`, `Zenith_RHILifetime.h` | heap, retirement | `Zenith2/Docs/Design/RHI/PublicAPI.md → "Zenith_RHIDescriptors header"`, "Zenith_RHILifetime header" |
| `Zenith_RHISwapchain.h`, `Zenith_RHIQueries.h`, `Zenith_RHIDebug.h` | presentation, timestamps, validation and captures | the headers of the same names in `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` |
| `Zenith_RHICapabilities.h`, `Zenith_FormatFeatures.h` | capability negotiation | `Zenith2/Docs/Design/RHI/PublicAPI.md → "Zenith_RHICapabilities header"`, "Zenith_FormatFeatures header" |
| `Zenith_RHIShaderInterface.h`, `Zenith_RHITypes.h`, `Zenith_RHIError.h`, `Zenith_RHIPipelines.h`, `Zenith_RHIFaultInjection.h`, `Zenith_RHIConfig.h` | shared vocabularies | the headers of the same names (`Zenith_RHIFaultInjection.h` and `Zenith_RHIConfig.h` in `PublicAPIDevice.md`, the rest in `PublicAPI.md`) |

### Device factories and the factory pointer type

```cpp
Zenith_Result<Zenith_Owned<Zenith_RHIDevice>, Zenith_RHIError> Zenith_CreateRHI_Vulkan(const Zenith_RHIDeviceDesc& xDesc);   // ZenithRHI_Vulkan/Zenith_RHI_Vulkan.h
Zenith_Result<Zenith_Owned<Zenith_RHIDevice>, Zenith_RHIError> Zenith_CreateRHI_D3D12(const Zenith_RHIDeviceDesc& xDesc);    // ZenithRHI_D3D12/Zenith_RHI_D3D12.h
using Zenith_PfnCreateDevice = Zenith_Result<Zenith_Owned<Zenith_RHIDevice>, Zenith_RHIError> (*)(const Zenith_RHIDeviceDesc& xDesc);   // ZenithRHI/Zenith_RHIDevice.h
```

A Windows game's composition table lists both backends (`{ "Vulkan", &Zenith_CreateRHI_Vulkan }, { "D3D12", &Zenith_CreateRHI_D3D12 }`), an Android game Vulkan alone; at startup step 12 the runtime picks the entry whose name matches `rhi.backend` (case-insensitive), fills the desc from `Zenith_ReadRHIConfig` and the platform, and calls the factory once ([Z2-N-REND-026], `Zenith2/Docs/Architecture/CompositionRoot.md → "The generated composition table"`). A factory failure stops startup with `EXIT_CODE_DEVICE` (4, "a required device could not be created") and prints `Zenith_RHIDeviceReport::m_acRefusal`.

### Handle kinds used

| Kind | Types |
|---|---|
| device handles | `Zenith_RHITextureHandle` (`Flux_TextureHandle`), `Zenith_RHIBufferHandle` (`Flux_BufferHandle`), `Zenith_RHIPipelineHandle` (`Flux_PipelineHandle`), `Zenith_RHIHeapHandle`, `Zenith_RHIQueryPoolHandle`, the kind-erased `Zenith_RHIResourceHandle` |
| operation handles | `Flux_UploadOperation` and `Flux_ReadbackHandle`, each wrapping a `Zenith_OperationHandle` |
| frame-scoped | the render graph's virtual handles (RPROD's, [Z2-N-CONTRACT-095]) |
| none | descriptor indices (`Zenith_RHIDescriptorHandle`) are values, not handles ([Z2-N-CONTRACT-090]) |

## Data structures and formats

### In-memory layouts

| Structure | Size | Where |
|---|---|---|
| `Zenith_DeviceHandle` | 8 bytes: slot 0–31, generation 32–55, kind 56–63 | `Zenith_DeviceHandle.h` |
| `Zenith_RHIQueueValues` | 24 bytes | `Zenith_RHITypes.h` |
| `Zenith_RHIRetiredEntry` | 16 bytes; a pending entry with its values 40 bytes; ring of 65,536 (2.5 MiB) | `Zenith_RHILifetime.h`, Memory.md "Retirement queue" |
| descriptor pending entry | 32 bytes; ring per space of the space capacity | Memory.md "Descriptor heap and slot allocation" |
| resource-table page | 1,024 records | Memory.md "Device resource table" |
| `Flux_ImportedStateEntry`, `Flux_ImportedStateRun` | 48 and 16 bytes | RenderGraphBackend.md "Imported-state table" |
| frame-slot record | frame index (8) + final values (24) | this file "Frame-in-flight rotation" |
| upload segment | 48 bytes per frame slot | Memory.md "Upload ring internals" |
| breadcrumb region | frames in flight × 2 × 4,096 × 4 bytes per queue | Recovery.md "Breadcrumbs" |
| GPU scene records | 128, 128, 32 and 64 bytes | `Zenith2/Docs/Rendering/GpuResources.md → "GPU scene tables"` |

### Committed data files

| File | Format | Specified in |
|---|---|---|
| `Zenith2/Engine/ZenithRHI/Internal/FormatFeatures.table` | the line grammar plus `platform=` | `Zenith2/Docs/Design/RHI/PublicAPI.md → "Zenith_FormatFeatures header"` |
| `Zenith2/Engine/ZenithRHI_Vulkan/Internal/Profiles/VP_ZENITH2_baseline.json` | Vulkan-Profiles JSON schema of the pinned Vulkan-Profiles | Vulkan.md "Baseline profile check" |
| `Zenith2/Engine/ZenithFlux/Tests/ValidationSuppressions.json` | `{ "suppressions": [ { "backend": "vulkan\|d3d12", "messageId": u32, "messageIdName": string, "decision": "Z2-D-NNN", "reason": string } ] }`; an entry without a decision fails loading ([Z2-N-REND-396]) | this file "Validation layers and the clean requirement" |

### Runtime file formats

| File | Format |
|---|---|
| persistent pipeline cache | the Zenith2 header of `Zenith2/Docs/Rendering/ShaderSystem.md → "Persistent pipeline caches"` then the driver payload; Recovery.md "Pipeline cache store" |
| `last_driver.json` | `{ "schemaVersion": 1, "backend": "vulkan\|d3d12", "vendor": u32, "device": u32, "driver": "0x…" }` |
| device-loss report | `zenith2.device-loss` version 1 (Recovery.md "Device-loss report") |
| device report JSON (`user:/logs/rhi_report.json`, produced by `Zenith_WriteRHIDeviceReportJson` and written by MOD's startup after step 12 in development builds, on success and on a refusal) | `{ "schemaVersion": 1, "backend", "adapter", "driver", "tier", "capabilities": { every member of Zenith_RHICapabilities }, "refusals": [ lines ] }` |
| frame dump back-end keys | RenderGraphBackend.md "Frame dump back-end sections" |

## Algorithms and state machines

### Algorithm index

| Algorithm | Heading |
|---|---|
| device creation | this file "Device creation sequence" |
| Vulkan baseline walk and refusal | `Zenith2/Docs/Design/RHI/Vulkan.md → "Baseline profile check"` |
| adapter selection | Vulkan.md "Physical device selection", D3D12.md "Adapter selection" |
| frame-in-flight rotation | this file "Frame-in-flight rotation" |
| frames-in-flight change | this file "Changing the frames in flight" |
| render-idle points | this file "Render-idle points" |
| retirement | this file "Retirement rule"; Memory.md "Retirement queue" |
| descriptor slot reuse | Memory.md "Descriptor heap and slot allocation" |
| upload ring reservation, growth, stall, split, build-node order, deferral, failure | Memory.md "Upload ring internals", "Build-node upload order", "Frame deferral", "Frame failures" |
| retirement drain headroom | Memory.md "Drain headroom" |
| readbacks | Memory.md "Readback pool" |
| residency and pressure | Memory.md "Residency reporting", "Memory pressure responder", "Device memory exhaustion" |
| GPU scene scatter, growth, persistent upload, pools | Memory.md "GPU scene implementation" |
| queue assignment, batching, partitioning, reachability | RenderGraphBackend.md "Step 3 queues lifetimes and reachability", "Step 6 queue assignment", "Step 7 command-list partitions" |
| barrier translation | RenderGraphBackend.md "Neutral barrier translation", Vulkan.md "Barrier translation", D3D12.md "Enhanced barrier translation" |
| queue-family ownership transfers | RenderGraphBackend.md "Queue-family ownership transfers" |
| swapchain recreation | Recovery.md "Swapchain recreation" |
| device loss: detection, breadcrumbs, recover or exit | Recovery.md "Device loss detection", "Breadcrumbs", "Recover-or-clean-exit policy" |
| pipeline-cache load and invalidation | Recovery.md "Pipeline cache store" |

### Device creation sequence

<!-- elaboration -->
The factory runs these steps in order; a failing step unwinds every earlier step in reverse, records its index in `Zenith_RHIDeviceReport::m_uFailedStep`, and returns its error ([Z2-N-CONTRACT-340]). Each step is also a fault-injection site (`ZENITH_RHI_FAULT_SITE_CREATE_STEP` with the index), which is how `Test_RHIDevice_Create_FailsAt<Step>_Unwinds` fails it alone ([Z2-N-CONTRACT-341]).

| # | Step | Vulkan | D3D12 |
|---|---|---|---|
| 1 | `CaptureTool` | RenderDoc library (development builds, `rhi.capture`) | PIX capturer or RenderDoc library |
| 2 | `Loader` | `volkInitializeCustom` through the platform | the D3D12 and DXGI entry points; the debug interfaces when validation is on |
| 3 | `Instance` | `vkCreateInstance` with layers and validation settings | the DXGI factory |
| 4 | `Messenger` | the debug-utils messenger | the debug interfaces: `ID3D12Debug6` when validation is on and `ID3D12DeviceRemovedExtendedDataSettings1` when breadcrumbs are on (the info-queue callback registers inside step 6) |
| 5 | `Adapter` | enumeration, profile walk, selection, capability fill | enumeration, baseline rows, selection, capability fill |
| 6 | `Device` | `vpCreateDevice`, the volk device table | the chosen adapter's device, DRED, the info-queue callback, the Agility version check |
| 7 | `Queues` | queues, timeline semaphores, loss-probe fences | queues, fences, wait events |
| 8 | `Allocator` | VMA | D3D12MA |
| 9 | `ResourceTables` | the five tables | as Vulkan |
| 10 | `DescriptorHeap` | set layout, pool, set; the capacities against the limits ([Z2-N-REND-030]) | the three heaps |
| 11 | `PipelineLayout` | the global layout with the immutable samplers | the root signature and the three command signatures |
| 12 | `NullDescriptors` | slot 0 of each space ([Z2-N-REND-031]) | as Vulkan |
| 13 | `Breadcrumbs` | the per-queue breadcrumb buffers | as Vulkan |

### Frame-in-flight rotation

<!-- elaboration -->
`Flux_FrameLifetime` owns F = `r.framesInFlight` (2 or 3, `Z2-B-RHI-008`) frame slots. Two counters are kept apart: the frame index counts every built frame (submitted, deferred or failed), and `m_ulSubmittedFrames` counts the frames submitted since device creation or the last frames-in-flight change; the frame being built uses slot `m_ulSubmittedFrames mod F`, so a frame that submits nothing (Memory.md "Frame deferral", "Frame failures") consumes a frame index but not a slot.

```cpp
// Zenith2/Engine/ZenithFlux/Internal/Lifetime/Flux_FrameLifetime.h
enum Flux_FrameOutcome : uint8
{
	FLUX_FRAME_OUTCOME_SUBMITTED = 1,      // the default until the build node decides otherwise
	FLUX_FRAME_OUTCOME_DEFERRED,           // DeferFrame: the upload need could not be met without a wait
	FLUX_FRAME_OUTCOME_FAILED,             // FailFrame: Memory.md "Frame failures"
};

/// @contract type Flux_FrameLifetime
/// Ownership: Owned by Flux_Renderer, one per device; destroyed after the last frame retires (shutdown step 6), or
///   by device-loss recovery, on the main thread at a render-idle point.
/// Thread affinity: RENDER_SUBMIT_THREAD, except: PrepareNextFrame for frame 0 (renderer creation) and
///   OnFramesInFlightChanged run on the main thread at a render-idle point ("Render-idle points"); DeferFrame, FailFrame
///   and OnSubgraphBegin run on the Flux.BuildGraph node of the frame; OnSubgraphEnd is the last statement of the
///   frame's Flux.Present; IsRenderIdle, GetFrameOutcome, GetExitRequest and the getters are ANY_THREAD (atomic reads).
/// Invalidation: OnFramesInFlightChanged invalidates every slot record and resets m_ulSubmittedFrames to 0, so slot
///   indices restart at 0; the frame index continues.
/// Units: Frame indices count every built frame; timeline values per queue.
/// Errors: PrepareNextFrame returns ZENITH_RHI_ERROR_DEVICE_LOST (after the loss check of a timeout) and never another code;
///   OnFramesInFlightChanged returns OUT_OF_HOST_MEMORY or OUT_OF_DEVICE_MEMORY with the old F and every old slot object
///   kept (it creates before it destroys). IsRenderIdle is false from OnSubgraphBegin to the end of the frame's
///   Flux.Present, whether or not PrepareNextFrame ran: PrepareNextFrame stores m_bSubgraphActive false before it
///   returns DEVICE_LOST, and OnSubgraphEnd stores it false on every path of Flux.Present.
/// Version: since Z2 Stage 2.
/// @endcontract
class Flux_FrameLifetime
{
public:
	Zenith_Result<void, Zenith_RHIError> PrepareNextFrame();   // steps 1 to 5 below, for the next frame index
	uint64 GetFrameIndex() const;                              // the frame being built (every built frame)
	uint32 GetFrameSlot() const;                               // m_ulSubmittedFrames mod F
	uint32 GetFramesInFlight() const;
	void RecordFinalValues(const Flux_QueueLastUse& xFinal);                  // Submit step 5: the slot record; ++m_ulSubmittedFrames
	void DeferFrame(uint64 ulUploadNeed);                                    // build node: the frame submits nothing
	void FailFrame(Flux_FrameFailure eReason);                                // build node: the frame is cancelled (Memory.md "Frame failures")
	Flux_FrameOutcome GetFrameOutcome() const;                  // read by Flux.Record, Flux.Submit, Flux.Present
	uint8 GetExitRequest() const;                              // 0 (none), or the Flux_FrameFailure the 30th consecutive FailFrame stored
	void OnSubgraphBegin();                                                   // first action of Flux.BuildGraph: m_bSubgraphActive = true
	void OnSubgraphEnd();                                                     // last statement of Flux.Present, on every path: m_bSubgraphActive = false
	bool IsRenderIdle() const;                                  // false from OnSubgraphBegin to the end of the frame's Flux.Present
	Zenith_Result<void, Zenith_RHIError> OnFramesInFlightChanged(uint32 uFrames);   // after SetFramesInFlight
private:
	uint64 m_ulFrameIndex = 0;
	uint64 m_ulSubmittedFrames = 0;
	uint32 m_uConsecutiveFailedFrames = 0;
	std::atomic<bool> m_bSubgraphActive = false;
	std::atomic<uint8> m_eExitRequest = 0;                                            // a Flux_FrameFailure value; 0 while none (Memory.md "Frame failures")
	// the slot records with their Flux_FrameTimestampRecord, the per-slot timestamp pools and upload segments,
	// the deferred need, the outcome
};

/// @contract value Flux_FrameTimestampRecord
/// Ownership: One per frame slot, owned by Flux_FrameLifetime; the name views point into the slot's own name pool
///   (a Zenith_Array<char> from the general heap, capacity kept), never into a graph arena.
/// Thread affinity: RENDER_SUBMIT_THREAD (written at Submit step 5, read at the slot's next preparation), or the main
///   thread at a render-idle point (a frames-in-flight change, "Changing the frames in flight").
/// Invalidation: Rewritten by the next frame submitted in the slot; a frame that submits nothing writes none.
/// Units: Timestamp pairs; queue positions.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Flux_FrameTimestampRecord
{
	uint64 m_ulFrameIndex = 0;                                           // the frame whose queries the slot's pools hold
	uint32 m_auPairCount[FLUX_QUEUE_COUNT] = {};                         // the compiled graph's m_uTimestampCount[q] / 2; 0 when timestamps were off
	Zenith_Span<const Zenith_StringView> m_axPassNames[FLUX_QUEUE_COUNT]; // per queue, by queue position: each timed pass's name with its view suffix
};
```

| Step (`PrepareNextFrame(f)`, render-submit thread, the tail of `Flux.Present` of frame f − 1; the main thread at renderer creation for frame 0) | Action |
|---|---|
| 1 | s = `m_ulSubmittedFrames mod F`; read slot s's record: the final values of the frame last submitted in slot s on every queue (all zero for a slot never submitted) |
| 2 | `WaitCPU` on each queue for its value ([Z2-N-REND-492]); a timeout follows Recovery.md "Device loss detection"; the wait time accumulates in `Flux.Frame.SlotWaitMs` |
| 3 | `GetCompletedValues()` once; `Zenith_RHIRetirementQueue::RetireCompleted`, `Zenith_RHIDescriptorHeap::RetireCompleted`, `Flux_DeferredDeletion` client dispatch ([Z2-N-REND-489]) |
| 4 | `BeginFrameSlot(s)` (command pools, split-barrier events); for each queue q whose `m_auPairCount[q]` in slot s's `Flux_FrameTimestampRecord` is non-zero, `ReadTimestamps` of the frame last submitted in s, then `Flux_PassTimingPublisher::Publish(record, ticks, q)` (RPROD's, `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md → "Timestamp insertion and attribution"`), once per queue; then `ResetTimestampPool` of the slot's pools; readback completion of that frame, upload `BeginFrame(s)` and `ReserveFrame` and the request service (Memory.md "Upload ring internals") ([Z2-N-REND-493]) |
| 5 | the frame is ready to build: `Flux.BuildGraph(f)` of the next iteration may start. When frame f − 1 submitted nothing, steps 1 to 4 were already done for s by the previous preparation, and step 5 alone runs, after the upload reservation of step 4 is re-made (`ReserveFrame` with the deferred need after a deferral, with the prediction after a failure) |

The iteration barrier of `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Frame structure"` puts `Flux.Present(f − 1)` before `Flux.BuildGraph(f)`, so the wait always precedes the build, and no job waits on the GPU ([Z2-N-REND-486]). GPU frames in flight are independent of the one frame of CPU render lag ([Z2-N-REND-494]).

The render-idle flag is cleared on every path out of the render subgraph. `Flux.Present`'s body is the present, the pacing hooks, the capture, `PrepareNextFrame`, then `OnSubgraphEnd()` as its last statement; each of the first four returns to that body when it fails rather than leaving the node, so `OnSubgraphEnd()` also runs after a device loss, where the present and `PrepareNextFrame` are skipped (Recovery.md "Device loss detection"). `PrepareNextFrame` itself stores `m_bSubgraphActive = false` before it returns `ZENITH_RHI_ERROR_DEVICE_LOST` from its step-2 wait. The first prologue after a loss therefore finds `IsRenderIdle()` true, and the entry assert of `Flux_DeviceLossHandler` ("Render-idle points") holds; `Test_Recovery_DeviceLossRecover` asserts it for a loss seen at submit and for one seen by the step-2 wait.

### Changing the frames in flight

<!-- elaboration: pending X89 -->
A change of F is a swapchain recreation (Recovery.md "Recreation state machine"), run at the frame prologue's render-idle point. By then `PrepareNextFrame(f)` has already run at the tail of `Flux.Present(f − 1)` for the old slot s: it called `BeginFrameSlot(s)` on the old pools, `ResetTimestampPool` on the old pools and upload `BeginFrame(s)` and `ReserveFrame` on the old segment. The procedure discards that preparation and re-makes it on slot 0:

| Order | Action |
|---|---|
| 1 | every swapchain is drained; `Zenith_RHIDevice::WaitIdle` (a CPU wait on every queue's last-signalled timeline value, never `vkDeviceWaitIdle`; X89); after it, the completed values equal the last-signalled values |
| 2 | `Zenith_RHIDevice::SetFramesInFlight(uFrames)`: the device creates its new per-slot objects, then destroys the old ones at once (PublicAPIDevice.md "Zenith_RHIDevice header"); a failure keeps the old F and the old objects, and nothing below runs |
| 3 | `Flux_FrameLifetime::OnFramesInFlightChanged(uFrames)`, main thread, asserting `IsRenderIdle()`: (a0) read and publish the timestamps of every frame still pending in the old slots — the last F − 1 submitted frames, whose reads would have happened at their slots' next preparation, which never comes: for each old slot other than s whose `Flux_FrameTimestampRecord` names a frame, `ReadTimestamps` (every value completed at order 1's `WaitIdle`), then `Flux_PassTimingPublisher::Publish(record, ticks, q)` once per queue with a non-zero pair count, so no submitted frame's GPU times are lost; (a) release frame f's upload segment on the old slot s (its reservation holds no byte: the build has not started) and every old segment; (b) enqueue the old slots' timestamp pools on the retirement queue with `GetLastSignalledValues()`; (c) with `GetCompletedValues()` read once, run `RetireCompleted` on the retirement queue, the descriptor heap and the client dispatch, so everything now satisfiable retires, the old timestamp pools included; (d) complete every pending readback (all their frames have completed); (e) create the new slot records (all-zero values, read as "nothing to wait for"), timestamp pools and upload segments for uFrames slots, creating before destroying so a failure keeps the old state; (f) reset `m_ulSubmittedFrames` to 0 and re-run steps 3 to 4 of `PrepareNextFrame` for slot 0 with no wait — `BeginFrameSlot(0)`, `ResetTimestampPool` of slot 0's pools, upload `BeginFrame(0)` and `ReserveFrame(n)`, with n the exact need `DeferFrame` recorded when frame f − 1 was deferred and `PredictReservation()` otherwise (the same rule as step 2 of Memory.md "Upload ring internals", so a deferred need survives the change and frame f is not deferred a second time for it). The request service is not re-run: the pieces it staged are recorded against no slot (Memory.md "Upload ring internals" step 3) and are copied by frame f as before |
| 4 | every swapchain is recreated with the new image count |

The frame index continues (frame f is still the next built), and frame f builds on slot 0 with a fresh reservation and records only into command pools created by `SetFramesInFlight`. Slot s's own record names the frame last submitted in s, whose timestamps step 4 of `PrepareNextFrame(f)` already read and published before the change, so (a0) skips it. `Test_FrameLifetime_FramesInFlightChangeRecreates` asserts each of these.

### Retirement rule

An object — RHI resource, descriptor slot, table slot, pool slot, upload chunk — carries its last-use value per queue: the value signalled by the last submission batch of each queue that accessed it (0 for a queue that never did). It is retired only when every queue's completed value is at or past its value on that queue, and it is never reused before that ([Z2-N-REND-480], [Z2-N-REND-488]). Absent queue classes alias graphics, so their values are graphics values. Retirement runs at step 3 of every frame's preparation, and additionally inside a full-queue wait ([Z2-N-REND-490]).

### Device status state machine

| State | Event | Next | Action |
|---|---|---|---|
| `OK` | any API call returns device lost, or a loss check after a wait timeout finds loss | `LOST` | every call returns `ZENITH_RHI_ERROR_DEVICE_LOST` from now on; no submission reaches the device; `Flux_DeviceLossHandler::OnDeviceLost` once |
| `LOST` | `CollectDeviceFaultReport` | `LOST` | fills the report (allowed only in `LOST`) |
| `LOST` | destruction | (destroyed) | objects released without GPU waits, the retirement listener called for each table resource; live-handle report skipped (the handles are stale by construction) |
| `OK` | destruction | (destroyed) | after `WaitIdle` (shutdown step 5): live-handle report, then release |

The swapchain, upload request, readback, recreation and device-loss machines are in Memory.md and Recovery.md.

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| `Zenith_RHIDevice` | `ANY_THREAD_EXCEPT(Submit, BeginFrameSlot, WaitIdle, CreateSwapchain, SetFramesInFlight)`: `Submit` render-submit; `BeginFrameSlot` render-submit, or the main thread at a render-idle point; the other three the main thread at a render-idle point |
| `Zenith_RHICommandList` | `OWNER_THREAD` (one recording job) |
| `Zenith_RHITimeline` | `ANY_THREAD_EXCEPT(WaitCPU)`; `WaitCPU` is `RENDER_SUBMIT_THREAD`, and the main thread for the permitted waits below |
| `Zenith_RHISwapchain` | `RENDER_SUBMIT_THREAD` for acquire, present and the pacing hooks (including `SetDesiredPresentTime` and `SetSwapInterval`); `MAIN_THREAD` at a render-idle point for recreate, surface release and `SetHDRMetadata` |
| `Zenith_RHIRetirementQueue` | `RequestRelease`, `RequestReleaseWithDescriptor` any thread; `Enqueue` and `RetireCompleted` render-submit, or the main thread at a render-idle point; `DrainReleaseRequests` render-submit alone; `SetRetirementListener` the main thread at a render-idle point |
| `Zenith_RHIDescriptorHeap` | `ANY_THREAD_EXCEPT(Free, RetireCompleted)`; both render-submit, or the main thread at a render-idle point |
| `Zenith_RHIRetirementListener` (`Flux_DeferredDeletion`) | the thread that destroys the resource: render-submit, or the main thread at a render-idle point |
| `Zenith_RHIPipelineCache`, `Zenith_RHIValidationSink`, `Zenith_RHIFaultInjector` | `ANY_THREAD` |
| `Flux_FrameLifetime` | `RENDER_SUBMIT_THREAD`, with the exceptions of its block ("Frame-in-flight rotation") |
| `Flux_GraphSubmitter` | `RENDER_SUBMIT_THREAD` |
| `Flux_DeferredDeletion` | `Enqueue` with explicit values and `RetireCompleted`: render-submit, or the main thread at a render-idle point; `Enqueue` of a resource without values: any thread (it is a `RequestRelease`) |
| `Flux_ImportedStateTable` | `OWNER_THREAD`: the render subgraph's nodes in sequence and the main thread at render-idle points (RenderGraphBackend.md "Imported-state table") |
| `Flux_UploadRing` | `ReserveFrame`, `BeginFrame`: render-submit, or the main thread at a render-idle point (frame 0, a frames-in-flight change); `DeclareFrameData`, `ExtendReservation`, `GetFrameData`: the build node; `Allocate`: any frame thread after `ExtendReservation`; `RequestUpload`: any worker |
| `Flux_ReadbackHandle`, `Flux_UploadOperation` | `OWNER_THREAD` (the requester) |
| `Flux_SwapchainManager`, `Flux_MemoryConsumer` callbacks | `MAIN_THREAD` (frame prologue, a render-idle point: the manager's `ProcessPendingRecreations` from `Flux_Renderer::RunPrologue`, the consumer's callbacks from `Zenith_MemoryGovernor::Update`, both in prologue step 4; "Render-idle points", assumption pending MOD, ARCH (X128)), except the manager's acquire and present calls, made on the render-submit thread in `Flux.Submit` and `Flux.Present` |
| `Flux_DeviceLossHandler` | `OnDeviceLost`: `RENDER_SUBMIT_THREAD`, once per loss; `RunPrologue`: `MAIN_THREAD`, called by `Flux_Renderer::RunPrologue` in prologue step 4, a render-idle point (assumption pending MOD, ARCH (X128)); `GetState`: `ANY_THREAD` (an atomic read, the "device-loss flag" the runtime polls through `Flux_Renderer::GetDeviceLossState`) |
| `Flux_PersistentResources` | `ANY_THREAD` |
| `Flux_GPUScene` range listener | `SetRangeListener`: `MAIN_THREAD` at a render-idle point; `Flux_GPUSceneRangeListener` members: the thread running `ApplyDeltas` (the `Flux.BuildGraph` node), never concurrently with another `Flux_GPUScene` member (`Zenith2/Docs/Design/RHI/Memory.md → "Material-slot range listener"`) |

### Render-idle points

<!-- elaboration: pending X71(b) -->
A render-idle point is a stretch of the main thread during which the render subgraph is not running and cannot start until the point returns, so any call this area otherwise reserves for the render-submit thread cannot overlap one made there. There are exactly five:

| Point | Why the render subgraph cannot run |
|---|---|
| renderer creation (startup step 13) and renderer destruction (shutdown step 6) | the frame loop has not started, or has stopped (shutdown step 2 completed the last render subgraph) |
| shutdown step 5 | as above |
| the frame prologue, before `RunFrame` (swapchain recreation, a frames-in-flight change, the pressure callbacks, the device-loss handler) | iteration k + 1 begins only after every subgraph of iteration k completed ([Z2-N-ARCH-386]), and the prologue precedes the `RunFrame` that starts the next render subgraph |
| the Android surface-release callback, after the runtime completed the in-flight render subgraph ([Z2-N-ARCH-914]) | the main thread is inside the callback, so the next `RunFrame` has not been called |
| device-loss recovery (`LOST` to `RESUMING`) | the frame loop runs `RunFrame` with `bRunRender = false`, so the render subgraph is empty |

<!-- assumption: pending MOD, ARCH (X128) -->
Rows 3 and 5 rest on three things no core document provides yet (cross-document item X128; cross-area note R19; "Reconciliation items" R22):

1. **A prologue hook.** [Z2-N-ARCH-437] makes the prologue exactly eight steps, and none of them hosts swapchain recreation, a frames-in-flight change or the device-loss handler (step 4 hosts the governor's pressure callbacks alone). This design assumes one renderer call, `void Flux_Renderer::RunPrologue()` (`MAIN_THREAD`), which the composition root makes in step 4 immediately after `Zenith_MemoryGovernor::Update` returns, so the pressure callbacks and the render-device housekeeping share one step. `Flux_Renderer` is RPROD's type (`Zenith2/Docs/Design/RPROD/Renderer.md`); the member forwards to the RHI-owned components, in this order: `Flux_DeviceLossHandler::RunPrologue()` (the prologue events of Recovery.md "Device-loss state machine", including the frame-failure exit request of Memory.md "Frame failures"); then, only while the handler's state is `FLUX_DEVICE_LOSS_STATE_NORMAL`, `Flux_SwapchainManager::ProcessPendingRecreations()`, which runs every marked recreation and a frames-in-flight change ("Changing the frames in flight"), and the `SURFACE_STALE` check of Recovery.md "Android surface loss".
2. **The device-loss flag.** Recovery.md calls it "the runtime's device-loss flag". This design assumes it is the polled status `Flux_DeviceLossState Flux_Renderer::GetDeviceLossState() const` (`ANY_THREAD`, an atomic read), which forwards to `Flux_DeviceLossHandler::GetState()`. `Flux_DeviceLossState` is declared in `Zenith2/Engine/ZenithFlux/Flux_QueueValues.h` (`Zenith2/Docs/Design/RHI/PublicAPIDevice.md → "RHI-owned ZenithFlux public headers"`). `OnDeviceLost` (render-submit thread) stores `FLUX_DEVICE_LOSS_STATE_LOST` before it returns, so the prologue that follows the loss already reads it.
3. **The `bRunRender` term.** `Zenith2/Docs/Architecture/CompositionRoot.md → "The frame loop"` step 2 passes `bRunRender = !headless && frameIndex > 0`, and `Zenith2/Docs/Design/MOD.md` → "Device loss and headless" names no device-loss condition. This design assumes step 2 becomes `bRunRender = !headless && frameIndex > 0 && renderer.GetDeviceLossState() == FLUX_DEVICE_LOSS_STATE_NORMAL`, read after step 4's `RunPrologue`, so the render subgraph is empty from `LOST` through `RESUMING` and while `EXITING`.

Until X128 is applied, a MOD ticket that lands the frame loop calls the two members as written here, and the RHI ticket that lands the handler cites this block.
<!-- /assumption -->

Every Flux entry point that runs at a render-idle point (`Flux_SwapchainManager::ProcessPendingRecreations`, `SetHDRMetadata` and its surface callbacks, `Flux_FrameLifetime::OnFramesInFlightChanged`, the frame-0 `PrepareNextFrame`, `Flux_DeviceLossHandler::RunPrologue`, renderer creation and destruction) asserts `Flux_FrameLifetime::IsRenderIdle()` on entry; `Test_FrameLifetime_RenderIdleEntryAssertsInsideSubgraph` (`z2-death-assert`) calls one from inside a render subgraph. The contract blocks that admit the main thread (`Zenith_RHIRetirementQueue`, `Zenith_RHIDescriptorHeap`, `Zenith_RHIRetirementListener`, `Zenith_RHIDevice`, `Zenith_RHISwapchain`, `Zenith_RHITimeline`, `Flux_FrameLifetime`) name this heading, so the ApiContractCheck gate step reads the exception in the block rather than in prose.

### Permitted main-thread GPU waits

<!-- elaboration: pending X71(b) -->
[Z2-N-REND-486] says the render-submit thread is the one thread that waits on a GPU timeline from the CPU, and `Zenith2/Docs/Rendering/GpuResources.md → "Swapchain recreation"` step 2 puts the drain there. This design admits exactly five further waits, each on the main thread at a render-idle point, and none of them a frame wait:

| Wait | Where | Why it is not a frame wait | Bound |
|---|---|---|---|
| swapchain drain | Recovery.md "Recreation state machine" (`DRAINING`), and the `WaitIdle` of a frames-in-flight change ("Changing the frames in flight") | runs in the frame prologue before `RunFrame`; the frame that follows starts from drained slots, and no job waits | `rhi.gpuWaitTimeoutMs` per wait and the loss check |
| Android surface release | Recovery.md "Android surface loss" (`OnReleaseSurface`) | the platform callback runs on the main thread after the in-flight render subgraph completed; the handshake requires the surface's work complete before it returns ([Z2-N-ARCH-914]) | the handshake's remaining budget of the 2,000 ms of [Z2-N-ARCH-916], never the 5-timeout loss rule |
| device-loss recovery | Recovery.md "Device-loss state machine" (`RECOVERING`): no wait on the lost device; the new device's first `PrepareNextFrame` for frame 0 | frame 0 of a new device waits on nothing (every slot is empty), so the call only establishes the slot records | as the first row |
| shutdown | "Startup and shutdown" (step 5 `WaitIdle`) | the frame loop has stopped; shutdown step 2 completed the last render subgraph | as the first row |
| a full-ring retirement `Enqueue` | a render-idle `Enqueue` (swapchain recreation's old images, a frames-in-flight change's old timestamp pools) that finds the ring full: `WaitCPU` for the oldest entry's values (Memory.md "Retirement queue") | unreachable while the assumption of Memory.md "Drain headroom" holds; when it does not, the oldest entry's values were signalled (`Enqueue` asserts it), so the wait completes and no frame waits on it | as the first row |

The Rendering patch that admits them is "Reconciliation items" R11 (cross-document item X71(b)); until it lands, the five waits are this design's elaboration.

### Frame-graph nodes and declared access

| Node (`Zenith2/Docs/Architecture/CPUFrameGraph.md → "Frame structure"`, `FRAME_SUBGRAPH_RENDER`) | Affinity | RHI-area work |
|---|---|---|
| `Flux.BuildGraph` | `JOB_AFFINITY_ANY` | in this order (Memory.md "Build-node upload order"): `OnSubgraphBegin`; the acquire list's imported-state registrations; GPU scene deltas into the pending store; setup, where every per-frame data producer declares its bytes (`DeclareFrameData`) and writes nothing; compile steps 3, 6, 7 (with RPROD's); upload `ExtendReservation` (the deferral or failure decision); on `FITS`/`EXTENDED` alone, the frame-data writes (scatter records and data, per-frame data), the descriptor writes for the frame's views ([Z2-N-REND-052]) and the frame-dump model copy; on `DEFERRED` or `TOO_LARGE`, `DeferFrame` or `FailFrame`, `Flux_GPUScene::OnFrameDeferred` and the failure of the frame's readbacks, with nothing written |
| `Flux.Record` | `JOB_AFFINITY_ANY`, `ParallelFor` over partitions | recording ("Recording" of RenderGraphBackend.md) |
| `Flux.Submit` | `JOB_AFFINITY_RENDER_SUBMIT` | acquire, batch submission, last-use commit, release drain, End phase |
| `Flux.Present` | `JOB_AFFINITY_RENDER_SUBMIT` | present, pacing hooks, captures, `PrepareNextFrame`, then `OnSubgraphEnd` on every path ("Frame-in-flight rotation") |

The render subgraph reads only the sealed `Flux_RenderWorld` of its frame and writes no simulation state ([Z2-N-ARCH-385]). Pipeline creation jobs (RPROD's PSO schedule) and asset loader jobs call the device's `ANY_THREAD` members; they never wait on the GPU.

### Lock-order rows added by RHI

The rows 400 and 450 exist in `Zenith2/Docs/Contracts.md → "Threading and concurrency contract"` (table "Lock ordering table"); the landing tickets add the rest in their diffs ([Z2-N-CONTRACT-320]); every constant is declared in `ZenithRHI/Internal/Zenith_RHILocks.h` or `ZenithFlux/Internal/Flux_Locks.h`:

| Level | Constant | Owner | Guards | Short-hold | Same level by address | Acquired on |
|---|---|---|---|---|---|---|
| 400 | `uLOCK_LEVEL_RHI_QUEUE_SUBMIT` | ZenithRHI | submission to and presentation on one queue | yes | yes | render-submit thread, the main thread during recovery |
| 410 | `uLOCK_LEVEL_RHI_RESOURCE_TABLE` | ZenithRHI | a resource table's page growth and free list | yes | no | any thread |
| 450 | `uLOCK_LEVEL_RHI_RETIREMENT` | ZenithRHI | the retirement ring, descriptor free and pending lists | yes | no | any thread |
| 460 | `uLOCK_LEVEL_RHI_DESCRIPTOR_WRITE` | ZenithRHI | Vulkan descriptor-set updates | yes | no | any thread |
| 470 | `uLOCK_LEVEL_RHI_PIPELINE_LIBRARY` | ZenithRHI | D3D12 `StorePipeline` | yes | no | pipeline creation jobs |
| 480 | `uLOCK_LEVEL_FLUX_PERSISTENT` | ZenithFlux | persistent-resource creation bookkeeping (the acquire list, the upload handles) | yes | no | asset loader jobs, render-submit thread, the build node (taking the acquire list) |

The imported-state table has no row and holds no mutex: its single-writer rule (RenderGraphBackend.md "Imported-state table") keeps every member on the render subgraph's sequential nodes or a render-idle point, and asset loader jobs never reach it. Row 450 is also acquired on the main thread at render-idle points.

### Bounded queues and full policies

| Queue | Capacity | Policy | Counters |
|---|---|---|---|
| retirement ring | 65,536 (`uZENITH_RHI_RETIREMENT_CAPACITY`) | the enqueuing thread waits for the oldest ([Z2-N-REND-490]); `Enqueue` asserts its values were signalled, so the wait always completes; unreachable in a correct program, because the drain stops `uFLUX_RETIREMENT_DRAIN_HEADROOM` = 512 entries short and direct enqueues are at most 105 per interval over at most 3 intervals under the one-prologue assumption stated there (Memory.md "Drain headroom") | `rhi.retirement.full` (the full-ring waits), `rhi.retirement.dropped` (always 0: the policy never drops), `rhi.retirement.high_water` (the peak of `GetPendingCount()` in the frame) ([Z2-N-CONTRACT-345]); `RHI.Retirement.FullWaits` is the profiler alias of `.full` (re-published as `Flux.Lifetime.DeletionQueueFullWaits`) |
| release requests | the sum of the resource-table capacities, `RESOURCE` entries alone (a descriptor slot released with its resource rides in the entry) | `QUEUE_FULL_POLICY_FATAL`; it cannot fill in a correct program because each live resource is released once and at most the table capacities are live; `CLIENT` retirements never use it (at most 8 batched entries per submitted frame), and the drain moves requests only while the ring holds fewer than 65,536 − 512 entries, never waiting | `rhi.release_requests.*` |
| upload request queue | 4,096 | `QUEUE_FULL_POLICY_REJECT` ([Z2-N-REND-497]) | `flux.upload_requests.*` |
| upload ring | up to `Z2-B-RHI-006` | `QUEUE_FULL_POLICY_GROW_THEN_BLOCK` on the render-submit thread ([Z2-N-CONTRACT-347]) | `Flux.Upload.Exhausted`, `Flux.Upload.StallMs` |
| readback pool | `Z2-B-RHI-011` | reject with `POOL_FULL`, retry next frame (BR-DEFER) | `Flux.Readback.Deferred` |
| descriptor spaces | tier capacities (`Z2-B-RHI-009`) | reject (BR-REFUSE) | `Flux.Descriptors.Exhausted` |
| resource tables | per kind | reject `TABLE_FULL` | `RHI.Resources.TableFull` |

### Startup and shutdown

| Moment | Action |
|---|---|
| startup step 12 | the device from `rhi.backend`; failure: `EXIT_CODE_DEVICE` (4) with the refusal text |
| startup step 13 | the renderer, a render-idle point: `Flux_DeferredDeletion` set as the retirement listener, swapchains, `Flux_FrameLifetime` (frame 0 prepared on the main thread), upload ring, readback pool, GPU scene, pipeline-cache load, memory consumer registration |
| headless | steps 12 and 13 skipped ([Z2-N-ARCH-867]); no RHI object exists |
| shutdown step 2 | the in-flight render subgraph completes |
| shutdown step 5 | `WaitIdle` on every queue (a permitted main-thread wait, above), then every retirement satisfied and retired; readback leaks reported |
| between steps 5 and 9 | the renderer is destroyed with the module subsystems of step 6 (swapchains, GPU scene, upload ring, pipeline-cache final write), a render-idle point; the retirement listener is cleared first — "Reconciliation items" R8 asks Contracts' table to name it |
| shutdown step 9 | the device: live-handle report, then release ([Z2-N-CONTRACT-089]) |

### Error codes

`Zenith_RHIError` is declared in `Zenith2/Docs/Design/RHI/PublicAPI.md → "Zenith_RHIError header"`. Where each is returned:

| Enumerator | Returned by |
|---|---|
| `LOADER_UNAVAILABLE` | factory steps 2 and 6 (Agility mismatch) |
| `BACKEND_UNSUPPORTED` | the D3D12 factory on a Windows without D3D12 |
| `NO_ADAPTER` | step 5 with no candidate |
| `PROFILE_UNSUPPORTED` | step 5 with every candidate refused; step 10 capacities |
| `DEVICE_CREATE` | steps 3 and 6 |
| `OUT_OF_HOST_MEMORY`, `OUT_OF_DEVICE_MEMORY` | any creating member |
| `DEVICE_LOST` | every member after loss |
| `INVALID_DESC` | descs outside their domain; placement violations; a swap interval outside 1 to 4 |
| `TABLE_FULL` | resource creation at a table's capacity |
| `STALE_HANDLE` | every member resolving a handle |
| `DESCRIPTOR_SPACE_FULL` | `Zenith_RHIDescriptorHeap::Allocate` |
| `SURFACE_UNAVAILABLE`, `SWAPCHAIN_OUT_OF_DATE`, `SWAPCHAIN_CREATE`, `TIMEOUT` | the swapchain |
| `PIPELINE_CREATE`, `SHADER_BLOB_INVALID` | pipeline creation |
| `PIPELINE_CACHE_REJECTED` | never escapes: `CreatePipelineCache` degrades to an empty cache and reports `WasLoadRejected` |
| `FEATURE_UNAVAILABLE` | mesh pipelines without mesh shaders, present wait, a timestamp pool on a 0-bit queue, a desired present time without `VK_GOOGLE_display_timing` (and on D3D12), HDR metadata without `VK_EXT_hdr_metadata` |

The Flux errors of this area — `Flux_UploadError`, `Flux_ReadbackError`, `Flux_DescriptorError`, `Flux_ResourceError`, `Flux_GPUSceneError` — are mapped from `Zenith_RHIError` at the Flux boundary ([Z2-N-CONTRACT-463]). Each enumeration has `Zenith_ToString` and `Zenith_GetErrorCategory`; `Flux_UploadError` and `Flux_DescriptorError` are Rendering's declarations, whose first enumerators this design numbers from 1 in their landing tickets ([Z2-N-CONTRACT-465]).

### Asserts results and fatal errors

| Condition | Treatment |
|---|---|
| a stale or foreign handle | `Zenith_Result` error in every build, never an assert ([Z2-N-CONTRACT-062]) |
| recording misuse (a barrier inside a rendering scope, a draw outside one, a double descriptor write, an unregistered import) | `ZENITH_ASSERT`; the call is ignored in builds without asserts |
| `Allocate` beyond the frame reservation | `FLUX_UPLOAD_ERROR_RESERVATION_EXHAUSTED` (programmer error, [Z2-N-REND-496]) |
| `Allocate` or `GetFrameData` before the frame's `ExtendReservation`, or `ExtendReservation` after an allocation | `ZENITH_ASSERT` (the invariant of Memory.md "Build-node upload order") |
| a render-idle entry point called inside a render subgraph | `ZENITH_ASSERT(IsRenderIdle())` ("Render-idle points") |
| concurrent use of the imported-state table | `ZENITH_ASSERT` of its overlap detector |
| a release-request queue overflow | `ZENITH_FATAL(FATAL_CODE_QUEUE_FULL)` (only a double release or a leak of table capacity reaches it) |
| a retirement `Enqueue` whose values exceed the last signalled | `ZENITH_ASSERT` (the full-ring wait would otherwise wait for a submission that never happens) |
| device loss whose clean exit (`EXIT_CODE_DEVICE_LOST`, 6) cannot finish in 10 s | `ZENITH_FATAL(FATAL_CODE_DEVICE_LOST)` ([Z2-N-CONTRACT-478] table, narrowed by X72) |
| untrusted input (cache files, the profile and table files at build time) | results and build errors, never asserts ([Z2-N-CONTRACT-472]) |
| Luau | the RHI has no Luau binding; script misuse cannot reach it |

## Diagnostics and profiling

### Counters

Every counter is emitted through `Zenith_DispatchCounter` (`Zenith2/Docs/Contracts.md → "No mutable global state"`) once per frame from `PrepareNextFrame` unless noted.

| Counter | Meaning |
|---|---|
| `RHI.Memory.DeviceLocalUsageBytes`, `RHI.Memory.DeviceLocalBudgetBytes`, `RHI.Memory.Heap<N>.UsageBytes`, `.BudgetBytes` | residency |
| `RHI.Memory.Category.<RenderTargets\|Tables\|Pools\|Textures\|Meshes\|UploadRing\|PSOs\|Other>Bytes` | the eight categories |
| `RHI.Resources.Live<Textures\|Buffers\|Pipelines\|Heaps>`, `RHI.Resources.TableFull` | tables |
| `RHI.Descriptors.Occupancy.<TextureSRV\|TextureUAV\|BufferSRV\|BufferUAV>` | per space ([Z2-N-REND-030]) |
| `RHI.Submit.Batches`, `RHI.Submit.CommandLists`, `RHI.Wait.CpuMs`, `RHI.Wait.Timeouts` | submission |
| `RHI.Validation.Messages` | per message (development) |
| `RHI.Swapchain.Recreations`, `RHI.Swapchain.OutOfDate`, `RHI.Swapchain.Occluded` | presentation |
| `RHI.Device.Losses`, `RHI.Device.Recoveries`, `RHI.Breadcrumbs.Overflow` | recovery |
| `rhi.retirement.full`, `rhi.retirement.dropped`, `rhi.retirement.high_water` | the retirement ring's queue triple of [Z2-N-CONTRACT-345]: full-ring waits in `Zenith_RHIRetirementQueue::Enqueue` (read through `GetFullWaitCount()`), drops (always 0), and the frame's peak `GetPendingCount()` (sampled after every `Enqueue` and drain, published and reset at `PrepareNextFrame`), so the on-screen profiler's queue view shows the drain headroom's margin |
| `RHI.Retirement.FullWaits` | the profiler alias of `rhi.retirement.full` |
| `RHI.Pipelines.CacheHits`, `RHI.Pipelines.CacheMisses` | per pipeline creation, from the backend's cache feedback; the same flag is returned as `Zenith_RHIPipelineCreateResult::m_bCacheHit`, from which Flux derives `Flux.PSO.CacheHits`/`CacheMisses` ([Z2-N-REND-212]; X71(f), pending) |
| `Flux.Lifetime.PendingRetirements`, `Flux.Lifetime.DeletionQueueFullWaits` (re-published from `RHI.Retirement.FullWaits`), `Flux.Frame.SlotWaitMs` | lifetime |
| `Flux.Upload.Exhausted`, `Flux.Upload.StallMs`, `Flux.Upload.ReservationDeferredFrames`, `Flux.Upload.RingBytes`, `Flux.Upload.UsedBytes`, `flux.upload_requests.full/.dropped/.high_water` | the upload ring ([Z2-N-REND-495]) |
| `RHI.Upload.OverCap` | frames failed because their exact upload need exceeded `Z2-B-RHI-006` (Memory.md "Frame failures") |
| `Flux.Readback.Pending`, `Flux.Readback.Deferred` | readbacks (`Deferred` counts `POOL_FULL` refusals and readbacks failed by a frame that submitted nothing) |
| `Flux.Descriptors.Exhausted` | descriptors |
| `Flux.GPUScene.TableFull`, `Flux.GPUScene.ScatterRecords`, `Flux.GPUScene.TableBytes` | the GPU scene |
| `Flux.Memory.DeviceOOM`, `Flux.Memory.OOMSkippedFrames` | exhaustion |
| `Flux.Graph.PartitionTargetRaised`, `Flux.Graph.TransferOnlyBatches` | the back end |
| `Flux.PipelineCache.Discarded` | caches |

### On-screen profiler rows

The profiler is DEVT's (`Zenith2/Docs/Design/DEVT.md`); the RHI area supplies: GPU time per pass per queue (from the timestamp pools RPROD inserts; the RHI supplies pools, period and calibration so the GPU zones align with the CPU timeline, [Z2-N-REND-312]); the queue timeline (batches, waits, signals per frame from the dump model); the memory panel (the categories, heap budgets, descriptor occupancy, upload ring size and use, readback pool, and the transient memory with and without aliasing and the savings, which RPROD computes and this area's reporter publishes, [Z2-N-REND-445]); the counters above.

### Log categories and messages

`rhi.device`, `rhi.vulkan`, `rhi.d3d12`, `rhi.validation`, `rhi.memory`, `rhi.swapchain`, `rhi.recovery`, `rhi.capture`, `flux.lifetime`, `flux.upload`, `flux.gpuscene`, `flux.pipelinecache`, each declared with `ZENITH_LOG_CATEGORY` and compiled minimum `LOG_LEVEL_TRACE` (`Zenith2/Docs/Design/CORE/Diagnostics.md → "Structured logging"`). The fixed message texts are those quoted in the split files (refusals, loader missing, layer unavailable, output fallback, driver changed, device lost, capture unavailable).

### Validation layers and the clean requirement

| Rule | Mechanism |
|---|---|
| modes | `rhi.validation` (`off`, `standard`, `sync`, `gpu`), realised per Vulkan.md "Instance creation" and D3D12.md "Debug layer and validation" |
| tests | every test executable that creates a device sets `SYNC` on Vulkan and `GPU` on D3D12 and installs `Zenith_RHITestValidationSink`, which fails the test on any unsuppressed warning or error ([Z2-N-REND-382], [Z2-N-REND-395]) |
| a layer missing in a test executable | the test fails at start-up (`validation layer required by the test harness is unavailable`), so a CI host without the layer can never pass silently |
| suppressions | `Zenith2/Engine/ZenithFlux/Tests/ValidationSuppressions.json`, one message ID per entry with a decision ID ([Z2-N-REND-396]) |
| development runs | `standard` in `win-debug` by default; messages are logged in `rhi.validation` |
| Shipping | the key is ignored; no layer is loaded |

### Frame-dump entries

The back-end keys `passes[].queue`, `partition`, `barriersBefore`, `batches`, `device` and `memory.heapBudgets` (RenderGraphBackend.md "Frame dump back-end sections"); the device-loss report's frame dumps (Recovery.md "Device-loss report").

### Captures

RenderDoc on both backends and PIX on D3D12, development builds only, triggered by `r.capture <frames>` or the profiler button (Recovery.md "Captures").

### Telemetry

Per run: backend, adapter, driver, tier, frames in flight (the `framesInFlight` field of the evidence record, [Z2-N-BUDGET-102]), the cold-PSO-cache flag ([Z2-N-REND-989]), device losses and recoveries with their reasons, swapchain recreations, the upload-ring stall and deferral counts (`Z2-B-RHI-007`), the failed frames by reason (`RHI.Upload.OverCap`, `Flux.Memory.OOMSkippedFrames`), a frame-failure exit with its reason and the consecutive count that triggered it (Memory.md "Frame failures"; a device-loss report is not written for it and `RHI.Device.Losses` does not count it), and the peaks of the memory categories.

## Authoring and cooking

The RHI area has no editor surface, importer or cook step, and no authoring document. Its committed data — `FormatFeatures.table` and `VP_ZENITH2_baseline.json` — are edited by hand in the ticket that changes the baseline, together with the specification's tables ([Z2-N-REND-038]); the generated profile files are regenerated by `Zenith2/Engine/ZenithRHI_Vulkan/Internal/Profiles/Generate-Profiles.ps1` in the same diff. Shader compilation, reflection, permutations and PSO precache sets, which feed pipeline creation, are the offline shader tool's and RPROD's (`Zenith2/Docs/Rendering.md → "Shader system and offline compilation"`, `Zenith2/Docs/Rendering/ShaderSystem.md → "The ZenithShaderCompiler tool"`).

## Failure handling

| Failure | Defined behaviour |
|---|---|
| the Vulkan loader, `d3d12.dll` or the Agility runtime missing | `LOADER_UNAVAILABLE`, `EXIT_CODE_DEVICE` (4) with the message; on Windows the other backend is not tried automatically (the configuration names one) |
| no adapter passes the baseline | `PROFILE_UNSUPPORTED`, `EXIT_CODE_DEVICE` (4), the exact refusal lines ([Z2-N-REND-020], [Z2-N-REND-024]) |
| a validation layer requested but absent | development run: degrade to `off` with a warning; test executable: fail |
| a validation error in a development run | logged in `rhi.validation`; with `breakOnError` a debugger break |
| a validation error in any test | the test fails (the clean requirement) |
| host memory exhaustion | the OOM handler; `OUT_OF_HOST_MEMORY` to the caller |
| device memory exhaustion | Memory.md "Device memory exhaustion": pressure `CRITICAL`, release and retry, then a frame failure (`FLUX_FRAME_FAILURE_DEVICE_OOM`); the 30th consecutive failed frame requests the frame-failure exit with the message `The graphics device ran out of memory. Your progress has been saved.` |
| a resource table or descriptor space full | `TABLE_FULL` / `DESCRIPTOR_SPACE_FULL`, counters, no eviction |
| an upload request larger than the cap | split when splittable, else `FLUX_UPLOAD_ERROR_TOO_LARGE` (BR-REFUSE) |
| a frame whose exact upload need exceeds the cap | a frame failure, never a deferral (Memory.md "Frame failures"): the frame is cancelled (nothing recorded, submitted or presented; the pending store kept; histories untouched), `RHI.Upload.OverCap` increments, one `flux.upload` error line per contributor; the 30th consecutive failed frame requests the frame-failure exit (Recovery.md "Device-loss state machine", the `NORMAL` row "exit requested by a frame failure"): the message `The renderer could not fit a frame's uploads into memory. Your progress has been saved.`, an autosave, `EXIT_CODE_DEVICE_LOST` (6), no device-loss report |
| a frame deferred at the upload cap | Memory.md "Frame deferral": nothing recorded, submitted or presented; its readbacks fail with `FLUX_READBACK_ERROR_FRAME_DEFERRED` and are re-requested |
| the readback pool full | `POOL_FULL`, retry next frame (BR-DEFER) |
| a wait timeout | loss check; 5 consecutive timeouts on one value declare loss |
| swapchain out of date, surface lost, minimised | Recovery.md "Swapchain recreation" |
| device loss | Recovery.md "Recover-or-clean-exit policy": recovery, or the clean exit with `EXIT_CODE_DEVICE_LOST` (6) |
| a corrupt, stale or rejected pipeline cache | discarded and rebuilt; never a failed start ([Z2-N-REND-205]) |
| an interrupted cache write | the previous file stays ([Z2-N-REND-206]) |
| a capture tool missing | captures unavailable, logged |
| cancellation of an upload or readback | the state machines of Memory.md; resources released on the executor before `CANCELLED` ([Z2-N-CONTRACT-335]) |
| a budget breach | the row's breach class: `Z2-B-RHI-006` BR-REFUSE, `-007` BR-ACCEPT, `-008` BR-INVALID, `-009` BR-REFUSE, `-011` BR-DEFER, the memory rows BR-GOVERNOR, the CPU rows and `-010` BR-ACCEPT, each exercised by the tests of Tests.md ([Z2-N-BUDGET-041]) |

## Test strategy

### Test targets and gate steps

| Target | Gate step | Presets |
|---|---|---|
| `ZenithRHI_Tests`, `ZenithRHI_Vulkan_Tests`, `ZenithRHI_D3D12_Tests`, `ZenithRHI_Conformance`, `ZenithFlux_Tests` | `Test` | `win-debug`, `win-release`; built on Android presets, never run there |
| the same | `Sanitizers -Preset win-asan` | when a diff reaches the `Zenith2Memory` paths, and in stage acceptance |
| `ZenithGraph_Conformance` (the Flux builder) | `Test` | `win` presets |
| `Zenith_Bench_BindlessProbe --smoke`, `Test_BudgetHarness_RHI_Smoke` | `Test` | `win` presets |
| the RHI conformance suite as the RHI half of backend parity | `BackendParity` (Stage 3 activation, `Zenith2Render`) | `win-release` |
| shader-related lints on the probe shaders | `ShaderChecks` | `win` |

### Required test groups

`Zenith2/Docs/Design/RHI/Tests.md` names every test. The §5.2 required tests are the GPU scene tests of `Zenith2/Docs/Rendering/GpuResources.md → "GPU scene tests"` (skinned meshes drawn in depth, base and shadow passes the frame they are skinned; GPU particles simulated then drawn; procedural geometry regenerated mid-frame; a VT tile uploaded then sampled; two tables referencing one texture during a streaming swap; a slot freed and reused across frames in flight; a pass writing a persistent read-only resource rejected), each with validation clean, plus the upload-ring, readback, retirement, descriptor, recovery and pipeline-cache tests of Tests.md; the D-SLANG probe cases, including `Test_BindlessProbe_StorageFormats` for the format features; and the failure-injection tests of every factory step.

### Headless CI devices

Every GPU test runs on WARP (D3D12, from the pinned NuGet package) and lavapipe (Vulkan, the pinned ICD selected through `VK_DRIVER_FILES` and `rhi.adapter=software`) ([Z2-N-REND-034]); a test that needs an absent capability (mesh shaders) exercises the fallback and records the skip reason ([Z2-N-REND-035]). Timings measured there are never budget evidence ([Z2-N-BUDGET-125]).

### CI windows for GPU tests

<!-- elaboration: pending X77 -->
A headless run in the sense of [Z2-N-ARCH-785] and [Z2-N-ARCH-867] creates no window, no device and no render subgraph, and the CI integration smoke stays that way. GPU test executables are not headless runs: they create a device on WARP or lavapipe directly, and those that need a swapchain (the rows of Tests.md marked "(hidden window)") first create a hidden window through `Zenith_Window` with `Zenith_WindowDesc::m_bHidden = true`, which PLAT realises with `SDL_WINDOW_HIDDEN` (`Zenith2/Docs/Design/PLAT/Platform.md` → "Windows", under the PLAT assumption above). The window has a real `HWND`, so D3D12 creates a flip-model swapchain on WARP, and Vulkan creates a `VkSurfaceKHR` through `VK_KHR_win32_surface`, which lavapipe's Win32 WSI supports; the pinned lavapipe build must therefore include WSI (a manifest note of R7). Every other GPU test creates a device with no window and no swapchain. The reconciliation of the plan's "boots headless on WARP and lavapipe" with [Z2-N-ARCH-785] is cross-document item X77 ("Reconciliation items", R15).

### The sync-validation-clean rule

No GPU test passes with a validation message: Vulkan runs with synchronization validation, D3D12 with GPU-based validation and the debug layer, and any unsuppressed warning or error fails the test (Validation layers and the clean requirement, above). A technique whose passes produce a hazard cannot land ([Z2-N-REND-382]).

### Backend parity RHI half

The `BackendParity` gate step compares Vulkan and D3D12 images (RPROD's and LIGHT's scenes, [Z2-N-REND-940]); its RHI half is the conformance suite: the same neutral command stream run on both backends gives bit-identical readbacks (copies, clears, compute, draw IDs, cross-queue transfers, aliasing, clip-space orientation), so an image difference is never the RHI's.

### Device steps

The needs-human device steps of Tests.md "Device steps": the S24 profile confirmation, the D-SLANG full probe and half-precision check on the S24 and the RTX GPU, Android surface loss on the S24, device-loss recovery on hardware, the RHI conformance suite on the RTX 3070.

## Budget rows

### Rows RHI owns

| Row | Status | How this area meets or measures it |
|---|---|---|
| Z2-B-RHI-001 Persistent render targets and view histories | pending | the `RenderTargets` category, `DEVICE_DEDICATED` allocations, reported per frame |
| Z2-B-RHI-002 Bindless descriptor heaps | pending | `Zenith_RHIDescriptorHeap::GetHeapBytes` |
| Z2-B-RHI-003 GPU residency budget per heap | pending | the per-heap cap of Memory.md "Residency reporting" (not summed) |
| Z2-B-RHI-004 Render graph build and compile CPU | pending | `Flux.Graph.CompileMs` ([Z2-N-REND-308]); the back-end steps are part of it |
| Z2-B-RHI-005 Command recording and submit CPU | pending | the `Flux.Record` and `Flux.Submit` zones |
| Z2-B-RHI-006 Upload ring hard cap | pending | the growth cap, BR-REFUSE |
| Z2-B-RHI-007 Upload ring stall frames | pending | frames with a stall or deferral |
| Z2-B-RHI-008 Frames in flight | adopted (2 to 3) | `r.framesInFlight`, recorded in every evidence record |
| Z2-B-RHI-009 Bindless heap capacity per kind | pending | the tier capacities, refusal at device creation, BR-REFUSE at allocation |
| Z2-B-RHI-010 Transient memory peak | pending | the transient category's peak over 300 frames |
| Z2-B-RHI-011 GPU readback pool size | pending | the pool size, BR-DEFER |

Every pending row's producer is the Stage 1 per-system budget ticket for RHI (`Zenith2/Docs/Budgets.md → "Row index"`, table "Producer tickets"), blocked by its harness below and by the CORE, JOBS and RPROD rulings.

### Rows RHI is measured against

`Z2-B-CORE-001` S24 process RSS and `Z2-B-CORE-003` Windows VRAM (the totals the rendering rows sum within); `Z2-B-CORE-008` Texture-swap peak (the streaming swap's reservation); `Z2-B-RPROD-001` GPU scene tables (the tables' maximum capacities); `Z2-B-RPROD-002` PSO cache and pipeline objects; `Z2-B-RPROD-030` On-disk pipeline-cache size (the cache store's cap); `Z2-B-GEOM-001` Unified vertex and index pools and `Z2-B-VFX-001` Particle pools and CPU particle data (pool growth bounds); `Z2-B-ADVGFX-001` (the VT pool, Stage 4); `Z2-B-SHIP-011` Dynamic-resolution floor (pressure action 11); `Z2-B-RPROD-021` CPU render lag (independent of frames in flight); `Z2-B-PROC-027` Step timeout BackendParity; `Z2-B-PROC-030` Integration smoke frame count (the WARP and lavapipe smoke boots the RHI).

### Pending row needs

One: the GPU scene's CPU pending store (Memory.md "Scatter staging and coalescing", bounded by the tables' maximum capacities) needs a RAM row, recorded as the pending Budgets row "GPU-scene CPU pending store" in MEM-WIN-RAM and MEM-S24, producer the Stage 1 per-system budget ticket for RHI (cross-document item X91, pending; until it lands the bytes count under `Z2-B-CORE-004`). Every capacity this design fixes as a constant (the retirement ring's 65,536 entries, the drain headroom of 512, the resource-table capacities, the 4,096 request queue, the 64 partitions per queue, the breadcrumb index range) is derived in its contract from bounded inputs ([Z2-N-CONTRACT-342]) and is not a budget.

### Measurement harness

`Zenith_Bench_Budgets_RHI` (`Zenith2/Benchmarks/Budgets/RHI/`, the Stage 1 per-system budget harness for RHI) measures on the RTX 3070 and the S24, 10 repetitions, medians with 95% confidence intervals, raw data under `Zenith2/Docs/Research/Budgets/RHI/`: descriptor-heap memory at the proposed tier capacities (RHI-002, RHI-009); copy-queue and upload bandwidth, and the upload bytes per frame of a synthetic frame shaped like each workload's pass count and GPU-scene churn (RHI-006, RHI-007); readback sizes of editor picking, timestamps and parity captures (RHI-011); the heap budgets the drivers report (RHI-003); and an analytic calculator for RHI-001 and RHI-010 from the per-pass format tables at each workload's resolutions. RHI-004 and RHI-005 are proposed from the harness's recording of a synthetic 400-pass graph and confirmed in Stage 2 and 3 workloads. `Test_BudgetHarness_RHI_Smoke` runs one repetition.

## Stage plan

### Stage 1

| Ticket | Kind | Blocks |
|---|---|---|
| the D-SLANG bindless probe harness (`Zenith_Bench_BindlessProbe`, smoke run) | harness | the D-SLANG ruling |
| D-SLANG ruling: the full probe on the RTX GPU, WARP, lavapipe and the S24 (a needs-human device step), the Slang pin, the alternative, the DecisionLog entry | decision (needs-human) | the descriptor heap realisation, every generated header's resource fields |
| the S24 profile: `VP_ZENITH2_baseline` confirmed on the Adreno 750, lavapipe and the RTX 3070; the half-precision check green on WARP and lavapipe (a needs-human device step for the S24) | decision (needs-human) | half-precision shaders in CI; the profile file |
| the RHI budget harness | harness | the RHI budget ruling |
| the Stage 1 per-system budget ticket for RHI | budget (needs-human) | every ticket citing a pending RHI row |
| the RHI Stage 2 revalidation (RHI, Stage 2, {D-SLANG}) | revalidation (needs-human) | Stage 2 RHI tickets that depend on the ruling |

### Stage 1 pull-forward and API-only slices

Independent of D-SLANG and pulled into the Stage 1 sprint with fix version `Z2 Stage 2`: the eighteen API-only `ZenithRHI` headers (`Zenith_RHITypes.h`, `Zenith_RHIError.h`; `Zenith_DeviceHandle.h`, `Zenith_RHICapabilities.h`, `Zenith_RHIShaderInterface.h`; `Zenith_RHIResources.h`, `Zenith_RHIDescriptors.h`, `Zenith_RHILifetime.h`, `Zenith_RHIQueue.h`; `Zenith_RHIPipelines.h`, `Zenith_RHIQueries.h`, `Zenith_RHIDebug.h`, `Zenith_RHIFaultInjection.h`; `Zenith_RHICommandList.h`, `Zenith_RHIBarrier.h`; `Zenith_RHISwapchain.h`, `Zenith_RHIConfig.h`, `Zenith_RHIDevice.h`, each with its unit tests) and the generated nineteenth, `Zenith_FormatFeatures.h`, with its table and generator; the two backend targets with their factory headers `Zenith_RHI_Vulkan.h` and `Zenith_RHI_D3D12.h` (and the Agility constants); the device-free implementations — the handle codec and resource table, `Zenith_RHIDeviceBase` with its test hooks, the retirement queue, the release requests and the descriptor slot lists — and the four revalidation tickets of Stages 2 to 4. The Vulkan baseline profile with its generated tables and `Zenith_VulkanProfileCheck`, which the S24 profile step runs, is Stage 1 work in the Stage 1 sprint. The Vulkan and D3D12 device bring-up (steps 1 to 9) stays in the Stage 2 sprint, because it needs PLAT's platform and window implementations, which are Stage 2 work. The Vulkan descriptor-heap realisation (step 10 onward) waits for D-SLANG (open, Stage 1) ([Z2-N-REND-970]); the D3D12 realisation does not, because Shader Model 6.6 heap indexing fixes it.

### Stage 2

Both backends complete through every header; the recording surface and barriers; swapchains and presentation with recreation (Z2-I-RHI-20); frames in flight, the upload ring and readbacks; frame failures with the frame-failure exit, which lands `Flux_DeviceLossHandler` with its `NORMAL` and `EXITING` states and `RunPrologue` (the device-loss states follow in Stage 3); the graph back end (steps 3, 6, 7, recording, submission, the imported-state table, ownership transfers, the frame dump); validation and the clean rule on WARP and lavapipe; allocation and placement rules; the conformance suite; timestamps and calibration for the profiler.

### Stage 3

The GPU scene tables and pools, the material-slot range listener (pulled into the Stage 2 sprint with the tables, because RPROD's Stage 2 renderer installs it), persistent resources and the streaming swap (Z2-I-RHI-08); the pipeline-cache store (Z2-I-RHI-22); captures (Z2-I-RHI-17); device-loss detection, breadcrumbs and the recover-or-exit policy with simulated-loss tests, which the Stage 3 acceptance's "resize and device-loss recovery" needs; the BackendParity step's activation (a PROC ticket) with the conformance suite as its RHI half; the S24 Forward+ bring-up and surface-loss device step.

### Stage 4

Residency reporting and the per-heap cap (Z2-I-RHI-10); the memory-pressure responder and device-memory exhaustion (Z2-I-RHI-12); the VT pool.

### Stage 5

Mesh pipelines and indirect mesh dispatch for the visibility-buffer path, with the compute fallback tested on WARP and lavapipe.

| Stage 5 extension | Rule |
|---|---|
| a fifth GPU-scene table | LIGHT's instance-lighting record (L14; cross-document item X106, pending its ruling) is appended to `Flux_GPUSceneTable` as `FLUX_GPU_SCENE_TABLE_INSTANCE_LIGHTING = 4`, takes its maximum-capacity share out of the instance share (0.60 split 128 : 16 between the instance and instance-lighting records, so both tables reach the same maximum slot count, the lighting record being indexed by the instance index), and extends the pending-store bound and `Test_GPUScene_InitialCapacities` in the ticket that lands it (Memory.md "Flux_GPUScene") |

### Stage 6

Recovery diagnostics complete (Z2-I-RHI-19): the device-loss report with DRED and `VK_EXT_device_fault` data, the last three frame dumps, the DEVT crash-report attachment, telemetry; device-loss recovery and 50-cycle surface-loss runs on the reference hardware as acceptance evidence.

### Stage 7

No RHI inventory item is scheduled; the RT, stereo and console backends are the ADVGFX and SCOPE deferred trees, which extend these interfaces.

## Decisions

### Settled decisions governing the area

| Decision | Effect here |
|---|---|
| Z2-D-010 Platforms | both backends on Windows chosen at runtime, Vulkan alone on Android, the S24 as the validated device |
| Z2-D-013 GPU baseline | the Vulkan 1.3 profile with `shaderFloat16`, SM 6.6, bindless, dynamic rendering |
| Z2-D-025 Offline shader compilation, PSO precaching | blobs only; pipeline caches persisted per driver version |
| Z2-D-026 volk | device-level dispatch through the device table |
| Z2-D-054 Fully bindless binding model | one heap per space, one 8-byte push constant, the draw ID |
| Z2-D-056 Automatic barrier synthesis | the recording surface is `Internal/`-only, the barrier header is Graph-only |
| Z2-D-057 Transient aliasing | placed resources and heaps, first-use discards |
| Z2-D-058 GPU scene tables | the tables and pools implemented here |
| Z2-D-061 Depth mode | the D24S8 capability flag and the D32F_S8 fallback |
| Z2-D-001, Z2-D-040, Z2-D-045, Z2-D-046, Z2-D-082, Z2-D-084, Z2-D-097 | backend libraries, ownership, errors, no mutable globals, third-party confinement, harness plus ruling split, rulings in the ticket's diff |

### Open decisions

D-SLANG (open, Stage 1): the Slang version and the Vulkan `DescriptorHandle<T>` lowering; this area owns the probe harness and shares the ruling with RPROD. D-SEL-HASH (open, Stage 1): the hash of pipeline-cache payloads and PSO key names. D-TBDR (open, Stage 5): tile-local reads, rejected until ruled.

### Rulings this design records

<!-- elaboration -->
These are design choices, recorded as rulings by the tickets that land them (appended to `Zenith2/Docs/DecisionLog.md` in their own diffs, Z2-D-097): the factory pointer type keeps the name `Zenith_PfnCreateDevice`; device-loss clean exit uses the new `EXIT_CODE_DEVICE_LOST` (6), with `FATAL_CODE_DEVICE_LOST` as the 10 s fallback alone (X72); the frames in flight change at runtime through `Zenith_RHIDevice::SetFramesInFlight`; client retirements are batched per kind and frame; the barrier mapping is split into a neutral Flux half and one backend function each; the upload ring's frame reservation is predicted on the render-submit thread and extended at compile without waiting, a frame that cannot be served being deferred after the GPU scene staged its deltas into a persistent pending store, so a deferral loses no table state (its temporal cost, a one-frame velocity error for an instance updated in the deferred frame and the next, is stated in Memory.md "Frame deferral"), and a frame whose need exceeds the cap failing instead; every build-node ring write follows the frame's `ExtendReservation`; the release-request drain stops a derived headroom short of the ring's capacity; the imported-state table is single-writer and a retired resource leaves it through a retirement listener; a descriptor slot released from any thread rides with its resource's release request.

### Selection decisions RHI owns

None beyond its share of D-SLANG; its candidates are the three alternatives of `Zenith2/Docs/Rendering.md → "Decision D-SLANG"`, its criteria the probe cases, and its acceptance rule [Z2-N-REND-972].

## Requirement index

| Requirement | Title | Design headings |
|---|---|---|
| Z2-R-RHI-001 | One timeline per physical queue | `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_RHIQueue header"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Submission and timelines"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Queues and fences" |
| Z2-R-RHI-002 | Last use recorded per batch | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Submission"; `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Imported-state table" |
| Z2-R-RHI-003 | Retirement after every queue completed | `Zenith2/Docs/Design/RHI.md` → "Retirement rule"; `Zenith2/Docs/Design/RHI/Memory.md` → "Retirement queue" |
| Z2-R-RHI-004 | Every satisfied entry retired at frame begin | `Zenith2/Docs/Design/RHI/Memory.md` → "Retirement queue"; `Zenith2/Docs/Design/RHI.md` → "Frame-in-flight rotation" |
| Z2-R-RHI-005 | Retirement ring capacity and full policy | `Zenith2/Docs/Design/RHI/Memory.md` → "Retirement queue"; `Zenith2/Docs/Design/RHI.md` → "Bounded queues and full policies" |
| Z2-R-RHI-006 | Release requests from any thread | `Zenith2/Docs/Design/RHI/Memory.md` → "Retirement queue"; `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_RHILifetime header" |
| Z2-R-RHI-007 | GPU waits only on the render-submit thread | `Zenith2/Docs/Design/RHI.md` → "Frame-in-flight rotation"; `Zenith2/Docs/Design/RHI.md` → "Thread affinity per type"; `Zenith2/Docs/Design/RHI/Recovery.md` → "Device loss detection"; `Zenith2/Docs/Design/RHI.md` → "Permitted main-thread GPU waits" |
| Z2-R-RHI-008 | Binary semaphores only for presentation | `Zenith2/Docs/Design/RHI/Vulkan.md` → "Submission and timelines"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Swapchain and presentation"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Queues and fences" |
| Z2-R-RHI-009 | Resources destroyed only through retirement | `Zenith2/Docs/Design/RHI.md` → "Rules across the API"; `Zenith2/Docs/Design/RHI/Memory.md` → "Device resource table"; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHIDevice header" |
| Z2-R-RHI-010 | Device handle tests | `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_DeviceHandle header"; `Zenith2/Docs/Design/RHI/Memory.md` → "Device resource table"; `Zenith2/Docs/Design/RHI/Tests.md` → "RHI unit tests" |
| Z2-R-RHI-011 | Descriptor slot reuse after retirement | `Zenith2/Docs/Design/RHI/Memory.md` → "Descriptor heap and slot allocation"; `Zenith2/Docs/Design/RHI/Memory.md` → "Retirement queue"; `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_RHILifetime header" |
| Z2-R-RHI-012 | Full descriptor space refused | `Zenith2/Docs/Design/RHI/Memory.md` → "Descriptor heap and slot allocation"; `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_RHIDescriptors header" |
| Z2-R-RHI-013 | Null substitute in slot zero | `Zenith2/Docs/Design/RHI/Memory.md` → "Descriptor heap and slot allocation"; `Zenith2/Docs/Design/RHI.md` → "Device creation sequence" |
| Z2-R-RHI-014 | Descriptor writes once per allocation | `Zenith2/Docs/Design/RHI/Memory.md` → "Descriptor heap and slot allocation"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Descriptor heap realisation"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Descriptor heaps"; `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Recording" |
| Z2-R-RHI-015 | Heap capacities and device refusal | `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_RHICapabilities header"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Baseline profile check"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Baseline checks" |
| Z2-R-RHI-016 | One global layout bound per partition | `Zenith2/Docs/Design/RHI/Vulkan.md` → "Descriptor heap realisation"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Root signature and command signatures"; `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Recording"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Descriptor heaps" |
| Z2-R-RHI-017 | Heap realisation after D-SLANG | `Zenith2/Docs/Design/RHI/Vulkan.md` → "Descriptor heap realisation"; `Zenith2/Docs/Design/RHI.md` → "Stage 1 pull-forward and API-only slices" |
| Z2-R-RHI-018 | Frames in flight configurable | `Zenith2/Docs/Design/RHI.md` → "Frame-in-flight rotation"; `Zenith2/Docs/Design/RHI.md` → "Changing the frames in flight"; `Zenith2/Docs/Design/RHI/Recovery.md` → "Recreation state machine"; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHIDevice header" |
| Z2-R-RHI-019 | Frame build waits for its slot | `Zenith2/Docs/Design/RHI.md` → "Frame-in-flight rotation" |
| Z2-R-RHI-020 | Per-slot resources reused after completion | `Zenith2/Docs/Design/RHI.md` → "Frame-in-flight rotation"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Command recording mapping"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Command recording mapping" |
| Z2-R-RHI-021 | Upload ring chunks | `Zenith2/Docs/Design/RHI/Memory.md` → "Upload ring internals"; `Zenith2/Docs/Design/RHI/Memory.md` → "Memory classes and allocation" |
| Z2-R-RHI-022 | Upload stalls on the render-submit thread only | `Zenith2/Docs/Design/RHI/Memory.md` → "Upload ring internals" |
| Z2-R-RHI-023 | Upload ring growth capped | `Zenith2/Docs/Design/RHI/Memory.md` → "Upload ring internals" |
| Z2-R-RHI-024 | Compile-time reservation extension and frame deferral | `Zenith2/Docs/Design/RHI/Memory.md` → "Upload ring internals"; `Zenith2/Docs/Design/RHI/Memory.md` → "Frame deferral"; `Zenith2/Docs/Design/RHI.md` → "Relationship to the specification"; `Zenith2/Docs/Design/RHI/Memory.md` → "Deferred frames and the pending store" |
| Z2-R-RHI-025 | Upload request queue | `Zenith2/Docs/Design/RHI/Memory.md` → "Upload request state machine"; `Zenith2/Docs/Design/RHI/Memory.md` → "Upload ring internals" |
| Z2-R-RHI-026 | Split oversized splittable requests | `Zenith2/Docs/Design/RHI/Memory.md` → "Splitting algorithm" |
| Z2-R-RHI-027 | Refuse oversized unsplittable requests | `Zenith2/Docs/Design/RHI/Memory.md` → "Upload request state machine"; `Zenith2/Docs/Design/RHI/Memory.md` → "Frame failures" |
| Z2-R-RHI-028 | Upload exhaustion telemetry | `Zenith2/Docs/Design/RHI.md` → "Counters"; `Zenith2/Docs/Design/RHI/Memory.md` → "Upload ring internals" |
| Z2-R-RHI-029 | Upload ring shrink | `Zenith2/Docs/Design/RHI/Memory.md` → "Upload ring internals" |
| Z2-R-RHI-030 | Draw-parameter bound enforced | `Zenith2/Docs/Design/RHI/Memory.md` → "Upload ring internals" |
| Z2-R-RHI-031 | Upload cancellation | `Zenith2/Docs/Design/RHI/Memory.md` → "Upload request state machine" |
| Z2-R-RHI-032 | Readbacks are operation handles | `Zenith2/Docs/Design/RHI/Memory.md` → "Readback state machine"; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "RHI-owned ZenithFlux public headers" |
| Z2-R-RHI-033 | Readback data lifetime | `Zenith2/Docs/Design/RHI/Memory.md` → "Readback pool" |
| Z2-R-RHI-034 | Readback pool bounded | `Zenith2/Docs/Design/RHI/Memory.md` → "Readback pool" |
| Z2-R-RHI-035 | Readbacks published at tick boundaries | `Zenith2/Docs/Design/RHI/Memory.md` → "Readback state machine" |
| Z2-R-RHI-036 | Readback cancellation and loss | `Zenith2/Docs/Design/RHI/Memory.md` → "Readback state machine" |
| Z2-R-RHI-037 | Imported-state table | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Imported-state table" |
| Z2-R-RHI-038 | Initial states registered by creators | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Imported-state table"; `Zenith2/Docs/Design/RHI/Memory.md` → "Flux_DeferredDeletion"; `Zenith2/Docs/Design/RHI/Memory.md` → "Persistent resource creation and the acquire list" |
| Z2-R-RHI-039 | Persistent histories and subresource runs | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Tracked resource kinds realised" |
| Z2-R-RHI-040 | Queue-family ownership transfers | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Queue-family ownership transfers" |
| Z2-R-RHI-041 | Queue order never orders memory | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Step 6 queue assignment"; `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Queue-family ownership transfers" |
| Z2-R-RHI-042 | Neutral barrier vocabulary and one call per batch | `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHIBarrier header"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Barrier translation"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Enhanced barrier translation" |
| Z2-R-RHI-043 | Barrier mapping tables | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Neutral barrier translation"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Barrier translation"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Enhanced barrier translation" |
| Z2-R-RHI-044 | Barriers reachable from the graph alone | `Zenith2/Docs/Design/RHI.md` → "Rules across the API"; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHICommandList header" |
| Z2-R-RHI-045 | Split barriers | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Step 7 command-list partitions"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Barrier translation"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Enhanced barrier translation"; `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Step 6 queue assignment" |
| Z2-R-RHI-046 | Aliasing discards realised | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Tracked resource kinds realised"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Barrier translation"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Enhanced barrier translation" |
| Z2-R-RHI-047 | GPU scene table layouts | `Zenith2/Docs/Design/RHI/Memory.md` → "Flux_GPUScene" |
| Z2-R-RHI-048 | Tables imported write-restricted | `Zenith2/Docs/Design/RHI/Memory.md` → "Flux_GPUScene"; `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Imported-state table" |
| Z2-R-RHI-049 | Scatter upload with coalescing | `Zenith2/Docs/Design/RHI/Memory.md` → "Scatter staging and coalescing" |
| Z2-R-RHI-050 | No scatter pass without changes | `Zenith2/Docs/Design/RHI/Memory.md` → "Flux_GPUScene" |
| Z2-R-RHI-051 | Table growth and full tables | `Zenith2/Docs/Design/RHI/Memory.md` → "Growth" |
| Z2-R-RHI-052 | Initial table capacities | `Zenith2/Docs/Design/RHI/Memory.md` → "Flux_GPUScene" |
| Z2-R-RHI-053 | Table slot recycling after retirement | `Zenith2/Docs/Design/RHI/Memory.md` → "Slot allocators" |
| Z2-R-RHI-054 | Persistent read-only resources | `Zenith2/Docs/Design/RHI/Memory.md` → "Persistent resource creation and the acquire list" |
| Z2-R-RHI-055 | Persistent uploads through the acquire list | `Zenith2/Docs/Design/RHI/Memory.md` → "Persistent resource creation and the acquire list" |
| Z2-R-RHI-056 | Pools with producer and consumer passes | `Zenith2/Docs/Design/RHI/Memory.md` → "Pool allocation" |
| Z2-R-RHI-057 | Pool growth bounded by memory rows | `Zenith2/Docs/Design/RHI/Memory.md` → "Pool allocation" |
| Z2-R-RHI-058 | VT tile upload then sample | `Zenith2/Docs/Design/RHI/Memory.md` → "Pool allocation" |
| Z2-R-RHI-059 | Streaming swap | `Zenith2/Docs/Design/RHI/Memory.md` → "Streaming swap integration" |
| Z2-R-RHI-060 | Delta replay reproduces tables | `Zenith2/Docs/Design/RHI/Memory.md` → "Flux_GPUScene"; `Zenith2/Docs/Design/RHI/Memory.md` → "Deferred frames and the pending store" |
| Z2-R-RHI-061 | One device allocator per API | `Zenith2/Docs/Design/RHI/Vulkan.md` → "Memory through VMA"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Memory through D3D12MA" |
| Z2-R-RHI-062 | Memory classes | `Zenith2/Docs/Design/RHI/Memory.md` → "Memory classes and allocation"; `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_RHITypes header" |
| Z2-R-RHI-063 | Device resource tables | `Zenith2/Docs/Design/RHI/Memory.md` → "Device resource table" |
| Z2-R-RHI-064 | Requirements before creation and placed resources | `Zenith2/Docs/Design/RHI/Memory.md` → "Heap alignment and placement"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Memory through VMA"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Memory through D3D12MA" |
| Z2-R-RHI-065 | Budget query and category report | `Zenith2/Docs/Design/RHI/Memory.md` → "Residency reporting" |
| Z2-R-RHI-066 | Per-heap residency cap | `Zenith2/Docs/Design/RHI/Memory.md` → "Residency reporting" |
| Z2-R-RHI-067 | Device-local usage to the governor | `Zenith2/Docs/Design/RHI/Memory.md` → "Residency reporting" |
| Z2-R-RHI-068 | Placement rules per API | `Zenith2/Docs/Design/RHI/Memory.md` → "Heap alignment and placement" |
| Z2-R-RHI-069 | Rendering memory consumer | `Zenith2/Docs/Design/RHI/Memory.md` → "Memory pressure responder" |
| Z2-R-RHI-070 | Warning-level shedding | `Zenith2/Docs/Design/RHI/Memory.md` → "Memory pressure responder" |
| Z2-R-RHI-071 | Critical-level shedding | `Zenith2/Docs/Design/RHI/Memory.md` → "Memory pressure responder" |
| Z2-R-RHI-072 | Device memory exhaustion | `Zenith2/Docs/Design/RHI/Memory.md` → "Device memory exhaustion"; `Zenith2/Docs/Design/RHI/Memory.md` → "Frame failures"; `Zenith2/Docs/Design/RHI/Recovery.md` → "Device-loss state machine" |
| Z2-R-RHI-073 | Swapchain outputs | `Zenith2/Docs/Design/RHI/Recovery.md` → "Present modes and formats"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Swapchain and presentation"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Swapchain and presentation"; `Zenith2/Docs/Design/RHI.md` → "CI windows for GPU tests" |
| Z2-R-RHI-074 | Present modes | `Zenith2/Docs/Design/RHI/Recovery.md` → "Present modes and formats"; `Zenith2/Docs/Design/RHI.md` → "CI windows for GPU tests" |
| Z2-R-RHI-075 | Acquire and present | `Zenith2/Docs/Design/RHI/Recovery.md` → "Acquire and present"; `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Submission"; `Zenith2/Docs/Design/RHI.md` → "CI windows for GPU tests" |
| Z2-R-RHI-076 | Frame-pacing hooks | `Zenith2/Docs/Design/RHI/Recovery.md` → "Frame pacing hooks"; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHISwapchain header"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Swapchain and presentation"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Swapchain and presentation" |
| Z2-R-RHI-077 | Zero-sized output | `Zenith2/Docs/Design/RHI/Recovery.md` → "Recreation state machine"; `Zenith2/Docs/Design/RHI.md` → "CI windows for GPU tests" |
| Z2-R-RHI-078 | One swapchain per rendering window | `Zenith2/Docs/Design/RHI/Recovery.md` → "Swapchain model" |
| Z2-R-RHI-079 | Clip-space convention | `Zenith2/Docs/Design/RHI/Vulkan.md` → "Command recording mapping"; `Zenith2/Docs/Design/RHI/Recovery.md` → "Swapchain model" |
| Z2-R-RHI-080 | Android pre-rotation | `Zenith2/Docs/Design/RHI/Recovery.md` → "Swapchain model"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Android specifics" |
| Z2-R-RHI-081 | Vulkan baseline profile | `Zenith2/Docs/Design/RHI/Vulkan.md` → "Baseline profile check" |
| Z2-R-RHI-082 | Exact refusal message | `Zenith2/Docs/Design/RHI/Vulkan.md` → "Refusal message"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Baseline checks"; `Zenith2/Docs/Design/RHI.md` → "Runtime file formats" |
| Z2-R-RHI-083 | D3D12 baseline | `Zenith2/Docs/Design/RHI/D3D12.md` → "Baseline checks" |
| Z2-R-RHI-084 | Format-feature table | `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_FormatFeatures header" |
| Z2-R-RHI-085 | Format features checked on both backends | `Zenith2/Docs/Design/RHI/Vulkan.md` → "Baseline profile check"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Baseline checks" |
| Z2-R-RHI-086 | Capabilities filled once | `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_RHICapabilities header"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Physical device selection"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Baseline checks" |
| Z2-R-RHI-087 | Capability tier and D24S8 fallback | `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_RHICapabilities header" |
| Z2-R-RHI-088 | Runtime backend choice | `Zenith2/Docs/Design/RHI.md` → "Device factories and the factory pointer type"; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHIConfig header" |
| Z2-R-RHI-089 | Adapter selection | `Zenith2/Docs/Design/RHI/Vulkan.md` → "Physical device selection"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Adapter selection" |
| Z2-R-RHI-090 | volk dispatch | `Zenith2/Docs/Design/RHI/Vulkan.md` → "Logical device and volk"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Instance creation" |
| Z2-R-RHI-091 | Agility SDK exports and version check | `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Backend factory headers"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Agility SDK loading" |
| Z2-R-RHI-092 | Third-party headers confined | `Zenith2/Docs/Design/RHI.md` → "Rules across the API"; `Zenith2/Docs/Design/RHI.md` → "Libraries" |
| Z2-R-RHI-093 | Device creation unwinds every step | `Zenith2/Docs/Design/RHI.md` → "Device creation sequence"; `Zenith2/Docs/Design/RHI/Tests.md` → "Failure-injection tests"; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHIDevice header" |
| Z2-R-RHI-094 | S24 profile confirmation | `Zenith2/Docs/Design/RHI/Tests.md` → "Device steps"; `Zenith2/Docs/Design/RHI.md` → "Stage 1" |
| Z2-R-RHI-095 | Half-precision check | `Zenith2/Docs/Design/RHI/Tests.md` → "Half-precision check" |
| Z2-R-RHI-096 | Headless CI devices | `Zenith2/Docs/Design/RHI.md` → "Headless CI devices"; `Zenith2/Docs/Design/RHI.md` → "CI windows for GPU tests" |
| Z2-R-RHI-097 | Capability fallbacks tested | `Zenith2/Docs/Design/RHI.md` → "Headless CI devices"; `Zenith2/Docs/Design/RHI.md` → "Stage 5" |
| Z2-R-RHI-098 | Mesh pipelines | `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_RHIPipelines header"; `Zenith2/Docs/Design/RHI.md` → "Stage 5" |
| Z2-R-RHI-099 | Bindless probe harness | `Zenith2/Docs/Design/RHI/Tests.md` → "Bindless probe" |
| Z2-R-RHI-100 | D-SLANG ruling | `Zenith2/Docs/Design/RHI.md` → "Stage 1"; `Zenith2/Docs/Design/RHI.md` → "Open decisions" |
| Z2-R-RHI-101 | Validation modes | `Zenith2/Docs/Design/RHI.md` → "Validation layers and the clean requirement"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Instance creation"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Debug layer and validation" |
| Z2-R-RHI-102 | Validation clean in every GPU test | `Zenith2/Docs/Design/RHI.md` → "The sync-validation-clean rule"; `Zenith2/Docs/Design/RHI.md` → "Validation layers and the clean requirement" |
| Z2-R-RHI-103 | Suppressions need decisions | `Zenith2/Docs/Design/RHI.md` → "Committed data files" |
| Z2-R-RHI-104 | Validation sink | `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHIDebug header" |
| Z2-R-RHI-105 | Missing layer handling | `Zenith2/Docs/Design/RHI.md` → "Validation layers and the clean requirement" |
| Z2-R-RHI-106 | Debug names and labels | `Zenith2/Docs/Design/RHI/Vulkan.md` → "Debug utils and validation"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Command recording mapping"; `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Recording" |
| Z2-R-RHI-107 | Frame dump back-end sections | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Frame dump back-end sections" |
| Z2-R-RHI-108 | Last frames kept for device loss | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Frame dump back-end sections"; `Zenith2/Docs/Design/RHI/Recovery.md` → "Device-loss report" |
| Z2-R-RHI-109 | Graph conformance test access | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Flux_RenderGraphTestAccess" |
| Z2-R-RHI-110 | Timestamp pools and calibration | `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHIQueries header"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Timestamps and calibration"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Timestamps and calibration"; `Zenith2/Docs/Design/RHI.md` → "Frame-in-flight rotation" |
| Z2-R-RHI-111 | RenderDoc capture | `Zenith2/Docs/Design/RHI/Vulkan.md` → "RenderDoc capture on Vulkan"; `Zenith2/Docs/Design/RHI/Recovery.md` → "Captures" |
| Z2-R-RHI-112 | PIX capture | `Zenith2/Docs/Design/RHI/D3D12.md` → "PIX and RenderDoc capture" |
| Z2-R-RHI-113 | Capture trigger and shipping exclusion | `Zenith2/Docs/Design/RHI/Recovery.md` → "Captures" |
| Z2-R-RHI-114 | Provisional queue and gap rule | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Step 3 queues lifetimes and reachability" |
| Z2-R-RHI-115 | Lifetimes and reachability | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Step 3 queues lifetimes and reachability" |
| Z2-R-RHI-116 | Submission batches and waits | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Step 6 queue assignment"; `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Submission" |
| Z2-R-RHI-117 | Command-list partitions | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Step 7 command-list partitions" |
| Z2-R-RHI-118 | Parallel recording equals serial | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Parallel recording jobs" |
| Z2-R-RHI-119 | Per-pass recording order | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Per-pass recording order" |
| Z2-R-RHI-120 | Swapchain writes on graphics only | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Queue-family ownership transfers" |
| Z2-R-RHI-121 | GPU breadcrumbs | `Zenith2/Docs/Design/RHI/Recovery.md` → "Breadcrumbs" |
| Z2-R-RHI-122 | Device-loss report | `Zenith2/Docs/Design/RHI/Recovery.md` → "Device-loss report" |
| Z2-R-RHI-123 | Device fault information | `Zenith2/Docs/Design/RHI/Vulkan.md` → "Device loss and fault reports"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Device removal and DRED" |
| Z2-R-RHI-124 | Recovery telemetry | `Zenith2/Docs/Design/RHI.md` → "Telemetry"; `Zenith2/Docs/Design/RHI.md` → "Counters" |
| Z2-R-RHI-125 | Swapchain recreation steps | `Zenith2/Docs/Design/RHI/Recovery.md` → "Recreation state machine"; `Zenith2/Docs/Design/RHI.md` → "Changing the frames in flight" |
| Z2-R-RHI-126 | Histories invalidated by extent | `Zenith2/Docs/Design/RHI/Recovery.md` → "Recreation state machine" |
| Z2-R-RHI-127 | Recreation failure retry | `Zenith2/Docs/Design/RHI/Recovery.md` → "Recreation state machine" |
| Z2-R-RHI-128 | Android surface loss and restore | `Zenith2/Docs/Design/RHI/Recovery.md` → "Android surface loss"; `Zenith2/Docs/Design/RHI.md` → "Permitted main-thread GPU waits" |
| Z2-R-RHI-129 | Device-loss detection | `Zenith2/Docs/Design/RHI/Recovery.md` → "Device loss detection"; `Zenith2/Docs/Design/RHI.md` → "Device status state machine" |
| Z2-R-RHI-130 | Recover on first loss | `Zenith2/Docs/Design/RHI/Recovery.md` → "Recover-or-clean-exit policy" |
| Z2-R-RHI-131 | Clean exit on repeated loss | `Zenith2/Docs/Design/RHI/Recovery.md` → "Device-loss state machine" |
| Z2-R-RHI-132 | Device loss on reference hardware | `Zenith2/Docs/Design/RHI/Tests.md` → "Device steps" |
| Z2-R-RHI-133 | Pipeline cache key and file | `Zenith2/Docs/Design/RHI/Recovery.md` → "Cache key and file naming" |
| Z2-R-RHI-134 | Invalidation never fails the device | `Zenith2/Docs/Design/RHI/Recovery.md` → "Load and invalidation" |
| Z2-R-RHI-135 | Journaled cache writes | `Zenith2/Docs/Design/RHI/Recovery.md` → "Write cadence" |
| Z2-R-RHI-136 | Stale caches and the size cap | `Zenith2/Docs/Design/RHI/Recovery.md` → "Load and invalidation"; `Zenith2/Docs/Design/RHI/Recovery.md` → "Write cadence" |
| Z2-R-RHI-137 | Pipeline cache used by every creation | `Zenith2/Docs/Design/RHI/Vulkan.md` → "Pipeline layout and pipelines"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Pipelines and the PSO library"; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHIDevice header"; `Zenith2/Docs/Design/RHI.md` → "Counters" |
| Z2-R-RHI-138 | RHI conformance suite | `Zenith2/Docs/Design/RHI/Tests.md` → "RHI conformance suite"; `Zenith2/Docs/Design/RHI.md` → "Backend parity RHI half" |
| Z2-R-RHI-139 | Backend parity gate | `Zenith2/Docs/Design/RHI.md` → "Backend parity RHI half"; `Zenith2/Docs/Design/RHI.md` → "Stage 3" |
| Z2-R-RHI-140 | RHI budget harness | `Zenith2/Docs/Design/RHI.md` → "Measurement harness" |
| Z2-R-RHI-141 | RHI budget rows allocated | `Zenith2/Docs/Design/RHI.md` → "Rows RHI owns" |
| Z2-R-RHI-142 | Recording and submit CPU budget | `Zenith2/Docs/Design/RHI.md` → "Rows RHI owns" |
| Z2-R-RHI-143 | Configuration keys | `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHIConfig header" |
| Z2-R-RHI-144 | Shutdown order | `Zenith2/Docs/Design/RHI.md` → "Startup and shutdown" |
| Z2-R-RHI-145 | Lock rows declared | `Zenith2/Docs/Design/RHI.md` → "Lock-order rows added by RHI" |
| Z2-R-RHI-146 | Headless runs create no device | `Zenith2/Docs/Design/RHI.md` → "Startup and shutdown" |
| Z2-R-RHI-147 | No public barrier API | `Zenith2/Docs/Design/RHI.md` → "Explicit exclusions" |
| Z2-R-RHI-148 | No runtime shader compilation | `Zenith2/Docs/Design/RHI.md` → "Explicit exclusions" |
| Z2-R-RHI-149 | No MSAA | `Zenith2/Docs/Design/RHI.md` → "Explicit exclusions" |
| Z2-R-RHI-150 | Client retirements batched per frame | `Zenith2/Docs/Design/RHI/Memory.md` → "Retirement queue"; `Zenith2/Docs/Design/RHI/Memory.md` → "Slot allocators" |
| Z2-R-RHI-151 | Retirement never waits on unsubmitted work | `Zenith2/Docs/Design/RHI/Memory.md` → "Retirement queue"; `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_RHILifetime header" |
| Z2-R-RHI-152 | A deferred frame loses no GPU-scene delta | `Zenith2/Docs/Design/RHI/Memory.md` → "Deferred frames and the pending store"; `Zenith2/Docs/Design/RHI/Memory.md` → "Scatter staging and coalescing" |
| Z2-R-RHI-153 | Build-node upload order | `Zenith2/Docs/Design/RHI/Memory.md` → "Build-node upload order"; `Zenith2/Docs/Design/RHI.md` → "Frame-graph nodes and declared access" |
| Z2-R-RHI-154 | Frame need above the cap fails the frame | `Zenith2/Docs/Design/RHI/Memory.md` → "Frame failures"; `Zenith2/Docs/Design/RHI.md` → "Failure handling"; `Zenith2/Docs/Design/RHI/Recovery.md` → "Device-loss state machine" |
| Z2-R-RHI-155 | Readbacks of a frame that submits nothing fail | `Zenith2/Docs/Design/RHI/Memory.md` → "Readback state machine"; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "RHI-owned ZenithFlux public headers" |
| Z2-R-RHI-156 | Retirement drain headroom | `Zenith2/Docs/Design/RHI/Memory.md` → "Drain headroom"; `Zenith2/Docs/Design/RHI.md` → "Bounded queues and full policies"; `Zenith2/Docs/Design/RHI.md` → "Permitted main-thread GPU waits" |
| Z2-R-RHI-157 | Render-idle points | `Zenith2/Docs/Design/RHI.md` → "Render-idle points"; `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_RHILifetime header"; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHIDevice header"; `Zenith2/Docs/Design/RHI.md` → "Frame-in-flight rotation"; `Zenith2/Docs/Design/RHI/Recovery.md` → "Device loss detection" |
| Z2-R-RHI-158 | Frames-in-flight change re-prepares on slot 0 | `Zenith2/Docs/Design/RHI.md` → "Changing the frames in flight"; `Zenith2/Docs/Design/RHI.md` → "Frame-in-flight rotation" |
| Z2-R-RHI-159 | Imported-state table has a single writer | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Imported-state table"; `Zenith2/Docs/Design/RHI.md` → "Lock-order rows added by RHI" |
| Z2-R-RHI-160 | Frame-failure exit | `Zenith2/Docs/Design/RHI/Memory.md` → "Frame failures"; `Zenith2/Docs/Design/RHI/Recovery.md` → "Device-loss state machine"; `Zenith2/Docs/Design/RHI.md` → "Render-idle points"; `Zenith2/Docs/Design/RHI.md` → "Telemetry" |
| Z2-R-RHI-161 | Back-end compile entries | `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Ownership of the compile steps"; `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Step 6 queue assignment"; `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Step 7 command-list partitions" |
| Z2-R-RHI-162 | HDR metadata and colour-space report | `Zenith2/Docs/Design/RHI/Recovery.md` → "Present modes and formats"; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHISwapchain header"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Swapchain and presentation"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Swapchain and presentation" |
| Z2-R-RHI-163 | Material-slot range listener | `Zenith2/Docs/Design/RHI/Memory.md` → "Material-slot range listener"; `Zenith2/Docs/Design/RHI/Memory.md` → "Slot allocators" |

## Reconciliation items

Each item names the core-document change this design needs, or the answer this design gives to another area's request; the round-1 review appended them to the cross-document register as X68 to X79, the round-2 review as X89 to X91 and the round-3 review as X128 to X130, applied under the freeze rule by the core-document batches, never by this design. "Ruled" means the register records the ruling and this design already conforms; "pending" means the core document still carries the old text.

| # | Item | Documents | State and proposal |
|---|---|---|---|
| R1 | the division of Flux code between RPROD and RHI, and the RPROD types the back end consumes | `Zenith2/Docs/Design/RPROD.md` (in drafting), `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` | accepted by the round-1 review; the shapes assumed are `Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "RPROD types the back end consumes"`, which RPROD's front-end file already declares |
| R2 | [Z2-N-REND-387] places `Flux_ToVkAccess`/`Flux_ToD3D12Access` in `ZenithFlux/Internal/Graph/`, which cannot include Vulkan or D3D12 headers ([Z2-N-CONTRACT-043]) | Rendering/RenderGraph.md | pending X74: "`Flux_ToRHIAccess` in `ZenithFlux/Internal/Graph/` plus one backend function each (`Zenith_Vk_ToStage`/`ToAccess`/`ToLayout`, `Zenith_D3D12_ToSync`/`ToAccess`/`ToLayout`)" |
| R3 | the device-loss exit code | Rendering/GpuResources.md "Device loss" step 6, Architecture/CompositionRoot.md "Exit codes", Contracts.md "Fatal errors", Design/MOD.md "Device loss and headless" | ruled by X72, pending in the four documents: a new `EXIT_CODE_DEVICE_LOST = 6` for the clean exit; `EXIT_CODE_DEVICE` (4) stays the creation refusal; `FATAL_CODE_DEVICE_LOST` only when the clean exit has not finished within 10 s; this design uses 6, also for the frame-failure exit (Memory.md "Frame failures"), so the enumerator's text is to read "the graphics device was lost and could not be recovered, or 30 consecutive frames could not be rendered (an upload need above the cap, or device memory exhausted); autosave requested" — the clause this item adds to X72's wording |
| R4 | [Z2-N-REND-006] names a ForbiddenCheck rule the rule table lacks | Contracts.md "Tests and code hygiene gates" | pending X76: add rule F20 as "Rules across the API" words it |
| R5 | backends including their API library's `Internal/` headers | Contracts.md "API boundary", Architecture BuildSystem | ruled by X68: only `Internal/Zenith_*Base.h`; this design conforms (`Zenith_RHIDeviceBase.h` includes the codec and the resource table); the Libgraph rule text must name the exemption (C-E of the review) |
| R6 | the format-feature table's home and its platform qualifier | Rendering.md "Format feature header", Contracts.md [Z2-N-CONTRACT-021] | home ruled by X69 (`Zenith2/Engine/ZenithRHI/Internal/FormatFeatures.table`, generator under `Internal/CMake/`), applied here; the `platform=` token and `m_uPlatformMask` still need Rendering's grammar row (X75, pending) |
| R7 | new manifest rows `renderdoc-app` (header-only, MIT) and the developer-only `python` and `vulkan-profiles` generator pins; lavapipe's ICD manifest path and its Win32 WSI in the shared gate environment; the composition example's factory names | Architecture.md "Third-party dependencies", Architecture/BuildSystem.md, Contracts.md "Tests and code hygiene gates", Architecture/CompositionRoot.md "The generated composition table" | pending X78 |
| R8 | the shutdown table names no row for the Flux renderer; the lock rows 410, 460, 470, 480 and row 400's main-thread use | Contracts.md "Threading and concurrency contract" | pending X76: the renderer is destroyed in step 6 after the modules, before the job system; the rows as "Lock-order rows added by RHI" |
| R9 | Stage 3 acceptance requires device-loss recovery while Z2-I-RHI-21 is S6; `Z2-B-RHI-007` counts stalls alone | Requirements/Inventory.md, Budgets.md | pending X79: Z2-I-RHI-21 S6 → S3 (Z2-I-RHI-19 stays S6); `Z2-B-RHI-007` counts stall or deferral frames |
| R10 | the new graph error `FLUX_RG_ERROR_SWAPCHAIN_QUEUE` | Rendering/RenderGraph.md "Graph errors" | pending X71(e); RPROD's `Flux_GraphErrors.h` already carries it |
| R11 | the main-thread GPU waits of "Permitted main-thread GPU waits" | Rendering/GpuResources.md "Queues and timelines" [Z2-N-REND-486], "Swapchain recreation" step 2 | pending X71(b): admit the main thread while no render subgraph is in flight (recreation, recovery, shutdown) |
| R12 | descriptor writes on the build node | Rendering.md "Binding model" [Z2-N-REND-052] | resolved: the current text puts the writes on the `Flux.BuildGraph` node after compile or at resource creation, never in a pass lambda or a `Flux.Record` job, which this design and Z2-R-RHI-014 follow |
| R13 | `Flux_`-prefixed names declared in ZenithRHI | Rendering.md "Capability negotiation", "Heap capacities per capability tier", [Z2-N-REND-028]; Contracts.md "Handle kinds" | pending X73: `Zenith_RHICapabilityTier` (`ZENITH_RHI_CAPABILITY_TIER_*`) and `Zenith_RHITextureTag` and the rest, applied here; Flux aliases `Flux_CapabilityTier`; the `Flux_*Handle` aliases stay |
| R14 | counters RHI code emits | Rendering/ShaderSystem.md [Z2-N-REND-212] | pending X71(f): RHI emits `RHI.Retirement.FullWaits` and `RHI.Pipelines.CacheHits/CacheMisses`; Flux derives `Flux.PSO.*` from the returned hit flag and re-publishes `Flux.Lifetime.DeletionQueueFullWaits` |
| R15 | GPU tests on WARP and lavapipe against "headless = no window" | Architecture.md "Headless execution" [Z2-N-ARCH-785], Roadmap.md integration smoke | pending X77: the CI smoke is device-free; GPU test executables create a device, and a hidden window for swapchain tests ("CI windows for GPU tests") |
| R16 | the upload ring's compile-time extension and at-cap deferral | Rendering.md [Z2-N-REND-482]; Rendering/GpuResources.md [Z2-N-REND-495], [Z2-N-REND-496], "Upload ring" step 2 and "Upload paths" ("at graph build, before `ReserveFrame`") | pending X71(d), amended by X90 and completed by X129 (the "Upload paths" row 2 and [Z2-N-REND-496], which X71(d) and X90 do not reach): name the extension, the deferral and the over-cap frame failure; the stall stays in `ReserveFrame`; build-node per-frame data is declared at setup (`DeclareFrameData`) and written through `GetFrameData` after `ExtendReservation` (`Zenith2/Docs/Design/RHI/Memory.md → "Build-node upload order"`); a deferral loses no GPU-scene delta (`Zenith2/Docs/Design/RHI/Memory.md → "GPU scene implementation"`) |
| R17 | the frames-in-flight trigger of swapchain recreation idles every queue, which [Z2-N-REND-983] ("never waits for device idle on Vulkan") does not admit, and [Z2-N-REND-492] ("changeable only through swapchain recreation") names no procedure | Rendering/GpuResources.md "Swapchain recreation" [Z2-N-REND-983] and step 2, "Frames in flight" [Z2-N-REND-492] | pending X89: [983] admits that one trigger as a wait for every queue's last-signalled timeline value (`Zenith_RHIDevice::WaitIdle`, never `vkDeviceWaitIdle`); [492] names the procedure of "Changing the frames in flight" (drain every swapchain, `WaitIdle`, `SetFramesInFlight`, `OnFramesInFlightChanged` re-preparing the frame about to build on slot 0, recreate every swapchain with the new image count); Z2-R-RHI-018 and Z2-R-RHI-125 are worded to it |
| R18 | the deferral's semantics and the readback enumerators; Rendering's retirement constant name | Rendering.md [Z2-N-REND-482]; Rendering/GpuResources.md [Z2-N-REND-490], [495], [496], "Readbacks" [Z2-N-REND-501] | pending X90: the pending store, the non-advancing histories and their one-frame velocity error, `FLUX_READBACK_ERROR_FRAME_DEFERRED` and `FLUX_READBACK_ERROR_FRAME_FAILED` beside `POOL_FULL`; `uFLUX_DEFERRED_DELETE_CAPACITY` realised as `uZENITH_RHI_RETIREMENT_CAPACITY` with the Flux alias of Memory.md "Drain headroom" |
| R19 | the GPU scene's CPU pending store has no RAM row | Budgets.md "Memory budgets", "Row index" | pending X91: the pending row "GPU-scene CPU pending store" in MEM-WIN-RAM and MEM-S24, producer the Stage 1 per-system budget ticket for RHI; until adopted the bytes count under `Z2-B-CORE-004` ("Pending row needs") |
| R20 | the interfaces RPROD and LIGHT asked of RHI: RP3 (the three back-end compile entries and `Flux_DemoteSplit` from steps 6 and 7), RP4 (`Flux_PassTimingPublisher::Publish` from `PrepareNextFrame` step 4), RP8 (`SetDesiredPresentTime`, `SetSwapInterval`), L21 (HDR metadata and the per-surface colour-space report) | `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` ("Compile driver", "Timestamp insertion and attribution"), `Zenith2/Docs/Design/RPROD/Renderer.md` ("Frame pacer"), `Zenith2/Docs/Design/LIGHT/ColourManagement.md` ("HDR calibration and metadata"); for L21 also Rendering.md "GPU baseline and profiles" (the optional-extension sentence) | answered, cross-area note R20: every name as those rows state it ("Names other documents assume"); the three designs may drop their `pending RHI` blocks. One core line follows from L21 and is carried by cross-area note R20 for the orchestrator's next core-document batch: Rendering's optional-extension list gains `VK_EXT_hdr_metadata`; until it lands the extension is this design's elaboration (Vulkan.md "Logical device and volk") |
| R21 | GM8: `ZENITH_RHI_FORMAT_R16_UNORM VK_FORMAT_R16_UNORM DXGI_FORMAT_R16_UNORM S` (sampled with texel `Load` alone, no `L`) for GEOM's terrain height tiles | Rendering.md "GPU baseline and profiles" (table "Required format features") | pending X130; the row joins `FormatFeatures.table` and the profile JSON in the ticket that applies X130's Rendering row, never before, because the table is one line per row of Rendering's table (`Zenith2/Docs/Design/RHI/PublicAPI.md → "Zenith_FormatFeatures header"`); cross-area note R21 |
| R22 | the renderer's prologue hook, the device-loss flag and the device-loss term of `bRunRender` that "Render-idle points" rows 3 and 5 rest on | Architecture/CPUFrameGraph.md "The frame prologue" step 4 and [Z2-N-ARCH-437]; Architecture/CompositionRoot.md "The frame loop" step 2; Design/MOD.md "Frame loop driver" | pending X128: step 4 admits `Flux_Renderer::RunPrologue()` after `Zenith_MemoryGovernor::Update`; step 2 reads `bRunRender = !headless && frameIndex > 0 && renderer.GetDeviceLossState() == FLUX_DEVICE_LOSS_STATE_NORMAL`; MOD's frame-loop driver names both (cross-area note R19; this design's R19 row above is the pending-store row, so the prologue item is numbered R22 here) |
