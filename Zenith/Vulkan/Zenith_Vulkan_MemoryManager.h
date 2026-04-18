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
#include "Flux/Flux.h"

class Zenith_Vulkan_VRAM;
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

class Zenith_Vulkan_MemoryManager
{
public:
	
	Zenith_Vulkan_MemoryManager() {}
	~Zenith_Vulkan_MemoryManager() {
		
	}

	static void Initialise();
	static void Shutdown();


	static void BeginFrame();
	static void EndFrame(bool bDefer = true);

	// Transient memory aliasing capability query. Backends that can bind
	// multiple images to disjoint offsets inside one allocation return true;
	// backends without aliasing support (or with the feature disabled at
	// build time) return false and the render graph falls back to one
	// allocation per transient. Vulkan returns true — VMA 3.x exposes the
	// aliasing primitives used by CreateAliasPoolVRAM / CreateAliasedImageVRAM.
	static bool SupportsTransientAliasing();

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
	static Flux_VRAMHandle CreateAliasPoolVRAM(u_int64 ulSize, u_int64 ulAlignment,
	                                           AliasPoolMemoryKind eMemoryKind = AliasPoolMemoryKind::DeviceLocal);

	// Query the driver for the actual memory requirements of an image that
	// matches xInfo. Creates a throwaway VkImage, calls vkGetImageMemoryRequirements,
	// destroys it, and returns the reported size + alignment. The render graph
	// uses this to size aliasing pools precisely instead of the old heuristic
	// (ceil(raw texel bytes) * 2, rounded to 1 MiB). Both outputs are set to
	// 0 on driver failure; callers should treat 0 as "probe failed, fall back
	// to heuristic".
	static void ProbeImageMemoryRequirements(const Flux_SurfaceInfo& xInfo,
	                                         u_int64& ulSizeOut,
	                                         u_int64& ulAlignmentOut);

	// Create a VkImage from xInfo and bind it into xPoolHandle's allocation
	// at the offset chosen by the packer. Returns the aliased-image VRAM
	// (m_bAliased == true; destructor will destroy only the image). The
	// caller is responsible for: (a) ensuring the pool is big enough,
	// (b) ensuring the offset respects the image's alignment requirements
	// (the VMA call asserts on misalignment).
	static Flux_VRAMHandle CreateAliasedImageVRAM(const Flux_SurfaceInfo& xInfo,
	                                              Flux_VRAMHandle xPoolHandle,
	                                              u_int64 ulOffsetInPool);

	// Per-frame end callback registered with Flux_PerFrame at Initialise time.
	// Drives the deferred-VRAM-deletion countdown that used to live inside
	// EndFrame. Registered AFTER the Vulkan begin-frame callback so it runs
	// at end-of-frame after any in-flight render submission has been queued.
	static void OnFluxPerFrameEnd(u_int uRingIndex, void* pUserData);

	static void ImageTransitionBarrier(vk::Image xImage, vk::ImageLayout eOldLayout, vk::ImageLayout eNewLayout, vk::ImageAspectFlags eAspect, vk::PipelineStageFlags eSrcStage, vk::PipelineStageFlags eDstStage, uint32_t uMipLevel = 0u, uint32_t uLayer = 0u);

	static void InitialiseVertexBuffer(const void* pData, size_t uSize, Flux_VertexBuffer& xBufferOut, bool bDeviceLocal = true);
	static void InitialiseDynamicVertexBuffer(const void* pData, size_t uSize, Flux_DynamicVertexBuffer& xBufferOut, bool bDeviceLocal = true);
	static void InitialiseIndexBuffer(const void* pData, size_t uSize, Flux_IndexBuffer& xBufferOut);
	static void InitialiseConstantBuffer(const void* pData, size_t uSize, Flux_ConstantBuffer& xBufferOut);
	static void InitialiseDynamicConstantBuffer(const void* pData, size_t uSize, Flux_DynamicConstantBuffer& xBufferOut);
	static void InitialiseIndirectBuffer(size_t uSize, Flux_IndirectBuffer& xBufferOut);
	static void InitialiseReadWriteBuffer(const void* pData, size_t uSize, Flux_ReadWriteBuffer& xBufferOut);
	static void InitialiseDynamicReadWriteBuffer(const void* pData, size_t uSize, Flux_DynamicReadWriteBuffer& xBufferOut);

