#pragma once

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "vulkan/vulkan.hpp"
#pragma warning(push, 0)
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#endif
#include "vma/vk_mem_alloc.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#pragma warning(pop)
#include "Memory/Zenith_MemoryManagement_Enabled.h"

#include "Flux/Flux_Types.h"

class Zenith_Vulkan_VRAM;
class Zenith_Vulkan;
class Zenith_Vulkan_Swapchain;
class Flux_RendererImpl;
class Flux_GraphicsImpl;
class Flux_VertexBuffer;
class Flux_DynamicVertexBuffer;
class Flux_IndexBuffer;
struct Flux_SurfaceInfo;
class Flux_VRAMHandle;
class Flux_DynamicConstantBuffer;
class Flux_ConstantBuffer;
class Flux_IndirectBuffer;
class Flux_ReadWriteBuffer;
class Flux_DynamicReadWriteBuffer;
class Zenith_Vulkan_CommandBuffer;
constexpr uint64_t g_uStagingPoolSize = 512u * 1024u * 1024u;
#define ALIGN(size, align) ((size + align - 1) / align) * align

#include "Vulkan/Zenith_Vulkan_CommandBuffer.h"
#include "Collections/Zenith_Vector.h"

// Per-Engine state + behaviour for the Vulkan memory manager. Replaces the
// static-facade `class Zenith_Vulkan_MemoryManager` (deleted) and the
// data-only `Zenith_Vulkan_MemoryManager`. Accessed via
// g_xEngine.FluxMemory().
class Zenith_Vulkan_MemoryManager
{
public:

	Zenith_Vulkan_MemoryManager() {}
	~Zenith_Vulkan_MemoryManager() {

	}
	Zenith_Vulkan_MemoryManager(const Zenith_Vulkan_MemoryManager&) = delete;
	Zenith_Vulkan_MemoryManager& operator=(const Zenith_Vulkan_MemoryManager&) = delete;

	void Initialise(); // no-arg per FluxBackendMemoryAlloc concept; self-wires deps from g_xEngine
	void Shutdown();

	// Synchronously drain pending memory work: record staged uploads into the
	// internal command buffer, submit to the copy queue, CPU-wait, reset.
	// No-op when nothing is staged or recorded. MUST NOT take m_xMutex —
	// reached from HandleStagingBufferFull with the non-recursive mutex held,
	// possibly on a worker thread.
	void Flush();

	// Renderer-only once-per-frame drain: record staged uploads, end the
	// command buffer, and hand it to the backend (submitted ahead of render
	// work against the memory semaphore in Zenith_Vulkan::EndFrame). No-op
	// when no memory work exists this frame. No memory operation may run
	// between this call and Zenith_Vulkan::EndFrame — recording then would
	// reset a command buffer that is pending submission.
	void SubmitFrameMemoryWork();

	// Transient memory aliasing capability query. Backends that can bind
	// multiple images to disjoint offsets inside one allocation return true;
	// backends without aliasing support (or with the feature disabled at
	// build time) return false and the render graph falls back to one
	// allocation per transient. Vulkan returns true — VMA 3.x exposes the
	// aliasing primitives used by CreateAliasPoolVRAM / CreateAliasedImageVRAM.
	bool SupportsTransientAliasing();

	// Memory-class selector for an aliasing pool. DeviceLocal is the only
	// value used today (all render-graph transients live in VRAM); the
	// HostVisibleCoherent option exists so future readback transients
	// (e.g. GPU-side occlusion-query aggregates that the CPU reads the
	// next frame) can share a pool without adding another overload. The
	// enum values intentionally map 1:1 onto Vulkan's memory-property
	// bitmask in CreateAliasPoolVRAM.
	enum class AliasPoolMemoryKind : u_int
	{
		DeviceLocal,
		HostVisibleCoherent,
	};

