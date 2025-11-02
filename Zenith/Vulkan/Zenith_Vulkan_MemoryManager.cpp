#include "Zenith.h"
#define VMA_IMPLEMENTATION

#include "Zenith_Vulkan_MemoryManager.h"
#include "Zenith_Vulkan_CommandBuffer.h"

#include "Zenith_Vulkan.h"
#include "Zenith_Vulkan_Buffer.h"
#include "Zenith_Vulkan_Texture.h"

#include "Flux/Flux_Buffers.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"

Zenith_Vulkan_CommandBuffer Zenith_Vulkan_MemoryManager::s_xCommandBuffer;
VmaAllocator Zenith_Vulkan_MemoryManager::s_xAllocator;
Zenith_Vulkan_Buffer Zenith_Vulkan_MemoryManager::s_xStagingBuffer;
vk::DeviceMemory Zenith_Vulkan_MemoryManager::s_xStagingMem;
std::list<Zenith_Vulkan_MemoryManager::StagingMemoryAllocation> Zenith_Vulkan_MemoryManager::s_xStagingAllocations;

size_t Zenith_Vulkan_MemoryManager::s_uNextFreeStagingOffset = 0;

Zenith_Mutex Zenith_Vulkan_MemoryManager::s_xMutex;

void Zenith_Vulkan_MemoryManager::InitialiseStagingBuffer()
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	const vk::PhysicalDevice& xPhysicalDevice = Zenith_Vulkan::GetPhysicalDevice();

	vk::BufferCreateInfo xInfo = vk::BufferCreateInfo()
		.setSize(g_uStagingPoolSize)
		.setUsage(vk::BufferUsageFlagBits::eTransferSrc)
		.setSharingMode(vk::SharingMode::eExclusive);

	vk::Buffer xBuffer = xDevice.createBuffer(xInfo);
	s_xStagingBuffer.SetBuffer(xBuffer);

	vk::MemoryRequirements xRequirements = xDevice.getBufferMemoryRequirements(xBuffer);

	uint32_t memoryType = ~0u;
	for (uint32_t i = 0; i < xPhysicalDevice.getMemoryProperties().memoryTypeCount; i++)
	{
		if ((xRequirements.memoryTypeBits & (1 << i)) && (xPhysicalDevice.getMemoryProperties().memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent) == vk::MemoryPropertyFlags(vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent))
		{
			memoryType = i;
			break;
		}
	}
	Zenith_Assert(memoryType != ~0u, "couldn't find physical memory type");

	vk::MemoryAllocateInfo xAllocInfo = vk::MemoryAllocateInfo()
		.setAllocationSize(ALIGN(xRequirements.size, 4096))
		.setMemoryTypeIndex(memoryType);

	s_xStagingMem = xDevice.allocateMemory(xAllocInfo);
	xDevice.bindBufferMemory(xBuffer, s_xStagingMem, 0);
}

void Zenith_Vulkan_MemoryManager::Initialise()
{
	

	VmaAllocatorCreateInfo xCreateInfo = {};
	xCreateInfo.device = Zenith_Vulkan::GetDevice();
	xCreateInfo.physicalDevice = Zenith_Vulkan::GetPhysicalDevice();
	xCreateInfo.instance = Zenith_Vulkan::GetInstance();
#ifdef VK_VERSION_1_3
	xCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
#else
#error check vulkan version
#endif

	vmaCreateAllocator(&xCreateInfo, &s_xAllocator);

	s_xCommandBuffer.Initialise(COMMANDTYPE_COPY);

	InitialiseStagingBuffer();

	Zenith_Log("Vulkan memory manager initialised");
}

void Zenith_Vulkan_MemoryManager::Shutdown()
{
	vmaDestroyAllocator(s_xAllocator);
}

Zenith_Vulkan_CommandBuffer& Zenith_Vulkan_MemoryManager::GetCommandBuffer() {
	return s_xCommandBuffer;
}

void Zenith_Vulkan_MemoryManager::BeginFrame()
{
	s_xCommandBuffer.BeginRecording();

	//#TO_TODO: asset handler
	//AssetHandler* pxAssetHandler = pxApp->m_pxAssetHandler;
	//pxAssetHandler->ProcessPendingDeletes();
}

void Zenith_Vulkan_MemoryManager::EndFrame(bool bDefer /*= true*/)
{
	FlushStagingBuffer();

	if (bDefer)
	{
		s_xCommandBuffer.GetCurrentCmdBuffer().end();
		Zenith_Vulkan::s_pxMemoryUpdateCmdBuf = &s_xCommandBuffer;
	}
	else
	{
		s_xCommandBuffer.EndAndCpuWait(false);
	}
}

void Zenith_Vulkan_MemoryManager::ImageTransitionBarrier(vk::Image xImage, vk::ImageLayout eOldLayout, vk::ImageLayout eNewLayout, vk::ImageAspectFlags eAspect, vk::PipelineStageFlags eSrcStage, vk::PipelineStageFlags eDstStage, uint32_t uMipLevel, uint32_t uLayer)
{
	vk::ImageSubresourceRange xSubRange = vk::ImageSubresourceRange(eAspect, uMipLevel, 1, uLayer, 1);

	vk::ImageMemoryBarrier xMemoryBarrier = vk::ImageMemoryBarrier()
		.setSubresourceRange(xSubRange)
		.setImage(xImage)
		.setOldLayout(eOldLayout)
		.setNewLayout(eNewLayout);

	switch (eNewLayout)
	{
	case (vk::ImageLayout::eTransferDstOptimal):
		xMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
		break;
	case (vk::ImageLayout::eTransferSrcOptimal):
		xMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eTransferRead);
		break;
	case (vk::ImageLayout::eColorAttachmentOptimal):
		xMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
		break;
	case (vk::ImageLayout::eDepthAttachmentOptimal):
		xMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);
		break;
	case (vk::ImageLayout::eDepthStencilAttachmentOptimal):
		xMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);
		break;
	case (vk::ImageLayout::eShaderReadOnlyOptimal):
		xMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
		break;
	default:
		break;
	}

	const vk::CommandBuffer& xCmd = s_xCommandBuffer.GetCurrentCmdBuffer();

	xCmd.pipelineBarrier(eSrcStage, eDstStage, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &xMemoryBarrier);
}

