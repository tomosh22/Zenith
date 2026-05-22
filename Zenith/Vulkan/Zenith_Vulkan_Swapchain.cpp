#include "Zenith.h"

#include "Zenith_Vulkan_Swapchain.h"
#include "Zenith_Vulkan_SwapchainImpl.h"

#include "Zenith_Vulkan.h"
#include "Zenith_VulkanImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_PerFrame.h"
#include "Flux/Flux_RenderTargets.h"
#include "DebugVariables/Zenith_DebugVariables.h"

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

// Phase 6b: swapchain state moved to Zenith_Vulkan_SwapchainImpl held by
// Zenith_Engine. Access via g_xEngine.VulkanSwapchain().m_xXxx.

uint32_t       Zenith_Vulkan_Swapchain::GetWidth()  { return g_xEngine.VulkanSwapchain().m_xExtent.width; }
uint32_t       Zenith_Vulkan_Swapchain::GetHeight() { return g_xEngine.VulkanSwapchain().m_xExtent.height; }
vk::Extent2D&  Zenith_Vulkan_Swapchain::GetExent()  { return g_xEngine.VulkanSwapchain().m_xExtent; }
vk::Format     Zenith_Vulkan_Swapchain::GetFormat() { return g_xEngine.VulkanSwapchain().m_xImageFormat; }

static Zenith_Vulkan_Shader s_xShader;
static Zenith_Vulkan_Pipeline s_xPipeline;
static Zenith_Vulkan_CommandBuffer s_xCopyToFramebufferCmd;

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
	const vk::PhysicalDevice& xPhysicalDevice = Zenith_Vulkan::GetPhysicalDevice();
	const vk::SurfaceKHR& xSurface = Zenith_Vulkan::GetSurface();

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
	s_xCopyToFramebufferCmd.Initialise();

	// Pilot Slang program — replaces the GLSL pair Flux_Fullscreen_UV.vert +
	// Flux_TexturedQuad.frag. Resolves to Quads/Flux_TexturedQuad.slang via
	// Flux_ShaderRegistry.
	s_xShader.Initialise(FluxShaderProgram::TexturedQuad);

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_aeColourAttachmentFormats[0] = g_xEngine.VulkanSwapchain().m_axColourAttachments[0].m_xSurfaceInfo.m_eFormat;
	xPipelineSpec.m_uNumColourAttachments = 1;
	xPipelineSpec.m_pxShader = &s_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	Flux_PipelineLayout& xLayout = xPipelineSpec.m_xPipelineLayout;
	xLayout.m_uNumBindingGroups = 1;
	xLayout.m_axBindingGroups[0].m_axBindings[0].m_eType = BINDING_TYPE_TEXTURE;

#if 0
	(
		xVertexDesc,
		&s_xShader,
		xBlendStates,
		false,
		false,
		DEPTH_COMPARE_FUNC_ALWAYS,
		DEPTHSTENCIL_FORMAT_NONE,
		false,
		false,
		{ 0,1 },
		{ 0,0 },
		s_axTargetSetups[0],
		false
	);
#endif

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

}

