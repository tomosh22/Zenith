#pragma once
#include "vulkan/vulkan.hpp"

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
class Zenith_Profiling;

// Per-Engine state + behaviour for the Vulkan swapchain. Replaces both the
// static-facade `class Zenith_Vulkan_Swapchain` and the data-only
// `Zenith_Vulkan_Swapchain`. Accessed via g_xEngine.FluxSwapchain().
class Zenith_Vulkan_Swapchain
{
public:
	Zenith_Vulkan_Swapchain() {}
	~Zenith_Vulkan_Swapchain();
	Zenith_Vulkan_Swapchain(const Zenith_Vulkan_Swapchain&) = delete;
	Zenith_Vulkan_Swapchain& operator=(const Zenith_Vulkan_Swapchain&) = delete;

	void Initialise();
	void Shutdown();

	// Instance entries called from the main loop via g_xEngine.FluxSwapchain().
	// BeginFrame returns false on swapchain-out-of-date so the main loop can
	// short-circuit the frame; in that case the caller must NOT advance the
	// frame index.
	bool BeginFrame();
	void EndFrame();

	uint32_t GetWidth();
	uint32_t GetHeight();

	vk::Extent2D& GetExtent();

	vk::Semaphore& GetCurrentImageAvailableSemaphore();

	// Ring index in [0, MAX_FRAMES_IN_FLIGHT). FrameContext owns the engine's
	// single frame-index variable; this accessor reads g_xEngine.Frame() at
	// the point of use because it is callable BEFORE Initialise() wires the
	// self-wired dep pointers (boot-time GPU asset uploads ask for the ring
	// slot) — see the definition for the call chain.
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

	// Command buffer for the final-frame present blit. The blit pipeline + shader
	// + recording are owned by the backend-neutral Flux_Present feature; only this
	// command buffer (whose record-pass begin / submit / present are inherently
	// backend-specific) stays here. EndFrame hands it to Flux_PresentImpl::
	// RecordBlit each frame.
	Zenith_Vulkan_CommandBuffer  m_xCopyToFramebufferCmd;

	// Self-wired cross-subsystem deps (set once at the top of Initialise()).
	// Public so the static BeginFrame()/EndFrame() entries can route through the
	// recovered g_xEngine.FluxSwapchain() reference, mirroring the existing
	// design where the static entries reach the data members.
	Zenith_Vulkan*               m_pxVulkan        = nullptr;
	Zenith_Vulkan_MemoryManager* m_pxVulkanMemory  = nullptr;
	Flux_RendererImpl*           m_pxFluxRenderer  = nullptr;
	Zenith_Profiling*            m_pxProfiling     = nullptr;

private:
	void BindAsTarget();
};
