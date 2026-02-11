#include "Zenith.h"

#include "Vulkan/Zenith_Vulkan.h"
#include "Vulkan/Zenith_Vulkan_Platform.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
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
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif //ZENITH_TOOLS

#ifdef ZENITH_DEBUG_VARIABLES
#include "DebugVariables/Zenith_DebugVariables.h"
#endif

#include "Flux/Flux_Graphics.h"

#ifdef ZENITH_DEBUG
static std::vector<const char*> s_xValidationLayers = { "VK_LAYER_KHRONOS_validation", /*"VK_LAYER_KHRONOS_synchronization2"*/ };
#endif

// Static member definitions
#ifdef ZENITH_TOOLS
vk::RenderPass Zenith_Vulkan::s_xImGuiRenderPass;
vk::DescriptorPool Zenith_Vulkan::s_xImGuiDescriptorPool;

// ImGui memory tracking
static std::atomic<u_int64> s_ulImGuiMemoryAllocated = 0;
static std::atomic<u_int64> s_ulImGuiAllocationCount = 0;

// Disable memory management macros for ImGui allocator (uses raw malloc/free)
#include "Memory/Zenith_MemoryManagement_Disabled.h"

// Custom ImGui allocator with tracking
static void* ImGuiAllocWrapper(size_t sz, void* user_data)
{
	(void)user_data;
	if (sz == 0)
		return nullptr;

	// Allocate with header for size tracking
	size_t* pBlock = static_cast<size_t*>(std::malloc(sizeof(size_t) + sz));
	if (!pBlock)
		return nullptr;

	*pBlock = sz;
	s_ulImGuiMemoryAllocated += sz;
	s_ulImGuiAllocationCount++;

	return pBlock + 1;
}

static void ImGuiFreeWrapper(void* ptr, void* user_data)
{
	(void)user_data;
	if (!ptr)
		return;

	size_t* pBlock = static_cast<size_t*>(ptr) - 1;
	size_t sz = *pBlock;

	s_ulImGuiMemoryAllocated -= sz;
	s_ulImGuiAllocationCount--;

	std::free(pBlock);
}

// Re-enable memory management macros
#include "Memory/Zenith_MemoryManagement_Enabled.h"

u_int64 Zenith_Vulkan::GetImGuiMemoryAllocated()
{
	return s_ulImGuiMemoryAllocated.load();
}

u_int64 Zenith_Vulkan::GetImGuiAllocationCount()
{
	return s_ulImGuiAllocationCount.load();
}
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
				VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME
};

vk::Instance Zenith_Vulkan::s_xInstance;
vk::DebugUtilsMessengerEXT Zenith_Vulkan::s_xDebugMessenger;
#ifdef ZENITH_DEBUG
vk::DispatchLoaderDynamic Zenith_Vulkan::s_xDispatchLoader;
#endif
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
					// Use base layer and mip from surface info to support cubemap face/mip rendering
					const uint32_t uBaseLayer = xTargetSetup.m_axColourAttachments[i].m_xSurfaceInfo.m_uBaseLayer;
					const uint32_t uBaseMip = xTargetSetup.m_axColourAttachments[i].m_xSurfaceInfo.m_uBaseMip;
					vk::ImageSubresourceRange xSubRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, uBaseMip, 1, uBaseLayer, 1);

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

static void TransitionTargetsForRenderPass(Zenith_Vulkan_CommandBuffer& xCommandBuffer, const Flux_TargetSetup& xTargetSetup,
	vk::PipelineStageFlags eSrcStage, vk::PipelineStageFlags eDstStage, bool bClear)
{
	// Transition color attachments to ColorAttachmentOptimal
	vk::ImageLayout eOldLayout = bClear ? vk::ImageLayout::eUndefined : vk::ImageLayout::eShaderReadOnlyOptimal;
	TransitionColorTargets(xCommandBuffer, xTargetSetup, eOldLayout, vk::ImageLayout::eColorAttachmentOptimal,
		vk::AccessFlagBits::eColorAttachmentWrite, eSrcStage, eDstStage);

	// Depth stays in DepthStencilReadOnlyOptimal between render passes
	// The render pass initial layout is DepthStencilReadOnlyOptimal (when loading) or Undefined (when clearing)
	// No explicit transition needed - depth is already in the correct layout from previous render pass
	// (or will be transitioned from Undefined by the render pass itself when clearing)
}

