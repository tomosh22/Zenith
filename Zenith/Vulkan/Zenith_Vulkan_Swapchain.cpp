#include "Zenith.h"

#include "Zenith_Vulkan_Swapchain.h"

#include "Zenith_Vulkan.h"
#include "Flux/Flux_Enums.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_RenderTargets.h"
#include "Zenith_Vulkan_MemoryManager.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Profiling/Zenith_Profiling.h"

#include "AssetHandling/Zenith_AssetHandler.h"

#include "Zenith_OS_Include.h" //#TO for Zenith_Window
#ifdef ZENITH_WINDOWS
#include "Zenith_Windows_Window.h"
#endif

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

vk::SwapchainKHR Zenith_Vulkan_Swapchain::s_xSwapChain;
std::vector<vk::Image> Zenith_Vulkan_Swapchain::s_xImages;
std::vector<vk::ImageView> Zenith_Vulkan_Swapchain::s_xImageViews;
vk::Format Zenith_Vulkan_Swapchain::s_xImageFormat;
vk::Extent2D Zenith_Vulkan_Swapchain::s_xExtent;
uint32_t Zenith_Vulkan_Swapchain::s_uCurrentImageIndex = 0;
vk::Semaphore Zenith_Vulkan_Swapchain::s_axImageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
uint32_t Zenith_Vulkan_Swapchain::s_uFrameIndex = 0;
Flux_TargetSetup Zenith_Vulkan_Swapchain::s_axTargetSetups[MAX_FRAMES_IN_FLIGHT];
bool Zenith_Vulkan_Swapchain::s_bShouldWaitOnImageAvailableSem;

static Zenith_Vulkan_Shader s_xShader;
static Zenith_Vulkan_Pipeline s_xPipeline;
static Zenith_Vulkan_CommandBuffer s_xCopyToFramebufferCmd;

DEBUGVAR bool dbg_bOutputMRT = false;
DEBUGVAR uint32_t dbg_uMRTIndex = MRT_INDEX_DIFFUSE;

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
	
}

void Zenith_Vulkan_Swapchain::InitialiseCopyToFramebufferCommands()
{
	s_xCopyToFramebufferCmd.Initialise();

	s_xShader.Initialise("Flux_Fullscreen_UV.vert", "Flux_TexturedQuad.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_pxTargetSetup = &s_axTargetSetups[0];
	xPipelineSpec.m_pxShader = &s_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	Flux_PipelineLayout& xLayout = xPipelineSpec.m_xPipelineLayout;
	xLayout.m_uNumDescriptorSets = 1;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_TEXTURE;

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

	volatile bool a = false;
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

	for (uint32_t u = 0; u < s_xImages.size(); u++)
	{
		vk::Image& xImage = s_xImages[u];
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

		s_xImageViews[u] = xDevice.createImageView(xViewCreate);

		s_axTargetSetups[u].m_axColourAttachments[0].m_pxTargetTexture = Zenith_AssetHandler::CreateDummyTexture("Swapchain " + std::to_string(u));

		s_axTargetSetups[u].m_axColourAttachments[0].m_pxTargetTexture->SetImage(xImage);
		s_axTargetSetups[u].m_axColourAttachments[0].m_pxTargetTexture->SetImageView(s_xImageViews[u]);

		s_axTargetSetups[u].m_axColourAttachments[0].m_uWidth = xExtent.width;
		s_axTargetSetups[u].m_axColourAttachments[0].m_uHeight = xExtent.height;
		//#TO_TODO: stop hardcoding swapchain colour format
		s_axTargetSetups[u].m_axColourAttachments[0].m_eColourFormat = COLOUR_FORMAT_BGRA8_SRGB;
		s_axTargetSetups[u].m_axColourAttachments[0].m_pxTargetTexture->SetFormat(xSurfaceFormat.format);
	}
	

	s_xImageFormat = xSurfaceFormat.format;
	s_xExtent = xExtent;

	vk::SemaphoreCreateInfo xSemaphoreInfo;

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		s_axImageAvailableSemaphores[i] = xDevice.createSemaphore(xSemaphoreInfo);
	}

	InitialiseCopyToFramebufferCommands();

	Zenith_Log("Vulkan swapchain initialised");

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
	vk::Result eResult = xDevice.acquireNextImageKHR(s_xSwapChain, UINT64_MAX - 1, s_axImageAvailableSemaphores[s_uFrameIndex], nullptr, &s_uCurrentImageIndex);

	s_bShouldWaitOnImageAvailableSem = eResult == vk::Result::eSuccess;

	Zenith_Assert(eResult == vk::Result::eSuccess || eResult == vk::Result::eErrorOutOfDateKHR, "Failed to acquire swapchain image");

	if (eResult == vk::Result::eErrorOutOfDateKHR)
	{
		//#TO_TODO: cleanup the rest, at least image views, probably other things
		xDevice.destroySwapchainKHR(s_xSwapChain);
		Initialise();
		Flux::OnResChange();
		//xDevice.resetFences(1, &Zenith_Vulkan::GetNextInFlightFence());
		//s_uFrameIndex = (s_uFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
		//return false;
	}
	Zenith_Profiling::EndProfile(ZENITH_PROFILE_INDEX__FLUX_SWAPCHAIN_BEGIN_FRAME);
	return true;
}

