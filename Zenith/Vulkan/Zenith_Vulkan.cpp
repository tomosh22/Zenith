#include "Zenith.h"

#include "Vulkan/Zenith_Vulkan.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Enums.h"
#include "Flux/Flux_Graphics.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "Multithreading/Zenith_Multithreading.h"
#include <algorithm>

#ifdef ZENITH_WINDOWS
#include "Zenith_Windows_Window.h"
#endif

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#ifdef ZENITH_WINDOWS
#include "backends/imgui_impl_glfw.h"
#endif //ZENITH_WINDOWS
#include "Memory/Zenith_MemoryManagement_Enabled.h"
vk::RenderPass Zenith_Vulkan::s_xImGuiRenderPass;
#endif //ZENITH_TOOLS

#ifdef ZENITH_DEBUG_VARIABLES
#include "DebugVariables/Zenith_DebugVariables.h"
#endif

#include "Flux/Flux_Graphics.h"

#ifdef ZENITH_DEBUG
static std::vector<const char*> s_xValidationLayers = { "VK_LAYER_KHRONOS_validation", /*"VK_LAYER_KHRONOS_synchronization2"*/ };
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
Zenith_Vulkan_CommandBuffer* Zenith_Vulkan::s_pxMemoryUpdateCmdBuf = nullptr;

vk::DescriptorPool Zenith_Vulkan::s_xBindlessTexturesDescriptorPool;
vk::DescriptorSet Zenith_Vulkan::s_xBindlessTexturesDescriptorSet;
vk::DescriptorSetLayout Zenith_Vulkan::s_xBindlessTexturesDescriptorSetLayout;

std::vector<Zenith_Vulkan_VRAM*> Zenith_Vulkan::s_xVRAMRegistry;
std::vector<uint32_t> Zenith_Vulkan::s_xFreeVRAMHandles;

std::vector<const Zenith_Vulkan_CommandBuffer*> Zenith_Vulkan::s_xPendingCommandBuffers[RENDER_ORDER_MAX];
Zenith_Vulkan_CommandBuffer g_xCommandBuffer;

DEBUGVAR bool dbg_bSubmitDrawCalls = true;
DEBUGVAR bool dbg_bUseDescSetCache = true;
DEBUGVAR bool dbg_bOnlyUpdateDirtyDescriptors = true;
DEBUGVAR u_int dbg_uNumDescSetAllocations = 0;

// Transition color targets to ColorAttachmentOptimal and ensure depth is in ReadOnlyOptimal before render pass
static void TransitionColorTargets(Zenith_Vulkan_CommandBuffer& xCommandBuffer, const Flux_TargetSetup& xTargetSetup,
	vk::ImageLayout eOldLayout, vk::ImageLayout eNewLayout, vk::AccessFlags eAccessMask,
	vk::PipelineStageFlags eSrcStage, vk::PipelineStageFlags eDstStage)
{
	vk::ImageMemoryBarrier axBarriers[FLUX_MAX_TARGETS];
	uint32_t uNumBarriers = 0;

	for (uint32_t i = 0; i < FLUX_MAX_TARGETS; i++)
	{
		if (xTargetSetup.m_axColourAttachments[i].m_xSurfaceInfo.m_eFormat != TEXTURE_FORMAT_NONE)
		{
			Flux_VRAMHandle xVRAMHandle = xTargetSetup.m_axColourAttachments[i].m_xVRAMHandle;
			if (xVRAMHandle.AsUInt() != UINT32_MAX)
			{
				Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(xVRAMHandle);
				if (pxVRAM)
				{
					vk::ImageSubresourceRange xSubRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

					axBarriers[uNumBarriers] = vk::ImageMemoryBarrier()
						.setSubresourceRange(xSubRange)
						.setImage(pxVRAM->GetImage())
						.setOldLayout(eOldLayout)
						.setNewLayout(eNewLayout)
						.setDstAccessMask(eAccessMask);

					uNumBarriers++;
				}
			}
		}
		else
		{
			break;
		}
	}

	if (uNumBarriers > 0)
	{
		xCommandBuffer.GetCurrentCmdBuffer().pipelineBarrier(
			eSrcStage, eDstStage, vk::DependencyFlags(),
			0, nullptr,
			0, nullptr,
			uNumBarriers, axBarriers
		);
	}
}

