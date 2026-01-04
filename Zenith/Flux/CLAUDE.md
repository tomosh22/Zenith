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
