#include "Zenith.h"

#include "Zenith_Vulkan_Swapchain.h"

#include "Zenith_Vulkan.h"
#include "Flux/Zenith_Flux_Enums.h"
#include "Maths/Zenith_Maths.h"
#include "Zenith_Vulkan_MemoryManager.h"

#include "Zenith_OS_Include.h" //#TO for Zenith_Window
#ifdef ZENITH_WINDOWS
#include "Zenith_Windows_Window.h"
#endif

vk::SwapchainKHR Zenith_Vulkan_Swapchain::s_xSwapChain;
std::vector<vk::Image> Zenith_Vulkan_Swapchain::s_xImages;
vk::Format Zenith_Vulkan_Swapchain::s_xImageFormat;
vk::Extent2D Zenith_Vulkan_Swapchain::s_xExtent;

static struct SwapChainSupportDetails {
	vk::SurfaceCapabilitiesKHR m_xCapabilities;
	std::vector <vk::SurfaceFormatKHR> m_xFormats;
	std::vector <vk::PresentModeKHR> m_xPresentModes;
};

static SwapChainSupportDetails QuerySwapChainSupport()
{
	const vk::PhysicalDevice& xPhysicalDevice = Zenith_Vulkan::GetPhysicalDevice();
	const vk::SurfaceKHR& xSurface = Zenith_Vulkan::GetSurface();

	SwapChainSupportDetails xDetails;
	xDetails.m_xCapabilities = xPhysicalDevice.getSurfaceCapabilitiesKHR(xSurface);

	xDetails.m_xFormats = xPhysicalDevice.getSurfaceFormatsKHR(xSurface);

	xDetails.m_xPresentModes = xPhysicalDevice.getSurfacePresentModesKHR(xSurface);

	return xDetails;
}

static vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& xAvailableFormats)
{
	for (const vk::SurfaceFormatKHR& xFormat : xAvailableFormats)
	{
		if (xFormat.format == vk::Format::eB8G8R8A8Srgb && xFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
		{
			return xFormat;
		}
	}
	Zenith_Assert(false, "b8g8r8a8_srgb not supported");
}

static vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& xCapabilities)
{
	if (xCapabilities.currentExtent.width != std::numeric_limits <uint32_t>::max()) return xCapabilities.currentExtent;
	int32_t iExtentWidth, iExtentHeight;
	GLFWwindow* pxWindow = Zenith_Window::GetInstance()->GetNativeWindow();
	glfwGetFramebufferSize(pxWindow, &iExtentWidth, &iExtentHeight);
	vk::Extent2D xExtent = { static_cast<uint32_t>(iExtentWidth),static_cast<uint32_t>(iExtentHeight) };
	xExtent.width = Zenith_Maths::Clamp(xExtent.width, xCapabilities.minImageExtent.width, xCapabilities.maxImageExtent.width);
	xExtent.height = Zenith_Maths::Clamp(xExtent.height, xCapabilities.minImageExtent.height, xCapabilities.maxImageExtent.height);
	return xExtent;
}

static vk::PresentModeKHR ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& xAvailablePresentModes)
{
	for (const vk::PresentModeKHR& eMode : xAvailablePresentModes) {
		if (eMode == vk::PresentModeKHR::eFifo)
		{
			return eMode;
		}
	}
	Zenith_Assert(false, "fifo not supported");
}

void Zenith_Vulkan_Swapchain::Initialise()
{
	const vk::SurfaceKHR& xSurface = Zenith_Vulkan::GetSurface();

	SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport();
	vk::SurfaceFormatKHR xSurfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.m_xFormats);
	vk::PresentModeKHR ePresentMode = ChooseSwapPresentMode(swapChainSupport.m_xPresentModes);
	vk::Extent2D xExtent = ChooseSwapExtent(swapChainSupport.m_xCapabilities);

	//do i need to + 1 here?
	Zenith_Assert(MAX_FRAMES_IN_FLIGHT >= swapChainSupport.m_xCapabilities.minImageCount, "Not enough frames in flight");
	uint32_t uImageCount = MAX_FRAMES_IN_FLIGHT;

	if (swapChainSupport.m_xCapabilities.maxImageCount > 0 && uImageCount > swapChainSupport.m_xCapabilities.maxImageCount)
		uImageCount = swapChainSupport.m_xCapabilities.maxImageCount;
	vk::SwapchainCreateInfoKHR xCreateInfo{};
	xCreateInfo.surface = xSurface;
	xCreateInfo.minImageCount = uImageCount;
	xCreateInfo.imageFormat = xSurfaceFormat.format;
	xCreateInfo.imageColorSpace = xSurfaceFormat.colorSpace;
	xCreateInfo.imageExtent = xExtent;
	xCreateInfo.imageArrayLayers = 1;
	xCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

	uint32_t uGraphicsQueueIdx = Zenith_Vulkan::GetQueueIndex(COMMANDTYPE_GRAPHICS);
	uint32_t uPresentQueueIdx = Zenith_Vulkan::GetQueueIndex(COMMANDTYPE_GRAPHICS);
	uint32_t indicesPtr[] = { uGraphicsQueueIdx,uPresentQueueIdx };
	if (uGraphicsQueueIdx != uPresentQueueIdx) {
		xCreateInfo.imageSharingMode = vk::SharingMode::eConcurrent;
		xCreateInfo.queueFamilyIndexCount = 2;
		xCreateInfo.pQueueFamilyIndices = indicesPtr;
	}
	else {
		xCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
		xCreateInfo.queueFamilyIndexCount = 0;
		xCreateInfo.pQueueFamilyIndices = nullptr;
	}
	xCreateInfo.preTransform = swapChainSupport.m_xCapabilities.currentTransform;
	xCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	xCreateInfo.presentMode = ePresentMode;
	xCreateInfo.clipped = VK_TRUE;
	xCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	s_xSwapChain = xDevice.createSwapchainKHR(xCreateInfo);

	xDevice.getSwapchainImagesKHR(s_xSwapChain, &uImageCount, nullptr);
	s_xImages.resize(uImageCount);
	xDevice.getSwapchainImagesKHR(s_xSwapChain, &uImageCount, s_xImages.data());

	
	for (vk::Image& xImage : s_xImages)
	{
		Zenith_Vulkan_MemoryManager::ImageTransitionBarrier(xImage, vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands);
	}
	
	s_xImageFormat = xSurfaceFormat.format;
	s_xExtent = xExtent;

	Zenith_Log("Vulkan swapchain initialised");
}
