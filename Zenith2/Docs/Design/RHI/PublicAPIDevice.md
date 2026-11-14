# RHI design: public API, recording, presentation and device headers

This file is part of the RHI design family. The entry document is `Zenith2/Docs/Design/RHI.md`; it continues `Zenith2/Docs/Design/RHI/PublicAPI.md`, which holds the header list and the value-type headers, with the recording, barrier, presentation, query, debug, fault-injection, configuration and device headers, the backend factory headers and the RHI-owned ZenithFlux public headers. The conventions of `Zenith2/Docs/Design/RHI/PublicAPI.md` apply unchanged.

## Zenith_RHICommandList header

The command list is the render graph's recording surface. It is reachable from `ZenithFlux/Internal/**` alone (ForbiddenCheck rule F20), so neither a pass context nor a feature function can name it ([Z2-N-REND-300]); its barrier operations are private and reachable only through `Zenith_RHIBarrier.h`, which rule F20 restricts further to `ZenithFlux/Internal/Graph/**` ([Z2-N-REND-006], Z2-D-056).

```cpp
// Zenith2/Engine/ZenithRHI/Zenith_RHICommandList.h
#pragma once
#include "ZenithRHI/Zenith_RHIResources.h"
#include "ZenithRHI/Zenith_RHIQueue.h"

enum Zenith_RHILoadOp : uint8 { ZENITH_RHI_LOAD_OP_LOAD = 1, ZENITH_RHI_LOAD_OP_CLEAR, ZENITH_RHI_LOAD_OP_DONT_CARE };
enum Zenith_RHIStoreOp : uint8 { ZENITH_RHI_STORE_OP_STORE = 1, ZENITH_RHI_STORE_OP_DONT_CARE };
enum Zenith_RHIBreadcrumbMarker : uint8 { ZENITH_RHI_BREADCRUMB_BEGIN = 1, ZENITH_RHI_BREADCRUMB_END };

/// @contract value Zenith_RHIColourAttachment
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Mip and layer indices.
/// Errors: None; the graph validates attachments before recording.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIColourAttachment { Zenith_RHITextureHandle m_xTexture; Zenith_RHIFormat m_eViewFormat = ZENITH_RHI_FORMAT_UNDEFINED; uint32 m_uMip = 0; uint32 m_uLayer = 0;
	Zenith_RHILoadOp m_eLoad = ZENITH_RHI_LOAD_OP_LOAD; Zenith_RHIStoreOp m_eStore = ZENITH_RHI_STORE_OP_STORE; Zenith_RHIClearValue m_xClear; };

/// @contract value Zenith_RHIDepthAttachment
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Layer index.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIDepthAttachment { Zenith_RHITextureHandle m_xTexture; uint32 m_uLayer = 0; bool m_bReadOnly = false;
	Zenith_RHILoadOp m_eDepthLoad = ZENITH_RHI_LOAD_OP_LOAD; Zenith_RHIStoreOp m_eDepthStore = ZENITH_RHI_STORE_OP_STORE;
	Zenith_RHILoadOp m_eStencilLoad = ZENITH_RHI_LOAD_OP_LOAD; Zenith_RHIStoreOp m_eStencilStore = ZENITH_RHI_STORE_OP_STORE; Zenith_RHIClearValue m_xClear; };

/// @contract value Zenith_RHIRenderingDesc
/// Ownership: Plain value borrowing its span for the BeginRendering call.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Pixels for the render area.
/// Errors: None.
/// Version: since Z2 Stage 2. Sample count is one; a rendering scope has one layer.
/// @endcontract
struct Zenith_RHIRenderingDesc { Zenith_Span<const Zenith_RHIColourAttachment> m_axColour; const Zenith_RHIDepthAttachment* m_pxDepth = nullptr; Zenith_RHIRect m_xRenderArea; const char* m_szScopeName = nullptr; };

/// @contract value Zenith_RHIVertexBufferBinding
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIVertexBufferBinding { Zenith_RHIBufferHandle m_xBuffer; uint64 m_ulOffset = 0; uint32 m_uStride = 0; };

/// @contract value Zenith_RHIPushConstants
/// Ownership: Plain 8-byte value, bit-identical to Flux_PushConstants ([Z2-N-REND-061]).
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: A BUFFER_SRV index and a byte offset.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIPushConstants { uint32 m_uWord0 = 0; uint32 m_uWord1 = 0; };
static_assert(sizeof(Zenith_RHIPushConstants) == 8);

struct Zenith_RHIBarrierBatch;
struct Zenith_RHISplitBarrierID;

/// @contract interface Zenith_RHICommandList
/// Ownership: Owned by the device's per-frame-slot command pools; borrowed by the render graph for one recording
///   partition through Zenith_RHIDevice::AcquireCommandList and returned by Submit. Never destroyed by a caller.
/// Thread affinity: OWNER_THREAD. One recording job records one list; ownership moves to the render-submit thread
///   at the Record-to-Submit graph edge.
/// Invalidation: Invalid after its frame slot is prepared again (Zenith_RHIDevice::BeginFrameSlot), which happens only
///   after the slot's submissions completed; every handle recorded must stay unretired until then.
/// Units: Vertex, index and instance counts; bytes for offsets; thread groups for dispatches.
/// Errors: Recording cannot fail; a device loss surfaces at Submit. Misuse (a draw outside a rendering scope, a
///   dispatch inside one, a pipeline of the wrong bind point) is ZENITH_ASSERT, and the call is ignored in builds
///   without asserts.
/// Reentrancy: NOT_APPLICABLE. The interface invokes no callbacks.
/// Version: since Z2 Stage 2. Recording surface of ZenithFlux/Internal alone (ForbiddenCheck rule F20).
/// @endcontract
class Zenith_RHICommandList
{
public:
	virtual ~Zenith_RHICommandList() = default;
	virtual Zenith_RHIQueue GetQueue() const = 0;
	virtual void BindGlobalDescriptors() = 0;                       // the partition prologue ([Z2-N-REND-306])
	virtual void BeginRendering(const Zenith_RHIRenderingDesc& xDesc) = 0;
	virtual void EndRendering() = 0;
	virtual void BindPipeline(Zenith_RHIPipelineHandle xPipeline) = 0;   // graphics, compute or mesh by its kind
	virtual void SetPushConstants(const Zenith_RHIPushConstants& xConstants) = 0;
	virtual void SetDrawIndexConstant(uint32 uDrawIndex) = 0;      // D3D12 root parameter 1; no-op on Vulkan ([Z2-N-REND-072])
	virtual void SetViewport(const Zenith_RHIViewport& xViewport) = 0;
	virtual void SetScissor(const Zenith_RHIRect& xRect) = 0;
	virtual void SetStencilReference(uint8 uReference) = 0;
	virtual void BindVertexBuffers(uint32 uFirstStream, Zenith_Span<const Zenith_RHIVertexBufferBinding> axBindings) = 0;
	virtual void BindIndexBuffer16(Zenith_RHIBufferHandle xBuffer, uint64 ulOffset) = 0;   // 16-bit indices alone ([Z2-N-REND-866])
	virtual void Draw(uint32 uVertexCount, uint32 uInstanceCount, uint32 uFirstVertex, uint32 uFirstInstance) = 0;
	virtual void DrawIndexed(uint32 uIndexCount, uint32 uInstanceCount, uint32 uFirstIndex, sint32 iVertexOffset, uint32 uFirstInstance) = 0;
	virtual void DrawIndexedIndirect(Zenith_RHIBufferHandle xArgs, uint64 ulOffset, uint32 uCommandCount) = 0;   // stride 24
	virtual void DrawIndexedIndirectCount(Zenith_RHIBufferHandle xArgs, uint64 ulOffset, Zenith_RHIBufferHandle xCount, uint64 ulCountOffset, uint32 uMaxCommands) = 0;
	virtual void DrawMeshTasksIndirectCount(Zenith_RHIBufferHandle xArgs, uint64 ulOffset, Zenith_RHIBufferHandle xCount, uint64 ulCountOffset, uint32 uMaxCommands) = 0;   // stride 12; Stage 5
	virtual void Dispatch(uint32 uGroupsX, uint32 uGroupsY, uint32 uGroupsZ) = 0;
	virtual void DispatchIndirect(Zenith_RHIBufferHandle xArgs, uint64 ulOffset) = 0;              // stride 12
	virtual void CopyBuffer(Zenith_RHIBufferHandle xSource, uint64 ulSourceOffset, Zenith_RHIBufferHandle xDestination, uint64 ulDestinationOffset, uint64 ulBytes) = 0;
	virtual void CopyBufferToTexture(Zenith_RHIBufferHandle xSource, uint64 ulSourceOffset, uint32 uRowPitch, Zenith_RHITextureHandle xDestination, uint32 uMip, uint32 uLayer, const Zenith_RHIRect& xRegion) = 0;
	virtual void CopyTexture(Zenith_RHITextureHandle xSource, Zenith_RHITextureHandle xDestination, const Zenith_RHISubresourceRange& xRange) = 0;
	virtual void CopyTextureToBuffer(Zenith_RHITextureHandle xSource, uint32 uMip, uint32 uLayer, const Zenith_RHIRect& xRegion, Zenith_RHIBufferHandle xDestination, uint64 ulOffset, uint32 uRowPitch) = 0;
	virtual void FillBuffer(Zenith_RHIBufferHandle xBuffer, uint64 ulOffset, uint64 ulBytes, uint32 uValue) = 0;   // outside rendering scopes
	virtual void ClearColourTexture(Zenith_RHITextureHandle xTexture, const Zenith_RHISubresourceRange& xRange, const Zenith_RHIClearValue& xValue) = 0;
	virtual void ClearDepthStencilTexture(Zenith_RHITextureHandle xTexture, const Zenith_RHISubresourceRange& xRange, const Zenith_RHIClearValue& xValue) = 0;
	virtual void WriteTimestamp(Zenith_RHIQueryPoolHandle xPool, uint32 uIndex) = 0;
	virtual void ResolveTimestamps(Zenith_RHIQueryPoolHandle xPool, uint32 uFirst, uint32 uCount) = 0;      // D3D12 ResolveQueryData; no-op on Vulkan
	virtual void BeginDebugLabel(const char* szName, uint32 uColourRGBA) = 0;          // development builds; no-op in Shipping
	virtual void EndDebugLabel() = 0;
	virtual void WriteBreadcrumb(uint32 uPassIndex, Zenith_RHIBreadcrumbMarker eMarker) = 0;   // Recovery.md "Breadcrumbs"
private:
	friend void Zenith_RHIRecordBarriers(Zenith_RHICommandList& xList, const Zenith_RHIBarrierBatch& xBatch);
	friend Zenith_RHISplitBarrierID Zenith_RHIRecordSplitBarrierBegin(Zenith_RHICommandList& xList, const Zenith_RHIBarrierBatch& xBatch);
	friend void Zenith_RHIRecordSplitBarrierEnd(Zenith_RHICommandList& xList, Zenith_RHISplitBarrierID xID, const Zenith_RHIBarrierBatch& xBatch);
	virtual void DoRecordBarriers(const Zenith_RHIBarrierBatch& xBatch) = 0;
	virtual uint32 DoBeginSplit(const Zenith_RHIBarrierBatch& xBatch) = 0;
	virtual void DoEndSplit(uint32 uSplitIndex, const Zenith_RHIBarrierBatch& xBatch) = 0;
};
```

