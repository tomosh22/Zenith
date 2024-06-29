#include "Zenith.h"

#include "vulkan/Zenith_Vulkan.h"


#ifdef ZENITH_WINDOWS
#include "Zenith_Windows_Window.h"
#endif

#include "Flux/Zenith_Flux.h"

#ifdef ZENITH_DEBUG
static std::vector<const char*> s_xValidationLayers = { "VK_LAYER_KHRONOS_validation" };
#endif

static const char* s_aszDeviceExtensions[] = {
				VK_KHR_SWAPCHAIN_EXTENSION_NAME,
				VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
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
vk::DescriptorPool Zenith_Vulkan::s_axPerFrameDescriptorPools[MAX_FRAMES_IN_FLIGHT];

const vk::DescriptorPool& Zenith_Vulkan::GetCurrentPerFrameDescriptorPool() { return s_axPerFrameDescriptorPools[Zenith_Flux::GetFrameIndex()]; }

void Zenith_Vulkan::Initialise()
{
	CreateInstance();
	CreateDebugMessenger();
	CreateSurface();
	CreatePhysicalDevice();
	CreateQueueFamilies();
	CreateDevice();
	CreateCommandPools();
}

void Zenith_Vulkan::BeginFrame()
{
	RecreatePerFrameDescriptorPool();
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
		.setPoolSizeCount(sizeof(axPoolSizes) / sizeof(axPoolSizes[0]))
		.setPPoolSizes(axPoolSizes)
		.setMaxSets(10000)
		.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind);

	vk::DescriptorPool& xPool = s_axPerFrameDescriptorPools[Zenith_Flux::GetFrameIndex()];

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

	vk::InstanceCreateInfo instanceInfo = vk::InstanceCreateInfo()
		.setPApplicationInfo(&xAppInfo)
		.setEnabledExtensionCount(xExtensions.size())
		.setPpEnabledExtensionNames(xExtensions.data())
#ifdef ZENITH_DEBUG
		.setEnabledLayerCount(s_xValidationLayers.size())
		.setPpEnabledLayerNames(s_xValidationLayers.data());
#else
		.setEnabledLayerCount(0);
#endif
	s_xInstance = vk::createInstance(instanceInfo);

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


	vk::PhysicalDeviceFeatures deviceFeatures = vk::PhysicalDeviceFeatures()
		.setSamplerAnisotropy(VK_TRUE)
		.setTessellationShader(VK_TRUE);

	vk::PhysicalDeviceFeatures2 deviceFeatures2;
	deviceFeatures2.setFeatures(deviceFeatures);

	vk::PhysicalDeviceDescriptorIndexingFeatures indexingFeatures;
	indexingFeatures.descriptorBindingUniformBufferUpdateAfterBind = true;
	indexingFeatures.descriptorBindingSampledImageUpdateAfterBind = true;
	indexingFeatures.descriptorBindingPartiallyBound = true;
	indexingFeatures.descriptorBindingVariableDescriptorCount = true;
	indexingFeatures.runtimeDescriptorArray = true;
	deviceFeatures2.setPNext((void*)&indexingFeatures);

#ifdef ZENITH_RAYTRACING
	vk::PhysicalDeviceRayTracingPipelineFeaturesKHR xRayFeatures;
	xRayFeatures.setRayTracingPipeline(true);
	indexingFeatures.setPNext(&xRayFeatures);

	vk::PhysicalDeviceRayQueryFeaturesKHR xRayQueury;
	xRayQueury.setRayQuery(true);
	xRayFeatures.setPNext(&xRayQueury);

	vk::PhysicalDeviceBufferDeviceAddressFeaturesKHR deviceAddressfeature(true);
	xRayQueury.setPNext(&deviceAddressfeature);

	vk::PhysicalDeviceAccelerationStructureFeaturesKHR xAccelStructFeatures;
	xAccelStructFeatures.accelerationStructure = true;

	deviceAddressfeature.setPNext(&xAccelStructFeatures);
#endif


	vk::DeviceCreateInfo deviceCreateInfo = vk::DeviceCreateInfo()
		.setPNext(&deviceFeatures2)
		.setPQueueCreateInfos(xQueueInfos.data())
		.setQueueCreateInfoCount(xQueueInfos.size())
		.setEnabledExtensionCount(COUNT_OF(s_aszDeviceExtensions))
		.setPpEnabledExtensionNames(s_aszDeviceExtensions)
#if DEBUG
		.setEnabledLayerCount(m_validationLayers.size())
		.setPpEnabledLayerNames(m_validationLayers.data());
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