	// Allocate a shared memory pool sized for multiple aliased images. The
	// returned VRAM owns the allocation and is destroyed via the normal
	// QueueVRAMDeletion path; aliased images bound to it must have been
	// destroyed first (the graph's DestroyTransients order guarantees this
	// via deferred-deletion frame counts).
	// ulAlignment is the max alignment across all transients that will be
	// packed into this pool (queried via ProbeImageMemoryRequirements).
	// Passing a correct alignment lets the pool ditch the conservative 64 KB
	// fallback that was used before the probe existed.
	// eMemoryKind defaults to DeviceLocal (the only current use case); pass
	// HostVisibleCoherent for readback pools once the render graph starts
	// producing them.
	Flux_VRAMHandle CreateAliasPoolVRAM(u_int64 ulSize, u_int64 ulAlignment,
	                                           AliasPoolMemoryKind eMemoryKind = AliasPoolMemoryKind::DeviceLocal);

	// Query the driver for the actual memory requirements of an image that
	// matches xInfo. Creates a throwaway VkImage, calls vkGetImageMemoryRequirements,
	// destroys it, and returns the reported size + alignment. The render graph
	// uses this to size aliasing pools precisely instead of the old heuristic
	// (ceil(raw texel bytes) * 2, rounded to 1 MiB). Both outputs are set to
	// 0 on driver failure; callers should treat 0 as "probe failed, fall back
	// to heuristic".
	void ProbeImageMemoryRequirements(const Flux_SurfaceInfo& xInfo,
	                                         u_int64& ulSizeOut,
	                                         u_int64& ulAlignmentOut);

	// Create a VkImage from xInfo and bind it into xPoolHandle's allocation
	// at the offset chosen by the packer. Returns the aliased-image VRAM
	// (m_bAliased == true; destructor will destroy only the image). The
	// caller is responsible for: (a) ensuring the pool is big enough,
	// (b) ensuring the offset respects the image's alignment requirements
	// (the VMA call asserts on misalignment).
	Flux_VRAMHandle CreateAliasedImageVRAM(const Flux_SurfaceInfo& xInfo,
	                                              Flux_VRAMHandle xPoolHandle,
	                                              u_int64 ulOffsetInPool);

	void ImageTransitionBarrier(vk::Image xImage, vk::ImageLayout eOldLayout, vk::ImageLayout eNewLayout, vk::ImageAspectFlags eAspect, vk::PipelineStageFlags eSrcStage, vk::PipelineStageFlags eDstStage, uint32_t uMipLevel = 0u, uint32_t uLayer = 0u);

	void InitialiseVertexBuffer(const void* pData, size_t uSize, Flux_VertexBuffer& xBufferOut, bool bDeviceLocal = true);
	void InitialiseDynamicVertexBuffer(const void* pData, size_t uSize, Flux_DynamicVertexBuffer& xBufferOut, bool bDeviceLocal = true);
	void InitialiseIndexBuffer(const void* pData, size_t uSize, Flux_IndexBuffer& xBufferOut);
	void InitialiseConstantBuffer(const void* pData, size_t uSize, Flux_ConstantBuffer& xBufferOut);
	void InitialiseDynamicConstantBuffer(const void* pData, size_t uSize, Flux_DynamicConstantBuffer& xBufferOut);
	void InitialiseIndirectBuffer(size_t uSize, Flux_IndirectBuffer& xBufferOut);
	void InitialiseReadWriteBuffer(const void* pData, size_t uSize, Flux_ReadWriteBuffer& xBufferOut);
	void InitialiseDynamicReadWriteBuffer(const void* pData, size_t uSize, Flux_DynamicReadWriteBuffer& xBufferOut);

	void UploadBufferData(Flux_VRAMHandle xBufferHandle, const void* pData, size_t uSize);
	void UploadBufferDataAtOffset(Flux_VRAMHandle xBufferHandle, const void* pData, size_t uSize, size_t uDestOffset);