## Zenith_RHIBarrier header

The neutral barrier vocabulary. Flux's access states (`Zenith2/Docs/Rendering/RenderGraph.md → "Barrier state machine"`, table "Access states") map onto it in `Flux_ToRHIBarrier` (`Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Neutral barrier translation"`), and each backend maps it onto its API in one function (`Zenith2/Docs/Design/RHI/Vulkan.md → "Barrier translation"`, `Zenith2/Docs/Design/RHI/D3D12.md → "Enhanced barrier translation"`).

```cpp
// Zenith2/Engine/ZenithRHI/Zenith_RHIBarrier.h
#pragma once
#include "ZenithRHI/Zenith_RHICommandList.h"

/// @contract enum Zenith_RHIStage
/// Ownership: Plain bit mask enumeration of pipeline stages.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_RHIStage : uint32
{
	ZENITH_RHI_STAGE_NONE                    = 0,
	ZENITH_RHI_STAGE_DRAW_INDIRECT           = 1u << 0,
	ZENITH_RHI_STAGE_INDEX_INPUT             = 1u << 1,
	ZENITH_RHI_STAGE_VERTEX_ATTRIBUTE_INPUT  = 1u << 2,
	ZENITH_RHI_STAGE_VERTEX_SHADER           = 1u << 3,
	ZENITH_RHI_STAGE_TASK_SHADER             = 1u << 4,
	ZENITH_RHI_STAGE_MESH_SHADER             = 1u << 5,
	ZENITH_RHI_STAGE_PIXEL_SHADER            = 1u << 6,
	ZENITH_RHI_STAGE_EARLY_FRAGMENT_TESTS    = 1u << 7,
	ZENITH_RHI_STAGE_LATE_FRAGMENT_TESTS     = 1u << 8,
	ZENITH_RHI_STAGE_COLOUR_ATTACHMENT_OUTPUT = 1u << 9,
	ZENITH_RHI_STAGE_COMPUTE_SHADER          = 1u << 10,
	ZENITH_RHI_STAGE_COPY                    = 1u << 11,
	ZENITH_RHI_STAGE_CLEAR                   = 1u << 12,
	ZENITH_RHI_STAGE_ALL_COMMANDS            = 1u << 13,
};

/// @contract enum Zenith_RHIAccess
/// Ownership: Plain bit mask enumeration of memory accesses.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_RHIAccess : uint32
{
	ZENITH_RHI_ACCESS_NONE                    = 0,
	ZENITH_RHI_ACCESS_INDIRECT_COMMAND_READ   = 1u << 0,
	ZENITH_RHI_ACCESS_INDEX_READ              = 1u << 1,
	ZENITH_RHI_ACCESS_VERTEX_ATTRIBUTE_READ   = 1u << 2,
	ZENITH_RHI_ACCESS_SHADER_SAMPLED_READ     = 1u << 3,
	ZENITH_RHI_ACCESS_SHADER_STORAGE_READ     = 1u << 4,
	ZENITH_RHI_ACCESS_SHADER_STORAGE_WRITE    = 1u << 5,
	ZENITH_RHI_ACCESS_COLOUR_ATTACHMENT_READ  = 1u << 6,
	ZENITH_RHI_ACCESS_COLOUR_ATTACHMENT_WRITE = 1u << 7,
	ZENITH_RHI_ACCESS_DEPTH_STENCIL_READ      = 1u << 8,
	ZENITH_RHI_ACCESS_DEPTH_STENCIL_WRITE     = 1u << 9,
	ZENITH_RHI_ACCESS_TRANSFER_READ           = 1u << 10,
	ZENITH_RHI_ACCESS_TRANSFER_WRITE          = 1u << 11,
};

/// @contract enum Zenith_RHITextureLayout
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_RHITextureLayout : uint8
{
	ZENITH_RHI_TEXTURE_LAYOUT_UNDEFINED = 1,         // first use; with m_bDiscard the contents are discarded
	ZENITH_RHI_TEXTURE_LAYOUT_GENERAL,               // storage (UAV)
	ZENITH_RHI_TEXTURE_LAYOUT_SHADER_READ,
	ZENITH_RHI_TEXTURE_LAYOUT_COLOUR_ATTACHMENT,
	ZENITH_RHI_TEXTURE_LAYOUT_DEPTH_STENCIL_WRITE,
	ZENITH_RHI_TEXTURE_LAYOUT_DEPTH_STENCIL_READ,
	ZENITH_RHI_TEXTURE_LAYOUT_TRANSFER_SRC,
	ZENITH_RHI_TEXTURE_LAYOUT_TRANSFER_DST,
	ZENITH_RHI_TEXTURE_LAYOUT_PRESENT,
	ZENITH_RHI_TEXTURE_LAYOUT_COMMON,                // D3D12 copy-queue exit state; Vulkan maps it to GENERAL
	ZENITH_RHI_TEXTURE_LAYOUT_PERSISTENT_READ,       // the steady state of Flux_PersistentReadOnly ([Z2-N-REND-388])
};

/// @contract value Zenith_RHITextureBarrier
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHITextureBarrier
{
	Zenith_RHITextureHandle m_xTexture;
	Zenith_RHISubresourceRange m_xRange;
	uint32 m_uSrcStages = 0; uint32 m_uSrcAccess = 0; Zenith_RHITextureLayout m_eSrcLayout = ZENITH_RHI_TEXTURE_LAYOUT_UNDEFINED;
	uint32 m_uDstStages = 0; uint32 m_uDstAccess = 0; Zenith_RHITextureLayout m_eDstLayout = ZENITH_RHI_TEXTURE_LAYOUT_UNDEFINED;
	Zenith_RHIQueue m_eSrcQueue = ZENITH_RHI_QUEUE_GRAPHICS;   // equal to m_eDstQueue: no ownership transfer
	Zenith_RHIQueue m_eDstQueue = ZENITH_RHI_QUEUE_GRAPHICS;
	bool m_bDiscard = false;                                    // first use: contents undefined ([Z2-N-REND-438])
	bool m_bReleaseHalf = false;                                // a cross-queue release (recorded on the source queue)
	bool m_bAcquireHalf = false;                                // a cross-queue acquire (recorded on the destination queue)
};

/// @contract value Zenith_RHIBufferBarrier
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIBufferBarrier
{
	Zenith_RHIBufferHandle m_xBuffer;
	uint64 m_ulOffset = 0; uint64 m_ulSize = 0;               // 0: whole buffer
	uint32 m_uSrcStages = 0; uint32 m_uSrcAccess = 0;
	uint32 m_uDstStages = 0; uint32 m_uDstAccess = 0;
};

/// @contract value Zenith_RHIGlobalBarrier
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned as a field of the descriptors and records that hold it.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIGlobalBarrier { uint32 m_uSrcStages = 0; uint32 m_uSrcAccess = 0; uint32 m_uDstStages = 0; uint32 m_uDstAccess = 0; };

/// @contract value Zenith_RHIBarrierBatch
/// Ownership: Plain value borrowing its spans (graph arena) for the recording call.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: None.
/// Errors: An empty batch records nothing.
/// Version: since Z2 Stage 2. One batch is one vkCmdPipelineBarrier2 / one ID3D12GraphicsCommandList7::Barrier call ([Z2-N-REND-389]).
/// @endcontract
struct Zenith_RHIBarrierBatch
{
	Zenith_Span<const Zenith_RHITextureBarrier> m_axTextures;
	Zenith_Span<const Zenith_RHIBufferBarrier> m_axBuffers;     // only barriers carrying a queue transfer on D3D12; the rest are folded into m_xGlobal ([Z2-N-REND-390])
	Zenith_RHIGlobalBarrier m_xGlobal;                          // all zero: none
};

/// @contract value Zenith_RHISplitBarrierID
/// Ownership: Plain value naming one open split barrier of one command list's frame slot.
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned into descriptors, records and tables.
/// Invalidation: Valid until its End call; both halves are recorded into the same command list ([Z2-N-REND-394]
///   plus the same-list rule of RenderGraphBackend.md "Step 7 command-list partitions").
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHISplitBarrierID { uint32 m_uIndex = 0xFFFFFFFFu; };

/// @contract functions Zenith_RHIBarrierRecording
/// Ownership: None; record into the list.
/// Thread affinity: OWNER_THREAD (the list's recording thread).
/// Invalidation: None.
/// Units: None.
/// Errors: Recording outside a rendering scope alone; a barrier inside a scope is ZENITH_ASSERT.
/// Version: since Z2 Stage 2. Callers: ZenithFlux/Internal/Graph alone (ForbiddenCheck rule F20).
/// @endcontract
inline void Zenith_RHIRecordBarriers(Zenith_RHICommandList& xList, const Zenith_RHIBarrierBatch& xBatch) { xList.DoRecordBarriers(xBatch); }
inline Zenith_RHISplitBarrierID Zenith_RHIRecordSplitBarrierBegin(Zenith_RHICommandList& xList, const Zenith_RHIBarrierBatch& xBatch) { return { xList.DoBeginSplit(xBatch) }; }
inline void Zenith_RHIRecordSplitBarrierEnd(Zenith_RHICommandList& xList, Zenith_RHISplitBarrierID xID, const Zenith_RHIBarrierBatch& xBatch) { xList.DoEndSplit(xID.m_uIndex, xBatch); }
```

