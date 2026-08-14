#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Zenith_Vulkan_Swapchain.h"

#include "Zenith_Vulkan.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_PerFrame.h"
#include "Flux/Flux_RendererImpl.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_SwapchainPolicy.h"
#include "Flux/Present/Flux_PresentImpl.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Core/Zenith_CommandLine.h"
#include "Core/Zenith_PlatformStdio.h"
#include "Flux/Flux_Screenshot.h"

#include <cstdio>

#ifdef ZENITH_WINDOWS
#include "Zenith_Windows_Window.h"
#endif

#ifdef ZENITH_TOOLS
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#endif

// Phase 6b: swapchain state moved to Zenith_Vulkan_Swapchain held by
// Zenith_Engine. Access via g_xEngine.FluxSwapchain().m_xXxx.

uint32_t       Zenith_Vulkan_Swapchain::GetWidth()  { return Zenith_Vulkan_Swapchain::m_xExtent.width; }
uint32_t       Zenith_Vulkan_Swapchain::GetHeight() { return Zenith_Vulkan_Swapchain::m_xExtent.height; }
vk::Extent2D&  Zenith_Vulkan_Swapchain::GetExtent() { return Zenith_Vulkan_Swapchain::m_xExtent; }
vk::Format     Zenith_Vulkan_Swapchain::GetFormat() { return Zenith_Vulkan_Swapchain::m_xImageFormat; }

struct SwapChainSupportDetails
{
	vk::SurfaceCapabilitiesKHR m_xCapabilities;
	std::vector <vk::SurfaceFormatKHR> m_xFormats;
	std::vector <vk::PresentModeKHR> m_xPresentModes;
};

// Capabilities ONLY. The full QuerySwapChainSupport also enumerates formats and
// present modes -- two more WSI round trips and two heap-allocating vectors --
// which the per-frame rebuild check does not read. That check runs EVERY frame
// on a rotated Android surface, because requesting an IDENTITY preTransform
// against a ROTATE_90 surface makes VK_SUBOPTIMAL_KHR the permanent steady
// state, so the difference is per-frame cost on the exact platform this all
// exists to serve.
static vk::SurfaceCapabilitiesKHR QuerySurfaceCapabilities()
{
	Zenith_Vulkan_Swapchain& xSwapchain = g_xEngine.FluxSwapchain();
	return VkUnwrap(xSwapchain.m_pxVulkan->GetPhysicalDevice()
		.getSurfaceCapabilitiesKHR(xSwapchain.m_pxVulkan->GetSurface()));
}

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

// Bit values are fixed by the Vulkan spec; Flux_SurfaceTransform mirrors them so
// the policy can stay backend-neutral (and therefore testable in the Null config).
static_assert(static_cast<u_int32>(vk::SurfaceTransformFlagBitsKHR::eIdentity) == Flux_SurfaceTransform::uIDENTITY, "IDENTITY bit drifted");
static_assert(static_cast<u_int32>(vk::SurfaceTransformFlagBitsKHR::eRotate90) == Flux_SurfaceTransform::uROTATE_90, "ROTATE_90 bit drifted");
static_assert(static_cast<u_int32>(vk::SurfaceTransformFlagBitsKHR::eRotate180) == Flux_SurfaceTransform::uROTATE_180, "ROTATE_180 bit drifted");
static_assert(static_cast<u_int32>(vk::SurfaceTransformFlagBitsKHR::eRotate270) == Flux_SurfaceTransform::uROTATE_270, "ROTATE_270 bit drifted");
static_assert(static_cast<u_int32>(vk::SurfaceTransformFlagBitsKHR::eHorizontalMirror) == Flux_SurfaceTransform::uHORIZONTAL_MIRROR, "HORIZONTAL_MIRROR bit drifted");
static_assert(static_cast<u_int32>(vk::SurfaceTransformFlagBitsKHR::eHorizontalMirrorRotate90) == Flux_SurfaceTransform::uHORIZONTAL_MIRROR_ROTATE_90, "HM_ROTATE_90 bit drifted");
static_assert(static_cast<u_int32>(vk::SurfaceTransformFlagBitsKHR::eHorizontalMirrorRotate180) == Flux_SurfaceTransform::uHORIZONTAL_MIRROR_ROTATE_180, "HM_ROTATE_180 bit drifted");
static_assert(static_cast<u_int32>(vk::SurfaceTransformFlagBitsKHR::eHorizontalMirrorRotate270) == Flux_SurfaceTransform::uHORIZONTAL_MIRROR_ROTATE_270, "HM_ROTATE_270 bit drifted");
static_assert(static_cast<u_int32>(vk::SurfaceTransformFlagBitsKHR::eInherit) == Flux_SurfaceTransform::uINHERIT, "INHERIT bit drifted");
static_assert(std::numeric_limits<uint32_t>::max() == Flux_SwapchainPolicy::uNO_CURRENT_EXTENT, "currentExtent sentinel drifted");

