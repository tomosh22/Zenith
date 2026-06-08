#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Zenith_Vulkan_Swapchain.h"
#include "Zenith_Vulkan_Swapchain.h"

#include "Zenith_Vulkan.h"
#include "Zenith_Vulkan.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_PerFrame.h"
#include "Flux/Flux_RendererImpl.h"
#include "Flux/Flux_RenderTargets.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Core/Zenith_CommandLine.h"

#include <cstdio>

#ifdef ZENITH_WINDOWS
#include "Zenith_Windows_Window.h"
#endif

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
void RenderImGui();
#endif

// Phase 6b: swapchain state moved to Zenith_Vulkan_Swapchain held by
// Zenith_Engine. Access via g_xEngine.FluxSwapchain().m_xXxx.

uint32_t       Zenith_Vulkan_Swapchain::GetWidth()  { return Zenith_Vulkan_Swapchain::m_xExtent.width; }
uint32_t       Zenith_Vulkan_Swapchain::GetHeight() { return Zenith_Vulkan_Swapchain::m_xExtent.height; }
vk::Extent2D&  Zenith_Vulkan_Swapchain::GetExent()  { return Zenith_Vulkan_Swapchain::m_xExtent; }
vk::Format     Zenith_Vulkan_Swapchain::GetFormat() { return Zenith_Vulkan_Swapchain::m_xImageFormat; }

DEBUGVAR bool dbg_bOutputMRT = false;
DEBUGVAR uint32_t dbg_uMRTIndex = MRT_INDEX_DIFFUSE;

struct SwapChainSupportDetails
{
	vk::SurfaceCapabilitiesKHR m_xCapabilities;
	std::vector <vk::SurfaceFormatKHR> m_xFormats;
	std::vector <vk::PresentModeKHR> m_xPresentModes;
};

static SwapChainSupportDetails QuerySwapChainSupport()
{
	Zenith_Vulkan_Swapchain& xSwapchain = g_xEngine.FluxSwapchain();
	const vk::PhysicalDevice& xPhysicalDevice = xSwapchain.m_pxVulkan->GetPhysicalDevice();
	const vk::SurfaceKHR& xSurface = xSwapchain.m_pxVulkan->GetSurface();

	SwapChainSupportDetails xDetails;
	xDetails.m_xCapabilities = VkUnwrap(xPhysicalDevice.getSurfaceCapabilitiesKHR(xSurface));

	xDetails.m_xFormats = VkUnwrap(xPhysicalDevice.getSurfaceFormatsKHR(xSurface));

	xDetails.m_xPresentModes = VkUnwrap(xPhysicalDevice.getSurfacePresentModesKHR(xSurface));

	return xDetails;
}

static vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& xAvailableFormats)
{
	// Prefer BGRA (Windows), fall back to RGBA (Android)
	for (const vk::SurfaceFormatKHR& xFormat : xAvailableFormats)
	{
		if (xFormat.format == vk::Format::eB8G8R8A8Srgb && xFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
		{
			return xFormat;
		}
	}
	for (const vk::SurfaceFormatKHR& xFormat : xAvailableFormats)
	{
		if (xFormat.format == vk::Format::eR8G8B8A8Srgb && xFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
		{
			return xFormat;
		}
	}
	Zenith_Assert(false, "No suitable sRGB surface format found");
	return xAvailableFormats[0];
}

static vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& xCapabilities)
{
	if (xCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return xCapabilities.currentExtent;
	}

#ifdef ZENITH_WINDOWS
	GLFWwindow* pxWindow = Zenith_Window::GetInstance()->GetNativeWindow();
	int32_t iExtentWidth, iExtentHeight;
	glfwGetFramebufferSize(pxWindow, &iExtentWidth, &iExtentHeight);

	// Wait for non-zero framebuffer size (can be 0 during monitor transitions or minimization)
	while (iExtentWidth == 0 || iExtentHeight == 0)
	{
		glfwGetFramebufferSize(pxWindow, &iExtentWidth, &iExtentHeight);
		glfwWaitEvents();
	}
#else
	int32_t iExtentWidth, iExtentHeight;
	Zenith_Window::GetInstance()->GetSize(iExtentWidth, iExtentHeight);
#endif

	vk::Extent2D xExtent = { static_cast<uint32_t>(iExtentWidth), static_cast<uint32_t>(iExtentHeight) };
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
	return vk::PresentModeKHR::eFifo;
}

Zenith_Vulkan_Swapchain::~Zenith_Vulkan_Swapchain()

{
	
}

void Zenith_Vulkan_Swapchain::InitialiseCopyToFramebufferCommands()
{
	Zenith_Vulkan_Swapchain& xSwapchain = *this;

	xSwapchain.m_xCopyToFramebufferCmd.Initialise();

	// Pilot Slang program — replaces the GLSL pair Flux_Fullscreen_UV.vert +
	// Flux_TexturedQuad.frag. Resolves to Quads/Flux_TexturedQuad.slang via
	// Flux_ShaderRegistry.
	xSwapchain.m_xCopyToFramebufferShader.Initialise(FluxShaderProgram::TexturedQuad);

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_aeColourAttachmentFormats[0] = xSwapchain.m_axColourAttachments[0].m_xSurfaceInfo.m_eFormat;
	xPipelineSpec.m_uNumColourAttachments = 1;
	xPipelineSpec.m_pxShader = &xSwapchain.m_xCopyToFramebufferShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	Flux_PipelineLayout& xLayout = xPipelineSpec.m_xPipelineLayout;
	xLayout.m_uNumBindingGroups = 1;
	xLayout.m_axBindingGroups[0].m_axBindings[0].m_eType = BINDING_TYPE_TEXTURE;

	Flux_PipelineBuilder::FromSpecification(xSwapchain.m_xCopyToFramebufferPipeline, xPipelineSpec);
}

void Zenith_Vulkan_Swapchain::Initialise()
{
	// Self-wire cross-subsystem deps once (FluxBackendPresentation forbids
	// params on this Initialise()). Every other reach routes through these.
	// Initialise() also runs on swapchain recreation (resize) — re-caching the
	// same pointers each time is harmless.
	m_pxVulkan       = &g_xEngine.FluxBackend();
	m_pxVulkanMemory = &g_xEngine.FluxMemory();
	m_pxFluxRenderer = &g_xEngine.FluxRenderer();
	m_pxFluxGraphics = &g_xEngine.FluxGraphics();
	m_pxProfiling    = &g_xEngine.Profiling();

	const vk::SurfaceKHR& xSurface = m_pxVulkan->GetSurface();

	SwapChainSupportDetails xSwapChainSupport = QuerySwapChainSupport();
	vk::SurfaceFormatKHR xSurfaceFormat = ChooseSwapSurfaceFormat(xSwapChainSupport.m_xFormats);
	vk::PresentModeKHR ePresentMode = ChooseSwapPresentMode(xSwapChainSupport.m_xPresentModes);
	vk::Extent2D xExtent = ChooseSwapExtent(xSwapChainSupport.m_xCapabilities);

	uint32_t uImageCount = std::max(static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT), xSwapChainSupport.m_xCapabilities.minImageCount);

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

	// --screenshot copies the swapchain image to a buffer, which requires
	// TRANSFER_SRC usage. Add it only when a screenshot was requested so
	// default rendering keeps byte-identical swapchain creation. Guarded on
	// surface support (virtually all desktop surfaces allow it).
	if (Zenith_CommandLine::GetScreenshotPath() != nullptr &&
		(xSwapChainSupport.m_xCapabilities.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferSrc))
	{
		xCreateInfo.imageUsage |= vk::ImageUsageFlagBits::eTransferSrc;
	}

	uint32_t uGraphicsQueueIdx = m_pxVulkan->GetQueueIndex(COMMANDTYPE_GRAPHICS);
	uint32_t uPresentQueueIdx = m_pxVulkan->GetQueueIndex(COMMANDTYPE_GRAPHICS);
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
	// Use INHERIT if OPAQUE is not supported (Android only supports INHERIT)
	if (xSwapChainSupport.m_xCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eOpaque)
	{
		xCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	}
	else
	{
		xCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eInherit;
	}
	xCreateInfo.presentMode = ePresentMode;
	xCreateInfo.clipped = VK_TRUE;
	xCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	const vk::Device& xDevice = m_pxVulkan->GetDevice();
	m_xSwapChain = VkUnwrap(xDevice.createSwapchainKHR(xCreateInfo));

	m_xImages = VkUnwrap(xDevice.getSwapchainImagesKHR(m_xSwapChain));
	uImageCount = static_cast<uint32_t>(m_xImages.size());
	m_xImageViews.resize(uImageCount);

	Zenith_Assert(uImageCount <= MAX_FRAMES_IN_FLIGHT, "Swapchain image count %u exceeds MAX_FRAMES_IN_FLIGHT %u", uImageCount, MAX_FRAMES_IN_FLIGHT);

	for (uint32_t u = 0; u < m_xImages.size(); u++)
	{
		vk::Image& xImage = m_xImages[u];
		m_pxVulkanMemory->ImageTransitionBarrier(xImage, vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands);

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

		m_xImageViews[u] = VkUnwrap(xDevice.createImageView(xViewCreate));

		// Swapchain images don't use VRAM handles since they're managed by the swapchain
		m_axColourAttachments[u].m_xVRAMHandle.SetValue(UINT32_MAX);

		m_axColourAttachments[u].m_xSurfaceInfo.m_uWidth = xExtent.width;
		m_axColourAttachments[u].m_xSurfaceInfo.m_uHeight = xExtent.height;
		m_axColourAttachments[u].m_xSurfaceInfo.m_eFormat =
			xSurfaceFormat.format == vk::Format::eR8G8B8A8Srgb ? TEXTURE_FORMAT_RGBA8_SRGB : TEXTURE_FORMAT_BGRA8_SRGB;

		// Create views for swapchain images - register with handle system for consistency
		m_axColourAttachments[u].SRV().m_xImageViewHandle = m_pxVulkanMemory->RegisterImageView(m_xImageViews[u]);
		m_axColourAttachments[u].SRV().m_xVRAMHandle.SetValue(UINT32_MAX);

		m_axColourAttachments[u].RTV().m_xImageViewHandle = m_pxVulkanMemory->RegisterImageView(m_xImageViews[u]);
		m_axColourAttachments[u].RTV().m_xVRAMHandle.SetValue(UINT32_MAX);
	}


	m_xImageFormat = xSurfaceFormat.format;
	m_xExtent = xExtent;

	Zenith_Log(LOG_CATEGORY_VULKAN, "Swapchain: %ux%u, format=%d, images=%u, present=%d",
		xExtent.width, xExtent.height,
		static_cast<int>(xSurfaceFormat.format),
		uImageCount,
		static_cast<int>(ePresentMode));
	Zenith_Log(LOG_CATEGORY_VULKAN, "Swapchain surface capabilities: minImages=%u, maxImages=%u, minExtent=%ux%u, maxExtent=%ux%u",
		xSwapChainSupport.m_xCapabilities.minImageCount,
		xSwapChainSupport.m_xCapabilities.maxImageCount,
		xSwapChainSupport.m_xCapabilities.minImageExtent.width,
		xSwapChainSupport.m_xCapabilities.minImageExtent.height,
		xSwapChainSupport.m_xCapabilities.maxImageExtent.width,
		xSwapChainSupport.m_xCapabilities.maxImageExtent.height);

	vk::SemaphoreCreateInfo xSemaphoreInfo;

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		m_axImageAvailableSemaphores[i] = VkUnwrap(xDevice.createSemaphore(xSemaphoreInfo));
		m_axRenderFinishedSemaphores[i] = VkUnwrap(xDevice.createSemaphore(xSemaphoreInfo));
	}

	InitialiseCopyToFramebufferCommands();

	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan swapchain initialised");

	//#TO_TODO: this is very hacky, Initialise gets called whenever we need to recreate the swapchain, e.g on resize, this should only be called once