## Zenith_RHISwapchain header

```cpp
// Zenith2/Engine/ZenithRHI/Zenith_RHISwapchain.h
#pragma once
#include "ZenithRHI/Zenith_RHIResources.h"
#include "ZenithRHI/Zenith_RHIError.h"
class Zenith_Window;                                            // ZenithPlatform/Zenith_Window.h

// Zenith_RHIPresentMode and Zenith_RHIOutputColourSpace are declared in Zenith_RHITypes.h (Zenith2/Docs/Design/RHI/PublicAPI.md).
enum Zenith_RHISurfaceTransform : uint8 { ZENITH_RHI_SURFACE_TRANSFORM_IDENTITY = 1, ZENITH_RHI_SURFACE_TRANSFORM_ROTATE_90, ZENITH_RHI_SURFACE_TRANSFORM_ROTATE_180, ZENITH_RHI_SURFACE_TRANSFORM_ROTATE_270 };

/// @contract value Zenith_RHISwapchainDesc
/// Ownership: Plain value borrowing the window pointer for the create or recreate call.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Pixels.
/// Errors: A null window is ZENITH_RHI_ERROR_INVALID_DESC; a window whose surface is unavailable is
///   ZENITH_RHI_ERROR_SURFACE_UNAVAILABLE.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHISwapchainDesc
{
	Zenith_Window* m_pxWindow = nullptr;
	uint32 m_uWidth = 0;                            // 0: the window's drawable size
	uint32 m_uHeight = 0;
	Zenith_RHIPresentMode m_ePresentMode = ZENITH_RHI_PRESENT_MODE_FIFO;
	Zenith_RHIOutputColourSpace m_eColourSpace = ZENITH_RHI_OUTPUT_SDR_SRGB;
	// The image count is the device's GetFramesInFlight() + 1, clamped to the surface's range; the swapchain has no
	// frames-in-flight field of its own, so the device's value is the one value (elaboration).
	const char* m_szName = "Main";
};

/// @contract value Zenith_RHISwapchainInfo
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: creation and every recreation rewrite it field by field.
/// Invalidation: Superseded by each recreation.
/// Units: Pixels.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHISwapchainInfo
{
	uint32 m_uWidth = 0; uint32 m_uHeight = 0; uint32 m_uImageCount = 0;
	Zenith_RHIFormat m_eFormat = ZENITH_RHI_FORMAT_UNDEFINED;
	Zenith_RHIOutputColourSpace m_eColourSpace = ZENITH_RHI_OUTPUT_SDR_SRGB;
	Zenith_RHIPresentMode m_ePresentMode = ZENITH_RHI_PRESENT_MODE_FIFO;       // the mode actually in use after fallback
	Zenith_RHISurfaceTransform m_eTransform = ZENITH_RHI_SURFACE_TRANSFORM_IDENTITY;
	bool m_bPresentWait = false; bool m_bPresentTiming = false; bool m_bLatencyWaitable = false;
	uint32 m_uGeneration = 0;                                                  // incremented by every recreation
	// elaboration (LIGHT's L21, RPROD's RP8): re-read at every creation and recreation (Recovery.md "Present modes and formats")
	uint8 m_uOutputColourSpacesSupported = 1;   // bit (1 << (eSpace − 1)) per Zenith_RHIOutputColourSpace the surface lists; SDR always
	bool m_bDisplayHdr = false;                  // D3D12: the window's output reports HDR10 (G2084/P2020); Vulkan: the surface lists HDR10_ST2084_EXT
	float m_fDisplayMaxLuminanceNits = 0.0f;     // D3D12: DXGI_OUTPUT_DESC1::MaxLuminance; 0 when unknown (Vulkan)
	uint32 m_uSwapInterval = 1;                 // the value of the last SetSwapInterval; survives Recreate
};

/// @contract value Zenith_RHIHDRMetadata
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: CIE 1931 xy chromaticities; nits for the luminances, MaxCLL and MaxFALL (SMPTE ST 2086 and CTA-861.3 static metadata).
/// Errors: None; a non-finite or negative member makes SetHDRMetadata return ZENITH_RHI_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 5 (HDR output, LIGHT). Member for member the layout of LIGHT's Flux_HDRMetadata
///   (Zenith2/Docs/Design/LIGHT/ColourManagement.md "HDR calibration and metadata"), which Flux copies into it.
/// @endcontract
struct Zenith_RHIHDRMetadata
{
	float m_afPrimariesXY[6] = {};     // red x, y, green x, y, blue x, y
	float m_afWhiteXY[2] = {};
	float m_fMaxMasteringNits = 0.0f;
	float m_fMinMasteringNits = 0.0f;
	float m_fMaxCLL = 0.0f;
	float m_fMaxFALL = 0.0f;
};

/// @contract value Zenith_RHISwapchainImage
/// Ownership: Plain value; the texture is owned by the swapchain and imported into the graph each frame.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: Valid until the image is presented, and never across a recreation.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHISwapchainImage { Zenith_RHITextureHandle m_xTexture; uint32 m_uImageIndex = 0; uint64 m_ulPresentID = 0; };

/// @contract value Zenith_RHIPresentTiming
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None.
/// Units: Nanoseconds of the monotonic clock (Zenith_GetMonotonicTimeNs), mapped from the API's clock.
/// Errors: None; m_bValid is false where the platform reports no timing.
/// Version: since Z2 Stage 3 (frame pacing, RPROD).
/// @endcontract
struct Zenith_RHIPresentTiming { uint64 m_ulPresentID = 0; uint64 m_ulActualPresentNs = 0; uint64 m_ulDesiredPresentNs = 0; uint64 m_ulEarliestPresentNs = 0; uint64 m_ulMarginNs = 0; uint64 m_ulRefreshNs = 0; bool m_bValid = false; };

/// @contract interface Zenith_RHISwapchain
/// Ownership: Created by Zenith_RHIDevice::CreateSwapchain as a Zenith_Owned; owned by Flux_SwapchainManager, one per
///   rendering window; destroyed (after a wait on the frames that used its images) before its window and before the
///   device.
/// Thread affinity: RENDER_SUBMIT_THREAD for AcquireNextImage, Present, WaitForPresent, WaitForLatencySlot,
///   GetPresentTimings, SetMaximumFrameLatency, SetDesiredPresentTime and SetSwapInterval (Flux.Present, before the
///   frame's Present); MAIN_THREAD for Recreate, ReleaseSurface and SetHDRMetadata, which run at a render-idle point
///   (Zenith2/Docs/Design/RHI.md "Render-idle points"; Recovery.md "Swapchain recreation"). Recreate enqueues the old
///   images on the retirement queue from there; ReleaseSurface destroys the images, the swapchain and the surface at
///   once, after the wait that completed every frame that used them (a named immediate destruction of
///   Zenith2/Docs/Design/RHI.md "Rules across the API").
/// Invalidation: Images and their texture handles are invalid after Recreate or ReleaseSurface.
/// Units: Timeouts in nanoseconds.
/// Errors: ZENITH_RHI_ERROR_SWAPCHAIN_OUT_OF_DATE (recreate and retry next frame), ZENITH_RHI_ERROR_SURFACE_UNAVAILABLE,
///   ZENITH_RHI_ERROR_DEVICE_LOST, ZENITH_RHI_ERROR_TIMEOUT (an acquire that exceeded rhi.gpuWaitTimeoutMs).
///   VK_SUBOPTIMAL_KHR presents successfully and sets IsRecreateRecommended. SetDesiredPresentTime returns
///   ZENITH_RHI_ERROR_FEATURE_UNAVAILABLE without VK_GOOGLE_display_timing and on D3D12; SetSwapInterval returns
///   ZENITH_RHI_ERROR_INVALID_DESC outside 1 to 4; SetHDRMetadata returns FEATURE_UNAVAILABLE without
///   VK_EXT_hdr_metadata (Zenith_RHICapabilities::m_bHdrMetadata false) (Recovery.md "Frame pacing hooks",
///   "Present modes and formats").
/// Reentrancy: NOT_APPLICABLE. The interface invokes no callbacks.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_RHISwapchain
{
public:
	virtual ~Zenith_RHISwapchain() = default;
	virtual const Zenith_RHISwapchainInfo& GetInfo() const = 0;
	virtual Zenith_Result<Zenith_RHISwapchainImage, Zenith_RHIError> AcquireNextImage(uint32 uFrameSlot) = 0;
	virtual Zenith_Result<void, Zenith_RHIError> Present(const Zenith_RHISwapchainImage& xImage) = 0;
	virtual Zenith_Result<void, Zenith_RHIError> Recreate(const Zenith_RHISwapchainDesc& xDesc) = 0;
	virtual void ReleaseSurface() = 0;                                                 // Android surface loss: destroy swapchain and surface, keep the device
	virtual bool IsRecreateRecommended() const = 0;                        // suboptimal, or a pending size change
	virtual Zenith_Result<Zenith_RHIWaitStatus, Zenith_RHIError> WaitForPresent(uint64 ulPresentID, uint64 ulTimeoutNs) = 0;   // FEATURE_UNAVAILABLE without present wait
	virtual Zenith_Result<Zenith_RHIWaitStatus, Zenith_RHIError> WaitForLatencySlot(uint64 ulTimeoutNs) = 0;                  // DXGI waitable object; Vulkan: WaitForPresent(current − frames in flight)
	virtual uint32 GetPresentTimings(Zenith_Span<Zenith_RHIPresentTiming> axOut) = 0;                                          // newest first; 0 when unsupported
	virtual Zenith_Result<void, Zenith_RHIError> SetMaximumFrameLatency(uint32 uFrames) = 0;
	virtual Zenith_Result<void, Zenith_RHIError> SetDesiredPresentTime(uint64 ulPresentID, uint64 ulDesiredNs) = 0;   // RP8: VkPresentTimeGOOGLE on the present carrying ulPresentID; monotonic ns
	virtual Zenith_Result<void, Zenith_RHIError> SetSwapInterval(uint32 uInterval) = 0;                              // RP8: D3D12 FIFO sync interval 1 to 4; Vulkan records it
	virtual Zenith_Result<void, Zenith_RHIError> SetHDRMetadata(const Zenith_RHIHDRMetadata& xMetadata) = 0;         // L21: vkSetHdrMetadataEXT / IDXGISwapChain4::SetHDRMetaData
};
```

