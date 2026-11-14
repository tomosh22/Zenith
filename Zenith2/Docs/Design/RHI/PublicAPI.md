# RHI design: public API

This file is part of the RHI design family. The entry document is `Zenith2/Docs/Design/RHI.md`; this file holds the header list and the value-type headers of `ZenithRHI` (from `Zenith_RHITypes.h` to `Zenith_RHIQueue.h`), and `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` holds the rest: the recording, barrier, swapchain, query, debug, fault-injection, configuration and device headers, the two backend factory headers and the ZenithFlux public headers the RHI area owns. Every declaration is written in Zenith Hungarian (Z2-D-034), every fallible function returns `Zenith_Result<T, E>` ([Z2-N-CONTRACT-461]), and every public type carries one contract block of the grammar of `Zenith2/Docs/Contracts.md → "Contract blocks per type"`. Where `Zenith2/Docs/Rendering.md` already declares a type, this file reproduces the declaration only to add its contract block and the members the specification leaves implicit, and marks each addition with a trailing `// elaboration` comment.

## Header list

| Header (`Zenith2/Engine/ZenithRHI/`) | Declares | Includes (besides ZenithCore headers) | Includable by |
|---|---|---|---|
| `Zenith_RHITypes.h` | `Zenith_RHIBackend`, `Zenith_RHIQueue`, `Zenith_RHIQueueValues`, `Zenith_RHIFormat`, `Zenith_RHIExtent3D`, `Zenith_RHIRect`, `Zenith_RHIViewport`, `Zenith_RHIClearValue`, `Zenith_RHITextureDimension`, `Zenith_RHITextureUsage`, `Zenith_RHIBufferUsage`, `Zenith_RHIMemoryClass`, `Zenith_RHIAdapterPreference`, `Zenith_RHIWaitStatus`, `Zenith_RHISubresourceRange`, `Zenith_RHIPresentMode`, `Zenith_RHIOutputColourSpace` | none | any library that declares `ZenithRHI` |
| `Zenith_RHIError.h` | `Zenith_RHIError`, `Zenith_ToString`, `Zenith_GetErrorCategory` | none | as above |
| `Zenith_DeviceHandle.h` | `Zenith_DeviceHandle<TTag>`, `Zenith_DeviceHandleKind`, the handle aliases, `Zenith_RHIResourceHandle`, `Zenith_ToResourceHandle` | none | as above |
| `Zenith_RHICapabilities.h` | `Zenith_RHICapabilityTier`, `Zenith_RHICapabilities`, `Zenith_RHIHeapCapacities`, `Zenith_GetDefaultHeapCapacities` | `Zenith_RHITypes.h` | as above |
| `Zenith_FormatFeatures.h` (generated) | `Zenith_RHIFormatFeatureBit`, `Zenith_RHIFormatFeature`, `axZENITH_RHI_FORMAT_FEATURES`, `Zenith_GetRequiredFormatFeatures` | `Zenith_RHITypes.h` | as above |
| `Zenith_RHIShaderInterface.h` | the shader-interface value types of `Zenith2/Docs/Rendering/ShaderSystem.md → "Generated parameter headers"` and "Shared GPU structs", plus the resource tag types | none | as above, and every generated shader header |
| `Zenith_RHIResources.h` | `Zenith_RHITextureDesc`, `Zenith_RHIBufferDesc`, `Zenith_RHIMemoryRequirements`, `Zenith_RHIHeapDesc`, `Zenith_RHITextureViewDesc`, `Zenith_RHIBufferViewDesc` | `Zenith_RHITypes.h`, `Zenith_DeviceHandle.h` | as above |
| `Zenith_RHIDescriptors.h` | `Zenith_RHIDescriptorSpace`, `Zenith_RHIDescriptorWrite`, `Zenith_RHIDescriptorHeap` | `Zenith_RHIResources.h`, `Zenith_RHIShaderInterface.h` | as above |
| `Zenith_RHILifetime.h` | `uZENITH_RHI_RETIREMENT_CAPACITY`, `Zenith_RHIRetiredKind`, `Zenith_RHIRetiredEntry`, `Zenith_RHIRetirementListener`, `Zenith_RHIRetirementQueue` | `Zenith_RHITypes.h`, `Zenith_DeviceHandle.h`, `Zenith_RHIDescriptors.h` | as above |
| `Zenith_RHIPipelines.h` | `Zenith_RHIVertexFormat`, `Zenith_RHIVertexInputAttribute`, `Zenith_RHIVertexInputStream`, `Zenith_RHIBlendState`, `Zenith_RHIRasterState` and its enums, `Zenith_RHIPipelineCreateResult`, `Zenith_RHIShaderBlob`, `Zenith_RHIGraphicsPipelineDesc`, `Zenith_RHIComputePipelineDesc`, `Zenith_RHIMeshPipelineDesc`, `Zenith_RHIPipelineCache` | `Zenith_RHITypes.h`, `Zenith_DeviceHandle.h` | as above |
| `Zenith_RHIQueue.h` | `Zenith_RHITimeline`, `Zenith_RHITimelineWait`, `Zenith_RHISubmitBatch` | `Zenith_RHITypes.h` | as above |
| `Zenith_RHICommandList.h` | `Zenith_RHICommandList`, `Zenith_RHILoadOp`, `Zenith_RHIStoreOp`, `Zenith_RHIColourAttachment`, `Zenith_RHIDepthAttachment`, `Zenith_RHIRenderingDesc`, `Zenith_RHIVertexBufferBinding`, `Zenith_RHIPushConstants`, `Zenith_RHIBreadcrumbMarker` | `Zenith_RHIResources.h`, `Zenith_RHIQueue.h` | `ZenithFlux/Internal/**`, the backends, RHI tests (ForbiddenCheck rule F20, see `Zenith2/Docs/Design/RHI.md → "Rules across the API"`) |
| `Zenith_RHIBarrier.h` | `Zenith_RHIStage`, `Zenith_RHIAccess`, `Zenith_RHITextureLayout`, `Zenith_RHITextureBarrier`, `Zenith_RHIBufferBarrier`, `Zenith_RHIGlobalBarrier`, `Zenith_RHIBarrierBatch`, `Zenith_RHISplitBarrierID`, the three barrier-recording functions | `Zenith_RHICommandList.h` | `ZenithFlux/Internal/Graph/**`, the backends, RHI tests (rule F20, [Z2-N-REND-006]) |
| `Zenith_RHISwapchain.h` | `Zenith_RHISurfaceTransform`, `Zenith_RHISwapchainDesc`, `Zenith_RHISwapchainInfo`, `Zenith_RHISwapchainImage`, `Zenith_RHIPresentTiming`, `Zenith_RHISwapchain` | `Zenith_RHIResources.h`; forward-declares `Zenith_Window` | as `Zenith_RHITypes.h` |
| `Zenith_RHIQueries.h` | `Zenith_RHIQueryPoolDesc`, `Zenith_RHIClockCalibration` | `Zenith_RHITypes.h`, `Zenith_DeviceHandle.h` | as above |
| `Zenith_RHIDebug.h` | `Zenith_RHIValidationMode`, `Zenith_RHIValidationSeverity`, `Zenith_RHIValidationMessage`, `Zenith_RHIValidationSink`, `Zenith_RHICaptureToolKind`, `Zenith_RHICaptureTool`, `Zenith_RHIBreadcrumbReport`, `Zenith_RHIDeviceFaultReport` | `Zenith_RHITypes.h` | as above |
| `Zenith_RHIFaultInjection.h` | `Zenith_RHIFaultSite`, `Zenith_RHIFaultInjector` | `Zenith_RHITypes.h` | as above |
| `Zenith_RHIConfig.h` | `Zenith_GetRHIConfigKeys`, `Zenith_RHIConfigValues`, `Zenith_ReadRHIConfig` | `Zenith_RHIDebug.h`; ZenithCore `Zenith_ConfigStore.h` | as above |
| `Zenith_RHIDevice.h` | `Zenith_RHIDeviceDesc`, `Zenith_RHIDeviceReport`, `Zenith_RHIDeviceStatus`, `Zenith_RHIDevice`, `Zenith_PfnCreateDevice` | every header above except `Zenith_RHIBarrier.h` and `Zenith_RHICommandList.h`, which it forward-declares | as `Zenith_RHITypes.h` |

| Backend header | Declares | Includable by |
|---|---|---|
| `Zenith2/Engine/ZenithRHI_Vulkan/Zenith_RHI_Vulkan.h` | `Zenith_CreateRHI_Vulkan` | the composition root, test, benchmark, fuzz and tool executables ([Z2-N-CONTRACT-042], [Z2-N-CONTRACT-050]) |
| `Zenith2/Engine/ZenithRHI_D3D12/Zenith_RHI_D3D12.h` | `Zenith_CreateRHI_D3D12`, `uZENITH_D3D12_AGILITY_SDK_VERSION`, `szZENITH_D3D12_AGILITY_SDK_PATH` | as above |

| RHI-owned ZenithFlux public header (`Zenith2/Engine/ZenithFlux/`) | Declares |
|---|---|
| `Flux_QueueValues.h` | `Flux_QueueClass`, `Flux_QueueLastUse`, `Flux_QueueCompletedValues`, `Flux_ToRHIQueue`, `Flux_ToRHIValues` |
| `Flux_PersistentResources.h` | `Flux_PersistentTextureDesc`, `Flux_PersistentBufferDesc`, `Flux_UploadSource`, `Flux_ResourceError`, `Flux_PersistentReadOnlyTexture`, `Flux_PersistentReadOnlyBuffer`, `Flux_PersistentResources` |
| `Flux_Readback.h` | `Flux_ReadbackStatus`, `Flux_ReadbackError`, `Flux_ReadbackHandle` |

The headers from `Zenith_RHICommandList.h` onward, the backend headers and the ZenithFlux headers are specified in `Zenith2/Docs/Design/RHI/PublicAPIDevice.md`. Every header has `#pragma once`, includes only the headers of its row plus `ZenithCore/Zenith_Types.h`, `ZenithCore/Zenith_Result.h` and `ZenithCore/Zenith_Span.h` where it names them, and compiles alone in the HeaderSelfContained gate step ([Z2-N-CONTRACT-027]).

## Zenith_RHITypes header