#ifdef ZENITH_DEBUG_VARIABLES
	static bool s_bInitialisedDbgVars = false;
	if (!s_bInitialisedDbgVars)
	{
		g_xEngine.DebugVariables().AddBoolean({ "Render", "Debug", "Output MRT" }, dbg_bOutputMRT);
		g_xEngine.DebugVariables().AddUInt32({ "Render", "Debug", "MRT Index" }, dbg_uMRTIndex, 0, MRT_INDEX_COUNT - 1);
		s_bInitialisedDbgVars = true;
	}
#endif
}

void Zenith_Vulkan_Swapchain::Shutdown()
{
	Zenith_Vulkan_Swapchain& xSwapchain = *this;
	const vk::Device& xDevice = m_pxVulkan->GetDevice();

	// Tear down the copy-to-framebuffer shader/pipeline/cmd buffer while the
	// Vulkan device is still alive. The members' destructors run later when
	// Zenith_Engine deletes the swapchain impl, but by then they'll be in the
	// freshly-reset state so the Reset()s inside the destructors are no-ops.
	xSwapchain.m_xCopyToFramebufferPipeline.Reset();
	xSwapchain.m_xCopyToFramebufferShader.Reset();
	xSwapchain.m_xCopyToFramebufferCmd.GetCurrentCmdBuffer() = VK_NULL_HANDLE;
	xSwapchain.m_xCopyToFramebufferCmd.SetCurrentRenderPass(VK_NULL_HANDLE);

	for (u_int u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		Flux_RenderAttachment& xAttachment = xSwapchain.m_axColourAttachments[u];
		if (xAttachment.SRV().m_xImageViewHandle.IsValid())
		{
			m_pxVulkanMemory->ReleaseImageViewHandle(xAttachment.SRV().m_xImageViewHandle);
		}
		if (xAttachment.RTV().m_xImageViewHandle.IsValid())
		{
			m_pxVulkanMemory->ReleaseImageViewHandle(xAttachment.RTV().m_xImageViewHandle);
		}
		xAttachment = Flux_RenderAttachment();

		if (xSwapchain.m_axImageAvailableSemaphores[u])
		{
			xDevice.destroySemaphore(xSwapchain.m_axImageAvailableSemaphores[u]);
			xSwapchain.m_axImageAvailableSemaphores[u] = VK_NULL_HANDLE;
		}
		if (xSwapchain.m_axRenderFinishedSemaphores[u])
		{
			xDevice.destroySemaphore(xSwapchain.m_axRenderFinishedSemaphores[u]);
			xSwapchain.m_axRenderFinishedSemaphores[u] = VK_NULL_HANDLE;
		}
	}

	for (vk::ImageView& xImageView : xSwapchain.m_xImageViews)
	{
		if (xImageView)
		{
			xDevice.destroyImageView(xImageView);
			xImageView = VK_NULL_HANDLE;
		}
	}
	xSwapchain.m_xImageViews.clear();
	xSwapchain.m_xImages.clear();

	if (xSwapchain.m_xSwapChain)
	{
		xDevice.destroySwapchainKHR(xSwapchain.m_xSwapChain);
		xSwapchain.m_xSwapChain = VK_NULL_HANDLE;
	}

	xSwapchain.m_xImageFormat = vk::Format::eUndefined;
	xSwapchain.m_xExtent = vk::Extent2D();
	xSwapchain.m_uCurrentImageIndex = 0;
	xSwapchain.m_bShouldWaitOnImageAvailableSem = false;

	m_pxVulkan       = nullptr;
	m_pxVulkanMemory = nullptr;
	m_pxFluxRenderer = nullptr;
	m_pxFluxGraphics = nullptr;
	m_pxProfiling    = nullptr;

	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan swapchain shut down");
}

