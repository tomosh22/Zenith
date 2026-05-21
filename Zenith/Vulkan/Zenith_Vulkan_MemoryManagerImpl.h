#pragma once

#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include "Vulkan/Zenith_Vulkan_CommandBuffer.h"
#include "Collections/Zenith_Vector.h"
#include "Core/Multithreading/Zenith_MultithreadingImpl.h"

// Phase 6b: per-Engine state for the Vulkan memory manager. Replaces
// the file-static members on Zenith_Vulkan_MemoryManager (VMA allocator,
// staging command buffer, per-frame staging slots, deferred-deletion
// queue, memory-accounting counters, image-view / buffer-descriptor
// handle registries + freelists, mutex).
class Zenith_Vulkan_MemoryManagerImpl
{
public:
	Zenith_Vulkan_MemoryManagerImpl() = default;
	~Zenith_Vulkan_MemoryManagerImpl() = default;

	Zenith_Vulkan_MemoryManagerImpl(const Zenith_Vulkan_MemoryManagerImpl&) = delete;
	Zenith_Vulkan_MemoryManagerImpl& operator=(const Zenith_Vulkan_MemoryManagerImpl&) = delete;

	// Single-buffer-spanning command buffer used to flush staging uploads.
	// The wrapper's per-frame VkCommandBuffer resolves via BeginRecording.
	Zenith_Vulkan_CommandBuffer m_xCommandBuffer;

	// VMA allocator instance.
	VmaAllocator                m_xAllocator = nullptr;

	// Per-frame staging slots (buffer offset, allocations, etc.).
	Zenith_Vulkan_MemoryManager::PerFrameStaging m_axStaging[MAX_FRAMES_IN_FLIGHT];

	// Deferred-deletion queue. Drained per-frame as the deletion clock ticks.
	Zenith_Vector<Zenith_Vulkan_MemoryManager::PendingVRAMDeletion> m_xPendingDeletions;

	// Memory accounting (read by the editor memory panel).
	u_int64                     m_ulImageMemoryUsed  = 0;
	u_int64                     m_ulBufferMemoryUsed = 0;
	u_int64                     m_ulMemoryUsed       = 0;

	// Mutex guarding the staging ring against worker-thread upload races.
	Zenith_Mutex                m_xMutex;

	// Handle registries -- abstract Vulkan types behind opaque handle ints
	// so engine code stays portable across backends.
	Zenith_Vector<vk::ImageView>            m_xImageViewRegistry;
	Zenith_Vector<u_int>                    m_xFreeImageViewHandles;
	Zenith_Vector<vk::DescriptorBufferInfo> m_xBufferDescriptorRegistry;
	Zenith_Vector<u_int>                    m_xFreeBufferDescHandles;
};