```cpp
// Zenith2/Engine/ZenithRHI/Zenith_RHITypes.h
#pragma once
#include "ZenithCore/Zenith_Types.h"

/// @contract enum Zenith_RHIBackend
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Values are written into pipeline-cache file names and telemetry; they never change.
/// @endcontract
enum Zenith_RHIBackend : uint8
{
	ZENITH_RHI_BACKEND_VULKAN = 1,
	ZENITH_RHI_BACKEND_D3D12 = 2,
};

/// @contract enum Zenith_RHIQueue
/// Ownership: Plain enumeration; the index of a queue class in every per-queue array.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Source API; Flux_QueueClass has identical values ([Z2-N-REND-485]).
/// @endcontract
enum Zenith_RHIQueue : uint8
{
	ZENITH_RHI_QUEUE_GRAPHICS = 0,
	ZENITH_RHI_QUEUE_ASYNC_COMPUTE = 1,
	ZENITH_RHI_QUEUE_COPY = 2,
	ZENITH_RHI_QUEUE_COUNT = 3,
};

/// @contract value Zenith_RHIQueueValues
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: Merge folds values into it in place.
/// Invalidation: None.
/// Units: Timeline values; 0 means "never used on that queue" in a last-use record and "nothing completed" in a
///   completed record.
/// Errors: None.
/// Version: since Z2 Stage 2. Never serialized.
/// @endcontract
struct Zenith_RHIQueueValues
{
	uint64 m_aulValue[ZENITH_RHI_QUEUE_COUNT] = {};
	constexpr bool IsSatisfiedBy(const Zenith_RHIQueueValues& xCompleted) const
	{
		return m_aulValue[0] <= xCompleted.m_aulValue[0] && m_aulValue[1] <= xCompleted.m_aulValue[1] && m_aulValue[2] <= xCompleted.m_aulValue[2];
	}
	constexpr void Merge(const Zenith_RHIQueueValues& xOther);    // per-queue maximum
};
```

`Zenith_RHIFormat` is exactly the enumeration of `Zenith2/Docs/Rendering.md → "GPU baseline and profiles"` (heading "Format feature header" of that section), carried in this header with this contract block:

```cpp
/// @contract enum Zenith_RHIFormat
/// Ownership: Plain enumeration; the one format enumeration of ZenithRHI and Flux ([Z2-N-REND-041]).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a device lacking a non-optional format row is refused at creation.
/// Version: since Z2 Stage 2. Enumerator values are serialized in PSO keys (Flux_AttachmentFormats) and the cooked
///   precache file, so an enumerator is only ever appended before ZENITH_RHI_FORMAT_COUNT, never renumbered.
/// @endcontract
enum Zenith_RHIFormat : uint8 { /* ZENITH_RHI_FORMAT_UNDEFINED = 0 … ZENITH_RHI_FORMAT_COUNT, as Rendering.md */ };
```

```cpp
/// @contract value Zenith_RHIExtent3D
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned as a field of the descriptors and records that hold it.
/// Invalidation: None.
/// Units: Texels.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIExtent3D { uint32 m_uWidth = 1; uint32 m_uHeight = 1; uint32 m_uDepth = 1; };

/// @contract value Zenith_RHIRect
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned as a field of the descriptors and records that hold it.
/// Invalidation: None.
/// Units: Pixels; the origin is the top-left texel on both APIs.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIRect { sint32 m_iX = 0; sint32 m_iY = 0; uint32 m_uWidth = 0; uint32 m_uHeight = 0; };

/// @contract value Zenith_RHIViewport
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned as a field of the descriptors and records that hold it.
/// Invalidation: None.
/// Units: Pixels for the rectangle; depth range in [0, 1]. The rectangle is given top-left-origin with a positive
///   height on every backend; the Vulkan backend applies the negative-height flip itself (see
///   Zenith2/Docs/Design/RHI/Vulkan.md "Command recording mapping").
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIViewport { float m_fX = 0.0f; float m_fY = 0.0f; float m_fWidth = 0.0f; float m_fHeight = 0.0f; float m_fMinDepth = 0.0f; float m_fMaxDepth = 1.0f; };

/// @contract value Zenith_RHIClearValue
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned as a field of the descriptors and records that hold it.
/// Invalidation: None.
/// Units: Colour channels in the attachment format's numeric domain; depth in [0, 1]; stencil 0..255.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIClearValue { float m_afColour[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; uint32 m_auColour[4] = { 0, 0, 0, 0 }; bool m_bInteger = false; float m_fDepth = 1.0f; uint8 m_uStencil = 0; };

/// @contract enum Zenith_RHITextureDimension
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Flux_TextureDimension maps one to one.
/// @endcontract
enum Zenith_RHITextureDimension : uint8
{
	ZENITH_RHI_TEXTURE_DIMENSION_2D = 1,
	ZENITH_RHI_TEXTURE_DIMENSION_2D_ARRAY,
	ZENITH_RHI_TEXTURE_DIMENSION_3D,
	ZENITH_RHI_TEXTURE_DIMENSION_CUBE,
	ZENITH_RHI_TEXTURE_DIMENSION_CUBE_ARRAY,
};

/// @contract enum Zenith_RHITextureUsage
/// Ownership: Plain bit mask enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a usage the format does not grant (Zenith_FormatFeatures.h) is ZENITH_RHI_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_RHITextureUsage : uint16
{
	ZENITH_RHI_TEXTURE_USAGE_SAMPLED            = 1u << 0,
	ZENITH_RHI_TEXTURE_USAGE_STORAGE            = 1u << 1,
	ZENITH_RHI_TEXTURE_USAGE_COLOUR_ATTACHMENT  = 1u << 2,
	ZENITH_RHI_TEXTURE_USAGE_DEPTH_STENCIL      = 1u << 3,
	ZENITH_RHI_TEXTURE_USAGE_TRANSFER_SRC       = 1u << 4,
	ZENITH_RHI_TEXTURE_USAGE_TRANSFER_DST       = 1u << 5,
	ZENITH_RHI_TEXTURE_USAGE_MUTABLE_FORMAT     = 1u << 6,   // requires m_eStorageViewFormat ([Z2-N-REND-039])
	ZENITH_RHI_TEXTURE_USAGE_CONCURRENT_QUEUES  = 1u << 7,   // Vulkan VK_SHARING_MODE_CONCURRENT across the used families
	ZENITH_RHI_TEXTURE_USAGE_PRESENT            = 1u << 8,   // swapchain images only; never set by a caller
};

/// @contract enum Zenith_RHIBufferUsage
/// Ownership: Plain bit mask enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. The usage union is part of every physical-resource cache key ([Z2-N-REND-287]).
/// @endcontract
enum Zenith_RHIBufferUsage : uint16
{
	ZENITH_RHI_BUFFER_USAGE_VERTEX         = 1u << 0,
	ZENITH_RHI_BUFFER_USAGE_INDEX          = 1u << 1,
	ZENITH_RHI_BUFFER_USAGE_INDIRECT       = 1u << 2,
	ZENITH_RHI_BUFFER_USAGE_STORAGE_READ   = 1u << 3,
	ZENITH_RHI_BUFFER_USAGE_STORAGE_WRITE  = 1u << 4,
	ZENITH_RHI_BUFFER_USAGE_TRANSFER_SRC   = 1u << 5,
	ZENITH_RHI_BUFFER_USAGE_TRANSFER_DST   = 1u << 6,
	ZENITH_RHI_BUFFER_USAGE_BREADCRUMB     = 1u << 7,   // AMD buffer-marker target; set by the RHI itself
};

/// @contract enum Zenith_RHIMemoryClass
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_RHIMemoryClass : uint8
{
	ZENITH_RHI_MEMORY_CLASS_DEVICE_LOCAL = 1,       // not host visible
	ZENITH_RHI_MEMORY_CLASS_DEVICE_DEDICATED,       // device local, one allocation per resource (persistent render targets)
	ZENITH_RHI_MEMORY_CLASS_UPLOAD,                 // host visible, write-combined, coherent; device local where exposed
	ZENITH_RHI_MEMORY_CLASS_READBACK,               // host visible, cached, coherent
};

/// @contract enum Zenith_RHIAdapterPreference
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_RHIAdapterPreference : uint8
{
	ZENITH_RHI_ADAPTER_PREFERENCE_HIGH_PERFORMANCE = 1,   // the default: discrete before integrated before virtual
	ZENITH_RHI_ADAPTER_PREFERENCE_SOFTWARE,               // WARP on D3D12, the lavapipe CPU device on Vulkan
};

/// @contract enum Zenith_RHIWaitStatus
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a timeout is a status, device loss is an error.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_RHIWaitStatus : uint8
{
	ZENITH_RHI_WAIT_STATUS_REACHED = 1,
	ZENITH_RHI_WAIT_STATUS_TIMEOUT = 2,
};

/// @contract enum Zenith_RHIPresentMode
/// Ownership: Plain enumeration; the value of rhi.presentMode.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; an unsupported mode falls back to FIFO (Zenith2/Docs/Design/RHI/Recovery.md "Present modes and formats").
/// Version: since Z2 Stage 2. Declared here, not in Zenith_RHISwapchain.h, so Zenith_RHIConfig.h can type its values.
/// @endcontract
enum Zenith_RHIPresentMode : uint8 { ZENITH_RHI_PRESENT_MODE_FIFO = 1, ZENITH_RHI_PRESENT_MODE_FIFO_RELAXED, ZENITH_RHI_PRESENT_MODE_MAILBOX, ZENITH_RHI_PRESENT_MODE_IMMEDIATE };

/// @contract enum Zenith_RHIOutputColourSpace
/// Ownership: Plain enumeration; the value of rhi.output.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; an unsupported output falls back to SDR.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_RHIOutputColourSpace : uint8 { ZENITH_RHI_OUTPUT_SDR_SRGB = 1, ZENITH_RHI_OUTPUT_HDR10_ST2084, ZENITH_RHI_OUTPUT_SCRGB_LINEAR };

/// @contract value Zenith_RHISubresourceRange
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned as a field of the descriptors and records that hold it.
/// Invalidation: None.
/// Units: Mip levels and array layers; uMipCount or uLayerCount of 0xFFFFFFFF means "to the end".
/// Errors: A range outside its texture is ZENITH_ASSERT in the backend (the graph checks ranges before recording).
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHISubresourceRange
{
	uint32 m_uFirstMip = 0;
	uint32 m_uMipCount = 0xFFFFFFFFu;
	uint32 m_uFirstLayer = 0;
	uint32 m_uLayerCount = 0xFFFFFFFFu;
	bool m_bDepthAspect = true;     // depth formats: depth and stencil aspects are tracked together ([Z2-N-REND-385])
	bool m_bStencilAspect = true;
};
```