bool Zenith_Vulkan_Swapchain::BeginFrame()
{
	// Static entry — no 'this'. Recover the singleton once (legitimate re-entry,
	// like a render-graph trampoline) and route all reaches through it.
	Zenith_Vulkan_Swapchain& xSwapchain = g_xEngine.FluxSwapchain();

	xSwapchain.m_pxProfiling->BeginProfile(ZENITH_PROFILE_INDEX__FLUX_SWAPCHAIN_BEGIN_FRAME);
	const vk::Device& xDevice = xSwapchain.m_pxVulkan->GetDevice();

	//#TO_TODO: -1 here to shut up validation layer
	vk::Result eResult = xDevice.acquireNextImageKHR(xSwapchain.m_xSwapChain, UINT64_MAX - 1, xSwapchain.m_axImageAvailableSemaphores[xSwapchain.GetCurrentFrameIndex()], nullptr, &xSwapchain.m_uCurrentImageIndex);

	xSwapchain.m_bShouldWaitOnImageAvailableSem = eResult == vk::Result::eSuccess;

	Zenith_Assert(eResult == vk::Result::eSuccess || eResult == vk::Result::eErrorOutOfDateKHR, "Failed to acquire swapchain image");

	if (eResult == vk::Result::eErrorOutOfDateKHR)
	{
		// Wait for GPU to finish using all resources before destroying them
		// This prevents "semaphore in use" errors during window resize/maximize
		xSwapchain.m_pxVulkan->WaitForGPUIdle();

		// Cleanup swapchain resources before recreation
		for (u_int u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
		{
			// Destroy image views directly - GPU is already idle so no deferred deletion needed
			xDevice.destroyImageView(xSwapchain.m_xImageViews[u]);
			xDevice.destroySemaphore(xSwapchain.m_axImageAvailableSemaphores[u]);
			xDevice.destroySemaphore(xSwapchain.m_axRenderFinishedSemaphores[u]);
		}
		xSwapchain.m_xImages.clear();
		xSwapchain.m_xImageViews.clear();
		xDevice.destroySwapchainKHR(xSwapchain.m_xSwapChain);
		xSwapchain.Initialise();
		xSwapchain.m_pxFluxRenderer->OnResChange();
	}
	xSwapchain.m_pxProfiling->EndProfile(ZENITH_PROFILE_INDEX__FLUX_SWAPCHAIN_BEGIN_FRAME);
	return true;
}

void Zenith_Vulkan_Swapchain::BindAsTarget()
{
	Flux_RenderAttachment* pxSwapchainAttachment = &m_axColourAttachments[m_uCurrentImageIndex];
	const TextureFormat aeColourFormats[] = { pxSwapchainAttachment->m_xSurfaceInfo.m_eFormat };
#if 1//def ZENITH_DEBUG
	vk::RenderPass xRenderPass = Zenith_Vulkan_Pipeline::TargetSetupToRenderPass(aeColourFormats, 1, TEXTURE_FORMAT_NONE, LOAD_ACTION_CLEAR, STORE_ACTION_STORE, LOAD_ACTION_CLEAR, STORE_ACTION_DONTCARE, RENDER_TARGET_USAGE_PRESENT);
#else
	vk::RenderPass xRenderPass = Zenith_Vulkan_Pipeline::TargetSetupToRenderPass(aeColourFormats, 1, TEXTURE_FORMAT_NONE, LOAD_ACTION_DONTCARE, STORE_ACTION_STORE, LOAD_ACTION_DONTCARE, STORE_ACTION_DONTCARE, RENDER_TARGET_USAGE_PRESENT);
#endif
	m_pxVulkan->m_pxCurrentFrame->DeferDestroyRenderPass(xRenderPass);

	// Wrap the raw swapchain attachment in a carrier so it flows through the same framebuffer-creation path as render graph attachments.
	Flux_RenderGraph_AttachmentRef axColourRefs[1];
	axColourRefs[0] = Flux_RenderGraph_AttachmentRef(Flux_GraphResource(*pxSwapchainAttachment), 0, 0);
	Flux_RenderGraph_AttachmentRef xDepthRef; // default-constructed -> IsValid() == false
	vk::Framebuffer xFramebuffer = Zenith_Vulkan_Pipeline::TargetSetupToFramebuffer(axColourRefs, 1, xDepthRef, GetWidth(), GetHeight(), xRenderPass);
	m_pxVulkan->m_pxCurrentFrame->DeferDestroyFramebuffer(xFramebuffer);

	vk::ClearValue xClear;
	vk::ClearColorValue xClearColourValue(0.f, 0.f, 0.f, 1.f);
	xClear.color = xClearColourValue;

	vk::RenderPassBeginInfo xRenderPassInfo = vk::RenderPassBeginInfo()
		.setRenderPass(xRenderPass)
		.setFramebuffer(xFramebuffer)
		.setRenderArea({ {0,0}, m_xExtent })
		.setPClearValues(&xClear)
		.setClearValueCount(1);

	Zenith_Vulkan_CommandBuffer& xCmd = m_xCopyToFramebufferCmd;
	xCmd.GetCurrentCmdBuffer().beginRenderPass(xRenderPassInfo, vk::SubpassContents::eInline);

	// Set the render pass in the command buffer object so RenderImGui() can verify it's active
	xCmd.SetCurrentRenderPass(xRenderPass);

	//flipping because porting from opengl
	vk::Viewport xViewport{};
	xViewport.x = 0.0f;
	xViewport.y = 0.0f;
	xViewport.width = static_cast<float>(m_axColourAttachments[m_uCurrentImageIndex].m_xSurfaceInfo.m_uWidth);
	xViewport.height = static_cast<float>(m_axColourAttachments[m_uCurrentImageIndex].m_xSurfaceInfo.m_uHeight);
	xViewport.minDepth = 0.0f;
	xViewport.maxDepth = 1.0f;

	vk::Rect2D xScissor{};
	xScissor.offset = vk::Offset2D(0, 0);
	xScissor.extent = vk::Extent2D(
		m_axColourAttachments[m_uCurrentImageIndex].m_xSurfaceInfo.m_uWidth,
		m_axColourAttachments[m_uCurrentImageIndex].m_xSurfaceInfo.m_uHeight);

	xCmd.GetCurrentCmdBuffer().setViewport(0, 1, &xViewport);
	xCmd.GetCurrentCmdBuffer().setScissor(0, 1, &xScissor);
}

bool Zenith_Vulkan_Swapchain::ShouldWaitOnImageAvailableSemaphore()
{
	return m_bShouldWaitOnImageAvailableSem;
}

namespace
{
	// --screenshot readback. Copies the just-rendered swapchain image into a
	// host-visible buffer and writes an uncompressed 32-bit TGA (top-left
	// origin, BGRA byte order — matches the usual B8G8R8A8 swapchain, R/B
	// swapped for an R8G8B8A8 surface). Self-contained (own buffer + one-time
	// command buffer, no engine registries) and runs only on the single
	// --screenshot-frame, so the waitIdle cost is irrelevant. Deterministic
	// with --fixed-dt; the in-engine dump avoids OS-compositor noise that
	// makes the CopyFromScreen fallback flaky for sub-0.1% A/B compares.
	void WriteSwapchainScreenshotTGA(Zenith_Vulkan_Swapchain& xSwapchain, const char* szPath)
	{
		// No valid acquired image this frame (transient acquire failure) — skip.
		if (!xSwapchain.m_bShouldWaitOnImageAvailableSem)
		{
			return;
		}

		Zenith_Vulkan* pxVulkan = xSwapchain.m_pxVulkan;
		const vk::Device& xDevice = pxVulkan->GetDevice();
		const uint32_t uW = xSwapchain.m_xExtent.width;
		const uint32_t uH = xSwapchain.m_xExtent.height;
		const vk::DeviceSize ulSize = vk::DeviceSize(uW) * uH * 4;

		// Ensure the just-submitted render into the swapchain image is complete.
		VkCheck(xDevice.waitIdle());

		// --- host-visible staging buffer ---
		vk::Buffer xBuf = VkUnwrap(xDevice.createBuffer(vk::BufferCreateInfo()
			.setSize(ulSize)
			.setUsage(vk::BufferUsageFlagBits::eTransferDst)
			.setSharingMode(vk::SharingMode::eExclusive)));

		const vk::MemoryRequirements xReq = xDevice.getBufferMemoryRequirements(xBuf);
		const vk::PhysicalDeviceMemoryProperties xMemProps = pxVulkan->GetPhysicalDevice().getMemoryProperties();
		const vk::MemoryPropertyFlags eWant = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
		uint32_t uMemType = UINT32_MAX;
		for (uint32_t u = 0; u < xMemProps.memoryTypeCount; ++u)
		{
			if ((xReq.memoryTypeBits & (1u << u)) && (xMemProps.memoryTypes[u].propertyFlags & eWant) == eWant)
			{
				uMemType = u;
				break;
			}
		}
		Zenith_Assert(uMemType != UINT32_MAX, "screenshot: no host-visible coherent memory type");

		vk::DeviceMemory xMem = VkUnwrap(xDevice.allocateMemory(vk::MemoryAllocateInfo()
			.setAllocationSize(xReq.size)
			.setMemoryTypeIndex(uMemType)));
		VkCheck(xDevice.bindBufferMemory(xBuf, xMem, 0));

		// --- one-time copy command buffer ---
		vk::CommandBuffer xCmd = VkUnwrap(xDevice.allocateCommandBuffers(vk::CommandBufferAllocateInfo()
			.setCommandPool(pxVulkan->GetCommandPool(COMMANDTYPE_GRAPHICS))
			.setLevel(vk::CommandBufferLevel::ePrimary)
			.setCommandBufferCount(1)))[0];

		VkCheck(xCmd.begin(vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit)));

		vk::Image xImage = xSwapchain.m_xImages[xSwapchain.m_uCurrentImageIndex];
		const vk::ImageSubresourceRange xColourRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

		// PresentSrc -> TransferSrc (render pass left the image in PresentSrc).
		vk::ImageMemoryBarrier xToSrc = vk::ImageMemoryBarrier()
			.setOldLayout(vk::ImageLayout::ePresentSrcKHR)
			.setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
			.setSrcAccessMask(vk::AccessFlagBits::eMemoryRead)
			.setDstAccessMask(vk::AccessFlagBits::eTransferRead)
			.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
			.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
			.setImage(xImage)
			.setSubresourceRange(xColourRange);
		xCmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer,
			{}, nullptr, nullptr, xToSrc);

		xCmd.copyImageToBuffer(xImage, vk::ImageLayout::eTransferSrcOptimal, xBuf,
			vk::BufferImageCopy()
				.setImageSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
				.setImageExtent(vk::Extent3D(uW, uH, 1)));

		// TransferSrc -> PresentSrc so the subsequent present is valid.
		vk::ImageMemoryBarrier xToPresent = xToSrc;
		xToPresent
			.setOldLayout(vk::ImageLayout::eTransferSrcOptimal)
			.setNewLayout(vk::ImageLayout::ePresentSrcKHR)
			.setSrcAccessMask(vk::AccessFlagBits::eTransferRead)
			.setDstAccessMask(vk::AccessFlagBits::eMemoryRead);
		xCmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands,
			{}, nullptr, nullptr, xToPresent);

		VkCheck(xCmd.end());
		vk::SubmitInfo xSubmit = vk::SubmitInfo().setCommandBufferCount(1).setPCommandBuffers(&xCmd);
		VkCheck(pxVulkan->GetQueue(COMMANDTYPE_GRAPHICS).submit(xSubmit, nullptr));
		VkCheck(pxVulkan->GetQueue(COMMANDTYPE_GRAPHICS).waitIdle());

		// --- map + write TGA ---
		const uint8_t* pSrc = static_cast<const uint8_t*>(VkUnwrap(xDevice.mapMemory(xMem, 0, ulSize)));
		const bool bIsBGRA =
			xSwapchain.m_xImageFormat == vk::Format::eB8G8R8A8Unorm ||
			xSwapchain.m_xImageFormat == vk::Format::eB8G8R8A8Srgb;

		std::FILE* pFile = nullptr;