// Transition color targets back to ShaderReadOnlyOptimal after render pass
// Depth stays in DepthStencilReadOnlyOptimal for shader sampling (that's the render pass final layout)
static void TransitionTargetsAfterRenderPass(Zenith_Vulkan_CommandBuffer& xCommandBuffer, const Flux_TargetSetup& xTargetSetup,
	vk::PipelineStageFlags eSrcStage, vk::PipelineStageFlags eDstStage)
{
	// Transition color attachments
	TransitionColorTargets(xCommandBuffer, xTargetSetup, vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits::eShaderRead, eSrcStage, eDstStage);

	// NOTE: Depth stays in DepthStencilReadOnlyOptimal (the render pass final layout)
	// This is the correct layout for sampling depth textures in shaders
	// The descriptor binding code now uses DepthStencilReadOnlyOptimal for depth SRVs
}

const vk::DescriptorPool& Zenith_Vulkan::GetPerFrameDescriptorPool(u_int uWorkerIndex)
{
	return s_pxCurrentFrame->GetDescriptorPoolForWorkerIndex(uWorkerIndex);
}

const vk::CommandPool& Zenith_Vulkan::GetWorkerCommandPool(u_int uThreadIndex)
{
	return s_pxCurrentFrame->GetCommandPoolForWorkerIndex(uThreadIndex);
}

vk::Fence& Zenith_Vulkan::GetCurrentInFlightFence()
{
	return s_pxCurrentFrame->m_xFence;
}

const bool Zenith_Vulkan::ShouldSubmitDrawCalls() { return dbg_bSubmitDrawCalls; }
const bool Zenith_Vulkan::ShouldUseDescSetCache() { return dbg_bUseDescSetCache; }
const bool Zenith_Vulkan::ShouldOnlyUpdateDirtyDescriptors() { return dbg_bOnlyUpdateDirtyDescriptors; }
#ifdef ZENITH_DEBUG_VARIABLES
void Zenith_Vulkan::IncrementDescriptorSetAllocations(){ dbg_uNumDescSetAllocations++; }
#endif

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
#ifdef ZENITH_DEBUG
	s_xDispatchLoader = vk::DispatchLoaderDynamic(s_xInstance, vkGetInstanceProcAddr, s_xDevice, vkGetDeviceProcAddr);
#endif
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

	Zenith_DebugVariables::AddUInt32_ReadOnly({ "Vulkan", "Descriptor Sets Allocated" }, dbg_uNumDescSetAllocations);
#endif

	s_pxCurrentFrame = &s_axPerFrame[0];

	g_xCommandBuffer.Initialise();
}

void Zenith_Vulkan::InitialiseScratchBuffers()
{
	for (Zenith_Vulkan_PerFrame& xFrame : s_axPerFrame)
	{
		xFrame.InitialiseScratchBuffers();
	}
}

void Zenith_Vulkan::BeginFrame()
{
	s_pxCurrentFrame = &s_axPerFrame[Zenith_Vulkan_Swapchain::GetCurrentFrameIndex()];
	s_pxCurrentFrame->BeginFrame();

#ifdef ZENITH_TOOLS
	// Update shader hot reload system (checks for file changes)
	Flux_ShaderHotReload::Update();
#endif

#ifdef ZENITH_DEBUG_VARIABLES
	dbg_uNumDescSetAllocations = 0;
#endif
}