void Zenith_Vulkan_Swapchain::Initialise()
{
	const vk::SurfaceKHR& xSurface = Zenith_Vulkan::GetSurface();

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

	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	g_xEngine.VulkanSwapchain().m_xSwapChain = VkUnwrap(xDevice.createSwapchainKHR(xCreateInfo));

	g_xEngine.VulkanSwapchain().m_xImages = VkUnwrap(xDevice.getSwapchainImagesKHR(g_xEngine.VulkanSwapchain().m_xSwapChain));
	uImageCount = static_cast<uint32_t>(g_xEngine.VulkanSwapchain().m_xImages.size());
	g_xEngine.VulkanSwapchain().m_xImageViews.resize(uImageCount);

	Zenith_Assert(uImageCount <= MAX_FRAMES_IN_FLIGHT, "Swapchain image count %u exceeds MAX_FRAMES_IN_FLIGHT %u", uImageCount, MAX_FRAMES_IN_FLIGHT);

	for (uint32_t u = 0; u < g_xEngine.VulkanSwapchain().m_xImages.size(); u++)
	{
		vk::Image& xImage = g_xEngine.VulkanSwapchain().m_xImages[u];
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

		g_xEngine.VulkanSwapchain().m_xImageViews[u] = VkUnwrap(xDevice.createImageView(xViewCreate));

		// Swapchain images don't use VRAM handles since they're managed by the swapchain
		g_xEngine.VulkanSwapchain().m_axColourAttachments[u].m_xVRAMHandle.SetValue(UINT32_MAX);

		g_xEngine.VulkanSwapchain().m_axColourAttachments[u].m_xSurfaceInfo.m_uWidth = xExtent.width;
		g_xEngine.VulkanSwapchain().m_axColourAttachments[u].m_xSurfaceInfo.m_uHeight = xExtent.height;
		g_xEngine.VulkanSwapchain().m_axColourAttachments[u].m_xSurfaceInfo.m_eFormat =
			xSurfaceFormat.format == vk::Format::eR8G8B8A8Srgb ? TEXTURE_FORMAT_RGBA8_SRGB : TEXTURE_FORMAT_BGRA8_SRGB;
		
		// Create views for swapchain images - register with handle system for consistency
		g_xEngine.VulkanSwapchain().m_axColourAttachments[u].SRV().m_xImageViewHandle = Zenith_Vulkan_MemoryManager::RegisterImageView(g_xEngine.VulkanSwapchain().m_xImageViews[u]);
		g_xEngine.VulkanSwapchain().m_axColourAttachments[u].SRV().m_xVRAMHandle.SetValue(UINT32_MAX);

		g_xEngine.VulkanSwapchain().m_axColourAttachments[u].RTV().m_xImageViewHandle = Zenith_Vulkan_MemoryManager::RegisterImageView(g_xEngine.VulkanSwapchain().m_xImageViews[u]);
		g_xEngine.VulkanSwapchain().m_axColourAttachments[u].RTV().m_xVRAMHandle.SetValue(UINT32_MAX);
	}
	

	g_xEngine.VulkanSwapchain().m_xImageFormat = xSurfaceFormat.format;
	g_xEngine.VulkanSwapchain().m_xExtent = xExtent;

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
		g_xEngine.VulkanSwapchain().m_axImageAvailableSemaphores[i] = VkUnwrap(xDevice.createSemaphore(xSemaphoreInfo));
		g_xEngine.VulkanSwapchain().m_axRenderFinishedSemaphores[i] = VkUnwrap(xDevice.createSemaphore(xSemaphoreInfo));
	}

	InitialiseCopyToFramebufferCommands();

	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan swapchain initialised");

	//#TO_TODO: this is very hacky, Initialise gets called whenever we need to recreate the swapchain, e.g on resize, this should only be called once
#ifdef ZENITH_DEBUG_VARIABLES
	static bool s_bInitialisedDbgVars = false;
	if (!s_bInitialisedDbgVars)
	{
		Zenith_DebugVariables::AddBoolean({ "Render", "Debug", "Output MRT" }, dbg_bOutputMRT);
		Zenith_DebugVariables::AddUInt32({ "Render", "Debug", "MRT Index" }, dbg_uMRTIndex, 0, MRT_INDEX_COUNT - 1);
		s_bInitialisedDbgVars = true;
	}
#endif
}