## Zenith_RHIQueries header

```cpp
// Zenith2/Engine/ZenithRHI/Zenith_RHIQueries.h
#pragma once
#include "ZenithRHI/Zenith_RHITypes.h"
#include "ZenithRHI/Zenith_DeviceHandle.h"

/// @contract value Zenith_RHIQueryPoolDesc
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Queries.
/// Errors: A count of 0 or above 8,192 ([Z2-N-REND-311]) is ZENITH_RHI_ERROR_INVALID_DESC; a queue whose timestamp
///   valid bits are 0 is ZENITH_RHI_ERROR_FEATURE_UNAVAILABLE.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIQueryPoolDesc { const char* m_szName = nullptr; Zenith_RHIQueue m_eQueue = ZENITH_RHI_QUEUE_GRAPHICS; uint32 m_uCount = 0; };

/// @contract value Zenith_RHIClockCalibration
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: Drifts; Flux re-reads it every 60 frames.
/// Units: A GPU tick and the monotonic CPU time in nanoseconds sampled together, with the sampling deviation.
/// Errors: m_bValid is false without VK_EXT_calibrated_timestamps (Vulkan); D3D12 always has GetClockCalibration.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIClockCalibration { uint64 m_ulGpuTicks = 0; uint64 m_ulCpuNs = 0; uint64 m_ulMaxDeviationNs = 0; double m_dTickPeriodNs = 0.0; bool m_bValid = false; };
```

## Zenith_RHIDebug header