// Structure representing a chunk of work for command buffer recording
// Task array function to record command buffers in parallel
void Zenith_Vulkan::RecordCommandBuffersTask(void* pData, u_int uInvocationIndex, u_int)
{
	const Flux_WorkDistribution* pWorkDistribution = static_cast<const Flux_WorkDistribution*>(pData);

	// Get the worker command buffer from the current frame
	Zenith_Vulkan_CommandBuffer& xCommandBuffer = Zenith_Vulkan::s_pxCurrentFrame->GetWorkerCommandBuffer(uInvocationIndex);
	xCommandBuffer.BeginRecording();

	Flux_TargetSetup xCurrentTargetSetup;

	const u_int uStartRenderOrder = pWorkDistribution->auStartRenderOrder[uInvocationIndex];
	const u_int uStartIndex = pWorkDistribution->auStartIndex[uInvocationIndex];
	const u_int uEndRenderOrder = pWorkDistribution->auEndRenderOrder[uInvocationIndex];
	const u_int uEndIndex = pWorkDistribution->auEndIndex[uInvocationIndex];

	// Log worker assignment for debugging
	//Zenith_Log(LOG_CATEGORY_RENDERER, "Worker %u: BEGIN recording, range [%u, %u) idx [%u, %u)", uInvocationIndex, uStartRenderOrder, uEndRenderOrder, uStartIndex, uEndIndex);

	// Process all assigned command lists across render orders
	for (u_int uRenderOrder = uStartRenderOrder; uRenderOrder <= uEndRenderOrder; uRenderOrder++)
	{
		Zenith_Vector<Flux_CommandListEntry>& xCommandLists = Flux::s_xPendingCommandLists[uRenderOrder];

		// Skip empty render orders
		if (xCommandLists.GetSize() == 0)
		{
			continue;
		}

		// Determine the range within this render order
		const u_int uIndexStart = (uRenderOrder == uStartRenderOrder) ? uStartIndex : 0;
		const u_int uIndexEnd = (uRenderOrder == uEndRenderOrder) ? uEndIndex : xCommandLists.GetSize();

		for (u_int i = uIndexStart; i < uIndexEnd; i++)
		{
			const Flux_CommandList* pxCommandList = xCommandLists.Get(i).m_pxCmdList;
			Flux_TargetSetup& xTargetSetup = xCommandLists.Get(i).m_xTargetSetup;
			
			const bool bClear = pxCommandList->RequiresClear();
			
			// Check if this is a compute pass (null target setup)
			const bool bIsComputePass = (xTargetSetup == Flux_Graphics::s_xNullTargetSetup);
		
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
			pxCommandList->IterateCommands(&xCommandBuffer);
		}
		else if (xTargetSetup != xCurrentTargetSetup || bClear || xCommandBuffer.m_xCurrentRenderPass == VK_NULL_HANDLE)
		{
			if (xCommandBuffer.m_xCurrentRenderPass != VK_NULL_HANDLE)
			{
				//Zenith_Log(LOG_CATEGORY_RENDERER, "Worker %u: Ending render pass for RenderOrder %u, prev target had depth=%s", uInvocationIndex, uRenderOrder, xCurrentTargetSetup.m_pxDepthStencil != nullptr ? "YES" : "NO");
				xCommandBuffer.EndRenderPass();
				TransitionTargetsAfterRenderPass(
					xCommandBuffer,
					xCurrentTargetSetup,
					vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eLateFragmentTests,
					vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eComputeShader
				);
			}

			//Zenith_Log(LOG_CATEGORY_RENDERER, "Worker %u: Starting render pass for RenderOrder %u '%s', clear=%s, depth=%s, depthVRAM=%u", uInvocationIndex, uRenderOrder, pxCommandList->GetName(), bClear ? "YES" : "NO", xTargetSetup.m_pxDepthStencil != nullptr ? "YES" : "NO",xTargetSetup.m_pxDepthStencil ? xTargetSetup.m_pxDepthStencil->m_xVRAMHandle.AsUInt() : 0);
			//Zenith_Log(LOG_CATEGORY_RENDERER, "Worker %u: Calling TransitionTargetsForRenderPass (bClear=%s)", uInvocationIndex, bClear ? "YES" : "NO");
			TransitionTargetsForRenderPass(
				xCommandBuffer,
				xTargetSetup,
				vk::PipelineStageFlagBits::eFragmentShader,
				vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
				bClear
			);
			//Zenith_Log(LOG_CATEGORY_RENDERER, "Worker %u: TransitionTargetsForRenderPass completed", uInvocationIndex);
			xCommandBuffer.BeginRenderPass(xTargetSetup, bClear, bClear, bClear);
			xCurrentTargetSetup = xTargetSetup;
			pxCommandList->IterateCommands(&xCommandBuffer);
		}
		else
		{
			pxCommandList->IterateCommands(&xCommandBuffer);
		}
		}
	}
	
	// Only end render pass and transition targets if we're not in a compute-only pass
	if (!(xCurrentTargetSetup == Flux_Graphics::s_xNullTargetSetup))
	{
		//Zenith_Log(LOG_CATEGORY_RENDERER, "Worker %u: END - final render pass had depth=%s", uInvocationIndex, xCurrentTargetSetup.m_pxDepthStencil != nullptr ? "YES" : "NO");
		xCommandBuffer.EndRenderPass();
		TransitionTargetsAfterRenderPass(
			xCommandBuffer,
			xCurrentTargetSetup,
			vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eLateFragmentTests,
			vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eComputeShader
		);
	}
	else
	{
		//Zenith_Log(LOG_CATEGORY_RENDERER, "Worker %u: END - no render pass (compute only or no work)", uInvocationIndex);
	}

	xCommandBuffer.GetCurrentCmdBuffer().end();

}