static void TransitionDepthStencilTarget(Zenith_Vulkan_CommandBuffer& xCommandBuffer, const Flux_TargetSetup& xTargetSetup,
	vk::ImageLayout eOldLayout, vk::ImageLayout eNewLayout, vk::AccessFlags eSrcAccessMask, vk::AccessFlags eDstAccessMask,
	vk::PipelineStageFlags eSrcStage, vk::PipelineStageFlags eDstStage)
{
	if (xTargetSetup.m_pxDepthStencil == nullptr)
	{
		return;
	}

	Flux_VRAMHandle xVRAMHandle = xTargetSetup.m_pxDepthStencil->m_xVRAMHandle;
	Zenith_Assert(xVRAMHandle.IsValid(), "Depth stencil target has invalid VRAM handle");

	Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(xVRAMHandle);

	vk::ImageSubresourceRange xSubRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1);

	vk::ImageMemoryBarrier xBarrier = vk::ImageMemoryBarrier()
		.setSubresourceRange(xSubRange)
		.setImage(pxVRAM->GetImage())
		.setOldLayout(eOldLayout)
		.setNewLayout(eNewLayout)
		.setSrcAccessMask(eSrcAccessMask)
		.setDstAccessMask(eDstAccessMask);

	xCommandBuffer.GetCurrentCmdBuffer().pipelineBarrier(
		eSrcStage, eDstStage, vk::DependencyFlags(),
		0, nullptr,
		0, nullptr,
		1, &xBarrier
	);
}

static void TransitionTargetsForRenderPass(Zenith_Vulkan_CommandBuffer& xCommandBuffer, const Flux_TargetSetup& xTargetSetup,
	vk::PipelineStageFlags eSrcStage, vk::PipelineStageFlags eDstStage, bool bClear)
{
	// Transition color attachments to ColorAttachmentOptimal
	vk::ImageLayout eOldLayout = bClear ? vk::ImageLayout::eUndefined : vk::ImageLayout::eShaderReadOnlyOptimal;
	TransitionColorTargets(xCommandBuffer, xTargetSetup, eOldLayout, vk::ImageLayout::eColorAttachmentOptimal,
		vk::AccessFlagBits::eColorAttachmentWrite, eSrcStage, eDstStage);

	// Ensure depth is in DepthStencilReadOnlyOptimal to match render pass initial layout
	// (Render pass handles ReadOnly->Attachment transition internally)
	// Use DepthStencilReadOnlyOptimal for all formats (doesn't require separateDepthStencilLayouts feature)
	if (!bClear)
	{
		TransitionDepthStencilTarget(xCommandBuffer, xTargetSetup,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::ImageLayout::eDepthStencilReadOnlyOptimal,
			vk::AccessFlagBits::eShaderRead,
			vk::AccessFlagBits::eDepthStencilAttachmentRead,
			eSrcStage, eDstStage);
	}
}

// Transition color targets back to ShaderReadOnlyOptimal after render pass
// Also transition depth to DepthStencilReadOnlyOptimal for shader sampling
static void TransitionTargetsAfterRenderPass(Zenith_Vulkan_CommandBuffer& xCommandBuffer, const Flux_TargetSetup& xTargetSetup,
	vk::PipelineStageFlags eSrcStage, vk::PipelineStageFlags eDstStage)
{
	// Transition color attachments
	TransitionColorTargets(xCommandBuffer, xTargetSetup, vk::ImageLayout::eColorAttachmentOptimal, 
		vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits::eShaderRead, eSrcStage, eDstStage);

	// Transition depth from render pass final layout to shader read layout
	// Use DepthStencilReadOnlyOptimal for all formats (doesn't require separateDepthStencilLayouts feature)
	TransitionDepthStencilTarget(xCommandBuffer, xTargetSetup,
		vk::ImageLayout::eDepthStencilReadOnlyOptimal,
		vk::ImageLayout::eShaderReadOnlyOptimal,
		vk::AccessFlagBits::eDepthStencilAttachmentRead,
		vk::AccessFlagBits::eShaderRead,
		eSrcStage, eDstStage);
}

const vk::DescriptorPool& Zenith_Vulkan::GetCurrentPerFrameDescriptorPool()
{
	//#TO_TODO: current thread
	u_int uThreadID = Zenith_Multithreading::GetCurrentThreadID();
	return s_pxCurrentFrame->GetDescriptorPoolForThread(uThreadID);
}