```cpp
// Zenith2/Engine/ZenithRHI/Zenith_RHIDebug.h
#pragma once
#include "ZenithRHI/Zenith_RHITypes.h"

/// @contract enum Zenith_RHIValidationMode
/// Ownership: Plain enumeration; the value of rhi.validation.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a mode whose layer is unavailable degrades to OFF with a warning (Failure handling, entry document).
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_RHIValidationMode : uint8
{
	ZENITH_RHI_VALIDATION_OFF = 1,          // Shipping: the only mode
	ZENITH_RHI_VALIDATION_STANDARD,         // Khronos validation / D3D12 debug layer
	ZENITH_RHI_VALIDATION_SYNC,             // + Vulkan synchronization validation (tests); D3D12: + synchronized queue validation
	ZENITH_RHI_VALIDATION_GPU,              // + Vulkan GPU-assisted validation / D3D12 GPU-based validation (tests on D3D12)
};

enum Zenith_RHIValidationSeverity : uint8 { ZENITH_RHI_VALIDATION_SEVERITY_INFO = 1, ZENITH_RHI_VALIDATION_SEVERITY_WARNING, ZENITH_RHI_VALIDATION_SEVERITY_ERROR };

/// @contract value Zenith_RHIValidationMessage
/// Ownership: Plain value borrowing its strings for the duration of the sink call.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: The strings are invalid after OnValidationMessage returns.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIValidationMessage
{
	Zenith_RHIBackend m_eBackend = ZENITH_RHI_BACKEND_VULKAN;
	Zenith_RHIValidationSeverity m_eSeverity = ZENITH_RHI_VALIDATION_SEVERITY_ERROR;
	uint32 m_uMessageID = 0;                    // VkDebugUtilsMessengerCallbackDataEXT::messageIdNumber, or D3D12_MESSAGE_ID
	const char* m_szMessageIDName = nullptr;     // pMessageIdName (Vulkan) or the D3D12 message ID name
	const char* m_szText = nullptr;
	bool m_bSuppressed = false;                  // matched an entry of ValidationSuppressions.json
};

/// @contract interface Zenith_RHIValidationSink
/// Ownership: Implemented above ZenithRHI (the runtime's logging sink, the test harness's failing sink); passed by
///   pointer in Zenith_RHIDeviceDesc and outlives the device.
/// Thread affinity: ANY_THREAD. Validation layers and GPU-assisted validation call back on driver threads.
/// Invalidation: None.
/// Units: None.
/// Errors: None; the sink never fails.
/// Reentrancy: NONE. The sink calls no RHI function.
/// Version: since Z2 Stage 2. The upward-notification interface of [Z2-N-ARCH-078] for validation.
/// @endcontract
class Zenith_RHIValidationSink
{
public:
	virtual ~Zenith_RHIValidationSink() = default;
	virtual void OnValidationMessage(const Zenith_RHIValidationMessage& xMessage) = 0;
};

enum Zenith_RHICaptureToolKind : uint8 { ZENITH_RHI_CAPTURE_TOOL_NONE = 1, ZENITH_RHI_CAPTURE_TOOL_RENDERDOC, ZENITH_RHI_CAPTURE_TOOL_PIX };

/// @contract interface Zenith_RHICaptureTool
/// Ownership: Owned by the device when rhi.capture names an available tool; borrowed through
///   Zenith_RHIDevice::GetCaptureTool(), which returns nullptr otherwise.
/// Thread affinity: RENDER_SUBMIT_THREAD. Captures bracket whole frames between Flux.Present calls.
/// Invalidation: None while the device lives.
/// Units: Frames.
/// Errors: EndCapture returns ZENITH_RHI_ERROR_FEATURE_UNAVAILABLE when the tool detached.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_RHICaptureTool
{
public:
	virtual ~Zenith_RHICaptureTool() = default;
	virtual Zenith_RHICaptureToolKind GetKind() const = 0;
	virtual void BeginFrameCapture(const char* szTitle) = 0;
	virtual Zenith_Result<void, Zenith_RHIError> EndFrameCapture() = 0;
	virtual bool IsCapturing() const = 0;
};

/// @contract value Zenith_RHIBreadcrumbReport
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None.
/// Units: Pass indices in the queue's submission order of the frame; 0xFFFFFFFF when none.
/// Errors: m_bValid is false when the breadcrumb buffer could not be read (device memory lost with the device).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_RHIBreadcrumbReport { Zenith_RHIQueue m_eQueue = ZENITH_RHI_QUEUE_GRAPHICS; uint64 m_ulFrameIndex = 0; uint32 m_uLastCompletedPass = 0xFFFFFFFFu; uint32 m_uFirstIncompletePass = 0xFFFFFFFFu; bool m_bValid = false; };

/// @contract value Zenith_RHIDeviceFaultReport
/// Ownership: Plain fixed-size value filled by Zenith_RHIDevice::CollectDeviceFaultReport; allocates nothing.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None.
/// Units: A GPU virtual address; vendor codes as reported.
/// Errors: m_bFaultInfoAvailable is false without VK_EXT_device_fault / DRED.
/// Version: since Z2 Stage 3; DRED and device-fault fields since Z2 Stage 6.
/// @endcontract
struct Zenith_RHIDeviceFaultReport
{
	Zenith_RHIBreadcrumbReport m_axQueues[ZENITH_RHI_QUEUE_COUNT];
	char m_acReason[160] = {};                   // VK result name or GetDeviceRemovedReason HRESULT text
	char m_acFaultDescription[256] = {};         // VkDeviceFaultInfoEXT::description or the DRED page-fault summary
	uint64 m_ulFaultAddress = 0;
	uint64 m_ulVendorFaultCode = 0;
	uint64 m_ulDriverVersion = 0;
	bool m_bFaultInfoAvailable = false;
};
```

## Zenith_RHIFaultInjection header

```cpp
// Zenith2/Engine/ZenithRHI/Zenith_RHIFaultInjection.h
#pragma once
#include "ZenithRHI/Zenith_RHITypes.h"

/// @contract enum Zenith_RHIFaultSite
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_RHIFaultSite : uint8
{
	ZENITH_RHI_FAULT_SITE_CREATE_STEP = 1,      // uDetail: the device-creation step index (entry document "Device creation sequence")
	ZENITH_RHI_FAULT_SITE_ALLOCATE,             // a device-memory allocation fails with OUT_OF_DEVICE_MEMORY
	ZENITH_RHI_FAULT_SITE_SUBMIT,               // the submission reports device loss
	ZENITH_RHI_FAULT_SITE_PRESENT,              // present reports SWAPCHAIN_OUT_OF_DATE
	ZENITH_RHI_FAULT_SITE_WAIT,                 // a timeline wait times out
	ZENITH_RHI_FAULT_SITE_PIPELINE_CREATE,      // pipeline creation fails
	ZENITH_RHI_FAULT_SITE_SWAPCHAIN_CREATE,     // swapchain (re)creation fails
	ZENITH_RHI_FAULT_SITE_DEDICATED_QUEUE,      // uDetail: a Zenith_RHIQueue; true makes the device treat that class as absent (aliased to graphics)
};

/// @contract interface Zenith_RHIFaultInjector
/// Ownership: Implemented by test code (Zenith_RHITestHooks in ZenithRHI/Tests/Support/); passed by pointer in
///   Zenith_RHIDeviceDesc; nullptr in every Player, so shipping code has no test switch ([Z2-N-CONTRACT-415]).
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Reentrancy: NONE. The injector calls no RHI function.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_RHIFaultInjector
{
public:
	virtual ~Zenith_RHIFaultInjector() = default;
	virtual bool ShouldFail(Zenith_RHIFaultSite eSite, uint32 uDetail) = 0;
};
```

## Zenith_RHIConfig header

The configuration keys are registered in the configuration schema of `Zenith2/Docs/Design/CORE/Diagnostics.md → "Configuration store"` from `Zenith_GetRHIConfigKeys()`, read by the composition root at startup step 12 through `Zenith_ReadRHIConfig`, and copied into `Zenith_RHIDeviceDesc`.

| Key | Type, range | Default | Read at | Meaning |
|---|---|---|---|---|
| `rhi.backend` | enum `vulkan`, `d3d12` | `vulkan` | step 12 | the backend on Windows ([Z2-N-REND-026]); Android accepts `vulkan` alone; any other value, `d3d12` included, is refused by the store as a logged `CONFIG_ERROR_INVALID_ENUM_VALUE` warning and the platform default `vulkan` stays in effect ([Z2-N-ARCH-929]) |
| `rhi.adapter` | string, 0–127 bytes | empty | step 12 | empty: the preference below; `software`: WARP or lavapipe; otherwise a case-insensitive substring of the adapter name |
| `rhi.validation` | enum `off`, `standard`, `sync`, `gpu` | `standard` in `win-debug`, `off` elsewhere; Shipping ignores the key | step 12 | `Zenith_RHIValidationMode` |
| `rhi.validation.breakOnError` | bool | false | step 12 | a debugger break on every validation error (development builds) |
| `rhi.capture` | enum `none`, `renderdoc`, `pix` | `none`; Shipping ignores the key | step 12 (before the device) | the capture tool to load |
| `rhi.debugNames` | bool | true in development builds, false in Shipping | step 12 | object names and debug labels |
| `rhi.breadcrumbs` | bool | true | step 12 | per-queue GPU breadcrumbs in every build |
| `rhi.gpuWaitTimeoutMs` | u32, 100..60,000 | 2000 | step 12 | the CPU timeline-wait timeout after which device status is checked ([Z2-N-REND-486]) |
| `rhi.deviceLossPolicy` | enum `recover`, `exit` | `recover` | step 12 | [Z2-N-REND-987] |
| `rhi.presentMode` | enum `fifo`, `fifoRelaxed`, `mailbox`, `immediate` | `fifo` | step 12, and at runtime through a swapchain recreation | `Zenith_RHIPresentMode` |
| `rhi.output` | enum `sdr`, `hdr10`, `scrgb` | `sdr` | step 12, and at runtime through a recreation | `Zenith_RHIOutputColourSpace`; an unsupported value falls back to `sdr` with a warning |
| `r.framesInFlight` | u32, 2..3 | 2 | step 12, and at runtime through a recreation ([Z2-N-REND-492]) | `Z2-B-RHI-008` |
| `rhi.heap.textureSrv`, `rhi.heap.textureUav`, `rhi.heap.bufferSrv`, `rhi.heap.bufferUav` | u32, 1,024..1,048,576 | the tier defaults of `Zenith_GetDefaultHeapCapacities` until `Z2-B-RHI-009` is adopted, then the row's values from the generated budget table | step 12 | `Zenith_RHIHeapCapacities` |

