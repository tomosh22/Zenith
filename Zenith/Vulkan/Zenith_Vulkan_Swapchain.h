#pragma once
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "vulkan/vulkan.hpp"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

//#TO for MAX_FRAMES_IN_FLIGHT which should really be somewhere else
#include "Flux/Flux_Enums.h"

#include "Flux/Flux_Types.h"  // Flux_RenderAttachment (complete type — m_axColourAttachments stores values)
#include "Vulkan/Zenith_Vulkan_Pipeline.h"
#include "Vulkan/Zenith_Vulkan_CommandBuffer.h"
#include "Core/ZenithConfig.h"

#include <vector>

// Cross-subsystem deps injected via self-wiring in Initialise() (this class is
// a Flux backend constrained by FluxBackendPresentation to a no-arg
// Initialise(), so the singleton reaches are cached as member pointers rather
// than passed as ctor/Initialise args). See the top of Initialise() in the .cpp.
class Zenith_Vulkan;
class Zenith_Vulkan_MemoryManager;
class Flux_RendererImpl;
class Flux_GraphicsImpl;
class Zenith_Profiling;

// Per-Engine state + behaviour for the Vulkan swapchain. Replaces both the
// static-facade `class Zenith_Vulkan_Swapchain` and the data-only
// `Zenith_Vulkan_Swapchain`. Accessed via g_xEngine.VulkanSwapchain().
class Zenith_Vulkan_Swapchain
{
public:
	Zenith_Vulkan_Swapchain() {}
	~Zenith_Vulkan_Swapchain();
	Zenith_Vulkan_Swapchain(const Zenith_Vulkan_Swapchain&) = delete;
	Zenith_Vulkan_Swapchain& operator=(const Zenith_Vulkan_Swapchain&) = delete;

	void Initialise();
	void Shutdown();

	// BeginFrame stays callable as a free-function-style entry (used by the
	// main loop's swapchain-acquire-failed branch via Flux_Swapchain alias).
	// Returns false on swapchain-out-of-date so the main loop can short-circuit
	// the frame; in that case the caller must NOT advance the frame counter.
	static bool BeginFrame();
	static void EndFrame();

	uint32_t GetWidth();
	uint32_t GetHeight();

	vk::Extent2D& GetExent();

	vk::Semaphore& GetCurrentImageAvailableSemaphore();

	// Ring index in [0, MAX_FRAMES_IN_FLIGHT). Owned by FluxRenderer as the
	// engine's single source of truth for frame counting; this accessor is
	// retained for compatibility but defined out-of-line so the header can
	// stay free of the Flux_RendererImpl include.
	uint32_t GetCurrentFrameIndex();

	vk::Format GetFormat();

	bool ShouldWaitOnImageAvailableSemaphore();

	Flux_RenderAttachment* GetCurrentSwapchainTarget(uint32_t& uNumColourAttachments, Flux_RenderAttachment*& pxDepthStencil);

	// ===== Data members (was Zenith_Vulkan_Swapchain) =====

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

	// Copy-to-framebuffer pass — samples the final render target and writes
	// to the current swapchain image. Manually Reset() in Shutdown() before
	// the Vulkan device tears down.
	Zenith_Vulkan_Shader         m_xCopyToFramebufferShader;
	Zenith_Vulkan_Pipeline       m_xCopyToFramebufferPipeline;
	Zenith_Vulkan_CommandBuffer  m_xCopyToFramebufferCmd;

	// Self-wired cross-subsystem deps (set once at the top of Initialise()).
	// Public so the static BeginFrame()/EndFrame() entries can route through the
	// recovered g_xEngine.VulkanSwapchain() reference, mirroring the existing
	// design where the static entries reach the data members.
	Zenith_Vulkan*               m_pxVulkan        = nullptr;
	Zenith_Vulkan_MemoryManager* m_pxVulkanMemory  = nullptr;
	Flux_RendererImpl*           m_pxFluxRenderer  = nullptr;
	Flux_GraphicsImpl*           m_pxFluxGraphics  = nullptr;
	Zenith_Profiling*            m_pxProfiling     = nullptr;

private:
	void BindAsTarget();
	void InitialiseCopyToFramebufferCommands();
};