	static void UploadBufferData(Flux_VRAMHandle xBufferHandle, const void* pData, size_t uSize);
	static void UploadBufferDataAtOffset(Flux_VRAMHandle xBufferHandle, const void* pData, size_t uSize, size_t uDestOffset);

	// Buffer destruction functions - queue VRAM for deferred deletion
	static void DestroyVertexBuffer(Flux_VertexBuffer& xBuffer);
	static void DestroyDynamicVertexBuffer(Flux_DynamicVertexBuffer& xBuffer);
	static void DestroyIndexBuffer(Flux_IndexBuffer& xBuffer);
	static void DestroyConstantBuffer(Flux_ConstantBuffer& xBuffer);
	static void DestroyDynamicConstantBuffer(Flux_DynamicConstantBuffer& xBuffer);
	static void DestroyIndirectBuffer(Flux_IndirectBuffer& xBuffer);
	static void DestroyReadWriteBuffer(Flux_ReadWriteBuffer& xBuffer);
	static void DestroyDynamicReadWriteBuffer(Flux_DynamicReadWriteBuffer& xBuffer);

	static Flux_VRAMHandle CreateBufferVRAM(const u_int uSize, const MemoryFlags eFlags, MemoryResidency eResidency);
	static Flux_VRAMHandle CreateTextureVRAM(const void* pData, const Flux_SurfaceInfo& xInfo, bool bCreateMips);
	static Flux_VRAMHandle CreateRenderTargetVRAM(const Flux_SurfaceInfo& xInfo);

	// Create a persistently mapped host-visible buffer (for scratch buffers, etc.)
	struct PersistentBuffer
	{
		vk::Buffer m_xBuffer;
		VmaAllocation m_xAllocation;
		void* m_pMappedPtr;
		u_int m_uSize;
	};
	static PersistentBuffer CreatePersistentlyMappedBuffer(u_int uSize, vk::BufferUsageFlags eUsageFlags);

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
	static Flux_RenderTargetView CreateRenderTargetView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel);
	static Flux_RenderTargetView CreateRenderTargetViewForLayer(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uLayer, uint32_t uMipLevel);
	static Flux_DepthStencilView CreateDepthStencilView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel);
	static Flux_ShaderResourceView CreateShaderResourceView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uBaseMip = 0, uint32_t uMipCount = 1);
	static Flux_ShaderResourceView CreateShaderResourceViewForLayer(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uLayer, uint32_t uBaseMip = 0, uint32_t uMipCount = 1);
	static Flux_UnorderedAccessView_Texture CreateUnorderedAccessView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel);
	static Flux_UnorderedAccessView_Texture CreateUnorderedAccessViewForSlice(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uSlice, uint32_t uMipLevel);

	// Handle registry system for abstracting Vulkan types from Flux layer
	static Flux_ImageViewHandle RegisterImageView(vk::ImageView xView);
	static vk::ImageView GetImageView(Flux_ImageViewHandle xHandle);
	static void ReleaseImageViewHandle(Flux_ImageViewHandle xHandle);

	static Flux_BufferDescriptorHandle RegisterBufferDescriptor(const vk::DescriptorBufferInfo& xInfo);
	static vk::DescriptorBufferInfo GetBufferDescriptor(Flux_BufferDescriptorHandle xHandle);
	static void ReleaseBufferDescriptorHandle(Flux_BufferDescriptorHandle xHandle);

	static Zenith_Vulkan_CommandBuffer& GetCommandBuffer();

	// Deferred deletion system - accepts abstract handles to keep Vulkan types internal.
	// NOTE: xHandle is taken by reference and auto-invalidated after queuing to prevent double-free.
	// uExtraFrameDelay is added on top of the standard MAX_FRAMES_IN_FLIGHT + 1 grace period.
	// Used by the aliasing path to queue the pool VRAM with one extra frame of delay, guaranteeing
	// all aliased images in it are destroyed before the pool's VkDeviceMemory is freed — otherwise
	// ProcessDeferredDeletions's RemoveSwap iteration can destroy pool and aliased image in any
	// order within the same frame, which Vulkan (correctly) rejects.
	static void QueueVRAMDeletion(Zenith_Vulkan_VRAM* pxVRAM, Flux_VRAMHandle& xHandle,
		Flux_ImageViewHandle xRTV = Flux_ImageViewHandle(), Flux_ImageViewHandle xDSV = Flux_ImageViewHandle(),
		Flux_ImageViewHandle xSRV = Flux_ImageViewHandle(), Flux_ImageViewHandle xUAV = Flux_ImageViewHandle(),
		u_int uExtraFrameDelay = 0);
	static void QueueImageViewDeletion(Flux_ImageViewHandle xImageViewHandle);
	static void ProcessDeferredDeletions();

	static void FlushStagingBuffer();

	static void IncreaseImageMemoryUsage(u_int64 ulSize) { s_ulImageMemoryUsed += ulSize; }
	static void DecreaseImageMemoryUsage(u_int64 ulSize) { s_ulImageMemoryUsed -= ulSize; }
	static const u_int64* GetImageMemoryUsagePtr() { return &s_ulImageMemoryUsed; }

	static void IncreaseBufferMemoryUsage(u_int64 ulSize) { s_ulBufferMemoryUsed += ulSize; }
	static void DecreaseBufferMemoryUsage(u_int64 ulSize) { s_ulBufferMemoryUsed -= ulSize; }
	static const u_int64* GetBufferMemoryUsagePtr() { return &s_ulBufferMemoryUsed; }

	static void IncreaseMemoryUsage(u_int64 ulSize) { s_ulMemoryUsed += ulSize; }
	static void DecreaseMemoryUsage(u_int64 ulSize) { s_ulMemoryUsed -= ulSize; }
	static const u_int64* GetMemoryUsagePtr() { return &s_ulMemoryUsed; }

	// VMA statistics - returns actual allocated GPU memory from VMA
	struct VMAStats
	{
		u_int64 m_ulTotalAllocatedBytes;
		u_int64 m_ulTotalUsedBytes;
		u_int64 m_ulAllocationCount;
	};
	static VMAStats GetVMAStats();

	// Direct access to VMA allocator (for advanced buffer creation)
	static VmaAllocator GetVMAAllocator() { return s_xAllocator; }

	// Determines the VkImageViewType from surface info flags (3D, cube, array, or 2D)
	static vk::ImageViewType DetermineImageViewType(const Flux_SurfaceInfo& xInfo);