void Zenith_Vulkan_MemoryManager::AllocateBuffer(size_t uSize, vk::BufferUsageFlags eUsageFlags, MemoryResidency eResidency, Zenith_Vulkan_Buffer& xBufferOut)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	vk::BufferCreateInfo xBufferInfo = vk::BufferCreateInfo()
		.setSize(uSize)
		.setUsage(eUsageFlags)
		.setSharingMode(vk::SharingMode::eExclusive);
	xBufferOut.SetSize(uSize);

	VmaAllocationCreateInfo xAllocInfo = {};
	if (eResidency == MEMORY_RESIDENCY_CPU)
	{
		xAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		xAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
		xAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	}
	else if (eResidency == MEMORY_RESIDENCY_GPU)
	{
		xAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	}

	const vk::BufferCreateInfo::NativeType xBufferInfo_Native = xBufferInfo;

	vmaCreateBuffer(s_xAllocator, &xBufferInfo_Native, &xAllocInfo, xBufferOut.GetBuffer_Ptr(), xBufferOut.GetAllocation_Ptr(), nullptr);

	Zenith_Assert(xBufferOut.GetBuffer() != VK_NULL_HANDLE, "Buffer allocation failed");
}

void Zenith_Vulkan_MemoryManager::FreeBuffer(Zenith_Vulkan_Buffer* pxBuffer)
{
	//#TO this happens as Reset is called twice on vertex and index buffers
	//	  during destruction of Flux_MeshGeometry
	if (!pxBuffer->IsValid())
	{
		return;
	}

	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	vmaDestroyBuffer(s_xAllocator, pxBuffer->GetBuffer(), pxBuffer->GetAllocation());

	pxBuffer->SetBuffer(VK_NULL_HANDLE);
}

void Zenith_Vulkan_MemoryManager::InitialiseVertexBuffer(const void* pData, size_t uSize, Flux_VertexBuffer& xBufferOut, bool bDeviceLocal /*= true*/)
{
	Zenith_Vulkan_Buffer& xBuffer = xBufferOut.GetBuffer();
	vk::BufferUsageFlags eFlags = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
	AllocateBuffer(uSize, eFlags, bDeviceLocal ? MEMORY_RESIDENCY_GPU : MEMORY_RESIDENCY_CPU, xBuffer);
	if (pData)
	{
		UploadBufferData(xBuffer, pData, uSize);
	}
}

void Zenith_Vulkan_MemoryManager::InitialiseDynamicVertexBuffer(const void* pData, size_t uSize, Flux_DynamicVertexBuffer& xBufferOut, bool bDeviceLocal /*= true*/)
{
	for (uint32_t u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		Zenith_Vulkan_Buffer& xBuffer = xBufferOut.GetBufferForFrameInFlight(u);
		vk::BufferUsageFlags eFlags = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
		AllocateBuffer(uSize, eFlags, bDeviceLocal ? MEMORY_RESIDENCY_GPU : MEMORY_RESIDENCY_CPU, xBuffer);
		if (pData)
		{
			UploadBufferData(xBuffer, pData, uSize);
		}
	}
}

void Zenith_Vulkan_MemoryManager::InitialiseIndexBuffer(const void* pData, size_t uSize, Flux_IndexBuffer& xBufferOut)
{
	Zenith_Vulkan_Buffer& xBuffer = xBufferOut.GetBuffer();
	vk::BufferUsageFlags eFlags = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst;
	AllocateBuffer(uSize, eFlags, MEMORY_RESIDENCY_GPU, xBuffer);
	if (pData)
	{
		UploadBufferData(xBuffer, pData, uSize);
	}
}

void Zenith_Vulkan_MemoryManager::InitialiseDynamicConstantBuffer(const void* pData, size_t uSize, Flux_DynamicConstantBuffer& xBufferOut)
{
	for (uint32_t u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		Zenith_Vulkan_Buffer& xBuffer = xBufferOut.GetBufferForFrameInFlight(u);
		vk::BufferUsageFlags eFlags = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst;
		AllocateBuffer(uSize, eFlags, MEMORY_RESIDENCY_CPU, xBuffer);
		if (pData)
		{
			UploadBufferData(xBuffer, pData, uSize);
		}
	}
}

void Zenith_Vulkan_MemoryManager::CreateColourAttachment(const Flux_SurfaceInfo& xInfo, Zenith_Vulkan_Texture& xTextureOut)
{
	FreeTexture(&xTextureOut);
	AllocateTexture(xInfo, vk::ImageUsageFlagBits::eColorAttachment, MEMORY_RESIDENCY_GPU, xTextureOut);
	ImageTransitionBarrier(xTextureOut.GetImage(), vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands);
}