#ifdef _MSC_VER
		::fopen_s(&pFile, szPath, "wb");
#else
		pFile = std::fopen(szPath, "wb");
#endif
		if (pFile != nullptr)
		{
			uint8_t aHeader[18] = {};
			aHeader[2]  = 2;                              // uncompressed true-color
			aHeader[12] = uint8_t(uW & 0xFF);
			aHeader[13] = uint8_t((uW >> 8) & 0xFF);
			aHeader[14] = uint8_t(uH & 0xFF);
			aHeader[15] = uint8_t((uH >> 8) & 0xFF);
			aHeader[16] = 32;                            // bits per pixel
			aHeader[17] = 0x28;                          // top-left origin (0x20) + 8 alpha bits (0x08)
			std::fwrite(aHeader, 1, sizeof(aHeader), pFile);

			// TGA 32-bit is stored BGRA. A B8G8R8A8 surface already matches; for
			// an R8G8B8A8 surface swap R/B in place in the (host-visible, about
			// to be unmapped) staging memory so the on-disk dump is consistent
			// regardless of swapchain format.
			if (!bIsBGRA)
			{
				uint8_t* pMut = const_cast<uint8_t*>(pSrc);
				const uint32_t uPixels = uW * uH;
				for (uint32_t p = 0; p < uPixels; ++p)
				{
					uint8_t* px = pMut + p * 4;
					const uint8_t uR = px[0];
					px[0] = px[2];
					px[2] = uR;
				}
			}
			std::fwrite(pSrc, 1, size_t(ulSize), pFile);

			std::fclose(pFile);
			Zenith_Log(LOG_CATEGORY_VULKAN, "Screenshot written: %s (%ux%u)", szPath, uW, uH);
		}
		else
		{
			Zenith_Error(LOG_CATEGORY_VULKAN, "Screenshot: failed to open '%s' for writing", szPath);
		}
		xDevice.unmapMemory(xMem);

		// --- cleanup ---
		xDevice.freeCommandBuffers(pxVulkan->GetCommandPool(COMMANDTYPE_GRAPHICS), xCmd);
		xDevice.destroyBuffer(xBuf);
		xDevice.freeMemory(xMem);
	}
}

