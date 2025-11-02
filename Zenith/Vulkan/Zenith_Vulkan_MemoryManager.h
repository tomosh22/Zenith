#pragma once

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "vulkan/vulkan.hpp"
#include "vma/vk_mem_alloc.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

#include "Flux/Flux_Enums.h"

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

	static Flux_VRAMHandle CreateColourAttachmentVRAM(const Flux_SurfaceInfo& xInfo);
	static Flux_VRAMHandle CreateDepthStencilAttachmentVRAM(const Flux_SurfaceInfo& xInfo);

	// View creation functions
	static vk::ImageView CreateRenderTargetView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel = 0);
	static vk::ImageView CreateDepthStencilView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel = 0);
	static vk::ImageView CreateShaderResourceView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uBaseMip = 0, uint32_t uMipCount = 1);
	static vk::ImageView CreateUnorderedAccessView(Flux_VRAMHandle xVRAMHandlee, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel = 0);

	static Zenith_Vulkan_CommandBuffer& GetCommandBuffer();
private:
	friend class Zenith_Vulkan_Texture;
	friend class Zenith_AssetHandler;
	
	static Flux_VRAMHandle CreateTextureVRAM(const void* pData, const Flux_SurfaceInfo& xInfo, bool bCreateMips);
	static Flux_VRAMHandle CreateTextureVRAM(const char* szPath, Flux_SurfaceInfo* pxInfoOut = nullptr);
	static Flux_VRAMHandle CreateTextureCubeVRAM(const char* szPathPX, const char* szPathNX, const char* szPathPY, const char* szPathNY, const char* szPathPZ, const char* szPathNZ, Flux_SurfaceInfo* pxInfoOut = nullptr);
	static void CreateColourAttachment(const Flux_SurfaceInfo& xInfo, Zenith_Vulkan_Texture& xTextureOut);
	static void CreateDepthStencilAttachment(const Flux_SurfaceInfo& xInfo, Zenith_Vulkan_Texture& xTextureOut);
	static void CreateTexture(const void* pData, Flux_SurfaceInfo xInfo, bool bCreateMips, Zenith_Vulkan_Texture& xTextureOut);
	static void CreateTexture(const char* szPath, Zenith_Vulkan_Texture& xTextureOut);
	static void CreateTextureCube(const char* szPathPX, const char* szPathNX, const char* szPathPY, const char* szPathNY, const char* szPathPZ, const char* szPathNZ, Zenith_Vulkan_Texture& xTextureOut);

	static void AllocateTexture(const Flux_SurfaceInfo&, vk::ImageUsageFlags eUsageFlags, MemoryResidency eResidency, Zenith_Vulkan_Texture& xTextureOut);
	static void FreeTexture(Zenith_Vulkan_Texture* pxTexture);
	static void InitialiseStagingBuffer();

	static void HandleStagingBufferFull();

	static void FlushStagingBuffer();

	struct StagingMemoryAllocation {
		AllocationType m_eType;
		void* m_pAllocation;
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
