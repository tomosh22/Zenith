#include "Zenith.h"

#include "Zenith_Vulkan_MemoryManager.h"
#include "Zenith_Vulkan_CommandBuffer.h"

#include "Zenith_Vulkan.h"
#include "Zenith_Vulkan_Buffer.h"
#include "Zenith_Vulkan_Texture.h"

#include "Flux/Flux_Buffers.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"

vk::DeviceMemory Zenith_Vulkan_MemoryManager::s_xCPUMemory;
vk::DeviceMemory Zenith_Vulkan_MemoryManager::s_xGPUMemory;

Zenith_Vulkan_Buffer* Zenith_Vulkan_MemoryManager::s_pxStagingBuffer = nullptr;
Zenith_Vulkan_CommandBuffer Zenith_Vulkan_MemoryManager::s_xCommandBuffer;

std::unordered_map<void*, Zenith_Vulkan_MemoryManager::MemoryAllocation> Zenith_Vulkan_MemoryManager::s_xCpuAllocationMap;
std::unordered_map<void*, Zenith_Vulkan_MemoryManager::MemoryAllocation> Zenith_Vulkan_MemoryManager::s_xGpuAllocationMap;

std::list<Zenith_Vulkan_MemoryManager::StagingMemoryAllocation> Zenith_Vulkan_MemoryManager::s_xStagingAllocations;

size_t Zenith_Vulkan_MemoryManager::s_uNextFreeCpuOffset = 0;
size_t Zenith_Vulkan_MemoryManager::s_uNextFreeGpuOffset = 0;
size_t Zenith_Vulkan_MemoryManager::s_uNextFreeStagingOffset = 0;


