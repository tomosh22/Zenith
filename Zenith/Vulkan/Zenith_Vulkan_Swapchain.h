#pragma once
#include "vulkan/vulkan.hpp"

//#TO for MAX_FRAMES_IN_FLIGHT which should really be somewhere else
#include "Flux/Flux_Enums.h"

struct Flux_TargetSetup;

class Zenith_Vulkan_Swapchain
{
public:
	Zenith_Vulkan_Swapchain() {}
	~Zenith_Vulkan_Swapchain();
	static void Initialise();

	static bool BeginFrame();
	static void EndFrame();

	static uint32_t GetWidth() { return s_xExtent.width; }
	static uint32_t GetHeight() { return s_xExtent.height; }

	static vk::Extent2D& GetExent() { return s_xExtent; }

	static vk::Semaphore& GetCurrentImageAvailableSemaphore();
	static vk::Semaphore& GetCurrentRenderCompleteSemaphore();
	static vk::Fence& GetCurrentInFlightFence();

	static uint32_t GetCurrentFrameIndex() { return s_uFrameIndex; }

	static Flux_TargetSetup& GetTargetSetup() { return s_xTargetSetup; }

	static vk::Format GetFormat() { return s_xImageFormat; }

	static void CopyToFramebuffer();
private:
	static void BindAsTarget();
	static vk::SwapchainKHR s_xSwapChain;
	//#TO_TODO: make these arrays, not vectors
	static std::vector<vk::Image> s_xImages;
	static std::vector<vk::ImageView> s_xImageViews;
	static vk::Format s_xImageFormat;
	static vk::Extent2D s_xExtent;

	static vk::Semaphore s_axImageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
	static vk::Semaphore s_axRenderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
	static vk::Fence s_axInFlightFences[MAX_FRAMES_IN_FLIGHT];

	static uint32_t s_uCurrentImageIndex; //set by acquireNextImageKHR
	static uint32_t s_uFrameIndex; //set by us

	static Flux_TargetSetup s_xTargetSetup;
};