	// Buffer destruction functions - queue VRAM for deferred deletion
	void DestroyVertexBuffer(Flux_VertexBuffer& xBuffer);
	void DestroyDynamicVertexBuffer(Flux_DynamicVertexBuffer& xBuffer);
	void DestroyIndexBuffer(Flux_IndexBuffer& xBuffer);
	void DestroyConstantBuffer(Flux_ConstantBuffer& xBuffer);
	void DestroyDynamicConstantBuffer(Flux_DynamicConstantBuffer& xBuffer);
	void DestroyIndirectBuffer(Flux_IndirectBuffer& xBuffer);
	void DestroyReadWriteBuffer(Flux_ReadWriteBuffer& xBuffer);
	void DestroyDynamicReadWriteBuffer(Flux_DynamicReadWriteBuffer& xBuffer);

	Flux_VRAMHandle CreateBufferVRAM(const u_int uSize, const MemoryFlags eFlags, MemoryResidency eResidency);
	Flux_VRAMHandle CreateTextureVRAM(const void* pData, const Flux_SurfaceInfo& xInfo, bool bCreateMips);
	// Full-image in-place re-upload of an existing texture created via
	// CreateTextureVRAM (terrain-editor live splatmap painting). xInfo MUST be
	// the surface info the texture was created with (same extents / format /
	// mip count — the staging flush copies mip 0 over the whole extent and
	// regenerates the mip chain). Race-free even while the texture is sampled
	// by in-flight frames: the data is staged into the current frame's slot and
	// the memory command buffer is submitted on the graphics queue AHEAD of
	// render work (which waits on the memory semaphore); the flush's
	// eUndefined -> TRANSFER_DST transition discards prior contents, which is
	// safe only because the whole image is overwritten.
	void UpdateTextureVRAM(Flux_VRAMHandle xHandle, const void* pData, const Flux_SurfaceInfo& xInfo);
	Flux_VRAMHandle CreateRenderTargetVRAM(const Flux_SurfaceInfo& xInfo);

	// Create a persistently mapped host-visible buffer (for scratch buffers, etc.)
	struct PersistentBuffer
	{
		vk::Buffer m_xBuffer;
		VmaAllocation m_xAllocation;
		void* m_pMappedPtr;
		u_int m_uSize;
	};
	PersistentBuffer CreatePersistentlyMappedBuffer(u_int uSize, vk::BufferUsageFlags eUsageFlags);

