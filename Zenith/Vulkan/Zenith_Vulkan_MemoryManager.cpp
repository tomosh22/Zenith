#include "Zenith.h"

#include "Zenith_Vulkan_MemoryManager.h"
#include "Zenith_Vulkan_CommandBuffer.h"

#include "Zenith_Vulkan.h"
#include "Zenith_Vulkan_Buffer.h"

vk::DeviceMemory Zenith_Vulkan_MemoryManager::s_xCPUMemory;
vk::DeviceMemory Zenith_Vulkan_MemoryManager::s_xGPUMemory;

Zenith_Vulkan_Buffer* Zenith_Vulkan_MemoryManager::s_pxStagingBuffer = nullptr;
Zenith_Vulkan_CommandBuffer* Zenith_Vulkan_MemoryManager::s_pxCommandBuffer = nullptr;

std::unordered_map<void*, Zenith_Vulkan_MemoryManager::MemoryAllocation> Zenith_Vulkan_MemoryManager::s_xCpuAllocationMap;
std::unordered_map<void*, Zenith_Vulkan_MemoryManager::MemoryAllocation> Zenith_Vulkan_MemoryManager::s_xGpuAllocationMap;

size_t Zenith_Vulkan_MemoryManager::s_uNextFreeCpuOffset = 0;
size_t Zenith_Vulkan_MemoryManager::s_uNextFreeGpuOffset = 0;
size_t Zenith_Vulkan_MemoryManager::s_uNextFreeStagingOffset = 0;


