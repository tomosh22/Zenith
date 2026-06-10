# Vulkan Backend

## Overview

Vulkan rendering backend providing GPU resource management, command buffer recording, and pipeline construction.

## Key Components

| File | Purpose |
|------|---------|
| `Zenith_Vulkan.h/.cpp` | Core Vulkan initialization, device management |
| `Zenith_Vulkan_MemoryManager.h/.cpp` | VRAM allocation, texture/buffer creation, staging buffer |
| `Zenith_Vulkan_CommandBuffer.h/.cpp` | Command buffer recording, barrier management |
| `Zenith_Vulkan_Pipeline.h/.cpp` | Pipeline and root signature construction |

## Texture Type Support

Supports 2D, 3D, and Cube textures via `TextureType` enum in `Flux_Enums.h`:

```cpp
enum TextureType
{
    TEXTURE_TYPE_2D,
    TEXTURE_TYPE_3D,
    TEXTURE_TYPE_CUBE
};
```

### Creating 3D Textures

Set `m_eTextureType = TEXTURE_TYPE_3D` and `m_uDepth` in `Flux_SurfaceInfo`:

```cpp
Flux_SurfaceInfo xInfo;
xInfo.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
xInfo.m_eTextureType = TEXTURE_TYPE_3D;
xInfo.m_uWidth = 160;
xInfo.m_uHeight = 90;
xInfo.m_uDepth = 64;  // 3D depth
xInfo.m_uNumMips = 1;
xInfo.m_uNumLayers = 1;
xInfo.m_uMemoryFlags = (1 << MEMORY_FLAGS__SHADER_READ) | (1 << MEMORY_FLAGS__UNORDERED_ACCESS);
```

### 3D Texture Functions

- `Zenith_Vulkan_MemoryManager::CreateRenderTargetVRAM()` - Creates 3D render targets
- `Zenith_Vulkan_MemoryManager::CreateTextureVRAM()` - Creates 3D textures with optional data
- `CreateShaderResourceView()` - Creates `vk::ImageViewType::e3D` for sampling
- `CreateUnorderedAccessView()` - Creates `vk::ImageViewType::e3D` for compute write
- `CreateRenderTargetView()` - Creates `vk::ImageViewType::e3D` for render targets

## Memory Manager

### Key Functions

| Function | Purpose |
|----------|---------|
| `CreateBufferVRAM()` | Allocate GPU buffer |
| `CreateRenderTargetVRAM()` | Allocate render target (2D/3D/Cube) |
| `CreateTextureVRAM()` | Allocate texture with optional data upload |
| `UploadBufferData()` | Upload data to buffer via staging |
| `QueueVRAMDeletion()` | Deferred deletion for in-flight resources |

### Staging Buffer

Large uploads use a staging buffer pool (`g_uStagingPoolSize`). Uploads larger than the pool are chunked automatically.

Memory operations are ad-hoc — callable at any time with no frame bracket. The manager opens its internal command buffer lazily on first use; pending work is drained once per frame by `SubmitFrameMemoryWork()` (renderer-only, submitted ahead of render work) or on demand by `Flush()` (synchronous, CPU-waits).

### Compressed Texture Handling

Compressed formats (BC1, BC3, BC5, BC7) require special handling:

1. **No runtime mipmap generation**: Compressed formats don't support `VK_FORMAT_FEATURE_BLIT_DST_BIT`, so `vkCmdBlitImage` cannot be used for mipmap generation. Compressed textures must have pre-generated mipmaps in the source data.

2. **Layout transitions**: In `FlushStagingBuffer()`, compressed textures skip the blit-based mipmap loop. Mip levels 1+ remain in `TRANSFER_DST_OPTIMAL` (not `TRANSFER_SRC_OPTIMAL` like non-compressed textures after blit). The final transition to `SHADER_READ_ONLY_OPTIMAL` must use the correct source layout.

3. **Format detection**: Use `IsCompressedFormat(TextureFormat)` from `Flux_Types.h` to check if a format is compressed.

```cpp
// StagingTextureMetadata includes format for compressed texture handling
struct StagingTextureMetadata {
    vk::Image m_xImage;
    uint32_t m_uWidth, m_uHeight, m_uDepth;
    uint32_t m_uNumMips, m_uNumLayers;
    TextureFormat m_eFormat;  // Used to detect compressed formats
};
```

### 3D Texture Upload Bug Fix Note

The `StagingTextureMetadata` struct stores texture dimensions for deferred buffer-to-image copy in `FlushStagingBuffer()`. The `m_uDepth` field must be correctly stored and used in the copy command's `setImageExtent()`, otherwise only the first Z slice(s) will have valid data.

If 3D textures only show data in early Z slices:
1. Check `StagingTextureMetadata::m_uDepth` is populated when staging the upload
2. Check `FlushStagingBuffer()` uses `m_uDepth` in `setImageExtent()` (not hardcoded to 1)

## Command Buffers

### Barrier Management

