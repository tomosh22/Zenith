#include "Zenith.h"

#include "Zenith_Vulkan_Swapchain.h"

#include "Zenith_Vulkan.h"
#include "Flux/Flux_Enums.h"
#include "Flux/Flux_RenderTargets.h"
#include "Maths/Zenith_Maths.h"
#include "Zenith_Vulkan_MemoryManager.h"

#include "Zenith_OS_Include.h" //#TO for Zenith_Window
#ifdef ZENITH_WINDOWS
#include "Zenith_Windows_Window.h"
#endif

vk::SwapchainKHR Zenith_Vulkan_Swapchain::s_xSwapChain;
std::vector<vk::Image> Zenith_Vulkan_Swapchain::s_xImages;
std::vector<vk::ImageView> Zenith_Vulkan_Swapchain::s_xImageViews;
vk::Format Zenith_Vulkan_Swapchain::s_xImageFormat;
vk::Extent2D Zenith_Vulkan_Swapchain::s_xExtent;
uint32_t Zenith_Vulkan_Swapchain::s_uCurrentImageIndex = 0;
vk::Semaphore Zenith_Vulkan_Swapchain::s_axImageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
vk::Semaphore Zenith_Vulkan_Swapchain::s_axRenderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
vk::Fence Zenith_Vulkan_Swapchain::s_axInFlightFences[MAX_FRAMES_IN_FLIGHT];
uint32_t Zenith_Vulkan_Swapchain::s_uFrameIndex = 0;
Flux_TargetSetup Zenith_Vulkan_Swapchain::s_xTargetSetup;

static struct SwapChainSupportDetails
{
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
	if (xCapabilities.currentExtent.width != std::numeric_limits <uint32_t>::max())
	{
		return xCapabilities.currentExtent;
	}
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
	for (const vk::PresentModeKHR& eMode : xAvailablePresentModes)
	{
		if (eMode == vk::PresentModeKHR::eFifo)
		{
			return eMode;
		}
	}
	Zenith_Assert(false, "fifo not supported");
}

Zenith_Vulkan_Swapchain::~Zenith_Vulkan_Swapchain()

{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		xDevice.destroySemaphore(s_axImageAvailableSemaphores[i], nullptr);
		xDevice.destroySemaphore(s_axRenderFinishedSemaphores[i], nullptr);
		xDevice.destroyFence(s_axInFlightFences[i], nullptr);
	}
}

