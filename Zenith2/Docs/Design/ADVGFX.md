# ADVGFX design: advanced graphics

Design document for the ADVGFX area (`Zenith2/Docs/Requirements/Inventory.md → "Inventory v1"`, section "ADVGFX Advanced GFX"): the classification of the advanced rendering programmes and the design of the two that are required before Stage 7. Virtual texturing is required in Stage 4, the path-traced reference in Stage 5, hardware ray tracing and virtualized geometry are deferred to Stage 7 with full requirement trees, and hair and groom are excluded (Z2-D-043). This family elaborates `Zenith2/Docs/Rendering.md` and never contradicts it ([Z2-N-REND-001]); where it needs a change to a Rendering table, struct or pass list, it says so in an assumption block and carries the change as a cross-doc item (X420 to X423 of `C:/dev/Zenith2-drafts/reviews/CROSS_DOC_ITEMS.md`). The requirement catalogue is `Zenith2/Docs/Requirements/ADVGFX.md`.

## Purpose and scope

### What ADVGFX owns

| Capability | Stage | Where specified |
|---|---|---|
| virtual texturing: the physical cache, the page-table pool, the records, the material handle encoding, sampling, feedback, residency and eviction, tile streaming, page-table updates, activation, the VT passes | S4 | `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` |
| the VT import flag, the tile cook, the `.zvt` format, its reader and parser limits | S4 | `Zenith2/Docs/Design/ADVGFX/VTCook.md` |
| the path-traced reference: the compute path tracer of `ZenithPathTracer`, its scenes, metrics, reference files, fingerprint, capture procedure and runner | S5 | `Zenith2/Docs/Design/ADVGFX/PathTracer.md` |
| hardware ray tracing: the RHI extension, acceleration structures, ray queries, hit shading, the four RT features, D-RT-SCOPE | S7 (deferred) | `Zenith2/Docs/Design/ADVGFX/RayTracing.md` |
| virtualized geometry: the cluster DAG cook, `.zvgeo`, page streaming, LOD selection, culling, rasterisation, D-VGEO-RASTER | S7 (deferred) | `Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md` |
| every public declaration | S4, S5, S7 | `Zenith2/Docs/Design/ADVGFX/PublicAPI.md` |
| every named test, fixture, harness and device run | S4, S5, S7 | `Zenith2/Docs/Design/ADVGFX/Tests.md` |

### Explicit exclusions

| Not in ADVGFX | Owner or decision |
|---|---|
| GEOM's streamed textures, their residency, the texture cook steps ADVGFX reuses, the upload arbitration ADVGFX's tiles join | GEOM (`Zenith2/Docs/Design/GEOM/Textures.md → "Virtual texturing boundary"`) |
| the render graph, barrier synthesis, views, culling, the frame of every path, the per-pass tables, the parity tooling | RPROD (`Zenith2/Docs/Rendering.md → "Render graph"`, → "Per-pass specifications") |
| GPU lifetime, the upload ring, readbacks, persistent resources, device-loss recovery, the device and backend code | RHI (`Zenith2/Docs/Rendering.md → "GPU lifetime and synchronisation"`) |
| the light model, the BRDF, the shadow, GI and reflection techniques the tracer validates, the comparison scenes and tolerances | LIGHT (`Zenith2/Docs/Design/LIGHT/Tests.md → "Path-tracer comparison scenes"`) |
| the TerrainVT reference scene and the terrain material | ENV (`Zenith2/Docs/Design/ENV/Terrain.md → "VT-backed terrain materials"`; ruling AV-R6) |
| the asset kind table and the cooked container | ASSET (`Zenith2/Docs/Design/ASSET.md → "Asset kinds"`) |
| cell streaming | STREAM; VT pages stream by feedback, independently of cells |
| tile-local merging | not before D-TBDR (open, Stage 5) (Z2-D-036) |
| MSAA | excluded (Z2-D-035) |

### Hair and groom

Strand hair and groom rendering are excluded (Z2-D-043, Z2-D-078; Z2-I-ADVGFX-04): ADVGFX has no requirement, ticket or code for them. The programme's hair is CHAR's classified alternative, hair cards (required) with shells excluded (`Zenith2/Docs/Design/CHAR.md → "Decisions"`, the hair alternatives classification), which use existing material features (alpha-tested, two-sided, anisotropic lobes) and need nothing from ADVGFX.

### Inventory items covered

Z2-I-ADVGFX-01 RT (deferred, S7), Z2-I-ADVGFX-02 virtualized geometry (deferred, S7), Z2-I-ADVGFX-03 virtual texturing (required, S4), Z2-I-ADVGFX-04 hair/groom (excluded), Z2-I-ADVGFX-05 path-traced reference (required, S5). The map is `Zenith2/Docs/Requirements/ADVGFX.md → "Coverage of inventory items"`.

### Document family

| File | Headings |
|---|---|
| `Zenith2/Docs/Design/ADVGFX.md` | the fifteen skeleton sections, then "Cross-area rows" |
| `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` | Scope of this file · Virtual texturing model · Virtual texture shape rules · Physical cache · Page-table pool · Virtual texture records · Material handle encoding · Sampling · Feedback · Feedback processing · Residency and eviction · Tile I/O and decode · Tile upload · Page-table updates · Frame-level flow · Activation and unload · Camera cuts · Deferred and failed frames · Memory accounting and pressure · Device loss recovery · Disabled virtual texturing · Mobile parameters · Pass tables |
| `Zenith2/Docs/Design/ADVGFX/VTCook.md` | Scope of this file · Import preset flag · Cook pipeline · Tile assembly · Tile compression · Determinism and derived-data keys · The cooked virtual texture format · Reader validation · Parser limits · Engine fallbacks · Cook report entry · Cook errors |
| `Zenith2/Docs/Design/ADVGFX/PublicAPI.md` | Header map · Flux_VirtualTexture header · VirtualTexture shader module · Flux_PathTracer header · Flux_PathTraceReference header · Flux_PhysicalAccuracyRunner header · Stage 7 declarations |
| `Zenith2/Docs/Design/ADVGFX/PathTracer.md` | Scope of this file · Role and boundaries · Library targets and programs · Reference scenes · Scene builder · BVH · Traversal · Wavefront pipeline · Execution substrate · Materials and gradients · BSDF evaluation and sampling · Lights and the environment · Integrators · Random numbers · Accumulation and convergence · Determinism · Raster capture · Comparison tonemap · Error metrics · Reference files · Reference manifest and fingerprint · Reference capture procedure · Gate and acceptance runs · Reports and artefacts · Proposed per-scene caps · Compute and hardware traversal · Path tracer failure handling |
| `Zenith2/Docs/Design/ADVGFX/RayTracing.md` | Scope of this file · Classification and gating · Device capability · RHI extension · Bottom-level structures · Top-level structure · Ray queries in passes · Hit shading · RT features · RT capability matrix rows and fallbacks · RT validation · RT budgets · Decision D-RT-SCOPE |
| `Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md` | Scope of this file · VG classification and gating · VG cooked format · Cluster DAG cook · Cluster page streaming · LOD selection · VG culling · VG rasterisation · VG path integration · VG validation · VG budgets · Decision D-VGEO-RASTER |
| `Zenith2/Docs/Design/ADVGFX/Tests.md` | Test targets and gate steps · Virtual texture tests · VT cook tests · Path tracer tests · Physical-accuracy runner tests · Fixtures · Benchmarks and harnesses · Device runs |

### Conventions

Zenith Hungarian (Z2-D-034) and Rendering's conventions ([Z2-N-REND-012] to [Z2-N-REND-014]): sizes in bytes, distances in metres, angles in radians, colours linear Rec.709. Design obligations are plain prose, each traceable to a `Z2-R-ADVGFX-NNN` requirement. Elaborations beyond the specification are marked `<!-- elaboration -->`; assumptions about other areas are fenced `<!-- assumption: pending <AREA>[, <X row>] -->` … `<!-- /assumption -->` and listed in "Assumed interfaces pending other areas" with their row AV1 … AV19 of `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` (AV16 is withdrawn). Under the core-doc freeze rule no ADVGFX ticket edits a core document: a ticket whose work depends on a pending X row cites it and is blocked until the orchestrator marks it applied. Two identifier families are kept apart: `AV<n>` names a cross-area row, `AV-R<n>` a design ruling of this family ("Design rulings ADVGFX owns").

## Dependencies

### Libraries ADVGFX code lives in