```cpp
// Zenith2/Engine/ZenithRHI/Zenith_RHIConfig.h
/// @contract value Zenith_RHIConfigValues
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the configuration reader default-constructs it and fills its fields.
/// Invalidation: None.
/// Units: As the key table.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIConfigValues
{
	Zenith_RHIBackend m_eBackend = ZENITH_RHI_BACKEND_VULKAN;
	char m_acAdapter[128] = {};
	Zenith_RHIValidationMode m_eValidation = ZENITH_RHI_VALIDATION_OFF;
	bool m_bValidationBreak = false;
	Zenith_RHICaptureToolKind m_eCapture = ZENITH_RHI_CAPTURE_TOOL_NONE;
	bool m_bDebugNames = false;
	bool m_bBreadcrumbs = true;
	uint32 m_uGpuWaitTimeoutMs = 2000;
	bool m_bRecoverOnDeviceLoss = true;
	Zenith_RHIPresentMode m_ePresentMode = ZENITH_RHI_PRESENT_MODE_FIFO;
	Zenith_RHIOutputColourSpace m_eOutput = ZENITH_RHI_OUTPUT_SDR_SRGB;
	uint32 m_uFramesInFlight = 2;
	Zenith_RHIHeapCapacities m_xHeapCapacities;
};

/// @contract functions Zenith_RHIConfig
/// Ownership: The key table is static constant data.
/// Thread affinity: MAIN_THREAD (startup step 12 and the frame prologue's user-layer step).
/// Invalidation: None.
/// Units: As the key table.
/// Errors: A value outside its range was already refused by the configuration store; Zenith_ReadRHIConfig cannot fail.
/// Version: since Z2 Stage 2.
/// @endcontract
Zenith_Span<const Zenith_ConfigKeyDesc> Zenith_GetRHIConfigKeys();
Zenith_RHIConfigValues Zenith_ReadRHIConfig(const Zenith_ConfigStore& xStore, Zenith_RHICapabilityTier eTier);
```

## Zenith_RHIDevice header