bool Zenith_Vulkan_Swapchain::BeginFrame()
{
	Zenith_Profiling::BeginProfile(ZENITH_PROFILE_INDEX__FLUX_SWAPCHAIN_BEGIN_FRAME);
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	//#TO_TODO: -1 here to shut up validation layer
	vk::Result eResult = xDevice.acquireNextImageKHR(g_xEngine.VulkanSwapchain().m_xSwapChain, UINT64_MAX - 1, g_xEngine.VulkanSwapchain().m_axImageAvailableSemaphores[GetCurrentFrameIndex()], nullptr, &g_xEngine.VulkanSwapchain().m_uCurrentImageIndex);

	g_xEngine.VulkanSwapchain().m_bShouldWaitOnImageAvailableSem = eResult == vk::Result::eSuccess;

	Zenith_Assert(eResult == vk::Result::eSuccess || eResult == vk::Result::eErrorOutOfDateKHR, "Failed to acquire swapchain image");

	if (eResult == vk::Result::eErrorOutOfDateKHR)
	{
		// Wait for GPU to finish using all resources before destroying them
		// This prevents "semaphore in use" errors during window resize/maximize
		Zenith_Vulkan::WaitForGPUIdle();

		// Cleanup swapchain resources before recreation
		for (u_int u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
		{
			// Destroy image views directly - GPU is already idle so no deferred deletion needed
			xDevice.destroyImageView(g_xEngine.VulkanSwapchain().m_xImageViews[u]);
			xDevice.destroySemaphore(g_xEngine.VulkanSwapchain().m_axImageAvailableSemaphores[u]);
			xDevice.destroySemaphore(g_xEngine.VulkanSwapchain().m_axRenderFinishedSemaphores[u]);
		}
		g_xEngine.VulkanSwapchain().m_xImages.clear();
		g_xEngine.VulkanSwapchain().m_xImageViews.clear();
		xDevice.destroySwapchainKHR(g_xEngine.VulkanSwapchain().m_xSwapChain);
		Initialise();
		Flux::OnResChange();
	}
	Zenith_Profiling::EndProfile(ZENITH_PROFILE_INDEX__FLUX_SWAPCHAIN_BEGIN_FRAME);
	return true;
}

void Zenith_Vulkan_Swapchain::BindAsTarget()
{
	Flux_RenderAttachment* pxSwapchainAttachment = &g_xEngine.VulkanSwapchain().m_axColourAttachments[g_xEngine.VulkanSwapchain().m_uCurrentImageIndex];
	const TextureFormat aeColourFormats[] = { pxSwapchainAttachment->m_xSurfaceInfo.m_eFormat };
#if 1//def ZENITH_DEBUG
	vk::RenderPass xRenderPass = Zenith_Vulkan_Pipeline::TargetSetupToRenderPass(aeColourFormats, 1, TEXTURE_FORMAT_NONE, LOAD_ACTION_CLEAR, STORE_ACTION_STORE, LOAD_ACTION_CLEAR, STORE_ACTION_DONTCARE, RENDER_TARGET_USAGE_PRESENT);
#else
	vk::RenderPass xRenderPass = Zenith_Vulkan_Pipeline::TargetSetupToRenderPass(aeColourFormats, 1, TEXTURE_FORMAT_NONE, LOAD_ACTION_DONTCARE, STORE_ACTION_STORE, LOAD_ACTION_DONTCARE, STORE_ACTION_DONTCARE, RENDER_TARGET_USAGE_PRESENT);
#endif
	g_xEngine.Vulkan().m_pxCurrentFrame->DeferDestroyRenderPass(xRenderPass);

	// Wrap the raw swapchain attachment in a carrier so it flows through the same framebuffer-creation path as render graph attachments.
	Flux_RenderGraph_AttachmentRef axColourRefs[1];
	axColourRefs[0] = Flux_RenderGraph_AttachmentRef(Flux_GraphResource(*pxSwapchainAttachment), 0, 0);
	Flux_RenderGraph_AttachmentRef xDepthRef; // default-constructed -> IsValid() == false
	vk::Framebuffer xFramebuffer = Zenith_Vulkan_Pipeline::TargetSetupToFramebuffer(axColourRefs, 1, xDepthRef, Zenith_Vulkan_Swapchain::GetWidth(), Zenith_Vulkan_Swapchain::GetHeight(), xRenderPass);
	g_xEngine.Vulkan().m_pxCurrentFrame->DeferDestroyFramebuffer(xFramebuffer);

	vk::ClearValue xClear;
	vk::ClearColorValue xClearColourValue(0.f, 0.f, 0.f, 1.f);
	xClear.color = xClearColourValue;

	vk::RenderPassBeginInfo xRenderPassInfo = vk::RenderPassBeginInfo()
		.setRenderPass(xRenderPass)
		.setFramebuffer(xFramebuffer)
		.setRenderArea({ {0,0}, g_xEngine.VulkanSwapchain().m_xExtent })
		.setPClearValues(&xClear)
		.setClearValueCount(1);

	s_xCopyToFramebufferCmd.GetCurrentCmdBuffer().beginRenderPass(xRenderPassInfo, vk::SubpassContents::eInline);
	
	// Set the render pass in the command buffer object so RenderImGui() can verify it's active
	s_xCopyToFramebufferCmd.SetCurrentRenderPass(xRenderPass);

	//flipping because porting from opengl
	vk::Viewport xViewport{};
	xViewport.x = 0.0f;
	xViewport.y = 0.0f;
	xViewport.width = static_cast<float>(g_xEngine.VulkanSwapchain().m_axColourAttachments[g_xEngine.VulkanSwapchain().m_uCurrentImageIndex].m_xSurfaceInfo.m_uWidth);
	xViewport.height = static_cast<float>(g_xEngine.VulkanSwapchain().m_axColourAttachments[g_xEngine.VulkanSwapchain().m_uCurrentImageIndex].m_xSurfaceInfo.m_uHeight);
	xViewport.minDepth = 0.0f;
	xViewport.maxDepth = 1.0f;

	vk::Rect2D xScissor{};
	xScissor.offset = vk::Offset2D(0, 0);
	xScissor.extent = vk::Extent2D(
		g_xEngine.VulkanSwapchain().m_axColourAttachments[g_xEngine.VulkanSwapchain().m_uCurrentImageIndex].m_xSurfaceInfo.m_uWidth,
		g_xEngine.VulkanSwapchain().m_axColourAttachments[g_xEngine.VulkanSwapchain().m_uCurrentImageIndex].m_xSurfaceInfo.m_uHeight);

	s_xCopyToFramebufferCmd.GetCurrentCmdBuffer().setViewport(0, 1, &xViewport);
	s_xCopyToFramebufferCmd.GetCurrentCmdBuffer().setScissor(0, 1, &xScissor);
}

bool Zenith_Vulkan_Swapchain::ShouldWaitOnImageAvailableSemaphore()
{
	return g_xEngine.VulkanSwapchain().m_bShouldWaitOnImageAvailableSem;
}

void Zenith_Vulkan_Swapchain::EndFrame()
{
	s_xCopyToFramebufferCmd.BeginRecording();

	BindAsTarget();

	s_xCopyToFramebufferCmd.SetPipeline(&s_xPipeline);

	s_xCopyToFramebufferCmd.SetVertexBuffer(g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	s_xCopyToFramebufferCmd.SetIndexBuffer(g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	s_xCopyToFramebufferCmd.BeginBind(0);
#ifdef ZENITH_DEBUG_VARIABLES
	if (dbg_bOutputMRT)
	{
		// When debugging, bind the MRT SRV
		Flux_ShaderResourceView* pxMRTSRV = g_xEngine.FluxGraphics().GetGBufferSRV((MRTIndex)dbg_uMRTIndex);
		if (pxMRTSRV)
		{
			s_xCopyToFramebufferCmd.BindSRV(pxMRTSRV, 0);
		}
	}
	else
#endif
	{
		// Bind final render target using SRV
		Flux_ShaderResourceView& xSRV = g_xEngine.FluxGraphics().GetFinalRenderTarget().SRV();
		if (xSRV.m_xImageViewHandle.IsValid())
		{
			s_xCopyToFramebufferCmd.BindSRV(&xSRV, 0);
		}
		else
		{
			Zenith_Assert(false, "Final render target SRV not created");
		}
	}

	s_xCopyToFramebufferCmd.DrawIndexed(6);

#ifdef ZENITH_TOOLS
	// Render ImGui on top of the game content
	// The render pass is still active from BindAsTarget()
	s_xCopyToFramebufferCmd.RenderImGui();
#endif

	s_xCopyToFramebufferCmd.GetCurrentCmdBuffer().endRenderPass();
	VkCheck(s_xCopyToFramebufferCmd.GetCurrentCmdBuffer().end());

	// Only signal renderFinished semaphore when we have a valid image to present
	// Otherwise the semaphore stays signaled and causes a double-signal on the next frame
	vk::SubmitInfo xRenderSubmitInfo = vk::SubmitInfo()
		.setCommandBufferCount(1)
		.setPCommandBuffers(&s_xCopyToFramebufferCmd.GetCurrentCmdBuffer())
		.setPWaitSemaphores(nullptr)
		.setWaitSemaphoreCount(0)
		.setPSignalSemaphores(g_xEngine.VulkanSwapchain().m_bShouldWaitOnImageAvailableSem ? &g_xEngine.VulkanSwapchain().m_axRenderFinishedSemaphores[g_xEngine.VulkanSwapchain().m_uCurrentImageIndex] : nullptr)
		.setSignalSemaphoreCount(g_xEngine.VulkanSwapchain().m_bShouldWaitOnImageAvailableSem ? 1 : 0);

	VkCheck(Zenith_Vulkan::GetQueue(COMMANDTYPE_GRAPHICS).submit(xRenderSubmitInfo, Zenith_Vulkan::GetCurrentInFlightFence()));

	if (g_xEngine.VulkanSwapchain().m_bShouldWaitOnImageAvailableSem)
	{
		vk::PresentInfoKHR presentInfo = vk::PresentInfoKHR()
			.setSwapchainCount(1)
			.setPSwapchains(&g_xEngine.VulkanSwapchain().m_xSwapChain)
			.setPImageIndices(&g_xEngine.VulkanSwapchain().m_uCurrentImageIndex)
			.setWaitSemaphoreCount(1)
			.setPWaitSemaphores(&g_xEngine.VulkanSwapchain().m_axRenderFinishedSemaphores[g_xEngine.VulkanSwapchain().m_uCurrentImageIndex]);

#ifdef ZENITH_ASSERT
		vk::Result eResult =
#endif
			Zenith_Vulkan::GetQueue(COMMANDTYPE_PRESENT).presentKHR(&presentInfo);

		Zenith_Assert(eResult == vk::Result::eSuccess || eResult == vk::Result::eErrorOutOfDateKHR || eResult == vk::Result::eSuboptimalKHR, "Failed to present");

	}
	// Frame counter advance is now owned by Flux_PerFrame::EndFrame, called
	// once at the bottom of Zenith_MainLoop. Removed the local counter bump
	// here to keep one source of truth.
}

vk::Semaphore& Zenith_Vulkan_Swapchain::GetCurrentImageAvailableSemaphore()
{
	return g_xEngine.VulkanSwapchain().m_axImageAvailableSemaphores[GetCurrentFrameIndex()];
}

uint32_t Zenith_Vulkan_Swapchain::GetCurrentFrameIndex()
{
	// Single source of truth — Flux_PerFrame owns the monotonic frame counter
	// and the ring index. The swapchain's previous s_uFrameIndex member has
	// been removed; backends and engine code that need the current ring slot
	// all derive it from here.
	return Flux_PerFrame::GetRingIndex();
}

Flux_RenderAttachment* Zenith_Vulkan_Swapchain::GetCurrentSwapchainTarget(uint32_t& uNumColourAttachments, Flux_RenderAttachment*& pxDepthStencil)
{
	uNumColourAttachments = 1;
	pxDepthStencil = nullptr;
	return &g_xEngine.VulkanSwapchain().m_axColourAttachments[g_xEngine.VulkanSwapchain().m_uCurrentImageIndex];
}
