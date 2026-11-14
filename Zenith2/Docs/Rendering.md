# Zenith2 rendering: Flux

This document family specifies Flux, the Zenith2 renderer, together with the render hardware interface below it (ZenithRHI and its two backends), the offline shader toolchain, the vertex data format and the rendering-production machinery (culling, views, PSO preparation, the GPU performance rules). The entry file carries every top-level contract; the section files listed in "Document family index" carry the full detail. Normative statements are tagged `[Z2-N-REND-NNN]`; the numbering is unique across the entry file and every section file.

## Purpose and scope

### What this document covers

| Area code | What of that area this document specifies | Detailed design doc (area phase) |
|---|---|---|
| RHI | GPU baseline, capability negotiation, binding model, GPU lifetime and synchronisation, queues, validation, recovery, graph inspection and captures | `Zenith2/Docs/Design/RHI.md` |
| GEOM | vertex data format, the shared CPU/GPU codec, mesh splitting, unified pools, material model and permutations, sampler policy | `Zenith2/Docs/Design/GEOM.md` |
| LIGHT | the lighting passes of every shading path, shadows, fog, AO, SSR, TAA/upscaling, exposure, post-processing, colour output | `Zenith2/Docs/Design/LIGHT.md` |
| RPROD | render graph, barrier synthesis, transient aliasing, render views, two-phase HiZ culling, indirect draw compaction, offline shader compilation, permutation limits, PSO precaching and the missing-PSO fallback, frame pacing, per-pass timestamps, quality tiers, the full frame of every path, the GPU performance rules and lints | `Zenith2/Docs/Design/RPROD.md` |
| ADVGFX | the classification of ray tracing, virtualized geometry, virtual texturing, hair and the path-traced reference, and the hooks this document reserves for them | `Zenith2/Docs/Design/ADVGFX.md` |

- [Z2-N-REND-001] Where a per-area design document under `Zenith2/Docs/Design/` and this document family disagree, this document family wins and the design document is corrected in the same diff that discovers the disagreement.

### Document family index

| File | Sections of this skeleton it details | Its `##` headings |
|---|---|---|
| `Zenith2/Docs/Rendering.md` | all (entry file) | the 27 skeleton headings |
| `Zenith2/Docs/Rendering/ShaderSystem.md` | Shader system and offline compilation; Pipeline state objects | Shader source layout · The ZenithShaderCompiler tool · Slang authoring vocabulary · Generated parameter headers · Reflection JSON · Shader blob container · Permutations · Shader lints · Watch mode and editor reload · Shared GPU structs · PSO keys · PSO precache sets · PSO creation schedule · Persistent pipeline caches · Missing-PSO fallback · PSO counters and tests |
| `Zenith2/Docs/Rendering/RenderGraph.md` | Render graph; Barrier synthesis; Transient aliasing | Graph lifecycle · Graph public API · Resource declaration and versioning · Pass declaration · Pass contexts · Graph compile · Pass merging · GPU timestamps · Frame dump · Barrier state machine · Barrier emission · Aliasing placement · Aliasing toggle and first-write rule · Graph errors · Graph tests |
| `Zenith2/Docs/Rendering/GpuResources.md` | GPU lifetime and synchronisation; GPU scene tables and pools; Recovery | Queues and timelines · Retirement · Frames in flight · Upload ring · Readbacks · Tracked resource kinds · GPU memory rules · GPU scene tables · Scatter upload · Table growth · Persistent resources · Pools · Texture streaming swap · GPU scene tests · Swapchain recreation · Device loss · Pipeline cache invalidation |
| `Zenith2/Docs/Rendering/Culling.md` | Hierarchical-Z occlusion culling | Pyramid format · Pyramid construction · Occlusion test · Two-phase culling · Indirect draw compaction · Meshlet culling · Shadow view culling · Culling diagnostics · Culling tests · Transparent commands |
| `Zenith2/Docs/Rendering/FrameAnatomy.md` | the six Frame anatomy sections | Reading the frame tables · Common prologue · Per-view prologue · Early-Z ordering · Forward+ frame · Forward frame · Deferred frame · Visibility buffer frame · Common epilogue · Async compute placement · Environment frame slots |
| `Zenith2/Docs/Rendering/PerPass/Common.md` | Per-pass specifications (passes shared by several paths) | Conventions · Prologue passes · Shadow passes · Culling and HiZ passes · Shared lighting passes · Shared late passes · Epilogue passes |
| `Zenith2/Docs/Rendering/PerPass/ForwardPlus.md` | Per-pass specifications (Forward+) | Conventions · Forward+ passes |
| `Zenith2/Docs/Rendering/PerPass/Forward.md` | Per-pass specifications (Forward) | Conventions · Forward passes |
| `Zenith2/Docs/Rendering/PerPass/Deferred.md` | Per-pass specifications (Deferred) | Conventions · Deferred passes |
| `Zenith2/Docs/Rendering/PerPass/VisibilityBuffer.md` | Per-pass specifications (Visibility buffer) | Conventions · Visibility buffer passes |
| `Zenith2/Docs/Rendering/VertexData.md` | Vertex data format | Vertex declaration in Slang · Generated layout tables · Default stream layout · QTangent codec · Octahedral codec · Half-float codec · Precision checks and fallbacks · Index buffers and mesh splitting · Unified stream pools · Buffer usage unions · Skinned vertex pool · Codec tests · D-VTX-TANGENTLESS harness |

- [Z2-N-REND-002] Every section file in the table above carries the same normative grammar, the same `REND` tag family and the same heading-stability rules as this entry file.

### Libraries

| Library (CMake target) | Kind | Depends on | Owns |
|---|---|---|---|
| `ZenithRHI` | API library | ZenithCore, ZenithProfiling, ZenithPlatform | the neutral device, queue, command-list, resource and descriptor interfaces; `Zenith_RHICapabilities`; no factory |
| `ZenithRHI_Vulkan` | backend library | ZenithRHI; volk, Vulkan-Profiles, VMA (in `Internal/`) | `Zenith_CreateRHI_Vulkan` |
| `ZenithRHI_D3D12` | backend library, Windows | ZenithRHI; D3D12 Agility SDK, D3D12MA (in `Internal/`) | `Zenith_CreateRHI_D3D12` |
| `ZenithVertexFormat` | leaf library | ZenithCore | the CPU vertex codec and the generated layout-table types (`Zenith_VertexFormat.h`) |
| `ZenithShadersGenerated` | `KIND GENERATED` library (an INTERFACE target), layer 4 | ZenithCore, ZenithVertexFormat, ZenithRHI | the offline shader-compile commands of its `SHADERS` option and the generated `<Name>.generated.h` headers (see `Zenith2/Docs/Architecture.md → "Build system and presets"`) |
| `ZenithShaderCompiler` | host tool executable (`Zenith2/Tools/ZenithShaderCompiler/`) | ZenithCore, ZenithVertexFormat; Slang, DXC | offline shader compilation, reflection, header generation, shader lints |
| `ZenithPathTracer` | tool-side static library, placed as `Zenith2/Docs/Architecture.md → "Repository layout"` lists it; `ZENITH_SUPPORTED` = `win-debug`, `win-release`, `win-tools`, `win-asan` | ZenithCore, ZenithRHI, ZenithFlux, ZenithShadersGenerated (as `Zenith2/Docs/Architecture.md → "Library dependency graph"` lists them) | the compute path tracer and its CPU BVH builder, linked into `ZenithFlux_Tests` through `zenith_add_library(ZenithFlux … TEST_DEPENDS ZenithPathTracer)` (`Zenith2/Docs/Architecture.md → "Build system and presets"`) and into `ZenithPathTracer_Tests`, and never into a Player or runtime target |

`ZenithFlux` is a layer-5 engine library whose dependency set is owned by `Zenith2/Docs/Architecture.md → "Library dependency graph"`: ZenithCore, ZenithGraph, ZenithJobs, ZenithProfiling, ZenithRHI, ZenithVertexFormat, ZenithAssets and ZenithShadersGenerated. Within this family, [Z2-N-REND-202] and [Z2-N-REND-206] are why it depends on ZenithJobs, and [Z2-N-REND-549] is why it depends on ZenithAssets; an edge to ZenithAssets is not an edge to ZenithScene.

```cpp
// ZenithRHI public root header Zenith_RHI.h (factories live in the backend libraries)
Zenith_Result<Zenith_Owned<Zenith_RHIDevice>, Zenith_RHIError> Zenith_CreateRHI_Vulkan(const Zenith_RHIDeviceDesc& xDesc);   // ZenithRHI_Vulkan
Zenith_Result<Zenith_Owned<Zenith_RHIDevice>, Zenith_RHIError> Zenith_CreateRHI_D3D12(const Zenith_RHIDeviceDesc& xDesc);    // ZenithRHI_D3D12
```

<!-- elaboration -->
- [Z2-N-REND-003] `ZenithShadersGenerated` owns every generated shader header and the shader-compile commands that produce them through its `SHADERS` option (`Zenith2/Docs/Architecture.md → "Build system and presets"`); generated headers use only ZenithCore, ZenithRHI (`Zenith_RHIShaderInterface.h`) and ZenithVertexFormat types plus the structs they generate themselves, `ZenithFlux` and every module that adds passes (such as `Zenith2D`) declares a direct dependency on it, and libgraph attributes every generated header to it.
- [Z2-N-REND-004] No runtime target links Slang or DXC; the link-surface stage of the Libgraph gate step proves it by finding no Slang or DXC symbol in any runtime target (Z2-D-025, Z2-D-042).
- [Z2-N-REND-005] Flux includes no ZenithScene or ZenithECS header; its whole view of the simulated world is the `Flux_RenderWorld` hand-off below, and libgraph's used-but-undeclared-edge check enforces the absence of the edge (see `Zenith2/Docs/Architecture.md → "Library dependency graph"`).
- [Z2-N-REND-006] Only `Zenith2/Engine/ZenithFlux/Internal/Graph/` may include the RHI barrier header `ZenithRHI/Zenith_RHIBarrier.h` or call any `Zenith_RHICommandList` barrier, layout-transition, queue-ownership or aliasing-barrier function; the ForbiddenCheck gate step fails any other use (Z2-D-056).

### The render world hand-off

`Flux_RenderWorld`, its per-world sections and `Flux_RenderWorldWriter` are ZenithFlux types defined here; `Zenith2/Docs/Architecture.md → "CPU frame graph and scheduler"` specifies what the CPU frame's extraction produces into them and when. Render extraction (frame phase 12, see `Zenith2/Docs/Architecture.md → "Frame phases and authority"`) snapshots every render-relevant value into an immutable `Flux_RenderWorld` for frame N. Flux builds, records and submits frame N from that snapshot while simulation N+1 runs (Z2-D-050).

<!-- elaboration -->
```cpp
struct Flux_RenderWorldSection        // one per extracted world, in ascending world-tag order
{
	uint32 m_uWorldTag;                                               // the world's stable tag
	Zenith_Span<const Flux_RenderInstanceDelta> m_axInstanceDeltas;    // add, update, remove; keyed by Flux_RenderInstanceKey
	Zenith_Span<const Flux_RenderLightDelta> m_axLightDeltas;
	Zenith_Span<const Flux_RenderDecal> m_axDecals;
	Zenith_Span<const Flux_RenderSkinnedPose> m_axSkinnedPoses;        // joint palettes + morph weights, one per skinned instance
	Zenith_Span<const Flux_RenderEmitterCommand> m_axEmitterCommands;  // GPU particle spawn/kill/parameter commands
	Zenith_Span<const Flux_RenderParticleCollider> m_axParticleColliders;   // VFX's element type (Flux_Particles.h)
	Zenith_Span<const Flux_ViewRequest> m_axViewRequests;              // main, editor, capture, preview, texture requests
	Zenith_Span<const Flux_RenderUIList> m_axUILists;
	Zenith_Span<const Flux_RenderDebugDraw> m_axDebugDraws;
	Flux_RenderEnvironment m_xEnvironment;                             // the world's one render environment (see "Render environment" below)
	Zenith_Span<const Flux_RenderInstanceKey> m_axHistoryResets;       // teleported instances: previous transform := current
	Zenith_Span<const Flux_RenderProceduralInstance> m_axProceduralInstances;   // GEOM's element type (Flux_ProceduralGeometry.h)
	Zenith_Span<const Flux_RenderTerrainDelta> m_axTerrainDeltas;               // ENV's element types (Flux_Environment.h) from here on
	Zenith_Span<const Flux_RenderTerrainTileUpdate> m_axTerrainTileUpdates;
	Zenith_Span<const Flux_RenderWaterBodyDelta> m_axWaterBodyDeltas;
	Zenith_Span<const Flux_RenderEnvironmentView> m_axEnvironmentViews;         // per-view underwater medium records
	Zenith_Span<const Flux_RenderSprite2DList> m_axSprite2DLists;               // TWOD's element types (Flux_Sprite2D.h) from here on
	Zenith_Span<const Flux_RenderTilemapChunk> m_axTilemapChunks;
	Zenith_Span<const Flux_RenderTilesetRecord> m_axTilesetRecords;
};

struct Flux_RenderWorld
{
	uint64 m_ulFrameIndex;
	uint64 m_ulSimulationTick;                                        // the tick of the frame's last simulation step
	float m_fInterpolationAlpha;                                       // Zenith_SimulationClock::GetInterpolationAlpha() of the frame
	uint64 m_ulInputSequence;                                         // the newest input sequence simulation consumed
	uint64 m_ulInputSampleTimeUs;                                     // its platform sample time, microseconds
	bool m_bFullState;                                                 // true: every section is a full-state extraction, not a delta
	Zenith_Span<const Flux_RenderWorldSection> m_axWorlds;
};

enum Flux_RenderWorldError : uint8 { FLUX_RENDER_WORLD_ERROR_SLOT_FULL, FLUX_RENDER_WORLD_ERROR_ENVIRONMENT_SET_TWICE };

class Flux_RenderWorldWriter          // one partition per extractor per chunk, handed to Zenith_PfnExtract
{
public:
	Zenith_Result<void, Flux_RenderWorldError> AddInstanceDelta(const Flux_RenderInstanceDelta& xDelta);
	Zenith_Result<void, Flux_RenderWorldError> AddLightDelta(const Flux_RenderLightDelta& xDelta);
	Zenith_Result<void, Flux_RenderWorldError> AddDecal(const Flux_RenderDecal& xDecal);
	Zenith_Result<void, Flux_RenderWorldError> AddSkinnedPose(const Flux_RenderSkinnedPose& xPose);
	Zenith_Result<void, Flux_RenderWorldError> AddEmitterCommand(const Flux_RenderEmitterCommand& xCommand);
	Zenith_Result<void, Flux_RenderWorldError> AddParticleCollider(const Flux_RenderParticleCollider& xCollider);
	Zenith_Result<void, Flux_RenderWorldError> AddViewRequest(const Flux_ViewRequest& xRequest);
	Zenith_Result<void, Flux_RenderWorldError> AddUIList(const Flux_RenderUIList& xList);
	Zenith_Result<void, Flux_RenderWorldError> AddDebugDraw(const Flux_RenderDebugDraw& xDraw);
	Zenith_Result<void, Flux_RenderWorldError> AddHistoryReset(Flux_RenderInstanceKey xKey);
	Zenith_Result<void, Flux_RenderWorldError> SetEnvironment(const Flux_RenderEnvironment& xEnvironment);
	Zenith_Result<void, Flux_RenderWorldError> AddProceduralInstance(const Flux_RenderProceduralInstance& xRecord);
	Zenith_Result<void, Flux_RenderWorldError> AddTerrainDelta(const Flux_RenderTerrainDelta& xDelta);
	Zenith_Result<void, Flux_RenderWorldError> AddTerrainTileUpdate(const Flux_RenderTerrainTileUpdate& xUpdate);
	Zenith_Result<void, Flux_RenderWorldError> AddWaterBodyDelta(const Flux_RenderWaterBodyDelta& xDelta);
	Zenith_Result<void, Flux_RenderWorldError> AddEnvironmentView(const Flux_RenderEnvironmentView& xView);
	Zenith_Result<void, Flux_RenderWorldError> AddSprite2DList(const Flux_RenderSprite2DList& xList);
	Zenith_Result<void, Flux_RenderWorldError> AddTilemapChunk(const Flux_RenderTilemapChunk& xChunk);
	Zenith_Result<void, Flux_RenderWorldError> AddTilesetRecord(const Flux_RenderTilesetRecord& xRecord);
	uint32 GetWorldTag() const;
	bool IsFullExtraction() const;           // extractors write full state instead of deltas this frame
};

class Flux_RenderWorldRing           // the two-slot ring; owned by ZenithFlux, driven by the composition root
{
public:
	void SetResyncPending(bool bResync);                   // main thread, between iterations, before RunFrame
	void RequestFullExtraction();                          // Flux itself: device-loss recovery
	bool IsFullExtractionPending() const;    // the OR of the two, read when EPILOGUE creates the writers
};
```

- [Z2-N-REND-007] A `Flux_RenderWorld` is immutable after `Render.Seal`; Flux reads it and writes nothing into it, and its storage is one slot of a ring of `uRENDER_WORLD_RING_SIZE` = 2 arenas: frame k's epilogue (phase 12 extraction) writes slot `k % 2`, and a slot is reset only at the start of the epilogue that reuses it, after the render subgraph that read it has completed, which gives the one frame of CPU render lag (Z2-D-050).
- [Z2-N-REND-008] The render world carries deltas, not full state: extractors write it exclusively through their own `Flux_RenderWorldWriter` partitions, which are merged in extractor registration order and then chunk order into each world's section; the GPU scene (see "GPU scene tables and pools") holds the persistent copy, replaying the delta stream from the first frame reproduces the table contents exactly, a record that does not fit the slot arena is refused with `FLUX_RENDER_WORLD_ERROR_SLOT_FULL` and counted in `Flux.RenderWorld.Dropped`, and `SetEnvironment` succeeds once per world per frame.
- [Z2-N-REND-009] An instance listed in a section's `m_axHistoryResets` has its previous transform set equal to its current transform for that frame, so a teleport produces zero velocity rather than a smear; only a camera cut (`Flux_ViewRequest::m_bCameraCut`) resets a view's temporal histories.

### Scope boundaries

| Capability | Classification | Governing decision |
|---|---|---|
| Hardware ray tracing | deferred to Stage 7 | Z2-D-043 |
| Virtualized geometry | deferred to Stage 7 | Z2-D-043 |
| Virtual texturing | required, Stage 4 | Z2-D-043 |
| Hair and groom | excluded | Z2-D-043, Z2-D-078 |
| Path-traced reference (compute, software BVH) | required, Stage 5, validation use exclusively | Z2-D-037, Z2-D-043 |
| Tile-local attachment merging on TBDR GPUs | not implemented until D-TBDR (open, Stage 5) is ruled | Z2-D-036 |
| MSAA | not provided by any path; antialiasing is TAA or a temporal upscaler | Z2-D-035 |
| Split-screen and per-player views | excluded | Z2-D-076 |
| Stereo views | deferred to Stage 7 (OpenXR), with view identity kept XR-ready | Z2-D-072 |

- [Z2-N-REND-010] No shading path implements MSAA, and no render target, PSO key or pass declaration carries a sample count other than one.
- [Z2-N-REND-011] No pass reads an attachment tile-locally (subpass input, `VK_KHR_dynamic_rendering_local_read`, or a D3D12 render-pass-local read) until D-TBDR (open, Stage 5) is ruled; the `ZENITH_RHI_PARAM_FIELD_ACCESS_TILE_LOCAL_READ` access kind exists in the field table, and the graph compiler rejects it with `FLUX_RG_ERROR_TILE_LOCAL_UNSUPPORTED` until then (Z2-D-036).

### Settled decisions this document implements