```cpp
// Zenith2/Engine/ZenithRHI/Zenith_RHIDevice.h
#pragma once
#include "ZenithRHI/Zenith_RHICapabilities.h"
#include "ZenithRHI/Zenith_RHIDescriptors.h"
#include "ZenithRHI/Zenith_RHILifetime.h"
#include "ZenithRHI/Zenith_RHIPipelines.h"
#include "ZenithRHI/Zenith_RHIQueue.h"
#include "ZenithRHI/Zenith_RHISwapchain.h"
#include "ZenithRHI/Zenith_RHIQueries.h"
#include "ZenithRHI/Zenith_RHIDebug.h"
#include "ZenithRHI/Zenith_RHIFaultInjection.h"
class Zenith_Platform;
class Zenith_Heap;
class Zenith_RHICommandList;

/// @contract value Zenith_RHIDeviceReport
/// Ownership: A fixed-size value the caller owns (on Zenith_RuntimeMain's stack, beside Zenith_StartupReport);
///   the factory fills it on success and on failure; allocates nothing.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: None; truncated text ends with "…".
/// Version: since Z2 Stage 2. The text format is Zenith2/Docs/Design/RHI/Vulkan.md "Refusal message" and
///   Zenith2/Docs/Design/RHI/D3D12.md "Baseline checks".
/// @endcontract
struct Zenith_RHIDeviceReport
{
	uint32 m_uAdaptersConsidered = 0;
	uint8 m_uFailedStep = 0;                    // 0: none; otherwise the device-creation step index
	char m_acSelectedAdapter[128] = {};
	char m_acRefusal[1024] = {};                 // the exact refusal message of every refused adapter, one per line
};

/// @contract value Zenith_RHIDeviceDesc
/// Ownership: Plain value borrowing every pointer for the device's lifetime (platform, heap, sink, injector) or for
///   the factory call (report).
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: As Zenith_RHIConfigValues.
/// Errors: A null platform or heap, frames in flight outside 2..3, heap capacities below 1,024 or above 1,048,576,
///   a GPU wait timeout outside 100..60,000 ms, or a capture tool other than NONE in a Shipping build are
///   ZENITH_RHI_ERROR_INVALID_DESC, returned before any API call (factory step 0).
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RHIDeviceDesc
{
	Zenith_Platform* m_pxPlatform = nullptr;             // Vulkan instance bootstrap, the native window handle
	Zenith_Heap* m_pxHeap = nullptr;                     // the general heap for every RHI host allocation
	Zenith_RHIAdapterPreference m_eAdapterPreference = ZENITH_RHI_ADAPTER_PREFERENCE_HIGH_PERFORMANCE;
	const char* m_szAdapterFilter = nullptr;             // rhi.adapter substring, or nullptr
	Zenith_RHICapabilityTier m_eTier = ZENITH_RHI_CAPABILITY_TIER_DESKTOP;   // from the platform: MOBILE on Android
	Zenith_RHIHeapCapacities m_xHeapCapacities;
	uint32 m_uFramesInFlight = 2;
	Zenith_RHIValidationMode m_eValidation = ZENITH_RHI_VALIDATION_OFF;
	bool m_bValidationBreak = false;
	Zenith_RHICaptureToolKind m_eCaptureTool = ZENITH_RHI_CAPTURE_TOOL_NONE;
	bool m_bDebugNames = false;
	bool m_bBreadcrumbs = true;
	uint32 m_uGpuWaitTimeoutMs = 2000;
	Zenith_RHIValidationSink* m_pxValidationSink = nullptr;
	Zenith_RHIFaultInjector* m_pxFaultInjector = nullptr; // tests only
	Zenith_RHIDeviceReport* m_pxReportOut = nullptr;     // optional
};

enum Zenith_RHIDeviceStatus : uint8 { ZENITH_RHI_DEVICE_STATUS_OK = 1, ZENITH_RHI_DEVICE_STATUS_LOST };

/// @contract value Zenith_RHIHeapBudget
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 4 (residency).
/// @endcontract
struct Zenith_RHIHeapBudget { uint64 m_ulBudgetBytes = 0; uint64 m_ulUsageBytes = 0; bool m_bDeviceLocal = false; };

/// @contract interface Zenith_RHIDevice
/// Ownership: Created by Zenith_CreateRHI_Vulkan or Zenith_CreateRHI_D3D12 as a Zenith_Owned<Zenith_RHIDevice>; the
///   composition root owns the one instance of the process (startup step 12) and destroys it at shutdown step 9,
///   after every queue went idle and every deferred deletion retired (steps 5 and 6); destruction reports every
///   live handle of every kind ([Z2-N-CONTRACT-089]). Device-loss recovery destroys and recreates it.
/// Thread affinity: ANY_THREAD_EXCEPT(Submit, BeginFrameSlot, WaitIdle, CreateSwapchain, SetFramesInFlight). Submit is
///   RENDER_SUBMIT_THREAD; BeginFrameSlot is RENDER_SUBMIT_THREAD, or MAIN_THREAD at a render-idle point (the
///   preparation of frame 0 at renderer creation and after a frames-in-flight change); WaitIdle, CreateSwapchain and
///   SetFramesInFlight are MAIN_THREAD at a render-idle point (Zenith2/Docs/Design/RHI.md "Render-idle points"), where
///   the render subgraph is not running and cannot start, so no render-submit call overlaps them.
///   Resource, pipeline and view creation are ANY_THREAD (PSO and streaming jobs create).
/// Invalidation: Every handle, command list, swapchain and pipeline cache is invalid after destruction. After
///   device loss every call returns ZENITH_RHI_ERROR_DEVICE_LOST (queries return empty values) until destruction.
/// Units: Bytes, texels and nanoseconds as named.
/// Errors: Zenith_Result<…, Zenith_RHIError> per member; the enumerator table is Zenith2/Docs/Design/RHI.md
///   "Error codes". Resolving a stale handle is ZENITH_RHI_ERROR_STALE_HANDLE in every build.
/// Reentrancy: NONE. The validation sink and fault injector it calls may not call back into the device.
/// Version: since Z2 Stage 2. Source API; replaceable subsystem with two implementations ([Z2-N-CONTRACT-035]).
/// Locks: uLOCK_LEVEL_RHI_QUEUE_SUBMIT (400), uLOCK_LEVEL_RHI_RESOURCE_TABLE (410), uLOCK_LEVEL_RHI_RETIREMENT (450),
///   uLOCK_LEVEL_RHI_DESCRIPTOR_WRITE (460), uLOCK_LEVEL_RHI_PIPELINE_LIBRARY (470).
/// Example: Zenith2/Engine/ZenithRHI/Tests/Examples/Zenith_RHIDevice_Example.cpp
/// @endcontract
class Zenith_RHIDevice
{
public:
	virtual ~Zenith_RHIDevice() = default;

	// Identity and status
	virtual const Zenith_RHICapabilities& GetCapabilities() const = 0;
	virtual Zenith_RHIDeviceStatus GetStatus() const = 0;
	virtual uint32 GetFramesInFlight() const = 0;
	// Main thread at a render-idle point, after WaitIdle, with every swapchain drained: changes F to uFrames (2 or 3, else
	// INVALID_DESC), rebuilds the breadcrumb regions and every per-slot object (command pools, split-barrier event pools,
	// the acquire semaphores of every swapchain); frame-slot indices restart at 0. The old per-slot objects are backend
	// objects with no device handle, destroyed at once: WaitIdle made every queue's completed value equal its last
	// signalled value, so no GPU reference to them remains (a named immediate destruction of
	// Zenith2/Docs/Design/RHI.md "Rules across the API"). The new objects are created before the old ones are destroyed,
	// so OUT_OF_HOST_MEMORY or OUT_OF_DEVICE_MEMORY leave F and the old objects unchanged. The swapchain recreation path
	// is its one caller (Zenith2/Docs/Design/RHI/Recovery.md "Recreation state machine").
	virtual Zenith_Result<void, Zenith_RHIError> SetFramesInFlight(uint32 uFrames) = 0;

	// Resources (ANY_THREAD)
	virtual Zenith_Result<Zenith_RHIMemoryRequirements, Zenith_RHIError> GetTextureMemoryRequirements(const Zenith_RHITextureDesc& xDesc) = 0;
	virtual Zenith_Result<Zenith_RHIMemoryRequirements, Zenith_RHIError> GetBufferMemoryRequirements(const Zenith_RHIBufferDesc& xDesc) = 0;
	virtual Zenith_Result<Zenith_RHITextureHandle, Zenith_RHIError> CreateTexture(const Zenith_RHITextureDesc& xDesc) = 0;
	virtual Zenith_Result<Zenith_RHIBufferHandle, Zenith_RHIError> CreateBuffer(const Zenith_RHIBufferDesc& xDesc) = 0;
	virtual Zenith_Result<Zenith_RHIHeapHandle, Zenith_RHIError> CreateHeap(const Zenith_RHIHeapDesc& xDesc) = 0;
	virtual Zenith_Result<Zenith_RHITextureHandle, Zenith_RHIError> CreatePlacedTexture(Zenith_RHIHeapHandle xHeap, uint64 ulOffset, const Zenith_RHITextureDesc& xDesc) = 0;
	virtual Zenith_Result<Zenith_RHIBufferHandle, Zenith_RHIError> CreatePlacedBuffer(Zenith_RHIHeapHandle xHeap, uint64 ulOffset, const Zenith_RHIBufferDesc& xDesc) = 0;
	virtual Zenith_Result<void*, Zenith_RHIError> GetMappedPointer(Zenith_RHIBufferHandle xBuffer) = 0;   // UPLOAD and READBACK classes: persistent mapping
	virtual Zenith_Result<Zenith_RHITextureDesc, Zenith_RHIError> GetTextureDesc(Zenith_RHITextureHandle xTexture) const = 0;
	virtual Zenith_RHIRetirementQueue& GetRetirementQueue() = 0;   // the one way to destroy a resource
	virtual Zenith_RHIDescriptorHeap& GetDescriptorHeap() = 0;

	// Pipelines (ANY_THREAD; PSO creation jobs)
	virtual Zenith_Result<Zenith_Owned<Zenith_RHIPipelineCache>, Zenith_RHIError> CreatePipelineCache(Zenith_Span<const uint8> xInitialPayload) = 0;
	virtual Zenith_Result<Zenith_RHIPipelineCreateResult, Zenith_RHIError> CreateGraphicsPipeline(const Zenith_RHIGraphicsPipelineDesc& xDesc, Zenith_RHIPipelineCache* pxCache) = 0;
	virtual Zenith_Result<Zenith_RHIPipelineCreateResult, Zenith_RHIError> CreateComputePipeline(const Zenith_RHIComputePipelineDesc& xDesc, Zenith_RHIPipelineCache* pxCache) = 0;
	virtual Zenith_Result<Zenith_RHIPipelineCreateResult, Zenith_RHIError> CreateMeshPipeline(const Zenith_RHIMeshPipelineDesc& xDesc, Zenith_RHIPipelineCache* pxCache) = 0;

	// Queues and recording
	virtual Zenith_RHITimeline& GetTimeline(Zenith_RHIQueue eQueue) = 0;
	virtual bool HasDedicatedQueue(Zenith_RHIQueue eQueue) const = 0;
	virtual Zenith_RHIQueueValues GetCompletedValues() const = 0;       // one read per queue
	virtual Zenith_RHIQueueValues GetLastSignalledValues() const = 0;
	virtual void BeginFrameSlot(uint32 uFrameSlot) = 0;                                // resets the slot's pools and events
	virtual Zenith_Result<Zenith_RHICommandList*, Zenith_RHIError> AcquireCommandList(Zenith_RHIQueue eQueue, uint32 uFrameSlot, uint32 uPartition) = 0;
	virtual Zenith_Result<uint64, Zenith_RHIError> Submit(Zenith_RHIQueue eQueue, const Zenith_RHISubmitBatch& xBatch) = 0;   // returns the signalled value
	// Main thread at a render-idle point: WaitCPU on each physical queue's timeline for its GetLastSignalledValue(), each
	// with ulTimeoutNanoseconds and the loss check of Recovery.md "Device loss detection"; never vkDeviceWaitIdle, never a
	// D3D12 device-wide wait (elaboration, pending X89). On success GetCompletedValues() == GetLastSignalledValues().
	virtual Zenith_Result<void, Zenith_RHIError> WaitIdle(uint64 ulTimeoutNanoseconds) = 0;

	// Presentation (MAIN_THREAD while no frame is in flight)
	virtual Zenith_Result<Zenith_Owned<Zenith_RHISwapchain>, Zenith_RHIError> CreateSwapchain(const Zenith_RHISwapchainDesc& xDesc) = 0;

	// Queries
	virtual Zenith_Result<Zenith_RHIQueryPoolHandle, Zenith_RHIError> CreateTimestampPool(const Zenith_RHIQueryPoolDesc& xDesc) = 0;
	virtual void ResetTimestampPool(Zenith_RHIQueryPoolHandle xPool) = 0;               // hostQueryReset; frame-slot preparation
	virtual Zenith_Result<uint32, Zenith_RHIError> ReadTimestamps(Zenith_RHIQueryPoolHandle xPool, uint32 uFirst, Zenith_Span<uint64> aulTicksOut) = 0;
	virtual double GetTimestampPeriodNs(Zenith_RHIQueue eQueue) const = 0;
	virtual Zenith_RHIClockCalibration GetClockCalibration(Zenith_RHIQueue eQueue) = 0;

	// Memory
	virtual uint32 QueryHeapBudgets(Zenith_Span<Zenith_RHIHeapBudget> axOut) = 0;   // returns the heap count (≤ 16)
	virtual uint64 GetDeviceLocalUsageBytes() const = 0;                             // for Zenith_MemoryGovernor::ReportProcessUsage

	// Diagnostics
	virtual void SetDebugName(Zenith_RHIResourceHandle xResource, const char* szName) = 0;         // no-op unless m_bDebugNames
	virtual void CollectDeviceFaultReport(Zenith_RHIDeviceFaultReport& xReportOut) = 0;           // after loss
	virtual Zenith_RHICaptureTool* GetCaptureTool() = 0;
};

using Zenith_PfnCreateDevice = Zenith_Result<Zenith_Owned<Zenith_RHIDevice>, Zenith_RHIError> (*)(const Zenith_RHIDeviceDesc& xDesc);
```

`Zenith_RHIDevice` destroys resources only through its retirement queue: `GetRetirementQueue().Enqueue` (render-submit thread, explicit last use) or `RequestRelease` (any thread). There is no immediate destroy on the public surface, which is how the device retires every resource after every queue's last use ([Z2-N-CONTRACT-088]). The device resolves a handle's table slot through its per-kind table (`Zenith2/Docs/Design/RHI/Memory.md → "Device resource table"`).

## Backend factory headers

```cpp
// Zenith2/Engine/ZenithRHI_Vulkan/Zenith_RHI_Vulkan.h
#pragma once
#include "ZenithRHI/Zenith_RHIDevice.h"

/// @contract functions Zenith_RHI_Vulkan
/// Ownership: Returns the device as a Zenith_Owned<Zenith_RHIDevice> backed by the general heap in the desc.
/// Thread affinity: MAIN_THREAD (startup step 12, and device-loss recovery with the render-submit thread idle).
/// Invalidation: None.
/// Units: None.
/// Errors: ZENITH_RHI_ERROR_LOADER_UNAVAILABLE, NO_ADAPTER, PROFILE_UNSUPPORTED, DEVICE_CREATE, OUT_OF_HOST_MEMORY,
///   OUT_OF_DEVICE_MEMORY, INVALID_DESC; every failed step unwinds what the earlier steps created ([Z2-N-CONTRACT-340]).
/// Version: since Z2 Stage 2. BACKEND_NAME Vulkan; the factory symbol is the helper's FACTORY value.
/// @endcontract
Zenith_Result<Zenith_Owned<Zenith_RHIDevice>, Zenith_RHIError> Zenith_CreateRHI_Vulkan(const Zenith_RHIDeviceDesc& xDesc);
```