	// View creation functions - return Flux view structs with abstract handles.
	// NOTE: uMipLevel has NO default on any view type that selects a single mip
	// (RTV / RTVForLayer / DSV / UAV). The previous RTV default (0) silently
	// collapsed every per-mip RTV in BuildColour / BuildColourFromAliasedVRAM
	// to mip 0 because those loops forgot to pass the mip index. The same
	// foot-gun exists for DSV and UAV — any loop over mip levels that forgets
	// to forward the index would silently write every iteration to mip 0.
	// Dropping the default forces a compile error at any site that relied on
	// the bug. The SRV overloads keep their (uBaseMip=0, uMipCount=1) defaults
	// because a single-mip view of mip 0 is a genuine common case and the
	// two-parameter form is unambiguous.
	Flux_RenderTargetView CreateRenderTargetView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel);
	Flux_RenderTargetView CreateRenderTargetViewForLayer(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uLayer, uint32_t uMipLevel);
	Flux_DepthStencilView CreateDepthStencilView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel);
	Flux_ShaderResourceView CreateShaderResourceView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uBaseMip = 0, uint32_t uMipCount = 1);
	Flux_ShaderResourceView CreateShaderResourceViewForLayer(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uLayer, uint32_t uBaseMip = 0, uint32_t uMipCount = 1);
	Flux_UnorderedAccessView_Texture CreateUnorderedAccessView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel);
	Flux_UnorderedAccessView_Texture CreateUnorderedAccessViewForSlice(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uSlice, uint32_t uMipLevel);

	// Handle registry system for abstracting Vulkan types from Flux layer
	Flux_ImageViewHandle RegisterImageView(vk::ImageView xView);
	vk::ImageView GetImageView(Flux_ImageViewHandle xHandle);
	void ReleaseImageViewHandle(Flux_ImageViewHandle xHandle);

	Flux_BufferDescriptorHandle RegisterBufferDescriptor(const vk::DescriptorBufferInfo& xInfo);
	vk::DescriptorBufferInfo GetBufferDescriptor(Flux_BufferDescriptorHandle xHandle);
	void ReleaseBufferDescriptorHandle(Flux_BufferDescriptorHandle xHandle);

	// Deferred deletion system - accepts abstract handles to keep Vulkan types internal.
	// NOTE: xHandle is taken by reference and auto-invalidated after queuing to prevent double-free.
	// uExtraFrameDelay is added on top of the standard MAX_FRAMES_IN_FLIGHT + 1 grace period.
	// Used by the aliasing path to queue the pool VRAM with one extra frame of delay, guaranteeing
	// all aliased images in it are destroyed before the pool's VkDeviceMemory is freed — otherwise
	// ProcessDeferredDeletions's RemoveSwap iteration can destroy pool and aliased image in any
	// order within the same frame, which Vulkan (correctly) rejects.
	void QueueVRAMDeletion(Flux_VRAMHandle& xHandle,
		Flux_ImageViewHandle xRTV = Flux_ImageViewHandle(), Flux_ImageViewHandle xDSV = Flux_ImageViewHandle(),
		Flux_ImageViewHandle xSRV = Flux_ImageViewHandle(), Flux_ImageViewHandle xUAV = Flux_ImageViewHandle(),
		u_int uExtraFrameDelay = 0);
	void QueueImageViewDeletion(Flux_ImageViewHandle xImageViewHandle);
	void ProcessDeferredDeletions();

	void IncreaseImageMemoryUsage(u_int64 ulSize);
	void DecreaseImageMemoryUsage(u_int64 ulSize);
	const u_int64* GetImageMemoryUsagePtr();

	void IncreaseBufferMemoryUsage(u_int64 ulSize);
	void DecreaseBufferMemoryUsage(u_int64 ulSize);
	const u_int64* GetBufferMemoryUsagePtr();

	void IncreaseMemoryUsage(u_int64 ulSize);
	void DecreaseMemoryUsage(u_int64 ulSize);
	const u_int64* GetMemoryUsagePtr();

	// VMA statistics - returns actual allocated GPU memory from VMA
	struct VMAStats
	{
		u_int64 m_ulTotalAllocatedBytes;
		u_int64 m_ulTotalUsedBytes;
		u_int64 m_ulAllocationCount;
	};
	VMAStats GetVMAStats();

	// Direct access to VMA allocator (for advanced buffer creation)
	VmaAllocator GetVMAAllocator();

	// Determines the VkImageViewType from surface info flags (3D, cube, array, or 2D)
	vk::ImageViewType DetermineImageViewType(const Flux_SurfaceInfo& xInfo);

