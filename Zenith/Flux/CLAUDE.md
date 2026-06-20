# Flux Rendering System

## Render Pipeline at a Glance

A typical frame compiles into roughly this topologically-sorted order. The render graph derives the order from each pass's declared Reads/Writes — this is **not** a hardcoded sequence and individual subsystems can be enabled/disabled without changing the rest. Click `Render/RenderGraph/Print Pass Order` in the debug variables panel to dump the live order at runtime.

```
+-----------------------------+
| Frame begin                 |
+-----------------------------+
              |
              v
+-----------------------------+    Cascaded Shadow Maps (4 cascades)
| Shadow cascades             |--> writes shadow depth targets
+-----------------------------+
              |
              v
+-----------------------------+    G-buffer build
| Terrain                     |\
| StaticMeshes                | >--> writes MRT diffuse / normals+ambient / material + scene depth
| AnimatedMeshes              |/
| Vegetation (foliage)        |
+-----------------------------+
              |
              v
+-----------------------------+    Screen-space effects
| SSAO                        |
| HiZ generation              |
| SSR (raymarch / upsample)   |
| SSGI (raymarch / denoise)   |
+-----------------------------+
              |
              v
+-----------------------------+    Lighting
| DeferredShading             |--> reads G-buffer + shadows + IBL + SSR + SSGI, writes HDR scene
| Skybox                      |
| Volumetric Fog              |
| Particles                   |
| Primitives (debug)          |
+-----------------------------+
              |
              v
+-----------------------------+    HDR -> LDR
| HDR Bloom + Tonemap         |--> reads HDR scene, writes swapchain LDR
+-----------------------------+
              |
              v
+-----------------------------+    Overlays (LDR)
| UI quads / text             |
| Editor gizmos (tools build) |
| ImGui (tools build)         |
+-----------------------------+
              |
              v
+-----------------------------+
| Present                     |
+-----------------------------+
```

For the full graph lifecycle (Setup -> Compile -> Execute), barrier synthesis, the fluent builder API, and the Print Pass Order debug button, see [RenderGraph/CLAUDE.md](RenderGraph/CLAUDE.md).

## Files

### Core
- `Flux.h/cpp` - Main rendering infrastructure, pipeline specification, `Flux_SurfaceInfo`
- `Flux_Buffers.h/cpp` - Buffer management
- `Flux_RecordValidation.h` - Shared inline asserts (pipeline/view/draw-constant validity) called by both backends' recorder methods
- `Flux_Enums.h` - Rendering enums (ResourceAccess, TextureFormat, BlendFactor, DepthCompareFunc, MeshTopology, ShaderDataType, BindingType, LoadAction, StoreAction, MRTIndex, etc.)
- `Flux_Graphics.h/cpp` - Global graphics state, frame constants
- `Flux_RenderTargets.h/cpp` - Render target management
- `Flux_Types.h` - Type definitions, `IsCompressedFormat()` helper

Note: Materials and textures are now in `AssetHandling/` (see AssetHandling/CLAUDE.md):
- `Zenith_MaterialAsset.h/cpp` - Material properties + texture references
- `Zenith_TextureAsset.h/cpp` - GPU texture wrapper with SRV

### Subdirectories
- `StaticMeshes/` - Opaque geometry
- `AnimatedMeshes/` - Skeletal animation rendering (bone buffers sourced from `Zenith_AnimatorComponent`)
- `MeshAnimation/` - Skeletal animation system (see MeshAnimation/CLAUDE.md). ECS entry point is `Zenith_AnimatorComponent`, not `Zenith_ModelComponent`.
- `Terrain/` - Terrain rendering (see Terrain/CLAUDE.md)
- `Shadows/` - Cascaded shadow maps
- `DeferredShading/` - Deferred lighting
- `SSAO/` - Screen-space ambient occlusion
- `Fog/` - Volumetric fog system (see Fog/CLAUDE.md)
  - Four techniques: Simple, Froxel, Raymarch, God Rays
  - Runtime technique selection via debug variables
  - Requires 3D texture support (see Vulkan/CLAUDE.md)
- `Particles/` - Particle systems
- `Skybox/` - Sky rendering
- `Text/` - Text rendering
- `Primitives/` - Debug primitives
- `Gizmos/` - Editor gizmos (see Gizmos/CLAUDE.md)

## Architecture

### Direct command recording
Render systems record GPU work by calling methods **directly** on the backend command
buffer (aliased `Flux_CommandBuffer` — `Zenith_Vulkan_CommandBuffer` in the Vulkan build):
`pxCmdBuf->SetPipeline(...)`, `Draw(...)`, `Dispatch(...)`, plus the named-binding helper
`Flux_ShaderBinder`. There is no intermediate command-list DSL — a pass's `OnRecord`
callback receives a `Flux_CommandBuffer*` and emits native draws/dispatches/binds. The
backend command-recorder surface is the C++20 `FluxBackendCommandRecorder` concept (see
Backend Abstraction); shared `Flux_RecordValidation.h` helpers carry the validity asserts.
(Historically this went through a deferred `Flux_CommandList` byte-buffer DSL replayed by
`IterateCommands` — that abstraction was removed once the render graph owned ordering +
barriers, leaving the DSL pure indirection.)