void Zenith_Vulkan_Swapchain::BindAsTarget()
{
	uint32_t uNumColourAttachments = 1;

#if 1//def ZENITH_DEBUG
	vk::RenderPass xRenderPass = Zenith_Vulkan_Pipeline::TargetSetupToRenderPass(s_axTargetSetups[s_uCurrentImageIndex], LOAD_ACTION_CLEAR, STORE_ACTION_STORE, LOAD_ACTION_CLEAR, STORE_ACTION_DONTCARE, RENDER_TARGET_USAGE_PRESENT);
#else
	vk::RenderPass xRenderPass = Zenith_Vulkan_Pipeline::TargetSetupToRenderPass(s_axTargetSetups[s_uCurrentImageIndex], LOAD_ACTION_DONTCARE, STORE_ACTION_STORE, LOAD_ACTION_DONTCARE, STORE_ACTION_DONTCARE, RENDER_TARGET_USAGE_PRESENT);
#endif

	vk::Framebuffer xFramebuffer = Zenith_Vulkan_Pipeline::TargetSetupToFramebuffer(s_axTargetSetups[s_uCurrentImageIndex], Zenith_Vulkan_Swapchain::GetWidth(), Zenith_Vulkan_Swapchain::GetHeight(), xRenderPass);

	vk::ClearValue xClear;
	vk::ClearColorValue xClearColourValue(0.f, 0.f, 0.f, 1.f);
	xClear.color = xClearColourValue;

	vk::RenderPassBeginInfo xRenderPassInfo = vk::RenderPassBeginInfo()
		.setRenderPass(xRenderPass)
		.setFramebuffer(xFramebuffer)
		.setRenderArea({ {0,0}, s_xExtent })
		.setPClearValues(&xClear)
		.setClearValueCount(1);

	s_xCopyToFramebufferCmd.GetCurrentCmdBuffer().beginRenderPass(xRenderPassInfo, vk::SubpassContents::eInline);

	//flipping because porting from opengl
	vk::Viewport xViewport{};
	xViewport.x = 0;
	xViewport.y = s_axTargetSetups[s_uCurrentImageIndex].m_axColourAttachments[0].m_uHeight;
	xViewport.width = s_axTargetSetups[s_uCurrentImageIndex].m_axColourAttachments[0].m_uWidth;
	xViewport.height = -1 * (float)s_axTargetSetups[s_uCurrentImageIndex].m_axColourAttachments[0].m_uHeight;
	xViewport.minDepth = 0;
	xViewport.maxDepth = 1;

	vk::Rect2D xScissor{};
	xScissor.offset = vk::Offset2D(0, 0);
	xScissor.extent = vk::Extent2D(xViewport.width, xViewport.y);

	s_xCopyToFramebufferCmd.GetCurrentCmdBuffer().setViewport(0, 1, &xViewport);
	s_xCopyToFramebufferCmd.GetCurrentCmdBuffer().setScissor(0, 1, &xScissor);
}

void Zenith_Vulkan_Swapchain::CopyToFramebuffer()
{
	
}

bool Zenith_Vulkan_Swapchain::ShouldWaitOnImageAvailableSemaphore()
{
	return s_bShouldWaitOnImageAvailableSem;
}