// Blocks until the platform surface has a non-zero size again.
//
// MUST be called BEFORE QuerySwapChainSupport, not after. A minimised Win32
// window reports currentExtent {0,0} AND minImageExtent == maxImageExtent ==
// {0,0} (the spec explicitly allows maxImageExtent to be (0,0) when minimised).
// Waiting inside ChooseSwapExtent -- i.e. after the caps were already captured
// -- then clamped the restored window size against those stale zero caps and
// produced 0x0 all over again, recreating the very invalid swapchain the guard
// was added to prevent. Every capability read after this point (currentExtent,
// min/maxImageExtent, minImageCount, supportedTransforms, currentTransform) is
// therefore from the RESTORED surface.
static void WaitForPresentableSurface()
{
#ifdef ZENITH_WINDOWS
	GLFWwindow* pxWindow = Zenith_Window::GetInstance()->GetNativeWindow();
	int32_t iWidth = 0;
	int32_t iHeight = 0;
	glfwGetFramebufferSize(pxWindow, &iWidth, &iHeight);
	while (iWidth == 0 || iHeight == 0)
	{
		// Blocking wait -- the app is minimised, there is nothing to render, and
		// spinning here would burn a core.
		glfwWaitEvents();
		glfwGetFramebufferSize(pxWindow, &iWidth, &iHeight);
	}
#endif
	// Android has no equivalent wait here: the activity's own looper owns that,
	// and Zenith_Android_Main gates the frame on s_bWindowReady. ChooseSwapExtent
	// still guards the zero case below so a caller that ignores that gate gets a
	// loud error rather than an invalid swapchain.
}

static vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& xCapabilities)
{
	// Rejects BOTH the UINT32_MAX "you pick" sentinel and a zero extent. The zero
	// case is a surface with no presentable size (a MINIMISED window, or an
	// Android activity with no live surface): a 0x0 swapchain is invalid usage
	// and DebugCallback escalates the validation error into a process kill.
	// On Windows WaitForPresentableSurface has already blocked until this is
	// true, so the fall-through below is the sentinel path only.
	if (Flux_SwapchainPolicy::IsCurrentExtentUsable(xCapabilities.currentExtent.width,
			xCapabilities.currentExtent.height))
	{
		return xCapabilities.currentExtent;
	}

	int32_t iExtentWidth = 0;
	int32_t iExtentHeight = 0;
#ifdef ZENITH_WINDOWS
	glfwGetFramebufferSize(Zenith_Window::GetInstance()->GetNativeWindow(), &iExtentWidth, &iExtentHeight);
#else
	Zenith_Window::GetInstance()->GetSize(iExtentWidth, iExtentHeight);
#endif

	vk::Extent2D xExtent = { static_cast<uint32_t>(iExtentWidth), static_cast<uint32_t>(iExtentHeight) };
	xExtent.width = Zenith_Maths::Clamp(xExtent.width, xCapabilities.minImageExtent.width, xCapabilities.maxImageExtent.width);
	xExtent.height = Zenith_Maths::Clamp(xExtent.height, xCapabilities.minImageExtent.height, xCapabilities.maxImageExtent.height);

	if (!Flux_SwapchainPolicy::IsCurrentExtentUsable(xExtent.width, xExtent.height))
	{
		// Reachable only with no live surface (in practice: Android, called
		// without waiting for the window). A 1x1 swapchain is legal where a 0x0
		// one is not, so the process survives to report the real problem.
		Zenith_Error(LOG_CATEGORY_VULKAN,
			"Swapchain: no presentable surface extent (window %dx%d, caps min %ux%u max %ux%u); "
			"falling back to 1x1. Initialise ran with no live window -- the frame should be "
			"gated on the window being ready.",
			iExtentWidth, iExtentHeight,
			xCapabilities.minImageExtent.width, xCapabilities.minImageExtent.height,
			xCapabilities.maxImageExtent.width, xCapabilities.maxImageExtent.height);
		xExtent.width = xExtent.width == 0u ? 1u : xExtent.width;
		xExtent.height = xExtent.height == 0u ? 1u : xExtent.height;
	}
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

void Zenith_Vulkan_Swapchain::Initialise()
{
	// Self-wire cross-subsystem deps once (FluxBackendPresentation forbids
	// params on this Initialise()). Every other reach routes through these.
	// Initialise() also runs on swapchain recreation (resize) — re-caching the
	// same pointers each time is harmless.
	Zenith_Engine& xEngine = g_xEngine;
	m_pxVulkan       = &xEngine.FluxBackend();
	m_pxVulkanMemory = &xEngine.FluxMemory();
	m_pxFluxRenderer = &xEngine.FluxRenderer();
	m_pxProfiling    = &xEngine.Profiling();

	const vk::SurfaceKHR& xSurface = m_pxVulkan->GetSurface();

	// BEFORE the capability query, never after -- see WaitForPresentableSurface.
	WaitForPresentableSurface();

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

	// Swapchain readback (CLI --screenshot OR the programmatic
	// Flux_Screenshot::RequestDump path) copies the swapchain image to a
	// buffer, which requires TRANSFER_SRC usage. In TOOLS builds always add it
	// (negligible cost) so RequestDump works without the CLI flag; in shipping
	// builds add it only when --screenshot was requested, keeping default
	// swapchain creation byte-identical. Guarded on surface support (virtually
	// all desktop surfaces allow it).
#ifdef ZENITH_TOOLS
	const bool bWantTransferSrc = true;
#else
	const bool bWantTransferSrc = (Zenith_CommandLine::GetScreenshotPath() != nullptr);
#endif
	if (bWantTransferSrc &&
		(xSwapChainSupport.m_xCapabilities.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferSrc))
	{
		xCreateInfo.imageUsage |= vk::ImageUsageFlagBits::eTransferSrc;
	}

	uint32_t uGraphicsQueueIdx = m_pxVulkan->GetQueueIndex(COMMANDTYPE_GRAPHICS);
	uint32_t uPresentQueueIdx = m_pxVulkan->GetQueueIndex(COMMANDTYPE_PRESENT);
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
	// preTransform declares the rotation the APP has already baked into its
	// content. Zenith bakes none, so declaring currentTransform on a rotated
	// surface renders sideways -- which is what every landscape Android activity
	// on a portrait-native panel did. Ask for IDENTITY when the surface offers
	// it and let the presentation engine rotate. See Flux_SwapchainPolicy.
	const u_int32 uSupportedTransforms = static_cast<u_int32>(xSwapChainSupport.m_xCapabilities.supportedTransforms);
	const u_int32 uCurrentTransform = static_cast<u_int32>(xSwapChainSupport.m_xCapabilities.currentTransform);
	const u_int32 uChosenTransform = Flux_SwapchainPolicy::SelectPreTransform(uSupportedTransforms, uCurrentTransform);
	xCreateInfo.preTransform = static_cast<vk::SurfaceTransformFlagBitsKHR>(uChosenTransform);
	m_uSurfaceTransform = uCurrentTransform;

	if (Flux_SwapchainPolicy::WillRenderRotated(uSupportedTransforms, uCurrentTransform))
	{
		// Only reachable on a surface that refuses IDENTITY. Creating the
		// swapchain anyway beats failing, but say plainly why it looks wrong.
		Zenith_Warning(LOG_CATEGORY_VULKAN,
			"Surface does not support IDENTITY preTransform (current=0x%X, supported=0x%X); "
			"the image will be presented rotated. Fixing this needs pre-rotation baked into "
			"the projection/viewport in shared render code.",
			uCurrentTransform, uSupportedTransforms);

		// A quarter-turn preTransform puts the images in DISPLAY-native space, so
		// the extent has to be swapped to match or the frame is aspect-mangled on
		// top of being rotated. (Never taken on the IDENTITY path, where the
		// images live in window space and currentExtent is already right.)
		if (Flux_SwapchainPolicy::ShouldTransposeExtent(uChosenTransform))
		{
			std::swap(xCreateInfo.imageExtent.width, xCreateInfo.imageExtent.height);
			xExtent = xCreateInfo.imageExtent;
		}
	}
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

		// NO layout transition here. Swapchain images come out of
		// vkCreateSwapchainKHR in UNDEFINED and are owned by the presentation
		// engine until vkAcquireNextImageKHR hands one over -- transitioning
		// them here touched images we do not own, which is undefined behaviour
		// (validation: "performs a layout transition on presentable VkImage,
		// but the image has not been acquired"). The present render pass now
		// declares an UNDEFINED initial layout instead, so there is nothing to
		// establish up front. See TargetSetupToRenderPass's PRESENT case.

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
	// Surface transform, once per swapchain creation. Without this a rotated
	// surface is invisible in the logs and a correctly-oriented frame cannot be
	// distinguished from one that was never rotated in the first place -- which
	// is exactly what makes an orientation bug so hard to confirm either way.
	// 0x1=IDENTITY 0x2=ROT90 0x4=ROT180 0x8=ROT270.
	Zenith_Log(LOG_CATEGORY_VULKAN,
		"Swapchain surface transform: current=0x%X, supported=0x%X, requested preTransform=0x%X%s",
		uCurrentTransform, uSupportedTransforms, uChosenTransform,
		uChosenTransform == uCurrentTransform ? " (app-space == display-space)"
											  : " (presentation engine rotates)");
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

	// The present blit's command buffer. Its pipeline + shader + the blit
	// recording itself are owned by the backend-neutral Flux_Present feature.
	m_xCopyToFramebufferCmd.Initialise();

	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan swapchain initialised");
}

void Zenith_Vulkan_Swapchain::Shutdown()
{
	const vk::Device& xDevice = m_pxVulkan->GetDevice();

	// Tear down the present command buffer while the Vulkan device is still alive.
	// The blit pipeline + shader are owned by Flux_Present and torn down by its
	// Shutdown (the feature shutdown walk runs before the device is destroyed).
	m_xCopyToFramebufferCmd.GetCurrentCmdBuffer() = VK_NULL_HANDLE;
	m_xCopyToFramebufferCmd.SetCurrentRenderPass(VK_NULL_HANDLE);

	// Same views + registry handles the recreation path releases -- one
	// implementation so the two can never drift again (the recreation path used
	// to drop the handles, leaking two per image per resize).
	ReleaseImageViewsAndHandles();

	for (u_int u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		if (m_axImageAvailableSemaphores[u])
		{
			xDevice.destroySemaphore(m_axImageAvailableSemaphores[u]);
			m_axImageAvailableSemaphores[u] = VK_NULL_HANDLE;
		}
		if (m_axRenderFinishedSemaphores[u])
		{
			xDevice.destroySemaphore(m_axRenderFinishedSemaphores[u]);
			m_axRenderFinishedSemaphores[u] = VK_NULL_HANDLE;
		}
	}

	m_xImages.clear();

	if (m_xSwapChain)
	{
		xDevice.destroySwapchainKHR(m_xSwapChain);
		m_xSwapChain = VK_NULL_HANDLE;
	}

	m_xImageFormat = vk::Format::eUndefined;
	m_xExtent = vk::Extent2D();
	m_uCurrentImageIndex = 0;
	m_bShouldWaitOnImageAvailableSem = false;
	m_uSurfaceTransform = 0;
	m_eRecreateRequest = RECREATE_REQUEST_NONE;

	m_pxVulkan       = nullptr;
	m_pxVulkanMemory = nullptr;
	m_pxFluxRenderer = nullptr;
	m_pxProfiling    = nullptr;

	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan swapchain shut down");
}

void Zenith_Vulkan_Swapchain::ReleaseImageViewsAndHandles()
{
	const vk::Device& xDevice = m_pxVulkan->GetDevice();

	// The registry handles MUST go back with the views. Destroying only the
	// vk::ImageView (what the recreation path used to do) leaked two
	// Flux_ImageViewHandles per swapchain image on every single resize.
	for (u_int u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		Flux_RenderAttachment& xAttachment = m_axColourAttachments[u];
		if (xAttachment.SRV().m_xImageViewHandle.IsValid())
		{
			m_pxVulkanMemory->ReleaseImageViewHandle(xAttachment.SRV().m_xImageViewHandle);
		}
		if (xAttachment.RTV().m_xImageViewHandle.IsValid())
		{
			m_pxVulkanMemory->ReleaseImageViewHandle(xAttachment.RTV().m_xImageViewHandle);
		}
		xAttachment = Flux_RenderAttachment();
	}

	for (vk::ImageView& xImageView : m_xImageViews)
	{
		if (xImageView)
		{
			// Direct destroy, not deferred: every caller has already made the GPU idle.
			xDevice.destroyImageView(xImageView);
			xImageView = VK_NULL_HANDLE;
		}
	}
	m_xImageViews.clear();
}

void Zenith_Vulkan_Swapchain::RecreateSwapchain()
{
	const vk::Device& xDevice = m_pxVulkan->GetDevice();

	// Wait for GPU to finish using all resources before destroying them.
	// This prevents "semaphore in use" errors during window resize/maximize.
	m_pxVulkan->WaitForGPUIdle();

	ReleaseImageViewsAndHandles();

	for (uint32_t u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		xDevice.destroySemaphore(m_axImageAvailableSemaphores[u]);
		xDevice.destroySemaphore(m_axRenderFinishedSemaphores[u]);
	}
	m_xImages.clear();
	xDevice.destroySwapchainKHR(m_xSwapChain);

	Initialise();
	m_pxFluxRenderer->OnResChange();

	// Whatever prompted the rebuild is now satisfied by the fresh swapchain.
	m_eRecreateRequest = RECREATE_REQUEST_NONE;
}

bool Zenith_Vulkan_Swapchain::BeginFrame()
{
	m_pxProfiling->BeginProfileZone(ZENITH_PROFILE_ZONE("Flux Swapchain Begin Frame"));
	const vk::Device& xDevice = m_pxVulkan->GetDevice();

	// A deferred rebuild requested by last frame's acquire or present, so the
	// frame that discovered it could still be presented.
	if (m_eRecreateRequest != RECREATE_REQUEST_NONE)
	{
		bool bRebuild = true;
		if (m_eRecreateRequest == RECREATE_REQUEST_IF_CHANGED)
		{
			// Advisory (SUBOPTIMAL): only rebuild if something the swapchain is
			// built FROM actually moved. Capabilities only -- see
			// QuerySurfaceCapabilities for why the full query is too expensive here.
			const vk::SurfaceCapabilitiesKHR xCaps = QuerySurfaceCapabilities();
			bRebuild = Flux_SwapchainPolicy::ShouldRecreateForSuboptimal(
				xCaps.currentExtent.width, xCaps.currentExtent.height,
				static_cast<u_int32>(xCaps.currentTransform),
				m_xExtent.width, m_xExtent.height, m_uSurfaceTransform);
		}

		if (bRebuild)
		{
			RecreateSwapchain();
			m_pxProfiling->EndProfileZone(ZENITH_PROFILE_ZONE("Flux Swapchain Begin Frame"));
			// Abandon this frame. Safe because the ring slot's fence is only
			// reset immediately before its submit -- see the lifecycle note in
			// Zenith_Vulkan_PerFrame::BeginFrame.
			return false;
		}

		// Suboptimal for a reason we cannot act on. Stop asking. (Never reached
		// for RECREATE_REQUEST_ALWAYS, which is mandatory.)
		m_eRecreateRequest = RECREATE_REQUEST_NONE;
	}

	//#TO_TODO: -1 here to shut up validation layer
	vk::Result eResult = xDevice.acquireNextImageKHR(m_xSwapChain, UINT64_MAX - 1, m_axImageAvailableSemaphores[GetCurrentFrameIndex()], nullptr, &m_uCurrentImageIndex);

	const bool bImageAcquired = eResult == vk::Result::eSuccess || eResult == vk::Result::eSuboptimalKHR;
	m_bShouldWaitOnImageAvailableSem = bImageAcquired;

	Zenith_Assert(bImageAcquired || eResult == vk::Result::eErrorOutOfDateKHR, "Failed to acquire swapchain image");

	if (eResult == vk::Result::eErrorOutOfDateKHR)
	{
		RecreateSwapchain();
		m_pxProfiling->EndProfileZone(ZENITH_PROFILE_ZONE("Flux Swapchain Begin Frame"));
		return false;
	}

	if (eResult == vk::Result::eSuboptimalKHR)
	{
		// Still a usable image, so present this frame and rebuild at the top of
		// the next one. Without this the swapchain stayed stale forever on any
		// resize/rotation that never escalated to OUT_OF_DATE.
		m_eRecreateRequest = RECREATE_REQUEST_IF_CHANGED;
	}

	if (!bImageAcquired)
	{
		// Neither SUCCESS/SUBOPTIMAL nor OUT_OF_DATE -- e.g. SURFACE_LOST from a
		// display hot-unplug, or an Android surface torn down mid-frame. Without
		// a recreate request the main loop would abandon EVERY subsequent frame
		// against the same dead swapchain, silently, forever. A forced rebuild is
		// the only recovery available from here; if the surface itself is gone it
		// will keep failing, but loudly and while trying.
		Zenith_Error(LOG_CATEGORY_VULKAN,
			"Swapchain acquire failed with %d; forcing a swapchain rebuild to attempt recovery.",
			static_cast<int>(eResult));
		m_eRecreateRequest = RECREATE_REQUEST_ALWAYS;
		m_pxProfiling->EndProfileZone(ZENITH_PROFILE_ZONE("Flux Swapchain Begin Frame"));
		return false;
	}
	m_pxProfiling->EndProfileZone(ZENITH_PROFILE_ZONE("Flux Swapchain Begin Frame"));
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

		std::FILE* pFile = Zenith_PlatformStdio::OpenFile(szPath, "wb");
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
	Zenith_Vulkan_CommandBuffer& xCmd = m_xCopyToFramebufferCmd;

	xCmd.BeginRecording();

	BindAsTarget();

	// The present blit pipeline + recording is owned by the backend-neutral
	// Flux_Present feature; delegate to it. It samples the Final RT (or a debug
	// MRT) into the backbuffer, inside the render pass BindAsTarget() began.
	g_xEngine.Present().RecordBlit(xCmd);

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
		.setPSignalSemaphores(m_bShouldWaitOnImageAvailableSem ? &m_axRenderFinishedSemaphores[m_uCurrentImageIndex] : nullptr)
		.setSignalSemaphoreCount(m_bShouldWaitOnImageAvailableSem ? 1 : 0);

	// Unsignal the ring slot's fence immediately before the one submit that
	// re-signals it. Paired here rather than in PerFrame::BeginFrame so a frame
	// abandoned by an out-of-date acquire (BeginFrame returned false, so we never
	// got here) leaves the fence signalled instead of deadlocking the slot.
	m_pxVulkan->ResetCurrentInFlightFence();

	VkCheck(m_pxVulkan->GetQueue(COMMANDTYPE_GRAPHICS).submit(xRenderSubmitInfo, m_pxVulkan->GetCurrentInFlightFence()));

	// --screenshot: capture the freshly-rendered swapchain image on the
	// requested frame, before present (the helper restores the PresentSrc
	// layout so present stays valid). Fires exactly once — the frame index
	// equals the target on a single EndFrame.
	{
		const char* szScreenshotPath = Zenith_CommandLine::GetScreenshotPath();
		if (szScreenshotPath != nullptr &&
			g_xEngine.Frame().GetFrameIndex() == Zenith_CommandLine::GetScreenshotFrame())
		{
			WriteSwapchainScreenshotTGA(*this, szScreenshotPath);
		}

		// Programmatic dump request (Flux_Screenshot::RequestDump) — lets a test
		// capture the full framebuffer at its exact step-frame, independent of
		// the global frame index the CLI flag keys off.
		std::string strPendingDump;
		if (Flux_Screenshot::ConsumePendingDump(strPendingDump))
		{
			WriteSwapchainScreenshotTGA(*this, strPendingDump.c_str());
		}
	}

	if (m_bShouldWaitOnImageAvailableSem)
	{
		vk::PresentInfoKHR presentInfo = vk::PresentInfoKHR()
			.setSwapchainCount(1)
			.setPSwapchains(&m_xSwapChain)
			.setPImageIndices(&m_uCurrentImageIndex)
			.setWaitSemaphoreCount(1)
			.setPWaitSemaphores(&m_axRenderFinishedSemaphores[m_uCurrentImageIndex]);

		// The result is consumed unconditionally now (not just under an assert):
		// present is the FIRST place a resize or rotation shows up on many
		// drivers, and dropping it left the swapchain stale until something else
		// escalated to OUT_OF_DATE.
		const vk::Result ePresentResult = m_pxVulkan->GetQueue(COMMANDTYPE_PRESENT).presentKHR(&presentInfo);

		Zenith_Assert(ePresentResult == vk::Result::eSuccess || ePresentResult == vk::Result::eErrorOutOfDateKHR || ePresentResult == vk::Result::eSuboptimalKHR, "Failed to present");

		if (ePresentResult == vk::Result::eErrorOutOfDateKHR)
		{
			// MANDATORY: the swapchain is retired. Must NOT be routed through the
			// did-anything-change check -- the reason it retired need not show up
			// in VkSurfaceCapabilitiesKHR at all.
			m_eRecreateRequest = RECREATE_REQUEST_ALWAYS;
		}
		else if (ePresentResult == vk::Result::eSuboptimalKHR)
		{
			// Advisory: rebuild at the top of the next BeginFrame, but only if
			// something actually changed.
			m_eRecreateRequest = RECREATE_REQUEST_IF_CHANGED;
		}
	}
	// Frame index advance is owned by Zenith_MainLoop (FrameContext), which
	// bumps it once at the bottom of the loop. Removed the local counter bump
	// here to keep one source of truth.
}

vk::Semaphore& Zenith_Vulkan_Swapchain::GetCurrentImageAvailableSemaphore()
{
	return m_axImageAvailableSemaphores[GetCurrentFrameIndex()];
}

uint32_t Zenith_Vulkan_Swapchain::GetCurrentFrameIndex()
{
	// Single source of truth — FrameContext owns the monotonic frame index
	// and the ring index. The swapchain's previous s_uFrameIndex member has
	// been removed; backends and engine code that need the current ring slot
	// all derive it from here.
	//
	// Deliberately a point-of-use g_xEngine reach, NOT a self-wired member
	// pointer: this accessor is called BEFORE Initialise() runs — boot-time
	// GPU asset uploads (Zenith_Engine::InitialiseGPUAssets → MemoryManager::
	// BeginFrame → CommandBuffer::BeginRecording) ask for the ring slot while
	// the swapchain's dep pointers are still null. FrameContext is allocated
	// up-front in Zenith_Engine::Initialise, so g_xEngine.Frame() is valid
	// throughout that window (and the index is still 0 there, so the answer
	// matches the old unwired-seam ring-slot-0 guard).
	return g_xEngine.Frame().GetRingIndex();
}

Flux_RenderAttachment* Zenith_Vulkan_Swapchain::GetCurrentSwapchainTarget(uint32_t& uNumColourAttachments, Flux_RenderAttachment*& pxDepthStencil)
{
	uNumColourAttachments = 1;
	pxDepthStencil = nullptr;
	return &m_axColourAttachments[m_uCurrentImageIndex];
}