const vk::CommandPool& Zenith_Vulkan::GetWorkerCommandPool(u_int uThreadIndex)
{
	return s_pxCurrentFrame->GetCommandPoolForThread(uThreadIndex);
}

vk::Fence& Zenith_Vulkan::GetCurrentInFlightFence()
{
	return s_pxCurrentFrame->m_xFence;
}

const bool Zenith_Vulkan::ShouldSubmitDrawCalls() { return dbg_bSubmitDrawCalls; }
const bool Zenith_Vulkan::ShouldUseDescSetCache() { return dbg_bUseDescSetCache; }
const bool Zenith_Vulkan::ShouldOnlyUpdateDirtyDescriptors() { return dbg_bOnlyUpdateDirtyDescriptors; }
const void Zenith_Vulkan::IncrementDescriptorSetAllocations(){ dbg_uNumDescSetAllocations++;}

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
	CreateBindlessTexturesDescriptorPool();

	for (Zenith_Vulkan_PerFrame& xFrame : s_axPerFrame)
	{
		xFrame.Initialise();
	}

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Vulkan", "Submit Draw Calls" }, dbg_bSubmitDrawCalls);
	Zenith_DebugVariables::AddBoolean({ "Vulkan", "Use Descriptor Set Cache" }, dbg_bUseDescSetCache);
	Zenith_DebugVariables::AddBoolean({ "Vulkan", "Only Update Dirty Descriptors" }, dbg_bOnlyUpdateDirtyDescriptors);

	Zenith_DebugVariables::AddUInt32_ReadOnly({ "Vulkan", "Descriptor Sets Allocated" }, dbg_uNumDescSetAllocations, 0,-1);
#endif

	s_pxCurrentFrame = &s_axPerFrame[0];

	g_xCommandBuffer.Initialise();
}

void Zenith_Vulkan::BeginFrame()
{
	s_pxCurrentFrame = &s_axPerFrame[Zenith_Vulkan_Swapchain::GetCurrentFrameIndex()];
	s_pxCurrentFrame->BeginFrame();

	dbg_uNumDescSetAllocations = 0;
}

// Structure to hold work for each thread
struct CommandRecordingWorkData
{
	u_int uStartRenderOrder;
	u_int uEndRenderOrder;
	std::vector<std::pair<u_int, vk::CommandBuffer>>* pxOutCommandBuffers; // pair of (startRenderOrder, commandBuffer)
	Zenith_Mutex* pxMutex;
};