void Zenith_Vulkan_MemoryManager::Initialise()
{
	const vk::PhysicalDevice& xPhysDevice = Zenith_Vulkan::GetPhysicalDevice();
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	uint64_t uAlignedCpuSize = ALIGN(g_uCpuPoolSize, 4096);

	uint32_t uCpuMemoryType = ~0u;
	for (uint32_t i = 0; i < xPhysDevice.getMemoryProperties().memoryTypeCount; i++)
	{
		vk::MemoryType xMemType = xPhysDevice.getMemoryProperties().memoryTypes[i];
		if (xMemType.propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible &&
			xMemType.propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent &&
			xPhysDevice.getMemoryProperties().memoryHeaps[xMemType.heapIndex].size > uAlignedCpuSize
			)
		{
			uCpuMemoryType = i;
		}
	}
	Zenith_Assert(uCpuMemoryType != ~0u, "couldn't find physical memory type");


	vk::MemoryAllocateInfo xCpuAllocInfo = vk::MemoryAllocateInfo()
		.setAllocationSize(uAlignedCpuSize)
		.setMemoryTypeIndex(uCpuMemoryType);

#ifdef ZENITH_RAYTRACING
	vk::MemoryAllocateFlagsInfo xCpuFlags = vk::MemoryAllocateFlagsInfo()
		.setFlags(vk::MemoryAllocateFlagBits::eDeviceAddress);
	xCpuAllocInfo.setPNext(&xCpuFlags);
#endif

	s_xCPUMemory = xDevice.allocateMemory(xCpuAllocInfo);

	uint64_t uAlignedGpuSize = ALIGN(g_uGpuPoolSize, 4096);

	uint32_t uGpuMemoryType = ~0u;
	for (uint32_t i = 0; i < xPhysDevice.getMemoryProperties().memoryTypeCount; i++)
	{
		vk::MemoryType xMemType = xPhysDevice.getMemoryProperties().memoryTypes[i];
		if (xMemType.propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal &&
			xPhysDevice.getMemoryProperties().memoryHeaps[xMemType.heapIndex].size > uAlignedGpuSize
			&& 59 & (1 << i) //#TO_TODO: what in the flying fuck is this about???
			)
		{
			uGpuMemoryType = i;
		}
	}
	Zenith_Assert(uGpuMemoryType != ~0u, "couldn't find physical memory type");


	vk::MemoryAllocateInfo xGpuAllocInfo = vk::MemoryAllocateInfo()
		.setAllocationSize(uAlignedGpuSize)
		.setMemoryTypeIndex(uGpuMemoryType);

#ifdef ZENITH_RAYTRACING
	vk::MemoryAllocateFlagsInfo xGpuFlags = vk::MemoryAllocateFlagsInfo()
		.setFlags(vk::MemoryAllocateFlagBits::eDeviceAddress);
	xGpuAllocInfo.setPNext(&xGpuFlags);
#endif

	s_xGPUMemory = xDevice.allocateMemory(xGpuAllocInfo);

	s_pxStagingBuffer = new Zenith_Vulkan_Buffer(g_uStagingPoolSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

	s_xCommandBuffer.Initialise(COMMANDTYPE_COPY);

	Zenith_Log("Vulkan memory manager initialised");
}

void Zenith_Vulkan_MemoryManager::Shutdown()
{
	delete s_pxStagingBuffer;
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
	xBufferOut.SetBuffer(xDevice.createBuffer(xBufferInfo));
	xBufferOut.SetSize(uSize);

	vk::MemoryRequirements xRequirements = xDevice.getBufferMemoryRequirements(xBufferOut.GetBuffer());
	uint32_t uAlign = xRequirements.alignment;
#ifdef ZENITH_RAYTRACING
	uAlign = std::max(uAlign, 128u);
#endif

	if (eResidency == MEMORY_RESIDENCY_CPU) {
		if (ALIGN(s_uNextFreeCpuOffset, uAlign) + xRequirements.size >= g_uCpuPoolSize)
		{
			HandleCpuOutOfMemory();
		}

		xDevice.bindBufferMemory(xBufferOut.GetBuffer(), s_xCPUMemory, ALIGN(s_uNextFreeCpuOffset, uAlign));

		MemoryAllocation xAllocation = { ALLOCATION_TYPE_BUFFER, xRequirements.size, ALIGN(s_uNextFreeCpuOffset, uAlign) };
		s_xCpuAllocationMap.insert({ &xBufferOut, xAllocation });
		s_uNextFreeCpuOffset = ALIGN(s_uNextFreeCpuOffset, uAlign) + xRequirements.size;
	}
	else if (eResidency == MEMORY_RESIDENCY_GPU)
	{
		if (ALIGN(s_uNextFreeGpuOffset, uAlign) + xRequirements.size >= g_uGpuPoolSize)
		{
			HandleGpuOutOfMemory();
		}

		xDevice.bindBufferMemory(xBufferOut.GetBuffer(), s_xGPUMemory, ALIGN(s_uNextFreeGpuOffset, uAlign));

		MemoryAllocation xAllocation = { ALLOCATION_TYPE_BUFFER, xRequirements.size, ALIGN(s_uNextFreeGpuOffset, uAlign) };
		s_xGpuAllocationMap.insert({ &xBufferOut, xAllocation });
		s_uNextFreeGpuOffset = ALIGN(s_uNextFreeGpuOffset, uAlign) + xRequirements.size;
	}
}

void Zenith_Vulkan_MemoryManager::InitialiseVertexBuffer(const void* pData, size_t uSize, Flux_VertexBuffer& xBufferOut)
{
	Zenith_Vulkan_Buffer& xBuffer = xBufferOut.GetBuffer();
	vk::BufferUsageFlags eFlags = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
	AllocateBuffer(uSize, eFlags, MEMORY_RESIDENCY_GPU, xBuffer);
	UploadData(&xBuffer, pData, uSize);
}

void Zenith_Vulkan_MemoryManager::InitialiseIndexBuffer(const void* pData, size_t uSize, Flux_IndexBuffer& xBufferOut)
{
	Zenith_Vulkan_Buffer& xBuffer = xBufferOut.GetBuffer();
	vk::BufferUsageFlags eFlags = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst;
	AllocateBuffer(uSize, eFlags, MEMORY_RESIDENCY_GPU, xBuffer);
	UploadData(&xBuffer, pData, uSize);
}

void Zenith_Vulkan_MemoryManager::InitialiseConstantBuffer(const void* pData, size_t uSize, Flux_ConstantBuffer& xBufferOut)
{
	Zenith_Vulkan_Buffer& xBuffer = xBufferOut.GetBuffer();
	vk::BufferUsageFlags eFlags = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst;
	AllocateBuffer(uSize, eFlags, MEMORY_RESIDENCY_CPU, xBuffer);
	if (pData)
	{
		UploadData(&xBuffer, pData, uSize);
	}
}

void Zenith_Vulkan_MemoryManager::CreateColourAttachment(uint32_t uWidth, uint32_t uHeight, ColourFormat eFormat, uint32_t uBitsPerPixel, Zenith_Vulkan_Texture& xTextureOut)
{
	FreeTexture(&xTextureOut);
	AllocateTexture(uWidth, uHeight, eFormat, DEPTHSTENCIL_FORMAT_NONE, uBitsPerPixel, 1 /* #TO_TODO: mips */, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled, MEMORY_RESIDENCY_GPU, xTextureOut);
	ImageTransitionBarrier(xTextureOut.GetImage(), vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands);
}
void Zenith_Vulkan_MemoryManager::CreateDepthStencilAttachment(uint32_t uWidth, uint32_t uHeight, DepthStencilFormat eFormat, uint32_t uBitsPerPixel, Zenith_Vulkan_Texture& xTextureOut)
{
	FreeTexture(&xTextureOut);
	AllocateTexture(uWidth, uHeight, COLOUR_FORMAT_NONE, eFormat, uBitsPerPixel, 1, vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled, MEMORY_RESIDENCY_GPU, xTextureOut);
	Zenith_Assert(eFormat == DEPTHSTENCIL_FORMAT_D32_SFLOAT, "#TO_TODO: layouts for just depth without stencil");
	ImageTransitionBarrier(xTextureOut.GetImage(), vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ImageAspectFlagBits::eDepth, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands);
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
	eFormat = vk::Format::eR8G8B8A8Unorm;
	//eFormat = StringToVkFormat(strFormat);

	size_t ulDataSize = uWidth * uHeight * uDepth * 4 /*bytes per pixel*/;
	pData = malloc(ulDataSize);
	memcpy(pData, pcData + ulCursor, ulDataSize);

	delete[] pcData;

	//#TO_TODO: other formats
	AllocateTexture(uWidth, uHeight, COLOUR_FORMAT_BGRA8_UNORM, DEPTHSTENCIL_FORMAT_NONE, 4 /*bytes per pizel*/, 1 /*num mips*/, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc, MEMORY_RESIDENCY_GPU, xTextureOut);
	xTextureOut.SetWidth(uWidth);
	xTextureOut.SetHeight(uHeight);
	xTextureOut.SetNumMips(1);
	UploadData(&xTextureOut, pData, ulDataSize);
	delete pData;
}

void Zenith_Vulkan_MemoryManager::AllocateTexture(uint32_t uWidth, uint32_t uHeight, ColourFormat eColourFormat, DepthStencilFormat eDepthStencilFormat, uint32_t uBitsPerPixel, uint32_t uNumMips, vk::ImageUsageFlags eUsageFlags, MemoryResidency eResidency, Zenith_Vulkan_Texture& xTextureOut)
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
		.setArrayLayers(1)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setUsage(eUsageFlags)
		.setSharingMode(vk::SharingMode::eExclusive)
		.setSamples(vk::SampleCountFlagBits::e1);

	xTextureOut.SetImage(xDevice.createImage(xImageInfo));

	vk::MemoryRequirements xRequirements = xDevice.getImageMemoryRequirements(xTextureOut.GetImage());
	uint32_t uAlign = xRequirements.alignment;

	if (eResidency == MEMORY_RESIDENCY_CPU)
	{
		if (s_uNextFreeCpuOffset + xRequirements.size >= g_uCpuPoolSize)
		{
			HandleCpuOutOfMemory();
		}

		xDevice.bindImageMemory(xTextureOut.GetImage(), s_xCPUMemory, s_uNextFreeCpuOffset);

		MemoryAllocation xAllocation = { ALLOCATION_TYPE_TEXTURE, xRequirements.size, s_uNextFreeCpuOffset };
		s_xCpuAllocationMap.insert({ &xTextureOut, xAllocation });
		s_uNextFreeCpuOffset += xRequirements.size;
	}
	else if (eResidency == MEMORY_RESIDENCY_GPU)
	{
		if (ALIGN(s_uNextFreeGpuOffset, uAlign) + xRequirements.size >= g_uGpuPoolSize)
		{
			HandleGpuOutOfMemory();
		}

		xDevice.bindImageMemory(xTextureOut.GetImage(), s_xGPUMemory, ALIGN(s_uNextFreeGpuOffset, uAlign));

		MemoryAllocation xAllocation = { ALLOCATION_TYPE_TEXTURE, xRequirements.size, ALIGN(s_uNextFreeGpuOffset, uAlign) };
		s_xGpuAllocationMap.insert({ &xTextureOut, xAllocation });
		s_uNextFreeGpuOffset = ALIGN(s_uNextFreeGpuOffset, uAlign) + xRequirements.size;
	}

	vk::ImageSubresourceRange xSubresourceRange = vk::ImageSubresourceRange()
		.setAspectMask(bIsDepth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor)
		.setBaseMipLevel(0)
		.setLevelCount(1)
		.setBaseArrayLayer(0)
		.setLayerCount(1);

	vk::ImageViewCreateInfo xViewCreate = vk::ImageViewCreateInfo()
		.setImage(xTextureOut.GetImage())
		.setViewType(vk::ImageViewType::e2D)
		.setFormat(xFormat)
		.setSubresourceRange(xSubresourceRange);

	xTextureOut.SetImageView(xDevice.createImageView(xViewCreate));
}

