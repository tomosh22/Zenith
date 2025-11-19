#include "Zenith.h"
#define VMA_IMPLEMENTATION

#include "Zenith_Vulkan_MemoryManager.h"
#include "Zenith_Vulkan_CommandBuffer.h"

#include "Zenith_Vulkan.h"

#include "Flux/Flux_Buffers.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"

Zenith_Vulkan_CommandBuffer Zenith_Vulkan_MemoryManager::s_xCommandBuffer;
VmaAllocator Zenith_Vulkan_MemoryManager::s_xAllocator;
vk::Buffer Zenith_Vulkan_MemoryManager::s_xStagingBuffer;
vk::DeviceMemory Zenith_Vulkan_MemoryManager::s_xStagingMem;
std::list<Zenith_Vulkan_MemoryManager::StagingMemoryAllocation> Zenith_Vulkan_MemoryManager::s_xStagingAllocations;
std::list<Zenith_Vulkan_MemoryManager::PendingVRAMDeletion> Zenith_Vulkan_MemoryManager::s_xPendingDeletions;

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
	s_xStagingBuffer = xBuffer;

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

	const u_int uNumAllocatedVRAMs = Zenith_Vulkan::s_xVRAMRegistry.size();

	// Process deferred VRAM deletions
	ProcessDeferredDeletions();

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
	s_xCommandBuffer.ImageTransitionBarrier(xImage, eOldLayout, eNewLayout, eAspect, eSrcStage, eDstStage, uMipLevel, uLayer);
}

void Zenith_Vulkan_MemoryManager::InitialiseVertexBuffer(const void* pData, size_t uSize, Flux_VertexBuffer& xBufferOut, bool bDeviceLocal /*= true*/)
{
	Flux_VRAMHandle xHandle = CreateBufferVRAM(uSize, static_cast<MemoryFlags>(1 << MEMORY_FLAGS__VERTEX_BUFFER), bDeviceLocal ? MEMORY_RESIDENCY_GPU : MEMORY_RESIDENCY_CPU);
	xBufferOut.GetBuffer().m_xVRAMHandle = xHandle;
	xBufferOut.GetBuffer().m_ulSize = uSize;
	
	if (pData)
	{
		UploadBufferData(xHandle, pData, uSize);
	}
}

void Zenith_Vulkan_MemoryManager::InitialiseDynamicVertexBuffer(const void* pData, size_t uSize, Flux_DynamicVertexBuffer& xBufferOut, bool bDeviceLocal /*= true*/)
{
	for (uint32_t u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		Flux_VRAMHandle xHandle = CreateBufferVRAM(uSize, static_cast<MemoryFlags>(1 << MEMORY_FLAGS__VERTEX_BUFFER), bDeviceLocal ? MEMORY_RESIDENCY_GPU : MEMORY_RESIDENCY_CPU);
		xBufferOut.GetBufferForFrameInFlight(u).m_xVRAMHandle = xHandle;
		xBufferOut.GetBufferForFrameInFlight(u).m_ulSize = uSize;
		
		if (pData)
		{
			UploadBufferData(xHandle, pData, uSize);
		}
	}
}

void Zenith_Vulkan_MemoryManager::InitialiseIndexBuffer(const void* pData, size_t uSize, Flux_IndexBuffer& xBufferOut)
{
	Flux_VRAMHandle xHandle = CreateBufferVRAM(uSize, static_cast<MemoryFlags>(1 << MEMORY_FLAGS__INDEX_BUFFER), MEMORY_RESIDENCY_GPU);
	xBufferOut.GetBuffer().m_xVRAMHandle = xHandle;
	xBufferOut.GetBuffer().m_ulSize = uSize;
	
	if (pData)
	{
		UploadBufferData(xHandle, pData, uSize);
	}
}

void Zenith_Vulkan_MemoryManager::InitialiseDynamicConstantBuffer(const void* pData, size_t uSize, Flux_DynamicConstantBuffer& xBufferOut)
{
	for (uint32_t u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		Flux_VRAMHandle xHandle = CreateBufferVRAM(uSize, static_cast<MemoryFlags>(1 << MEMORY_FLAGS__SHADER_READ), MEMORY_RESIDENCY_CPU);
		Flux_Buffer& xBuffer = xBufferOut.GetBufferForFrameInFlight(u);
		xBuffer.m_xVRAMHandle = xHandle;
		xBuffer.m_ulSize = uSize;

		Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(xHandle);
		Zenith_Assert(pxVRAM, "Invalid buffer VRAM handle");

		xBuffer.m_xCBV.m_xBufferInfo.setBuffer(pxVRAM->GetBuffer());
		xBuffer.m_xCBV.m_xBufferInfo.setOffset(0);
		xBuffer.m_xCBV.m_xBufferInfo.setRange(uSize);
		xBuffer.m_xCBV.m_xVRAMHandle = xHandle;
		
		if (pData)
		{
			UploadBufferData(xHandle, pData, uSize);
		}
	}
}