```cpp
// Zenith2/Engine/ZenithRHI_D3D12/Zenith_RHI_D3D12.h
#pragma once
#include "ZenithRHI/Zenith_RHIDevice.h"

/// @contract functions Zenith_RHI_D3D12
/// Ownership: As Zenith_RHI_Vulkan.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: The Agility SDK version is the SDK's integer version; the path is relative to the executable directory.
/// Errors: As Zenith_RHI_Vulkan, plus BACKEND_UNSUPPORTED.
/// Version: since Z2 Stage 2. BACKEND_NAME D3D12. The two constants follow ThirdParty/manifest.json's Agility SDK
///   pin; Test_D3D12_AgilityConstantsMatchManifest fails when they differ.
/// @endcontract
Zenith_Result<Zenith_Owned<Zenith_RHIDevice>, Zenith_RHIError> Zenith_CreateRHI_D3D12(const Zenith_RHIDeviceDesc& xDesc);
inline constexpr uint32 uZENITH_D3D12_AGILITY_SDK_VERSION = 0;          // the pinned SDK's D3D12SDKVersion; written by the ticket that pins it
inline constexpr const char* szZENITH_D3D12_AGILITY_SDK_PATH = ".\\D3D12\\";
```

`uZENITH_D3D12_AGILITY_SDK_VERSION` holds 0 until the manifest pin ticket writes the pinned version (the value is data from `ThirdParty/manifest.json`, which the same ticket commits); the generated composition TU of every Windows game exports `extern "C" __declspec(dllexport) const uint32 D3D12SDKVersion = uZENITH_D3D12_AGILITY_SDK_VERSION;` and `extern "C" __declspec(dllexport) const char* const D3D12SDKPath = szZENITH_D3D12_AGILITY_SDK_PATH;` (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Windows executable specifics"). Both exports are `const` objects in a read-only section, so the ForbiddenCheck writable-data scan (rule F16) finds nothing to allowlist. The staged runtime files under `<bin>/D3D12/` are `D3D12Core.dll` in every configuration and `d3d12SDKLayers.dll` in every configuration except Shipping.

## RHI-owned ZenithFlux public headers

`Flux_QueueClass`, `Flux_QueueLastUse` and `Flux_QueueCompletedValues` are the declarations of `Zenith2/Docs/Rendering/GpuResources.md → "Queues and timelines"`; `Flux_PersistentReadOnlyTexture`, `Flux_PersistentReadOnlyBuffer` and `Flux_PersistentResources` are those of `Zenith2/Docs/Rendering/GpuResources.md → "Persistent resources"`; `Flux_ReadbackHandle` and `Flux_ReadbackStatus` are those of `Zenith2/Docs/Rendering/GpuResources.md → "Readbacks"`. This design adds:

```cpp
// Zenith2/Engine/ZenithFlux/Flux_QueueValues.h (additions)
static_assert(FLUX_QUEUE_COUNT == ZENITH_RHI_QUEUE_COUNT);
using Flux_CapabilityTier = Zenith_RHICapabilityTier;          // elaboration: Rendering's name kept for Flux code until X73 lands
constexpr Zenith_RHIQueue Flux_ToRHIQueue(Flux_QueueClass eQueue);               // identity on values
constexpr Zenith_RHIQueueValues Flux_ToRHIValues(const Flux_QueueLastUse& xValues);
constexpr Zenith_RHIQueueValues Flux_ToRHIValues(const Flux_QueueCompletedValues& xValues);

/// @contract enum Flux_DeviceLossState
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2 (NORMAL and EXITING, the frame-failure exit); the loss states since Z2 Stage 3. One
///   enumerator per state of Zenith2/Docs/Design/RHI/Recovery.md "Device-loss state machine"; the runtime polls it
///   through Flux_Renderer::GetDeviceLossState and renders only while it is NORMAL (Zenith2/Docs/Design/RHI.md
///   "Render-idle points", assumption pending MOD, ARCH (X128)).
/// @endcontract
enum Flux_DeviceLossState : uint8
{
	FLUX_DEVICE_LOSS_STATE_NORMAL = 1,
	FLUX_DEVICE_LOSS_STATE_LOST,
	FLUX_DEVICE_LOSS_STATE_REPORTING,
	FLUX_DEVICE_LOSS_STATE_RECOVERING,
	FLUX_DEVICE_LOSS_STATE_REBUILDING,
	FLUX_DEVICE_LOSS_STATE_RESUMING,
	FLUX_DEVICE_LOSS_STATE_EXITING,
};

// Zenith2/Engine/ZenithFlux/Flux_PersistentResources.h (additions)
enum Flux_ResourceError : uint8
{
	FLUX_RESOURCE_ERROR_INVALID_DESC = 1,        // mapped from ZENITH_RHI_ERROR_INVALID_DESC
	FLUX_RESOURCE_ERROR_OUT_OF_DEVICE_MEMORY,    // the pressure response of Memory.md "Device memory exhaustion" ran
	FLUX_RESOURCE_ERROR_TABLE_FULL,
	FLUX_RESOURCE_ERROR_DESCRIPTOR_SPACE_FULL,
	FLUX_RESOURCE_ERROR_UPLOAD_QUEUE_FULL,       // the initial upload could not be queued; retry next tick
	FLUX_RESOURCE_ERROR_MEMORY_PRESSURE,         // shedding action 13 refuses new render targets ([Z2-N-REND-506])
	FLUX_RESOURCE_ERROR_DEVICE_LOST,
};
struct Flux_PersistentTextureDesc { const char* m_szName = nullptr; Zenith_RHITextureDimension m_eDimension = ZENITH_RHI_TEXTURE_DIMENSION_2D; Zenith_RHIFormat m_eFormat = ZENITH_RHI_FORMAT_UNDEFINED;
	Zenith_RHIExtent3D m_xExtent; uint32 m_uDepthOrLayers = 1; uint32 m_uMipCount = 1; uint32 m_uFirstResidentMip = 0; };
struct Flux_PersistentBufferDesc { const char* m_szName = nullptr; uint64 m_ulSize = 0; uint32 m_uStructureStride = 0; uint16 m_uUsage = 0; };   // usage union: vertex, index, storage read
struct Flux_UploadSource { Zenith_Span<const uint8> m_xBytes; uint32 m_uRowPitch = 0; bool m_bSplittable = true; };   // valid until the resource IsResident

// Zenith2/Engine/ZenithFlux/Flux_Readback.h (additions)
enum Flux_ReadbackError : uint8
{
	FLUX_READBACK_ERROR_POOL_FULL = 1,           // no pool range free: re-request next frame (BR-DEFER)
	FLUX_READBACK_ERROR_TOO_LARGE,               // the range exceeds the pool
	FLUX_READBACK_ERROR_DEVICE_LOST,
	FLUX_READBACK_ERROR_FRAME_DEFERRED,          // the declaring frame was deferred at the upload cap; no copy was recorded; re-request
	FLUX_READBACK_ERROR_FRAME_FAILED,            // the declaring frame failed (Memory.md "Frame failures"); no copy was recorded; re-request
};
```

Each of these types carries the contract block of its specification section, with these fields: `Flux_PersistentResources` — Ownership: owned by `Flux_Renderer`, one per device; Thread affinity: `ANY_THREAD` (asset loader jobs create and release; creation touches no imported-state entry and release frees no descriptor slot directly: the resource is registered when it joins the acquire list on the `Flux.BuildGraph` node, and released through `Zenith_RHIRetirementQueue::RequestReleaseWithDescriptor`, `Zenith2/Docs/Design/RHI/Memory.md → "Persistent resource creation and the acquire list"`); Errors: `Flux_ResourceError`; Locks: `uLOCK_LEVEL_FLUX_PERSISTENT` (480). `Flux_ReadbackHandle` — Ownership: move-only, owned by the requester, wrapping a `Zenith_OperationHandle` of the readback pool's operation table; Thread affinity: `OWNER_THREAD`; Invalidation: `GetData()` is non-empty exactly while `READY`, and the span stays valid until `Release()` or destruction ([Z2-N-REND-498]); Errors: `FAILED` with `FLUX_READBACK_ERROR_DEVICE_LOST` on device loss, `FAILED` with `FLUX_READBACK_ERROR_FRAME_DEFERRED` or `FLUX_READBACK_ERROR_FRAME_FAILED` when the frame that declared it submitted nothing (`Zenith2/Docs/Design/RHI/Memory.md → "Readback state machine"`), `CANCELLED` on `Cancel()` ([Z2-N-REND-501]).