void Zenith_Vulkan_Swapchain::EndFrame()
{
	Zenith_Vulkan_Swapchain& xSwapchain = g_xEngine.FluxSwapchain();
	Zenith_Vulkan_CommandBuffer& xCmd = xSwapchain.m_xCopyToFramebufferCmd;

	xCmd.BeginRecording();

	xSwapchain.BindAsTarget();

	xCmd.SetPipeline(&xSwapchain.m_xCopyToFramebufferPipeline);

	xCmd.SetVertexBuffer(xSwapchain.m_pxFluxGraphics->m_xQuadMesh.GetVertexBuffer());
	xCmd.SetIndexBuffer(xSwapchain.m_pxFluxGraphics->m_xQuadMesh.GetIndexBuffer());

	xCmd.BeginBind(0);
#ifdef ZENITH_DEBUG_VARIABLES
	if (dbg_bOutputMRT)
	{
		// When debugging, bind the MRT SRV
		Flux_ShaderResourceView* pxMRTSRV = xSwapchain.m_pxFluxGraphics->GetGBufferSRV((MRTIndex)dbg_uMRTIndex);
		if (pxMRTSRV)
		{
			xCmd.BindSRV(pxMRTSRV, 0);
		}
	}
	else
#endif
	{
		// Bind final render target using SRV
		Flux_ShaderResourceView& xSRV = xSwapchain.m_pxFluxGraphics->GetFinalRenderTarget().SRV();
		if (xSRV.m_xImageViewHandle.IsValid())
		{
			xCmd.BindSRV(&xSRV, 0);
		}
		else
		{
			Zenith_Assert(false, "Final render target SRV not created");
		}
	}

	xCmd.DrawIndexed(6);

#ifdef ZENITH_TOOLS
	// Render ImGui on top of the game content
	// The render pass is still active from BindAsTarget()
	xCmd.RenderImGui();
#endif

	xCmd.GetCurrentCmdBuffer().endRenderPass();
	VkCheck(xCmd.GetCurrentCmdBuffer().end());

	// Only signal renderFinished semaphore when we have a valid image to present
	// Otherwise the semaphore stays signaled and causes a double-signal on the next frame
	vk::SubmitInfo xRenderSubmitInfo = vk::SubmitInfo()
		.setCommandBufferCount(1)
		.setPCommandBuffers(&xCmd.GetCurrentCmdBuffer())
		.setPWaitSemaphores(nullptr)
		.setWaitSemaphoreCount(0)
		.setPSignalSemaphores(xSwapchain.m_bShouldWaitOnImageAvailableSem ? &xSwapchain.m_axRenderFinishedSemaphores[xSwapchain.m_uCurrentImageIndex] : nullptr)
		.setSignalSemaphoreCount(xSwapchain.m_bShouldWaitOnImageAvailableSem ? 1 : 0);

	VkCheck(xSwapchain.m_pxVulkan->GetQueue(COMMANDTYPE_GRAPHICS).submit(xRenderSubmitInfo, xSwapchain.m_pxVulkan->GetCurrentInFlightFence()));

	// --screenshot: capture the freshly-rendered swapchain image on the
	// requested frame, before present (the helper restores the PresentSrc
	// layout so present stays valid). Fires exactly once — the frame counter
	// equals the target on a single EndFrame.
	{
		const char* szScreenshotPath = Zenith_CommandLine::GetScreenshotPath();
		if (szScreenshotPath != nullptr &&
			g_xEngine.FluxRenderer().GetFrameCounter() == Zenith_CommandLine::GetScreenshotFrame())
		{
			WriteSwapchainScreenshotTGA(xSwapchain, szScreenshotPath);
		}
	}

	if (xSwapchain.m_bShouldWaitOnImageAvailableSem)
	{
		vk::PresentInfoKHR presentInfo = vk::PresentInfoKHR()
			.setSwapchainCount(1)
			.setPSwapchains(&xSwapchain.m_xSwapChain)
			.setPImageIndices(&xSwapchain.m_uCurrentImageIndex)
			.setWaitSemaphoreCount(1)
			.setPWaitSemaphores(&xSwapchain.m_axRenderFinishedSemaphores[xSwapchain.m_uCurrentImageIndex]);

#ifdef ZENITH_ASSERT
		vk::Result eResult =
#endif
			xSwapchain.m_pxVulkan->GetQueue(COMMANDTYPE_PRESENT).presentKHR(&presentInfo);

		Zenith_Assert(eResult == vk::Result::eSuccess || eResult == vk::Result::eErrorOutOfDateKHR || eResult == vk::Result::eSuboptimalKHR, "Failed to present");

	}
	// Frame counter advance is now owned by Flux_PerFrame::EndFrame, called
	// once at the bottom of Zenith_MainLoop. Removed the local counter bump
	// here to keep one source of truth.
}

vk::Semaphore& Zenith_Vulkan_Swapchain::GetCurrentImageAvailableSemaphore()
{
	return m_axImageAvailableSemaphores[GetCurrentFrameIndex()];
}

uint32_t Zenith_Vulkan_Swapchain::GetCurrentFrameIndex()
{
	// Single source of truth — Flux_PerFrame owns the monotonic frame counter
	// and the ring index. The swapchain's previous s_uFrameIndex member has
	// been removed; backends and engine code that need the current ring slot
	// all derive it from here.
	return m_pxFluxRenderer->GetRingIndex();
}

Flux_RenderAttachment* Zenith_Vulkan_Swapchain::GetCurrentSwapchainTarget(uint32_t& uNumColourAttachments, Flux_RenderAttachment*& pxDepthStencil)
{
	uNumColourAttachments = 1;
	pxDepthStencil = nullptr;
	return &m_axColourAttachments[m_uCurrentImageIndex];
}
