#pragma once

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "vulkan/vulkan.hpp"
#include "vma/vk_mem_alloc.h"
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

	static void ImageTransitionBarrier(vk::Image xImage, vk::ImageLayout eOldLayout, vk::ImageLayout eNewLayout, vk::ImageAspectFlags eAspect, vk::PipelineStageFlags eSrcStage, vk::PipelineStageFlags eDstStage, uint32_t uMipLevel = 0u, uint32_t uLayer = 0u);

	static void InitialiseVertexBuffer(const void* pData, size_t uSize, Flux_VertexBuffer& xBufferOut, bool bDeviceLocal = true);
	static void InitialiseDynamicVertexBuffer(const void* pData, size_t uSize, Flux_DynamicVertexBuffer& xBufferOut, bool bDeviceLocal = true);
	static void InitialiseIndexBuffer(const void* pData, size_t uSize, Flux_IndexBuffer& xBufferOut);
	static void InitialiseConstantBuffer(const void* pData, size_t uSize, Flux_ConstantBuffer& xBufferOut);
	static void InitialiseDynamicConstantBuffer(const void* pData, size_t uSize, Flux_DynamicConstantBuffer& xBufferOut);
	static void InitialiseIndirectBuffer(size_t uSize, Flux_IndirectBuffer& xBufferOut);
	static void InitialiseReadWriteBuffer(const void* pData, size_t uSize, Flux_ReadWriteBuffer& xBufferOut);

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

	static Flux_VRAMHandle CreateBufferVRAM(const u_int uSize, const MemoryFlags eFlags, MemoryResidency eResidency);
	static Flux_VRAMHandle CreateTextureVRAM(const void* pData, const Flux_SurfaceInfo& xInfo, bool bCreateMips);
	static Flux_VRAMHandle CreateRenderTargetVRAM(const Flux_SurfaceInfo& xInfo);

	// View creation functions - return Flux view structs with abstract handles
	static Flux_RenderTargetView CreateRenderTargetView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel = 0);
	static Flux_DepthStencilView CreateDepthStencilView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel = 0);
	static Flux_ShaderResourceView CreateShaderResourceView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uBaseMip = 0, uint32_t uMipCount = 1);
	static Flux_UnorderedAccessView_Texture CreateUnorderedAccessView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel = 0);

	// Handle registry system for abstracting Vulkan types from Flux layer
	static Flux_ImageViewHandle RegisterImageView(vk::ImageView xView);
	static vk::ImageView GetImageView(Flux_ImageViewHandle xHandle);
	static void ReleaseImageViewHandle(Flux_ImageViewHandle xHandle);

	static Flux_BufferDescriptorHandle RegisterBufferDescriptor(const vk::DescriptorBufferInfo& xInfo);
	static vk::DescriptorBufferInfo GetBufferDescriptor(Flux_BufferDescriptorHandle xHandle);
	static void ReleaseBufferDescriptorHandle(Flux_BufferDescriptorHandle xHandle);

	static Zenith_Vulkan_CommandBuffer& GetCommandBuffer();

	// Deferred deletion system - accepts abstract handles to keep Vulkan types internal
	static void QueueVRAMDeletion(Zenith_Vulkan_VRAM* pxVRAM, const Flux_VRAMHandle xHandle,
		Flux_ImageViewHandle xRTV = Flux_ImageViewHandle(), Flux_ImageViewHandle xDSV = Flux_ImageViewHandle(),
		Flux_ImageViewHandle xSRV = Flux_ImageViewHandle(), Flux_ImageViewHandle xUAV = Flux_ImageViewHandle());
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
private:

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
	static std::list<StagingMemoryAllocation> s_xStagingAllocations;

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
	static std::list<PendingVRAMDeletion> s_xPendingDeletions;

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
	static std::vector<vk::ImageView> s_xImageViewRegistry;
	static std::vector<u_int> s_xFreeImageViewHandles;
	static std::vector<vk::DescriptorBufferInfo> s_xBufferDescriptorRegistry;
	static std::vector<u_int> s_xFreeBufferDescHandles;
};