// Task array function to record command buffers in parallel
void Zenith_Vulkan::RecordCommandBuffersTask(void* pData, u_int uInvocationIndex, u_int uNumInvocations)
{
	CommandRecordingWorkData* pWorkData = static_cast<CommandRecordingWorkData*>(pData);
	
	// Calculate the range of render orders this thread should process
	u_int uTotalOrders = pWorkData->uEndRenderOrder - pWorkData->uStartRenderOrder;
	u_int uOrdersPerThread = (uTotalOrders + uNumInvocations - 1) / uNumInvocations;
	u_int uLocalStartOrder = pWorkData->uStartRenderOrder + (uInvocationIndex * uOrdersPerThread);
	u_int uLocalEndOrder = (uLocalStartOrder + uOrdersPerThread < pWorkData->uEndRenderOrder) ? 
		(uLocalStartOrder + uOrdersPerThread) : pWorkData->uEndRenderOrder;
	
	if (uLocalStartOrder >= uLocalEndOrder)
	{
		return; // No work for this thread
	}
	
	// Get the worker command buffer from the current frame
	Zenith_Vulkan_CommandBuffer& xCommandBuffer = Zenith_Vulkan::s_pxCurrentFrame->GetWorkerCommandBuffer(uInvocationIndex);
	xCommandBuffer.BeginRecording();
	
	Flux_TargetSetup xCurrentTargetSetup;
	
	for (u_int i = uLocalStartOrder; i < uLocalEndOrder; i++)
	{
		Zenith_Vector<std::pair<const Flux_CommandList*, Flux_TargetSetup>>& xCommandLists = Flux::s_xPendingCommandLists[i];
		
		// Skip empty render orders
		if (xCommandLists.GetSize() == 0)
		{
			continue;
		}
		
		for (Zenith_Vector<std::pair<const Flux_CommandList*, Flux_TargetSetup>>::Iterator xIt(xCommandLists); !xIt.Done(); xIt.Next())
		{
			const bool bClear = xIt.GetData().first->RequiresClear();
			
			// Check if this is a compute pass (null target setup)
			const bool bIsComputePass = (xIt.GetData().second == Flux_Graphics::s_xNullTargetSetup);
			
			if (bIsComputePass)
			{
				// Execute compute commands without render pass
				if (xCommandBuffer.m_xCurrentRenderPass != VK_NULL_HANDLE)
				{
					xCommandBuffer.EndRenderPass();
					TransitionTargetsAfterRenderPass(
						xCommandBuffer, 
						xCurrentTargetSetup,
						vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eLateFragmentTests,
						vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eComputeShader
					);
					xCurrentTargetSetup = Flux_Graphics::s_xNullTargetSetup;
				}
				xIt.GetData().first->IterateCommands(&xCommandBuffer);
			}
			else if (xIt.GetData().second != xCurrentTargetSetup || bClear || xCommandBuffer.m_xCurrentRenderPass == VK_NULL_HANDLE)
			{
				if (xCommandBuffer.m_xCurrentRenderPass != VK_NULL_HANDLE)
				{
					xCommandBuffer.EndRenderPass();
					TransitionTargetsAfterRenderPass(
						xCommandBuffer, 
						xCurrentTargetSetup,
						vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eLateFragmentTests,
						vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eComputeShader
					);
				}

				TransitionTargetsForRenderPass(
					xCommandBuffer, 
					xIt.GetData().second,
					vk::PipelineStageFlagBits::eFragmentShader,
					vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
					bClear
				);
				xCommandBuffer.BeginRenderPass(xIt.GetData().second, bClear, bClear, bClear);
				xCurrentTargetSetup = xIt.GetData().second;
				xIt.GetData().first->IterateCommands(&xCommandBuffer);
			}
			else
			{
				xIt.GetData().first->IterateCommands(&xCommandBuffer);
			}
		}
	}
	
	// Only end render pass and transition targets if we're not in a compute-only pass
	if (!(xCurrentTargetSetup == Flux_Graphics::s_xNullTargetSetup))
	{
		xCommandBuffer.EndRenderPass();
		TransitionTargetsAfterRenderPass(
			xCommandBuffer, 
			xCurrentTargetSetup,
			vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eLateFragmentTests,
			vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eComputeShader
		);
	}
	
	xCommandBuffer.GetCurrentCmdBuffer().end();
	
	// Thread-safe add the command buffer to the output list with its starting render order
	pWorkData->pxMutex->Lock();
	pWorkData->pxOutCommandBuffers->push_back(std::make_pair(uLocalStartOrder, xCommandBuffer.GetCurrentCmdBuffer()));
	pWorkData->pxMutex->Unlock();
}