uint32_t Zenith_Vulkan_MemoryManager::CreateColourAttachmentVRAM(const Flux_SurfaceInfo& xInfo)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	
	vk::Format xFormat = Zenith_Vulkan_Texture::ConvertToVkFormat_Colour(xInfo.m_eFormat);
	
	vk::ImageUsageFlags eUsageFlags = vk::ImageUsageFlagBits::eColorAttachment;
	if (xInfo.m_uMemoryFlags & 1 << MEMORY_FLAGS__SHADER_READ) eUsageFlags |= vk::ImageUsageFlagBits::eSampled;
	if (xInfo.m_uMemoryFlags & 1 << MEMORY_FLAGS__UNORDERED_ACCESS) eUsageFlags |= vk::ImageUsageFlagBits::eStorage;

	vk::ImageCreateInfo xImageInfo = vk::ImageCreateInfo()
		.setImageType(vk::ImageType::e2D)
		.setFormat(xFormat)
		.setTiling(vk::ImageTiling::eOptimal)
		.setExtent({ xInfo.m_uWidth, xInfo.m_uHeight, 1 })
		.setMipLevels(xInfo.m_uNumMips)
		.setArrayLayers(xInfo.m_uNumLayers)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setUsage(eUsageFlags)
		.setSharingMode(vk::SharingMode::eExclusive)
		.setSamples(vk::SampleCountFlagBits::e1);

	VmaAllocationCreateInfo xAllocInfo = {};
	xAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	const vk::ImageCreateInfo::NativeType xImageInfo_Native = xImageInfo;
	
	VkImage xImage;
	VmaAllocation xAllocation;
	vmaCreateImage(s_xAllocator, &xImageInfo_Native, &xAllocInfo, &xImage, &xAllocation, nullptr);

	Zenith_Vulkan_VRAM* pxVRAM = new Zenith_Vulkan_VRAM(vk::Image(xImage), xAllocation, s_xAllocator);
	uint32_t uHandle = Zenith_Vulkan::RegisterVRAM(pxVRAM);

	ImageTransitionBarrier(vk::Image(xImage), vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands);

	return uHandle;
}

void Zenith_Vulkan_MemoryManager::CreateDepthStencilAttachment(const Flux_SurfaceInfo& xInfo, Zenith_Vulkan_Texture& xTextureOut)
{
	FreeTexture(&xTextureOut);
	AllocateTexture(xInfo, vk::ImageUsageFlagBits::eDepthStencilAttachment, MEMORY_RESIDENCY_GPU, xTextureOut);
	Zenith_Assert(xInfo.m_eFormat == TEXTURE_FORMAT_D32_SFLOAT, "#TO_TODO: layouts for just depth without stencil");
	ImageTransitionBarrier(xTextureOut.GetImage(), vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilReadOnlyOptimal, vk::ImageAspectFlagBits::eDepth, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands);
}

uint32_t Zenith_Vulkan_MemoryManager::CreateDepthStencilAttachmentVRAM(const Flux_SurfaceInfo& xInfo)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	
	vk::Format xFormat = Zenith_Vulkan_Texture::ConvertToVkFormat_DepthStencil(xInfo.m_eFormat);
	
	vk::ImageUsageFlags eUsageFlags = vk::ImageUsageFlagBits::eDepthStencilAttachment;
	if (xInfo.m_uMemoryFlags & 1 << MEMORY_FLAGS__SHADER_READ) eUsageFlags |= vk::ImageUsageFlagBits::eSampled;

	vk::ImageCreateInfo xImageInfo = vk::ImageCreateInfo()
		.setImageType(vk::ImageType::e2D)
		.setFormat(xFormat)
		.setTiling(vk::ImageTiling::eOptimal)
		.setExtent({ xInfo.m_uWidth, xInfo.m_uHeight, 1 })
		.setMipLevels(xInfo.m_uNumMips)
		.setArrayLayers(xInfo.m_uNumLayers)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setUsage(eUsageFlags)
		.setSharingMode(vk::SharingMode::eExclusive)
		.setSamples(vk::SampleCountFlagBits::e1);

	VmaAllocationCreateInfo xAllocInfo = {};
	xAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	const vk::ImageCreateInfo::NativeType xImageInfo_Native = xImageInfo;
	
	VkImage xImage;
	VmaAllocation xAllocation;
	vmaCreateImage(s_xAllocator, &xImageInfo_Native, &xAllocInfo, &xImage, &xAllocation, nullptr);

	Zenith_Vulkan_VRAM* pxVRAM = new Zenith_Vulkan_VRAM(vk::Image(xImage), xAllocation, s_xAllocator);
	uint32_t uHandle = Zenith_Vulkan::RegisterVRAM(pxVRAM);

	Zenith_Assert(xInfo.m_eFormat == TEXTURE_FORMAT_D32_SFLOAT, "#TO_TODO: layouts for just depth without stencil");
	ImageTransitionBarrier(vk::Image(xImage), vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilReadOnlyOptimal, vk::ImageAspectFlagBits::eDepth, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands);

	return uHandle;
}