void Zenith_Vulkan_MemoryManager::Initialise()
{
	const vk::PhysicalDevice& xPhysDevice = Zenith_Vulkan::GetPhysicalDevice();
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	uint64_t uAlignedCpuSize = ALIGN(g_uCpuPoolSize, 4096);

	uint32_t uCpuMemoryType = ~0u;
	for (uint32_t i = 0; i < xPhysDevice.getMemoryProperties().memoryTypeCount; i++) {
		vk::MemoryType xMemType = xPhysDevice.getMemoryProperties().memoryTypes[i];
		if (xMemType.propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible &&
			xMemType.propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent &&
			xPhysDevice.getMemoryProperties().memoryHeaps[xMemType.heapIndex].size > uAlignedCpuSize
			)
			uCpuMemoryType = i;
	}
	Zenith_Assert(uCpuMemoryType != ~0u, "couldn't find physical memory type");


	vk::MemoryAllocateInfo xCpuAllocInfo = vk::MemoryAllocateInfo()
		.setAllocationSize(uAlignedCpuSize)
		.setMemoryTypeIndex(uCpuMemoryType);

#ifdef VCE_RAYTRACING
	vk::MemoryAllocateFlagsInfo xCpuFlags = vk::MemoryAllocateFlagsInfo()
		.setFlags(vk::MemoryAllocateFlagBits::eDeviceAddress);
	xCpuAllocInfo.setPNext(&xCpuFlags);
#endif

	s_xCPUMemory = xDevice.allocateMemory(xCpuAllocInfo);

	uint64_t uAlignedGpuSize = ALIGN(g_uGpuPoolSize, 4096);

	uint32_t uGpuMemoryType = ~0u;
	for (uint32_t i = 0; i < xPhysDevice.getMemoryProperties().memoryTypeCount; i++) {
		vk::MemoryType xMemType = xPhysDevice.getMemoryProperties().memoryTypes[i];
		if (xMemType.propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal &&
			xPhysDevice.getMemoryProperties().memoryHeaps[xMemType.heapIndex].size > uAlignedGpuSize
			&& 59 & (1 << i) //#TO_TODO: what in the flying fuck is this about???
			)
			uGpuMemoryType = i;
	}
	Zenith_Assert(uGpuMemoryType != ~0u, "couldn't find physical memory type");


	vk::MemoryAllocateInfo xGpuAllocInfo = vk::MemoryAllocateInfo()
		.setAllocationSize(uAlignedGpuSize)
		.setMemoryTypeIndex(uGpuMemoryType);

#ifdef VCE_RAYTRACING
	vk::MemoryAllocateFlagsInfo xGpuFlags = vk::MemoryAllocateFlagsInfo()
		.setFlags(vk::MemoryAllocateFlagBits::eDeviceAddress);
	xGpuAllocInfo.setPNext(&xGpuFlags);
#endif

	s_xGPUMemory = xDevice.allocateMemory(xGpuAllocInfo);

	s_pxStagingBuffer = new Zenith_Vulkan_Buffer(g_uStagingPoolSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

	s_pxCommandBuffer = new Zenith_Vulkan_CommandBuffer;

	Zenith_Log("Vulkan memory manager initialised");
}

void Zenith_Vulkan_MemoryManager::Shutdown()
{
	delete s_pxStagingBuffer;
	delete s_pxCommandBuffer;
}

Zenith_Vulkan_CommandBuffer* Zenith_Vulkan_MemoryManager::GetCommandBuffer() {
	return s_pxCommandBuffer;
}

void Zenith_Vulkan_MemoryManager::BeginFrame()
{
	s_pxCommandBuffer->BeginRecording();

	//#TO_TODO: asset handler
	//AssetHandler* pxAssetHandler = pxApp->m_pxAssetHandler;
	//pxAssetHandler->ProcessPendingDeletes();
}

void Zenith_Vulkan_MemoryManager::EndFrame(bool bDefer /*= true*/)
{
	//#TO_TODO: flush staging buffer
	//FlushStagingBuffer();

	if (bDefer)
	{
		s_pxCommandBuffer->EndRecording(RENDER_ORDER_MEMORY_UPDATE, false);
	}
	else
	{
		s_pxCommandBuffer->EndAndCpuWait(false);
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

	switch (eNewLayout) {
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

	const vk::CommandBuffer& xCmd = s_pxCommandBuffer->GetCurrentCmdBuffer();

	xCmd.pipelineBarrier(eSrcStage, eDstStage, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &xMemoryBarrier);
}

Zenith_Vulkan_Buffer* Zenith_Vulkan_MemoryManager::AllocateBuffer(size_t uSize, vk::BufferUsageFlags eUsageFlags, MemoryResidency eResidency)
{
	STUBBED

		/*Zenith_Vulkan_Buffer* pxRet = new Zenith_Vulkan_Buffer;
		vk::Device& xDevice = pxRenderer->GetDevice();

		vk::BufferCreateInfo xBufferInfo = vk::BufferCreateInfo()
			.setSize(uSize)
			.setUsage(eUsageFlags)
			.setSharingMode(vk::SharingMode::eExclusive);
		pxRet->m_xBuffer = xDevice.createBuffer(xBufferInfo);
		pxRet->SetSize(uSize);

		vk::MemoryRequirements xRequirements = xDevice.getBufferMemoryRequirements(pxRet->m_xBuffer);
		uint32_t uAlign = xRequirements.alignment;
	#ifdef VCE_RAYTRACING
		uAlign = std::max(uAlign, 128u);
	#endif

		if (eResidency == CPU_RESIDENT) {
			if (ALIGN(m_uNextFreeCpuOffset, uAlign) + xRequirements.size >= g_uCpuPoolSize)
				HandleCpuOutOfMemory();

			xDevice.bindBufferMemory(pxRet->m_xBuffer, m_xCPUMemory, ALIGN(m_uNextFreeCpuOffset, uAlign));

			MemoryAllocation xAllocation = { ALLOCATION_TYPE_BUFFER, xRequirements.size, ALIGN(m_uNextFreeCpuOffset, uAlign) };
			m_xCpuAllocationMap.insert({pxRet, xAllocation });
			m_uNextFreeCpuOffset = ALIGN(m_uNextFreeCpuOffset, uAlign) + xRequirements.size;
		}
		else if (eResidency == GPU_RESIDENT) {
			if (ALIGN(m_uNextFreeGpuOffset, uAlign) + xRequirements.size >= g_uGpuPoolSize)
				HandleGpuOutOfMemory();

			xDevice.bindBufferMemory(pxRet->m_xBuffer, m_xGPUMemory, ALIGN(m_uNextFreeGpuOffset, uAlign));

			MemoryAllocation xAllocation = { ALLOCATION_TYPE_BUFFER, xRequirements.size, ALIGN(m_uNextFreeGpuOffset, uAlign) };
			m_xGpuAllocationMap.insert({ pxRet, xAllocation });
			m_uNextFreeGpuOffset = ALIGN(m_uNextFreeGpuOffset, uAlign) + xRequirements.size;
		}

		return pxRet;
		*/
		return nullptr;
}

void Zenith_Vulkan_MemoryManager::AllocateTexture2DMemory(Zenith_Vulkan_Texture* pxTexture, uint32_t uWidth, uint32_t uHeight, TextureFormat eFormat, uint32_t uBitsPerPixel, uint32_t uNumMips, vk::ImageUsageFlags eUsageFlags, MemoryResidency eResidency)
{
	STUBBED

	//vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	//vk::Format xFormat;
	////#TO_TODO: make this a utility function
	//switch (eFormat, uBitsPerPixel) {
	//case (TextureFormat::RGBA, 32):
	//	xFormat = vk::Format::eR8G8B8A8Srgb;
	//	break;
	//default:
	//	VCE_Assert(false, "Unsupported texture format");
	//}

	//vk::ImageCreateInfo xImageInfo = vk::ImageCreateInfo()
	//	.setImageType(vk::ImageType::e2D)
	//	.setFormat(xFormat)
	//	.setTiling(vk::ImageTiling::eOptimal)
	//	.setExtent({ uWidth,uHeight,1 })
	//	.setMipLevels(uNumMips)
	//	.setArrayLayers(1)
	//	.setInitialLayout(vk::ImageLayout::eUndefined)
	//	.setUsage(eUsageFlags)
	//	.setSharingMode(vk::SharingMode::eExclusive)
	//	.setSamples(vk::SampleCountFlagBits::e1);

	//pxTexture->m_xImage = xDevice.createImage(xImageInfo);

	//vk::MemoryRequirements xRequirements = xDevice.getImageMemoryRequirements(pxTexture->m_xImage);
	//uint32_t uAlign = xRequirements.alignment;

	//if (eResidency == CPU_RESIDENT) {
	//	if (m_uNextFreeCpuOffset + xRequirements.size >= g_uCpuPoolSize)
	//		HandleCpuOutOfMemory();

	//	xDevice.bindImageMemory(pxTexture->m_xImage, m_xCPUMemory, m_uNextFreeCpuOffset);

	//	MemoryAllocation xAllocation = { ALLOCATION_TYPE_TEXTURE, xRequirements.size, m_uNextFreeCpuOffset };
	//	m_xCpuAllocationMap.insert({ pxTexture, xAllocation });
	//	m_uNextFreeCpuOffset += xRequirements.size;
	//}
	//else if (eResidency == GPU_RESIDENT) {
	//	if (ALIGN(m_uNextFreeGpuOffset, uAlign) + xRequirements.size >= g_uGpuPoolSize)
	//		HandleGpuOutOfMemory();

	//	xDevice.bindImageMemory(pxTexture->m_xImage, m_xGPUMemory, ALIGN(m_uNextFreeGpuOffset, uAlign));

	//	MemoryAllocation xAllocation = { ALLOCATION_TYPE_TEXTURE, xRequirements.size, ALIGN(m_uNextFreeGpuOffset, uAlign) };
	//	m_xGpuAllocationMap.insert({ pxTexture, xAllocation });
	//	m_uNextFreeGpuOffset = ALIGN(m_uNextFreeGpuOffset, uAlign) + xRequirements.size;
	//}
}

void Zenith_Vulkan_MemoryManager::FreeTexture2DMemory(Zenith_Vulkan_Texture* pxTexture, TextureFormat eFormat, MemoryResidency eResidency)
{
	STUBBED
	/*const vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	xDevice.destroyImage(pxTexture->m_xImage);

	if(eResidency == MemoryResidency::CPU_RESIDENT)
		m_xCpuAllocationMap.erase(pxTexture);
	else if (eResidency == MemoryResidency::GPU_RESIDENT)
		m_xGpuAllocationMap.erase(pxTexture);*/

}

void Zenith_Vulkan_MemoryManager::FlushStagingBuffer() {
	STUBBED
	//for (auto it = m_xStagingAllocations.begin(); it != m_xStagingAllocations.end(); it++) {
	//	StagingMemoryAllocation& xAlloc = *it;
	//	if (xAlloc.m_eType == ALLOCATION_TYPE_BUFFER) {
	//		Zenith_Vulkan_Buffer* pxVkBuffer = reinterpret_cast<Zenith_Vulkan_Buffer*>(xAlloc.m_pAllocation);
	//		m_pxCommandBuffer->CopyBufferToBuffer(m_pxStagingBuffer, pxVkBuffer, xAlloc.m_uSize, xAlloc.m_uOffset);
	//	}
	//	else if (xAlloc.m_eType == ALLOCATION_TYPE_TEXTURE) {
	//		VulkanTexture2D* pxVkTex2D = reinterpret_cast<VulkanTexture2D*>(xAlloc.m_pAllocation);

	//		for (uint32_t i = 0; i < pxVkTex2D->m_uNumMips; i++)
	//			m_pxCommandBuffer->ImageTransitionBarrier(pxVkTex2D->m_xImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, i, 0);

	//		m_pxCommandBuffer->CopyBufferToImage(m_pxStagingBuffer, pxVkTex2D, xAlloc.m_uOffset);

	//		m_pxCommandBuffer->ImageTransitionBarrier(pxVkTex2D->m_xImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, 0, 0);

	//		for (uint32_t i = 1; i < pxVkTex2D->m_uNumMips; i++)
	//			m_pxCommandBuffer->BlitImageToImage(pxVkTex2D, pxVkTex2D, i);

	//		m_pxCommandBuffer->ImageTransitionBarrier(pxVkTex2D->m_xImage, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, 0, 0);

	//		for (uint32_t i = 1; i < pxVkTex2D->m_uNumMips; i++)
	//			m_pxCommandBuffer->ImageTransitionBarrier(pxVkTex2D->m_xImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, i, 0);//todo will probably need to change this to vertex shader in the future
	//	}
	//}

	//m_xStagingAllocations.clear();

	//m_uNextFreeStagingOffset = 0;
}

void Zenith_Vulkan_MemoryManager::UploadStagingData(AllocationType eType, void* pAllocation, void* pData, size_t uSize) {
	STUBBED

	/*vk::Device& xDevice = pxRenderer->GetDevice();

	if (m_uNextFreeStagingOffset + uSize >= g_uStagingPoolSize)
		HandleStagingBufferFull();

	StagingMemoryAllocation xAllocation = { eType, pAllocation, uSize, m_uNextFreeStagingOffset };
	m_xStagingAllocations.push_back(xAllocation);

	void* pMap = xDevice.mapMemory(m_pxStagingBuffer->m_xDeviceMem, m_uNextFreeStagingOffset, uSize);
	memcpy(pMap, pData, uSize);
	xDevice.unmapMemory(m_pxStagingBuffer->m_xDeviceMem);
	m_uNextFreeStagingOffset += uSize;*/
}

void Zenith_Vulkan_MemoryManager::UploadData(void* pAllocation, void* pData, size_t uSize) {
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	STUBBED
	/*auto xCpuIt = m_xCpuAllocationMap.find(pAllocation);
	auto xGpuIt = m_xGpuAllocationMap.find(pAllocation);
	VCE_Assert(pData, "Invalid data");
	if (xCpuIt != m_xCpuAllocationMap.end()) {
		VCE_Assert(m_xGpuAllocationMap.find(pAllocation) == m_xGpuAllocationMap.end(), "This allocation has somehow become a CPU and GPU allocation???");
		MemoryAllocation xAlloc = xCpuIt->second;
		void* pMap = xDevice.mapMemory(m_xCPUMemory, xAlloc.m_uOffset, xAlloc.m_uSize);
		memcpy(pMap, pData, uSize);
		xDevice.unmapMemory(m_xCPUMemory);
	}
	else if (xGpuIt != m_xGpuAllocationMap.end()) {
		VCE_Assert(m_xCpuAllocationMap.find(pAllocation) == m_xCpuAllocationMap.end(), "This allocation has somehow become a CPU and GPU allocation???");
		MemoryAllocation xAlloc = xGpuIt->second;
		


		if (xAlloc.m_eType == ALLOCATION_TYPE_BUFFER) {
			UploadStagingData(ALLOCATION_TYPE_BUFFER, pAllocation, pData, uSize);
			
		}
		else if (xAlloc.m_eType == ALLOCATION_TYPE_TEXTURE) {
			UploadStagingData(ALLOCATION_TYPE_TEXTURE, pAllocation, pData, uSize);
			

		}
	}
	else
		VCE_Assert(false, "This allocation didn't go through the memory manager");*/
}

void Zenith_Vulkan_MemoryManager::ClearStagingBuffer()
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	STUBBED

	//void* pMap = xDevice.mapMemory(m_pxStagingBuffer->m_xDeviceMem, 0, g_uStagingPoolSize);
	//memset(pMap, 0u, g_uStagingPoolSize);
	//xDevice.unmapMemory(m_pxStagingBuffer->m_xDeviceMem);
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
