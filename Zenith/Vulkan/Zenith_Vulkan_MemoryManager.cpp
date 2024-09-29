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

void Zenith_Vulkan_MemoryManager::InitialiseStagingBuffer()
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	const vk::PhysicalDevice& xPhysicalDevice = Zenith_Vulkan::GetPhysicalDevice();

	vk::BufferCreateInfo xInfo = vk::BufferCreateInfo()
		.setSize(g_uStagingPoolSize)
		.setUsage(vk::BufferUsageFlagBits::eTransferSrc)
		.setSharingMode(vk::SharingMode::eExclusive);

	s_xStagingBuffer.SetBuffer(xDevice.createBuffer(xInfo));

	vk::MemoryRequirements xRequirements = xDevice.getBufferMemoryRequirements(s_xStagingBuffer.GetBuffer());

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
	xDevice.bindBufferMemory(s_xStagingBuffer.GetBuffer(), s_xStagingMem, 0);
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
		s_xCommandBuffer.EndRecording(RENDER_ORDER_MEMORY_UPDATE, false);
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
		xAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
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
	if (pxBuffer->GetBuffer() == VK_NULL_HANDLE)
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

void Zenith_Vulkan_MemoryManager::InitialiseConstantBuffer(const void* pData, size_t uSize, Flux_ConstantBuffer& xBufferOut)
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

void Zenith_Vulkan_MemoryManager::CreateColourAttachment(uint32_t uWidth, uint32_t uHeight, ColourFormat eFormat, uint32_t uBitsPerPixel, Zenith_Vulkan_Texture& xTextureOut)
{
	FreeTexture(&xTextureOut);
	AllocateTexture(uWidth, uHeight, 1, eFormat, DEPTHSTENCIL_FORMAT_NONE, uBitsPerPixel, 1 /* #TO_TODO: mips */, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled, MEMORY_RESIDENCY_GPU, xTextureOut);
	ImageTransitionBarrier(xTextureOut.GetImage(), vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands);
}
void Zenith_Vulkan_MemoryManager::CreateDepthStencilAttachment(uint32_t uWidth, uint32_t uHeight, DepthStencilFormat eFormat, uint32_t uBitsPerPixel, Zenith_Vulkan_Texture& xTextureOut)
{
	FreeTexture(&xTextureOut);
	AllocateTexture(uWidth, uHeight, 1, COLOUR_FORMAT_NONE, eFormat, uBitsPerPixel, 1, vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled, MEMORY_RESIDENCY_GPU, xTextureOut);
	Zenith_Assert(eFormat == DEPTHSTENCIL_FORMAT_D32_SFLOAT, "#TO_TODO: layouts for just depth without stencil");
	ImageTransitionBarrier(xTextureOut.GetImage(), vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ImageAspectFlagBits::eDepth, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands);
}

void Zenith_Vulkan_MemoryManager::CreateTexture(const void* pData, const uint32_t uWidth, const uint32_t uHeight, const uint32_t uDepth, ColourFormat eFormat, Zenith_Vulkan_Texture& xTextureOut)
{
	FreeTexture(&xTextureOut);

	uint32_t uNumMips = std::floor(std::log2(std::max(uWidth, uHeight))) + 1;

	//#TO_TODO: other formats
	AllocateTexture(uWidth, uHeight, 1, eFormat, DEPTHSTENCIL_FORMAT_NONE, ColourFormatBytesPerPixel(eFormat) /*bytes per pizel*/, uNumMips, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc, MEMORY_RESIDENCY_GPU, xTextureOut);
	xTextureOut.SetWidth(uWidth);
	xTextureOut.SetHeight(uHeight);
	xTextureOut.SetNumMips(uNumMips);
	xTextureOut.SetNumLayers(1);
	UploadTextureData(xTextureOut, pData, ColourFormatBytesPerPixel(eFormat) * uWidth * uHeight * uDepth);
}