private:

	// Common buffer destruction logic - validates handle, queues VRAM deletion
	void DestroySimpleBuffer(Flux_VRAMHandle& xHandle);

	// Destroys an image view if the handle is valid, then releases the handle
	void DestroyImageViewIfValid(const vk::Device& xDevice, Flux_ImageViewHandle& xHandle);

	void InitialiseStagingBuffer();

	void HandleStagingBufferFull();

	// Helper for chunked staging uploads when data exceeds staging buffer size
	void UploadBufferDataChunked(vk::Buffer xDestBuffer, const void* pData, size_t uSize);
	void UploadTextureDataChunked(vk::Image xDestImage, const void* pData, size_t uSize, uint32_t uWidth, uint32_t uHeight, uint32_t uNumMips, uint32_t uNumLayers);

	// CreateTextureVRAM helpers — extracted to keep the orchestrator small and
	// to give the s_xMutex invariant a single named owner. Allocation runs
	// pre-lock; only UploadTextureData acquires/releases s_xMutex.

	// Patch defaults onto a Flux_SurfaceInfo: derive mip count from extents
	// when bCreateMips is true, clamp depth/layers to a min of 1 to keep
	// downstream byte calculations honest.
	void NormalizeTextureInfo(Flux_SurfaceInfo& xInfo, bool bCreateMips);

	// Pure: produce a vk::ImageCreateInfo from the (already normalized) surface
	// info. Picks 2D vs 3D type, derives the usage mask from the memory flags,
	// and tags cubemaps with eCubeCompatible. No side effects, so this is safe
	// to call before the staging-buffer mutex is acquired.
	vk::ImageCreateInfo BuildImageCreateInfo(const Flux_SurfaceInfo& xInfo);

	// Allocate the VkImage via VMA and register it as a Flux_VRAMHandle.
	// Pure with respect to s_xMutex — does NOT acquire it. Returns an invalid
	// handle on allocation failure (caller treats as an early-out).
	Flux_VRAMHandle AllocateAndRegisterImage(const vk::ImageCreateInfo& xImageInfo,
		VkImage& xImageOut, VmaAllocation& xAllocationOut);

	// Upload pData to the freshly-allocated image via the right path
	// (host-visible direct copy / chunked / staging-pool bump-allocate).
	// Acquires s_xMutex internally; caller MUST NOT hold it on entry.
	void UploadTextureData(VkImage xImage, VmaAllocation xAllocation,
		const void* pData, const Flux_SurfaceInfo& xInfo, size_t ulDataSize);

	struct StagingTextureMetadata {
		vk::Image m_xImage;
		uint32_t m_uWidth;
		uint32_t m_uHeight;
		uint32_t m_uDepth;
		uint32_t m_uNumMips;
		uint32_t m_uNumLayers;
		TextureFormat m_eFormat;
	};

	struct StagingBufferMetadata {
		vk::Buffer m_xBuffer;
		size_t m_uDestOffset;
	};

	struct StagingMemoryAllocation {
		StagingMemoryAllocation() : m_eType(ALLOCATION_TYPE_COUNT), m_uSize(0), m_uOffset(0) {}

		AllocationType m_eType;
		union {
			StagingBufferMetadata m_xBufferMetadata;
			StagingTextureMetadata m_xTextureMetadata;
		};
		size_t m_uSize;
		size_t m_uOffset;
	};

	// Per-frame staging state. Each in-flight frame slot owns its own staging
	// buffer + memory + bump allocator + pending-allocation list, so CPU writes
	// for slot K can never touch staging memory the GPU is still consuming for
	// slot K+1's in-flight transfer. The per-frame fence already serialises
	// same-slot reuse (slot K's frame N+MFIF can't begin until slot K's frame
	// N is complete), and frame indexing covers the cross-slot case.
	//
	// Sized to MAX_FRAMES_IN_FLIGHT * g_uStagingPoolSize total — 1 GiB on
	// Windows (MFIF=2) and 2 GiB on Android (MFIF=4). Android sizing should
	// be tuned in a follow-up; this patch targets Windows RenderTest stability.
	struct PerFrameStaging {
		vk::Buffer       m_xBuffer;
		vk::DeviceMemory m_xMemory;
		size_t           m_uNextFreeOffset = 0;
		Zenith_Vector<StagingMemoryAllocation> m_xAllocations;
		// Debug counters, surfaced via debug variables so the engine can
		// observe whether the bump allocator is approaching the pool size and
		// how often mid-frame flushes are firing on each slot.
		size_t           m_uHighWaterMark      = 0;
		uint32_t         m_uMidFrameFlushCount = 0;
	};
	// PerFrameStaging slots (m_axStaging) live on
	// Zenith_Vulkan_MemoryManager held by Zenith_Engine. Access via
	// g_xEngine.FluxMemory().m_axStaging or through CurrentStaging() below.

	// Resolves to the staging slot for the current in-flight frame. Both the
	// CPU memcpy path and the GPU vkCmdCopyBuffer recording path target the
	// same slot, so a single accessor centralises the lookup.
	PerFrameStaging& CurrentStaging();

	// Record the current frame slot's pending staged uploads into the command
	// buffer (does not submit — Flush / SubmitFrameMemoryWork own submission).
	void FlushStagingBuffer();

	// Staging flush helpers (split by allocation type for readability)
	void FlushStagingBufferAllocation(const StagingMemoryAllocation& xAlloc);
	void FlushStagingTextureAllocation(const StagingMemoryAllocation& xAlloc);

	// Shared mipmap generation via blit chain, then transition all mips to shader-read layout.
	// For compressed textures (bIsCompressed=true), blitting is skipped (mips must be pre-generated).
	void GenerateMipmapsAndTransitionToShaderRead(vk::Image xImage, uint32_t uWidth, uint32_t uHeight, uint32_t uNumMips, uint32_t uLayer, bool bIsCompressed);

	// Deferred deletion tracking
	struct PendingVRAMDeletion {
		Zenith_Vulkan_VRAM* m_pxVRAM;
		Flux_VRAMHandle m_xHandle;
		uint32_t m_uFramesRemaining;

		// Image view handles that need to be destroyed
		Flux_ImageViewHandle m_xRTV;
		Flux_ImageViewHandle m_xDSV;
		Flux_ImageViewHandle m_xSRV;
		Flux_ImageViewHandle m_xUAV;
	};

	// Injected cross-subsystem dependencies (stored in Initialise). Replace the
	// former g_xEngine.FluxBackend() / VulkanSwapchain() / FluxRenderer() /
	// FluxGraphics() reaches in instance methods, removing the hard global
	// coupling. Static callbacks and the Zenith_Vulkan_VRAM ctor/dtor (a
	// different class with no Initialise seam) still route through g_xEngine.
	Zenith_Vulkan*           m_pxVulkan          = nullptr;
	Zenith_Vulkan_Swapchain* m_pxVulkanSwapchain = nullptr;
	Flux_GraphicsImpl*       m_pxFluxGraphics    = nullptr;

	// Lazily opens the internal command buffer. Every path that records into
	// m_xCommandBuffer calls this first, so memory operations are legal at any
	// time — no frame bracket required. MUST NOT take m_xMutex (reached from
	// HandleStagingBufferFull with the non-recursive mutex held).
	void EnsureRecording();
	bool m_bRecording = false;

	// Internal command buffer for staging flushes and layout transitions.
	// Lifecycle is driven lazily by EnsureRecording and drained by Flush
	// (synchronous) or SubmitFrameMemoryWork (deferred per-frame handoff).
	Zenith_Vulkan_CommandBuffer m_xCommandBuffer;