## Zenith_RHIError header

```cpp
// Zenith2/Engine/ZenithRHI/Zenith_RHIError.h
#pragma once
#include "ZenithCore/Zenith_ErrorCategory.h"

/// @contract enum Zenith_RHIError
/// Ownership: Plain enumeration; the one error enumeration of ZenithRHI and both backends.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Each enumerator is returned exactly where the table in Zenith2/Docs/Design/RHI.md "Error codes" states;
///   the detail (the missing feature list, the adapter name, the API result code) is logged at the point of
///   failure and, for device creation, copied into Zenith_RHIDeviceReport.
/// Version: since Z2 Stage 2. An enumerator keeps its meaning once shipped ([Z2-N-CONTRACT-467]).
/// @endcontract
enum Zenith_RHIError : uint8
{
	ZENITH_RHI_ERROR_LOADER_UNAVAILABLE = 1,      // vulkan-1 / libvulkan.so, d3d12.dll or the Agility runtime could not be loaded
	ZENITH_RHI_ERROR_BACKEND_UNSUPPORTED,         // the backend exists but the OS lacks it (D3D12 on a Windows without it)
	ZENITH_RHI_ERROR_NO_ADAPTER,                  // no adapter matched the preference and the name filter
	ZENITH_RHI_ERROR_PROFILE_UNSUPPORTED,         // every candidate adapter lacks a baseline row ([Z2-N-REND-020], [Z2-N-REND-024])
	ZENITH_RHI_ERROR_DEVICE_CREATE,               // vkCreateDevice / D3D12CreateDevice failed on an adapter that passed the baseline
	ZENITH_RHI_ERROR_OUT_OF_HOST_MEMORY,          // the API or the general heap could not allocate host memory
	ZENITH_RHI_ERROR_OUT_OF_DEVICE_MEMORY,        // a device-heap allocation failed (VK_ERROR_OUT_OF_DEVICE_MEMORY, E_OUTOFMEMORY)
	ZENITH_RHI_ERROR_DEVICE_LOST,                 // the device is lost; every subsequent call returns this
	ZENITH_RHI_ERROR_INVALID_DESC,                // a descriptor violates its documented domain (sizes, usages, format features)
	ZENITH_RHI_ERROR_TABLE_FULL,                  // the device resource table of the kind is at its capacity
	ZENITH_RHI_ERROR_STALE_HANDLE,                // a null, stale or foreign device handle was resolved
	ZENITH_RHI_ERROR_DESCRIPTOR_SPACE_FULL,       // a descriptor index space is at its capacity ([Z2-N-BUDGET-261])
	ZENITH_RHI_ERROR_SURFACE_UNAVAILABLE,         // the window has no native surface (Android surface lost, minimised to zero)
	ZENITH_RHI_ERROR_SWAPCHAIN_OUT_OF_DATE,       // acquire or present needs a swapchain recreation
	ZENITH_RHI_ERROR_SWAPCHAIN_CREATE,            // the swapchain could not be created for a valid surface
	ZENITH_RHI_ERROR_PIPELINE_CREATE,             // the driver failed to create a pipeline from valid blobs
	ZENITH_RHI_ERROR_SHADER_BLOB_INVALID,         // a blob is for another API, empty, or fails the driver's module check
	ZENITH_RHI_ERROR_PIPELINE_CACHE_REJECTED,     // a pipeline-cache payload does not match this device, driver or version
	ZENITH_RHI_ERROR_TIMEOUT,                     // a bounded API wait expired where a timeout is an error (swapchain acquire)
	ZENITH_RHI_ERROR_FEATURE_UNAVAILABLE,         // an optional capability is absent (mesh shaders, present wait, capture tool, validation layers)
};
constexpr const char* Zenith_ToString(Zenith_RHIError eError);
constexpr Zenith_ErrorCategory Zenith_GetErrorCategory(Zenith_RHIError eError);
```

| Enumerator | `Zenith_ErrorCategory` |
|---|---|
| `LOADER_UNAVAILABLE`, `DEVICE_CREATE`, `DEVICE_LOST`, `SURFACE_UNAVAILABLE`, `SWAPCHAIN_OUT_OF_DATE`, `SWAPCHAIN_CREATE`, `PIPELINE_CREATE` | `ERROR_CATEGORY_UNAVAILABLE` |
| `BACKEND_UNSUPPORTED`, `PROFILE_UNSUPPORTED`, `FEATURE_UNAVAILABLE` | `ERROR_CATEGORY_UNSUPPORTED` |
| `NO_ADAPTER` | `ERROR_CATEGORY_NOT_FOUND` |
| `OUT_OF_HOST_MEMORY`, `OUT_OF_DEVICE_MEMORY` | `ERROR_CATEGORY_OUT_OF_MEMORY` |
| `INVALID_DESC` | `ERROR_CATEGORY_INVALID_ARGUMENT` |
| `TABLE_FULL`, `DESCRIPTOR_SPACE_FULL` | `ERROR_CATEGORY_CAPACITY_EXCEEDED` |
| `STALE_HANDLE` | `ERROR_CATEGORY_STALE_HANDLE` |
| `SHADER_BLOB_INVALID` | `ERROR_CATEGORY_CORRUPT_DATA` |
| `PIPELINE_CACHE_REJECTED` | `ERROR_CATEGORY_VERSION_MISMATCH` |
| `TIMEOUT` | `ERROR_CATEGORY_TIMEOUT` |

`Zenith_ToString` returns the enumerator name without the `ZENITH_RHI_ERROR_` prefix in lower case with `_` replaced by `-` (for example `profile-unsupported`); logs, tests and the device report use it.

## Zenith_DeviceHandle header

The handle layout is `Zenith2/Docs/Contracts.md → "Handle kinds"` (heading "Zenith_DeviceHandle"): slot bits 0–31, generation bits 32–55, kind bits 56–63. This header adds the kind enumeration, the aliases every consumer uses, the kind-erased resource handle, and the accessors the RHI's own table needs.

```cpp
// Zenith2/Engine/ZenithRHI/Zenith_DeviceHandle.h
#pragma once
#include "ZenithCore/Zenith_Types.h"

/// @contract enum Zenith_DeviceHandleKind
/// Ownership: Plain enumeration; bits 56–63 of every device handle.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Never serialized.
/// @endcontract
enum Zenith_DeviceHandleKind : uint8
{
	DEVICE_HANDLE_KIND_TEXTURE = 1,
	DEVICE_HANDLE_KIND_BUFFER = 2,
	DEVICE_HANDLE_KIND_SAMPLER = 3,        // reserved: samplers are the static table ([Z2-N-REND-057]); never issued
	DEVICE_HANDLE_KIND_PIPELINE = 4,
	DEVICE_HANDLE_KIND_HEAP = 5,
	DEVICE_HANDLE_KIND_QUERY_POOL = 6,
};

// elaboration: tag names Zenith_RHI*Tag (Contracts' Flux_*Tag spelling), pending X73; the Flux_*Handle aliases below are unchanged
struct Zenith_RHITextureTag   { static constexpr Zenith_DeviceHandleKind eKIND = DEVICE_HANDLE_KIND_TEXTURE; };
struct Zenith_RHIBufferTag    { static constexpr Zenith_DeviceHandleKind eKIND = DEVICE_HANDLE_KIND_BUFFER; };
struct Zenith_RHISamplerTag   { static constexpr Zenith_DeviceHandleKind eKIND = DEVICE_HANDLE_KIND_SAMPLER; };
struct Zenith_RHIPipelineTag  { static constexpr Zenith_DeviceHandleKind eKIND = DEVICE_HANDLE_KIND_PIPELINE; };
struct Zenith_RHIHeapTag { static constexpr Zenith_DeviceHandleKind eKIND = DEVICE_HANDLE_KIND_HEAP; };
struct Zenith_RHIQueryPoolTag { static constexpr Zenith_DeviceHandleKind eKIND = DEVICE_HANDLE_KIND_QUERY_POOL; };

/// @contract value Zenith_DeviceHandle
/// Ownership: Plain value naming one RHI resource owned by the device (Zenith2/Docs/Contracts.md "Handle kinds").
///   Copying extends nothing; the creator destroys the resource through the retirement queue.
/// Thread affinity: IMMUTABLE. Resolving it is ANY_THREAD inside the device.
///   Members non-const: it is copy-assigned into descriptors, records and tables.
/// Invalidation: Stale once the resource's slot is freed at retirement (generation compare) and after the device is
///   destroyed; resolving a stale, null or foreign-kind handle returns ZENITH_RHI_ERROR_STALE_HANDLE in every build.
/// Units: None.
/// Errors: None in the handle itself.
/// Version: since Z2 Stage 2. Never serialized; the bit layout may change in any build ([Z2-N-CONTRACT-063]).
/// @endcontract
template<typename TTag>
class Zenith_DeviceHandle
{
public:
	constexpr Zenith_DeviceHandle() = default;
	constexpr bool IsNull() const { return m_ulBits == 0; }
	friend constexpr bool operator==(Zenith_DeviceHandle, Zenith_DeviceHandle) = default;
	constexpr uint64 GetBitsForHash() const { return m_ulBits; }   // elaboration: hash keys and dumps only
private:
	friend class Zenith_RHIHandleCodec;                                                     // elaboration: Internal encoder
	explicit constexpr Zenith_DeviceHandle(uint64 ulBits) : m_ulBits(ulBits) {}
	uint64 m_ulBits = 0;
};

using Flux_TextureHandle  = Zenith_DeviceHandle<Zenith_RHITextureTag>;
using Flux_BufferHandle   = Zenith_DeviceHandle<Zenith_RHIBufferTag>;
using Flux_SamplerHandle  = Zenith_DeviceHandle<Zenith_RHISamplerTag>;
using Flux_PipelineHandle = Zenith_DeviceHandle<Zenith_RHIPipelineTag>;
using Zenith_RHITextureHandle   = Flux_TextureHandle;        // one type, two names ([Z2-N-REND-294] uses the RHI name)
using Zenith_RHIBufferHandle    = Flux_BufferHandle;
using Zenith_RHIPipelineHandle  = Flux_PipelineHandle;
using Zenith_RHIHeapHandle      = Zenith_DeviceHandle<Zenith_RHIHeapTag>;
using Zenith_RHIQueryPoolHandle = Zenith_DeviceHandle<Zenith_RHIQueryPoolTag>;

/// @contract value Zenith_RHIResourceHandle
/// Ownership: Plain value; the kind-erased form of any device handle, used where one list holds resources of every
///   kind (the retirement queue, the imported-state table, residency reports, frame dumps).
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned into descriptors, records and tables.
/// Invalidation: As the handle it was made from; the kind bits are kept, so resolution still checks the kind.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Never serialized.
/// @endcontract
class Zenith_RHIResourceHandle
{
public:
	constexpr Zenith_RHIResourceHandle() = default;
	constexpr bool IsNull() const { return m_ulBits == 0; }
	constexpr Zenith_DeviceHandleKind GetKind() const { return static_cast<Zenith_DeviceHandleKind>(m_ulBits >> 56); }
	constexpr uint64 GetBitsForHash() const { return m_ulBits; }
	friend constexpr bool operator==(Zenith_RHIResourceHandle, Zenith_RHIResourceHandle) = default;
private:
	friend class Zenith_RHIHandleCodec;
	template<typename TTag> friend constexpr Zenith_RHIResourceHandle Zenith_ToResourceHandle(Zenith_DeviceHandle<TTag>);
	uint64 m_ulBits = 0;
};

// Explicit, never implicit ([Z2-N-CONTRACT-061]).
template<typename TTag>
constexpr Zenith_RHIResourceHandle Zenith_ToResourceHandle(Zenith_DeviceHandle<TTag> xHandle);
```

