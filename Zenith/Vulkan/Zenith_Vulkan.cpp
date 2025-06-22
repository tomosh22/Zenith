#include "Zenith.h"

#include "Vulkan/Zenith_Vulkan.h"
#include "Flux/Flux.h"

#ifdef ZENITH_WINDOWS
#include "Zenith_Windows_Window.h"
#endif

#ifdef ZENITH_TOOLS
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#ifdef ZENITH_WINDOWS
#include "backends/imgui_impl_glfw.h"
#endif //ZENITH_WINDOWS
vk::RenderPass Zenith_Vulkan::s_xImGuiRenderPass;
#endif //ZENITH_TOOLS

#ifdef ZENITH_DEBUG_VARIABLES
#include "DebugVariables/Zenith_DebugVariables.h"
#endif

#ifdef ZENITH_DEBUG
static std::vector<const char*> s_xValidationLayers = { "VK_LAYER_KHRONOS_validation", /*"VK_LAYER_KHRONOS_synchronization2"*/};
#endif

static const char* s_aszDeviceExtensions[] = {
				VK_KHR_SWAPCHAIN_EXTENSION_NAME,
				//VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
#ifdef ZENITH_RAYTRACING
				VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
				VK_KHR_SPIRV_1_4_EXTENSION_NAME,
				VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
				VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
				VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
				VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
				VK_KHR_RAY_QUERY_EXTENSION_NAME,
				VK_NV_RAY_TRACING_EXTENSION_NAME,
#endif
				VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME,
};

vk::Instance Zenith_Vulkan::s_xInstance;
vk::DebugUtilsMessengerEXT Zenith_Vulkan::s_xDebugMessenger;
vk::SurfaceKHR Zenith_Vulkan::s_xSurface;
vk::PhysicalDevice Zenith_Vulkan::s_xPhysicalDevice;
Zenith_Vulkan::GPUCapabilities Zenith_Vulkan::s_xGPUCapabilties;
uint32_t Zenith_Vulkan::s_auQueueIndices[COMMANDTYPE_MAX];
vk::Device Zenith_Vulkan::s_xDevice;
vk::Queue Zenith_Vulkan::s_axQueues[COMMANDTYPE_MAX];
vk::CommandPool Zenith_Vulkan::s_axCommandPools[COMMANDTYPE_MAX];
vk::DescriptorPool Zenith_Vulkan::s_xDefaultDescriptorPool;
Zenith_Vulkan_PerFrame Zenith_Vulkan::s_axPerFrame[MAX_FRAMES_IN_FLIGHT];
Zenith_Vulkan_PerFrame* Zenith_Vulkan::s_pxCurrentFrame = nullptr;

std::vector<const Zenith_Vulkan_CommandBuffer*> Zenith_Vulkan::s_xPendingCommandBuffers[RENDER_ORDER_MAX];

DEBUGVAR bool dbg_bSubmitDrawCalls = true;
const vk::DescriptorPool& Zenith_Vulkan::GetCurrentPerFrameDescriptorPool()
{
	//#TO_TODO: current thread
	return s_pxCurrentFrame->m_axDescriptorPools[0];
}
vk::Fence& Zenith_Vulkan::GetCurrentInFlightFence()
{
	return s_pxCurrentFrame->m_xFence;
}