void Zenith_Vulkan::EndFrame()
{
	static_assert(RENDER_ORDER_MEMORY_UPDATE == 0u, "Memory update needs to come first");

	vk::PipelineStageFlags eMemWaitStages = vk::PipelineStageFlagBits::eAllCommands;
	vk::PipelineStageFlags eRenderWaitStages = vk::PipelineStageFlagBits::eAllCommands;

	//#TO_TODO: stop making this every frame
	vk::Semaphore xMemorySemaphore = s_xDevice.createSemaphore(vk::SemaphoreCreateInfo());

	std::vector<vk::CommandBuffer> xPlatformMemoryCmdBufs;
	if (s_pxMemoryUpdateCmdBuf)
	{
		xPlatformMemoryCmdBufs.push_back(s_pxMemoryUpdateCmdBuf->GetCurrentCmdBuffer());
		s_pxMemoryUpdateCmdBuf = nullptr;

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

	// Use multithreaded command buffer recording
	std::vector<std::pair<u_int, vk::CommandBuffer>> xRecordedCommandBuffers;
	Zenith_Mutex xMutex;
	
	CommandRecordingWorkData xWorkData;
	xWorkData.uStartRenderOrder = RENDER_ORDER_MEMORY_UPDATE + 1;
	xWorkData.uEndRenderOrder = RENDER_ORDER_MAX;
	xWorkData.pxOutCommandBuffers = &xRecordedCommandBuffers;
	xWorkData.pxMutex = &xMutex;
	
	// Create and submit task array for parallel command buffer recording
	Zenith_TaskArray xRecordingTask(
		ZENITH_PROFILE_INDEX__FLUX_RECORD_COMMAND_BUFFERS,
		RecordCommandBuffersTask,
		&xWorkData,
		Zenith_Vulkan_PerFrame::NUM_WORKER_THREADS
	);
	
	Zenith_TaskSystem::SubmitTaskArray(&xRecordingTask);
	xRecordingTask.WaitUntilComplete();
	
	// Sort command buffers by render order before submission
	std::sort(xRecordedCommandBuffers.begin(), xRecordedCommandBuffers.end(),
		[](const std::pair<u_int, vk::CommandBuffer>& a, const std::pair<u_int, vk::CommandBuffer>& b) {
			return a.first < b.first;
		});
	
	// Extract just the command buffers for submission
	std::vector<vk::CommandBuffer> xCommandBuffersToSubmit;
	xCommandBuffersToSubmit.reserve(xRecordedCommandBuffers.size());
	for (const auto& pair : xRecordedCommandBuffers)
	{
		xCommandBuffersToSubmit.push_back(pair.second);
	}
	
	// Submit all recorded command buffers in correct render order
	if (!xCommandBuffersToSubmit.empty())
	{
		vk::SubmitInfo xRenderSubmitInfo = vk::SubmitInfo()
			.setCommandBufferCount(xCommandBuffersToSubmit.size())
			.setPCommandBuffers(xCommandBuffersToSubmit.data())
			.setPWaitSemaphores(nullptr)
			.setPSignalSemaphores(nullptr)
			.setWaitSemaphoreCount(0)
			.setSignalSemaphoreCount(0);

		s_axQueues[COMMANDTYPE_GRAPHICS].submit(xRenderSubmitInfo, VK_NULL_HANDLE);
	}

	for (uint32_t i = 0; i < RENDER_ORDER_MAX; i++)
	{
		Flux::s_xPendingCommandLists[i].Clear();
	}

	//#TO_TODO: plug semaphore leak
	//s_xDevice.destroySemaphore(xMemorySemaphore);
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
		vk::DispatchLoaderDynamic(s_xInstance, vkGetInstanceProcAddr)
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
		.setDepthBiasClamp(VK_TRUE)
		.setFillModeNonSolid(VK_TRUE);


	vk::PhysicalDeviceFeatures2 xDeviceFeatures2 = vk::PhysicalDeviceFeatures2()
		.setFeatures(xDeviceFeatures);

	

	vk::PhysicalDeviceDescriptorIndexingFeatures xIndexingFeatures = vk::PhysicalDeviceDescriptorIndexingFeatures()
		.setDescriptorBindingSampledImageUpdateAfterBind(true)
		.setRuntimeDescriptorArray(true)
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
	
	// Note: Worker thread command pools are now created per-frame in Zenith_Vulkan_PerFrame::Initialise()

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

void Zenith_Vulkan::CreateBindlessTexturesDescriptorPool()
{
	vk::DescriptorPoolSize axPoolSizes[] =
	{
		{ vk::DescriptorType::eCombinedImageSampler, 1000 },
	};

	vk::DescriptorPoolCreateInfo xPoolInfo = vk::DescriptorPoolCreateInfo()
		.setPoolSizeCount(COUNT_OF(axPoolSizes))
		.setPPoolSizes(axPoolSizes)
		.setMaxSets(1)
		.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind);

	s_xBindlessTexturesDescriptorPool = s_xDevice.createDescriptorPool(xPoolInfo);

	Zenith_Log("Vulkan bindless textures descriptor pool created");

	vk::DescriptorSetLayoutBinding xBind = vk::DescriptorSetLayoutBinding()
		.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
		.setDescriptorCount(1000)
		.setBinding(0)
		.setStageFlags(vk::ShaderStageFlagBits::eAll)
		.setPImmutableSamplers(nullptr);

	vk::DescriptorSetLayoutCreateInfo xLayoutInfo = vk::DescriptorSetLayoutCreateInfo()
		.setBindingCount(1)
		.setPBindings(&xBind)
		.setFlags(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool);

	s_xBindlessTexturesDescriptorSetLayout = s_xDevice.createDescriptorSetLayout(xLayoutInfo);

	vk::DescriptorSetAllocateInfo xSetInfo = vk::DescriptorSetAllocateInfo()
		.setDescriptorPool(s_xBindlessTexturesDescriptorPool)
		.setDescriptorSetCount(1)
		.setPSetLayouts(&s_xBindlessTexturesDescriptorSetLayout);

	s_xBindlessTexturesDescriptorSet = s_xDevice.allocateDescriptorSets(xSetInfo)[0];
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
		.setMaxSets(100000)
		.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind);

	for (vk::DescriptorPool& xPool : m_axDescriptorPools)
	{
		xPool = Zenith_Vulkan::GetDevice().createDescriptorPool(xPoolInfo);
	}
	
	// Create per-worker-thread command pools for multithreaded command buffer recording
	for (u_int i = 0; i < NUM_WORKER_THREADS; i++)
	{
		m_axCommandPools[i] = Zenith_Vulkan::GetDevice().createCommandPool(
			vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, 
			Zenith_Vulkan::GetQueueIndex(COMMANDTYPE_GRAPHICS)));
		
		// Initialize worker command buffers with their dedicated command pools
		m_axWorkerCommandBuffers[i].InitialiseWithCustomPool(m_axCommandPools[i]);
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

	for (vk::DescriptorPool& xPool : m_axDescriptorPools)
	{
		xDevice.resetDescriptorPool(xPool);
	}
}