`Zenith_RHIHandleCodec` is declared in `Zenith2/Engine/ZenithRHI/Internal/Zenith_RHIHandleCodec.h` (encode `{slot, generation, kind}` to bits and back); it is the only code that reads or writes `m_ulBits`. A backend never includes that header directly: it includes `Zenith2/Engine/ZenithRHI/Internal/Zenith_RHIDeviceBase.h`, the one `Internal/Zenith_*Base.h` header a backend may include from its API library (X68, `Zenith2/Docs/Design/RHI.md → "ZenithRHI target"`), which includes the codec and the resource table.

## Zenith_RHICapabilities header

`Zenith_RHICapabilities` is the struct of `Zenith2/Docs/Rendering.md → "GPU baseline and profiles"` (heading "Capability negotiation"), unchanged in member order; `ZENITH_RHI_QUEUE_COUNT` is the queue count of `Zenith_RHITypes.h`. This header adds the tier enumeration (declared here because the struct holds it and ZenithRHI cannot depend on ZenithFlux; named `Zenith_RHICapabilityTier` rather than Rendering's `Flux_CapabilityTier`, pending X73, with Flux keeping the old name as `using Flux_CapabilityTier = Zenith_RHICapabilityTier;` in `ZenithFlux/Flux_QueueValues.h` until Rendering is patched), the heap capacity record and three members the backends fill for Flux's own derived checks.

```cpp
// Zenith2/Engine/ZenithRHI/Zenith_RHICapabilities.h
#pragma once
#include "ZenithRHI/Zenith_RHITypes.h"

/// @contract enum Zenith_RHICapabilityTier
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. ZENITH_RHI_CAPABILITY_TIER_MOBILE on Android, ZENITH_RHI_CAPABILITY_TIER_DESKTOP on Windows ([Z2-N-REND-028]).
/// @endcontract
enum Zenith_RHICapabilityTier : uint8
{
	ZENITH_RHI_CAPABILITY_TIER_DESKTOP = 1,
	ZENITH_RHI_CAPABILITY_TIER_MOBILE = 2,
};

/// @contract value Zenith_RHIHeapCapacities
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned as a field of the descriptors and records that hold it.
/// Invalidation: None.
/// Units: Descriptors per index space; samplers are the 16 static entries.
/// Errors: None; a device whose update-after-bind limits are below these is refused ([Z2-N-REND-030]).
/// Version: since Z2 Stage 2. The values are those of Z2-B-RHI-009 once adopted; until then the proposed values of
///   Rendering.md "Heap capacities per capability tier".
/// @endcontract
struct Zenith_RHIHeapCapacities
{
	uint32 m_uTextureSRV = 0;
	uint32 m_uTextureUAV = 0;
	uint32 m_uBufferSRV = 0;
	uint32 m_uBufferUAV = 0;
	uint32 m_uStaticSamplers = 16;
};
constexpr Zenith_RHIHeapCapacities Zenith_GetDefaultHeapCapacities(Zenith_RHICapabilityTier eTier);   // 65,536/8,192/65,536/16,384 desktop; 16,384/2,048/16,384/4,096 mobile

/// @contract value Zenith_RHICapabilities
/// Ownership: Filled once by the backend at device creation and owned by the device; consumers read it through
///   Zenith_RHIDevice::GetCapabilities() for the device's lifetime.
/// Thread affinity: IMMUTABLE after device creation.
///   Members non-const: device creation fills it field by field.
/// Invalidation: Invalid after the device is destroyed; a recreated device (device-loss recovery) fills a new one,
///   which may differ only in m_ulDriverVersion when the driver was updated underneath the process.
/// Units: Bytes for memory sizes; bits for timestamp validity; lanes for subgroup sizes.
/// Errors: None.
/// Version: since Z2 Stage 2. Members are appended only; the struct is never serialized except into the device
///   report JSON (Zenith2/Docs/Design/RHI.md "Runtime file formats").
/// @endcontract
struct Zenith_RHICapabilities
{
	// the members of Rendering.md "Capability negotiation", in that order, then:
	char m_acAdapterName[128];                        // elaboration: UTF-8, NUL-terminated
	Zenith_RHIHeapCapacities m_xHeapCapacities;       // elaboration: the capacities the device was created with
	uint32 m_uAgilitySDKVersion;                     // elaboration: D3D12: the loaded Agility SDK version; 0 on Vulkan
	bool m_bSoftwareAdapter;                          // elaboration: WARP or lavapipe (a correctness host, [Z2-N-BUDGET-125])
	bool m_bTearingSupported;                         // elaboration: IMMEDIATE present mode / DXGI tearing
	bool m_bHdrOutput;                                // elaboration: an HDR10 or scRGB swapchain colour space is exposed
	bool m_bHdrMetadata;                              // elaboration (LIGHT's L21): Vulkan VK_EXT_hdr_metadata enabled; always true on D3D12 (IDXGISwapChain4)
};
```

Which colour spaces a given surface supports, and whether its display reports HDR, is per window rather than per device, so it is reported per swapchain (`Zenith_RHISwapchainInfo::m_uOutputColourSpacesSupported`, `m_bDisplayHdr`, `m_fDisplayMaxLuminanceNits`; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md → "Zenith_RHISwapchain header"`), not here.

## Zenith_FormatFeatures header

The generated header's declarations, the grammar of its source table and its generator are `Zenith2/Docs/Rendering.md → "GPU baseline and profiles"` (heading "Format feature header"). This design adds one token to the grammar (pending X75) and fixes the committed table, which lives under the library's `Internal/` directory (X69: the library root holds headers, `.inl` files and `CMakeLists.txt` alone, [Z2-N-CONTRACT-021]); the generated `Zenith_FormatFeatures.h` stays a public root header.

<!-- elaboration -->
| Grammar addition | Rule |
|---|---|
| `platform=<windows\|android>` | an optional last token (after `optional` when both appear); the row is checked and generated for that platform only, which is how the BC7 rows (Windows, `textureCompressionBC`) and the ASTC rows (Android, `textureCompressionASTC_LDR`) of the "Required format features" table are expressed; `Zenith_RHIFormatFeature` gains `uint8 m_uPlatformMask` (bit 0 Windows, bit 1 Android, 3 for both) |

The committed `Zenith2/Engine/ZenithRHI/Internal/FormatFeatures.table`, one line per row of Rendering's table:

```text
# Zenith2 required format features; a change is a profile change (Zenith_FormatFeatures.h, VP_ZENITH2_baseline.json).
ZENITH_RHI_FORMAT_R11G11B10_FLOAT       VK_FORMAT_B10G11R11_UFLOAT_PACK32 DXGI_FORMAT_R11G11B10_FLOAT     SLCBWT
ZENITH_RHI_FORMAT_R16G16B16A16_FLOAT    VK_FORMAT_R16G16B16A16_SFLOAT     DXGI_FORMAT_R16G16B16A16_FLOAT  SLCBWT
ZENITH_RHI_FORMAT_R16G16_FLOAT          VK_FORMAT_R16G16_SFLOAT           DXGI_FORMAT_R16G16_FLOAT        SLCWT
ZENITH_RHI_FORMAT_R16G16_UNORM          VK_FORMAT_R16G16_UNORM            DXGI_FORMAT_R16G16_UNORM        SLCBW
ZENITH_RHI_FORMAT_R16_FLOAT             VK_FORMAT_R16_SFLOAT              DXGI_FORMAT_R16_FLOAT           SLW
ZENITH_RHI_FORMAT_R8_UNORM              VK_FORMAT_R8_UNORM                DXGI_FORMAT_R8_UNORM            SLCW
ZENITH_RHI_FORMAT_R8G8B8A8_UNORM        VK_FORMAT_R8G8B8A8_UNORM          DXGI_FORMAT_R8G8B8A8_UNORM      SLCBW
ZENITH_RHI_FORMAT_R8G8B8A8_UNORM_SRGB   VK_FORMAT_R8G8B8A8_SRGB           DXGI_FORMAT_R8G8B8A8_UNORM_SRGB SLCB
ZENITH_RHI_FORMAT_R32_UINT              VK_FORMAT_R32_UINT                DXGI_FORMAT_R32_UINT            SCWT
ZENITH_RHI_FORMAT_D32_FLOAT_S8X24_UINT  VK_FORMAT_D32_SFLOAT_S8_UINT      DXGI_FORMAT_D32_FLOAT_S8X24_UINT SLD
ZENITH_RHI_FORMAT_D24_UNORM_S8_UINT     VK_FORMAT_D24_UNORM_S8_UINT       DXGI_FORMAT_D24_UNORM_S8_UINT   SLD optional
ZENITH_RHI_FORMAT_B8G8R8A8_UNORM_SRGB   VK_FORMAT_B8G8R8A8_SRGB           DXGI_FORMAT_B8G8R8A8_UNORM_SRGB C
ZENITH_RHI_FORMAT_R10G10B10A2_UNORM     VK_FORMAT_A2B10G10R10_UNORM_PACK32 DXGI_FORMAT_R10G10B10A2_UNORM  C
ZENITH_RHI_FORMAT_BC7_UNORM             VK_FORMAT_BC7_UNORM_BLOCK         DXGI_FORMAT_BC7_UNORM           SL platform=windows
ZENITH_RHI_FORMAT_BC7_UNORM_SRGB        VK_FORMAT_BC7_SRGB_BLOCK          DXGI_FORMAT_BC7_UNORM_SRGB      SL platform=windows
ZENITH_RHI_FORMAT_ASTC_4X4_UNORM        VK_FORMAT_ASTC_4x4_UNORM_BLOCK    -                               SL platform=android
ZENITH_RHI_FORMAT_ASTC_4X4_UNORM_SRGB   VK_FORMAT_ASTC_4x4_SRGB_BLOCK     -                               SL platform=android
ZENITH_RHI_FORMAT_R16_UNORM             VK_FORMAT_R16_UNORM               DXGI_FORMAT_R16_UNORM           S
```

The last row, `ZENITH_RHI_FORMAT_R16_UNORM`, is GEOM's terrain height tiles (sampled with texel `Load` alone, no `L`; cross-area note GM8). Rendering's "Required format features" carries the row and appends the enumerator before `ZENITH_RHI_FORMAT_COUNT` (cross-document item X130, applied), so the committed `FormatFeatures.table` and `VP_ZENITH2_baseline.json` carry it from their first commit, and `Test_FormatFeatures_TableMatchesRendering` and `Test_FormatFeatures_ProfileMatchesHeader` stay green ("Reconciliation items" R21 of `Zenith2/Docs/Design/RHI.md`).

The depth rows' `S` and `L` mean sampling of the depth aspect (D3D12 through the `R32_FLOAT_X8X24_TYPELESS` and `R24_UNORM_X8_TYPELESS` SRV formats), which the linear comparison sampler `uFLUX_SAMPLER_SHADOW_LESS_EQUAL` needs; the swapchain rows need colour attachment alone, because the UI composite writes scene colour before the present copy (`Zenith2/Docs/Rendering.md → "Frame anatomy common epilogue"`). The generator reads `ZENITH_TARGET_OS` from the configure and emits the rows of every platform, each with its mask, so one header serves both; the device checks skip rows whose mask excludes the running platform.

## Zenith_RHIShaderInterface header

The members of this header are the declarations of `Zenith2/Docs/Rendering.md → "Binding model"` (heading "DescriptorHandle usage") and `Zenith2/Docs/Rendering/ShaderSystem.md → "Generated parameter headers"`, "Permutations" and "Shared GPU structs": `Zenith_RHIDescriptorHandle`, `Zenith_RHISRV<TTag>`, `Zenith_RHIUAV<TTag>`, `Zenith_RHIParamFieldDesc` and its five enumerations, `Zenith_RHIShaderProgramID`, `Zenith_RHIEntryPointID`, `Zenith_RHIPermutationKey` and the plain-data types `Zenith_RHIFloat2` … `Zenith_RHIFloat4x4`. This design adds the resource tag types, the field-table primary template and the contract blocks:

```cpp
// Zenith2/Engine/ZenithRHI/Zenith_RHIShaderInterface.h (additions)
/// @contract value Zenith_RHIDescriptorHandle
/// Ownership: Plain value holding a space-local heap index; owns no descriptor (the slot is owned by whoever
///   allocated it from Zenith_RHIDescriptorHeap).
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned into descriptors, records and tables.
/// Invalidation: The index is meaningful until its slot is freed and retired; after retirement the slot may be
///   reissued, so a retained value then names another resource. Index 0 is the null substitute ([Z2-N-REND-031]).
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. 8 bytes in every configuration, D-SLANG (open, Stage 1) neutral ([Z2-N-REND-970]).
/// @endcontract
// struct Zenith_RHIDescriptorHandle { uint32 m_uIndex; uint32 m_uReserved; };   (as Rendering.md)

/// @contract value Zenith_RHISRV
/// Ownership: Plain value; a parameter-struct field, virtual graph handle during setup and real descriptor after
///   record ([Z2-N-REND-066]).
/// Thread affinity: IMMUTABLE.
///   Members non-const: the graph patches both words in place at record.
/// Invalidation: The setup form is valid in the frame's graph alone; the record form as Zenith_RHIDescriptorHandle.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
// template<typename TTag> struct Zenith_RHISRV { uint32 m_uWord0; uint32 m_uWord1; };   (as Rendering.md; Zenith_RHIUAV identical)

struct Zenith_RHITex2D { static constexpr Zenith_RHIResourceDim eDIM = ZENITH_RHI_RESOURCE_DIM_TEX2D; static constexpr uint32 uSTRIDE = 0; };
struct Zenith_RHITex2DArray { static constexpr Zenith_RHIResourceDim eDIM = ZENITH_RHI_RESOURCE_DIM_TEX2D_ARRAY; static constexpr uint32 uSTRIDE = 0; };
struct Zenith_RHITex3D { static constexpr Zenith_RHIResourceDim eDIM = ZENITH_RHI_RESOURCE_DIM_TEX3D; static constexpr uint32 uSTRIDE = 0; };
struct Zenith_RHITexCube { static constexpr Zenith_RHIResourceDim eDIM = ZENITH_RHI_RESOURCE_DIM_TEXCUBE; static constexpr uint32 uSTRIDE = 0; };
struct Zenith_RHITexCubeArray { static constexpr Zenith_RHIResourceDim eDIM = ZENITH_RHI_RESOURCE_DIM_TEXCUBE_ARRAY; static constexpr uint32 uSTRIDE = 0; };
struct Zenith_RHIByteBuffer { static constexpr Zenith_RHIResourceDim eDIM = ZENITH_RHI_RESOURCE_DIM_BYTE_BUFFER; static constexpr uint32 uSTRIDE = 0; };
template<typename TElem>
struct Zenith_RHIStructBuffer { static constexpr Zenith_RHIResourceDim eDIM = ZENITH_RHI_RESOURCE_DIM_STRUCT_BUFFER; static constexpr uint32 uSTRIDE = sizeof(TElem); };

/// @contract functions Zenith_RHIParamFieldTable
/// Ownership: Compile-time data; the primary template is undefined, and each generated header specialises it.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Byte offsets.
/// Errors: Using a parameter struct with no generated specialisation fails to compile.
/// Version: since Z2 Stage 2.
/// @endcontract
template<typename TParams> struct Zenith_RHIParamFieldTable;   // specialised by <Program>.generated.h
```

Each tag carries the `eDIM` and `uSTRIDE` that `FLUX_RG_ERROR_VIEW_TYPE_MISMATCH` compares with the view ([Z2-N-REND-067]). The header's plain-data types, IDs, permutation key and field-table types each carry a `value` or `enum` block whose `Version` field reads `since Z2 Stage 2. Layout fixed by Slang's scalar layout; a change bumps the shader blob container version.`

## Zenith_RHIResources header

```cpp
// Zenith2/Engine/ZenithRHI/Zenith_RHIResources.h
#pragma once
#include "ZenithRHI/Zenith_RHITypes.h"
#include "ZenithRHI/Zenith_DeviceHandle.h"

/// @contract value Zenith_RHITextureDesc
/// Ownership: Plain value; copied by the device at creation.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Texels; m_uDepthOrLayers is the depth of a 3D texture and the layer count otherwise (6 × cubes for cube arrays).
/// Errors: A zero extent, a mip count above floor(log2(max extent)) + 1, a usage the format does not grant, a
///   sample count other than one, or MUTABLE_FORMAT without a storage-view format is ZENITH_RHI_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHITextureDesc
{
	const char* m_szName = nullptr;                              // debug name (development builds apply it)
	Zenith_RHITextureDimension m_eDimension = ZENITH_RHI_TEXTURE_DIMENSION_2D;
	Zenith_RHIFormat m_eFormat = ZENITH_RHI_FORMAT_UNDEFINED;
	Zenith_RHIFormat m_eStorageViewFormat = ZENITH_RHI_FORMAT_UNDEFINED;   // the second view format of a mutable-format image
	Zenith_RHIExtent3D m_xExtent;
	uint32 m_uDepthOrLayers = 1;
	uint32 m_uMipCount = 1;
	uint16 m_uUsage = 0;                                        // Zenith_RHITextureUsage bits
	Zenith_RHIMemoryClass m_eMemoryClass = ZENITH_RHI_MEMORY_CLASS_DEVICE_LOCAL;
	Zenith_RHIClearValue m_xOptimisedClear;                      // D3D12 optimised clear; ignored by Vulkan
	uint32 m_uQueueMask = 1u << ZENITH_RHI_QUEUE_GRAPHICS;      // queues that access it (concurrent-sharing families on Vulkan)
};

/// @contract value Zenith_RHIBufferDesc
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Bytes; m_uStructureStride is the element size of a structured view, 0 for byte-address buffers.
/// Errors: m_ulSize 0 or above the device maximum (2 GiB) is ZENITH_RHI_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIBufferDesc
{
	const char* m_szName = nullptr;
	uint64 m_ulSize = 0;
	uint32 m_uStructureStride = 0;
	uint16 m_uUsage = 0;                                        // Zenith_RHIBufferUsage bits
	Zenith_RHIMemoryClass m_eMemoryClass = ZENITH_RHI_MEMORY_CLASS_DEVICE_LOCAL;
	uint32 m_uQueueMask = 1u << ZENITH_RHI_QUEUE_GRAPHICS;      // graph buffers: all three ([Z2-N-REND-391] table)
};

/// @contract value Zenith_RHIMemoryRequirements
/// Ownership: Plain value returned by the requirement queries.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: Valid for the device that produced it.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIMemoryRequirements
{
	uint64 m_ulSize = 0;
	uint64 m_ulAlignment = 0;
	uint32 m_uPlacementGroup = 0;   // opaque: Vulkan (memory type index << 1) | linear, D3D12 heap category (Memory.md "Heap alignment and placement")
};

/// @contract value Zenith_RHIHeapDesc
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Bytes; m_ulSize is a multiple of 64 KiB.
/// Errors: A zero size, a size that is not a 64 KiB multiple, or a placement group the device never reported is
///   ZENITH_RHI_ERROR_INVALID_DESC; exhaustion is ZENITH_RHI_ERROR_OUT_OF_DEVICE_MEMORY.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIHeapDesc
{
	const char* m_szName = nullptr;
	uint64 m_ulSize = 0;
	uint32 m_uPlacementGroup = 0;
};

/// @contract value Zenith_RHITextureViewDesc
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Mip levels and layers.
/// Errors: A storage view naming more than one mip ([Z2-N-REND-051]) or a view format the texture was not created
///   with is ZENITH_RHI_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHITextureViewDesc
{
	Zenith_RHITextureHandle m_xTexture;
	Zenith_RHIResourceDim m_eDim = ZENITH_RHI_RESOURCE_DIM_TEX2D;
	Zenith_RHIFormat m_eFormat = ZENITH_RHI_FORMAT_UNDEFINED;    // UNDEFINED: the texture's format
	Zenith_RHISubresourceRange m_xRange;
	bool m_bStorage = false;
};

/// @contract value Zenith_RHIBufferViewDesc
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Bytes; offsets are multiples of 16 for byte-address views and of the stride for structured views.
/// Errors: A range outside the buffer or a misaligned offset is ZENITH_RHI_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIBufferViewDesc
{
	Zenith_RHIBufferHandle m_xBuffer;
	uint64 m_ulOffset = 0;
	uint64 m_ulSize = 0;                     // 0: to the end of the buffer
	uint32 m_uStructureStride = 0;           // 0: byte-address
	bool m_bWritable = false;
};
```

## Zenith_RHIDescriptors header

```cpp
// Zenith2/Engine/ZenithRHI/Zenith_RHIDescriptors.h
#pragma once
#include "ZenithRHI/Zenith_RHIResources.h"
#include "ZenithRHI/Zenith_RHIShaderInterface.h"

/// @contract enum Zenith_RHIDescriptorSpace
/// Ownership: Plain enumeration; Flux_DescriptorSpace has identical values and order ([Z2-N-REND-050] table).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. The order is the D3D12 heap layout order ([Z2-N-REND-056]).
/// @endcontract
enum Zenith_RHIDescriptorSpace : uint8
{
	ZENITH_RHI_DESCRIPTOR_SPACE_TEXTURE_SRV = 0,
	ZENITH_RHI_DESCRIPTOR_SPACE_TEXTURE_UAV = 1,
	ZENITH_RHI_DESCRIPTOR_SPACE_BUFFER_SRV = 2,
	ZENITH_RHI_DESCRIPTOR_SPACE_BUFFER_UAV = 3,
	ZENITH_RHI_DESCRIPTOR_SPACE_COUNT = 4,
};

/// @contract value Zenith_RHIDescriptorWrite
/// Ownership: Plain value; exactly one of the two views is used, chosen by the target space.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: None.
/// Errors: A view whose kind does not match the space is ZENITH_RHI_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIDescriptorWrite
{
	Zenith_RHITextureViewDesc m_xTextureView;
	Zenith_RHIBufferViewDesc m_xBufferView;
};

/// @contract type Zenith_RHIDescriptorHeap
/// Ownership: Owned by the device, created with it, one per device; consumers borrow it through
///   Zenith_RHIDevice::GetDescriptorHeap(). Holds the four index spaces and their slot free lists.
/// Thread affinity: ANY_THREAD_EXCEPT(Free, RetireCompleted). Free and RetireCompleted are RENDER_SUBMIT_THREAD, or
///   MAIN_THREAD at a render-idle point (Zenith2/Docs/Design/RHI.md "Render-idle points"; a frames-in-flight change
///   retires there), because Free's last-use values must be final values the caller knows, which only the thread that
///   submitted them has ([Z2-N-REND-059]); a slot released from any other thread travels with its resource through
///   Zenith_RHIRetirementQueue::RequestReleaseWithDescriptor and is freed by the drain. Write runs on the Flux.BuildGraph
///   node after the frame's upload reservation is final, or on the creating thread at resource creation, never inside a
///   pass lambda or a Flux.Record job ([Z2-N-REND-052]).
/// Invalidation: A slot index is valid from Allocate until its retirement completes after Free; Write to a slot
///   twice between Allocate and Free is ZENITH_ASSERT ([Z2-N-REND-052]).
/// Units: Indices are space-local; capacities are descriptors.
/// Errors: Allocate on a full space returns ZENITH_RHI_ERROR_DESCRIPTOR_SPACE_FULL and never evicts ([Z2-N-REND-060]);
///   Write returns ZENITH_RHI_ERROR_INVALID_DESC or ZENITH_RHI_ERROR_STALE_HANDLE for a stale resource.
/// Version: since Z2 Stage 2. The layout of the heap depends on D-SLANG (open, Stage 1); this interface does not.
/// Locks: uLOCK_LEVEL_RHI_RETIREMENT (450) for the free and pending lists; uLOCK_LEVEL_RHI_DESCRIPTOR_WRITE (460)
///   around Vulkan descriptor-set updates.
/// @endcontract
class Zenith_RHIDescriptorHeapImpl;                             // Internal
class Zenith_RHIDescriptorHeap
{
public:
	Zenith_Result<Zenith_RHIDescriptorHandle, Zenith_RHIError> Allocate(Zenith_RHIDescriptorSpace eSpace);
	Zenith_Result<void, Zenith_RHIError> Write(Zenith_RHIDescriptorSpace eSpace, Zenith_RHIDescriptorHandle xSlot,
		const Zenith_RHIDescriptorWrite& xWrite);
	void Free(Zenith_RHIDescriptorSpace eSpace, Zenith_RHIDescriptorHandle xSlot, const Zenith_RHIQueueValues& xLastUse);
	void RetireCompleted(const Zenith_RHIQueueValues& xCompleted);
	uint32 GetOccupancy(Zenith_RHIDescriptorSpace eSpace) const;
	uint32 GetCapacity(Zenith_RHIDescriptorSpace eSpace) const;
	uint64 GetHeapBytes() const;        // the descriptor memory, for Z2-B-RHI-002
private:
	friend class Zenith_RHIDeviceBase;                          // Internal: constructed by the backend's device
	Zenith_RHIDescriptorHeap() = default;
	Zenith_RHIDescriptorHeapImpl* m_pxImpl = nullptr;
};
```

## Zenith_RHILifetime header

```cpp
// Zenith2/Engine/ZenithRHI/Zenith_RHILifetime.h
#pragma once
#include "ZenithRHI/Zenith_RHITypes.h"
#include "ZenithRHI/Zenith_DeviceHandle.h"
#include "ZenithRHI/Zenith_RHIDescriptors.h"

inline constexpr uint32 uZENITH_RHI_RETIREMENT_CAPACITY = 65536;   // pending entries, the 65,536 of [Z2-N-REND-490]

/// @contract enum Zenith_RHIRetiredKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_RHIRetiredKind : uint8
{
	ZENITH_RHI_RETIRED_KIND_RESOURCE = 1,   // an RHI resource: the queue destroys it on retirement
	ZENITH_RHI_RETIRED_KIND_CLIENT = 2,     // a client object (a table slot, a pool slot, an upload chunk): handed back to the client
};

/// @contract value Zenith_RHIRetiredEntry
/// Ownership: Plain 16-byte value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: entries are written in place into the retirement queue's reused storage.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIRetiredEntry
{
	Zenith_RHIRetiredKind m_eKind = ZENITH_RHI_RETIRED_KIND_RESOURCE;
	uint8 m_uClientKind = 0;              // CLIENT: the client's own object-kind code; RESOURCE: 0
	uint8 m_uDescriptorSpace = 0;         // RESOURCE: 1 + the Zenith_RHIDescriptorSpace of a slot released with it; 0: none
	uint8 m_uReserved = 0;
	uint32 m_uIndex = 0;                  // CLIENT: the client's batch index; RESOURCE with a descriptor space: that slot
	uint64 m_ulPayload = 0;               // RESOURCE: Zenith_RHIResourceHandle bits; CLIENT: the client's payload
};
static_assert(sizeof(Zenith_RHIRetiredEntry) == 16);

/// @contract interface Zenith_RHIRetirementListener
/// Ownership: Implemented above ZenithRHI (by Flux_DeferredDeletion); borrowed by the retirement queue from
///   SetRetirementListener(pxListener) until SetRetirementListener(nullptr), which the owner calls before it is
///   destroyed ([Z2-N-CONTRACT-325]).
/// Thread affinity: RENDER_SUBMIT_THREAD. OnResourceRetired runs on the thread that destroys the resource: the
///   render-submit thread inside RetireCompleted, or the main thread at a render-idle point
///   (Zenith2/Docs/Design/RHI.md "Render-idle points"), never both at once.
/// Invalidation: None.
/// Units: None.
/// Errors: None; the listener cannot fail.
/// Reentrancy: NONE. The listener may not call the retirement queue or the descriptor heap.
/// Version: since Z2 Stage 2 (elaboration: the upward path by which Flux removes a retired resource's imported state).
/// @endcontract
class Zenith_RHIRetirementListener
{
public:
	virtual ~Zenith_RHIRetirementListener() = default;
	// Once for every table resource the device destroys, by whichever path (retirement, or a named immediate
	// destruction of Zenith2/Docs/Design/RHI.md "Rules across the API"), before its table slot is freed.
	virtual void OnResourceRetired(Zenith_RHIResourceHandle xResource) = 0;
};

/// @contract type Zenith_RHIRetirementQueue
/// Ownership: Owned by the device; one per device; borrowed through Zenith_RHIDevice::GetRetirementQueue().
/// Thread affinity: ANY_THREAD_EXCEPT(Enqueue, RetireCompleted, DrainReleaseRequests, SetRetirementListener).
///   Enqueue and RetireCompleted are RENDER_SUBMIT_THREAD, or MAIN_THREAD at a render-idle point
///   (Zenith2/Docs/Design/RHI.md "Render-idle points": swapchain recreation and a frames-in-flight change enqueue and
///   retire there); at a render-idle point the render subgraph is not running and cannot start until the point
///   returns, so the two threads never call them concurrently. DrainReleaseRequests is RENDER_SUBMIT_THREAD alone
///   (Submit step 4). SetRetirementListener is MAIN_THREAD at a render-idle point (renderer creation and destruction).
///   RequestRelease, RequestReleaseWithDescriptor, GetFullWaitCount and GetPendingCount are ANY_THREAD.
/// Invalidation: None.
/// Units: Entries; values are per-queue timeline values.
/// Errors: RequestRelease and RequestReleaseWithDescriptor take RESOURCE entries alone (a CLIENT entry is ZENITH_ASSERT
///   and ignored): one request per live resource at most, so the request capacity (the sum of the device
///   resource-table capacities) is a provable bound; exceeding it is ZENITH_FATAL(FATAL_CODE_QUEUE_FULL), a defect.
///   CLIENT entries arrive through Enqueue as per-frame batches (Memory.md "Retirement queue"). Enqueue with values
///   above the last-signalled values is ZENITH_ASSERT, because its full-ring wait could never complete.
///   DrainReleaseRequests stops uHeadroom entries short of uZENITH_RHI_RETIREMENT_CAPACITY; the headroom is the
///   caller's derived bound on direct Enqueue calls ([Z2-N-CONTRACT-342]; Flux passes uFLUX_RETIREMENT_DRAIN_HEADROOM,
///   Memory.md "Drain headroom"), which makes the full-ring wait unreachable in a correct program.
/// Version: since Z2 Stage 2.
/// Locks: uLOCK_LEVEL_RHI_RETIREMENT (450), short-hold; never held across a WaitCPU or a listener call.
/// @endcontract
class Zenith_RHIRetirementQueueImpl;                            // Internal
class Zenith_RHIRetirementQueue
{
public:
	// An entry with explicit last-use values. When the pending list holds uZENITH_RHI_RETIREMENT_CAPACITY entries, waits
	// (WaitCPU) for the oldest entry's values and retires it first ([Z2-N-REND-490]); each such wait increments
	// RHI.Retirement.FullWaits (Flux re-publishes it as Flux.Lifetime.DeletionQueueFullWaits).
	// ZENITH_ASSERT(xLastUse.IsSatisfiedBy(GetLastSignalledValues())).
	void Enqueue(const Zenith_RHIRetiredEntry& xEntry, const Zenith_RHIQueueValues& xLastUse);
	// A RESOURCE release whose last use is "every submission made so far"; the values are taken when the
	// render-submit thread drains the request after the current frame's submissions (Memory.md "Retirement queue").
	void RequestRelease(const Zenith_RHIRetiredEntry& xEntry);
	// As RequestRelease, for a resource whose descriptor slot is released with it: the drain frees xSlot in eSpace with
	// the same drain values ([Z2-N-REND-059]), so the slot and the resource retire together.
	void RequestReleaseWithDescriptor(const Zenith_RHIRetiredEntry& xEntry, Zenith_RHIDescriptorSpace eSpace,
		Zenith_RHIDescriptorHandle xSlot);
	// Submit step 4: moves requests into the pending list with the last-signalled values of every queue while
	// GetPendingCount() < uZENITH_RHI_RETIREMENT_CAPACITY − uHeadroom, never waiting; the rest stay queued.
	void DrainReleaseRequests(uint32 uHeadroom);
	// Frame begin: retires every satisfied entry (not just the oldest, [Z2-N-REND-489]); a RESOURCE entry calls the
	// listener and is destroyed; CLIENT entries are appended, in enqueue order, to axClientOut.
	void RetireCompleted(const Zenith_RHIQueueValues& xCompleted, Zenith_Array<Zenith_RHIRetiredEntry>& axClientOut);
	void SetRetirementListener(Zenith_RHIRetirementListener* pxListener);
	uint64 GetFullWaitCount() const;    // elaboration: the statistic behind RHI.Retirement.FullWaits
	uint32 GetPendingCount() const;
private:
	friend class Zenith_RHIDeviceBase;
	Zenith_RHIRetirementQueue() = default;
	Zenith_RHIRetirementQueueImpl* m_pxImpl = nullptr;
};
```

## Zenith_RHIPipelines header

```cpp
// Zenith2/Engine/ZenithRHI/Zenith_RHIPipelines.h
#pragma once
#include "ZenithRHI/Zenith_RHITypes.h"
#include "ZenithRHI/Zenith_DeviceHandle.h"

/// @contract enum Zenith_RHIVertexFormat
/// Ownership: Plain enumeration; the closed storage-format vocabulary of Zenith2/Docs/Rendering/VertexData.md
///   "Vertex declaration in Slang" as the fetch unit sees it.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Flux maps each ZenithVertexFormat layout-table format to exactly one enumerator.
/// @endcontract
enum Zenith_RHIVertexFormat : uint8
{
	ZENITH_RHI_VERTEX_FORMAT_SNORM16X2 = 1, ZENITH_RHI_VERTEX_FORMAT_SNORM16X4, ZENITH_RHI_VERTEX_FORMAT_UNORM16X2,
	ZENITH_RHI_VERTEX_FORMAT_UNORM16X4, ZENITH_RHI_VERTEX_FORMAT_FLOAT16X2, ZENITH_RHI_VERTEX_FORMAT_FLOAT16X4,
	ZENITH_RHI_VERTEX_FORMAT_FLOAT32X2, ZENITH_RHI_VERTEX_FORMAT_FLOAT32X3, ZENITH_RHI_VERTEX_FORMAT_FLOAT32X4,
	ZENITH_RHI_VERTEX_FORMAT_UINT8X4, ZENITH_RHI_VERTEX_FORMAT_UINT16X4, ZENITH_RHI_VERTEX_FORMAT_UNORM8X4,
};

/// @contract value Zenith_RHIVertexInputAttribute
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Bytes for the offset.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIVertexInputAttribute { uint32 m_uLocation = 0; uint32 m_uStream = 0; Zenith_RHIVertexFormat m_eFormat = ZENITH_RHI_VERTEX_FORMAT_FLOAT32X3; uint32 m_uOffset = 0; const char* m_szSemantic = nullptr; uint32 m_uSemanticIndex = 0; };

/// @contract value Zenith_RHIVertexInputStream
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Bytes for the stride.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIVertexInputStream { uint32 m_uStream = 0; uint32 m_uStride = 0; bool m_bPerInstance = false; };

enum Zenith_RHICullMode : uint8 { ZENITH_RHI_CULL_MODE_BACK = 1, ZENITH_RHI_CULL_MODE_FRONT, ZENITH_RHI_CULL_MODE_NONE };
enum Zenith_RHIFillMode : uint8 { ZENITH_RHI_FILL_MODE_SOLID = 1, ZENITH_RHI_FILL_MODE_WIREFRAME };
enum Zenith_RHITopology : uint8 { ZENITH_RHI_TOPOLOGY_TRIANGLE_LIST = 1, ZENITH_RHI_TOPOLOGY_LINE_LIST, ZENITH_RHI_TOPOLOGY_POINT_LIST };
enum Zenith_RHICompareOp : uint8 { ZENITH_RHI_COMPARE_OP_NEVER = 1, ZENITH_RHI_COMPARE_OP_LESS, ZENITH_RHI_COMPARE_OP_EQUAL, ZENITH_RHI_COMPARE_OP_LESS_OR_EQUAL,
	ZENITH_RHI_COMPARE_OP_GREATER, ZENITH_RHI_COMPARE_OP_NOT_EQUAL, ZENITH_RHI_COMPARE_OP_GREATER_OR_EQUAL, ZENITH_RHI_COMPARE_OP_ALWAYS };
enum Zenith_RHIBlendFactor : uint8 { ZENITH_RHI_BLEND_FACTOR_ZERO = 1, ZENITH_RHI_BLEND_FACTOR_ONE, ZENITH_RHI_BLEND_FACTOR_SRC_ALPHA,
	ZENITH_RHI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, ZENITH_RHI_BLEND_FACTOR_DST_COLOUR, ZENITH_RHI_BLEND_FACTOR_SRC_COLOUR };
enum Zenith_RHIBlendOp : uint8 { ZENITH_RHI_BLEND_OP_ADD = 1, ZENITH_RHI_BLEND_OP_MIN, ZENITH_RHI_BLEND_OP_MAX };
enum Zenith_RHIStencilOp : uint8 { ZENITH_RHI_STENCIL_OP_KEEP = 1, ZENITH_RHI_STENCIL_OP_ZERO, ZENITH_RHI_STENCIL_OP_REPLACE, ZENITH_RHI_STENCIL_OP_INCREMENT_CLAMP };

/// @contract value Zenith_RHIBlendState
/// Ownership: Plain value; one colour attachment's blend state.
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned as a field of the descriptors and records that hold it.
/// Invalidation: None.
/// Units: None; m_uWriteMask is the RGBA write mask in bits 0 to 3.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIBlendState
{
	bool m_bEnable = false;
	Zenith_RHIBlendFactor m_eSrcColour = ZENITH_RHI_BLEND_FACTOR_ONE;
	Zenith_RHIBlendFactor m_eDstColour = ZENITH_RHI_BLEND_FACTOR_ZERO;
	Zenith_RHIBlendOp m_eColourOp = ZENITH_RHI_BLEND_OP_ADD;
	Zenith_RHIBlendFactor m_eSrcAlpha = ZENITH_RHI_BLEND_FACTOR_ONE;
	Zenith_RHIBlendFactor m_eDstAlpha = ZENITH_RHI_BLEND_FACTOR_ZERO;
	Zenith_RHIBlendOp m_eAlphaOp = ZENITH_RHI_BLEND_OP_ADD;
	uint8 m_uWriteMask = 0xF;
};

/// @contract value Zenith_RHIPipelineCreateResult
/// Ownership: Plain value returned by the pipeline-creation members of Zenith_RHIDevice.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. m_bCacheHit is the per-creation flag from which Flux derives Flux.PSO.CacheHits and
///   Flux.PSO.CacheMisses (pending X71(f)); the RHI itself counts RHI.Pipelines.CacheHits and RHI.Pipelines.CacheMisses.
/// @endcontract
struct Zenith_RHIPipelineCreateResult { Zenith_RHIPipelineHandle m_xPipeline; bool m_bCacheHit = false; };

/// @contract value Zenith_RHIRasterState
/// Ownership: Plain value; the resolved form of Flux_RasterState (Flux expands its fixed-table indices into it,
///   Zenith2/Docs/Rendering/ShaderSystem.md "PSO keys").
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Depth bias in the API's constant units; slope in depth units per unit slope.
/// Errors: None.
/// Version: since Z2 Stage 2. Front faces are counter-clockwise on both APIs ([Z2-N-REND-198]); the struct has no
///   winding member.
/// @endcontract
struct Zenith_RHIRasterState
{
	Zenith_RHICullMode m_eCull = ZENITH_RHI_CULL_MODE_BACK;
	Zenith_RHIFillMode m_eFill = ZENITH_RHI_FILL_MODE_SOLID;
	Zenith_RHITopology m_eTopology = ZENITH_RHI_TOPOLOGY_TRIANGLE_LIST;
	bool m_bDepthTest = true;
	bool m_bDepthWrite = true;
	bool m_bDepthClamp = false;
	Zenith_RHICompareOp m_eDepthCompare = ZENITH_RHI_COMPARE_OP_LESS_OR_EQUAL;
	float m_fDepthBiasConstant = 0.0f;
	float m_fDepthBiasSlope = 0.0f;
	float m_fDepthBiasClamp = 0.0f;
	bool m_bStencilTest = false;
	uint8 m_uStencilReadMask = 0xFF;
	uint8 m_uStencilWriteMask = 0xFF;
	Zenith_RHICompareOp m_eStencilCompare = ZENITH_RHI_COMPARE_OP_ALWAYS;
	Zenith_RHIStencilOp m_eStencilPass = ZENITH_RHI_STENCIL_OP_KEEP;
	Zenith_RHIStencilOp m_eStencilFail = ZENITH_RHI_STENCIL_OP_KEEP;
	Zenith_RHIStencilOp m_eStencilDepthFail = ZENITH_RHI_STENCIL_OP_KEEP;
	Zenith_RHIBlendState m_axBlend[8];
};

/// @contract value Zenith_RHIShaderBlob
/// Ownership: Borrowed bytes: the blob lives in a loaded shader container, which outlives the create call.
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned as a field of the descriptors and records that hold it.
/// Invalidation: None after the create call returns; the device keeps no reference to the bytes; the driver copies what it needs.
/// Units: Bytes.
/// Errors: An empty span or a blob for the other API is ZENITH_RHI_ERROR_SHADER_BLOB_INVALID.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIShaderBlob { Zenith_Span<const uint8> m_xBytes; const char* m_szEntryPoint = "main"; uint16 m_uStage = 0; };   // m_uStage: one Zenith_RHIShaderStage bit

/// @contract value Zenith_RHIGraphicsPipelineDesc
/// Ownership: Plain value borrowing its spans for the duration of the create call.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: None.
/// Errors: More than 8 colour formats, a colour format without the C feature, a depth format without D, or a
///   sample count other than one is ZENITH_RHI_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIGraphicsPipelineDesc
{
	const char* m_szName = nullptr;                                // the PSO key hash in hex, for captures and debug names
	Zenith_RHIShaderBlob m_xVertex;
	Zenith_RHIShaderBlob m_xPixel;                                 // empty: depth-only pipeline
	Zenith_Span<const Zenith_RHIVertexInputStream> m_axStreams;
	Zenith_Span<const Zenith_RHIVertexInputAttribute> m_axAttributes;
	Zenith_RHIRasterState m_xRaster;
	Zenith_RHIFormat m_aeColourFormats[8] = {};
	uint32 m_uColourCount = 0;
	Zenith_RHIFormat m_eDepthFormat = ZENITH_RHI_FORMAT_UNDEFINED;
	uint64 m_ulKeyHash = 0;                                       // the PSO key hash, the D3D12 PSO-library entry name
};

/// @contract value Zenith_RHIComputePipelineDesc
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: None.
/// Errors: As Zenith_RHIShaderBlob.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIComputePipelineDesc { const char* m_szName = nullptr; Zenith_RHIShaderBlob m_xCompute; uint64 m_ulKeyHash = 0; };

/// @contract value Zenith_RHIMeshPipelineDesc
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: None.
/// Errors: ZENITH_RHI_ERROR_FEATURE_UNAVAILABLE when m_bMeshShaders is false.
/// Version: since Z2 Stage 5 (visibility-buffer path).
/// @endcontract
struct Zenith_RHIMeshPipelineDesc { const char* m_szName = nullptr; Zenith_RHIShaderBlob m_xTask; Zenith_RHIShaderBlob m_xMesh; Zenith_RHIShaderBlob m_xPixel;
	Zenith_RHIRasterState m_xRaster; Zenith_RHIFormat m_aeColourFormats[8] = {}; uint32 m_uColourCount = 0; Zenith_RHIFormat m_eDepthFormat = ZENITH_RHI_FORMAT_UNDEFINED; uint64 m_ulKeyHash = 0; };

/// @contract interface Zenith_RHIPipelineCache
/// Ownership: Created by Zenith_RHIDevice::CreatePipelineCache as a Zenith_Owned; owned by Flux_PipelineCacheStore
///   for the device's lifetime; destroyed before the device.
/// Thread affinity: ANY_THREAD. Pipeline creation jobs use it concurrently (VkPipelineCache and
///   ID3D12PipelineLibrary1 are internally synchronised; the D3D12 StorePipeline path takes
///   uLOCK_LEVEL_RHI_PIPELINE_LIBRARY).
/// Invalidation: Serialize output is a snapshot; it is invalid for any other device, driver or key.
/// Units: Bytes.
/// Errors: Serialize returns ZENITH_RHI_ERROR_OUT_OF_HOST_MEMORY when the arena is too small.
/// Reentrancy: NOT_APPLICABLE. The interface invokes no callbacks.
/// Version: since Z2 Stage 3. The payload format is the driver's; the Zenith2 file header around it is
///   Zenith2/Docs/Rendering/ShaderSystem.md "Persistent pipeline caches".
/// Locks: uLOCK_LEVEL_RHI_PIPELINE_LIBRARY (470) on D3D12.
/// @endcontract
class Zenith_RHIPipelineCache
{
public:
	virtual ~Zenith_RHIPipelineCache() = default;
	virtual uint64 GetPayloadSizeEstimate() const = 0;
	virtual Zenith_Result<Zenith_Span<const uint8>, Zenith_RHIError> Serialize(Zenith_Arena& xArena) = 0;
	virtual bool WasLoadRejected() const = 0;   // the driver ignored or refused the initial payload
};
```

## Zenith_RHIQueue header

`Zenith_RHITimeline` is the class of `Zenith2/Docs/Rendering/GpuResources.md → "Queues and timelines"`; this header adds its contract block, the submission types and one accessor.

```cpp
// Zenith2/Engine/ZenithRHI/Zenith_RHIQueue.h
#pragma once
#include "ZenithRHI/Zenith_RHITypes.h"
#include "ZenithRHI/Zenith_RHIError.h"

/// @contract type Zenith_RHITimeline
/// Ownership: Owned by the device, one per physical queue; a queue class the device lacks aliases the graphics
///   timeline (GetTimeline(ZENITH_RHI_QUEUE_COPY) returns the graphics one when m_bCopyQueue is false).
/// Thread affinity: ANY_THREAD_EXCEPT(WaitCPU). WaitCPU is RENDER_SUBMIT_THREAD ([Z2-N-REND-486]), or MAIN_THREAD for
///   the five permitted main-thread waits of Zenith2/Docs/Design/RHI.md "Permitted main-thread GPU waits" (the
///   swapchain drain and the idle wait of a frames-in-flight change, the Android surface release, the first
///   preparation of a recovered device, shutdown, a full-ring retirement Enqueue), each at a render-idle point
///   (elaboration, pending X71(b)).
/// Invalidation: None while the device lives.
/// Units: Values are strictly increasing per queue; timeouts in nanoseconds.
/// Errors: WaitCPU returns ZENITH_RHI_WAIT_STATUS_TIMEOUT as a status and ZENITH_RHI_ERROR_DEVICE_LOST as an error.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_RHITimelineImpl;                                   // Internal
class Zenith_RHITimeline
{
public:
	uint64 GetCompletedValue() const;
	uint64 GetLastSignalledValue() const;
	Zenith_Result<Zenith_RHIWaitStatus, Zenith_RHIError> WaitCPU(uint64 ulValue, uint64 ulTimeoutNanoseconds);
	Zenith_RHIQueue GetPhysicalQueue() const;   // elaboration: the queue this timeline really signals
private:
	friend class Zenith_RHIDeviceBase;
	Zenith_RHITimeline() = default;
	Zenith_RHITimelineImpl* m_pxImpl = nullptr;
};

/// @contract value Zenith_RHITimelineWait
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: A timeline value.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHITimelineWait { Zenith_RHIQueue m_eQueue = ZENITH_RHI_QUEUE_GRAPHICS; uint64 m_ulValue = 0; };

class Zenith_RHICommandList;
class Zenith_RHISwapchain;

/// @contract value Zenith_RHISubmitBatch
/// Ownership: Plain value borrowing its spans for the Submit call.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: None.
/// Errors: A command list recorded for another queue, or a wait on a value its queue has not been asked to signal,
///   is ZENITH_ASSERT.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHISubmitBatch
{
	Zenith_Span<Zenith_RHICommandList* const> m_apxLists;     // recorded in order, submitted in this order
	Zenith_Span<const Zenith_RHITimelineWait> m_axWaits;      // cross-queue edges of step 6 of the graph compile
	Zenith_RHISwapchain* m_pxWaitAcquire = nullptr;           // this batch first writes the acquired swapchain image
	Zenith_RHISwapchain* m_pxSignalPresentReady = nullptr;    // this batch last writes it; the present waits on it
};
```