vk::Fence& Zenith_Vulkan::GetPreviousInFlightFence()
{
	uint32_t uPreviousFrame = (Zenith_Vulkan_Swapchain::GetCurrentFrameIndex() + MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT;
	return s_axPerFrame[uPreviousFrame].m_xFence;
}

vk::Fence& Zenith_Vulkan::GetNextInFlightFence()
{
	uint32_t uPreviousFrame = (Zenith_Vulkan_Swapchain::GetCurrentFrameIndex() - MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT;
	return s_axPerFrame[uPreviousFrame].m_xFence;
}

const bool Zenith_Vulkan::ShouldSubmitDrawCalls() { return dbg_bSubmitDrawCalls; }

void Zenith_Vulkan::Initialise()
{
	CreateInstance();
#ifdef ZENITH_DEBUG
	CreateDebugMessenger();
#endif
	CreateSurface();
	CreatePhysicalDevice();
	CreateQueueFamilies();
	CreateDevice();
	CreateCommandPools();
	CreateDefaultDescriptorPool();

	for (Zenith_Vulkan_PerFrame& xFrame : s_axPerFrame)
	{
		xFrame.Initialise();
	}

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Submit Draw Calls" }, dbg_bSubmitDrawCalls);
#endif

	s_pxCurrentFrame = &s_axPerFrame[0];
}

void Zenith_Vulkan::BeginFrame()
{
	s_pxCurrentFrame->BeginFrame();
}

void Zenith_Vulkan::EndFrame()
{
	static_assert(RENDER_ORDER_MEMORY_UPDATE == 0u, "Memory update needs to come first");

	vk::PipelineStageFlags eMemWaitStages = vk::PipelineStageFlagBits::eAllCommands;
	vk::PipelineStageFlags eRenderWaitStages = vk::PipelineStageFlagBits::eAllCommands;

	//#TO_TODO: stop making this every frame
	vk::Semaphore xMemorySemaphore = s_xDevice.createSemaphore(vk::SemaphoreCreateInfo());

	std::vector<vk::CommandBuffer> xPlatformMemoryCmdBufs;
	for (const Zenith_Vulkan_CommandBuffer* pCmdBuf : s_xPendingCommandBuffers[RENDER_ORDER_MEMORY_UPDATE])
	{
		const vk::CommandBuffer& xBuf = *reinterpret_cast<const vk::CommandBuffer*>(pCmdBuf);
		xPlatformMemoryCmdBufs.push_back(xBuf);
	}

	const bool bShouldWait = Zenith_Vulkan_Swapchain::ShouldWaitOnImageAvailableSemaphore();
	vk::SubmitInfo xMemorySubmitInfo = vk::SubmitInfo()
		.setCommandBufferCount(xPlatformMemoryCmdBufs.size())
		.setPCommandBuffers(xPlatformMemoryCmdBufs.data())
		.setPSignalSemaphores(&xMemorySemaphore)
		.setSignalSemaphoreCount(1)
		.setWaitDstStageMask(eMemWaitStages)
		.setPWaitSemaphores(bShouldWait ? &Zenith_Vulkan_Swapchain::GetCurrentImageAvailableSemaphore() : nullptr)
		.setWaitSemaphoreCount(bShouldWait);

	//#TO_TODO: change this to copy queue, how do I make sure this finishes before graphics?
	s_axQueues[COMMANDTYPE_GRAPHICS].submit(xMemorySubmitInfo, VK_NULL_HANDLE);

	std::vector<vk::CommandBuffer> xPlatformCmdBufs;
	for (uint32_t i = RENDER_ORDER_MEMORY_UPDATE + 1; i < RENDER_ORDER_MAX; i++)
	{
		for (const Zenith_Vulkan_CommandBuffer* pCmdBuf : s_xPendingCommandBuffers[i])
		{
			const vk::CommandBuffer& xBuf = *reinterpret_cast<const vk::CommandBuffer*>(pCmdBuf);
			xPlatformCmdBufs.push_back(xBuf);
		}
	}

	vk::SubmitInfo xRenderSubmitInfo = vk::SubmitInfo()
		.setCommandBufferCount(xPlatformCmdBufs.size())
		.setPCommandBuffers(xPlatformCmdBufs.data())
		.setPWaitSemaphores(&xMemorySemaphore)
		.setPSignalSemaphores(nullptr)
		.setWaitSemaphoreCount(1)
		.setSignalSemaphoreCount(0)
		.setWaitDstStageMask(eRenderWaitStages);

	s_axQueues[COMMANDTYPE_GRAPHICS].submit(xRenderSubmitInfo, GetCurrentInFlightFence());

	for (uint32_t i = 0; i < RENDER_ORDER_MAX; i++)
	{
		s_xPendingCommandBuffers[i].clear();
	}

	//#TO_TODO: plug semaphore leak
	//s_xDevice.destroySemaphore(xMemorySemaphore);

	s_pxCurrentFrame = &s_axPerFrame[Zenith_Vulkan_Swapchain::GetCurrentFrameIndex()];
}

void Zenith_Vulkan::CreateInstance()
{
	vk::ApplicationInfo xAppInfo = vk::ApplicationInfo()
		.setPApplicationName("Zenith_Vulkan")
		.setApplicationVersion(VK_MAKE_VERSION(1, 0, 0))
		.setPEngineName("Zenith")
		.setEngineVersion(VK_MAKE_VERSION(1, 0, 0))
		.setApiVersion(VK_API_VERSION_1_3);

#ifdef ZENITH_WINDOWS
	uint32_t uGLFWExtensionCount = 0;
	const char** pszGLFWExtensions = glfwGetRequiredInstanceExtensions(&uGLFWExtensionCount);
	std::vector<const char*> xExtensions(pszGLFWExtensions, pszGLFWExtensions + uGLFWExtensionCount);
#ifdef ZENITH_DEBUG
	xExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
#else
#error #TO_TODO: vulkan with not windows
#endif

	vk::InstanceCreateInfo xInstanceInfo = vk::InstanceCreateInfo()
		.setPApplicationInfo(&xAppInfo)
		.setEnabledExtensionCount(xExtensions.size())
		.setPpEnabledExtensionNames(xExtensions.data())
#ifdef ZENITH_DEBUG
		.setEnabledLayerCount(s_xValidationLayers.size())
		.setPpEnabledLayerNames(s_xValidationLayers.data());
#else
		.setEnabledLayerCount(0);
#endif
	s_xInstance = vk::createInstance(xInstanceInfo);

	Zenith_Log("Vulkan instance created");
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL Zenith_Vulkan::DebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT eMessageSeverity, vk::DebugUtilsMessageTypeFlagsEXT eMessageType, const vk::DebugUtilsMessengerCallbackDataEXT* pxCallbackData, void* pUserData)
{
	if (eMessageSeverity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
	{
		Zenith_Error("%s%s", "Zenith_Vulkan::DebugCallback: ", pxCallbackData->pMessage);
	}
		__debugbreak();
	return VK_FALSE;
}

#ifdef ZENITH_DEBUG
void Zenith_Vulkan::CreateDebugMessenger()
{
	vk::DebugUtilsMessengerCreateInfoEXT xCreateInfo = vk::DebugUtilsMessengerCreateInfoEXT()
		.setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
		.setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
			vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
			vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
		.setPfnUserCallback((PFN_vkDebugUtilsMessengerCallbackEXT)DebugCallback)
		.setPUserData(nullptr);
	s_xDebugMessenger = s_xInstance.createDebugUtilsMessengerEXT(
		xCreateInfo,
		nullptr,
		vk::detail::DispatchLoaderDynamic(s_xInstance, vkGetInstanceProcAddr)
	);

	Zenith_Log("Vulkan debug messenger created");
}
#endif

void Zenith_Vulkan::CreateSurface()
{
	GLFWwindow* pxWindow = Zenith_Window::GetInstance()->GetNativeWindow();
	glfwCreateWindowSurface(s_xInstance, pxWindow, nullptr, (VkSurfaceKHR*)&s_xSurface);

	Zenith_Log("Vulkan surface created");
}

void Zenith_Vulkan::CreatePhysicalDevice()
{
	uint32_t uNumDevices;
	s_xInstance.enumeratePhysicalDevices(&uNumDevices, nullptr);
	Zenith_Log("%u physical vulkan devices to choose from", uNumDevices);
	std::vector<vk::PhysicalDevice> xDevices;
	xDevices.resize(uNumDevices);
	s_xInstance.enumeratePhysicalDevices(&uNumDevices, xDevices.data());
	for (const vk::PhysicalDevice& xDevice : xDevices)
	{
		//#TO_TODO: check if physical device is suitable
		if (true)
		{
			s_xPhysicalDevice = xDevice;
			break;
		}
	}

	const vk::PhysicalDeviceProperties& xProps = s_xPhysicalDevice.getProperties();
	s_xGPUCapabilties.m_uMaxTextureWidth = xProps.limits.maxImageDimension2D;
	s_xGPUCapabilties.m_uMaxTextureHeight = xProps.limits.maxImageDimension2D;
	s_xGPUCapabilties.m_uMaxFramebufferWidth = xProps.limits.maxFramebufferWidth;
	s_xGPUCapabilties.m_uMaxFramebufferHeight = xProps.limits.maxFramebufferHeight;

	Zenith_Log("Vulkan physical device created");
}

void Zenith_Vulkan::CreateQueueFamilies()
{
	for (uint32_t& uIndex : s_auQueueIndices)
	{
		uIndex = UINT32_MAX;
	}

	std::vector<vk::QueueFamilyProperties> xQueueFamilyProperties = s_xPhysicalDevice.getQueueFamilyProperties();

	VkBool32 uSupportsPresent = false;

	for (uint32_t i = 0; i < xQueueFamilyProperties.size(); ++i)
	{
		uSupportsPresent = s_xPhysicalDevice.getSurfaceSupportKHR(i, s_xSurface);

		if (s_auQueueIndices[COMMANDTYPE_GRAPHICS] == UINT32_MAX && xQueueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics)
		{
			s_auQueueIndices[COMMANDTYPE_GRAPHICS] = i;
			if (uSupportsPresent && s_auQueueIndices[COMMANDTYPE_PRESENT] == UINT32_MAX) {
				s_auQueueIndices[COMMANDTYPE_PRESENT] = i;
			}
		}

		if (s_auQueueIndices[COMMANDTYPE_GRAPHICS] != i && s_auQueueIndices[COMMANDTYPE_COMPUTE] == UINT32_MAX && xQueueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eCompute)
		{
			s_auQueueIndices[COMMANDTYPE_COMPUTE] = i;
		}

		if (s_auQueueIndices[COMMANDTYPE_COPY] == UINT32_MAX && xQueueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eTransfer && xQueueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics)
		{
			s_auQueueIndices[COMMANDTYPE_COPY] = i;
		}
	}

	for (uint32_t uType = 0; uType < COMMANDTYPE_MAX; uType++)
	{
		Zenith_Assert(s_auQueueIndices[uType] != UINT32_MAX, "Couldn't find queue index");
	}

	Zenith_Log("Vulkan queue families created");
}

void Zenith_Vulkan::CreateDevice()
{
	std::vector<vk::DeviceQueueCreateInfo> xQueueInfos;
	std::set<uint32_t> xUniqueFamilies;
	for (uint32_t i = 0; i < COMMANDTYPE_MAX; i++)
	{
		xUniqueFamilies.insert(s_auQueueIndices[i]);
	}
	float fQueuePriority = 1;
	for (uint32_t uFamily : xUniqueFamilies)
	{
		vk::DeviceQueueCreateInfo xQueueInfo = vk::DeviceQueueCreateInfo()
			.setQueueFamilyIndex(uFamily)
			.setQueueCount(1)
			.setPQueuePriorities(&fQueuePriority);
		xQueueInfos.push_back(xQueueInfo);
	}


	vk::DeviceCreateInfo xDeviceCreateInfo = vk::DeviceCreateInfo()
		.setPQueueCreateInfos(xQueueInfos.data())
		.setQueueCreateInfoCount(xQueueInfos.size())
		.setEnabledExtensionCount(COUNT_OF(s_aszDeviceExtensions))
		.setPpEnabledExtensionNames(s_aszDeviceExtensions)
#ifdef ZENITH_DEBUG
		.setEnabledLayerCount(s_xValidationLayers.size())
		.setPpEnabledLayerNames(s_xValidationLayers.data());
#else
		.setEnabledLayerCount(0);
#endif

	

	vk::PhysicalDeviceFeatures xDeviceFeatures = vk::PhysicalDeviceFeatures()
		.setSamplerAnisotropy(VK_TRUE)
		.setTessellationShader(VK_TRUE)
		.setFillModeNonSolid(VK_TRUE);


	vk::PhysicalDeviceFeatures2 xDeviceFeatures2 = vk::PhysicalDeviceFeatures2()
		.setFeatures(xDeviceFeatures);

	

	vk::PhysicalDeviceDescriptorIndexingFeatures xIndexingFeatures = vk::PhysicalDeviceDescriptorIndexingFeatures()
		.setDescriptorBindingSampledImageUpdateAfterBind(true)
		.setPNext(&xDeviceFeatures2);

	vk::PhysicalDeviceFeatures2 xTemp;
	vk::PhysicalDeviceFragmentShaderBarycentricFeaturesKHR xTempBary;
	xTemp.setPNext(&xTempBary);
	s_xPhysicalDevice.getFeatures2(&xTemp);
	xTempBary.setPNext(&xIndexingFeatures);

	xDeviceCreateInfo.setPNext(&xTempBary);

	s_xDevice = s_xPhysicalDevice.createDevice(xDeviceCreateInfo);

	for (uint32_t i = 0; i < COMMANDTYPE_MAX; i++)
	{
		s_axQueues[i] = s_xDevice.getQueue(s_auQueueIndices[i], 0);
	}

	Zenith_Log("Vulkan device created");
}

void Zenith_Vulkan::CreateCommandPools()
{
	for (uint32_t i = 0; i < COMMANDTYPE_MAX; i++)
	{
		s_axCommandPools[i] = s_xDevice.createCommandPool(vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, s_auQueueIndices[i]));
	}

	Zenith_Log("Vulkan command pools created");
}

void Zenith_Vulkan::CreateDefaultDescriptorPool()
{
	vk::DescriptorPoolSize axPoolSizes[] =
	{
		{ vk::DescriptorType::eSampler, 10000 },
		{ vk::DescriptorType::eCombinedImageSampler, 10000 },
		{ vk::DescriptorType::eSampledImage, 10000 },
		{ vk::DescriptorType::eStorageImage, 10000 },
		{ vk::DescriptorType::eUniformTexelBuffer, 10000 },
		{ vk::DescriptorType::eStorageTexelBuffer, 10000 },
		{ vk::DescriptorType::eUniformBuffer, 10000 },
		{ vk::DescriptorType::eStorageBuffer, 10000 },
		{ vk::DescriptorType::eUniformBufferDynamic, 10000 },
		{ vk::DescriptorType::eStorageBufferDynamic, 10000 },
		{ vk::DescriptorType::eInputAttachment, 10000 }
	};

	vk::DescriptorPoolCreateInfo xPoolInfo = vk::DescriptorPoolCreateInfo()
		.setPoolSizeCount(COUNT_OF(axPoolSizes))
		.setPPoolSizes(axPoolSizes)
		.setMaxSets(10000)
		.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind);

	s_xDefaultDescriptorPool = s_xDevice.createDescriptorPool(xPoolInfo);

	Zenith_Log("Vulkan default descriptor pool created");
}

#ifdef ZENITH_TOOLS

void Zenith_Vulkan::InitialiseImGui()
{
	InitialiseImGuiRenderPass();
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGui::StyleColorsDark();

	ImGuiIO& xIO = ImGui::GetIO();
	xIO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

#ifdef ZENITH_WINDOWS
	GLFWwindow* pxWindow = Zenith_Window::GetInstance()->GetNativeWindow();
#endif

	ImGui_ImplGlfw_InitForVulkan(pxWindow, true);
	ImGui_ImplVulkan_InitInfo xInitInfo = {};
	xInitInfo.Instance = s_xInstance;
	xInitInfo.PhysicalDevice = s_xPhysicalDevice;
	xInitInfo.Device = s_xDevice;
	xInitInfo.QueueFamily = s_auQueueIndices[COMMANDTYPE_GRAPHICS];
	xInitInfo.Queue = s_axQueues[COMMANDTYPE_GRAPHICS];
	xInitInfo.DescriptorPool = s_xDefaultDescriptorPool;
	xInitInfo.MinImageCount = MAX_FRAMES_IN_FLIGHT;
	xInitInfo.ImageCount = MAX_FRAMES_IN_FLIGHT;
	xInitInfo.RenderPass = s_xImGuiRenderPass;
	ImGui_ImplVulkan_Init(&xInitInfo);

	ImGui_ImplVulkan_CreateFontsTexture();
}

void Zenith_Vulkan::InitialiseImGuiRenderPass()
{
	vk::AttachmentDescription xColorAttachment = vk::AttachmentDescription()
		.setFormat(Zenith_Vulkan_Swapchain::GetFormat())
		.setSamples(vk::SampleCountFlagBits::e1)
		.setLoadOp(vk::AttachmentLoadOp::eLoad)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
		.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setInitialLayout(vk::ImageLayout::ePresentSrcKHR)
		.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

	vk::AttachmentReference xColorAttachmentRef = vk::AttachmentReference()
		.setAttachment(0)
		.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

	vk::AttachmentReference axColorAtachments[1]{ xColorAttachmentRef };

	vk::SubpassDescription xSubpass = vk::SubpassDescription()
		.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
		.setColorAttachmentCount(1)
		.setPColorAttachments(axColorAtachments);

	vk::SubpassDependency xDependency = vk::SubpassDependency()
		.setSrcSubpass(VK_SUBPASS_EXTERNAL)
		.setDstSubpass(0)
		.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests)
		.setSrcAccessMask(vk::AccessFlagBits::eNone)
		.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests)
		.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite);

	vk::AttachmentDescription axAllAttachments[]{ xColorAttachment };

	vk::RenderPassCreateInfo xRenderPassInfo = vk::RenderPassCreateInfo()
		.setAttachmentCount(1)
		.setPAttachments(axAllAttachments)
		.setSubpassCount(1)
		.setPSubpasses(&xSubpass)
		.setDependencyCount(1)
		.setPDependencies(&xDependency);

	s_xImGuiRenderPass = s_xDevice.createRenderPass(xRenderPassInfo);
}
void Zenith_Vulkan::ImGuiBeginFrame()
{
	ImGui_ImplVulkan_NewFrame();
#ifdef ZENITH_WINDOWS
	ImGui_ImplGlfw_NewFrame();
#endif
	ImGui::NewFrame();
}
#endif

