#include "Zenith.h"

#include "Vulkan/Zenith_Vulkan.h"


#ifdef ZENITH_WINDOWS
#include "Zenith_Windows_Window.h"
#endif

#include "Flux/Flux.h"

#ifdef ZENITH_DEBUG
static std::vector<const char*> s_xValidationLayers = { "VK_LAYER_KHRONOS_validation" };
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
vk::DescriptorPool Zenith_Vulkan::s_axPerFrameDescriptorPools[MAX_FRAMES_IN_FLIGHT];

std::vector<const Zenith_Vulkan_CommandBuffer*> Zenith_Vulkan::s_xPendingCommandBuffers[RENDER_ORDER_MAX];



const vk::DescriptorPool& Zenith_Vulkan::GetCurrentPerFrameDescriptorPool() { return s_axPerFrameDescriptorPools[Zenith_Vulkan_Swapchain::GetCurrentFrameIndex()]; }

void Zenith_Vulkan::Initialise()
{
	CreateInstance();
	CreateDebugMessenger();
	CreateSurface();
	CreatePhysicalDevice();
	CreateQueueFamilies();
	CreateDevice();
	CreateCommandPools();
	CreateDefaultDescriptorPool();
}

void Zenith_Vulkan::BeginFrame()
{
	RecreatePerFrameDescriptorPool();
}

void Zenith_Vulkan::EndFrame()
{
	static_assert(RENDER_ORDER_MEMORY_UPDATE == 0u, "Memory update needs to come first");

	vk::PipelineStageFlags eMemWaitStages = vk::PipelineStageFlagBits::eTransfer;
	vk::PipelineStageFlags eRenderWaitStages = vk::PipelineStageFlagBits::eTransfer;

	//#TO_TODO: stop making this every frame
	vk::Semaphore xMemorySemaphore = s_xDevice.createSemaphore(vk::SemaphoreCreateInfo());

	std::vector<vk::CommandBuffer> xPlatformMemoryCmdBufs;
	for (const Zenith_Vulkan_CommandBuffer* pCmdBuf : s_xPendingCommandBuffers[RENDER_ORDER_MEMORY_UPDATE])
	{
		const vk::CommandBuffer& xBuf = *reinterpret_cast<const vk::CommandBuffer*>(pCmdBuf);
		xPlatformMemoryCmdBufs.push_back(xBuf);
	}

	vk::SubmitInfo xMemorySubmitInfo = vk::SubmitInfo()
		.setCommandBufferCount(xPlatformMemoryCmdBufs.size())
		.setPCommandBuffers(xPlatformMemoryCmdBufs.data())
		.setPWaitSemaphores(&Zenith_Vulkan_Swapchain::GetCurrentImageAvailableSemaphore())
		.setPSignalSemaphores(&xMemorySemaphore)
		.setWaitSemaphoreCount(1)
		.setSignalSemaphoreCount(1)
		.setWaitDstStageMask(eMemWaitStages);


	s_axQueues[COMMANDTYPE_COPY].submit(xMemorySubmitInfo, VK_NULL_HANDLE);

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
		.setPSignalSemaphores(&Zenith_Vulkan_Swapchain::GetCurrentRenderCompleteSemaphore())
		.setWaitSemaphoreCount(1)
		.setSignalSemaphoreCount(1)
		.setWaitDstStageMask(eRenderWaitStages);


	s_axQueues[COMMANDTYPE_GRAPHICS].submit(xRenderSubmitInfo, Zenith_Vulkan_Swapchain::GetCurrentInFlightFence());


	for (uint32_t i = 0; i < RENDER_ORDER_MAX; i++)
	{
		s_xPendingCommandBuffers[i].clear();
	}

	//#TO_TODO: plug semaphore leak
	//s_xDevice.destroySemaphore(xMemorySemaphore);
}

void Zenith_Vulkan::RecreatePerFrameDescriptorPool()
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

	vk::DescriptorPool& xPool = s_axPerFrameDescriptorPools[Zenith_Vulkan_Swapchain::GetCurrentFrameIndex()];

	s_xDevice.destroyDescriptorPool(xPool);
	xPool = s_xDevice.createDescriptorPool(xPoolInfo);
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
		Zenith_Error("%s%s","Zenith_Vulkan::DebugCallback: ", pxCallbackData->pMessage);
	}
	__debugbreak();
	return VK_FALSE;
}

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
		vk::DispatchLoaderDynamic(s_xInstance, vkGetInstanceProcAddr)
	);

	Zenith_Log("Vulkan debug messenger created");
}

void Zenith_Vulkan::CreateSurface()
{
	GLFWwindow* pxWindow = Zenith_Windows_Window::GetInstance()->GetNativeWindow();
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

		if (s_auQueueIndices[COMMANDTYPE_GRAPHICS] != i && s_auQueueIndices[COMMANDTYPE_COPY] == UINT32_MAX && xQueueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eTransfer)
		{
			s_auQueueIndices[COMMANDTYPE_COPY] = i;
		}
	}

	for (uint32_t uType = 0; uType < COMMANDTYPE_MAX; uType++)
	{
		Zenith_Assert(uType != UINT32_MAX, "Couldn't find queue index");
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


	vk::DeviceCreateInfo deviceCreateInfo = vk::DeviceCreateInfo()
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

	s_xDevice = s_xPhysicalDevice.createDevice(deviceCreateInfo);

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