### Pass Execution Order
There is no `RenderOrder` enum and no caller-supplied ordering token. Pass execution order is computed at frame boundary by `Flux_RenderGraph::Compile()`:

1. Each pass declares the resources it `Reads()`/`Writes()`/`DependsOn()` via the fluent `AddPass()` builder.
2. `Compile()` builds a dependency adjacency from those declarations and runs Kahn's topological sort.
3. Resource transitions (image layout / buffer access) are then synthesized automatically as barriers between consecutive passes.

At `Execute()`, the graph queues each enabled pass in topological order (`QueueRenderPass`) and then drives the single direct-recording stage: the backend records each pass's callback (`Flux_RenderGraph::RecordPassInto`) into per-worker command buffers, in topological order, with barriers emitted inline. Worker command buffers are submitted in order, so the topological order + inline barriers are what enforce cross-pass dependencies. See `Flux/RenderGraph/CLAUDE.md` for the full graph lifecycle and the **Print Pass Order** debug button which dumps the current frame's resolved order.

### Pipeline Specification
`Flux_PipelineSpecification` struct defines complete graphics pipeline state: shader, blend modes, depth test, vertex input, render targets, load/store actions.

### Material System
Materials (`Zenith_MaterialAsset`) store textures and rendering properties. Located in `AssetHandling/`. Use `SetDiffuseTexture(TextureHandle(...))` when creating materials — the handle covers both path-based (serializable) and procedural-pointer textures. See `AssetHandling/CLAUDE.md` for details on material and texture asset management.

### Backend Abstraction
The renderer is backend-agnostic. `Flux_Backend.h` aggregates the seven C++20 concepts that any backend must satisfy: `FluxBackendDevice`, `FluxBackendMemoryAlloc`, `FluxBackendMemoryDelete`, `FluxBackendCommandRecorder`, `FluxBackendSync`, `FluxBackendPresentation`, and the shader/pipeline-builder family (`FluxBackendShader`, `FluxBackendPipelineBuilder`, `FluxBackendComputePipelineBuilder`, `FluxBackendRootSigBuilder`). `Backend/Flux_BackendConformance.cpp` static-asserts the active backend against each concept, so signature drift fails the build instead of the first frame. Adding a second backend (DX12 / Metal / WebGPU) means providing classes that satisfy each concept and adding the conformance asserts.