uint32_t Zenith_Vulkan_MemoryManager::CreateTextureVRAM(const void* pData, const Flux_SurfaceInfo& xInfo, bool bCreateMips)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	
	Flux_SurfaceInfo xInfoCopy = xInfo;
	xInfoCopy.m_uNumMips = bCreateMips ? std::floor(std::log2(std::max(xInfo.m_uWidth, xInfo.m_uHeight))) + 1 : 1;
	
	vk::Format xFormat = Zenith_Vulkan_Texture::ConvertToVkFormat_Colour(xInfoCopy.m_eFormat);
	
	vk::ImageUsageFlags eUsageFlags = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc;
	if (xInfoCopy.m_uMemoryFlags & 1 << MEMORY_FLAGS__SHADER_READ) eUsageFlags |= vk::ImageUsageFlagBits::eSampled;
	if (xInfoCopy.m_uMemoryFlags & 1 << MEMORY_FLAGS__UNORDERED_ACCESS) eUsageFlags |= vk::ImageUsageFlagBits::eStorage;

	vk::ImageCreateInfo xImageInfo = vk::ImageCreateInfo()
		.setImageType(vk::ImageType::e2D)
		.setFormat(xFormat)
		.setTiling(vk::ImageTiling::eOptimal)
		.setExtent({ xInfoCopy.m_uWidth, xInfoCopy.m_uHeight, 1 })
		.setMipLevels(xInfoCopy.m_uNumMips)
		.setArrayLayers(xInfoCopy.m_uNumLayers)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setUsage(eUsageFlags)
		.setSharingMode(vk::SharingMode::eExclusive)
		.setSamples(vk::SampleCountFlagBits::e1);

	VmaAllocationCreateInfo xAllocInfo = {};
	xAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	const vk::ImageCreateInfo::NativeType xImageInfo_Native = xImageInfo;
	
	VkImage xImage;
	VmaAllocation xAllocation;
	vmaCreateImage(s_xAllocator, &xImageInfo_Native, &xAllocInfo, &xImage, &xAllocation, nullptr);

	Zenith_Vulkan_VRAM* pxVRAM = new Zenith_Vulkan_VRAM(vk::Image(xImage), xAllocation, s_xAllocator);
	uint32_t uHandle = Zenith_Vulkan::RegisterVRAM(pxVRAM);
	
	// Create default SRV for shader access
	vk::ImageView xDefaultSRV = CreateShaderResourceView(uHandle, xInfoCopy);
	pxVRAM->SetDefaultSRV(xDefaultSRV);

	if (pData)
	{
		// Upload using legacy texture temporarily
		Zenith_Vulkan_Texture xTempTexture;
		xTempTexture.SetImage(vk::Image(xImage));
		xTempTexture.SetWidth(xInfoCopy.m_uWidth);
		xTempTexture.SetHeight(xInfoCopy.m_uHeight);
		xTempTexture.SetNumMips(xInfoCopy.m_uNumMips);
		xTempTexture.SetNumLayers(1);
		// Set the actual allocation so upload can work
		xTempTexture.SetAllocation(xAllocation);
		
		UploadTextureData(xTempTexture, pData, ColourFormatBytesPerPixel(xInfoCopy.m_eFormat) * xInfoCopy.m_uWidth * xInfoCopy.m_uHeight * xInfoCopy.m_uDepth);
		
		// Flush staging buffer immediately so temp texture doesn't go out of scope
		// before the copy commands are recorded
		EndFrame(false);
		BeginFrame();
		
		// Clear allocation and image so FreeTexture won't destroy them (VRAM owns them now)
		xTempTexture.SetAllocation(VmaAllocation{});
		xTempTexture.SetImage(VK_NULL_HANDLE);
	}

	ImageTransitionBarrier(vk::Image(xImage), vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands);

	return uHandle;
}

uint32_t Zenith_Vulkan_MemoryManager::CreateTextureVRAM(const char* szPath)
{
	size_t ulDataSize;
	int32_t uWidth = 0, uHeight = 0, uDepth = 0;
	TextureFormat eFormat;

	Zenith_DataStream xStream;
	xStream.ReadFromFile(szPath);

	xStream >> uWidth;
	xStream >> uHeight;
	xStream >> uDepth;
	xStream >> eFormat;
	xStream >> ulDataSize;

	void* const pData = Zenith_MemoryManagement::Allocate(ulDataSize);
	xStream.ReadData(pData, ulDataSize);

	const uint32_t uNumMips = std::floor(std::log2(std::max(uWidth, uHeight))) + 1;

	Flux_SurfaceInfo xInfo;
	xInfo.m_uWidth = uWidth;
	xInfo.m_uHeight = uHeight;
	xInfo.m_uDepth = uDepth;
	xInfo.m_uNumLayers = 1;
	xInfo.m_eFormat = TEXTURE_FORMAT_RGBA8_UNORM;
	xInfo.m_uNumMips = uNumMips;
	xInfo.m_uMemoryFlags = 1 << MEMORY_FLAGS__SHADER_READ;

	uint32_t uHandle = CreateTextureVRAM(pData, xInfo, true);
	
	Zenith_MemoryManagement::Deallocate(pData);
	
	return uHandle;
}