void Zenith_Vulkan::EndFrame(bool bSubmitRenderWork)
{
	static_assert(RENDER_ORDER_MEMORY_UPDATE == 0u, "Memory update needs to come first");

	vk::PipelineStageFlags eMemWaitStages = vk::PipelineStageFlagBits::eAllCommands;
	vk::PipelineStageFlags eRenderWaitStages = vk::PipelineStageFlagBits::eAllCommands;

	const vk::Semaphore& xMemorySemaphore = s_pxCurrentFrame->GetMemorySemaphore();

	std::vector<vk::CommandBuffer> xPlatformMemoryCmdBufs;
	if (s_pxMemoryUpdateCmdBuf)
	{
		xPlatformMemoryCmdBufs.push_back(s_pxMemoryUpdateCmdBuf->GetCurrentCmdBuffer());
		s_pxMemoryUpdateCmdBuf = nullptr;

	}

	// Prepare frame work distribution in platform-independent layer
	// Do this BEFORE memory submit so we know whether to signal the semaphore
	Flux_WorkDistribution xWorkDistribution;
	const bool bHasRenderWork = bSubmitRenderWork && Flux::PrepareFrame(xWorkDistribution);

	const bool bShouldWait = Zenith_Vulkan_Swapchain::ShouldWaitOnImageAvailableSemaphore();
	vk::SubmitInfo xMemorySubmitInfo = vk::SubmitInfo()
		.setCommandBufferCount(static_cast<uint32_t>(xPlatformMemoryCmdBufs.size()))
		.setPCommandBuffers(xPlatformMemoryCmdBufs.data())
		// Only signal semaphore if we have render work that will wait on it
		.setPSignalSemaphores(bHasRenderWork ? &xMemorySemaphore : nullptr)
		.setSignalSemaphoreCount(bHasRenderWork ? 1 : 0)
		.setWaitDstStageMask(eMemWaitStages)
		.setPWaitSemaphores(bShouldWait ? &Zenith_Vulkan_Swapchain::GetCurrentImageAvailableSemaphore() : nullptr)
		.setWaitSemaphoreCount(bShouldWait);

	//#TO_TODO: change this to copy queue, how do I make sure this finishes before graphics?
	s_axQueues[COMMANDTYPE_GRAPHICS].submit(xMemorySubmitInfo, VK_NULL_HANDLE);

	if (!bHasRenderWork)
	{
		// CRITICAL: Clear pending command lists even when skipping render work
		// Without this, stale command list entries accumulate across frames and can
		// cause access violations when those entries are processed in subsequent frames
		// after scene resources have been destroyed and recreated
		Flux::ClearPendingCommandLists();
		return; // No work to do this frame
	}
	
	// Create and submit task array for parallel command buffer recording
	// Enable submitting thread joining to utilize the main thread
	Zenith_TaskArray xRecordingTask(
		ZENITH_PROFILE_INDEX__FLUX_RECORD_COMMAND_BUFFERS,
		RecordCommandBuffersTask,
		&xWorkDistribution,
		FLUX_NUM_WORKER_THREADS,
		true
	);
	
	Zenith_TaskSystem::SubmitTaskArray(&xRecordingTask);
	xRecordingTask.WaitUntilComplete();
	
	// Clear all pending command lists now that recording is complete
	Flux::ClearPendingCommandLists();
	
	// Submit all worker command buffers in order (0 to 7)
	// This maintains correct render order since work is distributed contiguously
	std::vector<vk::CommandBuffer> xCommandBuffersToSubmit;
	xCommandBuffersToSubmit.reserve(FLUX_NUM_WORKER_THREADS);
	for (u_int i = 0; i < FLUX_NUM_WORKER_THREADS; i++)
	{
		xCommandBuffersToSubmit.push_back(s_pxCurrentFrame->GetWorkerCommandBuffer(i).GetCurrentCmdBuffer());
	}
	
	// Submit all recorded command buffers in correct order
	if (!xCommandBuffersToSubmit.empty())
	{
		// Render submit waits on memory semaphore to ensure memory operations complete first
		// This also consumes the semaphore signal so it can be re-signaled next frame
		vk::SubmitInfo xRenderSubmitInfo = vk::SubmitInfo()
			.setCommandBufferCount(static_cast<uint32_t>(xCommandBuffersToSubmit.size()))
			.setPCommandBuffers(xCommandBuffersToSubmit.data())
			.setPWaitSemaphores(&xMemorySemaphore)
			.setWaitSemaphoreCount(1)
			.setWaitDstStageMask(eRenderWaitStages)
			.setPSignalSemaphores(nullptr)
			.setSignalSemaphoreCount(0);

		s_axQueues[COMMANDTYPE_GRAPHICS].submit(xRenderSubmitInfo, VK_NULL_HANDLE);
	}

}

void Zenith_Vulkan::WaitForGPUIdle()
{
	// Wait for all GPU work to complete
	// This is expensive (stalls the entire pipeline) but necessary for critical synchronization
	// Use cases: scene transitions, shutdown, debugging
	s_xDevice.waitIdle();

	Zenith_Log(LOG_CATEGORY_VULKAN, "GPU idle wait completed");
}