void Zenith_Vulkan_MemoryManager::FreeTexture(Zenith_Vulkan_Texture* pxTexture)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	xDevice.destroyImageView(pxTexture->GetImageView());
	xDevice.destroyImage(pxTexture->GetImage());

	auto xCpuIt = s_xCpuAllocationMap.find(pxTexture);
	auto xGpuIt = s_xGpuAllocationMap.find(pxTexture);
	if (xCpuIt != s_xCpuAllocationMap.end())
	{
		Zenith_Assert(s_xGpuAllocationMap.find(pxTexture) == s_xGpuAllocationMap.end(), "This allocation has somehow become a CPU and GPU allocation???");
		s_xCpuAllocationMap.erase(pxTexture);
	}
	else if (xGpuIt != s_xGpuAllocationMap.end())
	{
		Zenith_Assert(s_xCpuAllocationMap.find(pxTexture) == s_xCpuAllocationMap.end(), "This allocation has somehow become a CPU and GPU allocation???");
		s_xGpuAllocationMap.erase(pxTexture);
	}
}

void Zenith_Vulkan_MemoryManager::FlushStagingBuffer() {
	for (auto it = s_xStagingAllocations.begin(); it != s_xStagingAllocations.end(); it++) {
		StagingMemoryAllocation& xAlloc = *it;
		if (xAlloc.m_eType == ALLOCATION_TYPE_BUFFER) {
			Zenith_Vulkan_Buffer* pxVkBuffer = reinterpret_cast<Zenith_Vulkan_Buffer*>(xAlloc.m_pAllocation);
			s_xCommandBuffer.CopyBufferToBuffer(s_pxStagingBuffer, pxVkBuffer, xAlloc.m_uSize, xAlloc.m_uOffset);
		}
		else if (xAlloc.m_eType == ALLOCATION_TYPE_TEXTURE) {
			Zenith_Vulkan_Texture* pxTexture = reinterpret_cast<Zenith_Vulkan_Texture*>(xAlloc.m_pAllocation);

			for (uint32_t i = 0; i < pxTexture->GetNumMips(); i++)
			{
				s_xCommandBuffer.ImageTransitionBarrier(pxTexture->GetImage(), vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, i, 0);
			}

			s_xCommandBuffer.CopyBufferToTexture(s_pxStagingBuffer, pxTexture, xAlloc.m_uOffset);

			s_xCommandBuffer.ImageTransitionBarrier(pxTexture->GetImage(), vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, 0, 0);

			for (uint32_t i = 1; i < pxTexture->GetNumMips(); i++)
			{
				s_xCommandBuffer.BlitTextureToTexture(pxTexture, pxTexture, i);
			}

			s_xCommandBuffer.ImageTransitionBarrier(pxTexture->GetImage(), vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, 0, 0);

			for (uint32_t i = 1; i < pxTexture->GetNumMips(); i++)
			{
				s_xCommandBuffer.ImageTransitionBarrier(pxTexture->GetImage(), vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, i, 0);//todo will probably need to change this to vertex shader in the future
			}
		}
	}

	s_xStagingAllocations.clear();

	s_uNextFreeStagingOffset = 0;
}