uint32_t Zenith_Vulkan_MemoryManager::CreateTextureCubeVRAM(const char* szPathPX, const char* szPathNX, const char* szPathPY, const char* szPathNY, const char* szPathPZ, const char* szPathNZ)
{
	const char* aszPaths[6] =
	{
		szPathPX,
		szPathNX,
		szPathPY,
		szPathNY,
		szPathPZ,
		szPathNZ,
	};

	void* apDatas[6] =
	{
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
	};

	size_t aulDataSizes[6] = { 0, 0, 0, 0, 0, 0 };
	uint32_t uWidth = 0, uHeight = 0, uDepth = 0;

	for (uint32_t u = 0; u < 6; u++)
	{
		Zenith_DataStream xStream;
		xStream.ReadFromFile(aszPaths[u]);

		TextureFormat eFormat;

		xStream >> uWidth;
		xStream >> uHeight;
		xStream >> uDepth;
		xStream >> eFormat;
		xStream >> aulDataSizes[u];

		apDatas[u] = Zenith_MemoryManagement::Allocate(aulDataSizes[u]);
		xStream.ReadData(apDatas[u], aulDataSizes[u]);
	}

	const uint32_t uNumMips = std::floor(std::log2(std::max(uWidth, uHeight))) + 1;

	Flux_SurfaceInfo xInfo;
	xInfo.m_uWidth = uWidth;
	xInfo.m_uHeight = uHeight;
	xInfo.m_uNumLayers = 6;
	xInfo.m_eFormat = TEXTURE_FORMAT_RGBA8_UNORM;
	xInfo.m_uNumMips = uNumMips;
	xInfo.m_uMemoryFlags = 1 << MEMORY_FLAGS__SHADER_READ;

	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	
	vk::Format xFormat = Zenith_Vulkan_Texture::ConvertToVkFormat_Colour(xInfo.m_eFormat);
	
	vk::ImageUsageFlags eUsageFlags = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled;

	vk::ImageCreateInfo xImageInfo = vk::ImageCreateInfo()
		.setImageType(vk::ImageType::e2D)
		.setFormat(xFormat)
		.setTiling(vk::ImageTiling::eOptimal)
		.setExtent({ xInfo.m_uWidth, xInfo.m_uHeight, 1 })
		.setMipLevels(xInfo.m_uNumMips)
		.setArrayLayers(6)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setUsage(eUsageFlags)
		.setSharingMode(vk::SharingMode::eExclusive)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setFlags(vk::ImageCreateFlagBits::eCubeCompatible);

	VmaAllocationCreateInfo xAllocInfo = {};
	xAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	const vk::ImageCreateInfo::NativeType xImageInfo_Native = xImageInfo;
	
	VkImage xImage;
	VmaAllocation xAllocation;
	vmaCreateImage(s_xAllocator, &xImageInfo_Native, &xAllocInfo, &xImage, &xAllocation, nullptr);

	Zenith_Vulkan_VRAM* pxVRAM = new Zenith_Vulkan_VRAM(vk::Image(xImage), xAllocation, s_xAllocator);
	uint32_t uHandle = Zenith_Vulkan::RegisterVRAM(pxVRAM);
	
	// Create default SRV for shader access
	vk::ImageView xDefaultSRV = CreateShaderResourceView(uHandle, xInfo);
	pxVRAM->SetDefaultSRV(xDefaultSRV);

	// Upload using legacy texture temporarily - concatenate all layer data
	size_t ulTotalDataSize = 0;
	for (uint32_t u = 0; u < 6; u++)
	{
		ulTotalDataSize += aulDataSizes[u];
	}

	void* pAllData = Zenith_MemoryManagement::Allocate(ulTotalDataSize);
	size_t ulCursor = 0;
	for (uint32_t u = 0; u < 6; u++)
	{
		memcpy((uint8_t*)pAllData + ulCursor, apDatas[u], aulDataSizes[u]);
		ulCursor += aulDataSizes[u];
		Zenith_MemoryManagement::Deallocate(apDatas[u]);
	}

	Zenith_Vulkan_Texture xTempTexture;
	xTempTexture.SetImage(vk::Image(xImage));
	xTempTexture.SetWidth(xInfo.m_uWidth);
	xTempTexture.SetHeight(xInfo.m_uHeight);
	xTempTexture.SetNumMips(xInfo.m_uNumMips);
	xTempTexture.SetNumLayers(6);
	// Set the actual allocation so upload can work
	xTempTexture.SetAllocation(xAllocation);

	UploadTextureData(xTempTexture, pAllData, ulTotalDataSize);

	Zenith_MemoryManagement::Deallocate(pAllData);

	// Flush staging buffer immediately so temp texture doesn't go out of scope
	// before the copy commands are recorded
	EndFrame(false);
	BeginFrame();

	// Clear allocation and image so FreeTexture won't destroy them (VRAM owns them now)
	xTempTexture.SetAllocation(VmaAllocation{});
	xTempTexture.SetImage(VK_NULL_HANDLE);

	ImageTransitionBarrier(vk::Image(xImage), vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands);

	return uHandle;
}


void Zenith_Vulkan_MemoryManager::CreateTexture(const void* pData, Flux_SurfaceInfo xInfo, bool bCreateMips, Zenith_Vulkan_Texture& xTextureOut)
{
	FreeTexture(&xTextureOut);
	
	//#TO_TODO: this should be done by from calling code, make xInfo const ref
	xInfo.m_uNumMips = bCreateMips ? std::floor(std::log2(std::max(xInfo.m_uWidth, xInfo.m_uHeight))) + 1 : 1;

	//#TO_TODO: other formats
	AllocateTexture(xInfo, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc, MEMORY_RESIDENCY_GPU, xTextureOut);
	xTextureOut.SetWidth(xInfo.m_uWidth);
	xTextureOut.SetHeight(xInfo.m_uHeight);
	xTextureOut.SetNumMips(xInfo.m_uNumMips);
	xTextureOut.SetNumLayers(1);
	if (pData)
	{
		UploadTextureData(xTextureOut, pData, ColourFormatBytesPerPixel(xInfo.m_eFormat) * xInfo.m_uWidth * xInfo.m_uHeight * xInfo.m_uDepth);
	}
}

void Zenith_Vulkan_MemoryManager::CreateTexture(const char* szPath, Zenith_Vulkan_Texture& xTextureOut)
{
	FreeTexture(&xTextureOut);

	size_t ulDataSize;
	int32_t uWidth = 0, uHeight = 0, uDepth = 0;
	TextureFormat eFormat;

	Zenith_DataStream xStream;
	xStream.ReadFromFile(szPath);

	xStream >> uWidth;
	xStream >> uHeight;
	xStream >> uDepth;
	xStream >> eFormat;
	xStream >> ulDataSize;

	void* const pData = Zenith_MemoryManagement::Allocate(ulDataSize);
	xStream.ReadData(pData, ulDataSize);



	const uint32_t uNumMips = std::floor(std::log2(std::max(uWidth, uHeight))) + 1;

	Flux_SurfaceInfo xInfo;
	xInfo.m_uWidth = uWidth;
	xInfo.m_uHeight = uHeight;
	xInfo.m_uNumLayers = 1;
	xInfo.m_eFormat = TEXTURE_FORMAT_RGBA8_UNORM;
	xInfo.m_uNumMips = uNumMips;

	//#TO_TODO: other formats
	AllocateTexture(xInfo, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc, MEMORY_RESIDENCY_GPU, xTextureOut);
	xTextureOut.SetWidth(uWidth);
	xTextureOut.SetHeight(uHeight);
	xTextureOut.SetNumMips(uNumMips);
	xTextureOut.SetNumLayers(1);
	UploadTextureData(xTextureOut, pData, ulDataSize);
	Zenith_MemoryManagement::Deallocate(pData);
}

