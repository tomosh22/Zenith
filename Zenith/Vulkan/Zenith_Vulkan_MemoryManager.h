#pragma once

#include "vulkan/vulkan.hpp"
#include "Flux/Flux_Enums.h"

class Zenith_Vulkan_Buffer;
class Flux_VertexBuffer;
class Flux_IndexBuffer;
class Flux_ConstantBuffer;
class Zenith_Vulkan_Texture;
class Zenith_Vulkan_CommandBuffer;

constexpr uint64_t g_uCpuPoolSize = 2ull * 1024ull * 1024ull * 1024ull;
constexpr uint64_t g_uGpuPoolSize = 2ull * 1024ull * 1024ull * 1024ull;
constexpr uint64_t g_uStagingPoolSize = 1024u * 1024u * 256u;

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
	static void InitialiseVertexBuffer(const void* pData, size_t uSize, Flux_VertexBuffer& xBufferOut);
	static void InitialiseIndexBuffer(const void* pData, size_t uSize, Flux_IndexBuffer& xBufferOut);
	static void InitialiseConstantBuffer(const void* pData, size_t uSize, Flux_ConstantBuffer& xBufferOut);

	static void CreateColourAttachment(uint32_t uWidth, uint32_t uHeight, ColourFormat eFormat, uint32_t uBitsPerPixel, Zenith_Vulkan_Texture& xTextureOut);
	static void CreateDepthStencilAttachment(uint32_t uWidth, uint32_t uHeight, DepthStencilFormat eFormat, uint32_t uBitsPerPixel, Zenith_Vulkan_Texture& xTextureOut);
	static void CreateTexture(const char* szPath, Zenith_Vulkan_Texture& xTextureOut);

	static void AllocateTexture(uint32_t uWidth, uint32_t uHeight, ColourFormat eColourFormat, DepthStencilFormat eDepthStencilFormat, uint32_t uBitsPerPixel, uint32_t uNumMips, vk::ImageUsageFlags eUsageFlags, MemoryResidency eResidency, Zenith_Vulkan_Texture& xTextureOut);
	static void FreeTexture(Zenith_Vulkan_Texture* pxTexture);

	static void UploadStagingData(AllocationType eType, void* pAllocation, const void* pData, size_t uSize);

	static void UploadData(void* pAllocation, const void* pData, size_t uSize);
	static void ClearStagingBuffer();

	static bool MemoryWasAllocated(void* pAllocation);

	static Zenith_Vulkan_CommandBuffer& GetCommandBuffer();
private:

	static void HandleCpuOutOfMemory();
	static void HandleGpuOutOfMemory();
	static void HandleStagingBufferFull();

	static void FlushStagingBuffer();

	static vk::DeviceMemory s_xCPUMemory;
	static vk::DeviceMemory s_xGPUMemory;

	static Zenith_Vulkan_Buffer* s_pxStagingBuffer;
	static Zenith_Vulkan_CommandBuffer s_xCommandBuffer;
	
	struct MemoryAllocation {
		AllocationType m_eType;
		size_t m_uSize;
		size_t m_uOffset;
	};
	struct StagingMemoryAllocation {
		AllocationType m_eType;
		void* m_pAllocation;
		size_t m_uSize;
		size_t m_uOffset;
	};
	static std::list<StagingMemoryAllocation> s_xStagingAllocations;

	static std::unordered_map<void*, MemoryAllocation> s_xCpuAllocationMap;
	static std::unordered_map<void*, MemoryAllocation> s_xGpuAllocationMap;

	static size_t s_uNextFreeCpuOffset;
	static size_t s_uNextFreeGpuOffset;
	static size_t s_uNextFreeStagingOffset;
};

