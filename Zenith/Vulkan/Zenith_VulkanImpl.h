#pragma once

#include "Vulkan/Zenith_Vulkan.h"
#include <vector>

// Phase 6b: per-Engine state for the Vulkan backend's main class.
// Replaces the ~22 file-static data members previously declared on
// Zenith_Vulkan (instance / device / queues / command pools / per-frame
// rings / VRAM registry / pending command buffers / ImGui resources).
//
// Static facade methods on Zenith_Vulkan keep their signatures; bodies
// reach this Impl via g_xEngine.Vulkan().m_xXxx.
class Zenith_VulkanImpl
{
public:
	Zenith_VulkanImpl() = default;
	~Zenith_VulkanImpl() = default;

	Zenith_VulkanImpl(const Zenith_VulkanImpl&) = delete;
	Zenith_VulkanImpl& operator=(const Zenith_VulkanImpl&) = delete;

	// Instance / surface / device.
	vk::Instance                  m_xInstance;
	vk::DebugUtilsMessengerEXT    m_xDebugMessenger;
#ifdef ZENITH_FLUX_PROFILING
	vk::DispatchLoaderDynamic     m_xDispatchLoader;
#endif
	vk::SurfaceKHR                m_xSurface;
	vk::PhysicalDevice            m_xPhysicalDevice;
	Zenith_Vulkan::GPUCapabilities m_xGPUCapabilties = {};
	uint32_t                      m_auQueueIndices[COMMANDTYPE_MAX] = {};
	vk::Device                    m_xDevice;
	vk::Queue                     m_axQueues[COMMANDTYPE_MAX];
	vk::CommandPool               m_axCommandPools[COMMANDTYPE_MAX];

	// Default + bindless descriptor pools.
	vk::DescriptorPool            m_xDefaultDescriptorPool;
	vk::DescriptorPool            m_xBindlessTexturesDescriptorPool;
	vk::DescriptorSet             m_xBindlessTexturesDescriptorSet;
	vk::DescriptorSetLayout       m_xBindlessTexturesDescriptorSetLayout;

	// VRAM registry + freelist of slots.
	std::vector<Zenith_Vulkan_VRAM*> m_xVRAMRegistry;
	std::vector<uint32_t>            m_xFreeVRAMHandles;

	// Pending command buffers awaiting submission (Phase 2 of graph execute).
	std::vector<const Zenith_Vulkan_CommandBuffer*> m_xPendingCommandBuffers;

	// Per-frame ring state (fences, semaphores, command-pool slots).
	Zenith_Vulkan_PerFrame        m_axPerFrame[MAX_FRAMES_IN_FLIGHT];
	Zenith_Vulkan_PerFrame*       m_pxCurrentFrame = nullptr;

	// Transfer command buffer used for staging-buffer flushes.
	Zenith_Vulkan_CommandBuffer*  m_pxMemoryUpdateCmdBuf = nullptr;

#ifdef ZENITH_TOOLS
	// ImGui integration resources.
	vk::RenderPass                m_xImGuiRenderPass;
	vk::DescriptorPool            m_xImGuiDescriptorPool;
	vk::DescriptorSetLayout       m_xImGuiPreviewLayout;
#endif
};