ADVGFX adds one library, `ZenithPathTracer` (tool-side, already listed in `Zenith2/Docs/Architecture.md → "Library dependency graph"` and `Zenith2/Docs/Architecture.md → "Repository layout"`). Virtual texturing lives in `ZenithFlux` (layer 5, whose dependencies Architecture owns: ZenithCore, ZenithGraph, ZenithJobs, ZenithProfiling, ZenithRHI, ZenithVertexFormat, ZenithAssets, ZenithShadersGenerated; VT tile decode also calls `Zenith_DecompressSection`, so cross-doc item X421 (c) adds the direct edge ZenithFlux → ZenithCompression that [Z2-N-ARCH-167] requires, which GEOM's texture streamer needs as well), the VT cook in `ZenithCook`, the shaders under `Zenith2/Shaders/`, and two call sites in `ZenithRuntime` ("ZenithRuntime call sites"). The Stage 7 RHI extension lives in RHI's libraries under RHI's review.

| Dependency | Used for |
|---|---|
| ZenithCore | containers (`Zenith_Array`, `Zenith_BoundedQueue`), arenas, `Zenith_Hash64`, `Zenith_Sha256`, the memory governor (`Zenith2/Docs/Architecture.md → "Memory governor"`), configuration |
| ZenithJobs | `Zenith_JobContext::ParallelFor` sub-jobs of the build node for tile decode |
| ZenithRHI | device creation of the pools, formats, capabilities |
| ZenithAssets | the kind loader, range reads, section verification (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetKindLoader"`, → "Zenith_AssetReader header") |
| ZenithCompression | per-tile and per-plane decode (`Zenith2/Docs/Design/COMPRESS.md → "Section compression and decompression"`) |
| ZenithProfiling | counters and the DEVT area-registration interface |

### Headings this design relies on

| Heading | For |
|---|---|
| `Zenith2/Docs/Rendering.md → "GPU scene tables and pools"`, `Zenith2/Docs/Rendering/GpuResources.md → "Pools"`, → "Persistent resources", → "Upload ring", → "Readbacks", → "Device loss", → "Responses to memory pressure" | the VT pools' graph model, the record buffer, frame data, feedback readback, recovery, shedding |
| `Zenith2/Docs/Rendering.md → "Binding model"`, → "Materials" | handles, the push constant, the surface function contract VT extends |
| `Zenith2/Docs/Rendering.md → "Validation roles"` | the physical-accuracy role the tracer serves ([Z2-N-REND-944] to [Z2-N-REND-948]) |
| `Zenith2/Docs/Rendering/PerPass/Common.md → "Conventions"`, → "Prologue passes" | the VT pass tables and their placement |
| `Zenith2/Docs/Design/RPROD/Renderer.md → "Flux_Renderer header"`, → "Create sequence", → "Render frame sequence", → "Configuration keys" | ENABLE-FEATURES, the build-node steps, key registration |
| `Zenith2/Docs/Design/RPROD/Tests.md → "Parity suites"`, → "Parity scenes" | the parity runner, the scene format the tracer reads |
| `Zenith2/Docs/Design/GEOM/Textures.md → "Virtual texturing boundary"`, → "Streaming upload arbitration", → "Texture cook pipeline", → "Fallback textures" | the GEOM boundary |
| `Zenith2/Docs/Design/LIGHT/Lighting.md → "BRDF"`, → "Light attenuation and shape" | the reflectance and light model the tracer shares |
| `Zenith2/Docs/Design/ASSET/Formats.md → "Header layout"`, → "Checks"; `Zenith2/Docs/Design/ASSET/Cook.md → "Engine fallbacks"` | the container and fallbacks |
| `Zenith2/Docs/Design/DEVT.md → "Upward interfaces DEVT declares"` | area registration |

### Upward-callback interfaces

ADVGFX declares no upward interface. The renderer calls the VT system downward from its build node; the asset system reaches it through the `Zenith_AssetKindLoader` interface ZenithAssets declares; memory pressure arrives through RHI's rendering memory consumer, which calls `Flux_VirtualTextureSystem::SetPressureBias` downward; the debug view is a `Flux_ViewExtension`, an interface ZenithFlux declares (`Zenith2/Docs/Design/RPROD/Views.md → "View extensions"`); DEVT's area list calls `Flux_RegisterVirtualTextureDevTools` downward from ZenithRuntime. So every callback points downward (`Zenith2/Docs/Contracts.md → "Layering DAG"`).

### Assumed interfaces pending other areas

<!-- assumption: pending RPROD, RHI, GEOM, ASSET, CONTENT, SEC, ENV, LIGHT, DEVT, STREAM, MOD, EDIT, ARCH, BUDGET -->
| Row | Area | Cross-doc item | Assumption | Where used |
|---|---|---|---|---|
| AV1 | RPROD | X420 | (a) `Flux_Renderer::EnableVirtualTexturing` and `GetVirtualTextureSystem` declared by RPROD (ENABLE-FEATURES) and the VT calls at build-node steps 3c, 7, 11 and 13; (b) every pass with an alpha-test permutation or material variants (`Prepass`, `DeferredDepthPrepass` and `ShadowDepth` alpha-tested buckets, `VisibilityRaster`, `ForwardPlusOpaque`, `ForwardOpaquePerObject`, `GBuffer`, `MaterialResolve`) declares the three `[Optional]` VT pool fields and fills `Flux_MaterialInput::m_xVirtual`; (c) `EvaluateMaterial` (`Material.slang`, which imports `VirtualTexture`) calls the public `Flux_VTBeginInvocation(xIn.m_xVirtual, pixel)` before the material type runs; (d) `Flux_SampleMaterial`'s VT branch; (e) the four feedback writers' fields (`m_xVTFeedback`, `m_uVTFeedbackStride`, `m_uVTFeedbackJitter`) and `[earlydepthstencil]`; (f) `Flux_ViewTargets::m_xVTFeedback` and the path functions' calls of the two feedback feature functions; (g) the parity scene `pathTrace` object, the `VirtualTexture` parity scene and the HDR post-TAA capture; (h) `FLUX_INSTANCE_FLAG_VIRTUAL_GEOMETRY` at Stage 7 | "Material input binding", "Feedback writers", "Frame-level flow" (VirtualTexturing.md); "Reference scenes", "Raster capture" (PathTracer.md) |
| AV2 | MOD | — | ZenithRuntime calls `EnableVirtualTexturing(Flux_ReadVirtualTextureDesc(config))` at startup step 13 right after the renderer's `Create`, whether VT is enabled or not, and adds ADVGFX's DEVT area-list entry | "ZenithRuntime call sites" |
| AV3 | GEOM | — | (a) the material cook accepts a kind-18 GUID for a texture field of an opaque or alpha-tested material and refuses it in transparent and decal materials; (b) the material manager writes `GetMaterialHandle()` for a resident VT and the usage fallback otherwise, rewriting blocks on each change; (c) `Flux_StreamingUploader::Enqueue`; (d) `Zenith_BuildTextureMipChain` and `Zenith_EncodeTextureImage`; (e) Stage 7: mesh residency edges and per-slot RT alpha records; (f) Stage 7: a VG upload class | VirtualTexturing.md, VTCook.md, RayTracing.md, VirtualizedGeometry.md |
| AV4 | RHI | X420 | (a) a second sampled view format for block-compressed mutable images; (b) the texture-region form of `Flux_UploadDestination` into a graph-imported pool texture; (c) `m_uReserved` of a material-constant handle may be non-zero (the VT tag and slot; word 0 stays a real heap index, the tail texture's) and `Flux_HandleWords` in `Bindless.slang`; (d) Stage 7: the `Zenith_RHIRayTracing.h` extension in RHI's files | "Physical cache texture", "Material handle encoding", "Tile upload" (VirtualTexturing.md); "RHI extension" (RayTracing.md) |
| AV5 | ASSET | — | (a) kind 18's row stands as listed (`Flux_VirtualTextureResource`, SUBSTITUTE, not cacheable, GPU resident) and section kinds 50 to 54 are ADVGFX's; (b) Stage 7: `ASSET_KIND_VIRTUAL_GEOMETRY` requested, code by KIND-CODE-ORDER | VTCook.md, VirtualizedGeometry.md |
| AV6 | CONTENT | — | the import preset keys `texture.virtual`, `texture.virtualMipBias` and (Stage 7) `mesh.virtualized` | "Import preset flag" (VTCook.md) |
| AV7 | SEC | X421 (b) | the `.zvt` and `.zref` parser limits join SEC's inventory and Architecture's "Parser limits" table | "Parser limits" (VTCook.md), "Reference files" (PathTracer.md) |
| AV8 | ENV | — | answers EN19: ENV keeps the TerrainVT scene; the tracer reads its LOD-0 collision tiles | "Scene builder" (PathTracer.md) |
| AV9 | LIGHT | — | (a) `PT_TerrainVT` names ENV's TerrainVT scene; (b) `Flux_EvaluateBRDF` and `Flux_SampleDFG` pure and stage-independent with the assumed signature; (c) Stage 7: `Flux_WorldLightSource` and RT results consumed by LIGHT's lighting | PathTracer.md, RayTracing.md |
| AV10 | DEVT | — | the MP-STREAM runner and the overlay read `Flux.VT.*` | "Telemetry" |
| AV11 | STREAM | — | `Z2-B-STREAM-004`'s teleport measurement includes VT pages through DEVT's runner | "Telemetry" |
| AV12 | ARCH | X421 (a) | the VT shedding actions join Architecture's Rendering shedding row | "Memory accounting and pressure" (VirtualTexturing.md) |
| AV13 | EDIT | — | the inspector and asset browser show kind 18's stats | "Editor surfaces" |
| AV14 | BUDGET | X422 | the pending rows of "Pending row needs" | "Budget rows" |
| AV15 | DecisionLog | X423 | D-RT-SCOPE (open, Stage 7) and D-VGEO-RASTER (open, Stage 7) registered | "Open decisions" |
| AV16 | — | — | withdrawn: ADVGFX defines no misuse code and reserves no block (ruling MISUSE-BLOCKS-NEW-AREAS-3) | — |
| AV17 | RPROD | — | answered by `Zenith2/Docs/Design/RPROD/Renderer.md → "Tool graph entry"`: `RunToolGraph`, `SettleToolFrames`, `AcquireToolMaterialSlot`, `ReleaseToolMaterialSlot` and the 11-value `Flux_ToolGraphError`; a render frame is in progress from `Flux.BuildGraph` start to `Flux.Present` end; a tool frame takes the next frame slot under the slot rule, consumes a frame index and presents nothing; the caller is `MAIN_THREAD` and phases A and C run as root jobs continuing on `JOB_AFFINITY_RENDER_SUBMIT` with the job system's `Zenith_JobContext`; RPROD pumps VT settlement through `SettleToolFrames`; device loss releases every tool material slot | "Execution substrate" (PathTracer.md) |
| AV18 | RHI, RPROD | X420 (i) | the probe case `Test_BindlessProbe_HandleWordsRoundTrip` joins the bindless probe table and D-SLANG's acceptance rule, and ADVGFX joins D-SLANG's consumers blocked | "Material handle encoding" (VirtualTexturing.md) |
| AV19 | GEOM | — | `FLUX_UPLOAD_CLASS_VT_TILE` gets a per-frame floor of `ceil(Z2-B-ADVGFX-002 / 4)` tiles ahead of the other streaming classes (never ahead of `BLOCKING`), and its class limit equals the row value | "Tile upload" (VirtualTexturing.md) |
<!-- /assumption -->

## Library and file layout

### ZenithFlux files

| Path | Content |
|---|---|
| `Zenith2/Engine/ZenithFlux/Flux_VirtualTexture.h` | the public header (`Zenith2/Docs/Design/ADVGFX/PublicAPI.md → "Flux_VirtualTexture header"`) |
| `Zenith2/Engine/ZenithFlux/Internal/VirtualTexture/Flux_VirtualTextureSystem.cpp`, `.h` | the system: frame steps, enable, recovery |
| `Internal/VirtualTexture/Flux_VirtualTextureResource.cpp` | the resource |
| `Internal/VirtualTexture/Flux_VTRegionAllocator.cpp`, `.h` | the 2D buddy allocator |
| `Internal/VirtualTexture/Flux_VTPhysicalCache.cpp`, `.h` | the physical cache, page states, free list, LRU |
| `Internal/VirtualTexture/Flux_VTMirror.cpp`, `.h` | the CPU mirror, map and unmap, dirty rows, run records |
| `Internal/VirtualTexture/Flux_VTFeedback.cpp`, `.h` | readback ring, sort and unique, validation, touch, targets, priority |
| `Internal/VirtualTexture/Flux_VTTileStreamer.cpp`, `.h` | reads, decode sub-jobs, uploads, the tile state machine |
| `Internal/VirtualTexture/Flux_VTReader.cpp`, `.h` | the `.zvt` reader and its limits |
| `Internal/VirtualTexture/Flux_VTKindLoader.cpp`, `.h` | the kind loader and the activation queue |
| `Internal/VirtualTexture/Flux_VTPasses.cpp` | the three feature functions |
| `Internal/VirtualTexture/Flux_VTConfig.cpp` | key registration and `Flux_ReadVirtualTextureDesc` |
| `Internal/VirtualTexture/Flux_VTDevTools.cpp`, `Flux_VTDebugExtension.cpp`, `.h` | DEVT registration, the debug view extension |
| `Internal/Renderer/Flux_Renderer.cpp` | the bodies of `EnableVirtualTexturing` and `GetVirtualTextureSystem` and the owned member (ruling ENABLE-FEATURES) |
| `Zenith2/Engine/ZenithFlux/Content/VirtualTextures/` | the engine fallback sources |
| `Zenith2/Engine/ZenithFlux/Tests/VirtualTexture/Flux_VirtualTexture_Tests.cpp`, `Flux_VTSampling_Tests.cpp`, `Flux_VTResidency_Tests.cpp`, `Flux_VTFeedback_Tests.cpp` | the VT tests |
| `Zenith2/Engine/ZenithFlux/Tests/Fuzz/VirtualTextureFile.cpp` | the fuzz target |
| `Zenith2/Engine/ZenithFlux/Tests/PhysicalAccuracy/Flux_PhysicalAccuracy_Tests.cpp`, `Flux_PTMetrics_Tests.cpp`, `References/` | the runner and metric tests and the committed references |
| `Zenith2/Engine/ZenithFlux/Tests/PhysicalAccuracy/Runner/` | the runner, metrics and comparison tonemap: test support compiled into `ZenithFlux_Tests` alone (the `flip` header's one target) |
| `Zenith2/Engine/ZenithFlux/Tests/Support/PathTrace/Flux_PTSceneDocument.h` | the header-only scene-document helper (`Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Scene-document helper"`), included by the runner and by `Zenith_Bench_PathTraceReference` |
| `Zenith2/Engine/ZenithFlux/Tests/Parity/Scenes/VirtualTexture.scene.json` | the VT parity scene (RPROD's layout) |

### Shader files

| Path | Content |
|---|---|
| `Zenith2/Shaders/Common/VirtualTexture.slang` | the shared VT structs, sampling, reservoir, feedback write, codecs |
| `Zenith2/Shaders/Passes/VirtualTexture/VTFeedbackClear.slang`, `VTPageTableUpdate.slang` | the two VT programs |
| `Zenith2/Shaders/Tests/VirtualTexture/VTSampleHarness.slang` | the sampling test harness |
| `Zenith2/Tools/ZenithShaderCompiler/Tests/Fixtures/VTReservoirVisibility_CompileFail.slang.in`, `VTReservoirVisibility_Control.slang.in` | the visibility fixtures of `Test_VirtualTextureShader_ReservoirNotNameableFromModules`, outside `Shaders/Tests/` so no build compiles them |
| `Zenith2/Shaders/Tests/PathTracer/*.slang` | the tracer's eight programs (`Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Library targets and programs"`) |

### ZenithCook files

| Path | Content |
|---|---|
| `Zenith2/Tools/ZenithCook/Internal/VirtualTexture/Zenith_CookVirtualTexture.cpp`, `.h` | the cook step and its errors |
| `Zenith2/Tools/ZenithCook/Internal/VirtualTexture/Zenith_VTTileAssembler.cpp`, `.h` | tile assembly and rings |
| `Zenith2/Tools/ZenithCook/Tests/VirtualTexture/Zenith_CookVirtualTexture_Tests.cpp` | the cook tests |

### ZenithPathTracer files

The library of `Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Library targets and programs"`, `Zenith2/Tools/ZenithPathTracer/` with its two public headers, the internal files listed there, `Tests/Flux_PathTracer_Tests.cpp`, `Tests/Flux_PTReference_Tests.cpp` and `Tests/Fuzz/PathTraceReference.cpp`. The harnesses are `Zenith2/Benchmarks/PathTraceReference/` and `Zenith2/Benchmarks/VirtualTexture/`.

### ZenithRuntime call sites

<!-- assumption: pending MOD -->
| Call site | Rule |
|---|---|
| startup step 13 (`Zenith2/Docs/Architecture/CompositionRoot.md → "Startup sequence"`) | immediately after `Flux_Renderer::Create` succeeds: `EnableVirtualTexturing(Flux_ReadVirtualTextureDesc(xConfig))`; a failure is exit code 4 with one diagnostic line naming the `Flux_VirtualTextureError`, after the renderer unwinds (row AV2) |
| `Zenith2/Engine/ZenithRuntime/Internal/DevTools/Zenith_DevToolsAreaList.cpp` | the entry `{ "ADVGFX", &Flux_RegisterVirtualTextureDevTools, &<getter returning m_pxRenderer->GetVirtualTextureSystem()> }` |
<!-- /assumption -->

### Stage 7 files

| Path | Content |
|---|---|
| `Zenith2/Engine/ZenithRHI/Zenith_RHIRayTracing.h`, `ZenithRHI_Vulkan/Internal/Zenith_VkRayTracing.cpp`, `ZenithRHI_D3D12/Internal/Zenith_D3D12RayTracing.cpp` | the RHI extension (RHI's files) |
| `Zenith2/Engine/ZenithFlux/Flux_RayTracing.h`, `Internal/RayTracing/` | the RT scene, builds and features |
| `Zenith2/Shaders/Passes/RayTracing/` | `RTInstanceBuild`, `RTShadows`, `RTReflectionTrace`, `RTHitClassify`, `RTHitResolve`, `RTAmbientOcclusion`, `RTGITrace`, the denoisers |
| `Zenith2/Engine/ZenithFlux/Flux_VirtualGeometry.h`, `Internal/VirtualGeometry/`, `Zenith2/Shaders/Passes/VirtualGeometry/` | VG runtime and shaders |
| `Zenith2/Tools/ZenithCook/Internal/VirtualGeometry/` | the VG cook |
| `Zenith2/Benchmarks/RayTracing/`, `Zenith2/Benchmarks/VirtualGeometry/` | the decision harnesses |

### Supported matrices

| Target | `ZENITH_SUPPORTED` |
|---|---|
| ZenithFlux (VT code) | ZenithFlux's own matrix (RPROD's; every preset but `linux-tsan`) |
| ZenithPathTracer | `win-debug`, `win-release`, `win-tools`, `win-asan` ([Z2-N-REND-948]) |
| ZenithCook (VT cook) | the tool host presets |
| `Zenith_Bench_PathTraceReference`, `Zenith_Bench_VirtualTexture` | the Windows presets |

## Public API

### Header summary

The full declarations are `Zenith2/Docs/Design/ADVGFX/PublicAPI.md`; its "Header map" lists every header. In one line each:

| Type | Contract in one line |
|---|---|
| `Flux_VirtualTextureSystem` | owned by the renderer, created at startup step 13 by `EnableVirtualTexturing`; runs on the build node; owns every VT GPU resource |
| `Flux_VirtualTextureResource` | the kind-18 runtime object, shared through `Zenith_AssetHandle`; hands GEOM the material handle |
| `Flux_VirtualTextureDesc` | the `render.vt.*` configuration |
| `Flux_PathTracer` | a test tool's tracer: `Create`, `Run`, `Cancel` |
| `Flux_PathTraceSceneBuilder`, `Flux_PathTraceScene` | the static reference scene and its BVH |
| `Flux_PathTraceReference` | the `.zref` content and its fingerprint |
| `Flux_PhysicalAccuracyRunner`, `Flux_ImageMetrics` | test support of `ZenithFlux_Tests`: what LIGHT's and ENV's comparison tests call, and the comparison metrics |

### Rules across the API

- Every fallible function returns `Zenith_Result<T, E>` with E one of `Flux_VirtualTextureError`, `Flux_PathTraceError` or `Zenith_VirtualTextureCookError` ([Z2-N-CONTRACT-461]).
- Every public type carries one contract block (`Zenith2/Docs/Contracts.md → "Contract blocks per type"`); constructors that only a factory may call are private.
- No public header includes a third-party, platform or STL header; `ZenithPathTracer`'s headers include public headers of its declared dependencies alone (ZenithCore, ZenithRHI, ZenithCompression, ZenithFlux), one for every type they name, and the `flip` header is reached only from the test support of `ZenithFlux_Tests`.
- File data is untrusted: every count, size and offset is checked before use, and failures are errors, never asserts ([Z2-N-ARCH-951]).

### Handle kinds used

| Handle | Kind (`Zenith2/Docs/Contracts.md → "Handle kinds"`) | ADVGFX's role |
|---|---|---|
| `Zenith_AssetHandle<Flux_VirtualTextureResource>` | shared asset handle | declares the runtime type of kind 18 |
| `Zenith_RHITextureHandle`, `Zenith_RHIBufferHandle` | device handles | the pools and the record buffer |
| `Zenith_RHIDescriptorHandle` | plain value (heap index) | the VT material handle encoding |
| `Zenith_OperationHandle`, `Flux_UploadOperation`, `Flux_ReadbackHandle` | operation handles | tile reads, uploads, feedback readbacks |
| `Zenith_RHIAccelerationStructureHandle` (Stage 7) | device handle | acceleration structures |

### Configuration keys

Registered by `Flux_RegisterVirtualTextureConfigKeys`, which `Zenith_RegisterFluxConfigKeys` calls after RPROD's keys (`Zenith2/Docs/Design/RPROD/Renderer.md → "Configuration keys"`):

| Key | Type | Default (desktop / Android) | Layers | Restart | Meaning |
|---|---|---|---|---|---|
| `render.vt.enabled` | BOOL | true / true | engine, platform, game | yes | create the VT pools |
| `render.vt.physicalLayers` | INT [1, 64] | 12 / 4 | engine, platform, game | yes | physical cache layers |
| `render.vt.pageTableExtent` | ENUM {512, 1024, 2048, 4096} | 2048 / 1024 | engine, platform, game | yes | page-table pool extent |
| `render.vt.feedbackScale` | ENUM {8, 16} | 8 / 16 | every | no | feedback block size |
| `render.vt.tilesPerFrame` | INT [1, 512] | 32 / 12 | engine, platform, quality tier | no | tile uploads per frame outside a camera-cut boost; at most half of `Z2-B-ADVGFX-002`, whose value is the boosted peak |
| `render.vt.maxReadsInFlight` | INT [8, 1024] | 128 / 64 | engine, platform | no | tile reads in flight |
| `render.vt.maxAnisotropy` | ENUM {1, 2, 4, 8} | 8 / 4 | engine, platform, quality tier | no | the VT anisotropy cap |
| `render.vt.mipBias` | FLOAT [−2, 4] | 0 / 0 | every | no | the feedback bias |
| `render.vt.cutBoostFrames` | INT [0, 240] | 30 / 30 | engine, platform | no | the camera-cut boost |
| `render.vt.maxTailBytes` | INT [262,144, 67,108,864] | 8,388,608 / 2,097,152 | engine, platform | yes | the tail-texture reservation inside `Z2-B-ADVGFX-001` ("Physical cache sizing" of VirtualTexturing.md) |
| `render.vt.maxIndexBytes` | INT [1,048,576, 268,435,456] | 33,554,432 / 8,388,608 | engine, platform | yes | the host cap on tile indices and mirrors ("Host memory bound" of VirtualTexturing.md) |
| `render.vt.maxFeedbackWidth`, `render.vt.maxFeedbackHeight` | INT [256, 8192] | 3,840 × 2,160 / 2,340 × 1,080 | engine, platform | yes | the render extent the feedback sort arena is sized for |
| `render.vt.residencyMode` | ENUM {stream, full} | stream / stream | command line (development builds and tests) | yes | residency mode |

## Data structures and formats

### Format index

| Format | Where |
|---|---|
| `.zvt` (kind 18) | `Zenith2/Docs/Design/ADVGFX/VTCook.md → "The cooked virtual texture format"` |
| `.zref` reference images | `Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Reference files"` |
| `References.json` | `Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Reference manifest and fingerprint"` |
| the parity scene `pathTrace` object | `Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Reference scenes"` |
| `.zvgeo` (Stage 7) | `Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md → "VG cooked format"` |
| the VT dump (`r.vt.dump`) | "Console commands" |

### In-memory layouts that matter

| Structure | Size | Where |
|---|---|---|
| page-table entry | 4 bytes | `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Page-table entry encoding"` |
| feedback request | 4 bytes | `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Feedback request encoding"` |
| `Flux_VTRecord` | 32 bytes | `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Virtual texture records"` |
| `Flux_VTPageTableRun` | 16 bytes | `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Dirty rows and run records"` |
| VT material handle | 8 bytes | `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Material handle encoding"` |
| tile | 18,496 bytes | `Zenith2/Docs/Design/ADVGFX/VTCook.md → "Tile assembly"` |
| tile index entry | 40 bytes | `Zenith2/Docs/Design/ADVGFX/VTCook.md → "Tile index section"` |
| `Flux_PTNode`, `Flux_PTTriangle`, `Flux_PTVertex`, `Flux_PTPath`, `Flux_PTHit` | 32, 48, 64, 64, 16 bytes | `Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Node and triangle layout"`, → "Path state" |

## Algorithms and state machines

### Algorithm index

| Algorithm | Complexity | Failure behaviour | Where |
|---|---|---|---|
| VT sampling | O(1): one record load, ≤ 2 entry loads, ≤ 2 filtered samples | an unwritten entry cannot occur for an active VT | `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Flux_SampleVirtual algorithm"` |
| feedback sort and unique | O(n), n ≤ the sort arena's capacity (1,036,800 requests: 8 views of 3,840 × 2,160 at s = 8) | invalid requests dropped and counted | `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Sort and unique"` |
| touch, targets, priority | O(n × 8) touches; O(c log c) candidates | — | `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Touch rule"`, `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Priority order"` |
| eviction | O(1) per victim (LRU head) | starvation counted, never forced | `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Eviction policy"` |
| map, unmap | O((4^(m+1) − 1) / 3) per page | — | `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Map and unmap algorithms"` |
| run-record cut | O(dirty entries) | record cap defers rows | `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Dirty rows and run records"`, `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Eviction epochs"` |
| region allocation | O(log extent) | `FLUX_VT_ERROR_PAGE_TABLE_FULL` | `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Region allocation"` |
| tile assembly | O(tiles × 1,156 blocks) | — | `Zenith2/Docs/Design/ADVGFX/VTCook.md → "Tile assembly"` |
| BVH build | O(n log n) | `FLUX_PT_ERROR_BVH_LIMIT` | `Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Builder"` |
| tracing | O(pixels × samples × bounces × log n) | caps return `NOT_CONVERGED` | `Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Wavefront pipeline"` |
| convergence check | O(pixels) per batch on the CPU | — | `Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Accumulation and convergence"` |
| metrics | O(pixels) (FLIP's filters O(pixels × kernel)) | size mismatch is `FLUX_PT_ERROR_INVALID_SETTINGS` | `Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Error metrics"` |

### State machine index

| State machine | Where |
|---|---|
| physical page | `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Physical page state machine"` |
| tile request | `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Tile request state machine"` |
| VT activation (loader rows) | `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Activation steps"` |
| tracer run (batches: running, converged, not converged, cancelled) | `Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Accumulation and convergence"` |

### Common prologue placement

<!-- assumption: pending RPROD, X420 (e) -->
`VTPageTableUpdate` joins the common prologue of `Zenith2/Docs/Rendering/FrameAnatomy.md → "Common prologue"` immediately after `StreamingUpload` (step 2), before compute skinning and every other pass, so every material pass of every view reads the frame's page table; the per-view `VTFeedbackClear` runs in the per-view prologue before the view's first feedback writer, and `VTFeedbackReadback` after the view's last feedback writer and before `TemporalAA`. The frame-table rows and the three pass tables of `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Pass tables"` join Rendering through cross-doc item X420 (e).
<!-- /assumption -->

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| `Flux_VirtualTextureSystem` | `RENDER_SUBMIT_THREAD` (the `Flux.BuildGraph` node) for the frame members and feature functions; `MAIN_THREAD` for `EnableVirtualTexturing`, `SetPressureBias` and the recovery members; `ANY_THREAD` for queries |
| `Flux_VirtualTextureResource` | `ANY_THREAD` reads; values change on the build node |
| `Flux_VirtualTextureKindLoader` | ZenithAssets' loader threads for `BeginLoad`, `OnSectionsLoaded`, `Cancel` (before activation), `Unload` (a push onto `flux.vt.unloadQueue`, touching no VT state) and `DropCpuCopies` (an acquire read of the object's FAILED flag); the activation and unload drains on the build node |
| decode sub-jobs | worker threads, as `ParallelFor` sub-jobs of the build node, joined before it continues |
| `Flux_PathTracer` | `MAIN_THREAD`: the thread that drives the renderer's frames, between render frames; each batch is one `Flux_Renderer::RunToolGraph` call, which runs it on the render-submit role (row AV17); `Cancel` `ANY_THREAD` |
| `Flux_PathTraceSceneBuilder`, `Flux_PathTraceResult`, `Flux_PathTraceReference`, `Flux_PhysicalAccuracyRunner` | `OWNER_THREAD` (the test's thread) |
| cook step | one ZenithCook worker process per invocation, single-threaded |

### Frame-graph nodes and declared access

ADVGFX adds no CPU frame-graph node: its runtime work runs inside RPROD's `Flux.BuildGraph`, `Flux.Record` and `Flux.Submit` nodes of the render subgraph (`Zenith2/Docs/Design/RPROD/Renderer.md → "Render frame sequence"`), and reads only the sealed render world's view requests (for camera cuts) and its own GPU state, so it declares no ECS access. Its GPU passes declare their accesses through their parameter structs ("Pass tables" of VirtualTexturing.md).

### Lock-order entries

ADVGFX takes no lock. The activation queue `flux.vt.activationQueue` and the unload queue `flux.vt.unloadQueue` are CORE `Zenith_BoundedQueue`s (multi-producer, single-consumer use) of capacity 256 with `QUEUE_FULL_POLICY_REJECT`, which a push never meets because the kind loader admits at most 256 concurrent loads and the asset system unloads each object once; both are drained at step 3c, the unload queue first, so every VT state change happens on the build node; tile reads complete through polled operation handles; decode results return through the sub-jobs' own staging slots.

### Cancellation and shutdown

| Event | Rule |
|---|---|
| unload | "Activation steps" (VirtualTexturing.md): reads cancelled, staging discarded on completion, queued uploads cancelled, pages drained, region and tail retired, slot quarantined |
| shutdown | at the renderer's shutdown (step 6), after every queue went idle: every read is cancelled and its completion awaited by polling at most 2 s, then the system releases its pools through deferred deletion and is destroyed before GEOM's managers |
| device loss | "Device loss recovery" (VirtualTexturing.md) |
| tracer | `Cancel` takes effect at the next batch boundary ("Path tracer failure handling") |
| simulation | VT is presentation-only: no simulation-visible value depends on residency, feedback or pressure ([Z2-N-ARCH-812]) |

### Error enumerations

| Enumeration | Header | Values |
|---|---|---|
| `Flux_VirtualTextureError` | `Flux_VirtualTexture.h` | 17 values (`Zenith2/Docs/Design/ADVGFX/PublicAPI.md → "Flux_VirtualTexture header"`) |
| `Zenith_VirtualTextureCookError` | `Zenith_CookVirtualTexture.h` | 4 values (`Zenith2/Docs/Design/ADVGFX/VTCook.md → "Cook errors"`) |
| `Flux_PathTraceError` | `Flux_PathTracer.h` | 12 values |

Programmer errors (a feature function called off the build node, `WriteFrameData` before the reservation is final, a slot index out of range in engine code) are `ZENITH_ASSERT`; file data, GPU feedback values and configuration are never asserted on.

### Luau errors and asserts

ADVGFX registers no Luau binding and raises no misuse code: virtual textures reach scripts only as asset handles of the script name `VirtualTexture`, which ASSET's bindings carry, and the tracer is tool-side.

## Diagnostics and profiling

### Counters

Emitted once per render frame through the profiler hooks; `Flux_VirtualTextureStats` carries them in value form:

| Counter | Kind | Meaning |
|---|---|---|
| `Flux.VT.ActiveTextures`, `.PhysicalPages`, `.ResidentPages`, `.FreePages`, `.DrainingPages` | GAUGE | residency |
| `Flux.VT.Requests` | GAUGE | unique valid requests processed this frame |
| `Flux.VT.FeedbackRejected`, `.FeedbackSkipped` | CUMULATIVE | dropped requests; feedback frames not declared (ring full) |
| `Flux.VT.ReadsInFlight` | GAUGE | tile reads |
| `Flux.VT.TilesUploaded` | DELTA | tiles uploaded this frame |
| `Flux.VT.Evictions`, `.EvictionStarved` | CUMULATIVE | evictions; decoded tiles that found no page |
| `Flux.VT.PageTableRecords` | GAUGE | run records this frame |
| `Flux.VT.ReadFailures`, `.CorruptTiles`, `.UploadFailures` | CUMULATIVE | failures |
| `Flux.VT.CutBoostFrames` | CUMULATIVE | boosted frames |
| `Flux.VT.DeviceBytes` | GAUGE | device memory against `Z2-B-ADVGFX-001` |
| `Flux.VT.HostBytes` | GAUGE | host memory against the proposed host row |
| `Flux.VT.UploadStarvedFrames` | CUMULATIVE | frames in which `VT_TILE` received fewer tiles than the smaller of its floor and the decoded tiles waiting |
| `Flux.VT.FirstHopFramesP95`, `.FinestMipFramesP95` | GAUGE | the two latency rows' measures ("Request latency floor" of VirtualTexturing.md) |
| `Flux.VT.PressureBias` | GAUGE | 0, 1 or 2 |

### On-screen profiler rows

The rendering group gains a `VT` row (resident and free pages, requests, tiles uploaded against `Z2-B-ADVGFX-002`, memory against `Z2-B-ADVGFX-001`), and the GPU timeline shows `VTPageTableUpdate`, `VTFeedbackClear` and `VTFeedbackReadback` under `Z2-B-ADVGFX-003`. The overlay tab `vt` lists the active VTs with resident pages, corrupt pages and residency state. Registration is "Console commands".

### Log categories

`flux.vt` (activation, failures once per VT, eviction starvation once per 600 frames, configuration refusals); `flux.pt` (tracer runs, outcomes, reference freshness).

### Console commands

Registered by `Flux_RegisterVirtualTextureDevTools` through the DEVT area tokens (`Zenith2/Docs/Design/DEVT.md → "Upward interfaces DEVT declares"`), development builds:

| Command | Effect |
|---|---|
| `r.vt.dump` | writes `zenith2.vt-dump` version 1 (JSON: the stats, then per VT its GUID, shape, slot, region, resident and corrupt pages per mip, and the 64 most recent requests) to `user:/devtools/` |
| `r.vt.flush` | evicts every unpinned page at the next frame |
| `r.vt.freeze 0\|1` | stops (1) or resumes (0) feedback processing |
| `r.debug.vt <mode>` | the debug view of "Debug views" |

### Debug views

`Flux_VTDebugExtension`, a `Flux_ViewExtension` at `FLUX_EXTENSION_POINT_OVERLAY` for main and editor views, registered by `EnableVirtualTexturing` when the console exists: mode 0 off, 1 the resident page mip per pixel (colour ramp), 2 the gap between requested and resident mip, 3 page borders, 4 a thumbnail of the physical cache's occupancy, 5 the feedback buffer.

### Frame dump section

A `vt` section of the DEVT frame dump: the stats, the frame's run-record count per mip, the feedback views and their readback states.

### Telemetry

The frame telemetry record carries the gauges and the per-frame deltas of the cumulative counters; the MP-STREAM runner reads `Flux.VT.FirstHopFramesP95`, `Flux.VT.FinestMipFramesP95` and the resident-page gauges with GEOM's texture-residency counters for `Z2-B-STREAM-004` (rows AV10, AV11). The tracer's runs write `report.json` files, not telemetry.

## Authoring and cooking

### What ADVGFX authors

ADVGFX has no authoring document. A virtual texture is authored as any source image with the import preset keys `texture.virtual` and `texture.virtualMipBias` (CONTENT's preset, row AV6), and cooked by `Cook_VirtualTexture` (`Zenith2/Docs/Design/ADVGFX/VTCook.md → "Cook pipeline"`); a reference scene is a parity scene document with a `pathTrace` object; a reference image is recorded, never authored ("Reference capture procedure" of PathTracer.md). Stage 7 adds `mesh.virtualized` and the VG cook.

### Editor surfaces

<!-- assumption: pending EDIT -->
The asset browser shows a kind-18 asset's extent, usage, page mips, tiles and stored bytes from the cook report, and the inspector shows its live residency (`GetResidency`) and a thumbnail from the average colour and the tail; the import preset panel exposes the two keys; the `r.debug.vt` modes are reachable from the viewport's debug menu (row AV13).
<!-- /assumption -->

### Migration hooks

None of its own: the import preset keys migrate with CONTENT's `.meta` documents, and the `pathTrace` object with RPROD's scene document schema; a change to `.zvt` bumps `uADVGFX_VT_FORMAT_VERSION`, and cooked data re-cooks ([Z2-N-CONTRACT-528], `Zenith2/Docs/Contracts.md → "Compatibility policy per kind"`).

## Failure handling

### Failure table

| Case | Behaviour | Where |
|---|---|---|
| a missing `.zvt` | ASSET's SUBSTITUTE policy loads the engine fallback VT; the material shows the usage fallback until it is resident | VTCook.md "Engine fallbacks" |
| a corrupt `.zvt` header or loaded section | `Fail` with `FLUX_VT_ERROR_CORRUPT` before activation; usage fallback | VTCook.md "Reader validation" |
| a corrupt root tile | `ReportGpuFailed(FLUX_VT_ERROR_CORRUPT_TILE)`; usage fallback | VirtualTexturing.md "Activation steps" |
| a corrupt or unreadable streamed tile | that page stays at its ancestor; counted; the VT stays resident | VirtualTexturing.md "Tile request state machine" |
| the wrong platform's cook | `FLUX_VT_ERROR_FORMAT_UNSUPPORTED` | VTCook.md "Reader validation" |
| host allocation failure | `FLUX_VT_ERROR_OUT_OF_MEMORY`; everything the step allocated released | PublicAPI.md "Flux_VirtualTexture header" |
| slots, regions or pages exhausted at activation | `SLOTS_FULL`, `PAGE_TABLE_FULL`, `POOL_FULL`; usage fallback | VirtualTexturing.md "Activation steps" |
| host cap reached at activation | `HOST_BYTES_FULL`; usage fallback; an unload returns the bytes | VirtualTexturing.md "Host memory bound" |
| tail reservation full at activation | `TAIL_BYTES_FULL`; usage fallback; an unload returns the bytes | VirtualTexturing.md "Physical cache sizing" |
| a VT handle in a pass that binds no pool | the tail texture is sampled | VirtualTexturing.md "Flux_SampleVirtual algorithm" |
| no page for a decoded tile | waits, counted; never forced | VirtualTexturing.md "Eviction policy" |
| deferred or failed frame | no page-table write lost | VirtualTexturing.md "Deferred and failed frames" |
| memory pressure | coarser requests; pools unchanged | VirtualTexturing.md "Memory accounting and pressure" |
| device loss | pools re-created, VTs reloaded | VirtualTexturing.md "Device loss recovery" |
| budget breach at configuration | `FLUX_VT_ERROR_BUDGET_EXCEEDED`; exit code 4 at startup | VirtualTexturing.md "Physical cache sizing" |
| VT disabled | loads fail with `NOT_ENABLED`; usage fallback | VirtualTexturing.md "Disabled virtual texturing" |
| tracer failures | "Path tracer failure handling" | PathTracer.md |
| a stale or missing reference | the freshness test fails, naming the input, unless the entry carries `pendingRecording` (a recorded skip); acceptance fails while any marker remains; re-record by a `needs-human` ticket | PathTracer.md "Stale reference landing protocol" |

## Test strategy

### Test catalogue index

| Group | Where |
|---|---|
| targets and gate steps | `Zenith2/Docs/Design/ADVGFX/Tests.md → "Test targets and gate steps"` |
| VT runtime | `Zenith2/Docs/Design/ADVGFX/Tests.md → "Virtual texture tests"` |
| VT cook | `Zenith2/Docs/Design/ADVGFX/Tests.md → "VT cook tests"` |
| tracer | `Zenith2/Docs/Design/ADVGFX/Tests.md → "Path tracer tests"` |
| runner | `Zenith2/Docs/Design/ADVGFX/Tests.md → "Physical-accuracy runner tests"` |
| RT and VG (Stage 7) | `Zenith2/Docs/Design/ADVGFX/RayTracing.md → "RT validation"`, `Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md → "VG validation"` |
| fixtures, harnesses, device runs | `Zenith2/Docs/Design/ADVGFX/Tests.md → "Fixtures"`, → "Benchmarks and harnesses", → "Device runs" |

### Acceptance evidence

| Stage | Evidence |
|---|---|
| S4 | every VT test of the catalogue green in the `Test` step on WARP and lavapipe; the VT parity scene in the aliasing and backend parity suites; the RTX 3070 and S24 VT runs (`needs-human`); the W-Outdoor and W-OpenWorld budget runs for `Z2-B-ADVGFX-001` … `-003` |
| S5 | the tracer, metric and reference tests green; the committed references fresh, with no `pendingRecording` marker left; LIGHT's and ENV's comparison tests in gate mode in `Test` and in acceptance mode in the Stage 5 acceptance suite on WARP and lavapipe; the VT parity scene in path parity |
| S7 | the RT and VG validation tables; the two decision rulings |

## Budget rows

### Rows ADVGFX owns

| Row | Name | Status | Proposed value | Measured by |
|---|---|---|---|---|
| `Z2-B-ADVGFX-001` | Virtual texturing physical cache and page tables | pending | 224 MiB desktop, 72 MiB S24, device bytes only (fixed pools 222,257,492 B and 72,309,076 B, GPU totals 230,646,100 B and 74,406,228 B with the tail reservation; "Physical cache sizing" of VirtualTexturing.md) | MP-SUBMEM, the VT budget runs |
| `Z2-B-ADVGFX-002` | Virtual texturing tile uploads per frame | pending | 64 desktop, 24 S24: the camera-cut peak; the default limit is half | MP-COUNTER (`Flux.VT.TilesUploaded`) |
| `Z2-B-ADVGFX-003` | Virtual texturing feedback and page-table update GPU | pending | 0.25 ms desktop, 0.40 ms S24 | MP-GPUPASS over the three VT passes |
| `Z2-B-ADVGFX-004` | Path tracer resolution cap per reference scene | pending | "Proposed per-scene caps" of PathTracer.md | MP-PATHTRACE |
| `Z2-B-ADVGFX-005` | Path tracer wall-clock cap per reference scene | pending | as above | MP-PATHTRACE |
| `Z2-B-ADVGFX-006` | Path tracer minimum samples per pixel | pending | 256 (512 and 1,024 for the noisiest scenes) | MP-PATHTRACE |
| `Z2-B-ADVGFX-007` | Path tracer convergence variance threshold | pending | 1% of the mean for 99.5% of pixels (per scene) | MP-PATHTRACE |
| `Z2-B-ADVGFX-008` | Physical-accuracy error tolerance per reference scene | pending | LIGHT's and ENV's proposals | MP-PATHTRACE |

Every row is allocated by the Stage 1 per-system budget ticket for ADVGFX, a single `needs-human` ruling with no harness half; a ticket that cites a pending row is blocked by that ticket ([Z2-N-REND-796]).

### Rows ADVGFX is measured against

| Row | For |
|---|---|
| `Z2-B-CORE-001` S24 process RSS, `Z2-B-CORE-003` Windows VRAM | the totals the VT pools sum within |
| `Z2-B-RHI-006` Upload ring hard cap, `Z2-B-GEOM-003` Streaming upload bytes per frame | tile and record uploads |
| `Z2-B-RHI-011` GPU readback pool size | feedback readbacks |
| `Z2-B-RPROD-031` Per-scene parity tolerances | the VT parity scene |
| `Z2-B-STREAM-004` Teleport-to-resident time | VT pages in the teleport measurement |
| `Z2-B-SHIP-001` Frame time p95 | every VT-enabled workload |

### Pending row needs

Rows this family needs that `Zenith2/Docs/Budgets.md` does not carry, named descriptively until the Budgets writer assigns IDs (cross-doc item X422):

| Row | Unit | Procedure | Platform | Proposed | Producer |
|---|---|---|---|---|---|
| Virtual texturing first-hop latency | frames (p95) | MP-COUNTER (`Flux.VT.FirstHopFramesP95`) | Win, S24 | 12 desktop, 16 S24 (structural floor 2F + 1: 7 frames at F = 3) | Stage 1 per-system budget ticket for ADVGFX |
| Virtual texturing finest-mip latency | frames (p95) | MP-COUNTER (`Flux.VT.FinestMipFramesP95`) | Win, S24 | 40 desktop, 56 S24 (floor 4 × (2F + 1): 28 frames at F = 3 for a 16,384 × 16,384 VT) | Stage 1 per-system budget ticket for ADVGFX |
| Virtual texturing host memory | MB | MP-SUBMEM (`Flux.VT.HostBytes`), member of MEM-WIN-RAM and MEM-S24 | Win, S24 | 50 MiB desktop, 13 MiB S24 (worst cases 51,664,384 B and 13,286,656 B, "Host memory bound" of VirtualTexturing.md) | Stage 1 per-system budget ticket for ADVGFX |
| VT branch cost in VT-free material passes (S24) | ms | MP-GPUPASS A/B of `ForwardPlusOpaque` on a VT-free scene, `render.vt.enabled` true versus false | S24 | 0.10 ms | Stage 1 per-system budget ticket for ADVGFX |
| the six RT rows of `Zenith2/Docs/Design/ADVGFX/RayTracing.md → "RT budgets"` | as listed | as listed | as listed | set by D-RT-SCOPE's measurement | Stage 7 budget ticket for ADVGFX |
| the three VG rows of `Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md → "VG budgets"` | as listed | as listed | as listed | set by the Stage 7 VG measurement | Stage 7 budget ticket for ADVGFX |

## Stage plan

### Stage 1

The Stage 1 per-system budget ticket for ADVGFX is a single `needs-human` ruling with no harness half (`Zenith2/Docs/Budgets.md → "Per-system producer shape"`): it allocates `Z2-B-ADVGFX-001` … `-008` and the pending rows of "Pending row needs" from the derived sizing of `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Physical cache sizing"` and → "Host memory bound", the latency floors of → "Request latency floor", published practice, and, for `-004` … `-008`, `Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Proposed per-scene caps"`. `Zenith_Bench_VirtualTexture` lands at Stage 4 with the VT implementation, and the Stage 4 budget run (Z2-R-ADVGFX-069) confirms the VT rows by measurement, the shape (allocated at Stage 1, confirmed by the Stage 4 measurement) of `Zenith2/Docs/Roadmap.md → "Stage 4 budget rows"`.

### Stage 4

| Order | Slice | Blocked by |
|---|---|---|
| 1 | API-only: `Flux_VirtualTexture.h`, `VirtualTexture.slang` declarations, the no-op feature functions, RPROD's renderer member declarations (ruling ENABLE-FEATURES, declare-first) | RPROD's renderer API ticket; X420 (a); decision-pending on D-SLANG (open, Stage 1) through row AV18 for `Flux_HandleWords` |
| 2 | the region allocator, the codecs, the mirror and the run-record cut (CPU, unit-tested) | 1 |
| 3 | the `.zvt` reader, limits and fuzz target; the cook step, tile assembly, compression, fallbacks, report | 1; GEOM's cook functions (row AV3 (d)) |
| 4 | the pools, `EnableVirtualTexturing`, the record buffer, the kind loader and activation | 2, 3; RHI's second view format (AV4 (a)); MOD's step-13 call (AV2) |
| 5 | sampling: `Flux_SampleVirtual`, the material input binding, the pools-bound fallback, GEOM's handle writes | 4; X420 (b), (c), (d); AV3 (b); AV18 |
| 6 | feedback: the writers, clear, readback, processing | 5; X420 (c), (e), (j) |
| 7 | residency: touch, targets, priority, tile streaming, eviction, epochs and draining, `VTPageTableUpdate` | 6; GEOM's enqueue (AV3 (c)) and `VT_TILE` floor (AV19); RHI's region destination (AV4 (b)) |
| 8 | full mode, camera cuts, deferred frames, pressure, device loss, disabled mode, diagnostics, editor surfaces | 7; X421 (a) for pressure |
| 9 | the parity scene, the S24 path, `Zenith_Bench_VirtualTexture` and the budget runs | 8 |

### Stage 5

| Order | Slice | Blocked by |
|---|---|---|
| 1 | API-only: the three tracer headers | — |
| 2 | BVH builder, scene builder, traversal | 1 |
| 3 | the wavefront pipeline on the execution substrate, materials, BSDF, lights, integrators, random numbers, accumulation | 2; RPROD's tool-graph entry and tool material slots (AV17); LIGHT's BRDF (AV9 (b)); the Stage 5 shading paths for the variant enumeration |
| 4 | metrics, tonemap and runner (test support of `ZenithFlux_Tests`), reference format, fingerprint, the stale-reference landing protocol, raster capture | 3; RPROD's capture (AV1 (g)) |
| 5 | the recording harness | 4 |
| 6 | the `needs-human` recording ticket | 5 and every scene's owning ticket |
| 7 | gate-mode comparisons (LIGHT's and ENV's tickets), the Stage 5 acceptance runs | 6 |

### Stage 7

The RT tree (RayTracing.md) and the VG tree (VirtualizedGeometry.md), each starting with its decision harness and ruling, then API slices, then implementation, all blocked by their decision's revalidation ticket. A hardware traversal for the tracer exists only if flagged proposal ADVGFX-1 is approved, as that proposal's own ticket; nothing of it is declared at Stage 5.

### API-only slices

| Slice | Stage | Unblocks |
|---|---|---|
| `Flux_VirtualTexture.h` with no-op feature functions | S4 | RPROD's path functions (the feedback calls), GEOM's material manager, LIGHT's and ENV's material passes |
| `VirtualTexture.slang` declarations and `Flux_VTBinding` | S4 | RPROD's material passes |
| the tracer headers | S5 | LIGHT's and ENV's comparison tests |
| `Zenith_RHIRayTracing.h`, `Flux_RayTracing.h`, `Flux_VirtualGeometry.h` | S7 | the RT and VG implementation tickets |

## Decisions

### Settled decisions

| Decision | Use here |
|---|---|
| Z2-D-043 | the whole classification: VT S4, path-traced reference S5, RT and VG S7, hair excluded |
| Z2-D-037 | the compute path tracer with a software BVH, validation only, in `ZenithPathTracer` |
| Z2-D-063 | the physical-accuracy role, kept separate from cross-path consistency and the analytical BRDF tests |
| Z2-D-078 | hair and groom excluded |
| Z2-D-058 | the VT pools are dynamic content reached through pools, never through tables |
| Z2-D-054, Z2-D-015 | the 8-byte handles, Slang structs and generated headers VT uses |
| Z2-D-055, Z2-D-056, Z2-D-057 | VT passes in the per-frame graph; synthesised barriers; the first-write rule for feedback buffers |
| Z2-D-025 | every tracer and VT program compiled offline |
| Z2-D-012 | budget rows and their change rule |
| Z2-D-084, Z2-D-097 | harness plus `needs-human` ruling tickets; rulings appended to `Zenith2/Docs/DecisionLog.md` in the ticket's diff |
| Z2-D-038 | `.zvt`, `.zref` and `.zvgeo` are versioned little-endian binaries written field by field |

### Open decisions

| Decision | Stage | Governs |
|---|---|---|
| D-RT-SCOPE (open, Stage 7) | 7 | which RT features ship where; ray-tracing pipelines (`Zenith2/Docs/Design/ADVGFX/RayTracing.md → "Decision D-RT-SCOPE"`); registered through X423 |
| D-VGEO-RASTER (open, Stage 7) | 7 | the software rasteriser for small triangles (`Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md → "Decision D-VGEO-RASTER"`); registered through X423 |
| D-SLANG (open, Stage 1) | 1 | the lowering of `Flux_HandleWords`, proven for the handle's second word by the probe case of row AV18 (every VT ticket that uses the accessor is decision-pending until the ruling), and of the Stage 7 acceleration-structure space |
| D-TBDR (open, Stage 5) | 5 | none of ADVGFX's passes read attachments tile-locally; the VT feedback UAV writes in the forward opaque pass are within the S24 measurement |

### Design rulings ADVGFX owns

| Ruling | Choice | Rejected | Why |
|---|---|---|---|
| AV-R1 | software page tables, no sparse resources | sparse residency, per-texture mip arrays | the baseline profiles and WARP lack sparse residency; mip arrays do not scale to 16 K |
| AV-R2 | 128-texel tiles with 4-texel borders, one physical cache with sRGB and UNORM views | 64-texel or 256-texel tiles; one cache per format | 18,496-byte tiles balance I/O count against waste; one cache keeps Rendering's single `m_xVTPhysical` |
| AV-R3 | feedback written by the four material colour passes | a separate feedback geometry pass | no extra geometry pass and no extra PSOs; the passes already shade every visible opaque pixel |
| AV-R4 | VT runs on the S24 with the mobile parameters (not excluded there) | VT desktop-only | Forward+ is the S24's path and terrain materials bind VTs on every platform; the device memory is 74,406,228 B (70.96 MiB: fixed pools 72,309,076 B plus the 2 MiB tail reservation), within the proposed 72 MiB device-only row, and the host memory (at most 13,286,656 B with the 8 MiB index cap) is the separate proposed host row; the VT branch's cost in VT-free passes is measured against its own proposed row |
| AV-R5 | VTs bound by opaque and alpha-tested materials alone | VTs in transparent and decal materials | those passes write no feedback and bind no VT pool, so a VT there would never refine; a VT handle that reaches them anyway samples its tail |
| AV-R6 | the TerrainVT scene stays ENV's; LIGHT's `PT_TerrainVT` row is asked to name it (pending AV9 (a)) | a second ADVGFX copy | ENV's own criterion (the area whose design specifies the terrain material) selects ENV; one scene, one owner (answers EN19) |
| AV-R7 | references recorded per backend, compute traversal the acceptance reference, committed references a cache of the acceptance result for gate runs (gate mode cannot converge inside the 30-minute phase cap), a stale reference landing under the `pendingRecording` protocol | one reference for both backends; hardware traversal as reference | software adapters differ in the last bits; Z2-D-037 |
| AV-R8 | fixed-size VT pools; pressure sheds requests, never memory | resizing the cache under pressure | a resize needs a transition peak exactly when memory is short |
| AV-R9 | the tracer shares `EvaluateMaterial` and `BRDF.slang` with the raster paths | an independent tracer material model | physical accuracy measures transport; the analytical BRDF tests cover the reflectance model ([Z2-N-REND-949]) |
| AV-R10 | Stage 7 RT through inline ray queries unless D-RT-SCOPE rules otherwise | ray-tracing pipelines from the start | one binding model and unchanged pass kinds |

### Flagged proposals

ADVGFX-1 (`C:/dev/Zenith2-drafts/findings/FLAGGED_PROPOSALS.md`): a Stage 7 hardware ray-query traversal for the path tracer as an accelerator of development runs, with the compute traversal kept as the only acceptance reference. Z2-D-037 settles a compute tracer and rejects hardware RT as the reference; until the human approves the proposal, `Z2-R-ADVGFX-140` stays deferred and conditional, the Stage 5 public header declares nothing of it, and the design's compute traversal is complete without it.

## Requirement index

The map from every requirement to the design headings that specify it (`Zenith2/Docs/Requirements/ADVGFX.md`):

| Requirement | Title | Stage | Design headings |
|---|---|---|---|
| Z2-R-ADVGFX-001 | Software virtual texturing without sparse resources | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Virtual texturing model"; Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Why software page tables" |
| Z2-R-ADVGFX-002 | Virtual texture shape rules | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Virtual texture shape rules"; Zenith2/Docs/Design/ADVGFX/VTCook.md → "Cook errors" |
| Z2-R-ADVGFX-003 | Physical cache texture | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Physical cache texture"; Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Physical page addressing" |
| Z2-R-ADVGFX-004 | Physical cache sized once within its budget row | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Physical cache sizing" |
| Z2-R-ADVGFX-005 | Page-table pool texture | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Page-table texture" |
| Z2-R-ADVGFX-006 | Page-table entry encoding | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Page-table entry encoding"; Zenith2/Docs/Design/ADVGFX/PublicAPI.md → "Flux_VirtualTexture header" |
| Z2-R-ADVGFX-007 | Every entry names a resident page | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Page-table entry encoding"; Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Map and unmap algorithms" |
| Z2-R-ADVGFX-008 | Region allocation by a 2D buddy allocator | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Region allocation" |
| Z2-R-ADVGFX-009 | CPU mirror is the authority for the page table | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "CPU mirror"; Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Deferred and failed frames" |
| Z2-R-ADVGFX-010 | Virtual texture records | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Virtual texture records" |
| Z2-R-ADVGFX-011 | Material handle encoding | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Material handle encoding" |
| Z2-R-ADVGFX-012 | Usage fallback until resident | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Material handle encoding"; Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Activation steps" |
| Z2-R-ADVGFX-013 | Flux_SampleVirtual algorithm | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Flux_SampleVirtual algorithm"; Zenith2/Docs/Design/ADVGFX/PublicAPI.md → "VirtualTexture shader module" |
| Z2-R-ADVGFX-014 | Wrap and clamp addressing | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Address modes"; Zenith2/Docs/Design/ADVGFX/VTCook.md → "Tile assembly" |
| Z2-R-ADVGFX-015 | Tail sampling and the root-to-tail blend | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Tail sampling" |
| Z2-R-ADVGFX-016 | Filter footprint stays inside the page | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Anisotropy cap and border" |
| Z2-R-ADVGFX-017 | VT filtering tolerance against streamed textures | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Filtering tolerance against streamed textures" |
| Z2-R-ADVGFX-018 | Material input carries the pass's VT binding | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Material input binding" |
| Z2-R-ADVGFX-019 | Every material-evaluating pass binds the VT pools | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Material input binding"; Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Feedback fields of material passes" |
| Z2-R-ADVGFX-020 | VT bound only by opaque and alpha-tested materials | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Virtual texture shape rules" |
| Z2-R-ADVGFX-021 | Feedback request encoding | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Feedback request encoding" |
| Z2-R-ADVGFX-022 | Feedback written by the material colour passes | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Feedback writers"; Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Feedback fields of material passes" |
| Z2-R-ADVGFX-023 | One writer per feedback texel per frame | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Pixel selection and reservoir" |
| Z2-R-ADVGFX-024 | Deterministic reservoir choice among a pixel's requests | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Pixel selection and reservoir" |
| Z2-R-ADVGFX-025 | Feedback buffers, clear and readback | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Feedback buffers and readback"; Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "VTFeedbackClear"; Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "VTFeedbackReadback" |
| Z2-R-ADVGFX-026 | Views that write feedback | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Views that write feedback" |
| Z2-R-ADVGFX-027 | Deterministic feedback processing | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Sort and unique" |
| Z2-R-ADVGFX-028 | Request validation and slot quarantine | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Request validation"; Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Unload and slot quarantine" |
| Z2-R-ADVGFX-029 | Touch rule | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Touch rule" |
| Z2-R-ADVGFX-030 | Refinement step | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Load target and refinement step" |
| Z2-R-ADVGFX-031 | Load priority order | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Priority order" |
| Z2-R-ADVGFX-032 | Physical page state machine | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Physical page state machine" |
| Z2-R-ADVGFX-033 | Tile request state machine | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Tile request state machine" |
| Z2-R-ADVGFX-034 | Eviction policy | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Eviction policy" |
| Z2-R-ADVGFX-035 | Draining rule for evicted and unloaded pages | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Draining rule" |
| Z2-R-ADVGFX-036 | In-flight limits | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "In-flight limits" |
| Z2-R-ADVGFX-037 | Tile reads through the asset reader | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Tile request state machine"; Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Tile I/O and decode" |
| Z2-R-ADVGFX-038 | Tile verification and decode in sub-jobs | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Tile I/O and decode" |
| Z2-R-ADVGFX-039 | Read and decode failures | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Tile request state machine"; Zenith2/Docs/Design/ADVGFX.md → "Failure table" |
| Z2-R-ADVGFX-040 | Tile uploads in the VT_TILE class | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Tile upload" |
| Z2-R-ADVGFX-041 | Map after upload completion | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Physical page state machine"; Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Map and unmap algorithms" |
| Z2-R-ADVGFX-042 | Map and unmap algorithms | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Map and unmap algorithms" |
| Z2-R-ADVGFX-043 | Dirty rows and run records | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Dirty rows and run records" |
| Z2-R-ADVGFX-044 | VTPageTableUpdate pass | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "VTPageTableUpdate pass"; Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "VTPageTableUpdate pass table"; Zenith2/Docs/Design/ADVGFX.md → "Common prologue placement" |
| Z2-R-ADVGFX-045 | Frame-level flow on the build node | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Frame-level flow" |
| Z2-R-ADVGFX-046 | Deferred and failed frames keep the page table consistent | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Deferred and failed frames" |
| Z2-R-ADVGFX-047 | Kind loader for virtual textures | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Activation steps" |
| Z2-R-ADVGFX-048 | Activation reaches residency | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Activation steps" |
| Z2-R-ADVGFX-049 | Activation failures release and fall back | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Activation steps"; Zenith2/Docs/Design/ADVGFX.md → "Failure table" |
| Z2-R-ADVGFX-050 | Unload releases after retirement | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Activation steps"; Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Unload and slot quarantine" |
| Z2-R-ADVGFX-051 | 1,000 load and unload cycles | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Unload and slot quarantine" |
| Z2-R-ADVGFX-052 | Full residency mode | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Residency modes" |
| Z2-R-ADVGFX-053 | VT parity scenes in full mode | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Residency modes"; Zenith2/Docs/Design/ADVGFX/Tests.md → "Test targets and gate steps" |
| Z2-R-ADVGFX-054 | Streaming reaches the full-mode image | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Residency modes"; Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Frame-level flow" |
| Z2-R-ADVGFX-055 | Camera cut boost | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Camera cuts" |
| Z2-R-ADVGFX-056 | Memory accounting | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Memory accounting and pressure" |
| Z2-R-ADVGFX-057 | Pressure shedding coarsens requests | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Memory accounting and pressure" |
| Z2-R-ADVGFX-058 | Device loss rebuilds virtual texturing | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Device loss recovery" |
| Z2-R-ADVGFX-059 | Disabled virtual texturing | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Disabled virtual texturing" |
| Z2-R-ADVGFX-060 | Headless validate-only loading | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Activation steps" |
| Z2-R-ADVGFX-061 | EnableVirtualTexturing creation and unwinding | S4 | Zenith2/Docs/Design/ADVGFX/PublicAPI.md → "Flux_VirtualTexture header"; Zenith2/Docs/Design/ADVGFX.md → "ZenithRuntime call sites" |
| Z2-R-ADVGFX-062 | VT configuration keys | S4 | Zenith2/Docs/Design/ADVGFX.md → "Configuration keys" |
| Z2-R-ADVGFX-063 | VT public header | S4 | Zenith2/Docs/Design/ADVGFX/PublicAPI.md → "Flux_VirtualTexture header" |
| Z2-R-ADVGFX-064 | VT shader module | S4 | Zenith2/Docs/Design/ADVGFX/PublicAPI.md → "VirtualTexture shader module" |
| Z2-R-ADVGFX-065 | VT counters | S4 | Zenith2/Docs/Design/ADVGFX.md → "Counters" |
| Z2-R-ADVGFX-066 | VT developer tools | S4 | Zenith2/Docs/Design/ADVGFX.md → "Console commands"; Zenith2/Docs/Design/ADVGFX.md → "Debug views"; Zenith2/Docs/Design/ADVGFX.md → "Frame dump section"; Zenith2/Docs/Design/ADVGFX.md → "On-screen profiler rows" |
| Z2-R-ADVGFX-067 | Editor surfaces for virtual textures | S4 | Zenith2/Docs/Design/ADVGFX.md → "Editor surfaces" |
| Z2-R-ADVGFX-068 | S24 virtual texturing | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Mobile parameters"; Zenith2/Docs/Design/ADVGFX/Tests.md → "Device runs" |
| Z2-R-ADVGFX-069 | VT budget measurement | S4 | Zenith2/Docs/Design/ADVGFX.md → "Rows ADVGFX owns"; Zenith2/Docs/Design/ADVGFX/Tests.md → "Benchmarks and harnesses" |
| Z2-R-ADVGFX-070 | VT first-hop latency | S4 | Zenith2/Docs/Design/ADVGFX.md → "Pending row needs" |
| Z2-R-ADVGFX-071 | Teleport residency includes VT | S4 | Zenith2/Docs/Design/ADVGFX.md → "Telemetry" |
| Z2-R-ADVGFX-072 | VT budget harness | S4 | Zenith2/Docs/Design/ADVGFX/Tests.md → "Benchmarks and harnesses" |
| Z2-R-ADVGFX-073 | VT budget rows allocated | S1 | Zenith2/Docs/Design/ADVGFX.md → "Rows ADVGFX owns"; Zenith2/Docs/Design/ADVGFX.md → "Stage 1" |
| Z2-R-ADVGFX-074 | Import preset flag selects a virtual texture | S4 | Zenith2/Docs/Design/ADVGFX/VTCook.md → "Import preset flag" |
| Z2-R-ADVGFX-075 | Cook pipeline steps | S4 | Zenith2/Docs/Design/ADVGFX/VTCook.md → "Cook pipeline" |
| Z2-R-ADVGFX-076 | Tile assembly from encoded mips | S4 | Zenith2/Docs/Design/ADVGFX/VTCook.md → "Tile assembly" |
| Z2-R-ADVGFX-077 | Per-tile compression | S4 | Zenith2/Docs/Design/ADVGFX/VTCook.md → "Tile compression" |
| Z2-R-ADVGFX-078 | Deterministic VT cook | S4 | Zenith2/Docs/Design/ADVGFX/VTCook.md → "Determinism and derived-data keys" |
| Z2-R-ADVGFX-079 | Cooked virtual texture format | S4 | Zenith2/Docs/Design/ADVGFX/VTCook.md → "The cooked virtual texture format"; Zenith2/Docs/Design/ADVGFX/VTCook.md → "Info section"; Zenith2/Docs/Design/ADVGFX/VTCook.md → "Tile index section"; Zenith2/Docs/Design/ADVGFX/VTCook.md → "Tail section"; Zenith2/Docs/Design/ADVGFX/VTCook.md → "Root section"; Zenith2/Docs/Design/ADVGFX/VTCook.md → "Tile data section" |
| Z2-R-ADVGFX-080 | VT reader validation | S4 | Zenith2/Docs/Design/ADVGFX/VTCook.md → "Reader validation" |
| Z2-R-ADVGFX-081 | VT parser limits | S4 | Zenith2/Docs/Design/ADVGFX/VTCook.md → "Parser limits" |
| Z2-R-ADVGFX-082 | VT engine fallbacks | S4 | Zenith2/Docs/Design/ADVGFX/VTCook.md → "Engine fallbacks" |
| Z2-R-ADVGFX-083 | VT cook report entry | S4 | Zenith2/Docs/Design/ADVGFX/VTCook.md → "Cook report entry" |
| Z2-R-ADVGFX-084 | VT cook errors | S4 | Zenith2/Docs/Design/ADVGFX/VTCook.md → "Cook errors" |
| Z2-R-ADVGFX-085 | Platform formats of virtual textures | S4 | Zenith2/Docs/Design/ADVGFX/VTCook.md → "Info section"; Zenith2/Docs/Design/ADVGFX/VTCook.md → "Reader validation" |
| Z2-R-ADVGFX-086 | Path tracer role and boundaries | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Role and boundaries" |
| Z2-R-ADVGFX-087 | ZenithPathTracer library and link closure | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Library targets and programs"; Zenith2/Docs/Design/ADVGFX.md → "ZenithPathTracer files" |
| Z2-R-ADVGFX-088 | Tracer programs compiled offline | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Library targets and programs" |
| Z2-R-ADVGFX-089 | Path tracer public API | S5 | Zenith2/Docs/Design/ADVGFX/PublicAPI.md → "Flux_PathTracer header"; Zenith2/Docs/Design/ADVGFX/PublicAPI.md → "Flux_PathTraceReference header"; Zenith2/Docs/Design/ADVGFX/PublicAPI.md → "Flux_PhysicalAccuracyRunner header" |
| Z2-R-ADVGFX-090 | Reference scene documents | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Reference scenes"; Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Scene-document helper" |
| Z2-R-ADVGFX-091 | Scene builder | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Scene builder" |
| Z2-R-ADVGFX-092 | Terrain in reference scenes | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Scene builder" |
| Z2-R-ADVGFX-093 | Deterministic binned SAH BVH | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "BVH"; Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Builder"; Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Node and triangle layout" |
| Z2-R-ADVGFX-094 | Watertight traversal | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Traversal" |
| Z2-R-ADVGFX-095 | Wavefront pipeline | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Wavefront pipeline"; Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Path state"; Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Kernels in order" |
| Z2-R-ADVGFX-096 | Materials evaluated with shared code and analytic gradients | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Materials and gradients" |
| Z2-R-ADVGFX-097 | Virtual textures in the tracer | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Materials and gradients" |
| Z2-R-ADVGFX-098 | BSDF evaluation and sampling | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "BSDF evaluation and sampling" |
| Z2-R-ADVGFX-099 | Lights and the environment | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Lights and the environment" |
| Z2-R-ADVGFX-100 | Four integrators | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Integrators" |
| Z2-R-ADVGFX-101 | Counter-based random numbers | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Random numbers" |
| Z2-R-ADVGFX-102 | Accumulation and convergence criterion | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Accumulation and convergence" |
| Z2-R-ADVGFX-103 | Wall-clock and resolution caps | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Accumulation and convergence"; Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Proposed per-scene caps" |
| Z2-R-ADVGFX-104 | Tracer determinism | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Determinism" |
| Z2-R-ADVGFX-105 | Self-check scenes | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Gate and acceptance runs" |
| Z2-R-ADVGFX-106 | Raster capture for comparison | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Raster capture" |
| Z2-R-ADVGFX-107 | Comparison tonemap | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Comparison tonemap" |
| Z2-R-ADVGFX-108 | Error metrics | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Error metrics" |
| Z2-R-ADVGFX-109 | Every available raster path compared | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Reference scenes"; Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Gate and acceptance runs" |
| Z2-R-ADVGFX-110 | Reference file format | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Reference files" |
| Z2-R-ADVGFX-111 | Reference manifest and fingerprint | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Reference manifest and fingerprint" |
| Z2-R-ADVGFX-112 | Reference recording harness | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Reference capture procedure"; Zenith2/Docs/Design/ADVGFX/Tests.md → "Benchmarks and harnesses" |
| Z2-R-ADVGFX-113 | References recorded by a needs-human ticket | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Reference capture procedure" |
| Z2-R-ADVGFX-114 | Reference freshness check | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Reference capture procedure" |
| Z2-R-ADVGFX-115 | Gate-mode comparison | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Gate and acceptance runs" |
| Z2-R-ADVGFX-116 | Acceptance-mode in-run convergence | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Gate and acceptance runs" |
| Z2-R-ADVGFX-117 | Reports and failure artefacts | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Reports and artefacts" |
| Z2-R-ADVGFX-118 | Path tracer failure handling | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Path tracer failure handling" |
| Z2-R-ADVGFX-119 | Per-scene caps and tolerances proposed | S1 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Proposed per-scene caps" |
| Z2-R-ADVGFX-120 | Compute traversal is the reference | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Compute and hardware traversal" |
| Z2-R-ADVGFX-121 | Approximation-error references | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Integrators"; Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Reference scenes" |
| Z2-R-ADVGFX-122 | RT classification and scope decision | S7 | Zenith2/Docs/Design/ADVGFX/RayTracing.md → "Classification and gating"; Zenith2/Docs/Design/ADVGFX/RayTracing.md → "Decision D-RT-SCOPE" |
| Z2-R-ADVGFX-123 | D-RT-SCOPE harness and ruling | S7 | Zenith2/Docs/Design/ADVGFX/RayTracing.md → "Decision D-RT-SCOPE" |
| Z2-R-ADVGFX-124 | RT capability query | S7 | Zenith2/Docs/Design/ADVGFX/RayTracing.md → "Device capability"; Zenith2/Docs/Design/ADVGFX/PublicAPI.md → "Zenith_RHIRayTracing header" |
| Z2-R-ADVGFX-125 | Acceleration-structure resources | S7 | Zenith2/Docs/Design/ADVGFX/RayTracing.md → "Acceleration-structure resources" |
| Z2-R-ADVGFX-126 | Acceleration-structure descriptor space | S7 | Zenith2/Docs/Design/ADVGFX/RayTracing.md → "Descriptor index space for acceleration structures" |
| Z2-R-ADVGFX-127 | Acceleration-structure barriers are synthesised | S7 | Zenith2/Docs/Design/ADVGFX/RayTracing.md → "Builds and barrier states" |
| Z2-R-ADVGFX-128 | Static bottom-level structures | S7 | Zenith2/Docs/Design/ADVGFX/RayTracing.md → "Static meshes" |
| Z2-R-ADVGFX-129 | Skinned and procedural bottom-level structures | S7 | Zenith2/Docs/Design/ADVGFX/RayTracing.md → "Skinned and procedural geometry" |
| Z2-R-ADVGFX-130 | Top-level structure from the GPU scene | S7 | Zenith2/Docs/Design/ADVGFX/RayTracing.md → "Top-level structure" |
| Z2-R-ADVGFX-131 | Ray queries with alpha-tested candidates | S7 | Zenith2/Docs/Design/ADVGFX/RayTracing.md → "Ray queries in passes" |
| Z2-R-ADVGFX-132 | Hit shading | S7 | Zenith2/Docs/Design/ADVGFX/RayTracing.md → "Hit shading" |
| Z2-R-ADVGFX-133 | RT shadows | S7 | Zenith2/Docs/Design/ADVGFX/RayTracing.md → "RT shadows" |
| Z2-R-ADVGFX-134 | RT reflections | S7 | Zenith2/Docs/Design/ADVGFX/RayTracing.md → "RT reflections" |
| Z2-R-ADVGFX-135 | RT ambient occlusion | S7 | Zenith2/Docs/Design/ADVGFX/RayTracing.md → "RT ambient occlusion" |
| Z2-R-ADVGFX-136 | RT diffuse GI | S7 | Zenith2/Docs/Design/ADVGFX/RayTracing.md → "RT diffuse GI" |
| Z2-R-ADVGFX-137 | RT capability-matrix rows and fallbacks | S7 | Zenith2/Docs/Design/ADVGFX/RayTracing.md → "RT capability matrix rows and fallbacks" |
| Z2-R-ADVGFX-138 | RT validation | S7 | Zenith2/Docs/Design/ADVGFX/RayTracing.md → "RT validation" |
| Z2-R-ADVGFX-139 | RT budgets and device runs | S7 | Zenith2/Docs/Design/ADVGFX/RayTracing.md → "RT budgets"; Zenith2/Docs/Design/ADVGFX/RayTracing.md → "RT validation" |
| Z2-R-ADVGFX-140 | Hardware traversal for the path tracer | S7 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Compute and hardware traversal" |
| Z2-R-ADVGFX-141 | VG classification and paths | S7 | Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md → "VG classification and gating"; Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md → "VG path integration" |
| Z2-R-ADVGFX-142 | VG asset kind and format | S7 | Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md → "VG cooked format" |
| Z2-R-ADVGFX-143 | Cluster DAG cook | S7 | Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md → "Cluster DAG cook" |
| Z2-R-ADVGFX-144 | Cluster page streaming | S7 | Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md → "Cluster page streaming" |
| Z2-R-ADVGFX-145 | Crack-free LOD selection | S7 | Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md → "LOD selection" |
| Z2-R-ADVGFX-146 | VG culling | S7 | Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md → "VG culling" |
| Z2-R-ADVGFX-147 | VG rasterisation routes | S7 | Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md → "VG rasterisation" |
| Z2-R-ADVGFX-148 | D-VGEO-RASTER harness and ruling | S7 | Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md → "Decision D-VGEO-RASTER" |
| Z2-R-ADVGFX-149 | VG validation | S7 | Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md → "VG validation" |
| Z2-R-ADVGFX-150 | VG budgets and device runs | S7 | Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md → "VG budgets"; Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md → "VG validation" |
| Z2-R-ADVGFX-151 | Hair and groom excluded | S4 | Zenith2/Docs/Design/ADVGFX.md → "Hair and groom" |
| Z2-R-ADVGFX-152 | No Luau surface | S4 | Zenith2/Docs/Design/ADVGFX.md → "Luau errors and asserts" |
| Z2-R-ADVGFX-153 | Presentation-only virtual texturing | S4 | Zenith2/Docs/Design/ADVGFX.md → "Cancellation and shutdown" |
| Z2-R-ADVGFX-154 | Eviction epochs are assigned per eviction | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Eviction epochs" |
| Z2-R-ADVGFX-155 | Unloaded pages drain at the first submitted frame after the unload drain | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Physical page state machine"; Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Activation steps" |
| Z2-R-ADVGFX-156 | Unbound pools fall to the tail | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Flux_SampleVirtual algorithm"; Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Feedback writers" |
| Z2-R-ADVGFX-157 | Invocation state visible to the VirtualTexture module alone | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Material input binding"; Zenith2/Docs/Design/ADVGFX/PublicAPI.md → "VirtualTexture shader module" |
| Z2-R-ADVGFX-158 | VT_TILE upload floor | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Tile upload" |
| Z2-R-ADVGFX-159 | VT finest-mip latency | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Request latency floor"; Zenith2/Docs/Design/ADVGFX.md → "Pending row needs" |
| Z2-R-ADVGFX-160 | Handle second word readable under D-SLANG | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Material handle encoding" |
| Z2-R-ADVGFX-161 | VT branch cost on the S24 | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Cost of the VT branch"; Zenith2/Docs/Design/ADVGFX.md → "Pending row needs" |
| Z2-R-ADVGFX-162 | Stale reference landing protocol | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Stale reference landing protocol" |
| Z2-R-ADVGFX-163 | Acceptance refuses pending recordings | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Stale reference landing protocol" |
| Z2-R-ADVGFX-164 | Reference size budget and LFS tracking | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Reference size budget" |
| Z2-R-ADVGFX-165 | VT host memory bound | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Host memory bound"; Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Memory accounting and pressure" |
| Z2-R-ADVGFX-166 | Downsampled reference standard error | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Gate and acceptance runs" |
| Z2-R-ADVGFX-167 | Tracer execution substrate | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Execution substrate" |
| Z2-R-ADVGFX-168 | Synthetic block-grid cook test path | S4 | Zenith2/Docs/Design/ADVGFX/VTCook.md → "Synthetic block-grid test path" |
| Z2-R-ADVGFX-169 | Test-time VT fixtures fit the test timeout | S4 | Zenith2/Docs/Design/ADVGFX/Tests.md → "Fixtures"; Zenith2/Docs/Design/ADVGFX/Tests.md → "Test targets and gate steps" |
| Z2-R-ADVGFX-170 | Physical-accuracy metrics stay in a test target | S5 | Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Library targets and programs" |
| Z2-R-ADVGFX-171 | Full-precision positions in bottom-level structures | S7 | Zenith2/Docs/Design/ADVGFX/RayTracing.md → "Static meshes" |
| Z2-R-ADVGFX-172 | Dirty rows carry the minimum unapplied epoch and cut in epoch order | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Eviction epochs"; Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Dirty rows and run records" |
| Z2-R-ADVGFX-173 | Applied epoch publication and page exit | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Eviction epochs"; Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Draining rule" |
| Z2-R-ADVGFX-174 | Non-writing invocations record nothing | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Pixel selection and reservoir" |
| Z2-R-ADVGFX-175 | Tail textures inside the device row | S4 | Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Physical cache sizing"; Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Activation steps" |

## Cross-area rows

The rows ADVGFX appended, with their state at revision 1: AV1 to AV15 and AV17 to AV19 of `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` (summarised in "Assumed interfaces pending other areas"; AV16 is withdrawn), the cross-doc items X420 (Rendering family: (a) `Flux_HandleWords` and the handle encoding, (b) the material input binding and the [Z2-N-REND-242] amendment, (c) `Flux_SampleMaterial`'s VT branch and the feedback writers' fields, (d) the pool fields, (e) the pass tables and frame-table rows, (f) the capability-matrix row, (g) the Pools row with the record buffer, (h) the shader module table, (i) the D-SLANG probe case, (j) `Flux_ViewTargets::m_xVTFeedback` and the per-view feedback calls), X421 (Architecture: (a) the shedding row, (b) the parser limits, (c) the ZenithPathTracer and ZenithFlux dependency edges), X422 (Budgets: the pending rows and the notes on `-005`, `-007` and `-008`) and X423 (DecisionLog: the two open decisions with their consumers blocked and nouns); X424 is withdrawn (ruling MISUSE-BLOCKS-NEW-AREAS-3); and the flagged proposal ADVGFX-1.