void Zenith_Vulkan::CreateInstance()
{
	vk::ApplicationInfo xAppInfo = vk::ApplicationInfo()
		.setPApplicationName("Zenith_Vulkan")
		.setApplicationVersion(VK_MAKE_VERSION(1, 0, 0))
		.setPEngineName("Zenith")
		.setEngineVersion(VK_MAKE_VERSION(1, 0, 0))
		.setApiVersion(VK_API_VERSION_1_3);

	// Get platform-specific Vulkan extensions
	std::vector<const char*> xExtensions = Zenith_Vulkan_Platform::GetRequiredInstanceExtensions();

	vk::InstanceCreateInfo xInstanceInfo = vk::InstanceCreateInfo()
		.setPApplicationInfo(&xAppInfo)
		.setEnabledExtensionCount(static_cast<uint32_t>(xExtensions.size()))
		.setPpEnabledExtensionNames(xExtensions.data())
#ifdef ZENITH_DEBUG
		.setEnabledLayerCount(static_cast<uint32_t>(s_xValidationLayers.size()))
		.setPpEnabledLayerNames(s_xValidationLayers.data());
#else
		.setEnabledLayerCount(0);
#endif
	s_xInstance = vk::createInstance(xInstanceInfo);

	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan instance created");
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL Zenith_Vulkan::DebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT eMessageSeverity, vk::DebugUtilsMessageTypeFlagsEXT, const vk::DebugUtilsMessengerCallbackDataEXT* pxCallbackData, void*)
{
	if (eMessageSeverity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
	{
		Zenith_Error(LOG_CATEGORY_VULKAN, "%s%s", "Zenith_Vulkan::DebugCallback: ", pxCallbackData->pMessage);
	}
	// Only break on actual errors, not performance warnings
	if (eMessageSeverity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
	{
		Zenith_DebugBreak();
	}
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

	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan debug messenger created");
}
#endif

void Zenith_Vulkan::CreateSurface()
{
	// Use platform abstraction for surface creation
	s_xSurface = Zenith_Vulkan_Platform::CreateSurface(s_xInstance);

	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan surface created");
}

void Zenith_Vulkan::CreatePhysicalDevice()
{
	uint32_t uNumDevices;
	vk::Result eResult = s_xInstance.enumeratePhysicalDevices(&uNumDevices, nullptr);
	Zenith_Assert(eResult == vk::Result::eSuccess && uNumDevices > 0, "Failed to find any physical devices with Vulkan support");
	Zenith_Log(LOG_CATEGORY_VULKAN, "%u physical vulkan devices to choose from", uNumDevices);
	std::vector<vk::PhysicalDevice> xDevices;
	xDevices.resize(uNumDevices);
	eResult = s_xInstance.enumeratePhysicalDevices(&uNumDevices, xDevices.data());
	Zenith_Assert(eResult == vk::Result::eSuccess, "Failed to enumerate physical devices");
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

	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan physical device created");
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
		uSupportsPresent = static_cast<VkBool32>(s_xPhysicalDevice.getSurfaceSupportKHR(i, s_xSurface));

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

	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan queue families created");
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
		.setQueueCreateInfoCount(static_cast<uint32_t>(xQueueInfos.size()))
		.setEnabledExtensionCount(COUNT_OF(s_aszDeviceExtensions))
		.setPpEnabledExtensionNames(s_aszDeviceExtensions)
#ifdef ZENITH_DEBUG
		.setEnabledLayerCount(static_cast<uint32_t>(s_xValidationLayers.size()))
		.setPpEnabledLayerNames(s_xValidationLayers.data());
#else
		.setEnabledLayerCount(0);
#endif

	

	vk::PhysicalDeviceFeatures xDeviceFeatures = vk::PhysicalDeviceFeatures()
		.setSamplerAnisotropy(VK_TRUE)
		.setTessellationShader(VK_TRUE)
		.setDepthBiasClamp(VK_TRUE)
		.setMultiDrawIndirect(VK_TRUE)
		.setFillModeNonSolid(VK_TRUE);


	vk::PhysicalDeviceFeatures2 xDeviceFeatures2 = vk::PhysicalDeviceFeatures2()
		.setFeatures(xDeviceFeatures);

	vk::PhysicalDeviceShaderDrawParameterFeatures xShaderDrawFeatures = vk::PhysicalDeviceShaderDrawParameterFeatures()
	.setShaderDrawParameters(VK_TRUE)
		.setPNext(&xDeviceFeatures2);

	vk::PhysicalDeviceDescriptorIndexingFeatures xIndexingFeatures = vk::PhysicalDeviceDescriptorIndexingFeatures()
		.setDescriptorBindingSampledImageUpdateAfterBind(true)
		.setRuntimeDescriptorArray(true)
		.setPNext(&xShaderDrawFeatures);

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

	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan device created");
}

void Zenith_Vulkan::CreateCommandPools()
{
	for (uint32_t i = 0; i < COMMANDTYPE_MAX; i++)
	{
		s_axCommandPools[i] = s_xDevice.createCommandPool(vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, s_auQueueIndices[i]));
	}
	
	// Note: Worker thread command pools are now created per-frame in Zenith_Vulkan_PerFrame::Initialise()

	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan command pools created");
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

	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan default descriptor pool created");
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

	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan bindless textures descriptor pool created");

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
	
	// Create a dedicated descriptor pool for ImGui that won't be reset every frame
	vk::DescriptorPoolSize axImGuiPoolSizes[] =
	{
		{ vk::DescriptorType::eSampler, 1000 },
		{ vk::DescriptorType::eCombinedImageSampler, 1000 },
		{ vk::DescriptorType::eSampledImage, 1000 },
		{ vk::DescriptorType::eStorageImage, 1000 },
		{ vk::DescriptorType::eUniformTexelBuffer, 1000 },
		{ vk::DescriptorType::eStorageTexelBuffer, 1000 },
		{ vk::DescriptorType::eUniformBuffer, 1000 },
		{ vk::DescriptorType::eStorageBuffer, 1000 },
		{ vk::DescriptorType::eUniformBufferDynamic, 1000 },
		{ vk::DescriptorType::eStorageBufferDynamic, 1000 },
		{ vk::DescriptorType::eInputAttachment, 1000 }
	};

	vk::DescriptorPoolCreateInfo xImGuiPoolInfo = vk::DescriptorPoolCreateInfo()
		.setPoolSizeCount(COUNT_OF(axImGuiPoolSizes))
		.setPPoolSizes(axImGuiPoolSizes)
		.setMaxSets(1000)
		.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);

	s_xImGuiDescriptorPool = s_xDevice.createDescriptorPool(xImGuiPoolInfo);
	
	Zenith_Log(LOG_CATEGORY_EDITOR, "ImGui dedicated descriptor pool created");

	// Hook ImGui allocator for memory tracking BEFORE creating context
	ImGui::SetAllocatorFunctions(ImGuiAllocWrapper, ImGuiFreeWrapper, nullptr);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGui::StyleColorsDark();

	ImGuiIO& xIO = ImGui::GetIO();
	xIO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	xIO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable Docking

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
	xInitInfo.DescriptorPool = s_xImGuiDescriptorPool;  // Use dedicated ImGui pool
	xInitInfo.MinImageCount = MAX_FRAMES_IN_FLIGHT;
	xInitInfo.ImageCount = MAX_FRAMES_IN_FLIGHT;
	
	// Set up pipeline info for main viewport (newer ImGui API)
	xInitInfo.PipelineInfoMain.RenderPass = s_xImGuiRenderPass;
	xInitInfo.PipelineInfoMain.Subpass = 0;
	xInitInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	
	// Disable dynamic rendering since we're using a render pass
	xInitInfo.UseDynamicRendering = false;
	
	ImGui_ImplVulkan_Init(&xInitInfo);
}