void Zenith_Vulkan_MemoryManager::UploadStagingData(AllocationType eType, void* pAllocation, const void* pData, size_t uSize) {

	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	if (s_uNextFreeStagingOffset + uSize >= g_uStagingPoolSize)
	{
		HandleStagingBufferFull();
	}

	StagingMemoryAllocation xAllocation = { eType, pAllocation, uSize, s_uNextFreeStagingOffset };
	s_xStagingAllocations.push_back(xAllocation);

	void* pMap = xDevice.mapMemory(s_pxStagingBuffer->GetDeviceMemory(), s_uNextFreeStagingOffset, uSize);
	memcpy(pMap, pData, uSize);
	xDevice.unmapMemory(s_pxStagingBuffer->GetDeviceMemory());
	s_uNextFreeStagingOffset += uSize;
}

void Zenith_Vulkan_MemoryManager::UploadData(void* pAllocation, const void* pData, size_t uSize)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	auto xCpuIt = s_xCpuAllocationMap.find(pAllocation);
	auto xGpuIt = s_xGpuAllocationMap.find(pAllocation);
	Zenith_Assert(pData, "Invalid data");
	if (xCpuIt != s_xCpuAllocationMap.end())
	{
		Zenith_Assert(s_xGpuAllocationMap.find(pAllocation) == s_xGpuAllocationMap.end(), "This allocation has somehow become a CPU and GPU allocation???");
		MemoryAllocation xAlloc = xCpuIt->second;
		void* pMap = xDevice.mapMemory(s_xCPUMemory, xAlloc.m_uOffset, xAlloc.m_uSize);
		memcpy(pMap, pData, uSize);
		xDevice.unmapMemory(s_xCPUMemory);
	}
	else if (xGpuIt != s_xGpuAllocationMap.end())
	{
		Zenith_Assert(s_xCpuAllocationMap.find(pAllocation) == s_xCpuAllocationMap.end(), "This allocation has somehow become a CPU and GPU allocation???");
		MemoryAllocation xAlloc = xGpuIt->second;
		


		if (xAlloc.m_eType == ALLOCATION_TYPE_BUFFER)
		{
			UploadStagingData(ALLOCATION_TYPE_BUFFER, pAllocation, pData, uSize);
			
		}
		else if (xAlloc.m_eType == ALLOCATION_TYPE_TEXTURE)
		{
			UploadStagingData(ALLOCATION_TYPE_TEXTURE, pAllocation, pData, uSize);
			

		}
	}
	else
	{
		Zenith_Assert(false, "This allocation didn't go through the memory manager");
	}
}

void Zenith_Vulkan_MemoryManager::ClearStagingBuffer()
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	void* pMap = xDevice.mapMemory(s_pxStagingBuffer->GetDeviceMemory(), 0, g_uStagingPoolSize);
	memset(pMap, 0u, g_uStagingPoolSize);
	xDevice.unmapMemory(s_pxStagingBuffer->GetDeviceMemory());
}

bool Zenith_Vulkan_MemoryManager::MemoryWasAllocated(void* pAllocation)
{
	return s_xCpuAllocationMap.find(pAllocation) != s_xCpuAllocationMap.end() || s_xGpuAllocationMap.find(pAllocation) != s_xGpuAllocationMap.end();
}

void Zenith_Vulkan_MemoryManager::HandleCpuOutOfMemory() {
	Zenith_Assert(false, "Implement me");
}

void Zenith_Vulkan_MemoryManager::HandleGpuOutOfMemory() {
	Zenith_Assert(false, "Implement me");
}

void Zenith_Vulkan_MemoryManager::HandleStagingBufferFull() {
	Zenith_Assert(false, "Implement me");
}
