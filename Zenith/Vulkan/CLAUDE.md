# Vulkan Backend

## Overview

Vulkan rendering backend providing GPU resource management, command buffer recording, and pipeline construction.

### Device API and queue minimums

- Vulkan 1.1 is the explicit physical-device minimum. Zenith's generated
  shaders target SPIR-V 1.3, so there is no Vulkan 1.0 extension-only path.
- The selected graphics queue family must support both graphics and compute.
  Render-graph workers record draw and dispatch commands into graphics-family
  command buffers; a separate compute-only family does not make a
  graphics-only family suitable. Presentation may use a separate family.
- A separate presentation family gets **no command pool**. Presentation is not
  a command-buffer capability — `vkQueuePresentKHR` takes no command buffer,
  and a WSI-only family chosen purely because
  `vkGetPhysicalDeviceSurfaceSupportKHR` returned true may expose none of
  graphics/compute/transfer, making a pool on it useless at best and invalid
  at worst. `CreateCommandPools` therefore skips `COMMANDTYPE_PRESENT`
  whenever it resolved to a family of its own (it keeps the shared pool in the
  common case where present and graphics are the same family), and
  `GetCommandPool` asserts rather than handing back a null pool. Nothing asks
  for one: the only callers are GRAPHICS (recorders, staging, screenshot) and
  COPY (the memory manager's internal buffer).
- VMA receives the selected physical-device API version, clamped to both the
  Vulkan 1.3 version requested by the instance and the `VMA_VULKAN_VERSION`
  ceiling compiled into that platform's VMA build. This last clamp matters on
  Android, where a Vulkan 1.3 device can be paired with VMA compiled for 1.2.
  `vmaCreateAllocator` failure is checked before staging or command-buffer
  resources are initialised.

## Key Components

| File | Purpose |
|------|---------|
| `Zenith_Vulkan.h/.cpp` | Core Vulkan initialization, device management |
| `Zenith_Vulkan_MemoryManager.h/.cpp` | VRAM allocation, texture/buffer creation, staging buffer |
| `Zenith_Vulkan_MemoryManager_{Buffers,Textures,Views,Aliasing,Registries}.cpp`, `..._Internal.h` | MemoryManager implementation split by concern (buffer/texture/view creation, memory aliasing, handle registries) |
| `Zenith_Vulkan_CommandBuffer.h/.cpp` | Command buffer recording, barrier management |
| `Zenith_Vulkan_Pipeline.h/.cpp` | Pipeline, shader, and root signature construction (declares `Zenith_Vulkan_Shader`, `Zenith_Vulkan_RootSig`/`RootSigBuilder`, graphics + compute pipeline builders) |
| `Zenith_Vulkan_ComputePipelineBuilder.cpp`, `Zenith_Vulkan_Shader.cpp` | Compute-pipeline-builder and shader implementation TUs (classes declared in `Zenith_Vulkan_Pipeline.h`) |
| `Zenith_Vulkan_Buffer.h/.cpp`, `Zenith_Vulkan_Texture.h/.cpp` | GPU buffer and texture resource wrappers |
| `Zenith_Vulkan_Swapchain.h/.cpp` | Swapchain acquire/present, screenshot capture |
| `Zenith_Vulkan_Platform.h`, `Zenith_PlatformGraphics_Include.h` | Platform glue + `Flux_*` backend type aliases (e.g. `Flux_Pipeline`, `Flux_Shader`, `Flux_RootSig`) |
| `Zenith_Vulkan_ImGuiIntegration.cpp` | Vulkan implementation of ImGui integration (`ZENITH_TOOLS`-only): `RegisterTexture`, `UnregisterTexture`, `ProcessDeferredUnregistrations`, `GetImTextureID` |

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
| `DownloadBufferData()` | Read a buffer back to the CPU. **Explicit-call-only** — drains pending uploads, idles the device, round-trips a throwaway host-visible staging buffer. Never call it from a frame path. Legal only on `UNORDERED_ACCESS` / `INDIRECT_BUFFER` buffers: `CreateBufferVRAM` grants transfer-src usage to those families alone, and `vkCmdCopyBuffer` rejects any other source. |
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

### Indirect-count capability negotiation + fallback (Phase 1-8 of the terrain indirect-count compatibility plan)

`vkCmdDrawIndexedIndirectCount` (Vulkan 1.2 core or `VK_KHR_draw_indirect_count`)
is OPTIONAL: the Android emulator's goldfish/gfxstream ICD, among others,
supports neither. The engine keeps **advertised / enabled / usable** state
distinct in `Zenith_Vulkan::CreateDevice`:
- **Advertised**: enumerated via `vkEnumerateDeviceExtensionProperties` /
  `VkPhysicalDeviceVulkan12Features` BEFORE `vkCreateDevice`.
- **Enabled**: requested in the `vk::DeviceCreateInfo` feature chain (only
  for advertised features; never force-`VK_TRUE` an unadvertised feature
  bit, the old "pick first, force unsupported" behaviour the plan retired).
- **Usable**: advertised AND enabled AND the matching function pointer
  resolved (`vkGetDeviceProcAddr` returns non-null). A driver that
  advertises an extension but hands back a null proc address downgrades
  usable count to false; the recorder then never calls the null pointer.

`m_xIndirectDrawCaps` (`Flux_IndirectDrawCapabilities`) reports the USABLE
semantic booleans (nativeCount / multiDraw / firstInstance / drawParams) and
the raw `maxDrawIndirectCount`. The recorder's
`DrawIndexedIndirectCount` reads it plus the boot-time override
`m_eIndirectDrawOverride` (parsed by `Zenith_CommandLine` from
`--indirect-count-mode=auto|native|padded|single`, converted here at device
init, immutable after that) and selects an effective mode via the pure
`Flux_SelectIndirectExecutionMode` selector in `Flux_IndirectDraw.h`:

- **NATIVE_COUNT** — one `vkCmdDrawIndexedIndirectCount[KHR]` call. Runnable
  when usable count, the request fits the reported native limit, and the
  override permits it. `multiDrawIndirect` gates fixed indirect commands only;
  it does not gate this counted command.
- **PADDED_MULTI** — `vkCmdDrawIndexedIndirect` in batches no larger than the
  legal per-call limit (`maxDrawIndirectCount` when multi-draw is on, else 1).
  The count buffer is NOT dereferenced in this tier; the caller's
  `ZERO_PADDED_TO_MAX` policy guarantees the padded `[0, uMaxDrawCount)`
  range is a valid zero/no-op every frame.
- **PADDED_SINGLE** — one `vkCmdDrawIndexedIndirect` call per record
  (`drawCount == 1`, offset advancing by `stride`). The legal tier when
  multi-draw is unavailable.
- **FAILED_CLOSED** — no API command, hard assert/log. A `REQUIRE_NATIVE`
  caller whose native preconditions failed OR an explicit NATIVE override on
  unsupported hardware MUST fail closed; the recorder never silently slides
  into padded execution and never calls a null function pointer. Test telemetry
  increments once for every rejected semantic request, including validation,
  stale-resource, and missing-procedure failures—not only selector failures.

An over-limit request (e.g. `uMaxDrawCount` > `maxDrawIndirectCount`) MUST
NOT split native count across the same count buffer (each batch would
re-read the full count and over-draw). The selector chooses PADDED_MULTI
when the caller's policy permits it.

`DrawIndexedIndirect` (the ordinary fixed draw, not the counted one) uses
the same private/common fixed-emission helper via the batch planner, so
no caller can exceed a backend limit accidentally. Splits reset shader
draw ID at each batch boundary; the plan's caution about shaders that
consume draw ID across a logical multi-draw (grass, particles,
unified/instanced meshes) is documented in `Flux_IndirectDraw.h` and
exercised by the smoke matrix's forced-mode resource cases.

Device capability log lines (replace the retired
"terrain streaming will be disabled" wording, which was inaccurate):
```
vkCmdDrawIndexedIndirectCount: advertised(ext=yes core1.2=no) enabled=yes usable=yes (driver apiVersion 1.4.313.1)
  multiDrawIndirect: raw=no usable=no | drawIndirectFirstInstance: raw=yes usable=yes | shaderDrawParameters: raw=yes usable=yes
  maxDrawIndirectCount: raw/native=1 fixedPerCall=1 (fixed clamps to 1 when multi-draw off)
  --indirect-count-mode: auto (effective fallback negotiated per-request by the recorder)
```
On a device that lacks count (the Android emulator tier) the usable line
reads `usable=NO (terrain draws via padded fallback)` and terrain still
renders — the padded tier is the fallback, not a skip.

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

### GPU per-pass timestamps (profiling)

`Zenith_Vulkan_PerFrame` owns a per-frame-in-flight `VkQueryPool` (timestamp). Each
`Flux_RenderGraph` pass is bracketed by `Zenith_Vulkan_CommandBuffer::Begin/EndGPUTimer`
(`vkCmdWriteTimestamp`, bottom-of-pipe) from `RecordPassInto`. Query slots are claimed
atomically across recording workers (`ClaimGPUTimer`); worker 0 cmd-resets the pool at the
head of its first-submitted buffer (`CmdResetGPUTimers`). Results are read back deferred in
`BeginFrame` once the slot fence signals (`ReadbackGPUTimers`), scaled by
`limits.timestampPeriod`, and handed to the CPU profiler's GPU channel. Gated on
`ZENITH_FLUX_PROFILING`; auto-disabled when the graphics queue's `timestampValidBits == 0`.
See `Profiling/CLAUDE.md`.

## Pipeline Construction

### Backend type aliases

Engine code never names `Zenith_Vulkan_*` types directly — it uses the backend-neutral
`Flux_*` aliases defined in `Zenith_PlatformGraphics_Include.h` (and
`Core/Zenith_BackendAliases.h`, which `Flux/Flux_Fwd.h` includes),
which resolve to the Vulkan classes here or the D3D12 null-backend equivalents:
`Flux_Pipeline`, `Flux_PipelineBuilder`, `Flux_Shader`, `Flux_RootSig`,
`Flux_RootSigBuilder`, `Flux_ComputePipelineBuilder`.

### Shaders and root signatures

- `Zenith_Vulkan_Shader` (`Flux_Shader`) is initialised from a `Flux_ShaderDecl` via
  `Initialise(xDecl)` — on Windows it compiles the registered Slang program at runtime,
  otherwise it loads precompiled `.spv` + `.spv.refl` artifacts. `InitialiseEx` returns a
  `Zenith_Status` instead of hard-asserting on failure.
- `Zenith_Vulkan_RootSig` (`Flux_RootSig`) holds the `vk::PipelineLayout m_xLayout`,
  descriptor-set layouts, and reflection data for name-based binding lookup
  (`GetBinding(szName)`). Build one with `Zenith_Vulkan_RootSigBuilder::FromSpecification`
  (manual) or `FromReflection` (auto-generated from shader reflection).
  **`m_xReflection` is populated only on the `FromReflection` path.** The graphics
  pipeline builder goes through `FromSpecification(rootSig, spec.m_xPipelineLayout)`,
  so a graphics pipeline's root sig carries an EMPTY reflection — anything at draw
  time that wants reflection must not read it from there (it silently iterates
  nothing, which is how a validator can look green while doing no work).

### Every draw path binds the BINDLESS table itself (set 2)

`BindPersistentSpineSets` auto-binds GLOBAL (0) and VIEW (1) only; BINDLESS (2) is
bound explicitly by `UseBindlessTextures(2)`, right after the `SetPipeline` it
belongs to. Because a Vulkan set binding survives a pipeline switch between
prefix-compatible layouts — and every spine pipeline is — a pass that forgets the
call still draws correctly whenever some earlier feature in the same worker command
buffer bound it, and breaks only when that feature is disabled, reordered, or
early-outs on an empty frame.

A `ZENITH_DEBUG` pre-draw check in `ValidateDescriptorBindingsDebug` closes that:
`m_axCurrentPersistentSet[bindPoint][BINDLESS]` records the explicit bind and is
cleared by `BeginRecording` **and** by `SetPipeline` / `BindComputePipeline`, so
inheritance can never satisfy it, and
`Flux_PersistentSetLayouts::ShouldDemandBindlessBind` asserts by pass name for any
pipeline that reads the table without one.

> **"Reads the table" cannot come from reflection.** Slang's
> `IMetadata::isParameterLocationUsed` — the source of `m_bStaticallyUsed`, and
> reliable for the GLOBAL/VIEW members it was added for — answers **false** for the
> unbounded `g_axTextures` array even in a program that samples it. Measured
> directly: with the grass G-buffer draw's bind removed, Vulkan reported "VkPipeline
> ... uses set 2 but that set is not bound" for the very program whose reflection bit
> read 0. So `Zenith_Vulkan_Shader::DetectBindlessTableUsage` scans the loaded SPIR-V
> instead (`Flux/Slang/Flux_SpirvUsage.h`, pure + unit-tested) and the pipeline
> builders copy the verdict onto `Zenith_Vulkan_RootSig::m_bUsesBindlessTable`.
> Both shader-load paths are covered — the runtime Slang compile and the `.spv`
> artifact load both hold the module bytes.

### Graphics Pipeline

```cpp
Flux_PipelineSpecification xSpec;
xSpec.m_pxShader = &s_xShader;
// Render target setup is specified via individual fields (no m_pxTargetSetup):
xSpec.m_aeColourAttachmentFormats[0] = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
xSpec.m_uNumColourAttachments = 1;
xSpec.m_eColourLoadAction = LOAD_ACTION_CLEAR;
xSpec.m_eColourStoreAction = STORE_ACTION_STORE;
xSpec.m_eDepthStencilFormat = TEXTURE_FORMAT_D32_SFLOAT;
xSpec.m_eDepthStencilLoadAction = LOAD_ACTION_CLEAR;
xSpec.m_eDepthStencilStoreAction = STORE_ACTION_STORE;
xSpec.m_xPipelineLayout.m_uNumBindingGroups = 1;
// Kind-aware binding entries (m_bPresent marks a declared slot; m_eKind drives
// the descriptor type). Most layouts are reflection-derived via FromReflection;
// manual specs set the kind + present flag directly.
xSpec.m_xPipelineLayout.m_axBindingGroups[0].m_axBindings[0].m_eKind = FLUX_RESOURCE_KIND_CONSTANT_BUFFER;
xSpec.m_xPipelineLayout.m_axBindingGroups[0].m_axBindings[0].m_uDescriptorCount = 1;
xSpec.m_xPipelineLayout.m_axBindingGroups[0].m_axBindings[0].m_bPresent = true;
xSpec.m_xPipelineLayout.m_axBindingGroups[0].m_axBindings[1].m_eKind = FLUX_RESOURCE_KIND_COMBINED_TEXTURE_SAMPLER;
xSpec.m_xPipelineLayout.m_axBindingGroups[0].m_axBindings[1].m_uDescriptorCount = 1;
xSpec.m_xPipelineLayout.m_axBindingGroups[0].m_axBindings[1].m_bPresent = true;

Flux_PipelineBuilder::FromSpecification(s_xPipeline, xSpec);
```

### Compute Pipeline

```cpp
// Recommended one-call helper — combines WithShader + WithLayout + Build and
// assigns the root signature into the pipeline's m_xRootSig slot.
Zenith_Vulkan_ComputePipelineBuilder::BuildFromShader(s_xComputePipeline, xComputeShader, xRootSig);

// The fluent form is also available. Note WithLayout() takes a vk::PipelineLayout,
// so pass xRootSig.m_xLayout (not the Zenith_Vulkan_RootSig object):
//   Zenith_Vulkan_ComputePipelineBuilder xBuilder;
//   xBuilder.WithShader(xComputeShader);
//   xBuilder.WithLayout(xRootSig.m_xLayout);
//   xBuilder.Build(s_xComputePipeline);
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