void Zenith_Vulkan::InitialiseImGuiRenderPass()
{
	// Use swapchain format (BGRA8_SRGB) since ImGui will render directly to the swapchain
	vk::AttachmentDescription xColorAttachment = vk::AttachmentDescription()
		.setFormat(Zenith_Vulkan_Swapchain::GetFormat())
		.setSamples(vk::SampleCountFlagBits::e1)
		.setLoadOp(vk::AttachmentLoadOp::eLoad)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
		.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
		.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

	vk::AttachmentReference xColorAttachmentRef = vk::AttachmentReference()
		.setAttachment(0)
		.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

	vk::SubpassDescription xSubpass = vk::SubpassDescription()
		.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
		.setColorAttachmentCount(1)
		.setPColorAttachments(&xColorAttachmentRef);

	vk::RenderPassCreateInfo xRenderPassInfo = vk::RenderPassCreateInfo()
		.setAttachmentCount(1)
		.setPAttachments(&xColorAttachment)
		.setSubpassCount(1)
		.setPSubpasses(&xSubpass)
		.setDependencyCount(0)
		.setPDependencies(nullptr);
	
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

void Zenith_Vulkan::ShutdownImGui()
{
	// Wait for GPU to finish before destroying ImGui resources
	s_xDevice.waitIdle();

	// Shutdown ImGui backends
	ImGui_ImplVulkan_Shutdown();
#ifdef ZENITH_WINDOWS
	ImGui_ImplGlfw_Shutdown();
#endif
	ImGui::DestroyContext();

	// Destroy ImGui Vulkan resources
	s_xDevice.destroyRenderPass(s_xImGuiRenderPass);
	s_xDevice.destroyDescriptorPool(s_xImGuiDescriptorPool);

	Zenith_Log(LOG_CATEGORY_EDITOR, "ImGui shut down");
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
		
		// Initialize worker command buffers with their dedicated command pools and worker index
		m_axWorkerCommandBuffers[i].InitialiseWithCustomPool(m_axCommandPools[i], i);
	}

	vk::FenceCreateInfo xFenceInfo = vk::FenceCreateInfo()
		.setFlags(vk::FenceCreateFlagBits::eSignaled);
	m_xFence = Zenith_Vulkan::GetDevice().createFence(xFenceInfo);

	// Create persistent semaphore for memory submit synchronization (fixes per-frame semaphore leak)
	m_xMemorySemaphore = Zenith_Vulkan::GetDevice().createSemaphore(vk::SemaphoreCreateInfo());
}