private:

	// Common buffer destruction logic - validates handle, queues VRAM deletion
	static void DestroySimpleBuffer(Flux_VRAMHandle& xHandle);

	// Destroys an image view if the handle is valid, then releases the handle
	static void DestroyImageViewIfValid(const vk::Device& xDevice, Flux_ImageViewHandle& xHandle);

	static void InitialiseStagingBuffer();

	static void HandleStagingBufferFull();

	// Helper for chunked staging uploads when data exceeds staging buffer size
	static void UploadBufferDataChunked(vk::Buffer xDestBuffer, const void* pData, size_t uSize);
	static void UploadTextureDataChunked(vk::Image xDestImage, const void* pData, size_t uSize, uint32_t uWidth, uint32_t uHeight, uint32_t uNumMips, uint32_t uNumLayers);

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
	static Zenith_Vector<StagingMemoryAllocation> s_xStagingAllocations;

	// Staging flush helpers (split by allocation type for readability)
	static void FlushStagingBufferAllocation(const StagingMemoryAllocation& xAlloc);
	static void FlushStagingTextureAllocation(const StagingMemoryAllocation& xAlloc);

	// Shared mipmap generation via blit chain, then transition all mips to shader-read layout.
	// For compressed textures (bIsCompressed=true), blitting is skipped (mips must be pre-generated).
	static void GenerateMipmapsAndTransitionToShaderRead(vk::Image xImage, uint32_t uWidth, uint32_t uHeight, uint32_t uNumMips, uint32_t uLayer, bool bIsCompressed);

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
	static Zenith_Vector<PendingVRAMDeletion> s_xPendingDeletions;

	static VmaAllocator s_xAllocator;
	static vk::Buffer s_xStagingBuffer;
	static vk::DeviceMemory s_xStagingMem;

	static Zenith_Vulkan_CommandBuffer s_xCommandBuffer;

	static size_t s_uNextFreeStagingOffset;

	static Zenith_Mutex s_xMutex;

	static u_int64 s_ulImageMemoryUsed;
	static u_int64 s_ulBufferMemoryUsed;
	static u_int64 s_ulMemoryUsed;

	// Handle registry for abstracting Vulkan types
	static Zenith_Vector<vk::ImageView> s_xImageViewRegistry;
	static Zenith_Vector<u_int> s_xFreeImageViewHandles;
	static Zenith_Vector<vk::DescriptorBufferInfo> s_xBufferDescriptorRegistry;
	static Zenith_Vector<u_int> s_xFreeBufferDescHandles;
};