Flux_VRAMHandle Zenith_Vulkan_MemoryManager::CreateBufferVRAM(const u_int uSize, const MemoryFlags eFlags, MemoryResidency eResidency)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	vk::BufferUsageFlags eUsageFlags = vk::BufferUsageFlagBits::eTransferDst;
	
	if (eFlags & 1 << MEMORY_FLAGS__VERTEX_BUFFER)
		eUsageFlags |= vk::BufferUsageFlagBits::eVertexBuffer;
	if (eFlags & 1 << MEMORY_FLAGS__INDEX_BUFFER)
		eUsageFlags |= vk::BufferUsageFlagBits::eIndexBuffer;
	if (eFlags & 1 << MEMORY_FLAGS__INDIRECT_BUFFER)
		eUsageFlags |= vk::BufferUsageFlagBits::eIndirectBuffer;
	if (eFlags & 1 << MEMORY_FLAGS__UNORDERED_ACCESS)
		eUsageFlags |= vk::BufferUsageFlagBits::eStorageBuffer;
	if (eFlags & 1 << MEMORY_FLAGS__SHADER_READ)
		eUsageFlags |= vk::BufferUsageFlagBits::eUniformBuffer;

	vk::BufferCreateInfo xBufferInfo = vk::BufferCreateInfo()
		.setSize(uSize)
		.setUsage(eUsageFlags)
		.setSharingMode(vk::SharingMode::eExclusive);

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

	VkBuffer xBuffer;
	VmaAllocation xAllocation;
	vmaCreateBuffer(s_xAllocator, &xBufferInfo_Native, &xAllocInfo, &xBuffer, &xAllocation, nullptr);

	Zenith_Vulkan_VRAM* pxVRAM = new Zenith_Vulkan_VRAM(vk::Buffer(xBuffer), xAllocation, s_xAllocator, uSize);
	Flux_VRAMHandle xHandle = Zenith_Vulkan::RegisterVRAM(pxVRAM);

	return xHandle;
}

Flux_VRAMHandle Zenith_Vulkan_MemoryManager::CreateRenderTargetVRAM(const Flux_SurfaceInfo& xInfo)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	
	const bool bIsColour = xInfo.m_eFormat > TEXTURE_FORMAT_COLOUR_BEGIN && xInfo.m_eFormat < TEXTURE_FORMAT_COLOUR_END;
	const bool bIsDepthStencil = xInfo.m_eFormat > TEXTURE_FORMAT_DEPTH_STENCIL_BEGIN && xInfo.m_eFormat < TEXTURE_FORMAT_DEPTH_STENCIL_END;
	Zenith_Assert(bIsColour ^ bIsDepthStencil, "Invalid texture format for render target");
	
	vk::Format xFormat;
	vk::ImageUsageFlags eUsageFlags;
	vk::ImageAspectFlags eAspectFlags;
	vk::ImageLayout eInitialLayout;
	
	if (bIsColour)
	{
		xFormat = Zenith_Vulkan::ConvertToVkFormat_Colour(xInfo.m_eFormat);
		eUsageFlags = vk::ImageUsageFlagBits::eColorAttachment;
		eAspectFlags = vk::ImageAspectFlagBits::eColor;
		eInitialLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		
		if (xInfo.m_uMemoryFlags & 1 << MEMORY_FLAGS__UNORDERED_ACCESS) 
			eUsageFlags |= vk::ImageUsageFlagBits::eStorage;
	}
	else // bIsDepthStencil
	{
		xFormat = Zenith_Vulkan::ConvertToVkFormat_DepthStencil(xInfo.m_eFormat);
		eUsageFlags = vk::ImageUsageFlagBits::eDepthStencilAttachment;
		eAspectFlags = vk::ImageAspectFlagBits::eDepth;
		eInitialLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
	}
	
	if (xInfo.m_uMemoryFlags & 1 << MEMORY_FLAGS__SHADER_READ) 
		eUsageFlags |= vk::ImageUsageFlagBits::eSampled;

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
	Flux_VRAMHandle xHandle = Zenith_Vulkan::RegisterVRAM(pxVRAM);

	if (bIsDepthStencil)
	{
		Zenith_Assert(xInfo.m_eFormat == TEXTURE_FORMAT_D32_SFLOAT, "#TO_TODO: layouts for just depth without stencil");
	}
	
	ImageTransitionBarrier(vk::Image(xImage), vk::ImageLayout::eUndefined, eInitialLayout, eAspectFlags, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands);

	return xHandle;
}