void Zenith_Vulkan_MemoryManager::CreateTexture(const char* szPath, Zenith_Vulkan_Texture& xTextureOut)
{
	FreeTexture(&xTextureOut);

	uint32_t uWidth = 0, uHeight = 0, uDepth = 0;
	vk::Format eFormat = vk::Format::eUndefined;
	void* pData = nullptr;

	FILE* pxFile = fopen(szPath, "rb");
	fseek(pxFile, 0, SEEK_END);
	size_t ulFileSize = ftell(pxFile);
	fseek(pxFile, 0, SEEK_SET);
	char* pcData = new char[ulFileSize + 1];
	fread(pcData, ulFileSize, 1, pxFile);
	pcData[ulFileSize] = '\0';
	fclose(pxFile);

	size_t ulCursor = 0;

	uWidth = atoi(pcData + ulCursor);
	ulCursor += std::to_string(uWidth).length() + 1;

	uHeight = atoi(pcData + ulCursor);
	ulCursor += std::to_string(uHeight).length() + 1;

	uDepth = atoi(pcData + ulCursor);
	ulCursor += std::to_string(uDepth).length() + 1;

	std::string strFormat(pcData + ulCursor);
	ulCursor += strFormat.length() + 1;
	//#TO_TODO: other formats
	eFormat = Zenith_Vulkan_Texture::ConvertToVkFormat_Colour((ColourFormat)std::stoi(strFormat));
	//eFormat = vk::Format::eR8G8B8A8Unorm;

	size_t ulDataSize = uWidth * uHeight * uDepth * 4 /*bytes per pixel*/;
	pData = malloc(ulDataSize);
	memcpy(pData, pcData + ulCursor, ulDataSize);

	delete[] pcData;

	uint32_t uNumMips = std::floor(std::log2(std::max(uWidth, uHeight))) + 1;

	//#TO_TODO: other formats
	AllocateTexture(uWidth, uHeight, 1, COLOUR_FORMAT_RGBA8_UNORM, DEPTHSTENCIL_FORMAT_NONE, 4 /*bytes per pizel*/, uNumMips, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc, MEMORY_RESIDENCY_GPU, xTextureOut);
	xTextureOut.SetWidth(uWidth);
	xTextureOut.SetHeight(uHeight);
	xTextureOut.SetNumMips(uNumMips);
	xTextureOut.SetNumLayers(1);
	UploadTextureData(xTextureOut, pData, ulDataSize);
	delete pData;
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

		vk::Format eFormat = vk::Format::eUndefined;

		FILE* pxFile = fopen(szPath, "rb");
		fseek(pxFile, 0, SEEK_END);
		size_t ulFileSize = ftell(pxFile);
		fseek(pxFile, 0, SEEK_SET);
		char* pcData = new char[ulFileSize + 1];
		fread(pcData, ulFileSize, 1, pxFile);
		pcData[ulFileSize] = '\0';
		fclose(pxFile);

		size_t ulCursor = 0;

		uWidth = atoi(pcData + ulCursor);
		ulCursor += std::to_string(uWidth).length() + 1;

		uHeight = atoi(pcData + ulCursor);
		ulCursor += std::to_string(uHeight).length() + 1;

		uDepth = atoi(pcData + ulCursor);
		ulCursor += std::to_string(uDepth).length() + 1;

		std::string strFormat(pcData + ulCursor);
		ulCursor += strFormat.length() + 1;
		//#TO_TODO: other formats
		eFormat = vk::Format::eR8G8B8A8Unorm;
		//eFormat = StringToVkFormat(strFormat);

		size_t ulThisFileDataSize = uWidth * uHeight * uDepth * 4 /*bytes per pixel*/;
		aulDataSizes[u] = ulThisFileDataSize;

		ulTotalDataSize += ulThisFileDataSize;
		pData = malloc(ulThisFileDataSize);
		memcpy(pData, pcData + ulCursor, ulThisFileDataSize);

		delete[] pcData;

		uNumMips = std::floor(std::log2(std::max(uWidth, uHeight))) + 1;
	}

	//#TO_TODO: other formats
	AllocateTexture(uWidth, uHeight, 6, COLOUR_FORMAT_RGBA8_UNORM, DEPTHSTENCIL_FORMAT_NONE, 4 /*bytes per pizel*/, uNumMips, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc, MEMORY_RESIDENCY_GPU, xTextureOut);
	xTextureOut.SetWidth(uWidth);
	xTextureOut.SetHeight(uHeight);
	xTextureOut.SetNumMips(uNumMips);
	xTextureOut.SetNumLayers(6);

	void* pAllData = malloc(ulTotalDataSize);
	size_t ulCursor = 0;
	for (uint32_t u = 0; u < 6; u++)
	{
		memcpy((uint8_t*)pAllData + ulCursor, apDatas[u], aulDataSizes[u]);
		ulCursor += aulDataSizes[u];
	}

	UploadTextureData(xTextureOut, pAllData, ulTotalDataSize);

	for (uint32_t u = 0; u < 6; u++)
	{
		delete apDatas[u];
	}
}