public:
	// ===== Data members (was Zenith_Vulkan_MemoryManager) =====

	// VMA allocator instance.
	VmaAllocator                m_xAllocator = nullptr;

	// Per-frame staging slots (buffer offset, allocations, etc.).
	PerFrameStaging m_axStaging[MAX_FRAMES_IN_FLIGHT];

	// Deferred-deletion queue. Drained per-frame as the deletion clock ticks.
	Zenith_Vector<PendingVRAMDeletion> m_xPendingDeletions;

	// Memory accounting (read by the editor memory panel).
	u_int64                     m_ulImageMemoryUsed  = 0;
	u_int64                     m_ulBufferMemoryUsed = 0;
	u_int64                     m_ulMemoryUsed       = 0;

	// Mutex guarding the staging ring against worker-thread upload races.
	Zenith_Mutex                m_xMutex;

	// Handle registries -- abstract Vulkan types behind opaque handle ints
	// so engine code stays portable across backends.
	Zenith_Vector<vk::ImageView>            m_xImageViewRegistry;
	Zenith_Vector<u_int>                    m_xFreeImageViewHandles;
	Zenith_Vector<vk::DescriptorBufferInfo> m_xBufferDescriptorRegistry;
	Zenith_Vector<u_int>                    m_xFreeBufferDescHandles;
};