Flux_VRAMHandle Zenith_Vulkan_MemoryManager::CreateTextureVRAM(const void* pData, const Flux_SurfaceInfo& xInfo, bool bCreateMips)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	
	Flux_SurfaceInfo xInfoCopy = xInfo;
	xInfoCopy.m_uNumMips = bCreateMips ? std::floor(std::log2((std::max)(xInfo.m_uWidth, xInfo.m_uHeight))) + 1 : 1;
	
	vk::Format xFormat = Zenith_Vulkan::ConvertToVkFormat_Colour(xInfoCopy.m_eFormat);
	
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
	
	// Add cube compatible flag if this is a cubemap (6 layers)
	if (xInfoCopy.m_uNumLayers == 6)
	{
		xImageInfo.setFlags(vk::ImageCreateFlagBits::eCubeCompatible);
	}

	VmaAllocationCreateInfo xAllocInfo = {};
	xAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	const vk::ImageCreateInfo::NativeType xImageInfo_Native = xImageInfo;
	
	VkImage xImage;
	VmaAllocation xAllocation;
	vmaCreateImage(s_xAllocator, &xImageInfo_Native, &xAllocInfo, &xImage, &xAllocation, nullptr);

	Zenith_Vulkan_VRAM* pxVRAM = new Zenith_Vulkan_VRAM(vk::Image(xImage), xAllocation, s_xAllocator);
	Flux_VRAMHandle xHandle = Zenith_Vulkan::RegisterVRAM(pxVRAM);

	if (pData)
	{
		const size_t ulDataSize = ColourFormatBytesPerPixel(xInfoCopy.m_eFormat) * xInfoCopy.m_uWidth * xInfoCopy.m_uHeight * xInfoCopy.m_uDepth * xInfoCopy.m_uNumLayers;

		// Upload data directly without creating temp texture object
		s_xMutex.Lock();

		VkMemoryPropertyFlags eMemoryProps;
		vmaGetAllocationMemoryProperties(s_xAllocator, xAllocation, &eMemoryProps);

		if (eMemoryProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
		{
			// Direct upload to host-visible memory
			VmaAllocationInfo xAllocInfo;
			vmaGetAllocationInfo(s_xAllocator, xAllocation, &xAllocInfo);
			memcpy(xAllocInfo.pMappedData, pData, ulDataSize);
			vmaFlushAllocation(s_xAllocator, xAllocation, 0, VK_WHOLE_SIZE);
		}
		else
		{
			// Upload via staging buffer
			if (s_uNextFreeStagingOffset + ulDataSize >= g_uStagingPoolSize)
			{
				HandleStagingBufferFull();
			}

			// Create staging allocation with texture metadata directly - no temp texture object
			StagingMemoryAllocation xStagingAlloc;
			xStagingAlloc.m_eType = ALLOCATION_TYPE_TEXTURE;
			xStagingAlloc.m_xTextureMetadata.m_xImage = vk::Image(xImage);
			xStagingAlloc.m_xTextureMetadata.m_uWidth = xInfoCopy.m_uWidth;
			xStagingAlloc.m_xTextureMetadata.m_uHeight = xInfoCopy.m_uHeight;
			xStagingAlloc.m_xTextureMetadata.m_uNumMips = xInfoCopy.m_uNumMips;
			xStagingAlloc.m_xTextureMetadata.m_uNumLayers = xInfoCopy.m_uNumLayers;
			xStagingAlloc.m_uSize = ulDataSize;
			xStagingAlloc.m_uOffset = s_uNextFreeStagingOffset;
			s_xStagingAllocations.push_back(xStagingAlloc);

			void* pMap = xDevice.mapMemory(s_xStagingMem, s_uNextFreeStagingOffset, ulDataSize);
			memcpy(pMap, pData, ulDataSize);
			xDevice.unmapMemory(s_xStagingMem);
			s_uNextFreeStagingOffset += ulDataSize;
		}
		s_xMutex.Unlock();
	}
	else
	{
		// For host-visible memory, transition directly to shader read layout
		ImageTransitionBarrier(vk::Image(xImage), vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands);
	}

	return xHandle;
}

vk::ImageView Zenith_Vulkan_MemoryManager::CreateRenderTargetView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(xVRAMHandle);
	
	vk::Format xFormat = Zenith_Vulkan::ConvertToVkFormat_Colour(xInfo.m_eFormat);
	
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