void Zenith_Vulkan_MemoryManager::AllocateTexture(uint32_t uWidth, uint32_t uHeight, uint32_t uNumLayers, ColourFormat eColourFormat, DepthStencilFormat eDepthStencilFormat, uint32_t uBytesPerPixel, uint32_t uNumMips, vk::ImageUsageFlags eUsageFlags, MemoryResidency eResidency, Zenith_Vulkan_Texture& xTextureOut)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	vk::Format xFormat;
	bool bIsDepth;
	if (eColourFormat != COLOUR_FORMAT_NONE)
	{
		Zenith_Assert(eDepthStencilFormat == DEPTHSTENCIL_FORMAT_NONE, "Can't have both colour and d/s format");
		xFormat = Zenith_Vulkan_Texture::ConvertToVkFormat_Colour(eColourFormat);
		bIsDepth = false;
	}
	else if (eDepthStencilFormat != DEPTHSTENCIL_FORMAT_NONE)
	{
		Zenith_Assert(eColourFormat == COLOUR_FORMAT_NONE, "Can't have both colour and d/s format");
		xFormat = Zenith_Vulkan_Texture::ConvertToVkFormat_DepthStencil(eDepthStencilFormat);
		bIsDepth = true;
	}

	vk::ImageCreateInfo xImageInfo = vk::ImageCreateInfo()
		.setImageType(vk::ImageType::e2D)
		.setFormat(xFormat)
		.setTiling(vk::ImageTiling::eOptimal)
		.setExtent({ uWidth,uHeight,1 })
		.setMipLevels(uNumMips)
		.setArrayLayers(uNumLayers)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setUsage(eUsageFlags)
		.setSharingMode(vk::SharingMode::eExclusive)
		.setSamples(vk::SampleCountFlagBits::e1);
	if (uNumLayers == 6)
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

	vk::ImageSubresourceRange xSubresourceRange = vk::ImageSubresourceRange()
		.setAspectMask(bIsDepth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor)
		.setBaseMipLevel(0)
		.setLevelCount(uNumMips)
		.setBaseArrayLayer(0)
		.setLayerCount(uNumLayers);

	vk::ImageViewCreateInfo xViewCreate = vk::ImageViewCreateInfo()
		.setImage(xTextureOut.GetImage())
		.setViewType(uNumLayers == 1 ? vk::ImageViewType::e2D : vk::ImageViewType::eCube)
		.setFormat(xFormat)
		.setSubresourceRange(xSubresourceRange);

	xTextureOut.SetImageView(xDevice.createImageView(xViewCreate));
}

void Zenith_Vulkan_MemoryManager::FreeTexture(Zenith_Vulkan_Texture* pxTexture)
{
	if (pxTexture->GetImage() == VK_NULL_HANDLE)
	{
		return;
	}

	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	xDevice.destroyImageView(pxTexture->GetImageView());

	vmaDestroyImage(s_xAllocator, pxTexture->GetImage(), pxTexture->GetAllocation());

	pxTexture->SetImage(VK_NULL_HANDLE);
}


void Zenith_Vulkan_MemoryManager::UploadBufferData(Zenith_Vulkan_Buffer& xBuffer, const void* pData, size_t uSize)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	const VmaAllocation& xAlloc = xBuffer.GetAllocation();
	const VmaAllocationInfo* pxAllocInfo = xBuffer.GetAllocationInfo_Ptr();
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

		StagingMemoryAllocation xAllocation = { ALLOCATION_TYPE_BUFFER, &xBuffer, uSize, s_uNextFreeStagingOffset };
		s_xStagingAllocations.push_back(xAllocation);

		void* pMap = xDevice.mapMemory(s_xStagingMem, s_uNextFreeStagingOffset, uSize);
		memcpy(pMap, pData, uSize);
		xDevice.unmapMemory(s_xStagingMem);
		s_uNextFreeStagingOffset += uSize;
	}
}

void Zenith_Vulkan_MemoryManager::UploadTextureData(Zenith_Vulkan_Texture& xTexture, const void* pData, size_t uSize)
{
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

			for (uint32_t uLayer = 0; uLayer < pxTexture->GetNumLayers(); uLayer++)
			{
				for (uint32_t uMip = 0; uMip < pxTexture->GetNumMips(); uMip++)
				{
					s_xCommandBuffer.ImageTransitionBarrier(pxTexture->GetImage(), vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, uMip, uLayer);
				}
			}

			s_xCommandBuffer.CopyBufferToTexture(&s_xStagingBuffer, pxTexture, xAlloc.m_uOffset, pxTexture->GetNumLayers());

			for (uint32_t uLayer = 0; uLayer < pxTexture->GetNumLayers(); uLayer++)
			{
				s_xCommandBuffer.ImageTransitionBarrier(pxTexture->GetImage(), vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, 0, uLayer);

				for (uint32_t uMip = 1; uMip < pxTexture->GetNumMips(); uMip++)
				{
					s_xCommandBuffer.BlitTextureToTexture(pxTexture, pxTexture, uMip, uLayer);
				}

				s_xCommandBuffer.ImageTransitionBarrier(pxTexture->GetImage(), vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, 0, uLayer);

				for (uint32_t uMip = 1; uMip < pxTexture->GetNumMips(); uMip++)
				{
					s_xCommandBuffer.ImageTransitionBarrier(pxTexture->GetImage(), vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, uMip, uLayer);
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