void Zenith_Vulkan_MemoryManager::CreateTextureCube(const char* szPathPX, const char* szPathNX, const char* szPathPY, const char* szPathNY, const char* szPathPZ, const char* szPathNZ, Zenith_Vulkan_Texture& xTextureOut)
{
	FreeTexture(&xTextureOut);

	const char* aszPaths[6] =
	{
		szPathPX,
		szPathNX,
		szPathPY,
		szPathNY,
		szPathPZ,
		szPathNZ,
	};

	void* apDatas[6] =
	{
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
	};

	size_t aulDataSizes[6] =
	{
		0,0,0,0,0,0
	};

	uint32_t uWidth = 0, uHeight = 0, uDepth = 0, uNumMips = 0;
	size_t ulTotalDataSize = 0;

	for (uint32_t u = 0; u < 6; u++)
	{
		const char* szPath = aszPaths[u];
		void*& pData = apDatas[u];
		size_t ulDataSize;
		TextureFormat eFormat;

		Zenith_DataStream xStream;
		xStream.ReadFromFile(szPath);

		xStream >> uWidth;
		xStream >> uHeight;
		xStream >> uDepth;
		xStream >> eFormat;
		xStream >> ulDataSize;

		pData = Zenith_MemoryManagement::Allocate(ulDataSize);
		xStream.ReadData(pData, ulDataSize);
		//eFormat = StringToVkFormat(strFormat);

		aulDataSizes[u] = ulDataSize;

		ulTotalDataSize += ulDataSize;

		uNumMips = std::floor(std::log2(std::max(uWidth, uHeight))) + 1;
	}

	Flux_SurfaceInfo xInfo;
	xInfo.m_uWidth = uWidth;
	xInfo.m_uHeight = uHeight;
	xInfo.m_uNumLayers = 6;
	xInfo.m_eFormat = TEXTURE_FORMAT_RGBA8_UNORM;
	xInfo.m_uNumMips = uNumMips;

	//#TO_TODO: other formats
	AllocateTexture(xInfo, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc, MEMORY_RESIDENCY_GPU, xTextureOut);
	xTextureOut.SetWidth(uWidth);
	xTextureOut.SetHeight(uHeight);
	xTextureOut.SetNumMips(uNumMips);
	xTextureOut.SetNumLayers(6);

	void* pAllData = Zenith_MemoryManagement::Allocate(ulTotalDataSize);
	size_t ulCursor = 0;
	for (uint32_t u = 0; u < 6; u++)
	{
		memcpy((uint8_t*)pAllData + ulCursor, apDatas[u], aulDataSizes[u]);
		ulCursor += aulDataSizes[u];
	}

	UploadTextureData(xTextureOut, pAllData, ulTotalDataSize);

	for (uint32_t u = 0; u < 6; u++)
	{
		Zenith_MemoryManagement::Deallocate(apDatas[u]);
	}
	Zenith_MemoryManagement::Deallocate(pAllData);
}

void Zenith_Vulkan_MemoryManager::AllocateTexture(const Flux_SurfaceInfo& xInfo, vk::ImageUsageFlags eUsageFlags, MemoryResidency eResidency, Zenith_Vulkan_Texture& xTextureOut)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	
	const bool bIsDepth = xInfo.m_eFormat > TEXTURE_FORMAT_DEPTH_STENCIL_BEGIN && xInfo.m_eFormat < TEXTURE_FORMAT_DEPTH_STENCIL_END;

	const vk::Format xFormat = bIsDepth ? Zenith_Vulkan_Texture::ConvertToVkFormat_DepthStencil(xInfo.m_eFormat) : Zenith_Vulkan_Texture::ConvertToVkFormat_Colour(xInfo.m_eFormat);

	if (xInfo.m_uMemoryFlags & 1 << MEMORY_FLAGS__SHADER_READ) eUsageFlags |= vk::ImageUsageFlagBits::eSampled;
	if (xInfo.m_uMemoryFlags & 1 << MEMORY_FLAGS__UNORDERED_ACCESS) eUsageFlags |= vk::ImageUsageFlagBits::eStorage;

	vk::ImageCreateInfo xImageInfo = vk::ImageCreateInfo()
		.setImageType(vk::ImageType::e2D)
		.setFormat(xFormat)
		.setTiling(vk::ImageTiling::eOptimal)
		.setExtent({ xInfo.m_uWidth,xInfo.m_uHeight,1 })
		.setMipLevels(xInfo.m_uNumMips)
		.setArrayLayers(xInfo.m_uNumLayers)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setUsage(eUsageFlags)
		.setSharingMode(vk::SharingMode::eExclusive)
		.setSamples(vk::SampleCountFlagBits::e1);
	if (xInfo.m_uNumLayers == 6)
	{
		xImageInfo = xImageInfo.setFlags(vk::ImageCreateFlagBits::eCubeCompatible);
	}

	VmaAllocationCreateInfo xAllocInfo = {};
	if (eResidency == MEMORY_RESIDENCY_CPU)
	{
		xAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	}
	else if (eResidency == MEMORY_RESIDENCY_GPU)
	{
		xAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	}

	const vk::ImageCreateInfo::NativeType xImageInfo_Native = xImageInfo;

	vmaCreateImage(s_xAllocator, &xImageInfo_Native, &xAllocInfo, xTextureOut.GetImage_Ptr(), xTextureOut.GetAllocation_Ptr(), nullptr);

	const bool bIsCube = xInfo.m_uNumLayers == 6;
	const uint32_t uLayerCount = bIsCube ? 6 : (xInfo.m_uNumLayers > 0 ? xInfo.m_uNumLayers : 1);
	vk::ImageSubresourceRange xSubresourceRange = vk::ImageSubresourceRange()
		.setAspectMask(bIsDepth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor)
		.setBaseMipLevel(0)
		.setLevelCount(xInfo.m_uNumMips)
		.setBaseArrayLayer(0)
		.setLayerCount(uLayerCount);

	const vk::Image& xImage = xTextureOut.GetImage();

	vk::ImageViewCreateInfo xViewCreate = vk::ImageViewCreateInfo()
		.setImage(xImage)
		.setViewType(bIsCube ? vk::ImageViewType::eCube : vk::ImageViewType::e2D)
		.setFormat(xFormat)
		.setSubresourceRange(xSubresourceRange);

	xTextureOut.SetImageView(xDevice.createImageView(xViewCreate));
	xTextureOut.SetFormat(xFormat);
}

