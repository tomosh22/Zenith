#pragma once
#include "vulkan/vulkan.hpp"

class Zenith_Vulkan_Swapchain
{
public:
	Zenith_Vulkan_Swapchain() {}
	~Zenith_Vulkan_Swapchain() {}
	static void Initialise();
private:
	static vk::SwapchainKHR s_xSwapChain;
	static std::vector<vk::Image> s_xImages;
	static vk::Format s_xImageFormat;
	static vk::Extent2D s_xExtent;
};