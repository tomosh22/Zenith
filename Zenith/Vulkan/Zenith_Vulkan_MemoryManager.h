#pragma once

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "vulkan/vulkan.hpp"
#include "vma/vk_mem_alloc.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

#include "Flux/Flux_Types.h"

class Zenith_Vulkan_Buffer;
class Flux_VertexBuffer;
class Flux_DynamicVertexBuffer;
class Flux_IndexBuffer;
class Flux_SurfaceInfo;
class Flux_VRAMHandle;
class Flux_DynamicConstantBuffer;
class Zenith_Vulkan_Texture;
class Zenith_Vulkan_CommandBuffer;
constexpr uint64_t g_uStagingPoolSize = 1024u * 1024u * 1024u;
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

	static void AllocateBuffer(size_t uSize, vk::BufferUsageFlags eUsageFlags, MemoryResidency eResidency, Zenith_Vulkan_Buffer& xBufferOut);
	static void FreeBuffer(Zenith_Vulkan_Buffer* pxBuffer);
	static void InitialiseVertexBuffer(const void* pData, size_t uSize, Flux_VertexBuffer& xBufferOut, bool bDeviceLocal = true);
	static void InitialiseDynamicVertexBuffer(const void* pData, size_t uSize, Flux_DynamicVertexBuffer& xBufferOut, bool bDeviceLocal = true);
	static void InitialiseIndexBuffer(const void* pData, size_t uSize, Flux_IndexBuffer& xBufferOut);
	static void InitialiseDynamicConstantBuffer(const void* pData, size_t uSize, Flux_DynamicConstantBuffer& xBufferOut);

	static void UploadBufferData(Zenith_Vulkan_Buffer& xBuffer, const void* pData, size_t uSize);
	static void UploadTextureData(Zenith_Vulkan_Texture& xTexture, const void* pData, size_t uSize);

	static Flux_VRAMHandle CreateTextureVRAM(const void* pData, const Flux_SurfaceInfo& xInfo, bool bCreateMips);
	static Flux_VRAMHandle CreateRenderTargetVRAM(const Flux_SurfaceInfo& xInfo);

	// View creation functions
	static vk::ImageView CreateRenderTargetView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel = 0);
	static vk::ImageView CreateDepthStencilView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel = 0);
	static vk::ImageView CreateShaderResourceView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uBaseMip = 0, uint32_t uMipCount = 1);
	static vk::ImageView CreateUnorderedAccessView(Flux_VRAMHandle xVRAMHandlee, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel = 0);

	static Zenith_Vulkan_CommandBuffer& GetCommandBuffer();
private:

	static void InitialiseStagingBuffer();

	static void HandleStagingBufferFull();

	static void FlushStagingBuffer();

	struct StagingTextureMetadata {
		vk::Image m_xImage;
		uint32_t m_uWidth;
		uint32_t m_uHeight;
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

	static VmaAllocator s_xAllocator;
	static Zenith_Vulkan_Buffer s_xStagingBuffer;
	static vk::DeviceMemory s_xStagingMem;

	static Zenith_Vulkan_CommandBuffer s_xCommandBuffer;

	static size_t s_uNextFreeStagingOffset;

	static Zenith_Mutex s_xMutex;
};
