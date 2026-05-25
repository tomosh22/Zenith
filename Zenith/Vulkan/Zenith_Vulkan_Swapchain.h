#pragma once
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "vulkan/vulkan.hpp"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

//#TO for MAX_FRAMES_IN_FLIGHT which should really be somewhere else
#include "Flux/Flux_Enums.h"

struct Flux_RenderAttachment;

class Zenith_Vulkan_Swapchain
{
public:
	Zenith_Vulkan_Swapchain() {}
	~Zenith_Vulkan_Swapchain();
	static void Initialise();
	static void Shutdown();

	static bool BeginFrame();
	static void EndFrame();

	static uint32_t GetWidth();
	static uint32_t GetHeight();

	static vk::Extent2D& GetExent();

	static vk::Semaphore& GetCurrentImageAvailableSemaphore();

	// Ring index in [0, MAX_FRAMES_IN_FLIGHT). Owned by Flux_PerFrame as the
	// engine's single source of truth for frame counting; this accessor is
	// retained for compatibility but defined out-of-line so the header can
	// stay free of the Flux_PerFrame include.
	static uint32_t GetCurrentFrameIndex();

	static vk::Format GetFormat();

	static bool ShouldWaitOnImageAvailableSemaphore();
	
	static Flux_RenderAttachment* GetCurrentSwapchainTarget(uint32_t& uNumColourAttachments, Flux_RenderAttachment*& pxDepthStencil);
	
private:
	static void BindAsTarget();
	static void InitialiseCopyToFramebufferCommands();

	// Phase 6b: 9 statics (swapchain, images / image views / format / extent,
	// per-frame semaphores, current image index, colour attachments,
	// should-wait flag) moved to Zenith_Vulkan_SwapchainImpl held by
	// Zenith_Engine. Method bodies reach state via
	// g_xEngine.VulkanSwapchain().m_xXxx.
};