void Zenith_Vulkan_PerFrame::InitialiseScratchBuffers()
{
	// Create scratch buffer for push constant replacement
	Zenith_Vulkan_MemoryManager::PersistentBuffer xScratch = Zenith_Vulkan_MemoryManager::CreatePersistentlyMappedBuffer(
		uSCRATCH_BUFFER_SIZE,
		vk::BufferUsageFlagBits::eUniformBuffer);
	m_xScratchBuffer = xScratch.m_xBuffer;
	m_xScratchAllocation = xScratch.m_xAllocation;
	m_pScratchBufferMapped = xScratch.m_pMappedPtr;

	// Query alignment requirement
	m_uMinAlignment = static_cast<u_int>(Zenith_Vulkan::GetPhysicalDevice().getProperties().limits.minUniformBufferOffsetAlignment);

	// Initialize worker offsets
	for (u_int i = 0; i < NUM_WORKER_THREADS; i++)
	{
		m_auWorkerScratchOffsets[i] = i * uWORKER_PARTITION_SIZE;
	}
}

void Zenith_Vulkan_PerFrame::BeginFrame()
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	Zenith_Profiling::BeginProfile(ZENITH_PROFILE_INDEX__VULKAN_WAIT_FOR_GPU);
	vk::Result eResult = xDevice.waitForFences(1, &m_xFence, VK_TRUE, UINT64_MAX);
	Zenith_Assert(eResult == vk::Result::eSuccess, "Failed to wait for fence");
	Zenith_Profiling::EndProfile(ZENITH_PROFILE_INDEX__VULKAN_WAIT_FOR_GPU);
	eResult = xDevice.resetFences(1, &m_xFence);
	Zenith_Assert(eResult == vk::Result::eSuccess, "Failed to reset fence");

	Zenith_Profiling::BeginProfile(ZENITH_PROFILE_INDEX__VULKAN_RESET_DESCRIPTOR_POOLS);
	for (vk::DescriptorPool& xPool : m_axDescriptorPools)
	{
		xDevice.resetDescriptorPool(xPool);
	}
	Zenith_Profiling::EndProfile(ZENITH_PROFILE_INDEX__VULKAN_RESET_DESCRIPTOR_POOLS);
	// Reset scratch buffer offsets for each worker
	for (u_int i = 0; i < NUM_WORKER_THREADS; i++)
	{
		m_auWorkerScratchOffsets[i] = i * uWORKER_PARTITION_SIZE;
	}
}

const vk::DescriptorPool& Zenith_Vulkan_PerFrame::GetDescriptorPoolForWorkerIndex(u_int uWorkerIndex)
{
	Zenith_Assert(uWorkerIndex < NUM_WORKER_THREADS, "Worker index out of range");
	return m_axDescriptorPools[uWorkerIndex];
}

const vk::CommandPool& Zenith_Vulkan_PerFrame::GetCommandPoolForWorkerIndex(u_int uWorkerIndex)
{
	Zenith_Assert(uWorkerIndex < NUM_WORKER_THREADS, "Worker index out of range");
	return m_axCommandPools[uWorkerIndex];
}

Zenith_Vulkan_CommandBuffer& Zenith_Vulkan_PerFrame::GetWorkerCommandBuffer(u_int uWorkerIndex)
{
	Zenith_Assert(uWorkerIndex < NUM_WORKER_THREADS, "Worker index out of range");
	return m_axWorkerCommandBuffers[uWorkerIndex];
}