### Feature Registry & automatic shader hot-reload
`Flux_FeatureRegistry` (`Flux_FeatureRegistry.h/.cpp`) is the one table `Flux_RendererImpl` walks for init / render-graph setup / shutdown. Each `Flux_FeatureDesc` carries up to four captureless trampolines — `Initialise` / `SetupRenderGraph` / `Shutdown` / `BuildPipelines`. **A feature is added with exactly ONE call** — `RegisterFeature<&Zenith_Engine::Foo>(reg, "Foo")` — placed in render-graph declaration order inside `RegisterDefaultFeatures()`. That single call drives all three walks: **init** (registration order, forward), **setup** (the feature's `SetupRenderGraph` is auto-appended at the call site), and **shutdown** (reverse registration order, auto). The helper resolves each trampoline at compile time with a `requires` check, wiring only the methods `Foo` implements — so the former "irregulars" need no special casing: FluxGraphics omits `SetupRenderGraph`/`BuildPipelines`, DynamicLights omits both (gather/upload front-end), Fog omits `Shutdown` (RAII; its `BuildPipelines` rebuilds all fog techniques).

This works because init/shutdown are dependency-safe in ANY order beyond "FluxGraphics first" — every subsystem's `Initialise`/`Shutdown` touches only foundation (FluxGraphics/memory/backend) + its own state, never another feature. Only the **render-graph declaration order** is load-bearing (producer-before-consumer; see the ORDERING note in `Flux_FeatureRegistry.h`), and that's the order you write the calls in. The only non-feature exceptions are 2 `AddSetupStep(...)` raw steps interleaved in the list — Skybox's aerial-perspective second pass and the final-RT layout-transition pass — these aren't features. (All shared cross-feature transients — G-buffer / depth / final-RT / HDR scene — are created up front by `FluxGraphics::SetupRenderGraph`, the first feature, which is why FluxGraphics owns the HDR scene target rather than HDR.) `RegisterFeature` is constrained on the `FluxRenderFeature` C++20 concept (a feature type exposes all four lifecycle methods — `Initialise` / `SetupRenderGraph` / `Shutdown` / `BuildPipelines`, a no-op stub where it has nothing to do), so passing a non-feature accessor — or a feature missing a method — is a clear compile error at the call site. **If a render system can't fit the one-liner, fix the render system** rather than adding bespoke registration.

Shader hot-reload (ZENITH_TOOLS) is **derived from this table** — subsystems do *not* register themselves. `Flux_ShaderHotReload::AutoRegisterFeatures()` (called from `Flux::LateInitialise`) walks `Flux_ShaderRegistry` and wires every program to its owning feature's `BuildPipelines` callback, keyed on the program's `m_szSubsystem` string matched against the feature name. So:

> **Adding a renderer feature:** give the subsystem `Initialise`/`SetupRenderGraph`/`Shutdown`/`BuildPipelines` as needed, add ONE `RegisterFeature<&Zenith_Engine::Foo>(reg, "Foo")` line at the right spot in `RegisterDefaultFeatures()` (render-graph order), and set its shader programs' `m_szSubsystem` to `"Foo"`. Init, setup, shutdown, and hot-reload then all follow with **zero** extra wiring.

The handful of programs whose grouping differs from their feature, or that no feature owns (e.g. the swapchain blit `TexturedQuad`), live in a small override table in `Flux_ShaderHotReload.cpp`. Out-of-tree owners (a game's own pass) still call `RegisterProgram` / `RegisterSubsystem` directly — after `AutoRegisterFeatures`, which lets them override the auto-wired binding.

## Configuration

Key constants in `Core/ZenithConfig.h`:
- `FLUX_MAX_TARGETS` - Maximum render targets per pass
- `FLUX_MAX_BINDINGS_PER_GROUP` - Descriptors per shader
- `MAX_FRAMES_IN_FLIGHT` - Frame pipelining count

## Key Concepts

**Single-stage recording:** Each pass's `OnRecord` callback records native commands directly into a per-worker backend command buffer at `Execute()` time, in topological order, with graph-synthesized barriers emitted inline. There is no separate record-to-DSL + replay step.

**Multi-threaded Recording:** Passes are recorded in parallel — each worker records a contiguous topological slice of the pass list into its own command buffer (`FLUX_NUM_WORKER_THREADS`); the worker buffers are submitted in order, so order + inline barriers enforce dependencies.

**View Space Convention:** The engine uses **+Z forward** in view space (not -Z like OpenGL convention). When extracting linear depth from view-space positions, use `viewPos.z` directly without negation. See `Fog/CLAUDE.md` for depth reconstruction details.

**World Position Reconstruction:** Use `GetWorldPosFromDepthTex()` from `Shaders/Common.fxh` for reconstructing world positions from the depth buffer - it handles the view/projection inverse transforms correctly.

## Design Rationale

### Why View Wrapper Structs Are Separate Types

The view types (`Flux_ShaderResourceView`, `Flux_RenderTargetView`, `Flux_DepthStencilView`, etc.) may appear duplicated since they have identical structure. However, this design is intentional:

1. **Compile-Time Type Safety**: Separate types prevent accidentally passing an RTV where a DSV is expected. The compiler catches these errors, avoiding subtle runtime bugs.

2. **API Semantics Mirror Vulkan/D3D12**: The view types follow established graphics API conventions (D3D12's SRV/UAV/RTV/DSV pattern). Developers familiar with these APIs understand the semantic distinctions.

3. **View-Specific Extensions**: While currently identical, separate types allow adding view-specific fields later (e.g., mip levels for SRVs, array slices for RTVs) without breaking existing code.

4. **No Runtime Overhead**: The separate types add zero runtime cost - they're just naming conventions that the compiler optimizes away.

5. **Binding Function Overloads**: The command buffer's `BindSRV()`, `BindUAV()`, `BindRTV()` methods take specific view types, ensuring correct binding at compile time.

### Direct recording replaced the command-list DSL

Render systems used to record into a deferred `Flux_CommandList` byte-buffer DSL
(`AddCommand<Flux_CommandX>()`) that the backend later replayed via `IterateCommands`.
That DSL existed to "trivialise multithreading and synthesize barriers" — both of which the
render graph now owns (topological sort + `SynthesizeBarriers`), making the DSL pure
indirection. It was removed: passes now call the backend command-recorder methods directly
(the 1:1 targets the command classes used to forward to), recorded inside the dependency-
ordered worker stage. To add a new GPU operation, add the method to the
`FluxBackendCommandRecorder` concept + each backend, and call it from the pass callback.

### Buffer Wrapper Classes (Candidate for Simplification)

Note: The buffer wrapper classes (`Flux_VertexBuffer`, `Flux_DynamicVertexBuffer`, etc.) DO represent genuine code duplication. Seven classes follow two patterns (single-buffer vs frame-indexed-array), and template consolidation could reduce ~100 lines while preserving type safety. This is a candidate for future simplification.
