#pragma once

#include "Vulkan/Zenith_Vulkan_Swapchain.h"
#include "Flux/Flux_RenderTargets.h"
#include "Core/ZenithConfig.h"
#include <vector>

// Phase 6b: per-Engine state for the Vulkan swapchain. Replaces the
// file-statics on Zenith_Vulkan_Swapchain (swapchain object, image
// arrays, format/extent, current image index, per-frame semaphores,
// cached colour attachments).
class Zenith_Vulkan_SwapchainImpl
{
public:
	Zenith_Vulkan_SwapchainImpl() = default;
	~Zenith_Vulkan_SwapchainImpl() = default;

	Zenith_Vulkan_SwapchainImpl(const Zenith_Vulkan_SwapchainImpl&) = delete;
	Zenith_Vulkan_SwapchainImpl& operator=(const Zenith_Vulkan_SwapchainImpl&) = delete;

	vk::SwapchainKHR             m_xSwapChain;
	std::vector<vk::Image>       m_xImages;
	std::vector<vk::ImageView>   m_xImageViews;
	vk::Format                   m_xImageFormat = vk::Format::eUndefined;
	vk::Extent2D                 m_xExtent = {};

	uint32_t                     m_uCurrentImageIndex = 0;

	vk::Semaphore                m_axImageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
	vk::Semaphore                m_axRenderFinishedSemaphores [MAX_FRAMES_IN_FLIGHT];

	Flux_RenderAttachment        m_axColourAttachments[MAX_FRAMES_IN_FLIGHT];

	bool                         m_bShouldWaitOnImageAvailableSem = false;
};