u_int Zenith_Vulkan_PerFrame::AllocateScratchBuffer(u_int uSize, u_int uWorkerIndex)
{
	Zenith_Assert(uWorkerIndex < NUM_WORKER_THREADS, "Worker index out of range");

	// Align size to minUniformBufferOffsetAlignment
	u_int uAlignedSize = (uSize + m_uMinAlignment - 1) & ~(m_uMinAlignment - 1);

	// Get current offset for this worker
	u_int uOffset = m_auWorkerScratchOffsets[uWorkerIndex];

	// Check we don't exceed worker's partition
	u_int uPartitionEnd = (uWorkerIndex + 1) * uWORKER_PARTITION_SIZE;
	Zenith_Assert(uOffset + uAlignedSize <= uPartitionEnd,
		"Worker %u scratch buffer overflow (offset=%u, size=%u, end=%u)",
		uWorkerIndex, uOffset, uAlignedSize, uPartitionEnd);

	// Advance offset
	m_auWorkerScratchOffsets[uWorkerIndex] = uOffset + uAlignedSize;

	return uOffset;
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
		xHandle.SetValue(static_cast<u_int>(s_xVRAMRegistry.size()));
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
	// Two-channel formats
	case TEXTURE_FORMAT_R16G16_SFLOAT:
		return vk::Format::eR16G16Sfloat;
	case TEXTURE_FORMAT_R32G32_SFLOAT:
		return vk::Format::eR32G32Sfloat;
	// Single-channel formats (for heightmaps)
	case TEXTURE_FORMAT_R16_UNORM:
		return vk::Format::eR16Unorm;
	case TEXTURE_FORMAT_R32_SFLOAT:
		return vk::Format::eR32Sfloat;
	// BC Compressed formats
	case TEXTURE_FORMAT_BC1_RGB_UNORM:
		return vk::Format::eBc1RgbUnormBlock;
	case TEXTURE_FORMAT_BC1_RGBA_UNORM:
		return vk::Format::eBc1RgbaUnormBlock;
	case TEXTURE_FORMAT_BC3_RGBA_UNORM:
		return vk::Format::eBc3UnormBlock;
	case TEXTURE_FORMAT_BC5_RG_UNORM:
		return vk::Format::eBc5UnormBlock;
	case TEXTURE_FORMAT_BC7_RGBA_UNORM:
		return vk::Format::eBc7UnormBlock;
	default:
		Zenith_Assert(false, "Invalid format");
		return vk::Format::eUndefined;
	}
}

vk::Format Zenith_Vulkan::ConvertToVkFormat_DepthStencil(TextureFormat eFormat) {
	switch (eFormat)
	{
	case TEXTURE_FORMAT_D32_SFLOAT:
		return vk::Format::eD32Sfloat;
	default:
		Zenith_Assert(false, "Invalid format");
		return vk::Format::eUndefined;
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
		return vk::AttachmentLoadOp::eDontCare;
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
		return vk::AttachmentStoreOp::eDontCare;
	}
}

vk::Format Zenith_Vulkan::ShaderDataTypeToVulkanFormat(ShaderDataType t)
{
	switch (t)
	{
	case SHADER_DATA_TYPE_FLOAT:	return vk::Format::eR32Sfloat;
	case SHADER_DATA_TYPE_FLOAT2:	return vk::Format::eR32G32Sfloat;
	case SHADER_DATA_TYPE_FLOAT3:	return vk::Format::eR32G32B32Sfloat;
	case SHADER_DATA_TYPE_FLOAT4:	return vk::Format::eR32G32B32A32Sfloat;
	case SHADER_DATA_TYPE_INT:		return vk::Format::eR32Sint;
	case SHADER_DATA_TYPE_INT2:		return vk::Format::eR32G32Sint;
	case SHADER_DATA_TYPE_INT3:		return vk::Format::eR32G32B32Sint;
	case SHADER_DATA_TYPE_INT4:		return vk::Format::eR32G32B32A32Sint;
	case SHADER_DATA_TYPE_UINT:		return vk::Format::eR32Uint;
	case SHADER_DATA_TYPE_UINT2:	return vk::Format::eR32G32Uint;
	case SHADER_DATA_TYPE_UINT3:	return vk::Format::eR32G32B32Uint;
	case SHADER_DATA_TYPE_UINT4:	return vk::Format::eR32G32B32A32Uint;
	default:
		Zenith_Assert(false, "Unknown shader data type");
		return vk::Format::eUndefined;
	}
}

vk::DescriptorSet Zenith_Vulkan::CreateDescriptorSet(const vk::DescriptorSetLayout& xLayout, const vk::DescriptorPool& xPool)
{
	vk::DescriptorSetAllocateInfo xInfo = vk::DescriptorSetAllocateInfo()
		.setDescriptorPool(xPool)
		.setDescriptorSetCount(1)
		.setPSetLayouts(&xLayout);

	return (s_xDevice.allocateDescriptorSets(xInfo)[0]);
}
