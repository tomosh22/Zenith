#pragma once
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "vulkan/vulkan.hpp"
#include "vma/vk_mem_alloc.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

class Zenith_Vulkan_Buffer
{
public:
	Zenith_Vulkan_Buffer() = default;
	Zenith_Vulkan_Buffer(const Zenith_Vulkan_Buffer& other) = delete;
	~Zenith_Vulkan_Buffer()
	{
		Reset();
	}

	void Reset();

	const vk::Buffer GetBuffer() const { return m_xBuffer; }
	VkBuffer* GetBuffer_Ptr() { return &m_xBuffer; }
	const uint64_t GetSize() const { return m_ulSize; }
	const VmaAllocation& GetAllocation() const { return m_xAllocation; }
	VmaAllocation* GetAllocation_Ptr() { return &m_xAllocation; }
	VmaAllocationInfo* GetAllocationInfo_Ptr() { return &m_xAllocationInfo; }

	void SetSize(const uint64_t ulSize) { m_ulSize = ulSize; }
	void SetBuffer(const vk::Buffer xBuffer) { m_xBuffer = xBuffer; }
	void SetAllocation(const VmaAllocation& xAlloc) { m_xAllocation = xAlloc; }

	bool IsValid() const
	{
		return m_xBuffer != VK_NULL_HANDLE;
	}

private:
	//#TO native type to support vma
	vk::Buffer::NativeType m_xBuffer = VK_NULL_HANDLE;
	VmaAllocation m_xAllocation;
	VmaAllocationInfo m_xAllocationInfo;
	uint64_t m_ulSize = 0;
};