void Zenith_Vulkan_PerFrame::Initialise()
{
	vk::DescriptorPoolSize axPoolSizes[] =
	{
		{ vk::DescriptorType::eSampler, 10000 },
		{ vk::DescriptorType::eCombinedImageSampler, 10000 },
		{ vk::DescriptorType::eSampledImage, 10000 },
		{ vk::DescriptorType::eStorageImage, 10000 },
		{ vk::DescriptorType::eUniformTexelBuffer, 10000 },
		{ vk::DescriptorType::eStorageTexelBuffer, 10000 },
		{ vk::DescriptorType::eUniformBuffer, 10000 },
		{ vk::DescriptorType::eStorageBuffer, 10000 },
		{ vk::DescriptorType::eUniformBufferDynamic, 10000 },
		{ vk::DescriptorType::eStorageBufferDynamic, 10000 },
		{ vk::DescriptorType::eInputAttachment, 10000 }
	};

	vk::DescriptorPoolCreateInfo xPoolInfo = vk::DescriptorPoolCreateInfo()
		.setPoolSizeCount(COUNT_OF(axPoolSizes))
		.setPPoolSizes(axPoolSizes)
		.setMaxSets(10000)
		.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind);

	for (vk::DescriptorPool& xPool : m_axDescriptorPools)
	{
		xPool = Zenith_Vulkan::GetDevice().createDescriptorPool(xPoolInfo);
	}

	vk::FenceCreateInfo xFenceInfo = vk::FenceCreateInfo()
		.setFlags(vk::FenceCreateFlagBits::eSignaled);
	m_xFence = Zenith_Vulkan::GetDevice().createFence(xFenceInfo);
}

void Zenith_Vulkan_PerFrame::BeginFrame()
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	xDevice.waitForFences(1, &m_xFence, VK_TRUE, UINT64_MAX);
	xDevice.resetFences(1, &m_xFence);

	xDevice.resetDescriptorPool(m_axDescriptorPools[0]);
}