void Zenith_Vulkan_MemoryManager::FreeTexture(Zenith_Vulkan_Texture* pxTexture)
{
	if (!pxTexture->IsValid())
	{
		return;
	}

	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	xDevice.destroyImageView(pxTexture->GetImageView());

	vmaDestroyImage(s_xAllocator, pxTexture->GetImage(), pxTexture->GetAllocation());

	pxTexture->SetImage(VK_NULL_HANDLE);
}

vk::ImageView Zenith_Vulkan_MemoryManager::CreateRenderTargetView(uint32_t uVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(uVRAMHandle);
	
	vk::Format xFormat = Zenith_Vulkan_Texture::ConvertToVkFormat_Colour(xInfo.m_eFormat);
	
	const bool bIsCube = xInfo.m_uNumLayers == 6;
	const uint32_t uLayerCount = bIsCube ? 6 : (xInfo.m_uNumLayers > 0 ? xInfo.m_uNumLayers : 1);
	vk::ImageSubresourceRange xSubresourceRange = vk::ImageSubresourceRange()
		.setAspectMask(vk::ImageAspectFlagBits::eColor)
		.setBaseMipLevel(uMipLevel)
		.setLevelCount(1)
		.setBaseArrayLayer(0)
		.setLayerCount(uLayerCount);

	vk::ImageViewCreateInfo xViewCreate = vk::ImageViewCreateInfo()
		.setImage(pxVRAM->GetImage())
		.setViewType(bIsCube ? vk::ImageViewType::eCube : vk::ImageViewType::e2D)
		.setFormat(xFormat)
		.setSubresourceRange(xSubresourceRange);

	return xDevice.createImageView(xViewCreate);
}

vk::ImageView Zenith_Vulkan_MemoryManager::CreateDepthStencilView(uint32_t uVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(uVRAMHandle);
	
	vk::Format xFormat = Zenith_Vulkan_Texture::ConvertToVkFormat_DepthStencil(xInfo.m_eFormat);
	
	const bool bIsCube = xInfo.m_uNumLayers == 6;
	const uint32_t uLayerCount = bIsCube ? 6 : (xInfo.m_uNumLayers > 0 ? xInfo.m_uNumLayers : 1);
	vk::ImageSubresourceRange xSubresourceRange = vk::ImageSubresourceRange()
		.setAspectMask(vk::ImageAspectFlagBits::eDepth)
		.setBaseMipLevel(uMipLevel)
		.setLevelCount(1)
		.setBaseArrayLayer(0)
		.setLayerCount(uLayerCount);

	vk::ImageViewCreateInfo xViewCreate = vk::ImageViewCreateInfo()
		.setImage(pxVRAM->GetImage())
		.setViewType(bIsCube ? vk::ImageViewType::eCube : vk::ImageViewType::e2D)
		.setFormat(xFormat)
		.setSubresourceRange(xSubresourceRange);

	return xDevice.createImageView(xViewCreate);
}

vk::ImageView Zenith_Vulkan_MemoryManager::CreateShaderResourceView(uint32_t uVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uBaseMip, uint32_t uMipCount)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(uVRAMHandle);
	
	const bool bIsDepth = xInfo.m_eFormat > TEXTURE_FORMAT_DEPTH_STENCIL_BEGIN && xInfo.m_eFormat < TEXTURE_FORMAT_DEPTH_STENCIL_END;
	vk::Format xFormat = bIsDepth ? Zenith_Vulkan_Texture::ConvertToVkFormat_DepthStencil(xInfo.m_eFormat) : Zenith_Vulkan_Texture::ConvertToVkFormat_Colour(xInfo.m_eFormat);
	
	const bool bIsCube = xInfo.m_uNumLayers == 6;
	const uint32_t uLayerCount = bIsCube ? 6 : (xInfo.m_uNumLayers > 0 ? xInfo.m_uNumLayers : 1);
	vk::ImageSubresourceRange xSubresourceRange = vk::ImageSubresourceRange()
		.setAspectMask(bIsDepth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor)
		.setBaseMipLevel(uBaseMip)
		.setLevelCount(uMipCount)
		.setBaseArrayLayer(0)
		.setLayerCount(uLayerCount);

	vk::ImageViewCreateInfo xViewCreate = vk::ImageViewCreateInfo()
		.setImage(pxVRAM->GetImage())
		.setViewType(bIsCube ? vk::ImageViewType::eCube : vk::ImageViewType::e2D)
		.setFormat(xFormat)
		.setSubresourceRange(xSubresourceRange);

	return xDevice.createImageView(xViewCreate);
}

vk::ImageView Zenith_Vulkan_MemoryManager::CreateUnorderedAccessView(uint32_t uVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(uVRAMHandle);
	
	vk::Format xFormat = Zenith_Vulkan_Texture::ConvertToVkFormat_Colour(xInfo.m_eFormat);
	
	const bool bIsCube = xInfo.m_uNumLayers == 6;
	const uint32_t uLayerCount = bIsCube ? 6 : (xInfo.m_uNumLayers > 0 ? xInfo.m_uNumLayers : 1);
	vk::ImageSubresourceRange xSubresourceRange = vk::ImageSubresourceRange()
		.setAspectMask(vk::ImageAspectFlagBits::eColor)
		.setBaseMipLevel(uMipLevel)
		.setLevelCount(1)
		.setBaseArrayLayer(0)
		.setLayerCount(uLayerCount);

	vk::ImageViewCreateInfo xViewCreate = vk::ImageViewCreateInfo()
		.setImage(pxVRAM->GetImage())
		.setViewType(bIsCube ? vk::ImageViewType::eCube : vk::ImageViewType::e2D)
		.setFormat(xFormat)
		.setSubresourceRange(xSubresourceRange);

	return xDevice.createImageView(xViewCreate);
}