```cpp
s_xCommandBuffer.ImageTransitionBarrier(
    xImage,
    vk::ImageLayout::eUndefined,
    vk::ImageLayout::eShaderReadOnlyOptimal,
    vk::ImageAspectFlagBits::eColor,
    vk::PipelineStageFlagBits::eAllCommands,
    vk::PipelineStageFlagBits::eAllCommands
);
```

### Compute Dispatch

```cpp
// Dispatch is intentionally trivial: UpdateDescriptorSets() then vkCmdDispatch().
// All synchronisation (image layout transitions, UAV image memory barriers,
// UAV buffer memory barriers, indirect-arg buffer barriers) is owned by
// Flux_RenderGraph::SynthesizeBarriers and emitted from
// RecordCommandBuffersTask — image entries via ImageTransition, buffer
// entries via BufferBarrier — before the pass executes (outside any active
// render pass). Declare every read/write via Flux_RenderGraph::Read/Write
// (or ReadBuffer/WriteBuffer for buffers, or DependsOn for explicit
// compute→graphics edges) and the graph will emit the right barrier — no
// inline Flux_CommandImageTransition needed.
s_xCommandBuffer.Dispatch(uGroupsX, uGroupsY, uGroupsZ);
```

## Pipeline Construction

### Graphics Pipeline

```cpp
Flux_PipelineSpecification xSpec;
xSpec.m_pxShader = &s_xShader;
xSpec.m_pxTargetSetup = &s_xTargetSetup;
xSpec.m_xPipelineLayout.m_uNumBindingGroups = 1;
xSpec.m_xPipelineLayout.m_axBindingGroups[0].m_axBindings[0].m_eType = BINDING_TYPE_BUFFER;
xSpec.m_xPipelineLayout.m_axBindingGroups[0].m_axBindings[1].m_eType = BINDING_TYPE_TEXTURE;

Flux_PipelineBuilder::FromSpecification(s_xPipeline, xSpec);
```

### Compute Pipeline

```cpp
Zenith_Vulkan_ComputePipelineBuilder xBuilder;
xBuilder.WithShader(xComputeShader);
xBuilder.WithLayout(xRootSig);
xBuilder.Build(s_xComputePipeline);
```

## Handle System

GPU resources use opaque handles to abstract Vulkan types:

- `Flux_VRAMHandle` - VRAM allocation
- `Flux_ImageViewHandle` - Image views (SRV, UAV, RTV, DSV)
- `Flux_BufferDescriptorHandle` - Buffer descriptors

Handles are registered in internal registries and can be released for reuse.

## GPU Resource Lifecycle (CRITICAL)

### Deferred Deletion System

GPU resources cannot be immediately deleted because they may still be in use by in-flight command buffers. The `QueueVRAMDeletion()` system defers deletion until `MAX_FRAMES_IN_FLIGHT + 1` frames have passed.

### Correct Deletion Pattern

**Always use wrapper functions** that queue deletion AND invalidate handles:

```cpp
// CORRECT: Use wrapper functions
Flux_MemoryManager::DestroyVertexBuffer(xVertexBuffer);  // Queues + resets handle
Flux_MemoryManager::DestroyIndexBuffer(xIndexBuffer);    // Queues + resets handle
```

The wrapper functions:
1. Check if handle is valid (early return if not)
2. Queue VRAM for deferred deletion
3. Call `xBuffer.Reset()` to invalidate the handle

### Double-Free Anti-Pattern (AVOID)

**Never queue VRAM directly and then call a cleanup function that also queues:**

```cpp
// WRONG: don't hand-roll deletion when a wrapper exists.
QueueVRAMDeletion(xHandle);  // Queues deletion AND auto-invalidates xHandle
pxObject->Reset();           // Reset() now sees an invalid handle -- OK here,
                             // but bypassing the wrapper is still error-prone
```

`QueueVRAMDeletion` takes the handle by **non-const reference** and clears it
before returning (auto-invalidation), so a later `Reset()`/destructor pass sees
an invalid handle and does not double-free. The backend resolves the VRAM record
from the handle internally — callers never pass a `Flux_VRAM*`.

### When Directly Calling QueueVRAMDeletion

If you must call `QueueVRAMDeletion()` directly (e.g., for textures or render attachments), just pass the handle — it is auto-invalidated:

```cpp
// CORRECT direct usage -- pass only the handle; the backend resolves the VRAM
// record and auto-invalidates the handle (passed by reference).
if (xAttachment.m_xVRAMHandle.IsValid())
{
    g_xEngine.FluxMemory().QueueVRAMDeletion(xAttachment.m_xVRAMHandle, ...);
}
```

### Classes with VRAM-Owning Members

For classes like `Flux_MeshGeometry` that own GPU resources:
- Make them non-copyable (`= delete` copy/move constructors)
- Have `Reset()` check `IsValid()` before destroying
- Have destructor call `Reset()`
- Use the wrapper destroy functions, not direct `QueueVRAMDeletion()`
