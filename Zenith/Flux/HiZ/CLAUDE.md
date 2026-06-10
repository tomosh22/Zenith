# Flux_HiZ - Hierarchical Z-Buffer System

## Overview

The Hi-Z (Hierarchical Z-Buffer) system generates a depth pyramid from the main depth buffer. This mip chain enables efficient depth-based operations like screen-space reflections (SSR), occlusion culling, and ambient occlusion.

## Architecture

### Pass placement

The HiZ generation pass declares `Reads(scene depth)` and `Writes(HiZ chain)`. Because the render graph topo-sorts on declared dependencies, it automatically runs:

- **after** any pass that writes to scene depth (terrain, opaque static meshes, animated meshes, foliage)
- **before** any pass that reads the HiZ chain (SSR raymarch, SSGI raymarch, future occlusion-culling)

There is no explicit ordering enum — the dependency declarations alone produce the correct placement.

### Files

| File | Purpose |
|------|---------|
| [Flux_HiZImpl.h](Flux_HiZImpl.h) | `Flux_HiZImpl` class declaration, constants |
| [Flux_HiZ.cpp](Flux_HiZ.cpp) | Implementation, pipeline setup |
| [../Shaders/HiZ/Flux_HiZ_Generate.comp](../Shaders/HiZ/Flux_HiZ_Generate.comp) | Compute shader for mip generation |

## Implementation Details

### Render Target

- **Format:** `R32_SFLOAT` (single-channel 32-bit float)
- **Size:** Full resolution with complete mip chain
- **Memory Flags:** `MEMORY_FLAGS__UNORDERED_ACCESS | MEMORY_FLAGS__SHADER_READ`

### Per-Mip Views

The HiZ chain is a render-graph **transient** (created in `SetupRenderGraph` via
`CreateTransient`). The graph owns the image; per-mip SRV/UAV views are resolved
on demand from the transient attachment:

```cpp
static constexpr u_int uHIZ_MAX_MIPS = 12;  // Supports up to 4096x4096

Flux_ShaderResourceView&          GetMipSRV(u_int uMip);  // Read previous mip — GetHiZBuffer().SRV(uMip)
Flux_UnorderedAccessView_Texture& GetMipUAV(u_int uMip);  // Write current mip — GetHiZBuffer().UAV(uMip)
```

### Mip Generation Algorithm

1. **Mip 0:** Copy depth from main depth buffer
2. **Mips 1-N:** Sample 2x2 from previous mip, write minimum depth

```glsl
// For mip > 0: sample 2x2 from previous mip and take minimum
vec4 xDepths;
xDepths.x = textureLod(g_xInputMip, xUV + vec2(-0.5, -0.5) * xTexelSize, 0).r;
xDepths.y = textureLod(g_xInputMip, xUV + vec2( 0.5, -0.5) * xTexelSize, 0).r;
xDepths.z = textureLod(g_xInputMip, xUV + vec2(-0.5,  0.5) * xTexelSize, 0).r;
xDepths.w = textureLod(g_xInputMip, xUV + vec2( 0.5,  0.5) * xTexelSize, 0).r;

float fMinDepth = min(min(xDepths.x, xDepths.y), min(xDepths.z, xDepths.w));
```

### Barrier Handling

`Flux_RenderGraph::SynthesizeBarriers` (called from `Compile`) walks the
execution order, tracks per-(attachment, mip, layer) access state for images
and per-buffer access state for buffers, and populates each pass's
`m_xPrologueBarriers` list. The Vulkan backend
(`Zenith_Vulkan.cpp::RecordCommandBuffersTask`) consumes that list — image
entries via `ImageTransition`, buffer entries via `BufferBarrier` — right
before each pass executes, outside any active render pass so
`vkCmdPipelineBarrier` is unrestricted.

For the HiZ chain, each per-mip pass uses the fluent builder:

```cpp
xGraph.AddPass(szPassName, ExecuteHiZMip)
    .UserData(uMip)                                          // typed, no void* cast
    .ReadsTransient (m_xHiZBufferHandle, READ_SRV, uMip-1, 1) // previous mip
    .WritesTransient(m_xHiZBufferHandle, WRITE_UAV, uMip, 1); // current mip
```

From those declarations the graph emits:
- Mip pass `N`: `UNDEFINED → WRITE_UAV` (GENERAL) on mip `N` before the dispatch
- Mip pass `N+1`: `WRITE_UAV → READ_SRV` (SHADER_READ_ONLY) on mip `N` before the dispatch
- The next graphics consumer (SSR / SSGI / SSAO) declaring `Read(... 0, m_uMipCount)` triggers
  `WRITE_UAV → READ_SRV` on the final mip before its render pass begins

No inline `Flux_CommandImageTransition` calls live in `ExecuteHiZMip` — the
graph owns synchronisation end-to-end. Per-(mip, layer) state tracking
ensures consecutive mip passes don't over-synchronise.

## Public Interface

The subsystem is a non-static `Flux_HiZImpl` owned by `Zenith_Engine`; callers
reach it via `g_xEngine.HiZ()`. (The historical `Flux_HiZ` static facade was
removed in Phase 9.)

```cpp
class Flux_HiZImpl
{
public:
    void Initialise();
    void Shutdown();
    void BuildPipelines();

    void SetupRenderGraph(Flux_RenderGraph& xGraph);

    // Accessors for consumers (SSR, SSGI, SSAO, culling)
    Flux_RenderAttachment&            GetHiZAttachment();
    Flux_ShaderResourceView&          GetHiZSRV();           // Full mip chain
    u_int                             GetMipCount() const;
    Flux_ShaderResourceView&          GetMipSRV(u_int uMip); // Single mip
    Flux_UnorderedAccessView_Texture& GetMipUAV(u_int uMip); // Compute write
    bool                              IsEnabled() const;
};
```

## Cross-Subsystem Dependencies

`Initialise()` takes no parameters. Cross-subsystem deps (the swapchain,
`Flux_GraphicsImpl`, `Flux_RendererImpl`) are engine-owned singletons reached
via `g_xEngine.X()` at point of use — this is the Flux-wide pattern (the
earlier injected-member-pointer DI seam was reverted in favour of direct
reaches). Non-capturing fn-pointer trampolines — the resolution-change
callback, the `ZENITH_TOOLS` hot-reload callback, and the `ExecuteHiZMip`
graph callback (`void(*)(Flux_CommandList*, void*)`) — cannot capture `this`,
so they likewise re-enter via `g_xEngine.HiZ()` to reach the singleton
instance.

## Debug Variables

| Path | Type | Description |
|------|------|-------------|
| `Flux/HiZ/Enable` | bool | Enable/disable Hi-Z generation |
| `Flux/HiZ/Textures/Mip0` | texture | Full resolution depth |
| `Flux/HiZ/Textures/Mip2` | texture | Quarter resolution |
| `Flux/HiZ/Textures/Mip4` | texture | 1/16 resolution |

## Usage

### In SSR (Ray Marching)

```glsl
// Sample Hi-Z at current mip level
float fHiZDepth = textureLod(g_xHiZTex, xSampleUV, float(uMipLevel)).r;

// Compare ray depth against Hi-Z
if (fRayDepth > fHiZDepth)
{
    // Ray is below surface at this mip level
    // Refine to finer mip or confirm hit at mip 0
}
```

### In Occlusion Culling

```glsl
// Sample conservative depth for object's screen bounds
float fMinHiZ = textureLod(g_xHiZTex, xObjectCenter, fMipForObjectSize).r;

// If object's nearest depth is behind Hi-Z, it's occluded
if (fObjectNearZ > fMinHiZ)
    discard;  // or skip rendering
```

## Performance Considerations

- Compute shader uses 8x8 workgroups
- Sequential mip dispatch (mip N depends on mip N-1)
- Total dispatches = mip count (typically 10-12 for 1080p/4K)
- GPU memory: ~1.33x main depth buffer (due to mip pyramid)