Z2-D-013 (GPU baseline), Z2-D-014 (GPU performance rules), Z2-D-015 (Slang is the source of truth), Z2-D-025 (offline shader compilation and PSO precaching), Z2-D-026 (volk), Z2-D-035 (shading paths), Z2-D-036 (TBDR), Z2-D-037 (path tracer), Z2-D-043 (advanced graphics classification), Z2-D-050 (frame pipelining), Z2-D-054 (bindless binding model), Z2-D-055 (render graph), Z2-D-056 (barrier synthesis), Z2-D-057 (transient aliasing), Z2-D-058 (GPU scene tables), Z2-D-059 (render views), Z2-D-060 (two-phase HiZ culling), Z2-D-061 (depth mode), Z2-D-062 (vertex data), Z2-D-063 (validation roles), Z2-D-064 (research methodology). Open decisions cited: D-SLANG (open, Stage 1), D-SEL-HASH (open, Stage 1), D-VTX-TANGENTLESS (open, Stage 3), D-TBDR (open, Stage 5), D-SEL-UPSCALER (open, Stage 5; LIGHT's external temporal upscaler).

### Conventions used in this document

- [Z2-N-REND-012] Every type, function, member, enum value and constant introduced in this document family uses Zenith Hungarian (Z2-D-034): renderer classes `Flux_*`, RHI classes `Zenith_RHI*`, members `m_` with type prefixes, enums `SCREAMING_SNAKE_CASE` with a category prefix, constants `uFLUX_*`/`fFLUX_*`.
- [Z2-N-REND-013] Slang code follows the same Hungarian conventions as C++ (see `Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"`, subsection "Naming and layout"): vector and matrix fields and locals are objects and take `x` (`m_xPositionWS`, `m_xViewProjection`, arrays `m_ax…`), scalars take `u`/`i`/`f`/`b`, permutation constants are `bPERM_*`/`uPERM_*`, and functions are PascalCase; the C++ maths types are `Zenith_Vector2`, `Zenith_Vector3`, `Zenith_Vector4`, `Zenith_Matrix4` and `Zenith_Quaternion`.
- [Z2-N-REND-014] All sizes in this document family are bytes unless a unit is written, all distances are metres, all angles are radians, and all colour values in GPU tables are linear Rec.709 unless a format name says sRGB.

Paths are repo-root-relative (`Zenith2/Engine/ZenithFlux/Internal/Graph/…`). Budget rows are cited by their `Z2-B-<AREA>-NNN` ID and name from `Zenith2/Docs/Budgets.md`; a pending row is allocated by the Stage 1 per-system budget ticket for its area, and a number written beside a pending row is the proposed value that ticket adopts or replaces. A row this family needs that `Zenith2/Docs/Budgets.md` does not yet carry is named descriptively with its producer and recorded as a flagged proposal (Z2-D-022) for the Budgets writer. Tests are named by their test ID (`Test_<Type>_<Behaviour>`, per `Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"`, subsection "Test naming"); a test named in this document lives under `Zenith2/Engine/ZenithFlux/Tests/` unless another path is given.

### Render world resync

A render world carries deltas, so a world that no render instance consumes drops its deltas. After an iteration in which the frame executor's cancel flag was set, the executor sets `m_bResyncRenderWorld` (read through `Zenith_FrameExecutor::IsRenderWorldResyncPending()`, and cleared when an epilogue seals; see `Zenith2/Docs/Architecture.md → "CPU frame graph and scheduler"`), and the next sealed render world is a full-state extraction.

<!-- elaboration -->
| Step | Action |
|---|---|
| 1 | Between iterations, on the main thread, the composition root calls `Flux_RenderWorldRing::SetResyncPending(IsRenderWorldResyncPending())` before `RunFrame(k+1)`. |
| 2 | When EPILOGUE(k+1) creates the extractors' writers, `IsFullExtraction()` is `IsFullExtractionPending()`: the executor's resync, or Flux's own device-loss request (`RequestFullExtraction`, [Z2-N-REND-987] recovery step 4). |
| 3 | With full extraction set, every extractor writes the complete current state of its world (an add delta for every live instance and light, every live decal, pose and emitter state, the environment) and no remove deltas; the sealed world carries `m_bFullState = true`. |
| 4 | When the render subgraph consumes a world with `m_bFullState`, the GPU scene discards its CPU-side slot state, re-allocates every slot from the full-state records and scatters every record, so the tables equal a delta replay from the first frame. |
| 5 | Flux's own request is cleared when a world with `m_bFullState` seals; the executor's flag is cleared by the executor at the same seal. |

- [Z2-N-REND-015] The next sealed render world after any iteration in which the frame executor's cancel flag was set is a full-state extraction (`m_bFullState = true`), produced and consumed by the steps of the table above, and device-loss recovery uses the same path through `Flux_RenderWorldRing::RequestFullExtraction`.
- [Z2-N-REND-016] The GPU-scene update node of a render instance may return early at its cancel check, because the next world it consumes is a full-state world that rebuilds every table from its records; no other Flux node leaves partial state behind at a cancel.
- [Z2-N-REND-017] A full-state world is the sizing case of the render-world slot arenas, which are budgeted within `Z2-B-CORE-005` Frame and graph arenas (pending), and `Test_RenderWorld_ResyncAfterCancelIsFullState` (a cancel during TICK(k) gives a full-state world k+1 whose GPU-scene tables equal a delta replay) and `Test_RenderWorld_DeviceLossUsesFullState` exist and pass.

### Render-world record types

The section's element types are declared by the area that produces them, in ZenithFlux public headers, so Flux reads them without a module header; Rendering declares the section, the writer and the rules below.

<!-- elaboration -->
| Element | Declared in (owner) | Rule |
|---|---|---|
| `Flux_RenderInstanceKey`, `Flux_RenderInstanceDelta`, `Flux_ViewRequest` | `Flux_RenderWorld.h` (RPROD, `Zenith2/Docs/Design/RPROD/Extractors.md → "Flux_RenderWorld header additions"`) | `Flux_ViewRequest` carries LIGHT's `Flux_CameraLightingSettings m_xCameraLighting`, which `Flux_BuildViewList` copies into `Flux_View::m_xCameraLighting` |
| `Flux_RenderLightDelta` | `Flux_Lights.h` (LIGHT) | the sun and the moon are never light deltas: they are the slots of "Render environment" |
| `Flux_RenderSkinnedPose` | `Flux_Skinning.h` (GEOM) | one per skinned instance per frame |
| `Flux_RenderDecal` | `Flux_Decal.h` (GEOM) | one per live decal per frame |
| `Flux_RenderDebugDraw` | `Flux_DebugDraw.h` (DEVT, `Zenith2/Docs/Design/DEVT/DebugDraw.md` → "Flux_DebugDraw header") | `AddDebugDraw` copies the record and its lines or quads span into the partition's slot arena and re-points the span; a record that does not fit is refused whole with `FLUX_RENDER_WORLD_ERROR_SLOT_FULL` |
| `Flux_RenderUIList` | `Flux_UI.h` (UI, `Zenith2/Docs/Design/UI/Rendering.md` → "Render-world records"), which `Flux_RenderWorld.h` includes | one per world per frame through `AddUIList`; whole-frame state, never a delta, and presentation-only |
| `Flux_RenderEmitterCommand`, `Flux_RenderParticleCollider` | `Flux_Particles.h` (VFX, `Zenith2/Docs/Design/VFX/PublicAPI.md` → "Flux_Particles header") | `AddEmitterCommand` copies the command's `m_axCpuParticles` into the partition's slot arena and re-points the copy's span there, sized within `Z2-B-CORE-005` ([Z2-N-REND-017]); `AddParticleCollider` appends one collider record |
| `Flux_RenderProceduralInstance` | `Flux_ProceduralGeometry.h` (GEOM, `Zenith2/Docs/Design/GEOM/Procedural.md → "Flux_ProceduralGeometry header"`) | a procedural instance is two records with the same key: one `Flux_RenderProceduralInstance` (generator, topology, frame parameters) through `AddProceduralInstance`, and one `Flux_RenderInstanceDelta` carrying `FLUX_INSTANCE_FLAG_PROCEDURAL` and a null `Flux_MeshID`, whose material override 0 is its material; GEOM's `Flux_ProceduralApplyRecords` consumes the spans on `Flux.BuildGraph` before the GPU scene applies the deltas |
| `Flux_RenderTerrainDelta`, `Flux_RenderTerrainTileUpdate`, `Flux_RenderWaterBodyDelta`, `Flux_RenderEnvironmentView` | `Flux_Environment.h` (ENV, `Zenith2/Docs/Design/ENV.md → "Render-world records"`) | each typed member copies the record and every span it holds (tile chains, node runs, min/max values, a water body's polygon) into the partition's slot arena and re-points the copy's spans there; a record, or one of its spans, that does not fit is refused whole with `FLUX_RENDER_WORLD_ERROR_SLOT_FULL`; `Flux_EnvironmentRenderer::ApplyRecords` applies a section's four spans on `Flux.BuildGraph` in the order terrain deltas, tile updates, water-body deltas, view records, each in record order; a section of a world without the Environment module has the four spans empty |
| `Flux_RenderSprite2DList`, `Flux_RenderTilemapChunk`, `Flux_RenderTilesetRecord` | `Flux_Sprite2D.h` (TWOD, `Zenith2/Docs/Design/TWOD/PublicAPI.md` → "Flux_Sprite2D header") | `AddSprite2DList` (one whole-frame list per world per frame), `AddTilemapChunk` and `AddTilesetRecord` (delta records) each copy the record and every span it holds into the partition's slot arena and re-point the copy's spans there, the ENV rule; a record or span that does not fit is refused whole with `FLUX_RENDER_WORLD_ERROR_SLOT_FULL` |
| `Flux_VideoDrawRecord`, `Flux_ScreenOverlayRecord` | `Flux_Video.h` (CINE, `Zenith2/Docs/Design/CINE/Formats.md` → "Render-world records") | written by the Cinematics module's extractor through `AddVideoDraw` and `AddScreenOverlay` (typed members copied into the partition's arena); the merged spans are read by the `VideoComposite` pass |
| the engine components the engine extractors read (`Zenith.Render.MeshInstance`, `Zenith.Render.Camera`, LIGHT's light and world-lighting components) | `Flux_RenderComponents.h` (RPROD), `Flux_LightComponents.h` (LIGHT) | engine-defined component types are C++-registered structs with fixed property IDs, registered by ZenithRuntime in startup step 6, never data-defined schemas ([Z2-N-ARCH-591] is unchanged) |

- [Z2-N-REND-018] Every element type of a `Flux_RenderWorldSection` span is declared in the ZenithFlux public header the table above names, by the owner it names, and is appended exclusively through its own typed `Flux_RenderWorldWriter` member under the copying, refusal, consumption and component rules of that table.

### Render environment

`Flux_RenderEnvironment` is one plain value per world section. Its members' types are LIGHT's and ENV's; this document cites them and declares none of them.

<!-- elaboration -->
```cpp
// Zenith2/Engine/ZenithFlux/Flux_RenderWorld.h
struct Flux_RenderEnvironment
{
	Flux_RenderLightingEnvironment m_xLighting;   // LIGHT's type (Flux_LightSettings.h): the atmosphere block and the world-lighting block
	Flux_RenderLightDelta m_xSun;                 // LIGHT's record type: the sun slot, directional light 0 when m_bHasSun
	Flux_RenderLightDelta m_xMoon;                // the moon slot, directional light 1 when m_bHasMoon
	Flux_EnvironmentBlocks m_xBlocks;             // ENV's resource (Flux_Environment.h), copied whole; its member defaults without the Environment module
	bool m_bHasSun = false;
	bool m_bHasMoon = false;
};
```

| Rule | Specification |
|---|---|
| the one writer | `Render.ExtractEnvironment`, the engine environment extractor of ZenithRuntime (one phase-12 node per world, `Zenith2/Docs/Design/RPROD/Renderer.md → "Render environment"`), calls `SetEnvironment` exactly once per world per frame; no module extractor writes the render environment, and ENV supplies the blocks it reads |
| what it reads | LIGHT's world-lighting component for `m_xLighting`'s world-lighting block, and ENV's `PHASE_INPUT` world resource `Zenith.Environment.RenderBlocks` (`Flux_EnvironmentBlocks`) when the world runs the Environment module; the phase boundary orders the read after every phase-1 write, and no phase-12 node writes the resource |
| absent module | `m_xLighting`'s atmosphere block carries `m_bValid` false, neither slot is set, and `m_xBlocks` holds its member defaults |
| atmosphere and fog | `m_xLighting`'s atmosphere block (atmosphere, fog, local fog volumes, stars, cloud shadow, `m_uSkyRevision`) is copied from ENV's resource under LIGHT's type names; its local-fog-volume span points at the section's own copy in `m_xBlocks` |
| sun and moon slots | `m_xSun` and `m_xMoon` are directional lights 0 and 1 in LIGHT's reserved light-table slots 0 and 1 whatever other directional lights the scene holds; the slot position is the role, so no light flag names a sun or a moon; ENV writes the slot flag contract of `Zenith2/Docs/Design/LIGHT/Lighting.md → "Sun and moon slots"` |
| conflict rule | a scene directional light beyond the directional limit (two entries) is dropped by LIGHT's overflow rule and counted in `Flux.Lights.DirectionalDropped`; an authored directional light never displaces a slot |
| underwater medium | not an environment member: ENV's per-view `Flux_RenderEnvironmentView` records carry it |

- [Z2-N-REND-019] A world's render environment is written exclusively by the engine environment extractor `Render.ExtractEnvironment`, exactly once per world per frame, with the members, slots and conflict rule of the table above, and a second `SetEnvironment` for the world in one frame returns `FLUX_RENDER_WORLD_ERROR_ENVIRONMENT_SET_TWICE` and leaves the first value.

## GPU baseline and profiles

### Vulkan baseline profile

The Vulkan baseline is a custom Vulkan-Profiles JSON profile, `VP_ZENITH2_baseline`, committed at `Zenith2/Engine/ZenithRHI_Vulkan/Internal/Profiles/VP_ZENITH2_baseline.json`.

| Requirement | Kind | Why Zenith2 needs it |
|---|---|---|
| Vulkan API version 1.3 | core version | dynamic rendering, synchronization2, maintenance4 are core |
| `dynamicRendering` | 1.3 feature | no render-pass or framebuffer objects |
| `synchronization2` | 1.3 feature | `vkCmdPipelineBarrier2`, split barriers through `vkCmdSetEvent2`/`vkCmdWaitEvents2` |
| `maintenance4` | 1.3 feature | `vkGetDeviceImageMemoryRequirements` for placement before creation |
| `timelineSemaphore` | 1.2 feature | per-queue completion tracking |
| `drawIndirectCount` | 1.2 feature | `vkCmdDrawIndexedIndirectCount` |
| `shaderFloat16` | 1.2 feature | native half arithmetic; a custom Zenith2 addition beyond Roadmap 2022 |
| `hostQueryReset` | 1.2 feature | timestamp pool reset from the CPU |
| `runtimeDescriptorArray`, `descriptorBindingPartiallyBound`, `descriptorBindingUpdateUnusedWhilePending`, `descriptorBindingSampledImageUpdateAfterBind`, `descriptorBindingStorageImageUpdateAfterBind`, `descriptorBindingStorageBufferUpdateAfterBind`, `shaderSampledImageArrayNonUniformIndexing`, `shaderStorageImageArrayNonUniformIndexing`, `shaderStorageBufferArrayNonUniformIndexing` | 1.2 descriptor-indexing features | the bindless heaps |
| `shaderDrawParameters` | 1.1 feature | `DrawIndex` for indirect draws |
| `samplerAnisotropy`, `independentBlend`, `imageCubeArray`, `multiDrawIndirect`, `fragmentStoresAndAtomics`, `depthClamp`, `depthBiasClamp`, `shaderStorageImageWriteWithoutFormat` | 1.0 features | anisotropic filtering, MRT blend state, cube arrays, multi-draw, VT feedback and debug writes, shadow pancaking, typed UAV writes |
| `textureCompressionBC` (Windows) / `textureCompressionASTC_LDR` (Android) | 1.0 feature, per platform | cooked BC7 / ASTC textures |
| `VK_KHR_swapchain` | device extension | presentation |
| `maxPushConstantsSize` ≥ 8, `maxBoundDescriptorSets` ≥ 1 | limits | one set, one 8-byte push constant |
| `maxDescriptorSetUpdateAfterBindSampledImages`, `…StorageImages`, `…StorageBuffers` ≥ the tier's heap capacity | limits | see "Heap capacities per capability tier" |
| `timestampComputeAndGraphics` | limit | per-pass GPU timestamps |

Optional extensions, used when present and recorded in `Zenith_RHICapabilities`: `VK_EXT_mesh_shader`, `VK_EXT_memory_budget`, `VK_EXT_calibrated_timestamps`, `VK_KHR_present_id`, `VK_KHR_present_wait`, `VK_EXT_device_fault`, `VK_NV_device_diagnostic_checkpoints`, `VK_AMD_buffer_marker`, `VK_GOOGLE_display_timing`, `VK_EXT_hdr_metadata` (realises `Zenith_RHISwapchain::SetHDRMetadata` on Vulkan and sets `Zenith_RHICapabilities::m_bHdrMetadata`; D3D12 realises the call through `IDXGISwapChain4::SetHDRMetaData`, so the capability is present on every D3D12 device). `VK_EXT_mutable_descriptor_type` is consumed exclusively under the first alternative of D-SLANG (open, Stage 1) (see "Decision D-SLANG"), in which case it joins the baseline list.

- [Z2-N-REND-020] The Vulkan backend creates its device through the Vulkan-Profiles API against `VP_ZENITH2_baseline` (`vpGetPhysicalDeviceProfileSupport`, then `vpCreateDevice`), and refuses a physical device the profile does not support with `ZENITH_RHI_ERROR_PROFILE_UNSUPPORTED`, listing every missing feature, extension and limit in the error text (Z2-D-013); the Vulkan-Profiles files generated from `VP_ZENITH2_baseline.json` (`ZenithRHI_Vulkan/Internal/Profiles/Generated/vulkan_profiles_zenith2.hpp`) are committed rather than generated at build time, and `Test_VulkanProfile_GeneratedMatchesJson` fails when they no longer match the JSON.
- [Z2-N-REND-021] Every Vulkan instance-level and device-level entry point is loaded through volk (`volkInitialize`, `volkLoadInstanceOnly`, `volkLoadDevice`), with device-level dispatch through the loaded device table on Windows and Android alike; no Vulkan function is called through the loader trampoline after device creation (Z2-D-026).
- [Z2-N-REND-022] Vulkan memory is allocated exclusively through VMA, and the backend creates its `VmaAllocator` with `VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT` whenever `VK_EXT_memory_budget` is present.
- [Z2-N-REND-023] The Stage 1 S24 profile ticket confirms `VP_ZENITH2_baseline` on the Galaxy S24 Ultra (Adreno 750), lavapipe and the RTX 3070; a baseline requirement a validation device lacks is filed as a flagged proposal (Z2-D-022), and the profile is not silently weakened.

### D3D12 baseline

| Requirement | Query | Why |
|---|---|---|
| Shader Model 6.6 | `D3D12_FEATURE_SHADER_MODEL` | `ResourceDescriptorHeap` dynamic resources |
| Resource binding tier 3 | `D3D12_FEATURE_D3D12_OPTIONS.ResourceBindingTier` | full heap indexing from shaders |
| Enhanced Barriers | `D3D12_FEATURE_D3D12_OPTIONS12.EnhancedBarriersSupported` | barrier synthesis, split barriers, discard on first use |
| Native 16-bit shader ops | `D3D12_FEATURE_D3D12_OPTIONS4.Native16BitShaderOpsSupported` | `-enable-16bit-types` half precision |
| Wave intrinsics | `D3D12_FEATURE_D3D12_OPTIONS1.WaveOps` | SPD downsampler, compaction |
| Typed UAV load of `R11G11B10_FLOAT`, `R16G16B16A16_FLOAT`, `R16G16_FLOAT` | `D3D12_FEATURE_FORMAT_SUPPORT` | in-place post-process |
| `WriteBufferImmediate` | `ID3D12GraphicsCommandList2` | GPU breadcrumbs |
| Agility SDK version pinned in `ThirdParty/manifest.json` | loader export `D3D12SDKVersion` | enhanced barriers on every supported Windows build |

Optional, recorded in `Zenith_RHICapabilities`: mesh shader tier 1, resource heap tier 2, DRED (development builds enable it whenever the runtime exposes it).

- [Z2-N-REND-024] The D3D12 backend refuses an adapter that fails any row of the D3D12 baseline table with `ZENITH_RHI_ERROR_PROFILE_UNSUPPORTED`, listing every failed row (Z2-D-013).
- [Z2-N-REND-025] D3D12 memory is allocated exclusively through D3D12MA, and every placed transient lives in a D3D12MA-created heap.
- [Z2-N-REND-026] D3D12 exists on Windows alone; there the backend is chosen at runtime by the `rhi.backend` configuration value (`vulkan` default, `d3d12`), and both backend libraries are linked into every Windows Player and editor executable (Z2-D-010).

### Capability negotiation

```cpp
struct Zenith_RHICapabilities
{
	Zenith_RHIBackend m_eBackend;              // ZENITH_RHI_BACKEND_VULKAN, ZENITH_RHI_BACKEND_D3D12
	uint32 m_uVendorID;
	uint32 m_uDeviceID;
	uint64 m_ulDriverVersion;                 // VkPhysicalDeviceProperties::driverVersion or the D3D12 UMD version
	uint8 m_auPipelineCacheUUID[16];         // Vulkan; zero on D3D12
	bool m_bDepthD24S8;                        // D24_UNORM_S8_UINT usable as a depth attachment and sampled image
	bool m_bMeshShaders;
	bool m_bAsyncComputeQueue;                 // a compute-only queue family or a D3D12 compute queue exists
	bool m_bCopyQueue;
	uint32 m_auTimestampValidBits[ZENITH_RHI_QUEUE_COUNT];
	bool m_bMemoryBudget;
	bool m_bCalibratedTimestamps;
	bool m_bPresentWait;
	bool m_bDeviceFaultInfo;                   // VK_EXT_device_fault or DRED
	uint32 m_uHeapTier;                       // D3D12 resource heap tier; 0 on Vulkan
	uint32 m_uSubgroupSizeMin;
	uint32 m_uSubgroupSizeMax;
	uint64 m_ulDeviceLocalBytes;
	uint64 m_ulHostVisibleDeviceLocalBytes;   // ReBAR / UMA
	Zenith_RHICapabilityTier m_eCapabilityTier;   // derived, see below
};

enum Zenith_RHICapabilityTier : uint8 { ZENITH_RHI_CAPABILITY_TIER_DESKTOP = 1, ZENITH_RHI_CAPABILITY_TIER_MOBILE = 2 };   // Zenith_RHICapabilities.h

// ZenithFlux/Flux_QueueValues.h: the Flux spellings of the RHI tier, used by Flux code and the per-pass tables
using Flux_CapabilityTier = Zenith_RHICapabilityTier;
inline constexpr Zenith_RHICapabilityTier FLUX_CAPABILITY_TIER_DESKTOP = ZENITH_RHI_CAPABILITY_TIER_DESKTOP;
inline constexpr Zenith_RHICapabilityTier FLUX_CAPABILITY_TIER_MOBILE = ZENITH_RHI_CAPABILITY_TIER_MOBILE;
```

<!-- elaboration -->
| `Zenith_RHICapabilityTier` | Assigned when | Consequences |
|---|---|---|
| `ZENITH_RHI_CAPABILITY_TIER_DESKTOP` | Windows, either backend | every shading path; desktop heap capacities; async compute when `m_bAsyncComputeQueue` |
| `ZENITH_RHI_CAPABILITY_TIER_MOBILE` | Android | Forward+ is the one shading path; mobile heap capacities; `Z2-B-RPROD-026` HiZ mid-frame depth store and reload bandwidth applies |

- [Z2-N-REND-027] Flux reads device properties exclusively through `Zenith_RHICapabilities`, which the RHI backend fills once at device creation; no Flux code queries a Vulkan or D3D12 API for a capability.
- [Z2-N-REND-028] `m_eCapabilityTier` is `ZENITH_RHI_CAPABILITY_TIER_MOBILE` on Android and `ZENITH_RHI_CAPABILITY_TIER_DESKTOP` on Windows; the capability tier selects heap capacities and available shading paths, and is independent of the per-platform quality tier (see "Render views").
- [Z2-N-REND-043] Every type ZenithRHI declares carries the `Zenith_RHI` prefix, the capability tier `Zenith_RHICapabilityTier` and the device-handle tag types `Zenith_RHITextureTag`, `Zenith_RHIBufferTag`, `Zenith_RHISamplerTag`, `Zenith_RHIPipelineTag`, `Zenith_RHIHeapTag` and `Zenith_RHIQueryPoolTag` included; ZenithFlux keeps the `Flux_CapabilityTier` spelling and its two value names only as the aliases of the block above, and the `Flux_*Handle` aliases of the device handles stay (see `Zenith2/Docs/Contracts.md → "Handle kinds"`).
- [Z2-N-REND-029] When `m_bDepthD24S8` is false the device-wide default depth format becomes `D32_SFLOAT_S8_UINT` (the capability fallback of Z2-D-061), and the cross-API parity tests record, per device, which depth format and Z direction each parity view took, so that an AMD device's D32F path is visible in the parity report.

### Heap capacities per capability tier

<!-- elaboration -->
| Heap index space | `ZENITH_RHI_CAPABILITY_TIER_DESKTOP` | `ZENITH_RHI_CAPABILITY_TIER_MOBILE` |
|---|---|---|
| Sampled textures (`FLUX_DESCRIPTOR_SPACE_TEXTURE_SRV`) | 65,536 | 16,384 |
| Storage textures (`FLUX_DESCRIPTOR_SPACE_TEXTURE_UAV`) | 8,192 | 2,048 |
| Read-only buffers (`FLUX_DESCRIPTOR_SPACE_BUFFER_SRV`) | 65,536 | 16,384 |
| Read-write buffers (`FLUX_DESCRIPTOR_SPACE_BUFFER_UAV`) | 16,384 | 4,096 |
| Static samplers | 16 | 16 |

- [Z2-N-REND-030] The heap capacities in the table above are the proposed values of `Z2-B-RHI-009` Bindless heap capacity per kind (pending) and, once that row is adopted, profile minimums: device creation fails with `ZENITH_RHI_ERROR_PROFILE_UNSUPPORTED` if the device's update-after-bind descriptor limits are below them, and the on-screen profiler reports slot occupancy per space against them and the heap memory against `Z2-B-RHI-002` Bindless descriptor heaps.
- [Z2-N-REND-031] Descriptor slot 0 of every index space holds a valid null-substitute descriptor (a 1×1 texture holding `(1, 0, 1, 1)` for texture spaces, a 256-byte zeroed buffer for buffer spaces); no allocation returns slot 0, so an uninitialised `Zenith_RHIDescriptorHandle` reads a defined value in every build.

### Half precision

- [Z2-N-REND-032] Half-precision shader arithmetic compiles to SPIR-V `Float16` under `shaderFloat16` on Vulkan and to DXIL native 16-bit types (`-enable-16bit-types`, requiring `Native16BitShaderOpsSupported`) on D3D12; `min16float` and `RelaxedPrecision`-style minimum-precision lowering are not used for half precision anywhere.
- [Z2-N-REND-033] The Stage 1 half-precision check runs a compute shader exercising `half` add, multiply, fused multiply-add, conversion and storage on WARP, lavapipe, the S24 and the RTX 3070, and compares bit-exact results against a CPU oracle; half-precision shaders do not enter CI until WARP and lavapipe pass it.

### Headless and CI devices

- [Z2-N-REND-034] GPU test executables create their device on WARP (D3D12, from the pinned NuGet package) and lavapipe (Vulkan, the pinned ICD selected through `VK_DRIVER_FILES`); every render test, graph unit test and parity scene runs on both, and a test skipped on either records the skip reason in its result.
- [Z2-N-REND-044] A GPU test that needs a swapchain first creates a hidden window through `Zenith_Window` with `Zenith_WindowDesc::m_bHidden = true` (realised by `SDL_WINDOW_HIDDEN`), and every other GPU test creates its device with no window and no swapchain; a headless run in the sense of [Z2-N-ARCH-785] (the CI integration smoke) creates no window, no device and no render subgraph, and lavapipe's pinned build includes its Win32 WSI for the hidden-window case.
- [Z2-N-REND-035] A feature WARP or lavapipe lacks (for example mesh shaders) is exercised in CI through its capability fallback, and every capability fallback is itself a tested path.

### Required format features

<!-- elaboration -->
| Format (Vulkan / D3D12) | Used by | Sampled, linear filter | Colour attachment, blend | Storage write | Storage typed load | Depth-stencil attachment |
|---|---|---|---|---|---|---|
| `B10G11R11_UFLOAT_PACK32` / `R11G11B10_FLOAT` | HDR colour, SSR previous colour and composite, opaque colour copy, bloom chain, TAA output, depth of field, motion blur, sky-view LUT, forward specular weight, environment cube array and sky capture faces (Stage 4), ENV's cloud layer (Stage 4) | yes | yes, blend | yes | yes (SSR composite, in-place post) | — |
| `R16G16B16A16_SFLOAT` / `R16G16B16A16_FLOAT` | TAA history, froxel fog volumes and fog scattering history (3D), SSR hits, transmittance and multiple-scattering LUTs, SSGI history and result (Stage 5), scRGB swapchain | yes | yes, blend | yes | yes | — |
| `R16G16_SFLOAT` / `R16G16_FLOAT` | velocity, motion-blur tile and neighbour max, HiZ pyramid | yes | yes | yes | yes (single-pass downsampler) | — |
| `R16G16_UNORM` / `R16G16_UNORM` | prepass normals, G-buffer GB1 | yes | yes, blend (deferred decals) | yes (material resolve) | — | — |
| `R16_SFLOAT` / `R16_FLOAT` | circle of confusion, the sheen albedo LUT (Stage 5) | yes | — | yes | — | — |
| `R16_UNORM` / `R16_UNORM` | GEOM's terrain height tiles | yes (sampled with texel `Load` alone; no linear filter) | — | — | — | — |
| `R8_UNORM` / `R8_UNORM` | prepass roughness, AO, contact-shadow mask, ENV's cloud transmittance (Stage 4) and cloud shadow map (Stage 5) | yes | yes | yes | — | — |
| `R8G8B8A8_UNORM` / `R8G8B8A8_UNORM` | GB2, GB4 (advanced surfaces, Stage 5), GB0 storage view, grading LUT (3D) | yes | yes, blend | yes | — | — |
| `R8G8B8A8_SRGB` / `R8G8B8A8_UNORM_SRGB` | GB0 attachment and sampled view; TWOD's canvas render target (colour attachment, sampled by the composite) and its point-sampled atlas pages of usage `COLOUR_EXACT` (Stage 3) | yes | yes, blend | — | — | — |
| `R32_UINT` / `R32_UINT` | visibility ID | yes (no filter) | yes (no blend) | yes | yes | — |
| `D32_SFLOAT_S8_UINT` / `D32_FLOAT_S8X24_UINT` | reversed-Z views, the capability fallback | yes (depth aspect) | — | — | — | yes |
| `D24_UNORM_S8_UINT` / `D24_UNORM_S8_UINT` | the default depth format where exposed (`m_bDepthD24S8`) | yes (depth aspect) | — | — | — | optional |
| `B8G8R8A8_SRGB`, `A2B10G10R10_UNORM_PACK32` / `B8G8R8A8_UNORM_SRGB`, `R10G10B10A2_UNORM` | swapchain | — | yes | — | — | — |
| `BC7_UNORM_BLOCK`, `BC7_SRGB_BLOCK` (Windows) / `BC7_UNORM`, `BC7_UNORM_SRGB`; `ASTC_4x4_UNORM_BLOCK`, `ASTC_4x4_SRGB_BLOCK` (Android) | material textures | yes | — | — | — | — |

- [Z2-N-REND-036] Every Vulkan format row of the table above is required with exactly the listed `optimalTilingFeatures` bits (`SAMPLED_IMAGE`, `SAMPLED_IMAGE_FILTER_LINEAR`, `COLOR_ATTACHMENT`, `COLOR_ATTACHMENT_BLEND`, `STORAGE_IMAGE`, `DEPTH_STENCIL_ATTACHMENT`) in the `formats` section of `VP_ZENITH2_baseline.json`, except the row marked optional, and a device lacking one is refused with `ZENITH_RHI_ERROR_PROFILE_UNSUPPORTED`; storage reads declare their format in the shader, so no `shaderStorageImageReadWithoutFormat` is needed.
- [Z2-N-REND-037] The D3D12 backend checks the same rows through `D3D12_FEATURE_FORMAT_SUPPORT` (`Support1`: texture sample, render target, blendable, typed UAV, depth-stencil; `Support2`: `UAV_TYPED_STORE`, and `UAV_TYPED_LOAD` for the typed-load column) and refuses an adapter that lacks one with `ZENITH_RHI_ERROR_PROFILE_UNSUPPORTED`.
- [Z2-N-REND-038] A pass attaches, blends, stores to or typed-loads a format exclusively where the table grants that feature; the table's compiled form is the public ZenithRHI root header `Zenith2/Engine/ZenithRHI/Zenith_FormatFeatures.h` (see "Format feature header"), which both backends' device-refusal code, the graph compile check (`FLUX_RG_ERROR_FORMAT_FEATURE_MISSING`) and `Test_BindlessProbe_StorageFormats` read, and adding a format or a feature changes the table, the header's source and the profile in the same diff.
- [Z2-N-REND-039] G-buffer GB0 is created as a mutable-format image with the view-format list {`R8G8B8A8_SRGB`, `R8G8B8A8_UNORM`} (Vulkan `VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT` with `VkImageFormatListCreateInfo`; D3D12 `R8G8B8A8_TYPELESS`), attached and sampled through the sRGB view and written as storage through the UNORM view (see `Zenith2/Docs/Rendering/PerPass/Deferred.md` "GBuffer").

### Format feature header

<!-- elaboration -->
`Zenith_FormatFeatures.h` is a public root header of ZenithRHI generated at build time from `Zenith2/Engine/ZenithRHI/Internal/FormatFeatures.table`, a committed plain-text file holding one line per row of the format-feature table above. The table and its generator live under the library's `Internal/` directory, because a library root holds its public headers, their `.inl` companions and its `CMakeLists.txt` alone ([Z2-N-CONTRACT-021]); the generated header is the public root header the runtime reads.

| `FormatFeatures.table` grammar | Rule |
|---|---|
| line | `<Zenith_RHIFormat enumerator> <VkFormat> <DXGI_FORMAT> <features> [optional]`, fields separated by one or more spaces; `<DXGI_FORMAT>` is `-` for a format D3D12 lacks (ASTC) |
| `<features>` | letters from `SLCBWTD`, each at most once, in that order: S sampled, L linear filter, C colour attachment, B blend, W storage write, T storage typed load, D depth-stencil attachment |
| `optional` | the literal word; the row is checked but a device lacking it is not refused (D24S8 alone) |
| `platform=<windows\|android>` | an optional last token (after `optional` when both appear): the row is checked and generated for that platform alone (the BC7 rows are `platform=windows`, the ASTC rows `platform=android`), and it sets the row's `m_uPlatformMask` |
| comments and blank lines | `#` to end of line is a comment; blank lines are ignored |
| example | `ZENITH_RHI_FORMAT_R11G11B10_FLOAT VK_FORMAT_B10G11R11_UFLOAT_PACK32 DXGI_FORMAT_R11G11B10_FLOAT SLCBWT` |

| Generator element | Value |
|---|---|
| script | `Zenith2/Engine/ZenithRHI/Internal/CMake/GenerateFormatFeatures.cmake`, run as `cmake -P GenerateFormatFeatures.cmake -DINPUT=<table> -DOUTPUT=<header>` |
| declaration | `zenith_add_library(ZenithRHI … GENERATES Zenith_FormatFeatures.h)` (the `GENERATES` option of `Zenith2/Docs/Architecture.md → "Build system and presets"`), which attributes the header to ZenithRHI |
| output | `${ZENITH_OUT}/generated/ZenithRHI/Zenith_FormatFeatures.h` |
| failure | a malformed line stops the build with `FormatFeatures.table:<line>: <reason>` (unknown enumerator, unknown Vulkan or DXGI name, a letter outside `SLCBWTD`, a repeated letter, a duplicate format) |

```cpp
// Zenith_RHITypes.h (public root header of ZenithRHI)
enum Zenith_RHIFormat : uint8
{
	ZENITH_RHI_FORMAT_UNDEFINED = 0,
	ZENITH_RHI_FORMAT_R11G11B10_FLOAT,
	ZENITH_RHI_FORMAT_R16G16B16A16_FLOAT,
	ZENITH_RHI_FORMAT_R16G16_FLOAT,
	ZENITH_RHI_FORMAT_R16G16_UNORM,
	ZENITH_RHI_FORMAT_R16_FLOAT,
	ZENITH_RHI_FORMAT_R8_UNORM,
	ZENITH_RHI_FORMAT_R8G8B8A8_UNORM,
	ZENITH_RHI_FORMAT_R8G8B8A8_UNORM_SRGB,
	ZENITH_RHI_FORMAT_R32_UINT,
	ZENITH_RHI_FORMAT_D32_FLOAT_S8X24_UINT,
	ZENITH_RHI_FORMAT_D24_UNORM_S8_UINT,
	ZENITH_RHI_FORMAT_B8G8R8A8_UNORM_SRGB,
	ZENITH_RHI_FORMAT_R10G10B10A2_UNORM,
	ZENITH_RHI_FORMAT_BC7_UNORM,
	ZENITH_RHI_FORMAT_BC7_UNORM_SRGB,
	ZENITH_RHI_FORMAT_ASTC_4X4_UNORM,
	ZENITH_RHI_FORMAT_ASTC_4X4_UNORM_SRGB,
	ZENITH_RHI_FORMAT_R16_UNORM,         // appended: GEOM's terrain height tiles
	ZENITH_RHI_FORMAT_COUNT              // enumerators are appended before COUNT, never renumbered (PSO keys and the precache file store them)
};
```

```cpp
// Zenith_FormatFeatures.h (generated; public root header of ZenithRHI)
#pragma once
#include "Zenith_RHITypes.h"

/// @contract
/// Ownership: a constexpr table with static storage; nothing is allocated.
/// Thread affinity: any thread; the table is immutable.
/// Invalidation: never; the table changes only with a rebuild.
/// Units: none; m_uFeatures is a Zenith_RHIFormatFeatureBit mask.
/// Errors: none; a device that lacks a non-optional row is refused at creation with ZENITH_RHI_ERROR_PROFILE_UNSUPPORTED.
/// Version: regenerated from FormatFeatures.table; a change is a profile change.
/// @endcontract
enum Zenith_RHIFormatFeatureBit : uint32
{
	ZENITH_RHI_FORMAT_FEATURE_SAMPLED            = 1u << 0,   // S
	ZENITH_RHI_FORMAT_FEATURE_FILTER_LINEAR      = 1u << 1,   // L
	ZENITH_RHI_FORMAT_FEATURE_COLOUR_ATTACHMENT  = 1u << 2,   // C
	ZENITH_RHI_FORMAT_FEATURE_BLEND              = 1u << 3,   // B
	ZENITH_RHI_FORMAT_FEATURE_STORAGE_WRITE      = 1u << 4,   // W
	ZENITH_RHI_FORMAT_FEATURE_STORAGE_TYPED_LOAD = 1u << 5,   // T
	ZENITH_RHI_FORMAT_FEATURE_DEPTH_STENCIL      = 1u << 6,   // D
};

struct Zenith_RHIFormatFeature
{
	Zenith_RHIFormat m_eFormat;
	uint32 m_uVkFormat;      // the VkFormat value
	uint32 m_uDxgiFormat;    // the DXGI_FORMAT value
	uint32 m_uFeatures;      // Zenith_RHIFormatFeatureBit mask
	bool m_bOptional;
	uint8 m_uPlatformMask;   // bit 0 Windows, bit 1 Android; 3 (both bits) when the row has no platform= token
};

inline constexpr Zenith_RHIFormatFeature axZENITH_RHI_FORMAT_FEATURES[] = { /* one entry per table line, in file order */ };
constexpr uint32 Zenith_GetRequiredFormatFeatures(Zenith_RHIFormat eFormat);   // 0 for a format not in the table
```

- [Z2-N-REND-040] `Zenith_FormatFeatures.h` is the one compiled form of the format-feature table: the Vulkan and D3D12 device checks, the graph's format check and the bindless probe read `axZENITH_RHI_FORMAT_FEATURES` and nothing else.
- [Z2-N-REND-041] `Zenith_RHIFormat` is the one format enumeration of ZenithRHI and Flux: its enumerators are `ZENITH_RHI_FORMAT_<DXGI_FORMAT name without the DXGI_FORMAT_ prefix>`, or for a format D3D12 lacks `ZENITH_RHI_FORMAT_<VkFormat name without VK_FORMAT_ and _BLOCK, upper case>` (the ASTC formats), plus `ZENITH_RHI_FORMAT_UNDEFINED`, every Flux resource description, attachment-format set and PSO key stores a `Zenith_RHIFormat`, and no `Flux_Format` type exists.
- [Z2-N-REND-042] The format-feature tests are `Test_FormatFeatures_ProfileMatchesHeader` in `Zenith2/Engine/ZenithRHI_Vulkan/Tests/` (it reads the backend's `Internal/Profiles/VP_ZENITH2_baseline.json` and includes `Zenith_FormatFeatures.h`, and fails when the `formats` section and the header differ), `Test_FormatFeatures_DeviceRefusesMissingFeature` in `Zenith2/Engine/ZenithRHI_Vulkan/Tests/` and `Zenith2/Engine/ZenithRHI_D3D12/Tests/` (a mocked capability report lacking one required bit is refused), `Test_FormatFeatures_GraphRejectsUngrantedUse` in `Zenith2/Engine/ZenithFlux/Tests/Graph/`, and the probe case `Test_BindlessProbe_StorageFormats`.

## Binding model

### Binding overview

Zenith2 binds fully bindlessly with one indirection per draw or dispatch (Z2-D-054): global heaps for sampled textures, storage textures and buffers, a static sampler table, and one 8-byte push or root constant per draw or dispatch that locates the pass's parameter struct in the per-frame upload ring.

<!-- informative -->
Zenith bound descriptors per draw through `Flux_ShaderBinder`, with per-subsystem descriptor layouts generated from reflection and committed under `Flux/Shaders/Generated/`. Every layout had to agree with its shader, every pass bound its own sets, and a binding mismatch surfaced at runtime. Zenith2 removes the layout entirely: a pass's parameter struct holds heap indices, and the per-draw state is two 32-bit words.
<!-- /informative -->

### Descriptor index spaces

<!-- elaboration -->
| Space (`Flux_DescriptorSpace`) | Holds | Vulkan descriptor type | D3D12 descriptor | Slang resource types reachable |
|---|---|---|---|---|
| `FLUX_DESCRIPTOR_SPACE_TEXTURE_SRV` | every sampled texture view: 2D, 2D array, 3D, cube, cube array, depth-as-colour views | `SAMPLED_IMAGE` | SRV | `Texture2D<T>`, `Texture2DArray<T>`, `Texture3D<T>`, `TextureCube<T>`, `TextureCubeArray<T>` |
| `FLUX_DESCRIPTOR_SPACE_TEXTURE_UAV` | every storage-texture view (one mip per view) | `STORAGE_IMAGE` | UAV | `RWTexture2D<T>`, `RWTexture2DArray<T>`, `RWTexture3D<T>` |
| `FLUX_DESCRIPTOR_SPACE_BUFFER_SRV` | read-only buffer views: byte-address, structured | `STORAGE_BUFFER` (NonWritable) | SRV (raw or structured) | `ByteAddressBuffer`, `StructuredBuffer<T>` |
| `FLUX_DESCRIPTOR_SPACE_BUFFER_UAV` | read-write buffer views | `STORAGE_BUFFER` | UAV (raw or structured) | `RWByteAddressBuffer`, `RWStructuredBuffer<T>` |

- [Z2-N-REND-050] Every shader-visible resource is reached through a heap index into one of the four descriptor index spaces or through the static sampler table; no pass declares a descriptor set, descriptor table or root descriptor of its own.
- [Z2-N-REND-051] A texture view in `FLUX_DESCRIPTOR_SPACE_TEXTURE_UAV` covers exactly one mip level (all array layers of that mip), so a UAV field names one mip and the graph tracks it at mip granularity.
- [Z2-N-REND-052] Descriptor writes into a heap slot happen on the `Flux.BuildGraph` node after compile, once the frame's `Flux_UploadRing::ExtendReservation` has returned `FITS` or `EXTENDED` (the transient views of the frame's graph, order 5 of `Zenith2/Docs/Rendering/GpuResources.md` "Build-node upload order"; a deferred or failed frame writes none), or on the creating thread at resource creation, never inside a pass lambda or a `Flux.Record` job, and a slot is written at most once between allocation and retirement; record jobs read the indices already written.

### Vulkan heap realisation

The Vulkan heap layout depends on D-SLANG (open, Stage 1), which selects how Slang lowers `DescriptorHandle<T>` on Vulkan (see "Decision D-SLANG").

| D-SLANG (open, Stage 1) alternative | Set 0 layout |
|---|---|
| 1: mutable descriptors in the profile | binding 0: `VK_DESCRIPTOR_TYPE_MUTABLE_EXT` array covering all four spaces (space base offsets as in the D3D12 realisation); binding 1: the immutable sampler array |
| 2: custom per-kind typed-array lowering | binding 0: `SAMPLED_IMAGE[TEXTURE_SRV capacity]`; binding 1: `STORAGE_IMAGE[TEXTURE_UAV capacity]`; binding 2: `STORAGE_BUFFER[BUFFER_SRV capacity]`; binding 3: `STORAGE_BUFFER[BUFFER_UAV capacity]`; binding 4: `SAMPLER[16]` immutable |
| 3: `uint` indices plus generated accessors | identical to alternative 2 |

- [Z2-N-REND-053] Whatever D-SLANG (open, Stage 1) rules, the Vulkan backend creates exactly one descriptor set layout with `VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT`, every array binding flagged `PARTIALLY_BOUND | UPDATE_AFTER_BIND | UPDATE_UNUSED_WHILE_PENDING`, one descriptor set allocated from it at device creation, and one global `VkPipelineLayout` (that set plus one `VkPushConstantRange{VK_SHADER_STAGE_ALL, 0, 8}`) shared by every pipeline.
- [Z2-N-REND-054] The global descriptor set is bound once per command buffer per bind point (graphics and compute) by the graph's command-list partition prologue; no pass lambda binds it.

### D3D12 heap realisation

The D3D12 backend creates one shader-visible `CBV_SRV_UAV` descriptor heap sized to the sum of the four space capacities, with the spaces laid out contiguously in the order of the index-space table, and one global root signature.

| Root parameter | Kind | Register | Content |
|---|---|---|---|
| 0 | 32-bit constants ×2 | `b0, space0` | `Flux_PushConstants`: the entry point's `uniform` parameter, whose constant buffer Slang places at this register (checked through reflection, [Z2-N-REND-998]) |
| 1 | 32-bit constants ×1 | `b1, space0` | the draw ID (see "Draw ID on Vulkan and D3D12"), declared in `Zenith2/Shaders/Common/Params.slang` beside `Flux_PushConstants` for the D3D target only, the one `register` of that module |
| static samplers ×16 | static sampler | `s0`–`s15, space0` | the static sampler table |

- [Z2-N-REND-055] The D3D12 global root signature carries the flags `D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED` and `D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT`, and every PSO on D3D12 uses it; the space base offsets are compile-time constants the shader compiler emits into `Zenith2/Shaders/Common/Bindless.slang` per capability tier.
- [Z2-N-REND-056] A heap index as seen by shader code is space-local on both APIs; the D3D12 space base offset is added inside the `Bindless.slang` accessors (`Flux_TextureSRV<T>(uIndex)`, `Flux_TextureUAV<T>(uIndex)`, `Flux_BufferSRV(uIndex)`, `Flux_BufferUAV(uIndex)`), so parameter structs are byte-identical across backends.

### Static sampler table

<!-- elaboration -->
| Index constant | Filter | Address mode | Other |
|---|---|---|---|
| `uFLUX_SAMPLER_POINT_CLAMP` = 0 | point, point mip | clamp | |
| `uFLUX_SAMPLER_POINT_WRAP` = 1 | point, point mip | wrap | |
| `uFLUX_SAMPLER_LINEAR_CLAMP` = 2 | linear, linear mip | clamp | |
| `uFLUX_SAMPLER_LINEAR_WRAP` = 3 | linear, linear mip | wrap | |
| `uFLUX_SAMPLER_LINEAR_MIRROR` = 4 | linear, linear mip | mirror | |
| `uFLUX_SAMPLER_ANISO_WRAP` = 5 | anisotropic, max 16 desktop / 8 mobile | wrap | material default |
| `uFLUX_SAMPLER_ANISO_CLAMP` = 6 | anisotropic, as above | clamp | |
| `uFLUX_SAMPLER_SHADOW_LESS_EQUAL` = 7 | linear comparison | clamp | compare `LESS_OR_EQUAL`, standard Z |
| 8 | reserved; created as `POINT_CLAMP` | clamp | reserved slot (shadow views are always standard Z, so shadow comparison is `LESS_OR_EQUAL` alone) |
| `uFLUX_SAMPLER_LINEAR_CLAMP_NO_MIP` = 9 | linear, max LOD 0 | clamp | LUT sampling |
| `uFLUX_SAMPLER_POINT_CLAMP_BORDER_BLACK` = 10 | point | clamp to border (0,0,0,0) | |
| `uFLUX_SAMPLER_LINEAR_CLAMP_BORDER_WHITE` = 11 | linear | clamp to border (1,1,1,1) | shadow atlas outside a tile |
| 12–15 | reserved; created as `POINT_CLAMP` | clamp | reserved slots |

- [Z2-N-REND-057] Samplers are the sixteen static entries of the table above, identical on both APIs (Vulkan immutable samplers in the global layout, D3D12 static samplers in the global root signature); shaders select one through its `uFLUX_SAMPLER_*` index, and no runtime-created sampler exists.
- [Z2-N-REND-058] Changing the static sampler table is a change to the global pipeline layout and root signature, and therefore bumps the shader blob container version and invalidates every persistent pipeline cache (see "Pipeline state objects").

### Descriptor slot allocation

```cpp
class Flux_DescriptorAllocator   // ZenithFlux/Internal/Bindless; one per device
{
public:
	Zenith_Result<Zenith_RHIDescriptorHandle, Flux_DescriptorError> Allocate(Flux_DescriptorSpace eSpace);
	void Free(Zenith_RHIDescriptorHandle xHandle, Flux_DescriptorSpace eSpace, const Flux_QueueLastUse& xLastUse);   // deferred
	void RetireCompleted(const Flux_QueueCompletedValues& xCompleted);   // called at frame begin
	uint32 GetOccupancy(Flux_DescriptorSpace eSpace) const;
};
```

- [Z2-N-REND-059] A freed descriptor slot is placed on its space's pending list with the per-queue last-use values of its resource and is returned to the free list after every queue's timeline has passed those values (the retirement rule of "GPU lifetime and synchronisation"), and not before.
- [Z2-N-REND-060] `Allocate` on a full index space returns `FLUX_DESCRIPTOR_ERROR_SPACE_FULL`, increments the `Flux.Descriptors.Exhausted` counter and never evicts or reuses a live slot.

### The push constant

`Flux_PushConstants` is declared `[FluxShared]` in `Zenith2/Shaders/Common/Params.slang`; the C++ block below is its generated form in `Flux_SharedTypes.generated.h` (see `Zenith2/Docs/Rendering/ShaderSystem.md` "Shared GPU structs").

```cpp
// generated
struct Flux_PushConstants
{
	uint32 m_uParamsBufferIndex;   // FLUX_DESCRIPTOR_SPACE_BUFFER_SRV index of the upload-ring page holding the params
	uint32 m_uParamsOffset;        // byte offset of the parameter struct inside that page
};
static_assert(sizeof(Flux_PushConstants) == 8);
```

```slang
// Zenith2/Shaders/Common/Params.slang
[FluxShared] struct Flux_PushConstants { uint m_uParamsBufferIndex; uint m_uParamsOffset; };

T Flux_LoadParams<T>(Flux_PushConstants xPush)
{
	return Flux_BufferSRV(xPush.m_uParamsBufferIndex).Load<T>(xPush.m_uParamsOffset);
}
```

```slang
// Every entry point takes the push constant as its one entry-point uniform parameter (illustrative excerpt)
[shader("compute")]
[numthreads(8, 8, 1)]
void GTAOMain(uint3 xDispatchThreadID : SV_DispatchThreadID, uniform Flux_PushConstants xPush)
{
	const GTAOParams xParams = Flux_LoadParams<GTAOParams>(xPush);
	// ...
}
```

<!-- informative -->
The Slang user guide (shader-slang.org; in the Slang repository `docs/user-guide/a2-01-spirv-target-specific.md` → "Push Constants") states that a `uniform` parameter in an entry point's parameter list whose type is ordinary data is translated to a push constant in SPIR-V, with std430 layout, and that Vulkan admits at most one push-constant block per entry point. For D3D12 the same parameters form the entry point's scope of shader parameters, whose ordinary bytes Slang places in a constant buffer reported by `EntryPointReflection::getVarLayout` (`09-reflection.md` → "Entry Point Parameters"); the root signature decides that this constant buffer is fed by 32-bit root constants (`09-targets.md` → "Direct3D 12"). So one API-neutral declaration serves both backends, and no `vk::` attribute is needed.
<!-- /informative -->

- [Z2-N-REND-061] Every draw and every dispatch receives exactly one `Flux_PushConstants` (8 bytes), declared as the one `uniform` parameter of every entry point (an entry point declares no other ordinary-data `uniform` parameter); Slang lays it out as the entry point's push-constant block on Vulkan (`VK_SHADER_STAGE_ALL`, offset 0) and as the entry point's constant buffer on D3D12, which the global root signature binds as the 32-bit root constants of root parameter 0, and no other push or root constant exists except the D3D12 draw ID of root parameter 1 (Z2-D-054).
- [Z2-N-REND-062] A shader reads its parameter struct exclusively through `Flux_LoadParams<T>(xPush)` at entry-point start, where `xPush` is the entry point's `uniform Flux_PushConstants` parameter and `T` is the generated params type named by the program's reflection.
- [Z2-N-REND-998] No `vk::` attribute or intrinsic appears in any Slang source or Slang snippet (Z2-D-141): push and root constants are entry-point `uniform` parameters, never a global `[vk::push_constant]` declaration, and resources are reached through the bindless accessors of `Common/Bindless.slang` and, where a program declares one, `ParameterBlock` or `ConstantBuffer` with Slang's reflection-driven layout, never an explicit `vk::binding`. The shader compiler reads the push constant's placement from the program's reflection (`EntryPointReflection::getVarLayout`: offset 0 of the push-constant range on Vulkan, `b0, space0` on D3D12) and fails a program whose placement differs from the global layout's; `ShaderChecks.NoVulkanAttributes` (`Zenith2/Docs/Rendering/ShaderSystem.md`) fails any `vk::` token.

### Parameter structs in the upload ring

- [Z2-N-REND-063] A parameter struct is written into the current frame's upload ring at a 16-byte-aligned offset, with a maximum size of `uFLUX_MAX_PARAMS_BYTES` = 2,048 bytes; the shader compiler fails a program whose parameter struct exceeds it with `ZSC_ERROR_PARAMS_TOO_LARGE`.
- [Z2-N-REND-064] Parameter structs use a scalar layout identical on both APIs (Slang scalar layout on Vulkan, byte-address loads on D3D12, C++ natural alignment in the generated struct), and each generated header asserts every field offset with `static_assert(offsetof(...))`.
- [Z2-N-REND-065] A parameter struct is written once per pass at record time (compute and fullscreen passes) or once per `BeginDrawParams` call in a raster lambda, and is immutable after its push constant has been recorded.

### DescriptorHandle usage

Resource fields in a parameter struct are Slang `DescriptorHandle<T>` values, subject to D-SLANG (open, Stage 1). Each such field is 8 bytes on both sides of the interface.

```slang
// Zenith2/Shaders/Passes/GTAO/GTAO.slang (excerpt)
[FluxParams]
struct GTAOParams
{
	DescriptorHandle<Texture2D<float>> m_xDepth;                 // SRV, compute
	DescriptorHandle<Texture2D<float2>> m_xNormals;              // SRV, compute
	[FullCoverage] DescriptorHandle<RWTexture2D<float>> m_xAO;   // UAV, compute, full-coverage first write
	float4x4 m_xInvProjection;
	float2 m_xInvExtent;
	float m_fRadius;
	uint m_uSliceCount;
};
```

```cpp
// ZenithRHI public root header Zenith_RHIShaderInterface.h: the shader-interface value types (Z2-N-ARCH-083)
struct Zenith_RHIDescriptorHandle
{
	uint32 m_uIndex;      // space-local heap index
	uint32 m_uReserved;   // 0, except a material-constants handle carrying the virtual-texture tag ([Z2-N-REND-068])
};
static_assert(sizeof(Zenith_RHIDescriptorHandle) == 8);

template<typename TTag> struct Zenith_RHISRV { uint32 m_uWord0; uint32 m_uWord1; };   // 8 bytes, see the state table
template<typename TTag> struct Zenith_RHIUAV { uint32 m_uWord0; uint32 m_uWord1; };   // 8 bytes
```

| Field state | `m_uWord0` | `m_uWord1` |
|---|---|---|
| Setup (virtual graph handle, written by `SRV()`/`UAV()`) | graph-local resource index | bits 0–15: view index into the pass's view table; bits 16–31: resource version |
| Record (patched by the graph) | space-local heap index | 0 |

- [Z2-N-REND-066] A `Zenith_RHISRV<T>` or `Zenith_RHIUAV<T>` field holds a virtual graph handle from setup until record and a real descriptor from record onward; the graph patches every resource field listed in the struct's field table before the struct is copied into the upload ring, and a field left unset at setup is a graph compile error (`FLUX_RG_ERROR_FIELD_UNSET`).
- [Z2-N-REND-067] The tag type `T` of `Zenith_RHISRV<T>`/`Zenith_RHIUAV<T>` names the Slang resource type (`Zenith_RHITex2D`, `Zenith_RHITex2DArray`, `Zenith_RHITex3D`, `Zenith_RHITexCube`, `Zenith_RHITexCubeArray`, `Zenith_RHIByteBuffer`, `Zenith_RHIStructBuffer<TElem>`), and the graph rejects a view whose dimension or element stride does not match the tag with `FLUX_RG_ERROR_VIEW_TYPE_MISMATCH`.
- [Z2-N-REND-068] Descriptor handles that are not graph-tracked (persistent read-only resources reached through GPU scene tables or material constants) are written as `Zenith_RHIDescriptorHandle` values holding real heap indices, and exclusively resources registered `Flux_PersistentReadOnly` are referenced that way (see "GPU scene tables and pools"). A material-constants handle may carry the virtual-texture tag: word 0 (`m_uIndex`) stays a real `TEXTURE_SRV` heap index, the VT's tail texture, so a tag-blind reader samples the tail, and word 1 (`m_uReserved`) carries the VT's slot in bits 0–11, zeros in bits 12–27 and 0xA in bits 28–31 (`Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Material handle encoding"); a shader reads both words through `uint2 Flux_HandleWords<T>(DescriptorHandle<T>)` of `Zenith2/Shaders/Common/Bindless.slang`, whose lowering the D-SLANG (open, Stage 1) ruling fixes.

### Draw ID on Vulkan and D3D12

Indirect draws get their per-command data through a draw ID that indexes the culling-produced per-draw record (instance range and material slot).

| API | Source of the draw ID | Value |
|---|---|---|
| Vulkan | `DrawIndex` built-in (`SPV_KHR_shader_draw_parameters`, core in 1.2 as the `shaderDrawParameters` feature and in the profile), exposed in Slang as `SV_DrawIndex` | the command index within the current `vkCmdDrawIndexedIndirectCount` (0 for direct draws) |
| D3D12 | root parameter 1 (the root constant at `b1, space0`, declared in `Common/Params.slang`; `Common/DrawID.slang` declares no binding), written per command by `ExecuteIndirect` through a `D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT` argument | the command index within the bucket, written by the culling shader (0 for direct draws, set by the raster context) |

```cpp
// Both structs are declared [FluxShared] in Zenith2/Shaders/Common/DrawID.slang and generated into Flux_SharedTypes.generated.h.
struct Flux_IndirectDrawIndexed        // one command, 24 bytes, identical layout on both APIs
{
	uint32 m_uDrawIndex;              // D3D12 root constant b1; Vulkan reads from offset + 4 and ignores it
	uint32 m_uIndexCount;
	uint32 m_uInstanceCount;
	uint32 m_uFirstIndex;
	int32 m_iVertexOffset;
	uint32 m_uFirstInstance;          // 0 in every command
};
static_assert(sizeof(Flux_IndirectDrawIndexed) == 24);

struct Flux_DrawRecord                 // one per indirect command, 16 bytes
{
	uint32 m_uVisibleInstanceBase;    // first entry of this draw in the visible-instance list
	uint32 m_uPrimitiveIndex;         // GPU scene primitive table index
	uint32 m_uMaterialSlot;           // GPU scene material-slot table index
	uint32 m_uFlags;                  // FLUX_DRAW_RECORD_FLAG_*: alpha-tested bucket, two-sided, skinned
};
static_assert(sizeof(Flux_DrawRecord) == 16);
```

```slang
// Zenith2/Shaders/Common/DrawID.slang (declares no binding: the D3D12 draw-ID root constant at b1, space0 is declared in
// Common/Params.slang, beside Flux_PushConstants)
uint Flux_DrawIndex();   // Vulkan: SV_DrawIndex; D3D12: Params.slang's root constant b1
uint Flux_DrawRecordIndex(uint uBucketRecordBase) { return uBucketRecordBase + Flux_DrawIndex(); }
uint Flux_InstanceIndex(Flux_DrawRecord xRecord, uint uInstanceID) { return Flux_VisibleInstances()[xRecord.m_uVisibleInstanceBase + uInstanceID]; }
```

- [Z2-N-REND-069] Every indirect draw command uses the 24-byte `Flux_IndirectDrawIndexed` layout on both APIs: Vulkan issues `vkCmdDrawIndexedIndirectCount` (or `vkCmdDrawIndexedIndirect` for the non-compacted transparent commands) with the argument offset advanced by 4 bytes and a stride of 24; D3D12 uses one global command signature for every indirect indexed draw, a `CONSTANT` argument (root parameter 1, one 32-bit value) followed by a `DRAW_INDEXED` argument with stride 24, plus one global signature for indirect dispatch and one for indirect mesh-task dispatch (stride 12 each).
- [Z2-N-REND-070] `m_uDrawIndex` equals the command's index within its bucket, so `Flux_DrawIndex()` returns the same value on both APIs, and `m_uFirstInstance` is 0 in every command so that `SV_InstanceID` is the same zero-based index on both APIs.
- [Z2-N-REND-071] A shader locates its draw record as `Flux_DrawRecordIndex(m_uBucketRecordBase)`, where `m_uBucketRecordBase` is a parameter-struct field the pass lambda sets per bucket through `BeginDrawParams`, and locates each instance through the visible-instance list; no per-draw data travels in a vertex stream.
- [Z2-N-REND-072] Direct (non-indirect) draws see `Flux_DrawIndex()` equal to 0: on Vulkan by definition, on D3D12 because `Flux_RasterContext` sets root parameter 1 to 0 before every direct draw.

### The raster vertex-stream exception

- [Z2-N-REND-073] Raster passes fetch vertex data through fixed-function vertex streams whose layout is declared in Slang (see "Vertex data format"), bound exclusively through the `Flux_VertexStreamRef`/`Flux_IndexBufferRef` entries the pass declared in its `Flux_RasterTargets`; this is the one exception to bindless access, and it exists so that the fetch unit decodes the compressed formats.
- [Z2-N-REND-074] Compute passes that read or write mesh data (skinning, the visibility-buffer resolve, GPU scene work) reach the same buffers through `FLUX_DESCRIPTOR_SPACE_BUFFER_SRV`/`BUFFER_UAV` views and decode them with `Zenith2/Shaders/Common/VertexFormats.slang`, under GEOM's decode-route consistency rule (`Zenith2/Docs/Design/GEOM/VertexFormat.md → "Decode-route consistency"`): passes that must agree bit for bit on a vertex use one route, all fixed-function or all manual, because no API specifies the fetch unit's conversion bit for bit, and the fetch unit's agreement with the manual route is measured per format and per device by `Test_VertexFormat_FetchUnitParity`.

### What the binding model excludes

- [Z2-N-REND-075] Zenith2 has no per-pass descriptor sets, no per-pass pipeline layouts or root signatures, no layout objects to keep in sync with shaders, and no "bind bindless" call in any public or pass-context API.
- [Z2-N-REND-076] The binding-model tests `Test_Binding_PushConstantSize`, `Test_Binding_DrawIndexParity` (the same indirect bucket yields identical draw-record lookups on Vulkan and D3D12), `Test_Binding_NullSlotZero`, `Test_Binding_SlotRetirementAcrossFrames` and `Test_Binding_ViewTypeMismatchRejected` exist and pass on WARP and lavapipe.

## Shader system and offline compilation

The full specification is `Zenith2/Docs/Rendering/ShaderSystem.md`, headings "Shader source layout", "The ZenithShaderCompiler tool", "Slang authoring vocabulary", "Generated parameter headers", "Reflection JSON", "Shader blob container", "Permutations", "Shader lints" and "Watch mode and editor reload".

- [Z2-N-REND-100] Slang is the source of truth for every shader program, parameter struct, vertex layout and permutation, and every C++ parameter struct is generated from it by `ZenithShaderCompiler`; no hand-written C++ mirror of a shader struct exists (Z2-D-015).
- [Z2-N-REND-101] Shaders are compiled offline exclusively, by the separate `ZenithShaderCompiler` process, at build time through a CMake custom command or in its file-watch mode during editor iteration; no game, editor or Player process compiles Slang, HLSL or any shader source (Z2-D-025).
- [Z2-N-REND-104] The one exemption from [Z2-N-REND-101] is the Stage 1 harness `Zenith_Bench_BindlessProbe` (`Zenith2/Benchmarks/BindlessProbe/`), which compiles its probe shaders under `Zenith2/Benchmarks/BindlessProbe/Shaders/` at build time with the pinned `slangc` host tool, because it runs before `ZenithShaderCompiler` exists; it is never a runtime or shipped target, every other test shader is compiled from its Slang source by `ZenithShaderCompiler` as a build step, and the ticket that lands `ZenithShaderCompiler` routes the probe's shaders through it and removes this exemption.
- [Z2-N-REND-102] The runtime loads precompiled SPIR-V and DXIL blobs from the shader blob container, and driver ISA compilation happens at PSO creation through PSO precaching, never on a first draw.
- [Z2-N-REND-103] Every permutation of every program is enumerated and compiled offline; a permutation that content references and the cooked set lacks is a cook-time error, never a runtime compile.

## Pipeline state objects

The full specification is `Zenith2/Docs/Rendering/ShaderSystem.md`, headings "PSO keys", "PSO precache sets", "PSO creation schedule", "Persistent pipeline caches", "Missing-PSO fallback" and "PSO counters and tests".

- [Z2-N-REND-190] A PSO is keyed by (shader program, permutation, raster state, attachment formats), and the render graph supplies the attachment formats from the pass's `Flux_RasterTargets`.
- [Z2-N-REND-191] PSOs are created ahead of use from each game's recorded and predicted PSO sets, at load and asynchronously during streaming, with a Vulkan pipeline cache and a D3D12 PSO library persisted per driver version.
- [Z2-N-REND-192] A PSO missing at draw time uses the defined fallback (shipping: the draw, or with indirect draws the whole material bucket, is skipped for that frame; development: a magenta fallback PSO draws it), increments the miss counters, and never causes a synchronous compile.

## Materials

### Material model

A material is a Slang surface function plus a material document (`*.material.json`) holding its parameters and texture references. There is no material node graph; materials are authored in code, with previews (Z2-D-079).

- [Z2-N-REND-240] A material consists of exactly two artefacts: a Slang module declaring one type that conforms to `Flux_MaterialType`, and a `*.material.json` document naming that module, that type, its domain and its parameter values; no other material representation exists at runtime or in the editor.
- [Z2-N-REND-241] Every shading path evaluates materials through the single-source `EvaluateMaterial` and lights them through the single-source `EvaluateLighting` in `Zenith2/Shaders/Lighting/`; no path carries its own copy of material or BRDF code (Z2-D-035).

### Surface function contract

<!-- elaboration -->
```slang
// Zenith2/Shaders/Common/Material.slang
struct Flux_UVGradients { float2 m_xDdx; float2 m_xDdy; };

struct Flux_VTBinding                                            // Stage 4 (Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Material input binding")
{
	DescriptorHandle<Texture2DArray<float4>> m_xPhysicalSRGB;    // the pass's patched field on m_xVTPhysical, sRGB view
	DescriptorHandle<Texture2DArray<float4>> m_xPhysicalLinear;  // the pass's patched field, UNORM view
	DescriptorHandle<Texture2D<uint>> m_xPageTable;              // the pass's patched field on m_xVTPageTable
	uint m_uMaxAnisotropy;
	float m_fFeedbackBias;
	uint m_uFeedbackFlags;                                       // bit 0: the pass records requests; bit 1: the pass binds the VT pools
	uint m_uRecordBufferSRV;                                     // FLUX_DESCRIPTOR_SPACE_BUFFER_SRV index of the VT record buffer
	uint m_uFeedbackJitter;
};

struct Flux_MaterialInput
{
	float3 m_xPositionWS;
	float3 m_xViewDirWS;            // unit, surface to camera
	float3 m_xNormalWS;             // unit, interpolated vertex normal (n = cross(t, b) from the QTangent)
	float3 m_xTangentWS;            // unit
	float3 m_xBitangentWS;          // unit, handedness sign applied
	float2 m_xUV0;
	float2 m_xUV1;
	Flux_UVGradients m_xUV0Grad;    // explicit: hardware derivatives in raster, analytic in the visibility-buffer resolve
	Flux_UVGradients m_xUV1Grad;
	half4 m_xVertexColour;
	bool m_bFrontFace;
	uint m_uInstanceIndex;
	float m_fTimeSeconds;
	Flux_VTBinding m_xVirtual;      // EvaluateMaterial hands it to Sampling.slang's per-invocation state before the material type runs
};

struct Flux_SurfaceData
{
	half3 m_xAlbedo;
	half m_fOpacity;
	float3 m_xNormalWS;             // unit, after normal mapping
	half m_fRoughness;              // perceptual, [0,1]
	half m_fMetallic;
	half m_fAO;                     // material AO, multiplied with SSAO at lighting
	half m_fSpecular;               // dielectric F0 scale, 0.5 = 4% reflectance
	half3 m_xEmissive;              // linear, pre-exposure
	uint m_uShadingModel;           // bits 0–1 base model, bits 2–3 surface-model flags (see "Advanced surface models")
	half m_fClearCoat;              // Stage 5 advanced surfaces from here on; each defaults to 0, so a default-lit material is unchanged
	half m_fClearCoatRoughness;     // perceptual
	half m_fAnisotropy;             // [-1, 1]
	half2 m_xAnisotropyDirection;   // tangent space
	half3 m_xSheenColour;           // linear
	half m_fSheenRoughness;
	half3 m_xSubsurfaceColour;
	half m_fThickness;              // [0, 1]
};

struct Flux_VertexModifierInput
{
	float3 m_xPivotWS;                         // the instance's local-to-world translation
	float3 m_xPositionWS;                      // the world position before modification
	half4 m_xVertexColour;
	float2 m_xUV0;                             // dequantised UV0 (the null stream's zero when the stream-set has none)
	uint m_uCustomData;                        // the instance record's m_uCustomData
	Env_FrameEnvironment m_xFrameEnvironment;  // ENV's [FluxShared] block of the instance's world (Common/Environment.slang); zeroed without the Environment module
};

interface Flux_MaterialType
{
	associatedtype Params;                                                     // the material constants struct
	static Flux_SurfaceData Evaluate(Flux_MaterialInput xIn, Params xParams);
	static half EvaluateOpacity(Flux_MaterialInput xIn, Params xParams);       // depth-only and alpha-tested passes
	static const bool bHAS_VERTEX_MODIFIER;                                    // false for a type without a vertex modifier
	static float3 ModifyPositionWS(Flux_VertexModifierInput xIn, Params xParams, float fTimeSeconds);   // returns m_xPositionWS unchanged when bHAS_VERTEX_MODIFIER is false
};

extern struct Flux_ActiveMaterial : Flux_MaterialType;   // bound per permutation by link-time specialisation

Flux_SurfaceData EvaluateMaterial(Flux_MaterialInput xIn, uint uMaterialSlot);
```

- [Z2-N-REND-242] A material type's `Evaluate` and `EvaluateOpacity` are pure functions of their inputs: they read textures through the handles in their `Params`, sample exclusively with explicit gradients, and write nothing (no UAV, no groupshared, no discard), except the one write reachable from a material module's sampling, the public mutator `Flux_VTRecordRequest` of the Slang module `VirtualTexture`, whose invocation state (the stored binding, the writing flag and the request reservoir) is declared `internal` so that no other module can name it (`Sampling.slang` and `Material.slang` import `VirtualTexture`, which imports neither); `EvaluateMaterial` owns the one `discard` of an `[AlphaTest]` variant, driven by `EvaluateOpacity` against the material's alpha cutoff.
- [Z2-N-REND-243] Material selection inside a program is link-time specialisation of `Flux_ActiveMaterial`, so every (program, material type) pair is its own compiled permutation; the permutation key carries the material type ID.
- [Z2-N-REND-244] `Flux_SurfaceData` is the one hand-off from material evaluation to lighting in every path: Forward+ and Forward pass it in registers, Deferred encodes it into the G-buffer, and the visibility buffer's material resolve writes it into the same G-buffer-style targets.
- [Z2-N-REND-996] `Flux_SampleMaterial` branches to `Flux_SampleVirtual` when a handle carries the virtual-texture tag and the pass's pools-bound flag (bit 1 of `m_uFeedbackFlags`) is set, and otherwise samples word 0 as an ordinary texture. Every pass whose programs evaluate materials of the `opaque` or `alphaTest` domains (every pass with an alpha-test permutation or material variants: `Prepass`, `DeferredDepthPrepass`, `ShadowDepth`, `VisibilityRaster`, `ForwardPlusOpaque`, `ForwardOpaquePerObject`, `GBuffer`, `MaterialResolve`) declares the three `[Optional]` VT pool fields; the four colour passes (`ForwardPlusOpaque`, `ForwardOpaquePerObject`, `GBuffer`, `MaterialResolve`) add the feedback fields `m_xVTFeedback`, `m_uVTFeedbackStride` and `m_uVTFeedbackJitter`, and the three pixel passes among them carry `[earlydepthstencil]`; the transparent and decal passes declare none. `Flux_ViewTargets::m_xVTFeedback` carries a view's feedback buffer, and every path function calls `Flux_AddVTFeedbackClear` before a feedback view's first writer and `Flux_AddVTFeedbackReadback` after its last (`Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Pass tables").

### Explicit gradients

- [Z2-N-REND-245] Every texture sample in a material module uses `Flux_SampleMaterial(xTexture, uSampler, xUV, xGrad)` (a `SampleGrad` wrapper), and the ShaderChecks gate step fails a material module that calls `Sample`, `SampleBias` or any other implicit-derivative sampling function (lint `ShaderChecks.MaterialExplicitGradients`).
- [Z2-N-REND-246] Raster paths fill `Flux_MaterialInput` gradients from `ddx`/`ddy` of the interpolated UVs at pixel-shader entry, before any divergent branch; the visibility-buffer resolve fills them analytically from the triangle's barycentric derivatives (see "Frame anatomy Visibility buffer").

### Material document

<!-- elaboration -->
```json
{
	"schemaVersion": 1,
	"id": "2f6a0d7e-8c1b-4e4f-9d4a-3b7f2a9c11e0",
	"module": "Zenith2/Shaders/Materials/Standard.slang",
	"type": "Material_Standard",
	"domain": "opaque",
	"twoSided": false,
	"alphaCutoff": 0.5,
	"blend": "none",
	"permutations": { "bPERM_DETAIL_NORMAL": false },
	"parameters": { "m_xBaseColourFactor": [1.0, 1.0, 1.0, 1.0], "m_fRoughnessFactor": 0.8, "m_fMetallicFactor": 0.0 },
	"textures": { "m_xBaseColour": "7c1d...-guid", "m_xNormal": "a93e...-guid", "m_xORM": "0b44...-guid" }
}
```

| Key | Type | Meaning |
|---|---|---|
| `schemaVersion` | integer | the material document schema version; migrated per `Zenith2/Docs/Contracts.md → "Compatibility policy per kind"` |
| `id` | GUID string | stable authoring ID |
| `module` | repo-relative path | the Slang module declaring the material type; engine materials under `Zenith2/Shaders/Materials/`, game materials under `Zenith2/Games/<Name>/Assets/Materials/` |
| `type` | identifier | the `Flux_MaterialType` type in `module` |
| `domain` | `opaque` · `alphaTest` · `transparent` · `decal` | the material class; selects buckets and passes |
| `twoSided` | bool | disables back-face culling for this material's PSOs; reported by the PSO lint |
| `alphaCutoff` | float | `alphaTest` domain: opacity below it discards |
| `blend` | `none` · `alpha` · `premultiplied` · `additive` | `transparent` domain blend state |
| `permutations` | object | values of the module's material-level `bPERM_*`/`uPERM_*` constants |
| `parameters` | object | values of plain fields of the type's `Params` struct, by field name |
| `textures` | object | texture asset GUIDs for the `DescriptorHandle` fields of `Params`, by field name |

- [Z2-N-REND-247] A material document is an authoring document: it carries a schema version and migration chain, preserves unknown fields on round-trip with a warning, and is written through the journaled atomic write, as `Zenith2/Docs/Contracts.md → "Compatibility policy per kind"` specifies for material definitions (Z2-D-048).
- [Z2-N-REND-248] The cook validates every material document against its module's reflected `Params` struct: an unknown parameter or texture name, a type mismatch, a missing texture for a non-optional handle field, or a permutation value outside the declared value set is a cook error naming the document, the key and the expected type.

### Material classes and buckets

| Domain | Depth-writing passes | Colour passes | Discard | Culling |
|---|---|---|---|---|
| `opaque` | opaque bucket (drawn first) | equal-depth test, no discard | none | back faces unless `twoSided` |
| `alphaTest` | alpha-tested bucket (drawn after the opaque bucket), `[AlphaTest]` variant | equal-depth test, no discard | depth-writing passes only | back faces unless `twoSided` |
| `transparent` | none (reads depth) | transparent forward, sorted back to front | none | back faces unless `twoSided` |
| `decal` | none | clustered decals (Forward+, visibility buffer), deferred decals (Deferred), per-object decals (Forward) | none | not applicable |

- [Z2-N-REND-249] A material's domain fixes the buckets and passes that draw it as the table above lists, and the early-Z ordering of "Frame anatomy common prologue" (opaque bucket, then alpha-tested bucket, then equal-test colour passes without discard) applies to every depth-writing pass.

### Material constants and the material-slot table

- [Z2-N-REND-250] At material load, the material's `Params` values (plain data plus real `Zenith_RHIDescriptorHandle` values of its `Flux_PersistentReadOnly` textures) are written once into the material-constants buffer, which is itself `Flux_PersistentReadOnly`, and the material's material-slot record (see "GPU scene tables and pools") points at them.
- [Z2-N-REND-251] A shader reaches material constants exclusively through the material-slot record of its draw (`Flux_DrawRecord::m_uMaterialSlot`), so rebinding a material changes one table record and no PSO.
- [Z2-N-REND-252] A material parameter edit in the editor rewrites the material constants through a reallocate-and-swap scatter upload (the texture streaming swap mechanism of "GPU scene tables and pools"), never by writing a constants buffer another in-flight frame reads.

### Material permutations and limits

- [Z2-N-REND-253] The permutation count per program is the product of its program-level permutation constants, its material types, the material-level permutation values content uses, and the vertex stream-sets it draws; the cook reports this count per game, and `Z2-B-RPROD-024` Shader permutation limit per game (pending) caps it, failing the cook when exceeded.

### Material previews

- [Z2-N-REND-254] The editor previews a material through a `VIEW_KIND_PREVIEW` view rendering the Forward path against a preview scene; a material module change reaches the preview through the watch-mode reload protocol of "Shader system and offline compilation", and a `Params` layout change is reported as needing a C++-free re-cook of the material constants, not a C++ rebuild, because material constants are data.

### Material tests

- [Z2-N-REND-255] The material tests `Test_Material_DocumentRoundTrip`, `Test_Material_UnknownFieldPreserved`, `Test_Material_CookRejectsTypeMismatch`, `Test_Material_CookRejectsUnknownPermutationValue`, `Test_Material_SlotRebindNoPSOChange` and the lint fixture `Test_ShaderChecks_MaterialImplicitDerivative_Fails` exist and pass.

### Advanced surface models

GEOM owns the surface data and the lobe definitions (`Zenith2/Docs/Design/GEOM/Materials.md → "Advanced surface models"`); LIGHT integrates the lobes into `EvaluateLightingEx`.

| `m_uShadingModel` bits | Value |
|---|---|
| 0–1 | the base model: `FLUX_SHADING_MODEL_DEFAULT_LIT` 0, `FLUX_SHADING_MODEL_UNLIT` 1, `FLUX_SHADING_MODEL_SUBSURFACE` 2, `FLUX_SHADING_MODEL_CLOTH` 3 |
| 2 | `FLUX_SURFACE_MODEL_FLAG_CLEAR_COAT` |
| 3 | `FLUX_SURFACE_MODEL_FLAG_ANISOTROPY` |
| 4–7 | zero |

- [Z2-N-REND-256] `Flux_SurfaceData` carries the eight half-precision advanced-surface fields of the surface function contract, each defaulting to 0 so that a default-lit material is unchanged, and `m_uShadingModel` uses exactly the bit assignment of the table above, the 8-bit value G-buffer GB2's alpha channel stores ([Z2-N-REND-842]).
- [Z2-N-REND-257] On Forward+ and Forward the shading model is a link-time property of the material type, so the colour program's lighting takes the extended branch only in variants whose type declares it and no program permutation is added; on Deferred and the visibility buffer the extended parameters travel in the G-buffer target GB4 of `Zenith2/Docs/Rendering/PerPass/Deferred.md` "GBuffer", written only by the advanced-surfaces permutation.

### Material vertex modifier

A material type may displace vertices in world space (foliage wind, water waves) through `ModifyPositionWS`. Every mesh vertex program positions its vertices through GEOM's `Zenith2/Shaders/Common/MeshVertex.slang`, which applies the modifier, so no pass program calls it itself; `Material_Foliage`, `Material_DetailFoliage` and `Material_Water` are its first users (`Zenith2/Docs/Design/ENV/Foliage.md → "Material_Foliage"`).

<!-- elaboration -->
| Element | Rule |
|---|---|
| declaration | every material type declares `bHAS_VERTEX_MODIFIER`; a type that declares it `false` returns `xIn.m_xPositionWS` unchanged from `ModifyPositionWS`, and the compiler folds the call away |
| permutation | `bHAS_VERTEX_MODIFIER` is a property of the material type, so it is part of the material variant ([Z2-N-REND-243]); a material without a modifier costs nothing |
| callers | both prepass phases, the forward opaque passes, shadow depth, transparent forward, and from Stage 5 the G-buffer pass and the visibility raster: every mesh vertex program of every bucket |
| velocity | a program that writes velocity calls the modifier twice: with the current time and transform, and with the previous frame's time (`Env_WindParams::m_fPrevTimeSeconds` for wind) and the previous transform |
| shadows | shadow views evaluate the modifier with the main view's time, so a displaced surface casts the shadow of the displaced geometry |
| input | `Flux_VertexModifierInput` of the surface function contract: the pivot, the pre-modifier world position, the vertex colour, UV0, the instance custom data, and the instance's world `Env_FrameEnvironment` (the frame environment of `Zenith2/Docs/Rendering/PerPass/Common.md` "Conventions") |

- [Z2-N-REND-258] A material vertex modifier is applied exclusively inside `MeshVertex.slang`'s vertex positioning, in every mesh vertex program listed in the table above, for the current frame and, where the program writes velocity, again with the previous frame's time and transform, and a material type whose `bHAS_VERTEX_MODIFIER` is false leaves positions bit-identical to a program with no modifier.

## Render graph

The full specification is `Zenith2/Docs/Rendering/RenderGraph.md`, headings "Graph lifecycle", "Graph public API", "Resource declaration and versioning", "Pass declaration", "Pass contexts", "Graph compile", "Pass merging", "GPU timestamps", "Frame dump", "Graph errors" and "Graph tests".

- [Z2-N-REND-280] The render graph is built on `ZenithGraph` (see `Zenith2/Docs/Architecture.md → "ZenithGraph shared core"`), is rebuilt every frame in an arena, and caches physical resources across frames by hashing their descriptions (Z2-D-055).
- [Z2-N-REND-281] A disabled feature adds no passes; the graph has no rebuild requests, no placeholder passes and no early-return callbacks.
- [Z2-N-REND-282] Features are free functions over graph handles, such as `Flux_RGTexture Flux_AddSSAO(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_SSAOSettings& xSettings)`, and composition is plain data flow between their returned handles.
- [Z2-N-REND-283] A resource is declared once, in two phases: at setup a pass fills its generated parameter struct with virtual graph handles, at compile the graph walks the struct's field table to derive reads, writes, stages and access, and at record the fields are patched to real descriptor values.

<!-- informative -->
Zenith declared every resource twice (once in the pass's `Reads`/`Writes` chain and again when binding it in the record callback), ordered passes partly by registration order through `FindBestWriter`'s declared-earlier rule, and aliased transients only between same-format slots. Zenith2 derives access from the one declaration the shader already needs, orders purely by versioned data flow, and places transients at arbitrary offsets across formats.
<!-- /informative -->

## Barrier synthesis

The full specification is `Zenith2/Docs/Rendering/RenderGraph.md`, headings "Barrier state machine" and "Barrier emission".

- [Z2-N-REND-380] Barrier synthesis is completely automatic: graphics programmers never write a barrier, a layout transition, a queue-ownership transfer or an aliasing barrier, and the pass contexts expose no command list and no barrier API through which one could be written (Z2-D-056).
- [Z2-N-REND-381] Every access is derived from the parameter struct's field table (SRV, UAV or tile-local, plus the shader stage) and from `Flux_RasterTargets` (attachments, depth read or write, vertex, index and indirect-argument buffers).
- [Z2-N-REND-382] Every graph unit test and every render test runs with Vulkan synchronization validation and D3D12 GPU-based validation enabled and completes with zero validation messages; a new technique lands with no hazard.

## Transient aliasing

The full specification is `Zenith2/Docs/Rendering/RenderGraph.md`, headings "Aliasing placement" and "Aliasing toggle and first-write rule".

- [Z2-N-REND-430] Transient aliasing is on wherever possible: transients are grouped per heap and memory type, sorted largest first and placed first-fit into free lifetime intervals at arbitrary offsets across formats (Z2-D-057).
- [Z2-N-REND-431] `Flux_GraphSettings::m_bAliasTransients` is a runtime toggle reachable from the on-screen debug UI and the console; the next graph compile switches placement, with no restart and no other change to the graph.
- [Z2-N-REND-432] A transient's first access is a clear or a declared full-coverage write, a read of a region not yet written is a graph compile error, and a debug poison-on-first-use mode fills every transient with NaN or a sentinel before its first write.
- [Z2-N-REND-433] The parity suite renders every parity scene with aliasing on and off, poisoning enabled, and requires bit-identical images and clean synchronization validation in both modes.

## GPU lifetime and synchronisation

The full specification is `Zenith2/Docs/Rendering/GpuResources.md`, headings "Queues and timelines", "Retirement", "Frames in flight", "Upload ring", "Readbacks", "Tracked resource kinds" and "GPU memory rules".

- [Z2-N-REND-480] Every queue has a timeline semaphore, and a resource or descriptor slot is retired once every queue's last-use value for it has completed, and not earlier; descriptor slots are reused after that retirement.
- [Z2-N-REND-481] Frames in flight are configurable between 2 and 3.
- [Z2-N-REND-482] The upload ring stalls, on the render-submit thread alone and inside `ReserveFrame`, until space frees, grows up to `Z2-B-RHI-006` Upload ring hard cap and no further, splits a single request larger than the cap or else refuses it with an error, serves worker jobs through queued upload requests that return operation handles, and counts every exhaustion in a telemetry counter; the build node extends the frame's reservation to its compiled exact need with `ExtendReservation` and never waits, so a need the ring cannot serve without a wait defers the frame, and a need above the cap fails the frame (see `Zenith2/Docs/Rendering/GpuResources.md` "Frame deferral" and "Frame failures").
- [Z2-N-REND-483] Readbacks are operation handles (see `Zenith2/Docs/Contracts.md → "Handle kinds"`), owned by the requester, resolved after fence completion, with the lifetime "GPU lifetime and synchronisation" documents.

## GPU scene tables and pools

The full specification is `Zenith2/Docs/Rendering/GpuResources.md`, headings "GPU scene tables", "Scatter upload", "Table growth", "Persistent resources", "Pools", "Texture streaming swap" and "GPU scene tests".

- [Z2-N-REND-530] Indirectly reachable resources follow the UE5 GPU Scene model: a small fixed set of persistent table buffers (instance, primitive, material-slot, light) is imported into the graph every frame as external buffers, and passes read them through ordinary parameter-struct fields (Z2-D-058).
- [Z2-N-REND-531] The CPU never writes table memory directly: updates are queued as records in the frame's upload ring and applied by a scatter-upload graph pass that declares a write on the table and runs before any consumer.
- [Z2-N-REND-532] Everything a table references is registered `Flux_PersistentReadOnly`, stays in a steady readable state, and cannot hand out a UAV or render-target view to any pass.
- [Z2-N-REND-533] Dynamic content reaches its consumers through pools (skinned-vertex, particle, procedural-geometry, VT physical/page-table), which are ordinary graph resources written by a declared producer pass and read by declared consumer passes; tables stay read-only.

## Render views

### What a render view is

A render view is one independent "render the scene from here" request. It is the unit a feature function receives, and the unit that per-view state belongs to (Z2-D-059). Views are not per-player: local multiplayer and split-screen are excluded (Z2-D-076).

### Flux_View

```cpp
struct Flux_View
{
	Flux_ViewID m_xViewID;                     // stable identity; per-view persistent state is keyed on it across frames
	Flux_ViewKind m_eKind;                     // selects the pass set
	Flux_ViewCamera m_xCamera;
	Flux_DepthMode m_xDepthMode;
	Flux_ViewExtent m_xExtent;
	Flux_ViewOutputs m_xOutputs;
	uint32 m_uFeatureMask;                    // Flux_ViewFeature bits: which passes run
	Flux_QualityTier m_eQualityTier;           // per-platform tier
	Flux_QualityOverrides m_xQualityOverrides; // per-view overrides of tier settings
	Flux_ShadingPath m_eShadingPath;           // resolved by Flux_SelectShadingPath
	Flux_ViewPersistentState* m_pxPersistent;  // owned by Flux_ViewRegistry, never by the view
	Flux_CameraLightingSettings m_xCameraLighting;   // LIGHT's type: copied from the view's request by Flux_BuildViewList
};

struct Flux_ViewCamera
{
	Zenith_Matrix4 m_xView;
	Zenith_Matrix4 m_xProjection;                 // unjittered
	Zenith_Matrix4 m_xJitteredProjection;
	Zenith_Matrix4 m_xPrevView;
	Zenith_Matrix4 m_xPrevProjection;             // unjittered
	Zenith_Vector2 m_xJitterNDC;
	Zenith_Vector2 m_xPrevJitterNDC;
	float m_fNear;
	float m_fFar;                              // +infinity allowed for reversed-Z infinite projections
	Flux_ProjectionKind m_eProjection;         // FLUX_PROJECTION_PERSPECTIVE, FLUX_PROJECTION_ORTHOGRAPHIC
	bool m_bCameraCut;                         // true: every temporal history of this view restarts this frame
};

struct Flux_ViewExtent
{
	uint32 m_uOutputWidth;
	uint32 m_uOutputHeight;
	float m_fDynamicResolutionScale;           // (0,1]; render extent = ceil(output * scale), floor from Budgets
	uint32 m_uRenderWidth;
	uint32 m_uRenderHeight;
	Flux_Viewport m_xViewport;                 // sub-rectangle of the output, in output pixels
};

struct Flux_ViewOutputs
{
	Flux_ViewOutputKind m_eKind;               // FLUX_VIEW_OUTPUT_SWAPCHAIN, _TEXTURE, _CUBEMAP_FACE, _SHADOW_ATLAS_TILE
	Flux_TextureHandle m_xTexture;             // for _TEXTURE and _CUBEMAP_FACE
	uint32 m_uCubeFace;
	Flux_AtlasTile m_xAtlasTile;               // for _SHADOW_ATLAS_TILE
};
```

| Field | Meaning |
|---|---|
| `m_xViewID` | a stable identity; per-view persistent state is keyed on it across frames |
| `m_eKind` | the view kind (below), which selects the pass set |
| `m_xCamera` | view and projection matrices, previous-frame matrices, jitter, near/far planes, the camera-cut flag |
| `m_xDepthMode` | the Z direction and the depth format, two independent axes (see "Depth mode") |
| `m_xExtent` | output resolution, dynamic-resolution scale, render resolution, viewport |
| `m_xOutputs` | the target the view produces: the swapchain, a texture, a cubemap face, a shadow-atlas tile |
| `m_uFeatureMask` | which passes run: a shadow view runs culling and depth, nothing else |
| `m_eQualityTier`, `m_xQualityOverrides` | the per-platform tier and any per-view overrides |
| `m_pxPersistent` | per-view persistent state: the TAA/upscaler history, the previous frame's HiZ pyramid, the exposure history, SSR's previous colour, the velocity history, the previous visibility bits, the froxel fog scattering history, and from Stage 5 the SSGI history |
| `m_xCameraLighting` | LIGHT's per-camera lighting settings (exposure, depth of field, grading), copied from `Flux_ViewRequest::m_xCameraLighting`; features read them through `Flux_ViewContext::m_pxView` |

- [Z2-N-REND-590] A `Flux_View` is built by `Flux_BuildViewList` each frame and is immutable for the rest of that frame's graph build; features read it through `Flux_ViewContext` and never modify it.
- [Z2-N-REND-591] The render extent is `ceil(output × m_fDynamicResolutionScale)` per axis, the scale never goes below the dynamic-resolution floor of `Zenith2/Docs/Budgets.md → "Frame-rate definitions and measurement procedure"`, and every pass before the TAA/upscaler runs at render extent while every pass from the upscaler on runs at output extent.

### Depth mode

```cpp
struct Flux_DepthMode
{
	Flux_ZDirection m_eZDirection;   // FLUX_Z_DIRECTION_STANDARD, FLUX_Z_DIRECTION_REVERSED
	Flux_DepthFormat m_eFormat;      // FLUX_DEPTH_FORMAT_D24S8, FLUX_DEPTH_FORMAT_D32F_S8
	Flux_DepthModeReason m_eReason;  // FLUX_DEPTH_MODE_REASON_DEFAULT, _PRECISION, _CAPABILITY
};
```

| Z direction | Format | When | Depth clear value | Depth test (opaque) | Projection |
|---|---|---|---|---|---|
| standard | D24S8 | every orthographic view and every shadow view, unconditionally; every other perspective view whose precision check passes | 1.0 | `LESS_OR_EQUAL` (prepass), `EQUAL` (colour) | standard [0,1] |
| standard | D32F_S8 | the capability fallback: D24S8 not exposed (for example AMD Vulkan drivers) | 1.0 | as above | standard [0,1] |
| reversed | D32F_S8 | a perspective view that is not a shadow view whose precision check fails, or whose far plane is infinite | 0.0 | `GREATER_OR_EQUAL` (prepass), `EQUAL` (colour) | reversed [1,0], infinite far allowed |

The precision check applies to perspective views. For a standard-Z [0,1] projection with near plane n and far plane f, one D24 step at view depth z is `Δz(z) = 2⁻²⁴ · z² · (f − n) / (n · f) ≈ z² / (n · 2²⁴)`, largest at the far plane, `Δz(f) = 2⁻²⁴ · f · (f − n) / n`. A 2 km view gives Δz(f) = 4.77 m at n = 0.05 m, 2.38 m at n = 0.1 m, 1.19 m at n = 0.2 m and 0.4767 m at n = 0.5 m. Orthographic projections are linear in depth, so D24 gives a uniform step of `(f − n) / 2²⁴` (about 0.1 mm over 2 km) and they take no precision check.

<!-- elaboration -->
| Near plane | Farthest standard-Z far plane at the proposed tolerance 0.25 m |
|---|---|
| 0.05 m | 458 m |
| 0.1 m | 648 m |
| 0.2 m | 916 m |
| 0.5 m | 1,448 m |

- [Z2-N-REND-592] A perspective view that is not a shadow view uses standard Z when its far plane is finite and its worst-case D24 depth step at the far plane, `Δz(f) = 2⁻²⁴ · f · (f − n) / n`, is at most `Z2-B-RPROD-029` D24 depth-precision tolerance (pending; proposed 0.25 m), and otherwise uses reversed Z with D32F_S8 (Z2-D-061); the row's adopted value is strictly below 0.4767 m (the far-plane step of a 2 km view at a 0.5 m near plane), so that a 2 km outdoor main view takes reversed Z for every near plane from 0.05 m to 0.5 m.
- [Z2-N-REND-593] Orthographic views (every directional shadow cascade and every other orthographic view) and every shadow view always use standard Z with D24S8, or D32F_S8 under the capability fallback, and never reversed Z; the precision check is not evaluated for them.
- [Z2-N-REND-594] Depth format follows capability, independently of Z direction: D24S8 where `m_bDepthD24S8` is true, else D32F_S8; reversed-Z views use D32F_S8 in every case; stencil (S8) is present in every depth target.
- [Z2-N-REND-595] Every depth-mode combination runs through the same code: projection matrices, clear values, comparison operators and depth linearisation come from `Flux_DepthModeOps` (`GetClearDepth`, `GetOpaqueCompare`, `GetColourCompare`, `GetInvertedCompare` for back-face volume draws such as deferred decal boxes (`GREATER_OR_EQUAL` for standard Z, `LESS_OR_EQUAL` for reversed Z), `BuildProjection`, `LineariseDepth` on the CPU, and the matching `Zenith2/Shaders/Common/DepthMode.slang` functions), and no other shader or C++ code branches on the depth format or Z direction or names a depth compare operator.
- [Z2-N-REND-596] A D32F_S8 depth target on a view whose `m_eReason` is neither `FLUX_DEPTH_MODE_REASON_PRECISION` nor `FLUX_DEPTH_MODE_REASON_CAPABILITY` is a graph compile error (`FLUX_RG_ERROR_D32_UNJUSTIFIED`), which is the enforcement of the depth-format rule of "GPU performance rules and lints".

### View identity and persistent state

```cpp
struct Flux_ViewID { uint64 m_ulValue; };

Flux_ViewID Flux_MakeViewID(Flux_ViewKind eKind, uint64 ulOwnerID, uint32 uSubIndex);

struct Flux_ViewPersistentState
{
	Flux_TextureHandle m_axTAAHistory[2];        // RGBA16F, ping-pong
	Flux_TextureHandle m_axHiZ[2];               // RG16F linear min/max pyramid, ping-pong
	Flux_BufferHandle m_axVisibilityBits[2];     // 1 bit per GPU-scene instance, ping-pong
	Flux_BufferHandle m_xExposure;               // 16 bytes: current exposure, target, adaptation state
	Flux_TextureHandle m_xSSRPrevColour;         // R11G11B10F
	Flux_TextureHandle m_axVelocityHistory[2];   // RG16F
	Flux_TextureHandle m_axFogScattering[2];     // RGBA16F 3D, the tier's froxel grid, ping-pong (LIGHT)
	Flux_TextureHandle m_axSSGIHistory[2];       // RGBA16F, half render extent, ping-pong (LIGHT, Stage 5)
	uint32 m_uHistoryIndex;                     // which ping-pong half is "current" this frame
	uint32 m_uRenderWidth;                      // the render extent of the last submitted frame, for UV-normalised resampling
	uint32 m_uRenderHeight;
	uint64 m_ulLastUsedFrame;
	bool m_bHistoryValid;
};
```

| View kind | `ulOwnerID` | `uSubIndex` |
|---|---|---|
| `VIEW_KIND_MAIN` | the world's stable ID | 0 |
| `VIEW_KIND_SHADOW` | the light's persistent ID | cascade index, or cube face index 0–5, or 0 for a spot light |
| `VIEW_KIND_EDITOR` | the editor viewport's stable ID | 0 |
| `VIEW_KIND_CAPTURE` | the probe's persistent ID, or the screenshot request ID | cube face index 0–5, or 0 |
| `VIEW_KIND_PREVIEW` | the preview request's asset GUID hash | 0 |
| `VIEW_KIND_TEXTURE` | the render-to-texture camera's persistent entity ID | 0, or the planar-reflection plane index |

- [Z2-N-REND-597] `Flux_MakeViewID` is a 64-bit hash, selected by D-SEL-HASH (open, Stage 1), of the kind, the owner ID and the sub-index in the table above, so a view's identity is stable across frames, sessions and view-list order.
- [Z2-N-REND-598] `Flux_ViewRegistry` owns every `Flux_ViewPersistentState`, keyed by `Flux_ViewID`; each view's histories are independent, so an editor viewport never reads or writes the main view's TAA history, and no two views share a history resource.
- [Z2-N-REND-599] A view's histories are invalid (`m_bHistoryValid == false`) on the first frame the view exists, on a camera cut, on an output-extent change or a change of a history resource's own extent, and on a shading-path change; every temporal consumer treats an invalid history as "no history" (TAA takes the current frame, HiZ phase 1 treats every instance as visible-last-frame-unknown, exposure starts from the view's configured exposure).
- [Z2-N-REND-612] A render-extent change alone (a dynamic-resolution step) keeps `m_bHistoryValid`: the output-extent histories (TAA colour, exposure), the froxel history (a fixed grid) and the SSR previous colour (sampled by normalised UV) stay valid as they are, each render-extent history (velocity, the SSGI history) is recreated at the new extent when its half next becomes current and its previous half is resampled by normalised UV with the extent recorded in `m_uRenderWidth`/`m_uRenderHeight`, and the previous HiZ pyramid is projected with its own extent.
- [Z2-N-REND-600] Persistent state of a view absent from the view list for `uFLUX_VIEW_STATE_RETIRE_FRAMES` = 60 consecutive frames is released through deferred deletion (the retirement rule of "GPU lifetime and synchronisation").

### View kinds

```cpp
enum Flux_ViewKind : uint8
{
	VIEW_KIND_MAIN,
	VIEW_KIND_SHADOW,
	VIEW_KIND_EDITOR,
	VIEW_KIND_CAPTURE,
	VIEW_KIND_PREVIEW,
	VIEW_KIND_TEXTURE,
	VIEW_KIND_STEREO,   // Stage 7, XR
	VIEW_KIND_COUNT
};
```

| Kind | Used for | Passes run | Shading path | Own histories |
|---|---|---|---|---|
| `VIEW_KIND_MAIN` | the game camera | the full frame for the active shading path (see the Frame anatomy sections) | the active path | yes |
| `VIEW_KIND_SHADOW` | one per shadow cascade, per local shadowed light, or per point-light cube face | GPU culling, then depth. No lighting or post | none (depth only) | HiZ pyramid and visibility bits, where the tier enables shadow HiZ |
| `VIEW_KIND_EDITOR` | each editor viewport (the scene view, the game view, and every extra viewport) | the main-view frame plus editor overlays (gizmos, selection outline, grid) | the active path | yes, each its own |
| `VIEW_KIND_CAPTURE` | reflection-probe and IBL cubemap faces, the environment-lighting bake, screenshots | a reduced frame: no TAA, no post, a fixed exposure | Forward (desktop), Forward+ (mobile) | HiZ pyramid and visibility bits only (no TAA, exposure or SSR history) |
| `VIEW_KIND_PREVIEW` | asset, material and prefab thumbnails, and the material previews in the editor | a small forward frame against a preview scene | Forward | no |
| `VIEW_KIND_TEXTURE` | in-game render-to-texture cameras: security monitors, mirrors, portals, planar reflections | the main frame at reduced quality, into a texture that the main view samples | the active path | yes |
| `VIEW_KIND_STEREO` | Stage 7, XR: left and right eyes that share culling | multi-view rendering | the active path | yes, per eye |

```cpp
enum Flux_ViewFeature : uint32
{
	FLUX_VIEW_FEATURE_HIZ_OCCLUSION     = 1u << 0,
	FLUX_VIEW_FEATURE_SHADOW_RECEIVE    = 1u << 1,
	FLUX_VIEW_FEATURE_SSAO              = 1u << 2,
	FLUX_VIEW_FEATURE_CONTACT_SHADOWS   = 1u << 3,
	FLUX_VIEW_FEATURE_VOLUMETRIC_FOG    = 1u << 4,
	FLUX_VIEW_FEATURE_SSR               = 1u << 5,
	FLUX_VIEW_FEATURE_TRANSPARENT       = 1u << 6,
	FLUX_VIEW_FEATURE_PARTICLES         = 1u << 7,
	FLUX_VIEW_FEATURE_TEMPORAL_AA       = 1u << 8,
	FLUX_VIEW_FEATURE_AUTO_EXPOSURE     = 1u << 9,
	FLUX_VIEW_FEATURE_DEPTH_OF_FIELD    = 1u << 10,
	FLUX_VIEW_FEATURE_MOTION_BLUR       = 1u << 11,
	FLUX_VIEW_FEATURE_BLOOM             = 1u << 12,
	FLUX_VIEW_FEATURE_TONEMAP           = 1u << 13,
	FLUX_VIEW_FEATURE_UI                = 1u << 14,
	FLUX_VIEW_FEATURE_DEBUG_OVERLAY     = 1u << 15,
	FLUX_VIEW_FEATURE_EDITOR_OVERLAY    = 1u << 16,
	FLUX_VIEW_FEATURE_SKY               = 1u << 17,
	FLUX_VIEW_FEATURE_COLOUR            = 1u << 18,   // absent for shadow views
};
```

<!-- elaboration -->
| Kind | Default feature mask |
|---|---|
| `VIEW_KIND_MAIN` | every bit except `EDITOR_OVERLAY`; `DEPTH_OF_FIELD` and `MOTION_BLUR` per camera settings |
| `VIEW_KIND_SHADOW` | `HIZ_OCCLUSION` when the tier enables shadow HiZ; nothing else |
| `VIEW_KIND_EDITOR` | as `VIEW_KIND_MAIN` plus `EDITOR_OVERLAY`; `UI` for the game view |
| `VIEW_KIND_CAPTURE` | `HIZ_OCCLUSION`, `SHADOW_RECEIVE`, `SSAO`, `VOLUMETRIC_FOG`, `SKY`, `TRANSPARENT`, `PARTICLES`, `COLOUR`; no TAA, no post, fixed exposure |
| `VIEW_KIND_PREVIEW` | `SHADOW_RECEIVE`, `SKY`, `TRANSPARENT`, `TONEMAP`, `COLOUR` |
| `VIEW_KIND_TEXTURE` | as `VIEW_KIND_MAIN` minus `UI`, `DEBUG_OVERLAY`, `DEPTH_OF_FIELD`, `MOTION_BLUR`; reduced quality through `m_xQualityOverrides` |

- [Z2-N-REND-601] A view's feature mask is the default of its kind in the table above, intersected with the quality tier's enabled features and the capability matrix of "Shading paths and capability matrix", then adjusted by per-view overrides; a feature whose bit is clear adds no passes for that view.
- [Z2-N-REND-997] A `VIEW_KIND_MAIN`, `VIEW_KIND_EDITOR` or `VIEW_KIND_TEXTURE` view whose effective feature mask holds `FLUX_VIEW_FEATURE_COLOUR` and none of the scene bits (`HIZ_OCCLUSION` to `TONEMAP`, and `SKY`) declares no culling, prepass, opaque, lighting or temporal pass and keeps no history; its frame is a clear of its output and the common epilogue (`Zenith2/Docs/Design/RPROD/Views.md` → "Source world tag and scene-free views").
- [Z2-N-REND-602] `VIEW_KIND_STEREO` is declared now and rejected by `Flux_BuildViewList` with `FLUX_VIEW_ERROR_KIND_UNAVAILABLE` until Stage 7, so the view identity, per-view state and feature-function signatures stay XR-ready without shipping stereo (Z2-D-072).

### View list

```cpp
Zenith_Result<Flux_ViewList, Flux_ViewError> Flux_BuildViewList(
	const Flux_RenderWorld& xWorld,
	const Flux_RendererSettings& xSettings,
	Flux_ViewRegistry& xRegistry,
	Zenith_Arena& xFrameArena);
```

| Order | Views | Derivation |
|---|---|---|
| 1 | the main view (zero or one) | the world's active camera request |
| 2 | shadow views | from the visible lights: directional cascades in cascade order, then local shadowed lights by descending shadow priority, each point light's six faces in face order |
| 3 | editor views | requested viewports, by ascending viewport ID |
| 4 | capture views | requested captures scheduled this frame by the round-robin, by ascending view ID |
| 5 | preview views | requested previews scheduled this frame, by ascending request ID |
| 6 | texture views | requested render-to-texture cameras scheduled this frame, by ascending view ID |

<!-- elaboration -->
Shadowed-light selection is a CPU step of view-list construction: each shadow-casting light that intersects the union of the main, editor and texture view frusta gets a priority of `luminous power × projected solid angle from the nearest such view`, the highest-priority lights up to the tier's shadowed-light cap receive shadow views, and the shadow atlas tiles are allocated on the CPU in the same step (the shadow-atlas allocation producer).

- [Z2-N-REND-603] The view list is built in the order of the table above, with the stated tie-breaks, so that view-list order, pass declaration order and every derived ordering are deterministic for the same render world.
- [Z2-N-REND-604] Shadowed-light selection and shadow-atlas tile allocation happen on the CPU during view-list construction, and the resulting atlas rectangles are written into the light records through the frame's GPU-scene scatter upload before any shadow view renders.

### Shared producers

- [Z2-N-REND-605] The shared producers run once per frame, not once per view: GPU-scene scatter upload, streaming uploads, skinning, particle simulation, procedural geometry, the light and decal list, the shadow-atlas allocation and shadow-view records, the sky LUTs (transmittance, multiple scattering, sky view), and with the Environment module ENV's cloud layer (Stage 4) and its Stage 5 prologue passes; their outputs reach per-view passes through `Flux_FrameShared`.

```cpp
// Engine/ZenithFlux/Flux_ViewContext.h (excerpt)
class Flux_EnvironmentRenderer;   // ENV's GPU side (Zenith2/Docs/Design/ENV.md), owned by Flux_Renderer
struct Flux_ParticleFrame;        // VFX's frame (Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_Particles header")
struct Flux_UIFrame;              // UI's frame (Zenith2/Docs/Design/UI/ServicesAPI.md → "Flux_UI header")
struct Flux_Sprite2DFrame;        // TWOD's frame (Flux_Sprite2D.h)
class Flux_VideoSystem;           // CINE's video system (Flux_Video.h)
class Flux_MainViewCapture;       // RPROD's development main-view capture

struct Flux_FrameShared
{
	Flux_RGBuffer m_xInstanceTable;
	Flux_RGBuffer m_xPrimitiveTable;
	Flux_RGBuffer m_xMaterialSlotTable;
	Flux_RGBuffer m_xLightTable;
	Flux_RGBuffer m_xSkinnedCurrent;         // skinned-vertex pool, this frame
	Flux_RGBuffer m_xSkinnedPrevious;        // skinned-vertex pool, previous frame
	Flux_RGBuffer m_xParticlePool;
	Flux_RGBuffer m_xParticleSorted;         // the sorted (key, slot) pairs of ParticleSort, read by Particles
	const Flux_ParticleFrame* m_pxParticles = nullptr;   // VFX's frame; null when particles are not enabled or the frame has no particle work
	Flux_RGBuffer m_xProceduralPool;
	Flux_RGTexture m_xVTPhysical;
	Flux_RGTexture m_xVTPageTable;
	Flux_RGBuffer m_xLightList;              // frame-visible lights, compacted
	Flux_RGBuffer m_xDecalList;
	Flux_RGTexture m_xShadowAtlas;           // after every shadow view has rendered; unset when no light is shadowed
	Flux_RGTexture m_xSkyViewLUT;
	Flux_RGTexture m_xTransmittanceLUT;      // 256 × 64 RGBA16F, persistent-imported, recomputed on an m_uSkyRevision change
	const Flux_LightingFrame* m_pxLighting;  // LIGHT's frame, set by the renderer before any feature function runs
	Flux_RGBuffer m_xShadowViewRecords;      // the frame chunk holding the shadow-view records (ImportFrameChunk), located by m_uShadowViewBase
	Flux_RGTexture m_xMultiScatterLUT;       // 32 × 32 RGBA16F, persistent-imported, recomputed with the transmittance LUT
	Flux_RGTexture m_xCloudLayer;            // ENV's Env_AddCloudLayer output, 192 × 108 R11G11B10F; unset without it (Stage 4)
	Flux_RGTexture m_xCloudTransmittance;    // R8_UNORM, with the cloud layer (Stage 4)
	Flux_RGTexture m_xCloudShadowMap;        // ENV's Env_AddCloudShadowMap output, 512² R8_UNORM; unset without it (Stage 5)
	Flux_RGTexture m_xIBLCubeArray;          // the environment cube array, persistent-imported, marked output on frames that write it (Stage 4)
	Flux_RGBuffer m_xIBLIrradiance;          // the sky irradiance SH, persistent-imported (Stage 4)
	Flux_RGBuffer m_xProbeRecords;           // Flux_ReflectionProbeRecord × resident probes, the frame chunk, rebuilt every frame (Stage 4)
	Flux_EnvironmentRenderer* m_pxEnvironment;   // ENV's GPU side, set by Flux_Renderer before any feature function runs when the Environment module enabled it; null otherwise (Stage 3)
	const Flux_UIFrame* m_pxUIFrame;         // UI's frame (Flux_UIRenderer::GetFrame), set by the renderer before any feature function runs when the UI module enabled it (Flux_Renderer::EnableUI); null otherwise (Stage 3)
	const Flux_Sprite2DFrame* m_px2DFrame;   // TWOD's frame, set before any feature function runs; null when 2D is not enabled
	Flux_VideoSystem* m_pxVideo;             // CINE's video system, set before any feature function runs; null when video is not enabled
	Flux_MainViewCapture* m_pxMainViewCapture;   // development builds: the main-view capture; null when not enabled
};
```

- [Z2-N-REND-995] `Flux_FrameShared::m_pxEnvironment`, `Flux_FrameShared::m_pxUIFrame`, `Flux_FrameShared::m_px2DFrame` and `Flux_FrameShared::m_pxVideo` are the only routes from a path or feature function to ENV's, UI's, TWOD's and CINE's GPU sides (a process-wide pointer is forbidden, [Z2-N-CONTRACT-149], Z2-D-046): `Flux_Renderer` sets each before any feature function of the frame runs, each to null when its module did not enable it (`Zenith2/Docs/Design/RPROD/Renderer.md` → "Feature systems enabled after Create"), and every ENV entry point (`Env_AddCloudLayer`, `Env_AddUnderwater`, `Env_AddVolumetricClouds`, `Env_AddCloudShadowMap`, `Env_AddOceanFFT`) and UI pass function (`Flux_AddUIComposite`, `Flux_AddUIWorldSpace`) adds no pass when its pointer is null.

### Per-view feature functions and Flux_ViewContext

```cpp
struct Flux_ViewContext
{
	const Flux_View* m_pxView;
	const Flux_FrameShared* m_pxShared;
	const Flux_QualitySettings* m_pxQuality;   // the view's tier with overrides applied; its m_xLight is LIGHT's Flux_LightQualitySettings
	Flux_RGTexture m_xPrevHiZ;                 // imported from the view's persistent state
	Flux_RGBuffer m_xPrevVisibilityBits;
	Flux_RGTexture m_xPrevTAAHistory;
	Flux_RGTexture m_xPrevSSRColour;
	Flux_RGBuffer m_xExposure;
	Flux_RGTexture m_xPrevVelocity;            // the previous velocity half; unset when the history is invalid
	Flux_RGTexture m_xPrevFogScattering;       // the previous froxel scattering half; unset when the history is invalid
	Flux_RGTexture m_xPrevSSGI;                // the previous SSGI half (Stage 5); unset when the history is invalid
	uint32 m_uSourceWorldTag;                 // the world tag of the section whose request made the view; 0xFFFFFFFF for a shadow view
};

struct Flux_ViewTargets   // the per-view handles the path function threads between features
{
	Flux_RGTexture m_xDepth;
	Flux_RGTexture m_xVelocity;
	Flux_RGTexture m_xNormals;          // RG16_UNORM octahedral (Forward+/Forward prepass)
	Flux_RGTexture m_xRoughness;        // R8_UNORM (Forward+/Forward prepass)
	Flux_RGTexture m_xHiZ;
	Flux_RGBuffer m_xVisibilityBits;
	Flux_RGTexture m_xAO;
	Flux_RGTexture m_xContactShadows;
	Flux_RGTexture m_xFogVolume;
	Flux_RGBuffer m_xClusters;
	Flux_RGTexture m_axGBuffer[4];      // Deferred and visibility buffer
	Flux_RGTexture m_xVisibilityID;     // visibility buffer
	Flux_RGTexture m_xSceneColour;      // HDR R11G11B10F
	Flux_RGTexture m_xSpecularWeight;   // R11G11B10F, render extent (Forward+/Forward); unset without FLUX_VIEW_FEATURE_SSR
	Flux_RGBuffer m_xVTFeedback;        // the view's virtual-texture feedback buffer (Stage 4); unset for a view that writes no feedback
	Flux_RGTexture m_xOutput;
};

void Flux_BuildView(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets);   // dispatches on m_eShadingPath
```

- [Z2-N-REND-606] Per-view feature functions are called once per view, with that view's `Flux_ViewContext`, and add that view's passes to the same frame graph as every other view, so the graph schedules across all views together and aliases their transients against each other.
- [Z2-N-REND-607] Dependencies between views are explicit graph edges carried by resource handles: a texture view's output handle is read by the main view's material pass, and the shadow atlas handle is read by the lighting passes; no view reads another view's resource except through a handle the graph can see.

### View budgets

- [Z2-N-REND-608] The number of views per frame is capped per quality tier by `Z2-B-RPROD-022` View count per frame per quality tier (pending), and texture views and captures by `Z2-B-RPROD-023` Texture views and captures per frame (pending); views beyond a cap are not rendered that frame and are counted in `Flux.Views.Deferred`.
- [Z2-N-REND-609] Texture views and captures are round-robined across frames: each has a requested update interval in frames, and the scheduler picks, each frame, the requests whose interval has elapsed in ascending order of (frames overdue descending, view ID ascending) up to the tier cap.

### View delivery staging

- [Z2-N-REND-610] Stage 2 delivers view identity, per-view resources and independent per-view histories (needed by shadow views and editor previews from Stage 3); Stage 5 adds advanced multi-view (many simultaneous captures and probes, stereo-ready paths).

### Render view tests

- [Z2-N-REND-611] The view tests `Test_View_IDStableAcrossListOrder`, `Test_View_EditorHistoryIndependentOfMain` (an editor viewport's TAA leaves the main view's history bit-identical), `Test_View_CameraCutResetsHistory`, `Test_View_OutputExtentChangeResetsHistory` (an output-extent change invalidates every history; a dynamic-resolution step from 1.0 to 0.9 keeps `m_bHistoryValid`), `Test_View_PersistentStateRetired`, `Test_View_DepthModePrecisionCheck` (analytic cases either side of the tolerance; a 2 km view with near planes 0.05 m, 0.2 m and 0.5 m takes reversed Z with D32F_S8; an orthographic 2 km cascade and a perspective spot-light shadow view take standard Z with D24S8), `Test_View_D32UnjustifiedRejected`, `Test_View_ListOrderDeterministic` and `Test_View_TextureViewEdgeToMain` exist and pass.

## Hierarchical-Z occlusion culling

The full specification is `Zenith2/Docs/Rendering/Culling.md`, headings "Pyramid format", "Pyramid construction", "Occlusion test", "Two-phase culling", "Indirect draw compaction", "Meshlet culling", "Shadow view culling", "Culling diagnostics" and "Culling tests".

- [Z2-N-REND-630] Two-phase HiZ occlusion culling is present in every shading path and runs for every main, editor and texture view, inside the shared GPU-scene pipeline after frustum culling (Z2-D-060).
- [Z2-N-REND-631] The HiZ pyramid is RG16F, holding linear view-space depth: the max (farthest) channel, rounded up, drives occlusion culling; the min (nearest) channel, rounded down, drives SSR's hierarchical tracing.
- [Z2-N-REND-632] Occlusion never lags a frame: phase 1 tests last frame's visible instances against last frame's unmodified pyramid with last frame's view-projection, and phase 2 tests every instance not drawn in phase 1 against this frame's pyramid.
- [Z2-N-REND-633] Culling writes compacted indirect draw arguments with a count buffer, per material bucket and pipeline, so a culled object never reaches the vertex or pixel stage and the CPU never sees per-object visibility.

## Shading paths and capability matrix

### The four shading paths

| Path | Role | Platforms | Geometry stage | Lighting stage |
|---|---|---|---|---|
| Forward+ (clustered) | the default; the one path on Android | Windows (Vulkan, D3D12), S24 | depth/velocity/normal prepass, two-phase | clustered forward shading in the opaque pass |
| Forward (non-clustered) | the parity reference; previews and captures | Windows | the Forward+ prepass | per-object light lists (at most N lights per object) |
| Deferred | desktop alternative | Windows | depth prepass, then a G-buffer pass | compute lighting over tiles and clusters |
| Visibility buffer | desktop alternative | Windows | meshlet culling and a 32-bit visibility ID raster | per-material compute resolve into `SurfaceData`, then compute lighting |

```cpp
enum Flux_ShadingPath : uint8
{
	FLUX_SHADING_PATH_FORWARD_PLUS,
	FLUX_SHADING_PATH_FORWARD,
	FLUX_SHADING_PATH_DEFERRED,
	FLUX_SHADING_PATH_VISIBILITY_BUFFER,
	FLUX_SHADING_PATH_COUNT
};

Flux_ShadingPath Flux_SelectShadingPath(Flux_ViewKind eKind, const Flux_RendererSettings& xSettings, Zenith_RHICapabilityTier eTier);
```

- [Z2-N-REND-690] Zenith2 provides exactly four shading paths behind `Flux_ShadingPath`, with clustered Forward+ as the default; Forward, Deferred and visibility buffer are supported behind the capability matrix below (Z2-D-035).
- [Z2-N-REND-691] `Flux_SelectShadingPath` returns `FLUX_SHADING_PATH_FORWARD_PLUS` for every view on `ZENITH_RHI_CAPABILITY_TIER_MOBILE`; on desktop it returns `FLUX_SHADING_PATH_FORWARD` for `VIEW_KIND_PREVIEW` and `VIEW_KIND_CAPTURE`, and `Flux_RendererSettings::m_eShadingPath` for main, editor and texture views. Until the Forward path exists (Stage 5) it returns `FLUX_SHADING_PATH_FORWARD_PLUS` for desktop `VIEW_KIND_CAPTURE` and `VIEW_KIND_PREVIEW` views, and from the Stage 5 Forward-path implementation on it returns `FLUX_SHADING_PATH_FORWARD` for them.
- [Z2-N-REND-692] The active path is switchable at runtime on desktop through the `r.shadingPath` console value and the on-screen debug UI; the switch takes effect at the next frame's graph build and invalidates the temporal histories of every affected view (see "Render views").
- [Z2-N-REND-693] Every path shares the single-source `EvaluateMaterial` and `EvaluateLighting` with explicit gradients; the paths differ in where `Flux_SurfaceData` lives between those two calls and in how light lists are built, never in the material or BRDF mathematics.

### Single-source lighting

<!-- elaboration -->
```slang
// Zenith2/Shaders/Lighting/EvaluateLighting.slang
struct Flux_LightingInput
{
	float3 m_xPositionWS;
	float3 m_xViewDirWS;
	float2 m_xScreenUV;
	float m_fLinearDepth;
	half m_fScreenSpaceAO;         // 1 when SSAO is off
	half m_fContactShadow;         // 1 when contact shadows are off
	uint2 m_xPixel;                // render-extent pixel
	float3 m_xGeometricNormalWS;   // unit interpolated vertex normal (shadow normal offset, clear coat)
	half3 m_xIndirectIrradiance;   // irradiance (lux, not pre-exposed) from Flux_EvaluateIndirectDiffuse; the sky SH when nothing else applies
	uint m_uLightingChannels;      // the instance's lighting-channel mask (instance record m_uLayerMask bits 24–31)
	uint m_uFrameIndex;            // the frame index, for noise
	float m_fPreExposure;          // this frame's pre-exposure multiplier
};

struct Flux_LightingOutput
{
	half3 m_xRadiance;             // pre-exposed outgoing radiance: direct + environment specular + indirect diffuse + emissive
	half3 m_xEnvSpecularWeight;    // the environment specular weight SSR composites with
	half3 m_xEnvSpecular;          // the pre-exposed probe or sky specular that SSR replaces where it hits
};

struct Flux_DirectionalLight          // one directional entry of the frame light list
{
	Flux_LightRecord m_xRecord;
	uint m_uRoles;                    // bit 0 the sun role, bit 1 the moon role
};

struct Flux_LightingAtmosphere        // what directional evaluation needs besides the light records
{
	Texture2D<float4> m_xTransmittanceLUT;    // Flux_FrameShared::m_xTransmittanceLUT
	Texture2D<float> m_xCloudShadowMap;       // the pass's [Optional] m_xCloudShadowMap (slot 0's null substitute when unset)
	Flux_AtmosphereGPU m_xAtmosphere;         // Common/Media.slang
	float4 m_xCloudShadowMapping;             // origin x, origin z, 1 / extent (0: no map), outside transmittance
};

interface Flux_LightSource             // how a path enumerates the lights affecting a shading point
{
	uint GetLightCount(Flux_LightingInput xIn);
	Flux_LightRecord GetLight(Flux_LightingInput xIn, uint uIndex);
	uint GetDirectionalCount(Flux_LightingInput xIn);                          // at most 2
	Flux_DirectionalLight GetDirectional(Flux_LightingInput xIn, uint uIndex);
	Flux_LightingAtmosphere GetAtmosphere(Flux_LightingInput xIn);
	uint GetProbeCount(Flux_LightingInput xIn);                                // Stage 4 reflection probes
	Flux_ReflectionProbeRecord GetProbe(Flux_LightingInput xIn, uint uIndex);
};

struct Flux_ClusterLightSource : Flux_LightSource { /* froxel cluster lists: Forward+, Deferred, visibility buffer, clustered transparent forward */ };
struct Flux_PerObjectLightSource : Flux_LightSource { /* per-object list: Forward */ };

Flux_LightingOutput EvaluateLightingEx<TLights : Flux_LightSource>(Flux_SurfaceData xSurface, Flux_LightingInput xIn, TLights xLights);
half3 EvaluateLighting<TLights : Flux_LightSource>(Flux_SurfaceData xSurface, Flux_LightingInput xIn, TLights xLights);   // EvaluateLightingEx(…).m_xRadiance
```

The members, the evaluation order and the BRDF are LIGHT's (`Zenith2/Docs/Design/LIGHT/Lighting.md → "The EvaluateLighting contract"`). Both light sources implement the directional members from the frame light list, which is path-independent, and the atmosphere member from the pass's fields; froxel fog is applied by the caller after the call.

- [Z2-N-REND-694] Light enumeration is the one path-specific input to `EvaluateLighting`, supplied through `Flux_LightSource`; directional light, shadows, IBL, AO, fog application and the BRDF are evaluated by the same code in every path.
- [Z2-N-REND-698] `EvaluateLighting` keeps its three-argument signature and returns `EvaluateLightingEx(…).m_xRadiance`, and `EvaluateLightingEx` is a pure function of its three arguments (no UAV, no groupshared, no discard, no derivative instruction), so pixel and compute stages call the same code.

### Capability matrix

The matrix states, per feature, whether each path provides it on each platform, and how the parity suite (see "Validation roles") compares it. "Win" is Windows on Vulkan and D3D12 alike unless a row says otherwise.

<!-- elaboration -->
| Feature | Forward+ Win | Forward+ S24 | Forward Win | Deferred Win | Visibility buffer Win | Parity comparison |
|---|---|---|---|---|---|---|
| Two-phase HiZ instance culling | yes | yes | yes | yes | yes | draw counts equal across paths |
| Virtual texture sampling (Stage 4) | yes | yes | yes | yes | yes | final image in VT full residency mode |
| Meshlet culling (frustum, cone, HiZ) | no | no | no | no | yes | excluded (path-specific) |
| Mesh shaders | no | no | no | no | when `m_bMeshShaders`, else compute/indirect fallback | fallback compared with mesh-shader output: bit-exact visibility ID |
| Depth/velocity/normal prepass (RG16 normals + R8 roughness) | yes | yes | yes | depth only | no (visibility raster) | depth near-exact |
| Clustered lights (froxel lists) | yes | yes | no | yes | yes | final image, FLIP threshold |
| Per-object light lists (≤ N lights) | no | no | yes | no | no | compared on parity scenes with ≤ N lights per object |
| Clustered decals applied to `SurfaceData` | yes | yes | no | no | yes | surface channels near-exact |
| Deferred decals modifying the G-buffer | no | no | no | yes | no | surface channels, per-feature tolerance (G-buffer quantisation) |
| Per-object decals | no | no | yes | no | no | surface channels, per-feature tolerance |
| Volumetric froxel fog, applied in-shader | yes | yes | yes | yes | yes | final image |
| GTAO + bilateral denoise | yes | yes | yes | yes | yes | AO channel near-exact |
| Screen-space contact shadows (sun) | yes | yes | yes | yes | yes | final image |
| Screen-space GI (SSGI, Stage 5) | yes | no | yes | yes | yes | final image, per-feature tolerance (the SSGI error budget of `Zenith2/Docs/Design/LIGHT/GI.md → "SSGI error budget"`) |
| Cascaded and local shadow maps | yes | yes | yes | yes | yes | shadow-view depth bit-exact |
| IBL | yes | yes | yes | yes | yes | final image |
| Sky and atmosphere | yes | yes | yes | yes | yes | final image |
| SSR (hierarchical trace on HiZ min) | yes | yes | yes | yes (G-buffer normals and roughness) | yes (`SurfaceData`) | final image, per-feature tolerance |
| Transparent forward, sorted, with refraction | yes | yes | yes | yes | yes (never in the visibility buffer) | final image |
| Particles and ribbons (soft) | yes | yes | yes | yes | yes | final image |
| G-buffer | no | no | no | yes | yes (`SurfaceData` targets) | surface channels near-exact |
| Material classification and resolve | no | no | no | no | yes | excluded (path-specific) |
| Velocity | prepass | prepass | prepass | G-buffer pass | reconstructed from the visibility ID | velocity near-exact |
| TAA / temporal upscaler | yes | yes | yes | yes | yes | final image after a fixed sequence |
| MSAA | no | no | no | no | no | not applicable |
| Auto-exposure, DoF, motion blur, bloom, tonemap | yes | yes | yes | yes | yes | final image |
| Tile-local merging | no until D-TBDR (open, Stage 5) | no until D-TBDR (open, Stage 5) | no | no | no | not applicable |
| Terrain (ENV, GEOM terrain geometry) | yes | yes | yes | yes | yes | surface channels near-exact; final image |
| Placed foliage (ENV, instanced meshes with the foliage vertex modifier) | yes | yes | yes | yes | yes | depth near-exact (the modifier runs in every mesh vertex program); final image |
| Detail foliage (ENV, through GEOM's procedural geometry) | yes | yes | yes | yes | yes | final image after a fixed sequence (the generator is seeded per frame index) |
| Water surfaces (ENV) | yes | yes | yes | yes | yes | final image |
| Underwater composite (ENV, `Env_AddUnderwater`) | yes | yes | yes | yes | yes | final image, per-feature tolerance (the pending ENV row "Underwater composite error") |
| Cloud layer (ENV, Stage 4) | yes | yes | yes | yes | yes | final image, per-feature tolerance (the pending ENV row "Cloud layer approximation error") |
| Volumetric clouds and cloud shadows (ENV, Stage 5, desktop high tier) | yes | no | yes | yes | yes | final image, per-feature tolerance (the pending ENV row "Volumetric cloud approximation error") |
| 2D canvas (TWOD sprite and tile canvas, `Sprite2DCanvas` and `Sprite2DComposite`, Stage 3) | yes | yes | yes | yes | yes | final canvas: byte-exact on the pixel-exact subset (opaque, untinted, point-sampled texels, `Zenith2/Docs/Design/TWOD/Rendering.md` → "Pixel exactness rules"); per-feature tolerance 1/255 per channel of the encoded value on pixels covered by a semi-transparent texel, a tint, or the ADDITIVE or MULTIPLY blend (a conforming sRGB encode of a blended linear value may differ by one code between WARP and lavapipe) |

<!-- informative -->
The SSGI row's platform scope (the desktop paths only, and within them the tiers that set `render.quality.ssgiEnabled`: `QUALITY_TIER_WIN_HIGH` and `QUALITY_TIER_WIN_EPIC`) is LIGHT's design (`Zenith2/Docs/Design/LIGHT/GI.md → "SSGI platform scope"`), accepted by the human's ruling Q41 (a) of 2026-10-16: dynamic GI is SSGI on those two tiers, with the sky-SH recapture and realtime probe recapture on every other tier.
<!-- /informative -->

- [Z2-N-REND-695] The capability matrix above is the single authority for which path provides which feature on which platform; `Flux_SelectShadingPath`, feature masks, and the PathParity gate step all read it from its data form `Zenith2/Engine/ZenithFlux/Internal/Paths/CapabilityMatrix.json`, and a change to one changes the other in the same diff.
- [Z2-N-REND-696] A feature marked "no" for a path adds no pass in that path, and a feature whose parity comparison is "excluded" or carries a per-feature tolerance is compared in the parity suite exactly as the matrix states and no more strictly.

### Capability matrix tests

- [Z2-N-REND-697] The tests `Test_Paths_MatrixMatchesData` (the JSON matrix and this table agree row for row), `Test_Paths_MobileForcesForwardPlus`, `Test_Paths_PreviewUsesForward`, `Test_Paths_RuntimeSwitchInvalidatesHistory` and `Test_Paths_DisabledFeatureAddsNoPass` exist and pass.

## Frame anatomy common prologue

Here the common prologue is the first group of GPU passes of a render frame, declared by the render subgraph's graph build; it is distinct from the frame prologue of `Zenith2/Docs/Architecture.md → "CPU frame graph and scheduler"`, the eight main-thread steps before `RunFrame`.

The full pass tables are `Zenith2/Docs/Rendering/FrameAnatomy.md`, headings "Reading the frame tables", "Common prologue", "Per-view prologue", "Early-Z ordering" and "Async compute placement".

- [Z2-N-REND-720] The common prologue runs once per frame, not per view, in the order given by `Zenith2/Docs/Rendering/FrameAnatomy.md` "Common prologue": GPU-scene growth and scatter upload, the draw-key bucket scatter, geometry pool growth, streaming uploads, compute skinning and morph targets, particle simulation, procedural geometry, ENV's Stage 5 prologue passes, the light and decal list build, the shared sky LUTs and ENV's cloud layer, then the shadow views.
- [Z2-N-REND-721] The per-view prologue (instance culling phase 1) runs once per main, editor, capture and texture view before that view's first depth-writing pass.
- [Z2-N-REND-722] Every depth-writing pass draws the opaque bucket first and the alpha-tested bucket second, the alpha-tested pipelines are the only pipelines allowed to `discard`, and every subsequent colour pass uses an equal depth test with no discard, so early-Z stays enabled for all shading.

## Frame anatomy Forward+

The full pass table is `Zenith2/Docs/Rendering/FrameAnatomy.md` "Forward+ frame".

- [Z2-N-REND-735] The Forward+ frame for one main view is, in order: prepass phase 1, HiZ build, instance culling phase 2, prepass phase 2, final HiZ, light clustering, volumetric fog, GTAO, contact shadows, SSGI (Stage 5, desktop tiers), forward opaque, sky and atmosphere, SSR, transparent forward, particles and ribbons, ENV's underwater composite for a submerged view, then the common epilogue.
- [Z2-N-REND-736] Forward+ is the default path and the one path on Android; volumetric fog integrates before any shading so the opaque, transparent and particle passes all sample the integrated volume in-shader.

## Frame anatomy Forward

The full pass table is `Zenith2/Docs/Rendering/FrameAnatomy.md` "Forward frame".

- [Z2-N-REND-750] The Forward frame is, in order: the Forward+ prepass, HiZ and culling passes; per-object light lists; volumetric fog, GTAO and contact shadows; SSGI (Stage 5); forward opaque with per-object lights, per-object decals and in-shader fog; sky, SSR, transparent, particles and the underwater composite as in Forward+; then the common epilogue.
- [Z2-N-REND-751] Forward is the non-clustered reference path: it is used for the parity reference, previews and captures, and it is not selectable on mobile.

## Frame anatomy Deferred

The full pass table is `Zenith2/Docs/Rendering/FrameAnatomy.md` "Deferred frame".

- [Z2-N-REND-760] The Deferred frame is, in order: depth prepass phase 1, HiZ build, culling phase 2, prepass phase 2, final HiZ; the G-buffer pass with an equal depth test and no discard; deferred decals; GTAO and contact shadows; SSGI (Stage 5); light clustering and volumetric fog; compute deferred lighting into HDR colour with froxel fog; sky, SSR, transparent forward, particles and the underwater composite; then the common epilogue.
- [Z2-N-REND-761] Deferred is desktop-only, every G-buffer target (including the advanced-surface target GB4 of the Stage 5 advanced-surfaces permutation) is at most 16 bits per channel with the formats fixed in `Zenith2/Docs/Rendering/PerPass/Deferred.md`, and there is no MSAA.

## Frame anatomy Visibility buffer

The full pass table is `Zenith2/Docs/Rendering/FrameAnatomy.md` "Visibility buffer frame".

- [Z2-N-REND-770] The visibility-buffer frame is, in order: meshlet culling phase 1; visibility raster phase 1; HiZ build, meshlet culling phase 2, visibility raster phase 2, final HiZ; material classification; light and decal clustering; material resolve (one compute dispatch per material bin, applying clustered decals and writing `SurfaceData`); velocity reconstruction; GTAO and volumetric fog; SSGI (Stage 5); lighting as in Deferred; sky, SSR, transparent forward, particles and the underwater composite; then the common epilogue.
- [Z2-N-REND-771] The visibility ID is 32 bits, (visible-meshlet index : 25 bits, triangle-in-meshlet : 7 bits), stored in an R32_UINT target cleared to the reserved value `0xFFFFFFFF` meaning "no geometry"; per-instance data is reached through the visible-meshlet list.
- [Z2-N-REND-772] The visibility buffer is desktop-only, and transparent geometry never enters it.

## Frame anatomy common epilogue

The full pass table is `Zenith2/Docs/Rendering/FrameAnatomy.md` "Common epilogue".

- [Z2-N-REND-785] The common epilogue runs per view, in order: TAA or the temporal upscaler, auto-exposure, depth of field (optional, per camera), motion blur (optional), bloom, tonemap and colour grading, UI composite, debug overlays and the on-screen profiler, then present (the main view's frame-pacing-controlled swapchain present) or a copy into the view's output texture.
- [Z2-N-REND-786] The TAA/upscaler is the resolution boundary: every pass before it runs at render extent and every pass from it on runs at output extent, and the upscaler choice is a quality-tier setting.

## Per-pass specifications

The per-pass specifications live in five section files, one per group of passes; a pass that appears in more than one path is specified once, in `Zenith2/Docs/Rendering/PerPass/Common.md`.

| Pass | File | Heading |
|---|---|---|
| GPU-scene scatter upload; GPU-scene growth; geometry pool growth; streaming upload; compute skinning; morph targets; particle emit, simulate, compact and sort keys, sort; procedural geometry; light and decal list build | `Zenith2/Docs/Rendering/PerPass/Common.md` | "Prologue passes" |
| Shadow instance culling; shadow depth; shadow HiZ | `Zenith2/Docs/Rendering/PerPass/Common.md` | "Shadow passes" |
| Instance culling phases 1 and 2 with draw compaction; culling clears; per-command sort; bucket scatter; HiZ build; transparent culling | `Zenith2/Docs/Rendering/PerPass/Common.md` | "Culling and HiZ passes" |
| Light clustering; volumetric fog; GTAO; contact shadows; SSGI (Stage 5); deferred lighting (Deferred and visibility buffer) | `Zenith2/Docs/Rendering/PerPass/Common.md` | "Shared lighting passes" |
| Sky and atmosphere; sky capture and probe filter (Stage 4); SSR; opaque colour copy; transparent forward; particles and ribbons | `Zenith2/Docs/Rendering/PerPass/Common.md` | "Shared late passes" |
| TAA/upscaler; auto-exposure; depth of field; motion blur; bloom; tonemap and grading; UI composite; debug overlay; HiZ debug overlay; present | `Zenith2/Docs/Rendering/PerPass/Common.md` | "Epilogue passes" || Forward+ prepass; Forward+ opaque | `Zenith2/Docs/Rendering/PerPass/ForwardPlus.md` | "Forward+ passes" |
| Per-object light lists; forward opaque with per-object lights | `Zenith2/Docs/Rendering/PerPass/Forward.md` | "Forward passes" |
| Deferred depth prepass; G-buffer; deferred decals | `Zenith2/Docs/Rendering/PerPass/Deferred.md` | "Deferred passes" |
| Meshlet culling phases 1 and 2; visibility raster; material classification; material resolve; velocity reconstruction | `Zenith2/Docs/Rendering/PerPass/VisibilityBuffer.md` | "Visibility buffer passes" |

- [Z2-N-REND-795] Every pass of every path is specified in exactly one per-pass section file with its parameter struct fields, its targets and their formats, its queue, its capability requirements, its budget row (by `Z2-B-` ID and name) and its permutations, and a pass implementation matches its specification table field for field.
- [Z2-N-REND-796] A pass's budget row is a GPU-time row of `Zenith2/Docs/Budgets.md → "Rendering rows"` cited by ID; every such row is pending until the Stage 1 per-system budget ticket for its area (RPROD, LIGHT, GEOM, ENV, VFX, UI or DEVT) allocates it, and a ticket that cites a pending row is blocked by that budget ticket.
- [Z2-N-REND-797] The parity scenes of "Validation roles" exercise every pass that appears in more than one path.

## Vertex data format

The full specification is `Zenith2/Docs/Rendering/VertexData.md`, headings "Vertex declaration in Slang", "Generated layout tables", "Default stream layout", "QTangent codec", "Octahedral codec", "Half-float codec", "Precision checks and fallbacks", "Index buffers and mesh splitting", "Unified stream pools", "Buffer usage unions", "Skinned vertex pool", "Codec tests" and "D-VTX-TANGENTLESS harness".

- [Z2-N-REND-865] Vertex layouts are declared in Slang and nowhere else; the offline compiler bakes each layout into a generated table that runtime PSO creation and the cook's packers both consume, so the vertex writer and the vertex reader cannot disagree (Z2-D-062).
- [Z2-N-REND-866] The default mesh family stores position in a dedicated SNORM16x4 stream dequantised by a per-mesh scale and bias, the tangent frame as an SNORM16x4 QTangent, UVs as UNORM16x2 with a per-mesh scale and bias, and indices as 16 bits with mesh splitting; a 32-bit vertex or index format appears exclusively where a documented precision check requires it, and the cook reports every fallback.
- [Z2-N-REND-867] One codec, in the leaf library `ZenithVertexFormat` (owned by GEOM, depending on ZenithCore alone), serves Flux, ZenithCook and ZenithEditor, and its GPU half `Zenith2/Shaders/Common/VertexFormats.slang` matches it bit for bit, proven by round-trip unit tests of every format across both.

## GPU performance rules and lints

These rules apply on every platform, Windows and Android alike (Z2-D-014). Each is enforced by automation where it can be; the lint and check names below are the names the gate output, the frame dump and ticket Validation sections use.

| Rule | Mechanism | Enforcement | Check names |
|---|---|---|---|
| No empty render passes; merge similar passes | The graph compiler culls raster passes with no draws and no side effects, and folds clear-only passes into the next user's load op (`CLEAR` instead of a separate clear pass). It merges consecutive raster passes into one rendering scope when all of these hold: the attachment set is identical; load/store is compatible; no synthesised barrier falls between them, because dynamic rendering forbids barriers inside a rendering instance (for example, A writing a UAV counter that B reads blocks the merge). This is not tile-local-read merging, which is D-TBDR (open, Stage 5) | graph unit tests (including a barrier-blocked merge); the frame dump shows the merges | `FLUX_RG_INFO_PASS_CULLED`, `FLUX_RG_INFO_CLEAR_FOLDED`, `FLUX_RG_INFO_PASSES_MERGED`, dump field `mergeBlockedReason`; tests `Test_RenderGraph_EmptyRasterPassCulled`, `Test_RenderGraph_ClearFoldedIntoLoadOp`, `Test_RenderGraph_MergeIdenticalAttachments`, `Test_RenderGraph_MergeBlockedByBarrier` |
| Keep early-Z | Opaque pipelines never `discard`/`clip` and never write `SV_Depth`. Alpha-tested materials use separate pipelines, drawn after opaque in depth-writing passes, and all subsequent passes use an equal test with no discard | ShaderChecks lint: `discard`/`clip` only in `[AlphaTest]` variants; `SV_Depth`/`SV_DepthGreaterEqual` forbidden unless a decision allowlists it; PSO check of the colour-pass depth test | `ShaderChecks.DiscardOnlyInAlphaTest`, `ShaderChecks.NoDepthExport`, `PSO.ColourPassEqualDepth` |
| Depth format | D24S8 by default (standard Z), including all shadow views. Z direction and format are decoupled: reversed Z is used exclusively for views whose near/far ratio fails the D24 precision check, and those use D32F_S8. Capability fallback: Vulkan guarantees one of D24S8 or D32F_S8, AMD drivers do not expose D24S8, so the device picks D32F_S8 there. Stencil is kept. No HiZ or shader code depends on the depth bit depth (the pyramid is linear) | graph compile: a D32 target needs the view's precision justification, or the capability fallback | `FLUX_RG_ERROR_D32_UNJUSTIFIED` |
| No full 32-bit render targets unless necessary | HDR colour is R11G11B10F (bloom, SSR previous colour). The TAA history is RGBA16F (sign, alpha and precision against history drift). Normals RG16_UNORM octahedral; velocity RG16F; the G-buffer ≤16 bits per channel; the HiZ pyramid RG16F linear min/max | graph compile: a 32-bit-per-channel format needs an entry on the justified list: the visibility ID (R32_UINT), D32F views, and GPU-scene data in buffers | `FLUX_RG_ERROR_32BIT_TARGET_UNJUSTIFIED` |
| Don't bind attachments unnecessarily | A pass binds exactly the attachments in its `Flux_RasterTargets`. Depth is bound read-only whenever the pass doesn't write it. Unwritten attachments are checked at two levels: a declared-shader-set check (each pass lists the shader programs it may use, and the graph checks their reflected outputs against its targets), and a development-build record-time check for indirect multi-PSO passes, whose PSOs aren't known at compile time. An MRT output written conditionally has a colour write mask of 0 in PSOs that don't write it, because in Vulkan an unwritten output is undefined, not preserved. Depth-only passes are fine | graph compile checks; record-time checks in development builds and every graph and render test, including the depth check: a pass that declared depth `WRITE`, issued at least one draw and bound no PSO with depth write enabled fails the frame, because the raster state a lambda binds is visible at record time alone | `FLUX_RG_ERROR_UNWRITTEN_ATTACHMENT`, `FLUX_RG_ERROR_DEPTH_WRITABLE_UNUSED`, `FLUX_RECORD_CHECK_UNWRITTEN_ATTACHMENT`, `ShaderChecks.ConditionalOutputWrite`, `PSO.UnwrittenOutputMaskZero` |
| Half precision where possible | Colour, normal, BRDF-term and post-process maths use `half` with native 16-bit types: SPIR-V `Float16` under `shaderFloat16`, D3D12 SM 6.2 `-enable-16bit-types` with `Native16BitShaderOpsSupported`; not `min16float`, which is a minimum-precision hint. Positions, depth, UV derivatives and accumulations stay `float`. The vertex codec's integer-only half conversion is kept regardless, so compute codecs never need `shaderInt16` | ShaderChecks reports the float/half ratio per shader; review signal | `ShaderChecks.HalfRatioReport` (report), `ShaderChecks.NoMin16Float` (fail) |
| Back-face culling for opaque | The default raster state is cull back. Two-sided rendering needs an explicit material flag, and is reported | PSO lint: an opaque PSO with culling off carries the material's two-sided flag | `PSO.OpaqueCullBack`, counter `Flux.PSO.TwoSided` |
| Compressed vertex data, 16-bit indices, a position stream | see "Vertex data format" | cook checks and generated layout tables | `Cook.VertexPrecisionFallbacks` (report), `Cook.NoIndex32` (fail), `Cook.PositionStreamDedicated` (fail), `ShaderChecks.VertexFormatVocabulary` (fail) |

- [Z2-N-REND-915] The graph compiler culls every raster pass that declares no draws and carries no side effect, folds every clear-only pass into the next user's load op, and merges consecutive raster passes into one rendering scope exactly under the merge conditions of `Zenith2/Docs/Rendering/RenderGraph.md` "Pass merging" (Z2-D-014).
- [Z2-N-REND-916] Opaque pipelines contain no `discard` or `clip` and write no `SV_Depth`, `SV_DepthGreaterEqual` or `SV_DepthLessEqual`; `discard`/`clip` appear exclusively in code reachable from an entry point attributed `[AlphaTest]`, and a depth export is allowed only for a program listed with a `Z2-D-*` decision ID in `Zenith2/Shaders/ShaderChecks_allowlist.json`.
- [Z2-N-REND-917] Every colour pass that follows a depth-writing pass in the same view uses an `EQUAL` depth test with depth writes off, which `PSO.ColourPassEqualDepth` verifies at PSO creation in development builds and at cook time for every precached PSO.
- [Z2-N-REND-918] A render-target texture with 32 bits per channel is a graph compile error unless its format and use are on the justified list (R32_UINT for the visibility ID, D32F for a justified view), and extending the list takes a decision ID recorded beside the list in `Zenith2/Engine/ZenithFlux/Internal/Graph/Justified32BitFormats.json`.
- [Z2-N-REND-919] A pass binds exactly the attachments in its `Flux_RasterTargets`, binds depth read-only whenever none of its declared programs writes depth, and every declared attachment is written by every declared program's reflected outputs or else carries a colour write mask of 0 in that program's PSOs.
- [Z2-N-REND-925] In development builds and in every graph and render test, a raster pass that declared depth with `FLUX_DEPTH_ACCESS_WRITE`, issued at least one draw and bound no PSO with depth write enabled appends `FLUX_RG_ERROR_DEPTH_WRITABLE_UNUSED` to the frame's record-check report at the end of the pass's recording, which fails the frame; this is the one graph error code evaluated at record time rather than at compile.
- [Z2-N-REND-920] Every raster pass declares its shader set (`Flux_RasterTargets::m_xPrograms`, the programs its lambda may bind); the graph compile checks the set's reflected outputs against the targets, and in development builds the record-time check `FLUX_RECORD_CHECK_UNWRITTEN_ATTACHMENT` fails the frame when an indirect multi-PSO pass binds a program outside its declared set or whose outputs leave a bound attachment unwritten.
- [Z2-N-REND-921] Within one permutation, an MRT output is written on every control path or on none; `ShaderChecks.ConditionalOutputWrite` fails a program whose reflection reports a conditionally written output, and the PSO builder sets a write mask of 0 on every output a permutation never writes.
- [Z2-N-REND-922] Colour, normal, BRDF-term and post-process arithmetic uses `half` with native 16-bit types; positions, depth, UV derivatives and accumulations stay `float`; `ShaderChecks.NoMin16Float` fails any use of `min16float`/`min16half`, and `ShaderChecks.HalfRatioReport` reports the float/half instruction ratio per program as a review signal.
- [Z2-N-REND-923] The default raster state culls back faces, and `PSO.OpaqueCullBack` fails an opaque or alpha-tested PSO with culling off unless its material document sets `twoSided`; the number of two-sided PSOs per game is reported in the cook report.
- [Z2-N-REND-924] Every graphics ticket's `## Validation` section names the checks of the table above that the change must pass, and the WorkerGuide carries these rules in short form.

## Validation roles

The three validation roles are kept separate, because each catches a class of defect the others pass (Z2-D-063).

### Cross-path consistency

| Step | Compared | Metric | Threshold source |
|---|---|---|---|
| 1 | surface-data channels (albedo, normal, roughness, metallic, AO, emissive, velocity, depth) captured by a debug output of each path | per-channel max absolute difference | near-exact: ≤ 1 quantum of the channel's storage format (1/255 for UNORM8, 1/65535 for UNORM16, 2 half ULPs for 16F), per-feature exceptions in the capability matrix |
| 2 | final images after the common epilogue | FLIP mean, plus per-channel max absolute thresholds | `Z2-B-RPROD-031` Per-scene parity tolerances (pending; one FLIP mean and one per-channel threshold per parity scene, per backend and per path comparison) |

- [Z2-N-REND-940] The raster paths are compared with each other and between Vulkan and D3D12, surface-data channels first and final images second, as the table above specifies; the PathParity gate step runs the path comparisons (from Stage 5) and the BackendParity gate step runs the Vulkan-against-D3D12 comparisons (from Stage 3).
- [Z2-N-REND-941] Only features that behave identically across paths are compared; a path-specific approximation is either disabled in the parity configuration or compared with its own per-feature tolerance from the capability matrix.
- [Z2-N-REND-942] Parity scenes live under `Zenith2/Engine/ZenithFlux/Tests/Parity/Scenes/`, render with a fixed camera path, a fixed jitter sequence, fixed noise seeds per frame index and a fixed exposure, and capture at fixed frame indices, so a parity run is reproducible for the same build, backend and device.
- [Z2-N-REND-943] A parity failure retains its artefacts (both images, the FLIP heat map, the per-channel difference images and the graph dump of both runs) under `Zenith2/out/<checkout-hash>/Parity/<scene>/`.

### Physical accuracy

- [Z2-N-REND-944] The compute path tracer (a Slang compute program under `Zenith2/Shaders/Tests/PathTracer/` over a software BVH built on the CPU with a binned SAH builder, both in the tool-side library `ZenithPathTracer`, placed as `Zenith2/Docs/Architecture.md → "Repository layout"` lists it) is compared with the raster paths on controlled reference scenes (analytic lights, the IBL furnace, area-light shadows, VT-backed terrain materials) with every screen-space approximation disabled (Z2-D-037).
- [Z2-N-REND-945] The tracer runs to a convergence criterion: at least `Z2-B-ADVGFX-006` Path tracer minimum samples per pixel (pending; proposed 256), then until the per-pixel variance estimate is below `Z2-B-ADVGFX-007` Path tracer convergence variance threshold (pending; proposed: the standard error of the luminance mean below 1% of the mean for 99.5% of pixels), or until the scene's wall-clock cap; the error tolerance between the converged reference and each raster path is `Z2-B-ADVGFX-008` Physical-accuracy error tolerance per reference scene.
- [Z2-N-REND-946] WARP and lavapipe runs of the tracer are capped per scene by `Z2-B-ADVGFX-004` Path tracer resolution cap per reference scene and `Z2-B-ADVGFX-005` Path tracer wall-clock cap per reference scene, and a run that hits its wall-clock cap before convergence reports "not converged" rather than passing.
- [Z2-N-REND-947] Screen-space and approximate techniques (SSAO, SSR, contact shadows, approximate GI) are validated against their own documented error budgets on dedicated scenes, not against path-tracer convergence.
- [Z2-N-REND-948] The path tracer is a validation tool: `ZenithPathTracer` declares `ZENITH_SUPPORTED` as the Windows presets `win-debug`, `win-release`, `win-tools` and `win-asan` (no Android, Linux or shipping preset), reaches `ZenithFlux_Tests` exclusively through the `TEST_DEPENDS ZenithPathTracer` option of ZenithFlux's helper call and `ZenithPathTracer_Tests` through its own, and the Libgraph gate step fails any Player, game or runtime target that depends on it.

### Independent analytical material tests

- [Z2-N-REND-949] A separately written CPU reference BRDF in `Zenith2/Engine/ZenithFlux/Tests/ReferenceBRDF/` (sharing no code with `Zenith2/Shaders/Lighting/`) checks the shader BRDF, evaluated through a compute test harness at fixed sample points, for white-furnace energy conservation (directional albedo ≤ 1 + 1e-3 across a roughness and F0 sweep), reciprocity (`|f(l,v) − f(v,l)| ≤ 1e-5`) and closed-form cases (Lambert, GGX normalisation `∫ D(h)(n·h) dω = 1` within 1e-3).
- [Z2-N-REND-950] The analytical material tests `Test_BRDF_WhiteFurnace`, `Test_BRDF_Reciprocity`, `Test_BRDF_LambertClosedForm`, `Test_BRDF_GGXNormalisation` and `Test_BRDF_ShaderMatchesReference` exist and pass on WARP and lavapipe.

## Shading path research methodology

The comparison between shading paths is a deliverable and an experiment, so its methodology guards against measuring the harness instead of the architecture (Z2-D-064). Both halves follow the methodology of `Zenith2/Docs/Design/RESEARCH.md`; the comparison statistics and verdicts are RESEARCH's, and the shared procedure below binds both.

### What each shading-path report contains

| Section | Content | Source |
|---|---|---|
| GPU time per pass | median and 95% confidence interval per pass per path | the graph's per-pass timestamps |
| Memory | transient peak with and without aliasing, persistent render-target memory, pool sizes | the on-screen profiler's memory counters |
| Bandwidth | read and write bytes per frame and per pass | Adreno counters on the S24, vendor counters on desktop |
| Image difference | the "Validation roles" cross-path metrics against the Forward reference | the parity tooling |
| Exclusions | every capability-matrix exclusion that applied to the workload, stated explicitly | the capability matrix |

- [Z2-N-REND-955] Each shading path runs on the same workload versions with the same quality settings, and its report contains every section of the table above.
- [Z2-N-REND-956] Research measurements use controlled conditions: locked desktop GPU and CPU clocks, and Android fixed-performance mode where available; the report still gives run-to-run variance, because fixed-performance mode can overheat and does not remove scheduling noise.
- [Z2-N-REND-957] Research measurements are separate from shipping qualification: shipping qualification (see `Zenith2/Docs/Budgets.md → "Production-quality acceptance"`) runs devices in their ordinary state, including throttling, and neither set of results substitutes for the other.

### Measurement procedure

| Step | Rule |
|---|---|
| 1 | fixed hardware, drivers and clocks, recorded in the report header |
| 2 | warm-up per `Zenith2/Docs/Budgets.md → "Frame-rate definitions and measurement procedure"` |
| 3 | at least 10 repetitions per (path, workload, platform) |
| 4 | medians with 95% confidence intervals |
| 5 | raw data committed under `Zenith2/Docs/Research/` |
| 6 | conclusions recorded as decision records |

<!-- elaboration -->
- [Z2-N-REND-958] Every measurement follows the six steps of the table above; confidence intervals are the distribution-free order-statistic intervals of `Zenith2/Docs/Budgets.md` → "Repetitions and statistics", and the raw per-frame data of every repetition is committed as CSV under the run directory `Zenith2/Docs/Research/ShadingPaths/runs/<RunId>/` of RESEARCH's layout, whose RunId begins with the date.

<!-- informative -->
At n = 10 the order-statistic interval [x(2), x(9)] covers 97.85%, so a rule that two intervals "do not overlap" (D-TBDR's among them) is stricter than with a 95% interval; the rules keep their wording.
<!-- /informative -->
- [Z2-N-REND-959] A research conclusion is a decision record, appended to `Zenith2/Docs/DecisionLog.md` with an explicit `Z2-D-NNN` in the ruling ticket's own diff (Z2-D-097), and never silently changes a default: the default shading path stays Forward+ until a decision changes it (Z2-D-035).
- [Z2-N-REND-960] Measurement harnesses live under `Zenith2/Benchmarks/ShadingPaths/` with a CTest smoke run in the normal Test step; the full measurement on reference hardware is a `needs-human` ruling ticket blocked by its harness ticket (Z2-D-084).

## Decision D-SLANG

D-SLANG (open, Stage 1) pins the Slang version and selects the Vulkan lowering of `DescriptorHandle<T>`. The documented default lowering needs `VK_EXT_mutable_descriptor_type`, which is not in the baseline profile.

| Alternative | Vulkan realisation | Cost |
|---|---|---|
| 1: mutable descriptors in the profile | one `MUTABLE_EXT` binding; `VK_EXT_mutable_descriptor_type` joins `VP_ZENITH2_baseline` | narrows the device set; must hold on Adreno 750, lavapipe and the RTX 3070 |
| 2: a custom per-kind typed-array lowering | one binding per descriptor index space; Slang lowers each `DescriptorHandle<T>` to an index into the array of its kind | a Slang lowering Zenith2 maintains against the pinned version |
| 3: `uint` indices plus generated accessors | as alternative 2; parameter-struct fields become `uint2` and the generator emits typed accessors | loses `DescriptorHandle<T>` type safety in Slang; the C++ `Zenith_RHISRV<T>`/`Zenith_RHIUAV<T>` types keep it on the C++ side |

- [Z2-N-REND-970] Until D-SLANG (open, Stage 1) is ruled, engine code depends on the D-SLANG-neutral surface: the 8-byte `Zenith_RHIDescriptorHandle`, the generated `Zenith_RHISRV<T>`/`Zenith_RHIUAV<T>` fields and the `Zenith2/Shaders/Common/Bindless.slang` accessors; code that depends on a particular alternative's lowering is blocked by the D-SLANG ruling ticket.
- [Z2-N-REND-971] D-SLANG (open, Stage 1) is ruled through an ordinary harness ticket (the Stage 1 bindless probe under `Zenith2/Benchmarks/BindlessProbe/`, with a CTest smoke run) and a `needs-human` ruling ticket blocked by it; the `android-arm64` run of the probe on the S24 is a `needs-human` device step (Z2-D-084).
- [Z2-N-REND-972] The acceptance rule of D-SLANG (open, Stage 1): non-uniform indexing, descriptor limits, generated layouts and the two-word handle read of `Test_BindlessProbe_HandleWordsRoundTrip` all pass on an RTX GPU, the S24 (Adreno 750), WARP and lavapipe, for the chosen alternative.

<!-- elaboration -->
| Probe case | What it proves |
|---|---|
| `Test_BindlessProbe_NonUniformSampled` | divergent per-lane `NonUniformResourceIndex` sampling of `Texture2D` in pixel and compute stages returns each lane's own texture |
| `Test_BindlessProbe_NonUniformStorage` | divergent storage-buffer and storage-image writes land in each lane's own resource |
| `Test_BindlessProbe_MaxIndex` | the last slot of each index space at the tier capacity is reachable |
| `Test_BindlessProbe_PartiallyBound` | unwritten slots never touched by a shader do not fault or report validation errors |
| `Test_BindlessProbe_UpdateAfterBind` | a slot written while the set is bound in a pending command buffer is visible to subsequent submissions |
| `Test_BindlessProbe_HandleLayout` | a GPU readback of `offsetof` for every field of a generated parameter struct equals the generated `static_assert` values on both APIs |
| `Test_BindlessProbe_DrawIndex` | `Flux_DrawIndex()` equals the command index for a 1,000-command indirect bucket on both APIs |
| `Test_BindlessProbe_DrawIndexLowering` | `SV_DrawIndex` lowers to the SPIR-V `DrawIndex` built-in on Vulkan and to a read of root constant `b1` on DXIL, verified by disassembling the compiled blobs |
| `Test_BindlessProbe_LinkTimeMaterial` | link-time specialisation of `Flux_ActiveMaterial` compiles and runs for two material types in one program, giving two permutations with each material's output |
| `Test_BindlessProbe_HandleWordsRoundTrip` | an 8-byte handle with a non-zero second word in a material-constants buffer, read through `Flux_HandleWords` in pixel and compute stages, returns both words bit-exact, and an ordinary handle reads `y == 0` |
| `Test_BindlessProbe_StorageFormats` | every storage-write and typed-load row of "Required format features" is written and read back correctly through a storage view |

- [Z2-N-REND-973] The bindless probe implements every case of the table above on the four targets (an RTX GPU, the S24, WARP and lavapipe), and its report records, per device, the Slang version, the alternative, and pass or fail per case.

## Recovery

The full specification is `Zenith2/Docs/Rendering/GpuResources.md`, headings "Swapchain recreation", "Device loss" and "Pipeline cache invalidation".

- [Z2-N-REND-980] Swapchain recreation (resize, `VK_ERROR_OUT_OF_DATE_KHR`, `VK_SUBOPTIMAL_KHR`, DXGI resize, Android surface loss and restore) completes without restarting the renderer, and invalidates exactly the histories of views whose output extent changed.
- [Z2-N-REND-981] Device loss is detected, reported with GPU breadcrumbs naming the last completed and the first incomplete pass, and handled by the recover-or-clean-exit policy of `Zenith2/Docs/Rendering/GpuResources.md` "Device loss".
- [Z2-N-REND-982] Persistent pipeline caches are keyed on the backend, vendor, device, driver version and (Vulkan) pipeline-cache UUID, plus the shader blob container version; a mismatch discards the cache and rebuilds it, and never loads a stale one.