vk::ImageView Zenith_Vulkan_MemoryManager::CreateDepthStencilView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(xVRAMHandle);
	
	vk::Format xFormat = Zenith_Vulkan::ConvertToVkFormat_DepthStencil(xInfo.m_eFormat);
	
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

vk::ImageView Zenith_Vulkan_MemoryManager::CreateShaderResourceView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uBaseMip, uint32_t uMipCount)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(xVRAMHandle);
	
	const bool bIsDepth = xInfo.m_eFormat > TEXTURE_FORMAT_DEPTH_STENCIL_BEGIN && xInfo.m_eFormat < TEXTURE_FORMAT_DEPTH_STENCIL_END;
	vk::Format xFormat = bIsDepth ? Zenith_Vulkan::ConvertToVkFormat_DepthStencil(xInfo.m_eFormat) : Zenith_Vulkan::ConvertToVkFormat_Colour(xInfo.m_eFormat);
	
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

vk::ImageView Zenith_Vulkan_MemoryManager::CreateUnorderedAccessView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(xVRAMHandle);
	
	vk::Format xFormat = Zenith_Vulkan::ConvertToVkFormat_Colour(xInfo.m_eFormat);
	
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

void Zenith_Vulkan_MemoryManager::UploadBufferData(Flux_VRAMHandle xBufferHandle, const void* pData, size_t uSize)
{
	s_xMutex.Lock();
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	
	Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(xBufferHandle);
	const VmaAllocation& xAlloc = pxVRAM->GetAllocation();
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

		StagingMemoryAllocation xAllocation;
		xAllocation.m_eType = ALLOCATION_TYPE_BUFFER;
		xAllocation.m_xBufferMetadata.m_xBuffer = pxVRAM->GetBuffer();
		xAllocation.m_uSize = uSize;
		xAllocation.m_uOffset = s_uNextFreeStagingOffset;
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
			const StagingBufferMetadata& xMeta = xAlloc.m_xBufferMetadata;
			vk::BufferCopy xCopyRegion(xAlloc.m_uOffset, 0, xAlloc.m_uSize);
			s_xCommandBuffer.GetCurrentCmdBuffer().copyBuffer(s_xStagingBuffer, xMeta.m_xBuffer, xCopyRegion);
		}
		else if (xAlloc.m_eType == ALLOCATION_TYPE_TEXTURE) {
			const StagingTextureMetadata& xMeta = xAlloc.m_xTextureMetadata;
			const vk::Image& xImage = xMeta.m_xImage;

			for (uint32_t uLayer = 0; uLayer < xMeta.m_uNumLayers; uLayer++)
			{
				for (uint32_t uMip = 0; uMip < xMeta.m_uNumMips; uMip++)
				{
					s_xCommandBuffer.ImageTransitionBarrier(xImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, uMip, uLayer);
				}
			}

			// Copy buffer to image using metadata
			vk::ImageSubresourceLayers xSubresource = vk::ImageSubresourceLayers()
				.setAspectMask(vk::ImageAspectFlagBits::eColor)
				.setMipLevel(0)
				.setBaseArrayLayer(0)
				.setLayerCount(xMeta.m_uNumLayers);

			vk::BufferImageCopy region = vk::BufferImageCopy()
				.setBufferOffset(xAlloc.m_uOffset)
				.setBufferRowLength(0)
				.setBufferImageHeight(0)
				.setImageSubresource(xSubresource)
				.setImageOffset({ 0, 0, 0 })
				.setImageExtent({ xMeta.m_uWidth, xMeta.m_uHeight, 1 });

			s_xCommandBuffer.GetCurrentCmdBuffer().copyBufferToImage(s_xStagingBuffer, xImage, vk::ImageLayout::eTransferDstOptimal, 1, &region);

			for (uint32_t uLayer = 0; uLayer < xMeta.m_uNumLayers; uLayer++)
			{
				s_xCommandBuffer.ImageTransitionBarrier(xImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, 0, uLayer);

				for (uint32_t uMip = 1; uMip < xMeta.m_uNumMips; uMip++)
				{
					// Blit mipmap generation using metadata
					std::array<vk::Offset3D, 2> axSrcOffsets;
					axSrcOffsets.at(0).setX(0);
					axSrcOffsets.at(0).setY(0);
					axSrcOffsets.at(0).setZ(0);
					axSrcOffsets.at(1).setX(xMeta.m_uWidth >> (uMip - 1));
					axSrcOffsets.at(1).setY(xMeta.m_uHeight >> (uMip - 1));
					axSrcOffsets.at(1).setZ(1);

					vk::ImageSubresourceLayers xSrcSubresource = vk::ImageSubresourceLayers()
						.setAspectMask(vk::ImageAspectFlagBits::eColor)
						.setMipLevel(uMip - 1)
						.setBaseArrayLayer(uLayer)
						.setLayerCount(1);

					std::array<vk::Offset3D, 2> axDstOffsets;
					axDstOffsets.at(0).setX(0);
					axDstOffsets.at(0).setY(0);
					axDstOffsets.at(0).setZ(0);
					axDstOffsets.at(1).setX(xMeta.m_uWidth >> uMip);
					axDstOffsets.at(1).setY(xMeta.m_uHeight >> uMip);
					axDstOffsets.at(1).setZ(1);

					vk::ImageSubresourceLayers xDstSubresource = vk::ImageSubresourceLayers()
						.setAspectMask(vk::ImageAspectFlagBits::eColor)
						.setMipLevel(uMip)
						.setBaseArrayLayer(uLayer)
						.setLayerCount(1);

					vk::ImageBlit xBlit = vk::ImageBlit()
						.setSrcOffsets(axSrcOffsets)
						.setSrcSubresource(xSrcSubresource)
						.setDstOffsets(axDstOffsets)
						.setDstSubresource(xDstSubresource);

					s_xCommandBuffer.GetCurrentCmdBuffer().blitImage(xImage, vk::ImageLayout::eTransferSrcOptimal, xImage, vk::ImageLayout::eTransferDstOptimal, 1, &xBlit, vk::Filter::eLinear);
					s_xCommandBuffer.ImageTransitionBarrier(xImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, uMip, uLayer);
				}

				s_xCommandBuffer.ImageTransitionBarrier(xImage, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, 0, uLayer);

				for (uint32_t uMip = 1; uMip < xMeta.m_uNumMips; uMip++)
				{
					s_xCommandBuffer.ImageTransitionBarrier(xImage, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, uMip, uLayer);
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

void Zenith_Vulkan_MemoryManager::QueueVRAMDeletion(Zenith_Vulkan_VRAM* pxVRAM, const Flux_VRAMHandle xHandle, 
	vk::ImageView xRTV, vk::ImageView xDSV, vk::ImageView xSRV, vk::ImageView xUAV)
{
	if (pxVRAM == nullptr && xRTV == VK_NULL_HANDLE && xDSV == VK_NULL_HANDLE && 
		xSRV == VK_NULL_HANDLE && xUAV == VK_NULL_HANDLE)
	{
		return;
	}

	PendingVRAMDeletion xDeletion;
	xDeletion.m_pxVRAM = pxVRAM;
	xDeletion.m_xHandle = xHandle;
	xDeletion.m_xRTV = xRTV;
	xDeletion.m_xDSV = xDSV;
	xDeletion.m_xSRV = xSRV;
	xDeletion.m_xUAV = xUAV;
	xDeletion.m_uFramesRemaining = MAX_FRAMES_IN_FLIGHT;
	s_xPendingDeletions.push_back(xDeletion);
}

void Zenith_Vulkan_MemoryManager::QueueImageViewDeletion(vk::ImageView xImageView)
{
	if (xImageView == VK_NULL_HANDLE)
	{
		return;
	}
	
	// Queue for deletion without VRAM - just destroy the image view
	Flux_VRAMHandle xInvalidHandle;  // Default constructed handle is invalid
	QueueVRAMDeletion(nullptr, xInvalidHandle, xImageView);
}

void Zenith_Vulkan_MemoryManager::ProcessDeferredDeletions()
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	
	for (auto it = s_xPendingDeletions.begin(); it != s_xPendingDeletions.end();)
	{
		it->m_uFramesRemaining--;
		
		if (it->m_uFramesRemaining == 0)
		{
			// Destroy all image views before deleting VRAM
			if (it->m_xRTV != VK_NULL_HANDLE)
			{
				xDevice.destroyImageView(it->m_xRTV);
			}
			if (it->m_xDSV != VK_NULL_HANDLE)
			{
				xDevice.destroyImageView(it->m_xDSV);
			}
			if (it->m_xSRV != VK_NULL_HANDLE)
			{
				xDevice.destroyImageView(it->m_xSRV);
			}
			if (it->m_xUAV != VK_NULL_HANDLE)
			{
				xDevice.destroyImageView(it->m_xUAV);
			}
			
			// Delete VRAM and release handle (only if VRAM exists)
			if (it->m_pxVRAM != nullptr)
			{
				delete it->m_pxVRAM;
				Zenith_Vulkan::ReleaseVRAMHandle(it->m_xHandle);
			}
			
			it = s_xPendingDeletions.erase(it);
		}
		else
		{
			++it;
		}
	}
}
