# Flux Rendering System

## Files

### Core
- `Flux.h/cpp` - Main rendering infrastructure, pipeline specification
- `Flux_Buffers.h/cpp` - Buffer management
- `Flux_CommandList.h/cpp` - Command list recording
- `Flux_Enums.h` - Rendering enums including RenderOrder
- `Flux_Graphics.h/cpp` - Global graphics state, frame constants
- `Flux_MaterialAsset.h/cpp` - Material and asset management
- `Flux_RenderTargets.h/cpp` - Render target management
- `Flux_Types.h` - Type definitions

### Subdirectories
- `StaticMeshes/` - Opaque geometry
- `AnimatedMeshes/` - Skeletal animation rendering
- `MeshAnimation/` - Skeletal animation system (see MeshAnimation/CLAUDE.md)
- `Terrain/` - Terrain rendering (see Terrain/CLAUDE.md)
- `Shadows/` - Cascaded shadow maps
- `DeferredShading/` - Deferred lighting
- `SSAO/` - Screen-space ambient occlusion
- `Fog/` - Fog application
- `Particles/` - Particle systems
- `Skybox/` - Sky rendering
- `Text/` - Text rendering
- `Primitives/` - Debug primitives
- `Gizmos/` - Editor gizmos (see Gizmos/CLAUDE.md)

## Architecture

### Command List System
Platform-agnostic command recording. Commands stored sequentially in dynamically-sized buffer. `AddCommand<T>()` template adds commands, `IterateCommands()` executes them on platform command buffer.

### Render Order
`RenderOrder` enum in `Flux_Enums.h` defines execution order. Passed to `Flux::SubmitCommandList()`. GPU executes commands in order from beginning to end.

### Command List Reset
`Reset(bool bClearTargets)` clears command list for new frame. Reuses buffer allocation. Parameter controls whether render targets are cleared on render pass begin (true) or contents preserved (false for multi-pass rendering).

### Pipeline Specification
`Flux_PipelineSpecification` struct defines complete graphics pipeline state: shader, blend modes, depth test, vertex input, render targets, load/store actions.

### Material System
Materials store textures and rendering properties. Use `SetDiffuseWithPath()` when creating materials to store texture source path for scene serialization.

## Configuration

Key constants in `Core/ZenithConfig.h`:
- `FLUX_MAX_TARGETS` - Maximum render targets per pass
- `FLUX_MAX_DESCRIPTOR_BINDINGS` - Descriptors per shader
- `STATIC_MESH_VERTEX_STRIDE` - 60 bytes (position, UV, normal, tangent, bitangent, color)
- `MAX_FRAMES_IN_FLIGHT` - Frame pipelining count

## Key Concepts

**Deferred Execution:** Commands recorded into lists, submitted for execution, then iterated on worker threads to build Vulkan command buffers.

**Multi-threaded Recording:** Multiple command lists can be recorded in parallel across worker threads.

**Memory Reuse:** Command lists reuse allocations across frames via Reset().

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
