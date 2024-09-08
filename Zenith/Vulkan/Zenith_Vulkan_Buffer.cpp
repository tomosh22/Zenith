#include "Zenith.h"

#include "Zenith_Vulkan.h"
#include "Zenith_Vulkan_Buffer.h"
#include "Zenith_Vulkan_MemoryManager.h"

Zenith_Vulkan_Buffer::Zenith_Vulkan_Buffer(vk::DeviceSize uSize, vk::BufferUsageFlags eUsageFlags, vk::MemoryPropertyFlags eMemProperties)
	: m_ulSize(uSize)
{
	vk::BufferCreateInfo xInfo = vk::BufferCreateInfo()
		.setSize(uSize)
		.setUsage(eUsageFlags)
		.setSharingMode(vk::SharingMode::eExclusive);

	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	const vk::PhysicalDevice& xPhysicalDevice = Zenith_Vulkan::GetPhysicalDevice();

	m_xBuffer = xDevice.createBuffer(xInfo);

	vk::MemoryRequirements xRequirements = xDevice.getBufferMemoryRequirements(m_xBuffer);

	uint32_t memoryType = ~0u;
	for (uint32_t i = 0; i < xPhysicalDevice.getMemoryProperties().memoryTypeCount; i++)
	{
		if ((xRequirements.memoryTypeBits & (1 << i)) && (xPhysicalDevice.getMemoryProperties().memoryTypes[i].propertyFlags & eMemProperties) == eMemProperties)
		{
			memoryType = i;
			break;
		}
	}
	Zenith_Assert(memoryType != ~0u, "couldn't find physical memory type");

	vk::MemoryAllocateInfo xAllocInfo = vk::MemoryAllocateInfo()
		.setAllocationSize(ALIGN(xRequirements.size, 4096))
		.setMemoryTypeIndex(memoryType);

#ifdef ZENITH_RAYTRACING
	vk::MemoryAllocateFlagsInfo xFlags = vk::MemoryAllocateFlagsInfo()
		.setFlags(vk::MemoryAllocateFlagBits::eDeviceAddress);
	xAllocInfo.setPNext(&xFlags);
#endif

	m_xDeviceMem = xDevice.allocateMemory(xAllocInfo);
	xDevice.bindBufferMemory(m_xBuffer, m_xDeviceMem, 0);
}

Zenith_Vulkan_Buffer::~Zenith_Vulkan_Buffer()
{
	//#TO_TODO: memory manager should handle this
	//const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	//xDevice.destroyBuffer(m_xBuffer);
	//xDevice.freeMemory(m_xDeviceMem);
}

void Zenith_Vulkan_Buffer::UploadData(void* pData, uint32_t uSize)
{
	STUBBED
		const vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	//void* pMappedPtr;
	////TO_TODO: clean this up once every buffer goes through the memory manager
	//if (dynamic_cast<Zenith_Vulkan_MemoryManager*>(pxRenderer->m_pxMemoryManager)->MemoryWasAllocated(this)) {
	//	dynamic_cast<Zenith_Vulkan_MemoryManager*>(pxRenderer->m_pxMemoryManager)->UploadData(this, pData, uSize);
	//}
	//else {
	//	vkMapMemory(xDevice, m_xDeviceMem, 0, uSize, 0, &pMappedPtr);
	//	memcpy(pMappedPtr, pData, uSize);
	//	vkUnmapMemory(xDevice, m_xDeviceMem);
	//}
}

void Zenith_Vulkan_Buffer::CopyBufferToBuffer(Zenith_Vulkan_Buffer* pxSrc, Zenith_Vulkan_Buffer* pxDst, size_t uSize)
{
	STUBBED
		/*vk::CommandBuffer xCmd = VulkanRenderer::GetInstance()->BeginSingleUseCmdBuffer(COMMANDTYPE_GRAPHICS);

		vk::BufferCopy xCopyRegion(0,0,uSize);
		xCmd.copyBuffer(pxSrc->m_xBuffer, pxDst->m_xBuffer, xCopyRegion);

		VulkanRenderer::GetInstance()->EndSingleUseCmdBuffer(xCmd, COMMANDTYPE_GRAPHICS);*/
}
void Zenith_Vulkan_Buffer::CopyBufferToImage(Zenith_Vulkan_Buffer* pxSrc, Zenith_Vulkan_Texture* pxDst, bool bAsyncLoader /*= false*/)
{
	STUBBED

		/*vk::ImageSubresourceLayers xSubresource = vk::ImageSubresourceLayers()
			.setAspectMask(vk::ImageAspectFlagBits::eColor)
			.setMipLevel(0)
			.setBaseArrayLayer(0)
			.setLayerCount(1);

		vk::BufferImageCopy region = vk::BufferImageCopy()
			.setBufferOffset(0)
			.setBufferRowLength(0)
			.setBufferImageHeight(0)
			.setImageSubresource(xSubresource)
			.setImageOffset({ 0,0,0 })
			.setImageExtent({ pxDst->GetWidth(), pxDst->GetHeight(), 1});

		vk::CommandBuffer xCmd = VulkanRenderer::GetInstance()->BeginSingleUseCmdBuffer(COMMANDTYPE_GRAPHICS);
		xCmd.copyBufferToImage(pxSrc->m_xBuffer, pxDst->m_xImage, vk::ImageLayout::eTransferDstOptimal, 1, &region);
		VulkanRenderer::GetInstance()->EndSingleUseCmdBuffer(xCmd, COMMANDTYPE_GRAPHICS);
		*/
}