const vk::DescriptorPool& Zenith_Vulkan_PerFrame::GetDescriptorPoolForThread(u_int uThreadID)
{
	// Map thread ID to pool index (0 to NUM_WORKER_THREADS-1)
	u_int uPoolIndex = uThreadID % NUM_WORKER_THREADS;
	return m_axDescriptorPools[uPoolIndex];
}

const vk::CommandPool& Zenith_Vulkan_PerFrame::GetCommandPoolForThread(u_int uThreadID)
{
	// Map thread ID to pool index (0 to NUM_WORKER_THREADS-1)
	u_int uPoolIndex = uThreadID % NUM_WORKER_THREADS;
	return m_axCommandPools[uPoolIndex];
}

Zenith_Vulkan_CommandBuffer& Zenith_Vulkan_PerFrame::GetWorkerCommandBuffer(u_int uThreadID)
{
	// Map thread ID to buffer index (0 to NUM_WORKER_THREADS-1)
	u_int uBufferIndex = uThreadID % NUM_WORKER_THREADS;
	return m_axWorkerCommandBuffers[uBufferIndex];
}

Flux_VRAMHandle Zenith_Vulkan::RegisterVRAM(Zenith_Vulkan_VRAM* pxVRAM)
{
	Flux_VRAMHandle xHandle;
	
	// Check if there are any free handles to recycle
	if (!s_xFreeVRAMHandles.empty())
	{
		uint32_t uFreeIndex = s_xFreeVRAMHandles.back();
		s_xFreeVRAMHandles.pop_back();
		
		xHandle.SetValue(uFreeIndex);
		s_xVRAMRegistry[uFreeIndex] = pxVRAM;
	}
	else
	{
		// No free handles, grow the registry
		xHandle.SetValue(s_xVRAMRegistry.size());
		s_xVRAMRegistry.push_back(pxVRAM);
	}
	
	return xHandle;
}

Zenith_Vulkan_VRAM* Zenith_Vulkan::GetVRAM(const Flux_VRAMHandle xHandle)
{
	Zenith_Assert(xHandle.AsUInt() < s_xVRAMRegistry.size(), "Invalid VRAM handle");
	return s_xVRAMRegistry[xHandle.AsUInt()];
}

void Zenith_Vulkan::ReleaseVRAMHandle(const Flux_VRAMHandle xHandle)
{
	if (!xHandle.IsValid())
	{
		return;
	}
	
	Zenith_Assert(xHandle.AsUInt() < s_xVRAMRegistry.size(), "Invalid VRAM handle");
	
	// Mark slot as free by setting to nullptr
	s_xVRAMRegistry[xHandle.AsUInt()] = nullptr;
	
	// Add index to free list for recycling
	s_xFreeVRAMHandles.push_back(xHandle.AsUInt());
}

