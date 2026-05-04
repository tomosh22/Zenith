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
- `Flux_CommandList.h/cpp` - Command list recording
- `Flux_Enums.h` - Rendering enums (CommandType, ResourceAccess, TextureFormat, BlendFactor, DepthCompareFunc, MeshTopology, ShaderDataType, BindingType, LoadAction, StoreAction, MRTIndex, etc.)
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

### Command List System
Platform-agnostic command recording. Commands stored sequentially in dynamically-sized buffer. `AddCommand<T>()` template adds commands, `IterateCommands()` executes them on platform command buffer.

### Pass Execution Order
There is no `RenderOrder` enum and no caller-supplied ordering token. Pass execution order is computed at frame boundary by `Flux_RenderGraph::Compile()`:

1. Each pass declares the resources it `Reads()`/`Writes()`/`DependsOn()` via the fluent `AddPass()` builder.
2. `Compile()` builds a dependency adjacency from those declarations and runs Kahn's topological sort.
3. Resource transitions (image layout / buffer access) are then synthesized automatically as barriers between consecutive passes.

`Flux::SubmitCommandList()` queues a recorded `Flux_CommandList` for the platform backend's worker-thread iteration; submission order is determined by the topo sort, not by call order at submit time. See `Flux/RenderGraph/CLAUDE.md` for the full graph lifecycle and the **Print Pass Order** debug button which dumps the current frame's resolved order.

### Command List Reset
`Reset(bool bClearTargets)` clears command list for new frame. Reuses buffer allocation. Parameter controls whether render targets are cleared on render pass begin (true) or contents preserved (false for multi-pass rendering).

### Pipeline Specification
`Flux_PipelineSpecification` struct defines complete graphics pipeline state: shader, blend modes, depth test, vertex input, render targets, load/store actions.

### Material System
Materials (`Zenith_MaterialAsset`) store textures and rendering properties. Located in `AssetHandling/`. Use `SetDiffuseTexture(TextureHandle(...))` when creating materials — the handle covers both path-based (serializable) and procedural-pointer textures. See `AssetHandling/CLAUDE.md` for details on material and texture asset management.

### Backend Abstraction
The renderer is backend-agnostic. `Flux_Backend.h` aggregates the seven C++20 concepts that any backend must satisfy: `FluxBackendDevice`, `FluxBackendMemoryAlloc`, `FluxBackendMemoryDelete`, `FluxBackendCommandRecorder`, `FluxBackendSync`, `FluxBackendPresentation`, and the shader/pipeline-builder family (`FluxBackendShader`, `FluxBackendPipelineBuilder`, `FluxBackendComputePipelineBuilder`, `FluxBackendRootSigBuilder`). `Backend/Flux_BackendConformance.cpp` static-asserts the active backend against each concept, so signature drift fails the build instead of the first frame. Adding a second backend (DX12 / Metal / WebGPU) means providing classes that satisfy each concept and adding the conformance asserts.

## Configuration

Key constants in `Core/ZenithConfig.h`:
- `FLUX_MAX_TARGETS` - Maximum render targets per pass
- `FLUX_MAX_BINDINGS_PER_GROUP` - Descriptors per shader
- `MAX_FRAMES_IN_FLIGHT` - Frame pipelining count

## Key Concepts

**Deferred Execution:** Commands recorded into lists, submitted for execution, then iterated on worker threads to build Vulkan command buffers.

**Multi-threaded Recording:** Multiple command lists can be recorded in parallel across worker threads.

**Memory Reuse:** Command lists reuse allocations across frames via Reset().

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

### Why Command Classes Are Verbose But Correct

The 16+ command classes in `Flux_CommandList.h` follow an identical pattern. While verbose, this design is appropriate:

1. **Explicit and Debuggable**: Each command class is self-documenting. When debugging, you can set breakpoints in specific command's `operator()` method.

2. **Type-Safe Command Parameters**: Each command's constructor parameters are type-checked at compile time. A `Flux_CommandBindSRV` constructor explicitly requires `Flux_ShaderResourceView*`.

3. **Efficient Binary Storage**: Commands are stored directly in a byte buffer without heap allocation. The switch-case dispatch in `IterateCommands()` compiles to efficient jump tables.

4. **C++ Limitations**: There's no elegant C++ solution that doesn't trade clarity for metaprogramming complexity:
   - `std::variant` still requires all the struct definitions, plus visitor boilerplate
   - Macros obscure the code and complicate debugging
   - Union + type tag loses compile-time type safety

5. **Clear Extension Pattern**: Adding a new command requires: (1) enum value, (2) class definition, (3) switch case. This is predictable and doesn't break existing commands.

The verbosity is the cost of explicit, type-safe, debuggable code in C++ without metaprogramming complexity.

### Buffer Wrapper Classes (Candidate for Simplification)

Note: The buffer wrapper classes (`Flux_VertexBuffer`, `Flux_DynamicVertexBuffer`, etc.) DO represent genuine code duplication. Seven classes follow two patterns (single-buffer vs frame-indexed-array), and template consolidation could reduce ~100 lines while preserving type safety. This is a candidate for future simplification.