void Zenith_Vulkan_MemoryManager::UploadBufferData(Zenith_Vulkan_Buffer& xBuffer, const void* pData, size_t uSize)
{
	s_xMutex.Lock();
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	const VmaAllocation& xAlloc = xBuffer.GetAllocation();
	VkMemoryPropertyFlags eMemoryProps;
	vmaGetAllocationMemoryProperties(s_xAllocator, xAlloc, &eMemoryProps);

	if (eMemoryProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		void* pMap = nullptr;
		vmaMapMemory(s_xAllocator, xAlloc, &pMap);
		Zenith_Assert(pMap != nullptr, "Memory isn't mapped");
		memcpy(pMap, pData, uSize);
#ifdef ZENITH_ASSERT
		VkResult eResult = 
#endif
		vmaFlushAllocation(s_xAllocator, xAlloc, 0, uSize);
		Zenith_Assert(eResult == VK_SUCCESS, "Failed to flush allocation");

		vmaUnmapMemory(s_xAllocator, xAlloc);
	}
	else
	{
		if (s_uNextFreeStagingOffset + uSize >= g_uStagingPoolSize)
		{
			HandleStagingBufferFull();
		}

		StagingMemoryAllocation xAllocation = { ALLOCATION_TYPE_BUFFER, &xBuffer, uSize, s_uNextFreeStagingOffset };
		s_xStagingAllocations.push_back(xAllocation);

		void* pMap = xDevice.mapMemory(s_xStagingMem, s_uNextFreeStagingOffset, uSize);
		memcpy(pMap, pData, uSize);
		xDevice.unmapMemory(s_xStagingMem);
		s_uNextFreeStagingOffset += uSize;
	}
	s_xMutex.Unlock();
}

void Zenith_Vulkan_MemoryManager::UploadTextureData(Zenith_Vulkan_Texture& xTexture, const void* pData, size_t uSize)
{
	s_xMutex.Lock();
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	const VmaAllocation& xAlloc = xTexture.GetAllocation();
	const VmaAllocationInfo* pxAllocInfo = xTexture.GetAllocationInfo_Ptr();
	VkMemoryPropertyFlags eMemoryProps;
	vmaGetAllocationMemoryProperties(s_xAllocator, xAlloc, &eMemoryProps);

	if (eMemoryProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		memcpy(pxAllocInfo->pMappedData, pData, uSize);
#ifdef ZENITH_ASSERT
		VkResult eResult =
#endif
			vmaFlushAllocation(s_xAllocator, xAlloc, 0, VK_WHOLE_SIZE);
		Zenith_Assert(eResult == VK_SUCCESS, "Failed to flush allocation");
	}
	else
	{
		if (s_uNextFreeStagingOffset + uSize >= g_uStagingPoolSize)
		{
			HandleStagingBufferFull();
		}

		StagingMemoryAllocation xAllocation = { ALLOCATION_TYPE_TEXTURE, &xTexture, uSize, s_uNextFreeStagingOffset };
		s_xStagingAllocations.push_back(xAllocation);

		void* pMap = xDevice.mapMemory(s_xStagingMem, s_uNextFreeStagingOffset, uSize);
		memcpy(pMap, pData, uSize);
		xDevice.unmapMemory(s_xStagingMem);
		s_uNextFreeStagingOffset += uSize;
	}
	s_xMutex.Unlock();
}

void Zenith_Vulkan_MemoryManager::FlushStagingBuffer()
{
	for (auto it = s_xStagingAllocations.begin(); it != s_xStagingAllocations.end(); it++) {
		StagingMemoryAllocation& xAlloc = *it;
		if (xAlloc.m_eType == ALLOCATION_TYPE_BUFFER) {
			Zenith_Vulkan_Buffer* pxVkBuffer = reinterpret_cast<Zenith_Vulkan_Buffer*>(xAlloc.m_pAllocation);
			s_xCommandBuffer.CopyBufferToBuffer(&s_xStagingBuffer, pxVkBuffer, xAlloc.m_uSize, xAlloc.m_uOffset);
		}
		else if (xAlloc.m_eType == ALLOCATION_TYPE_TEXTURE) {
			Zenith_Vulkan_Texture* pxTexture = reinterpret_cast<Zenith_Vulkan_Texture*>(xAlloc.m_pAllocation);
			vk::Image xImage = pxTexture->GetImage();

			for (uint32_t uLayer = 0; uLayer < pxTexture->GetNumLayers(); uLayer++)
			{
				for (uint32_t uMip = 0; uMip < pxTexture->GetNumMips(); uMip++)
				{
					s_xCommandBuffer.ImageTransitionBarrier(xImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, uMip, uLayer);
				}
			}

			s_xCommandBuffer.CopyBufferToTexture(&s_xStagingBuffer, pxTexture, xAlloc.m_uOffset, pxTexture->GetNumLayers());

			for (uint32_t uLayer = 0; uLayer < pxTexture->GetNumLayers(); uLayer++)
			{
				s_xCommandBuffer.ImageTransitionBarrier(xImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, 0, uLayer);

				for (uint32_t uMip = 1; uMip < pxTexture->GetNumMips(); uMip++)
				{
					s_xCommandBuffer.BlitTextureToTexture(pxTexture, pxTexture, uMip, uLayer);
				}

				s_xCommandBuffer.ImageTransitionBarrier(xImage, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, 0, uLayer);

				for (uint32_t uMip = 1; uMip < pxTexture->GetNumMips(); uMip++)
				{
					s_xCommandBuffer.ImageTransitionBarrier(xImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, uMip, uLayer);
				}
			}
		}
	}

	s_xStagingAllocations.clear();

	s_uNextFreeStagingOffset = 0;
}

void Zenith_Vulkan_MemoryManager::HandleStagingBufferFull() {
	Zenith_Log("Staging buffer full, flushing");
	EndFrame(false);
	BeginFrame();
}