vk::Format Zenith_Vulkan::ConvertToVkFormat_Colour(TextureFormat eFormat) {
	switch (eFormat)
	{
	case TEXTURE_FORMAT_RGB8_UNORM:
		return vk::Format::eR8G8B8Unorm;
	case TEXTURE_FORMAT_RGBA8_UNORM:
		return vk::Format::eR8G8B8A8Unorm;
	case TEXTURE_FORMAT_BGRA8_SRGB:
		return vk::Format::eB8G8R8A8Srgb;
	case TEXTURE_FORMAT_R16G16B16A16_SFLOAT:
		return vk::Format::eR16G16B16A16Sfloat;
	case TEXTURE_FORMAT_R32G32B32A32_SFLOAT:
		return vk::Format::eR32G32B32A32Sfloat;
	case TEXTURE_FORMAT_R32G32B32_SFLOAT:
		return vk::Format::eR32G32B32Sfloat;
	case TEXTURE_FORMAT_R16G16B16A16_UNORM:
		return vk::Format::eR16G16B16A16Unorm;
	case TEXTURE_FORMAT_BGRA8_UNORM:
		return vk::Format::eB8G8R8A8Unorm;
	default:
		Zenith_Assert(false, "Invalid format");
	}
}

vk::Format Zenith_Vulkan::ConvertToVkFormat_DepthStencil(TextureFormat eFormat) {
	switch (eFormat)
	{
	case TEXTURE_FORMAT_D32_SFLOAT:
		return vk::Format::eD32Sfloat;
	default:
		Zenith_Assert(false, "Invalid format");
	}
}

vk::AttachmentLoadOp Zenith_Vulkan::ConvertToVkLoadAction(LoadAction eAction) {
	switch (eAction)
	{
	case LOAD_ACTION_DONTCARE:
		return vk::AttachmentLoadOp::eDontCare;
	case LOAD_ACTION_CLEAR:
		return vk::AttachmentLoadOp::eClear;
	case LOAD_ACTION_LOAD:
		return vk::AttachmentLoadOp::eLoad;
	default:
		Zenith_Assert(false, "Invalid action");
	}
}

vk::AttachmentStoreOp Zenith_Vulkan::ConvertToVkStoreAction(StoreAction eAction) {
	switch (eAction)
	{
	case STORE_ACTION_DONTCARE:
		return vk::AttachmentStoreOp::eDontCare;
	case STORE_ACTION_STORE:
		return vk::AttachmentStoreOp::eStore;
	default:
		Zenith_Assert(false, "Invalid action");
	}
}

void Zenith_Vulkan_Sampler::InitialiseRepeat(Zenith_Vulkan_Sampler& xSampler)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	const vk::PhysicalDevice& xPhysDevice = Zenith_Vulkan::GetPhysicalDevice();

	vk::SamplerCreateInfo xInfo = vk::SamplerCreateInfo()
		.setMagFilter(vk::Filter::eLinear)
		.setMinFilter(vk::Filter::eLinear)
		.setAddressModeU(vk::SamplerAddressMode::eRepeat)
		.setAddressModeV(vk::SamplerAddressMode::eRepeat)
		.setAddressModeW(vk::SamplerAddressMode::eRepeat)
		//.setAnisotropyEnable(VK_TRUE)
		//.setMaxAnisotropy(xPhysDevice.getProperties().limits.maxSamplerAnisotropy)
		.setBorderColor(vk::BorderColor::eIntOpaqueBlack)
		.setUnnormalizedCoordinates(VK_FALSE)
		.setCompareEnable(VK_FALSE)
		.setCompareOp(vk::CompareOp::eAlways)
		.setMipmapMode(vk::SamplerMipmapMode::eLinear)
		.setMaxLod(FLT_MAX);

	xSampler.m_xSampler = xDevice.createSampler(xInfo);
}

void Zenith_Vulkan_Sampler::InitialiseClamp(Zenith_Vulkan_Sampler& xSampler)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	const vk::PhysicalDevice& xPhysDevice = Zenith_Vulkan::GetPhysicalDevice();

	vk::SamplerCreateInfo xInfo = vk::SamplerCreateInfo()
		.setMagFilter(vk::Filter::eLinear)
		.setMinFilter(vk::Filter::eLinear)
		.setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
		.setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
		.setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
		//.setAnisotropyEnable(VK_TRUE)
		//.setMaxAnisotropy(xPhysDevice.getProperties().limits.maxSamplerAnisotropy)
		.setBorderColor(vk::BorderColor::eIntOpaqueBlack)
		.setUnnormalizedCoordinates(VK_FALSE)
		.setCompareEnable(VK_FALSE)
		.setCompareOp(vk::CompareOp::eAlways)
		.setMipmapMode(vk::SamplerMipmapMode::eLinear)
		.setMaxLod(FLT_MAX);

	xSampler.m_xSampler = xDevice.createSampler(xInfo);
}