void Zenith_Vulkan_Swapchain::EndFrame()
{
	s_xCopyToFramebufferCmd.BeginRecording();

	BindAsTarget();

	s_xCopyToFramebufferCmd.SetPipeline(&s_xPipeline);

	s_xCopyToFramebufferCmd.SetVertexBuffer(Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	s_xCopyToFramebufferCmd.SetIndexBuffer(Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	s_xCopyToFramebufferCmd.BeginBind(0);
#ifdef ZENITH_DEBUG_VARIABLES
	if (dbg_bOutputMRT)
	{
		s_xCopyToFramebufferCmd.BindTexture(&Flux_Graphics::GetGBufferTexture((MRTIndex)dbg_uMRTIndex), 0);
	}
	else
#endif
	{
		s_xCopyToFramebufferCmd.BindTexture(Flux_Graphics::s_xFinalRenderTarget.m_axColourAttachments[0].m_pxTargetTexture, 0);
	}

	s_xCopyToFramebufferCmd.DrawIndexed(6);

#ifdef ZENITH_TOOLS
	vk::CommandBuffer& xCmd = s_xCopyToFramebufferCmd.GetCurrentCmdBuffer();

	xCmd.endRenderPass();

	vk::RenderPassBeginInfo xRenderPassInfo = vk::RenderPassBeginInfo()
		.setRenderPass(Zenith_Vulkan::s_xImGuiRenderPass)
		.setFramebuffer(Zenith_Vulkan_Pipeline::TargetSetupToFramebuffer(s_axTargetSetups[s_uCurrentImageIndex], Zenith_Vulkan_Swapchain::GetWidth(), Zenith_Vulkan_Swapchain::GetHeight(), Zenith_Vulkan::s_xImGuiRenderPass))
		.setRenderArea({ {0,0}, s_xExtent });

	xCmd.beginRenderPass(xRenderPassInfo, vk::SubpassContents::eInline);

	vk::Viewport xViewport{};
	xViewport.x = 0;
	xViewport.y = 0;
	xViewport.width = s_xExtent.width;
	xViewport.height = s_xExtent.height;
	xViewport.minDepth = 0;
	xViewport.maxDepth = 1;

	vk::Rect2D xScissor{};
	xScissor.offset = vk::Offset2D(0, 0);
	xScissor.extent = s_xExtent;

	xCmd.setViewport(0, 1, &xViewport);
	xCmd.setScissor(0, 1, &xScissor);

	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), xCmd);
#endif

	s_xCopyToFramebufferCmd.GetCurrentCmdBuffer().endRenderPass();
	s_xCopyToFramebufferCmd.GetCurrentCmdBuffer().end();

	vk::SubmitInfo xRenderSubmitInfo = vk::SubmitInfo()
		.setCommandBufferCount(1)
		.setPCommandBuffers(&s_xCopyToFramebufferCmd.GetCurrentCmdBuffer())
		.setPWaitSemaphores(nullptr)
		.setPSignalSemaphores(nullptr)
		.setWaitSemaphoreCount(0)
		.setSignalSemaphoreCount(0);

	Zenith_Vulkan::GetQueue(COMMANDTYPE_GRAPHICS).submit(xRenderSubmitInfo, Zenith_Vulkan::GetCurrentInFlightFence());

	if (s_bShouldWaitOnImageAvailableSem)
	{
		vk::PresentInfoKHR presentInfo = vk::PresentInfoKHR()
			.setSwapchainCount(1)
			.setPSwapchains(&s_xSwapChain)
			.setPImageIndices(&s_uCurrentImageIndex)
			.setWaitSemaphoreCount(0).
			setPWaitSemaphores(nullptr);

#ifdef ZENITH_ASSERT
		vk::Result eResult =
#endif
			Zenith_Vulkan::GetQueue(COMMANDTYPE_PRESENT).presentKHR(&presentInfo);

		Zenith_Assert(eResult == vk::Result::eSuccess || eResult == vk::Result::eErrorOutOfDateKHR || eResult == vk::Result::eSuboptimalKHR, "Failed to present");

	}
	s_uFrameIndex = (s_uFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

vk::Semaphore& Zenith_Vulkan_Swapchain::GetCurrentImageAvailableSemaphore()
{
	return s_axImageAvailableSemaphores[s_uFrameIndex];
}
