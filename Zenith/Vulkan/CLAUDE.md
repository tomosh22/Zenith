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
// Dispatch handles UAV barriers automatically
s_xCommandBuffer.Dispatch(uGroupsX, uGroupsY, uGroupsZ);
```

## Pipeline Construction

### Graphics Pipeline

```cpp
Flux_PipelineSpecification xSpec;
xSpec.m_pxShader = &s_xShader;
xSpec.m_pxTargetSetup = &s_xTargetSetup;
xSpec.m_xPipelineLayout.m_uNumDescriptorSets = 1;
xSpec.m_xPipelineLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;
xSpec.m_xPipelineLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_TEXTURE;

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
