#include "Zenith.h"

#include "Zenith_Vulkan.h"
#include "Zenith_Vulkan_Buffer.h"
#include "Zenith_Vulkan_MemoryManager.h"

void Zenith_Vulkan_Buffer::Reset()
{
	// Clear handles to mark buffer as invalid
	// Note: Actual GPU memory cleanup is handled by Zenith_Vulkan_MemoryManager
	// through the deferred deletion system (QueueVRAMDeletion)
	m_xBuffer = VK_NULL_HANDLE;
	m_xAllocation = VK_NULL_HANDLE;
	m_ulSize = 0;
}