void Zenith_Vulkan_Swapchain::Initialise()
{
	const vk::SurfaceKHR& xSurface = Zenith_Vulkan::GetSurface();

	SwapChainSupportDetails xSwapChainSupport = QuerySwapChainSupport();
	vk::SurfaceFormatKHR xSurfaceFormat = ChooseSwapSurfaceFormat(xSwapChainSupport.m_xFormats);
	vk::PresentModeKHR ePresentMode = ChooseSwapPresentMode(xSwapChainSupport.m_xPresentModes);
	vk::Extent2D xExtent = ChooseSwapExtent(xSwapChainSupport.m_xCapabilities);

	Zenith_Assert(MAX_FRAMES_IN_FLIGHT >= xSwapChainSupport.m_xCapabilities.minImageCount, "Not enough frames in flight");
	uint32_t uImageCount = MAX_FRAMES_IN_FLIGHT;

	if (xSwapChainSupport.m_xCapabilities.maxImageCount > 0 && uImageCount > xSwapChainSupport.m_xCapabilities.maxImageCount)
	{
		uImageCount = xSwapChainSupport.m_xCapabilities.maxImageCount;
	}
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
	if (uGraphicsQueueIdx != uPresentQueueIdx)
	{
		xCreateInfo.imageSharingMode = vk::SharingMode::eConcurrent;
		xCreateInfo.queueFamilyIndexCount = 2;
		xCreateInfo.pQueueFamilyIndices = indicesPtr;
	}
	else
	{
		xCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
		xCreateInfo.queueFamilyIndexCount = 0;
		xCreateInfo.pQueueFamilyIndices = nullptr;
	}
	xCreateInfo.preTransform = xSwapChainSupport.m_xCapabilities.currentTransform;
	xCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	xCreateInfo.presentMode = ePresentMode;
	xCreateInfo.clipped = VK_TRUE;
	xCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	s_xSwapChain = xDevice.createSwapchainKHR(xCreateInfo);

	xDevice.getSwapchainImagesKHR(s_xSwapChain, &uImageCount, nullptr);
	s_xImages.resize(uImageCount);
	s_xImageViews.resize(uImageCount);
	xDevice.getSwapchainImagesKHR(s_xSwapChain, &uImageCount, s_xImages.data());

	Zenith_Assert(uImageCount == MAX_FRAMES_IN_FLIGHT, "Swapchain has wrong number of images");
	
	for (uint32_t i = 0; i < s_xImages.size(); i++)
	{
		vk::Image& xImage = s_xImages[i];
		Zenith_Vulkan_MemoryManager::ImageTransitionBarrier(xImage, vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands);

		vk::ImageSubresourceRange xSubresourceRange = vk::ImageSubresourceRange()
			.setAspectMask(vk::ImageAspectFlagBits::eColor)
			.setBaseMipLevel(0)
			.setLevelCount(1)
			.setBaseArrayLayer(0)
			.setLayerCount(1);

		vk::ImageViewCreateInfo xViewCreate = vk::ImageViewCreateInfo()
			.setImage(xImage)
			.setViewType(vk::ImageViewType::e2D)
			.setFormat(xSurfaceFormat.format)
			.setSubresourceRange(xSubresourceRange);

		s_xImageViews[i] = xDevice.createImageView(xViewCreate);

		s_xTargetSetup.m_axColourAttachments[0].m_axTargetTextures[i].SetImage(xImage);
		s_xTargetSetup.m_axColourAttachments[0].m_axTargetTextures[i].SetImageView(s_xImageViews[i]);
	}
	s_xTargetSetup.m_axColourAttachments[0].m_uWidth = xExtent.width;
	s_xTargetSetup.m_axColourAttachments[0].m_uHeight = xExtent.height;
	//#TO_TODO: stop hardcoding swapchain colour format
	s_xTargetSetup.m_axColourAttachments[0].m_eColourFormat = COLOUR_FORMAT_BGRA8_SRGB;
	
	s_xImageFormat = xSurfaceFormat.format;
	s_xExtent = xExtent;

	vk::SemaphoreCreateInfo xSemaphoreInfo;

	vk::FenceCreateInfo xFenceInfo = vk::FenceCreateInfo()
		.setFlags(vk::FenceCreateFlagBits::eSignaled);

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		s_axImageAvailableSemaphores[i] = xDevice.createSemaphore(xSemaphoreInfo);
		s_axRenderFinishedSemaphores[i] = xDevice.createSemaphore(xSemaphoreInfo);
		s_axInFlightFences[i] = xDevice.createFence(xFenceInfo);
	}

	Zenith_Log("Vulkan swapchain initialised");
}

void Zenith_Vulkan_Swapchain::BeginFrame()
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	uint32_t uPreviousFrame = (s_uFrameIndex - 1) % MAX_FRAMES_IN_FLIGHT;
	xDevice.waitForFences(1, &s_axInFlightFences[uPreviousFrame], VK_TRUE, UINT64_MAX);

#ifdef ZENITH_ASSERT
	vk::Result eResult =
#endif
	 xDevice.acquireNextImageKHR(s_xSwapChain, UINT64_MAX, s_axImageAvailableSemaphores[s_uFrameIndex], nullptr, &s_uCurrentImageIndex);

	Zenith_Assert(eResult == vk::Result::eSuccess, "Failed to acquire swapchain image");

	xDevice.resetFences(1, &s_axInFlightFences[s_uFrameIndex]);
}

void Zenith_Vulkan_Swapchain::EndFrame()
{
	vk::PresentInfoKHR presentInfo = vk::PresentInfoKHR()
		.setSwapchainCount(1)
		.setPSwapchains(&s_xSwapChain)
		.setPImageIndices(&s_uCurrentImageIndex)
		.setWaitSemaphoreCount(1).
		setPWaitSemaphores(&s_axRenderFinishedSemaphores[s_uFrameIndex]);

#ifdef ZENITH_ASSERT
	vk::Result eResult =
#endif
		Zenith_Vulkan::GetQueue(COMMANDTYPE_PRESENT).presentKHR(&presentInfo);

	Zenith_Assert(eResult == vk::Result::eSuccess || eResult == vk::Result::eErrorOutOfDateKHR || eResult == vk::Result::eSuboptimalKHR, "Failed to present");

	s_uFrameIndex = (s_uFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

vk::Semaphore& Zenith_Vulkan_Swapchain::GetCurrentImageAvailableSemaphore()
{
	return s_axImageAvailableSemaphores[s_uFrameIndex];
}

vk::Semaphore& Zenith_Vulkan_Swapchain::GetCurrentRenderCompleteSemaphore()
{
	return s_axRenderFinishedSemaphores[s_uFrameIndex];
}

vk::Fence& Zenith_Vulkan_Swapchain::GetCurrentInFlightFence()
{
	return s_axInFlightFences[s_uFrameIndex];
}
