#pragma once
#include "vulkan/vulkan.hpp"


class Zenith_Vulkan_Texture;

class Zenith_Vulkan_Buffer
{
public:
	Zenith_Vulkan_Buffer() = default;
	Zenith_Vulkan_Buffer(const Zenith_Vulkan_Buffer& other) = delete;
	~Zenith_Vulkan_Buffer();

	Zenith_Vulkan_Buffer(vk::DeviceSize uSize, vk::BufferUsageFlags eUsageFlags, vk::MemoryPropertyFlags eMemProperties);

	void UploadData(void* pData, uint32_t uSize);

	static void CopyBufferToBuffer(Zenith_Vulkan_Buffer* pxSrc, Zenith_Vulkan_Buffer* pxDst, size_t uSize);
	static void CopyBufferToImage(Zenith_Vulkan_Buffer* pxSrc, Zenith_Vulkan_Texture* pxDst, bool bAsyncLoader = false);

	
private:
	vk::Buffer m_xBuffer;
	vk::DeviceMemory m_xDeviceMem;
	uint64_t m_ulSize;
};


