#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Flux/Flux_PerFrame.h"
#include "Flux/Flux_RendererImpl.h"

#include "Vulkan/Zenith_Vulkan.h"
#include "Vulkan/Zenith_Vulkan_Platform.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include "Flux/Flux.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include <algorithm>
#include <set>

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

#ifdef ZENITH_DEBUG
static std::vector<const char*> s_xValidationLayers = { "VK_LAYER_KHRONOS_validation", /*"VK_LAYER_KHRONOS_synchronization2"*/ };
#endif

// Phase 6b: Vulkan backend state lives on Zenith_Vulkan held by
// Zenith_Engine. Methods below dereference g_xEngine.FluxBackend().m_xXxx.

#ifdef ZENITH_TOOLS
const vk::DescriptorPool& Zenith_Vulkan::GetImGuiDescriptorPool() { return Zenith_Vulkan::m_xImGuiDescriptorPool; }

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
#ifndef ZENITH_ANDROID
				VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME,
#endif
				// Core in Vulkan 1.2 - only needed as extension on 1.1 devices
#ifndef ZENITH_ANDROID
				VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME
#endif
};

// All previously here-defined statics moved to Zenith_Vulkan.

const vk::Instance&         Zenith_Vulkan::GetInstance()                  { return Zenith_Vulkan::m_xInstance; }
const vk::PhysicalDevice&   Zenith_Vulkan::GetPhysicalDevice()            { return Zenith_Vulkan::m_xPhysicalDevice; }
const vk::Device&           Zenith_Vulkan::GetDevice()                    { return Zenith_Vulkan::m_xDevice; }
const vk::CommandPool&      Zenith_Vulkan::GetCommandPool(CommandType eType) { return Zenith_Vulkan::m_axCommandPools[eType]; }
const vk::Queue&            Zenith_Vulkan::GetQueue(CommandType eType)    { return Zenith_Vulkan::m_axQueues[eType]; }
const vk::SurfaceKHR&       Zenith_Vulkan::GetSurface()                   { return Zenith_Vulkan::m_xSurface; }
const uint32_t              Zenith_Vulkan::GetQueueIndex(CommandType eType){ return Zenith_Vulkan::m_auQueueIndices[eType]; }
const vk::DescriptorPool&   Zenith_Vulkan::GetDefaultDescriptorPool()     { return Zenith_Vulkan::m_xDefaultDescriptorPool; }
vk::DescriptorSet&          Zenith_Vulkan::GetBindlessTexturesDescriptorSet()       { return Zenith_Vulkan::m_xBindlessTexturesDescriptorSet; }
vk::DescriptorSetLayout&    Zenith_Vulkan::GetBindlessTexturesDescriptorSetLayout() { return Zenith_Vulkan::m_xBindlessTexturesDescriptorSetLayout; }
#ifdef ZENITH_FLUX_PROFILING
vk::DispatchLoaderDynamic&  Zenith_Vulkan::GetDispatchLoader()            { return Zenith_Vulkan::m_xDispatchLoader; }
#endif
DEBUGVAR bool dbg_bSubmitDrawCalls = true;
DEBUGVAR bool dbg_bUseDescSetCache = true;
DEBUGVAR bool dbg_bOnlyUpdateDirtyDescriptors = true;
DEBUGVAR u_int dbg_uNumDescSetAllocations = 0;

// All pre/post render-pass transitions are now driven by
// Flux_RenderGraph::SynthesizeBarriers and emitted via the per-pass prologue
// barrier list (see RecordCommandBuffersTask::EmitGraphPrologueBarriers).
// The previous TransitionColorTargets / TransitionDepthStencilTarget /
// TransitionTargetsForRenderPass / TransitionTargetsAfterRenderPass helpers
// were deleted as part of the Phase B follow-up consolidation.
//
// Render-pass attachment initialLayout / finalLayout (in
// Zenith_Vulkan_Pipeline::TargetSetupToRenderPass) are set to the working
// layout of the access (COLOR_ATTACHMENT for colour, DEPTH_*_ATTACHMENT for
// depth, depending on bDepthIsReadOnly) so the render pass itself never
// transitions layouts — the graph put the resource there before BeginRenderPass
// and the next pass's prologue moves it elsewhere if needed.

const vk::DescriptorPool& Zenith_Vulkan::GetPerFrameDescriptorPool(u_int uWorkerIndex)
{
	return m_pxCurrentFrame->GetDescriptorPoolForWorkerIndex(uWorkerIndex);
}

const vk::CommandPool& Zenith_Vulkan::GetWorkerCommandPool(u_int uThreadIndex)
{
	return m_pxCurrentFrame->GetCommandPoolForWorkerIndex(uThreadIndex);
}

vk::Fence& Zenith_Vulkan::GetCurrentInFlightFence()
{
	return m_pxCurrentFrame->m_xFence;
}

const bool Zenith_Vulkan::ShouldSubmitDrawCalls() { return dbg_bSubmitDrawCalls; }
const bool Zenith_Vulkan::ShouldUseDescSetCache() { return dbg_bUseDescSetCache; }
const bool Zenith_Vulkan::ShouldOnlyUpdateDirtyDescriptors() { return dbg_bOnlyUpdateDirtyDescriptors; }
#ifdef ZENITH_DEBUG_VARIABLES
void Zenith_Vulkan::IncrementDescriptorSetAllocations(){ dbg_uNumDescSetAllocations++; }
#endif

void Zenith_Vulkan::Initialise()
{
	// Self-wire cross-subsystem dependencies once, here, into member pointers so
	// every steady-state instance method routes through them instead of g_xEngine.
	// Initialise() stays no-arg to satisfy the FluxBackendDevice backend concept;
	// the sibling *Impl objects are allocated up-front, so caching their pointers
	// here (even before they are themselves Initialised) is safe — the cached
	// objects are only USED later at runtime, exactly as before.
	m_pxFluxRenderer = &g_xEngine.FluxRenderer();
	m_pxTasks = &g_xEngine.Tasks();
	m_pxVulkanSwapchain = &g_xEngine.FluxSwapchain();
	m_pxVulkanMemory = &g_xEngine.FluxMemory();

	CreateInstance();
#ifdef ZENITH_DEBUG
	CreateDebugMessenger();
#endif
	CreateSurface();
	CreatePhysicalDevice();
	LogFormatSupport();
	CreateQueueFamilies();
	CreateDevice();
#ifdef ZENITH_FLUX_PROFILING
	m_xDispatchLoader = vk::DispatchLoaderDynamic(m_xInstance, vkGetInstanceProcAddr, m_xDevice, vkGetDeviceProcAddr);
#endif
	CreateCommandPools();
	CreateDefaultDescriptorPool();
	CreateBindlessTexturesDescriptorPool();

	for (Zenith_Vulkan_PerFrame& xFrame : m_axPerFrame)
	{
		xFrame.Initialise();
	}

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables& xDebugVariables = g_xEngine.DebugVariables();
	xDebugVariables.AddBoolean({ "Vulkan", "Submit Draw Calls" }, dbg_bSubmitDrawCalls);
	xDebugVariables.AddBoolean({ "Vulkan", "Use Descriptor Set Cache" }, dbg_bUseDescSetCache);
	xDebugVariables.AddBoolean({ "Vulkan", "Only Update Dirty Descriptors" }, dbg_bOnlyUpdateDirtyDescriptors);

	xDebugVariables.AddUInt32_ReadOnly({ "Vulkan", "Descriptor Sets Allocated" }, dbg_uNumDescSetAllocations);
#endif

	m_pxCurrentFrame = &m_axPerFrame[0];

	// Register the per-frame begin callback with Flux_PerFrame. This is the
	// load-bearing begin callback (waits for the slot's fence, resets descriptor
	// pools, drains typed deletion queues, resets scratch offsets) — register
	// it FIRST so it runs before any other backend's begin callback that might
	// touch the slot. Counted in FLUX_PERFRAME_BEGIN_SUBSCRIBER_TALLY
	// (Flux_PerFrame.cpp): bump that tally if you add another begin callback.
	m_pxFluxRenderer->RegisterBeginFrameCallback(&Zenith_Vulkan::OnFluxPerFrameBegin, nullptr);
}

void Zenith_Vulkan::InitialisePerFrameResources()
{
	for (Zenith_Vulkan_PerFrame& xFrame : m_axPerFrame)
	{
		xFrame.InitialisePerFrameResources();
	}
}

void Zenith_Vulkan::OnFluxPerFrameBegin(u_int uRingIndex, void* /*pUserData*/)
{
	// Frame index / ring index is owned by FrameContext (g_xEngine.Frame());
	// this callback receives the current ring index directly (no longer pulled
	// from the swapchain). The swapchain itself will read
	// GetCurrentFrameIndex(), a thin wrapper over g_xEngine.Frame().GetRingIndex().
	// Static callback (no 'this'): recover the Vulkan singleton once and route
	// all member reaches through xSelf.
	Zenith_Vulkan& xSelf = g_xEngine.FluxBackend();
	xSelf.m_pxCurrentFrame = &xSelf.m_axPerFrame[uRingIndex];
	xSelf.m_pxCurrentFrame->BeginFrame();

#ifdef ZENITH_TOOLS
	// Update shader hot reload system (checks for file changes)
	Flux_ShaderHotReload::Update();
#endif

#ifdef ZENITH_DEBUG_VARIABLES
	dbg_uNumDescSetAllocations = 0;
#endif
}

// Tracks the open render-pass state across passes within a worker's slice.
// Compared element-wise (resource + mip + layer) so two passes writing the
// same base image but different subresources (cube faces, mip levels) restart
// the render pass instead of mistakenly continuing it.
struct RenderPassRecordingState
{
	Flux_RenderGraph_AttachmentRef m_axColourAttachments[FLUX_MAX_TARGETS];
	uint32_t m_uNumColour = 0;
	Flux_RenderGraph_AttachmentRef m_xDepthStencil;
	bool m_bDepthIsReadOnly = false;
};

static void ResetRenderPassState(RenderPassRecordingState& xState)
{
	for (u_int u = 0; u < xState.m_uNumColour; u++)
	{
		xState.m_axColourAttachments[u] = Flux_RenderGraph_AttachmentRef();
	}
	xState.m_uNumColour = 0;
	xState.m_xDepthStencil = Flux_RenderGraph_AttachmentRef();
	xState.m_bDepthIsReadOnly = false;
}

// Aliasing barriers + image/buffer prologue transitions for this pass. Sits
// outside any active render pass scope so vkCmdPipelineBarrier is unrestricted.
// One barrier per aliasing entry (not unioned) so stage masks stay tight — a
// colour→fragment hand-off stays at ColourAttachmentOutput → FragmentShader
// rather than eAllCommands. xEntry.m_pxPass is asserted non-null in
// SubmitCommandList; we no-op if absent so this helper is safe to call early.
static void EmitGraphPrologueBarriers(Zenith_Vulkan_CommandBuffer& xCommandBuffer, const Flux_CommandListEntry& xEntry)
{
	if (!xEntry.m_pxPass) return;

	for (Zenith_Vector<Flux_RenderGraph_AliasingBarrier>::Iterator itA(xEntry.m_pxPass->m_xAliasingBarriers); !itA.Done(); itA.Next())
	{
		const Flux_RenderGraph_AliasingBarrier& rxA = itA.GetData();
		vk::ImageLayout        eSrcLayoutUnused, eDstLayoutUnused;
		vk::AccessFlags        eSrcMask, eDstMask;
		vk::PipelineStageFlags eSrcStage, eDstStage;
		Flux_ResourceAccessToVulkan(rxA.m_eSrcAccess, rxA.m_bSrcIsDepth, eSrcLayoutUnused, eSrcMask, eSrcStage);
		Flux_ResourceAccessToVulkan(rxA.m_eDstAccess, rxA.m_bDstIsDepth, eDstLayoutUnused, eDstMask, eDstStage);

		vk::MemoryBarrier xMemBarrier = vk::MemoryBarrier()
			.setSrcAccessMask(eSrcMask)
			.setDstAccessMask(eDstMask);
		xCommandBuffer.GetCurrentCmdBuffer().pipelineBarrier(
			eSrcStage, eDstStage, vk::DependencyFlags{},
			1, &xMemBarrier, 0, nullptr, 0, nullptr);
	}

	// Image- and buffer-kind prologue barriers share one list (see header doc
	// on Flux_RenderGraph_Barrier); the neutral ResourceBarrier dispatches on
	// resource kind inside the backend.
	for (Zenith_Vector<Flux_RenderGraph_Barrier>::Iterator itB(xEntry.m_pxPass->m_xPrologueBarriers); !itB.Done(); itB.Next())
	{
		const Flux_RenderGraph_Barrier& rxB = itB.GetData();
		xCommandBuffer.ResourceBarrier(
			rxB.m_xResource,
			Flux_SubresourceRange{ rxB.m_uBaseMip, rxB.m_uMipCount, rxB.m_uBaseLayer, rxB.m_uLayerCount },
			rxB.m_eSrcAccess, rxB.m_eDstAccess);
	}
}

// Compute pass: close any open render pass (compute happens outside one),
// emit barriers, then iterate the command list (which becomes vkCmdDispatch).
static void ProcessComputePass(Zenith_Vulkan_CommandBuffer& xCommandBuffer, RenderPassRecordingState& xState,
	const Flux_CommandListEntry& xEntry)
{
	if (xCommandBuffer.m_xCurrentRenderPass != VK_NULL_HANDLE)
	{
		xCommandBuffer.EndRendering();
		ResetRenderPassState(xState);
	}
	EmitGraphPrologueBarriers(xCommandBuffer, xEntry);
	xEntry.m_pxCmdList->IterateCommands(&xCommandBuffer);
}

// Compare attachment lists element-wise (resource pointer + mip + layer).
// Returns true if both lists describe the same target set, allowing render-
// pass continuation; otherwise the recorder must End/Begin a fresh render pass.
static bool RenderTargetsMatch(const Flux_RenderGraph_AttachmentRef* axA, uint32_t uNumA,
	const Flux_RenderGraph_AttachmentRef* axB, uint32_t uNumB,
	const Flux_RenderGraph_AttachmentRef& rxDepthA, const Flux_RenderGraph_AttachmentRef& rxDepthB)
{
	auto RefsMatch = [](const Flux_RenderGraph_AttachmentRef& a, const Flux_RenderGraph_AttachmentRef& b) -> bool
	{
		return a.m_xResource.GetVoidPtr() == b.m_xResource.GetVoidPtr()
			&& a.m_uMip == b.m_uMip
			&& a.m_uLayer == b.m_uLayer;
	};
	if (uNumA != uNumB) return false;
	if (!RefsMatch(rxDepthA, rxDepthB)) return false;
	for (uint32_t u = 0; u < uNumA; u++)
	{
		if (!RefsMatch(axA[u], axB[u])) return false;
	}
	return true;
}

// Graphics pass: decide whether to restart the render pass (target set differs,
// clear requested, depth-readonly flag flipped, or no pass open) or continue
// the existing one. Continuation still End/Begins if there are prologue
// barriers, since vkCmdPipelineBarrier inside a render pass needs subpass
// self-deps we don't model.
static void ProcessRenderPass(Zenith_Vulkan_CommandBuffer& xCommandBuffer, RenderPassRecordingState& xState,
	const Flux_CommandListEntry& xEntry, u_int uInvocationIndex, u_int i)
{
	const Flux_RenderGraph_AttachmentRef* axColourAttachments = xEntry.m_axColourAttachments;
	const uint32_t uNumColour = xEntry.m_uNumColourAttachments;
	const Flux_RenderGraph_AttachmentRef& rxDepthStencil = xEntry.m_xDepthStencil;
	const bool bClear = xEntry.m_bClearTargets;
	const bool bDepthIsReadOnly = xEntry.m_bDepthIsReadOnly;

	const bool bTargetsChanged = !RenderTargetsMatch(
		axColourAttachments, uNumColour,
		xState.m_axColourAttachments, xState.m_uNumColour,
		rxDepthStencil, xState.m_xDepthStencil);

	// Must restart when bDepthIsReadOnly differs because the render pass was
	// created with the previous flag baked into its initial/final layouts.
	if (bTargetsChanged || bClear || xCommandBuffer.m_xCurrentRenderPass == VK_NULL_HANDLE
		|| bDepthIsReadOnly != xState.m_bDepthIsReadOnly)
	{
		if (xCommandBuffer.m_xCurrentRenderPass != VK_NULL_HANDLE)
		{
			xCommandBuffer.EndRendering();
		}
		// Graph-driven prologue barriers put every declared subresource into the
		// layout the upcoming render pass expects (matching the render-pass
		// attachment initialLayout set by TargetSetupToRenderPass).
		EmitGraphPrologueBarriers(xCommandBuffer, xEntry);

		xCommandBuffer.BeginRendering(Flux_RenderingBeginInfo{ axColourAttachments, uNumColour, rxDepthStencil, bClear, bClear, bClear, bDepthIsReadOnly });
		xState.m_uNumColour = uNumColour;
		xState.m_xDepthStencil = rxDepthStencil;
		xState.m_bDepthIsReadOnly = bDepthIsReadOnly;
		for (uint32_t u = 0; u < uNumColour; u++)
		{
			xState.m_axColourAttachments[u] = axColourAttachments[u];
		}
		xEntry.m_pxCmdList->IterateCommands(&xCommandBuffer);
		return;
	}

	Zenith_Assert(xCommandBuffer.m_xCurrentRenderPass != VK_NULL_HANDLE,
		"RecordCommandBuffersTask: Attempting to continue render pass for '%s' but no render pass is active (worker %u, index %u)",
		xEntry.m_pxCmdList->GetName(), uInvocationIndex, i);

	// Same target setup as previous pass — but the pass may still READ different
	// resources as SRVs (e.g. its own dependencies on upstream UAV writes). We
	// have to End+Begin the render pass to emit the barriers; restart rather
	// than skip them.
	const bool bHasBarriers = xEntry.m_pxPass && (
		xEntry.m_pxPass->m_xPrologueBarriers.GetSize() > 0 ||
		xEntry.m_pxPass->m_xAliasingBarriers.GetSize() > 0);
	if (bHasBarriers)
	{
		xCommandBuffer.EndRendering();
		EmitGraphPrologueBarriers(xCommandBuffer, xEntry);
		xCommandBuffer.BeginRendering(Flux_RenderingBeginInfo{ axColourAttachments, uNumColour, rxDepthStencil, false, false, false, bDepthIsReadOnly });
	}
	xEntry.m_pxCmdList->IterateCommands(&xCommandBuffer);
}

// Data-parallel task function to record command buffers in parallel.
//
// W15/W20/W21: consumes the render-graph-produced Flux_CommandListEntry layout.
// The clear flag and target setup come directly from the entry (populated from
// the graph pass at submit time); the execution-order field has been removed.
void Zenith_Vulkan::RecordCommandBuffersTask(void* pData, u_int uInvocationIndex, u_int)
{
	const Flux_WorkDistribution* pWorkDistribution = static_cast<const Flux_WorkDistribution*>(pData);

	// Static task entry point (no 'this'): recover the Vulkan singleton once.
	Zenith_Vulkan& xSelf = g_xEngine.FluxBackend();
	Zenith_Vulkan_CommandBuffer& xCommandBuffer = xSelf.m_pxCurrentFrame->GetWorkerCommandBuffer(uInvocationIndex);
	xCommandBuffer.BeginRecording();

	RenderPassRecordingState xState;

	const u_int uStartIndex = pWorkDistribution->auStartIndex[uInvocationIndex];
	const u_int uEndIndex = pWorkDistribution->auEndIndex[uInvocationIndex];

	for (u_int i = uStartIndex; i < uEndIndex; i++)
	{
		const Flux_CommandListEntry& xEntry = g_xEngine.FluxRenderer().GetPendingCommandLists().Get(i);
		const bool bIsComputePass = (xEntry.m_uNumColourAttachments == 0 && !xEntry.m_xDepthStencil.IsValid());
		if (bIsComputePass)
		{
			ProcessComputePass(xCommandBuffer, xState, xEntry);
		}
		else
		{
			ProcessRenderPass(xCommandBuffer, xState, xEntry, uInvocationIndex, i);
		}
	}

	if (xState.m_uNumColour > 0 || xState.m_xDepthStencil.IsValid())
	{
		xCommandBuffer.EndRendering();
		// No after-pass transition — the resource sits in its render-pass
		// finalLayout (COLOR_ATTACHMENT for colour, DEPTH_*_ATTACHMENT for
		// depth). The next pass that touches it (in this frame or the next)
		// will emit the necessary transition via SynthesizeBarriers when it
		// declares its access.
	}

	VkCheck(xCommandBuffer.GetCurrentCmdBuffer().end());
}

void Zenith_Vulkan::EndFrame(bool bSubmitRenderWork)
{
	// Memory update is handled separately before render graph execution

	vk::PipelineStageFlags eMemWaitStages = vk::PipelineStageFlagBits::eAllCommands;
	vk::PipelineStageFlags eRenderWaitStages = vk::PipelineStageFlagBits::eAllCommands;

	const vk::Semaphore& xMemorySemaphore = m_pxCurrentFrame->GetMemorySemaphore();

	std::vector<vk::CommandBuffer> xPlatformMemoryCmdBufs;
	if (m_pxMemoryUpdateCmdBuf)
	{
		xPlatformMemoryCmdBufs.push_back(m_pxMemoryUpdateCmdBuf->GetCurrentCmdBuffer());
		m_pxMemoryUpdateCmdBuf = nullptr;

	}

	// Prepare frame work distribution in platform-independent layer
	// Do this BEFORE memory submit so we know whether to signal the semaphore
	Flux_WorkDistribution xWorkDistribution;
	const bool bHasRenderWork = bSubmitRenderWork && m_pxFluxRenderer->PrepareFrame(xWorkDistribution);

	const bool bShouldWait = m_pxVulkanSwapchain->ShouldWaitOnImageAvailableSemaphore();
	vk::SubmitInfo xMemorySubmitInfo = vk::SubmitInfo()
		.setCommandBufferCount(static_cast<uint32_t>(xPlatformMemoryCmdBufs.size()))
		.setPCommandBuffers(xPlatformMemoryCmdBufs.data())
		// Only signal semaphore if we have render work that will wait on it
		.setPSignalSemaphores(bHasRenderWork ? &xMemorySemaphore : nullptr)
		.setSignalSemaphoreCount(bHasRenderWork ? 1 : 0)
		.setWaitDstStageMask(eMemWaitStages)
		.setPWaitSemaphores(bShouldWait ? &m_pxVulkanSwapchain->GetCurrentImageAvailableSemaphore() : nullptr)
		.setWaitSemaphoreCount(bShouldWait);

	//#TO_TODO: change this to copy queue, how do I make sure this finishes before graphics?
	VkCheck(m_axQueues[COMMANDTYPE_GRAPHICS].submit(xMemorySubmitInfo, VK_NULL_HANDLE));

	if (!bHasRenderWork)
	{
		// CRITICAL: Clear pending command lists even when skipping render work
		// Without this, stale command list entries accumulate across frames and can
		// cause access violations when those entries are processed in subsequent frames
		// after scene resources have been destroyed and recreated
		m_pxFluxRenderer->ClearPendingCommandLists();
		return; // No work to do this frame
	}
	
	Zenith_DataParallelTask xRecordingTask(
		ZENITH_PROFILE_INDEX__VULKAN_RECORD_COMMAND_BUFFERS,
		RecordCommandBuffersTask,
		&xWorkDistribution,
		FLUX_NUM_WORKER_THREADS,
		true
	);

	m_pxTasks->SubmitDataParallelTask(&xRecordingTask);
	xRecordingTask.WaitUntilComplete();

	// Clear all pending command lists now that recording is complete
	m_pxFluxRenderer->ClearPendingCommandLists();

	// Submit all worker command buffers in order (0 to 7)
	// This maintains correct render order since work is distributed contiguously
	std::vector<vk::CommandBuffer> xCommandBuffersToSubmit;
	xCommandBuffersToSubmit.reserve(FLUX_NUM_WORKER_THREADS);
	for (u_int i = 0; i < FLUX_NUM_WORKER_THREADS; i++)
	{
		xCommandBuffersToSubmit.push_back(m_pxCurrentFrame->GetWorkerCommandBuffer(i).GetCurrentCmdBuffer());
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

		VkCheck(m_axQueues[COMMANDTYPE_GRAPHICS].submit(xRenderSubmitInfo, VK_NULL_HANDLE));
	}

}

void Zenith_Vulkan::WaitForGPUIdle()
{
	// Wait for all GPU work to complete
	// This is expensive (stalls the entire pipeline) but necessary for critical synchronization
	// Use cases: scene transitions, shutdown, debugging
	VkCheck(m_xDevice.waitIdle());

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

#ifdef ZENITH_DEBUG
	// Check which validation layers are actually available on this device
	std::vector<const char*> xEnabledLayers;
	std::vector<vk::LayerProperties> xAvailableLayers = VkUnwrap(vk::enumerateInstanceLayerProperties());
	for (const char* szLayerName : s_xValidationLayers)
	{
		bool bFound = false;
		for (const auto& xLayer : xAvailableLayers)
		{
			if (strcmp(szLayerName, xLayer.layerName) == 0)
			{
				bFound = true;
				break;
			}
		}
		if (bFound)
		{
			xEnabledLayers.push_back(szLayerName);
			Zenith_Log(LOG_CATEGORY_VULKAN, "Enabling validation layer: %s", szLayerName);
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_VULKAN, "Validation layer not available: %s", szLayerName);
		}
	}
#endif

	// Synchronization validation — catches missed/wrong layout transitions and
	// host-device sync errors that the standard validator silently lets through.
	// Enabled in debug builds; the upload-path WAW hazard that previously made
	// this fatal is fixed by AccessMaskForLayout in
	// Zenith_Vulkan_CommandBuffer.cpp::CreateImageBarrier.
#ifdef ZENITH_DEBUG
	// Validation features turned on for the debug build:
	//   eSynchronizationValidation — flags missing buffer/image memory barriers,
	//     queue-family acquire/release omissions, host-write/shader-read mismatches.
	//   eGpuAssisted               — instruments shader code at runtime and
	//     reports out-of-bounds buffer reads/writes, which is exactly the class
	//     of bug behind a "device lost" on a malformed indirect command's
	//     firstIndex/vertexOffset.
	//   eGpuAssistedReserveBindingSlot — required companion to eGpuAssisted;
	//     reserves a descriptor binding for the validation runtime to write its
	//     error log into. Must be enabled together or the layer rejects the pair.
	//   eBestPractices             — non-fatal warnings about API misuse that
	//     often correlates with subtle correctness issues.
	const vk::ValidationFeatureEnableEXT axEnabledFeatures[] = {
		vk::ValidationFeatureEnableEXT::eSynchronizationValidation,
		//vk::ValidationFeatureEnableEXT::eGpuAssisted,
		//vk::ValidationFeatureEnableEXT::eGpuAssistedReserveBindingSlot,
		//vk::ValidationFeatureEnableEXT::eBestPractices,
	};
	vk::ValidationFeaturesEXT xValidationFeatures = vk::ValidationFeaturesEXT()
		.setEnabledValidationFeatureCount(static_cast<uint32_t>(std::size(axEnabledFeatures)))
		.setPEnabledValidationFeatures(axEnabledFeatures);
#endif

	vk::InstanceCreateInfo xInstanceInfo = vk::InstanceCreateInfo()
		.setPApplicationInfo(&xAppInfo)
		.setEnabledExtensionCount(static_cast<uint32_t>(xExtensions.size()))
		.setPpEnabledExtensionNames(xExtensions.data())
#ifdef ZENITH_DEBUG
		.setEnabledLayerCount(static_cast<uint32_t>(xEnabledLayers.size()))
		.setPpEnabledLayerNames(xEnabledLayers.data())
		.setPNext(&xValidationFeatures);
#else
		.setEnabledLayerCount(0);
#endif
	m_xInstance = VkUnwrap(vk::createInstance(xInstanceInfo));

	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan instance created");
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL Zenith_Vulkan::DebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT eMessageSeverity, vk::DebugUtilsMessageTypeFlagsEXT, const vk::DebugUtilsMessengerCallbackDataEXT* pxCallbackData, void*)
{
	if (eMessageSeverity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
	{
		Zenith_Error(LOG_CATEGORY_VULKAN, "VK ERROR: %s", pxCallbackData->pMessage);
		Zenith_DebugBreak();
	}
	else if (eMessageSeverity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
	{
		Zenith_Warning(LOG_CATEGORY_VULKAN, "VK WARN: %s", pxCallbackData->pMessage);
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_VULKAN, "VK INFO: %s", pxCallbackData->pMessage);
	}
	return VK_FALSE;
}

#ifdef ZENITH_DEBUG
void Zenith_Vulkan::CreateDebugMessenger()
{
	vk::DebugUtilsMessengerCreateInfoEXT xCreateInfo = vk::DebugUtilsMessengerCreateInfoEXT()
		.setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
		.setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
			vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
			vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
		.setPfnUserCallback((PFN_vkDebugUtilsMessengerCallbackEXT)DebugCallback)
		.setPUserData(nullptr);
	m_xDebugMessenger = VkUnwrap(m_xInstance.createDebugUtilsMessengerEXT(
		xCreateInfo,
		nullptr,
		vk::DispatchLoaderDynamic(m_xInstance, vkGetInstanceProcAddr)
	));

	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan debug messenger created");
}
#endif

void Zenith_Vulkan::CreateSurface()
{
	// Use platform abstraction for surface creation
	m_xSurface = Zenith_Vulkan_Platform::CreateSurface(m_xInstance);

	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan surface created");
}

void Zenith_Vulkan::CreatePhysicalDevice()
{
	uint32_t uNumDevices;
	vk::Result eResult = m_xInstance.enumeratePhysicalDevices(&uNumDevices, nullptr);
	Zenith_Assert(eResult == vk::Result::eSuccess && uNumDevices > 0, "Failed to find any physical devices with Vulkan support");
	Zenith_Log(LOG_CATEGORY_VULKAN, "%u physical vulkan devices to choose from", uNumDevices);
	std::vector<vk::PhysicalDevice> xDevices;
	xDevices.resize(uNumDevices);
	eResult = m_xInstance.enumeratePhysicalDevices(&uNumDevices, xDevices.data());
	Zenith_Assert(eResult == vk::Result::eSuccess, "Failed to enumerate physical devices");
	for (const vk::PhysicalDevice& xDevice : xDevices)
	{
		//#TO_TODO: check if physical device is suitable
		if (true)
		{
			m_xPhysicalDevice = xDevice;
			break;
		}
	}

	const vk::PhysicalDeviceProperties& xProps = m_xPhysicalDevice.getProperties();
	m_xGPUCapabilities.m_uMaxTextureWidth = xProps.limits.maxImageDimension2D;
	m_xGPUCapabilities.m_uMaxTextureHeight = xProps.limits.maxImageDimension2D;
	m_xGPUCapabilities.m_uMaxFramebufferWidth = xProps.limits.maxFramebufferWidth;
	m_xGPUCapabilities.m_uMaxFramebufferHeight = xProps.limits.maxFramebufferHeight;

	Zenith_Log(LOG_CATEGORY_VULKAN, "GPU: %s", xProps.deviceName);
	Zenith_Log(LOG_CATEGORY_VULKAN, "GPU API version: %u.%u.%u",
		VK_API_VERSION_MAJOR(xProps.apiVersion),
		VK_API_VERSION_MINOR(xProps.apiVersion),
		VK_API_VERSION_PATCH(xProps.apiVersion));
	Zenith_Log(LOG_CATEGORY_VULKAN, "GPU driver version: %u", xProps.driverVersion);
	Zenith_Log(LOG_CATEGORY_VULKAN, "Max image dimension 2D: %u", xProps.limits.maxImageDimension2D);
	Zenith_Log(LOG_CATEGORY_VULKAN, "Max framebuffer: %ux%u", xProps.limits.maxFramebufferWidth, xProps.limits.maxFramebufferHeight);
	Zenith_Log(LOG_CATEGORY_VULKAN, "Max memory alloc count: %u", xProps.limits.maxMemoryAllocationCount);
	Zenith_Log(LOG_CATEGORY_VULKAN, "Max bound descriptor sets: %u", xProps.limits.maxBoundDescriptorSets);

	vk::PhysicalDeviceMemoryProperties xMemProps = m_xPhysicalDevice.getMemoryProperties();
	Zenith_Log(LOG_CATEGORY_VULKAN, "Memory heaps: %u, Memory types: %u", xMemProps.memoryHeapCount, xMemProps.memoryTypeCount);
	for (uint32_t i = 0; i < xMemProps.memoryHeapCount; i++)
	{
		Zenith_Log(LOG_CATEGORY_VULKAN, "  Heap %u: %llu MB, flags: %u", i,
			xMemProps.memoryHeaps[i].size / (1024 * 1024),
			static_cast<uint32_t>(xMemProps.memoryHeaps[i].flags));
	}
}

void Zenith_Vulkan::LogFormatSupport()
{
	// Check support for formats used by the renderer
	struct FormatCheck
	{
		vk::Format m_eFormat;
		const char* m_szName;
		vk::FormatFeatureFlags m_eRequired;
	};

	FormatCheck axFormats[] = {
		{ vk::Format::eR8G8B8A8Unorm, "RGBA8_UNORM (MRT diffuse/material)", vk::FormatFeatureFlagBits::eColorAttachment | vk::FormatFeatureFlagBits::eSampledImage },
		{ vk::Format::eR16G16B16A16Sfloat, "RGBA16F (MRT normals)", vk::FormatFeatureFlagBits::eColorAttachment | vk::FormatFeatureFlagBits::eSampledImage },
		{ vk::Format::eR16G16B16A16Unorm, "RGBA16_UNORM (final RT)", vk::FormatFeatureFlagBits::eColorAttachment | vk::FormatFeatureFlagBits::eSampledImage },
		{ vk::Format::eD32Sfloat, "D32F (depth)", vk::FormatFeatureFlagBits::eDepthStencilAttachment },
		{ vk::Format::eB8G8R8A8Srgb, "BGRA8_SRGB (swapchain)", vk::FormatFeatureFlagBits::eColorAttachment },
		{ vk::Format::eR8G8B8A8Srgb, "RGBA8_SRGB (swapchain fallback)", vk::FormatFeatureFlagBits::eColorAttachment },
	};

	Zenith_Log(LOG_CATEGORY_VULKAN, "=== Format support check ===");
	for (const FormatCheck& xCheck : axFormats)
	{
		vk::FormatProperties xProps = m_xPhysicalDevice.getFormatProperties(xCheck.m_eFormat);
		bool bOptimalSupported = (xProps.optimalTilingFeatures & xCheck.m_eRequired) == xCheck.m_eRequired;
		Zenith_Log(LOG_CATEGORY_VULKAN, "  %s: optimal=%s linear=0x%x optimal=0x%x buffer=0x%x",
			xCheck.m_szName,
			bOptimalSupported ? "YES" : "NO",
			static_cast<uint32_t>(xProps.linearTilingFeatures),
			static_cast<uint32_t>(xProps.optimalTilingFeatures),
			static_cast<uint32_t>(xProps.bufferFeatures));
		if (!bOptimalSupported)
		{
			Zenith_Warning(LOG_CATEGORY_VULKAN, "FORMAT NOT SUPPORTED: %s - this will likely cause rendering issues!", xCheck.m_szName);
		}
	}
	Zenith_Log(LOG_CATEGORY_VULKAN, "=== End format support check ===");
}

void Zenith_Vulkan::CreateQueueFamilies()
{
	for (uint32_t& uIndex : m_auQueueIndices)
	{
		uIndex = UINT32_MAX;
	}

	std::vector<vk::QueueFamilyProperties> xQueueFamilyProperties = m_xPhysicalDevice.getQueueFamilyProperties();

	VkBool32 uSupportsPresent = false;

	for (uint32_t i = 0; i < xQueueFamilyProperties.size(); ++i)
	{
		uSupportsPresent = static_cast<VkBool32>(VkUnwrap(m_xPhysicalDevice.getSurfaceSupportKHR(i, m_xSurface)));

		if (m_auQueueIndices[COMMANDTYPE_GRAPHICS] == UINT32_MAX && xQueueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics)
		{
			m_auQueueIndices[COMMANDTYPE_GRAPHICS] = i;
			if (uSupportsPresent && m_auQueueIndices[COMMANDTYPE_PRESENT] == UINT32_MAX) {
				m_auQueueIndices[COMMANDTYPE_PRESENT] = i;
			}
		}

		if (m_auQueueIndices[COMMANDTYPE_GRAPHICS] != i && m_auQueueIndices[COMMANDTYPE_COMPUTE] == UINT32_MAX && xQueueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eCompute)
		{
			m_auQueueIndices[COMMANDTYPE_COMPUTE] = i;
		}

		if (m_auQueueIndices[COMMANDTYPE_COPY] == UINT32_MAX && xQueueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eTransfer && xQueueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics)
		{
			m_auQueueIndices[COMMANDTYPE_COPY] = i;
		}
	}

	for (uint32_t uType = 0; uType < COMMANDTYPE_MAX; uType++)
	{
		Zenith_Assert(m_auQueueIndices[uType] != UINT32_MAX, "Couldn't find queue index");
	}

	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan queue families created");
}

void Zenith_Vulkan::CreateDevice()
{
	std::vector<vk::DeviceQueueCreateInfo> xQueueInfos;
	std::set<uint32_t> xUniqueFamilies;
	for (uint32_t i = 0; i < COMMANDTYPE_MAX; i++)
	{
		xUniqueFamilies.insert(m_auQueueIndices[i]);
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
		.setEnabledLayerCount(0);

	

	vk::PhysicalDeviceFeatures xDeviceFeatures = vk::PhysicalDeviceFeatures()
		.setSamplerAnisotropy(VK_TRUE)
		.setTessellationShader(VK_TRUE)
		.setDepthBiasClamp(VK_TRUE)
		.setMultiDrawIndirect(VK_TRUE)
		// Required so the terrain compute shader can write a non-zero
		// firstInstance to its indirect draw commands. firstInstance carries
		// the stable per-chunk index that the terrain vertex shader reads via
		// SV_StartInstanceLocation to look up LODLevelBuffer[chunkIndex] —
		// without this feature, any non-zero firstInstance in an indirect
		// draw is undefined behaviour (manifests as holes / device-lost on
		// chunks past the first).
		.setDrawIndirectFirstInstance(VK_TRUE)
		.setFillModeNonSolid(VK_TRUE);


	vk::PhysicalDeviceFeatures2 xDeviceFeatures2 = vk::PhysicalDeviceFeatures2()
		.setFeatures(xDeviceFeatures);

	vk::PhysicalDeviceShaderDrawParameterFeatures xShaderDrawFeatures = vk::PhysicalDeviceShaderDrawParameterFeatures()
	.setShaderDrawParameters(VK_TRUE)
		.setPNext(&xDeviceFeatures2);

	vk::PhysicalDeviceDescriptorIndexingFeatures xIndexingFeatures = vk::PhysicalDeviceDescriptorIndexingFeatures()
		.setDescriptorBindingSampledImageUpdateAfterBind(true)
		.setDescriptorBindingPartiallyBound(true)
		.setRuntimeDescriptorArray(true)
		.setPNext(&xShaderDrawFeatures);

#ifndef ZENITH_ANDROID
	vk::PhysicalDeviceFeatures2 xTemp;
	vk::PhysicalDeviceFragmentShaderBarycentricFeaturesKHR xTempBary;
	xTemp.setPNext(&xTempBary);
	m_xPhysicalDevice.getFeatures2(&xTemp);
	xTempBary.setPNext(&xIndexingFeatures);
	xDeviceCreateInfo.setPNext(&xTempBary);
#else
	xDeviceCreateInfo.setPNext(&xIndexingFeatures);
#endif

	m_xDevice = VkUnwrap(m_xPhysicalDevice.createDevice(xDeviceCreateInfo));

	for (uint32_t i = 0; i < COMMANDTYPE_MAX; i++)
	{
		m_axQueues[i] = m_xDevice.getQueue(m_auQueueIndices[i], 0);
	}


	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan device created");
}

void Zenith_Vulkan::CreateCommandPools()
{
	for (uint32_t i = 0; i < COMMANDTYPE_MAX; i++)
	{
		m_axCommandPools[i] = VkUnwrap(m_xDevice.createCommandPool(vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, m_auQueueIndices[i])));
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

	m_xDefaultDescriptorPool = VkUnwrap(m_xDevice.createDescriptorPool(xPoolInfo));

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

	m_xBindlessTexturesDescriptorPool = VkUnwrap(m_xDevice.createDescriptorPool(xPoolInfo));

	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan bindless textures descriptor pool created");

	vk::DescriptorSetLayoutBinding xBind = vk::DescriptorSetLayoutBinding()
		.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
		.setDescriptorCount(1000)
		.setBinding(0)
		.setStageFlags(vk::ShaderStageFlagBits::eAll)
		.setPImmutableSamplers(nullptr);

	vk::DescriptorBindingFlags xBindingFlags = vk::DescriptorBindingFlagBits::eUpdateAfterBind | vk::DescriptorBindingFlagBits::ePartiallyBound;

	vk::DescriptorSetLayoutBindingFlagsCreateInfo xBindingFlagsInfo = vk::DescriptorSetLayoutBindingFlagsCreateInfo()
		.setBindingCount(1)
		.setPBindingFlags(&xBindingFlags);

	vk::DescriptorSetLayoutCreateInfo xLayoutInfo = vk::DescriptorSetLayoutCreateInfo()
		.setBindingCount(1)
		.setPBindings(&xBind)
		.setFlags(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool)
		.setPNext(&xBindingFlagsInfo);

	m_xBindlessTexturesDescriptorSetLayout = VkUnwrap(m_xDevice.createDescriptorSetLayout(xLayoutInfo));

	vk::DescriptorSetAllocateInfo xSetInfo = vk::DescriptorSetAllocateInfo()
		.setDescriptorPool(m_xBindlessTexturesDescriptorPool)
		.setDescriptorSetCount(1)
		.setPSetLayouts(&m_xBindlessTexturesDescriptorSetLayout);

	m_xBindlessTexturesDescriptorSet = VkUnwrap(m_xDevice.allocateDescriptorSets(xSetInfo))[0];
}

void Zenith_Vulkan::WriteBindlessDescriptor(uint32_t uIndex, vk::ImageView xImageView, vk::Sampler xSampler)
{
	vk::DescriptorImageInfo xImageInfo = vk::DescriptorImageInfo()
		.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
		.setImageView(xImageView)
		.setSampler(xSampler);

	vk::WriteDescriptorSet xWrite = vk::WriteDescriptorSet()
		.setDstSet(m_xBindlessTexturesDescriptorSet)
		.setDstBinding(0)
		.setDstArrayElement(uIndex)
		.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
		.setDescriptorCount(1)
		.setPImageInfo(&xImageInfo);

	m_xDevice.updateDescriptorSets(1, &xWrite, 0, nullptr);
}

void Zenith_Vulkan::WriteBindlessTextureSlot(uint32_t uIndex, const Flux_ShaderResourceView& xView, const Zenith_Vulkan_Sampler& xSampler)
{
	const vk::ImageView xVkView = m_pxVulkanMemory->GetImageView(xView.m_xImageViewHandle);
	WriteBindlessDescriptor(uIndex, xVkView, xSampler.GetSampler());
}

#ifdef ZENITH_TOOLS

uint64_t Zenith_Vulkan::CreateImGuiTextureID(const Flux_ShaderResourceView& xView, const Zenith_Vulkan_Sampler& xSampler)
{
	// Build an ImGui-compatible texture ID by allocating a one-shot descriptor
	// set out of the per-frame worker-0 pool, writing the (sampler + image view)
	// pair into it, and returning the descriptor-set handle as a uint64. The
	// pool is reset every frame so the descriptor set is implicitly freed —
	// this is intended for ImGui::Image preview widgets that re-issue every
	// frame, NOT for long-lived textures. The descriptor-set layout is created
	// once in InitialiseImGui and reused here.
	// Worker index 0 — this is called from the main thread during ImGui rendering.
	vk::DescriptorSetAllocateInfo xAllocInfo = vk::DescriptorSetAllocateInfo()
		.setDescriptorPool(GetPerFrameDescriptorPool(0))
		.setDescriptorSetCount(1)
		.setPSetLayouts(&m_xImGuiPreviewLayout);

	vk::DescriptorSet xSet = m_xDevice.allocateDescriptorSets(xAllocInfo)[0];

	vk::DescriptorImageInfo xImageInfo = vk::DescriptorImageInfo()
		.setSampler(xSampler.GetSampler())
		.setImageView(m_pxVulkanMemory->GetImageView(xView.m_xImageViewHandle))
		.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

	vk::WriteDescriptorSet xWriteInfo = vk::WriteDescriptorSet()
		.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
		.setDstSet(xSet)
		.setDstBinding(0)
		.setDstArrayElement(0)
		.setDescriptorCount(1)
		.setPImageInfo(&xImageInfo);

	m_xDevice.updateDescriptorSets(1, &xWriteInfo, 0, nullptr);

	return reinterpret_cast<uint64_t>(static_cast<VkDescriptorSet>(xSet));
}


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

	m_xImGuiDescriptorPool = m_xDevice.createDescriptorPool(xImGuiPoolInfo);

	// One reusable layout for ImGui preview widgets (CreateImGuiTextureID). The
	// layout never changes — 1 combined-image-sampler binding in the fragment
	// stage — so caching a single instance avoids leaking one layout per call.
	vk::DescriptorSetLayoutBinding xPreviewBinding = vk::DescriptorSetLayoutBinding()
		.setBinding(0)
		.setDescriptorCount(1)
		.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
		.setStageFlags(vk::ShaderStageFlagBits::eFragment);
	vk::DescriptorSetLayoutCreateInfo xPreviewLayoutInfo = vk::DescriptorSetLayoutCreateInfo()
		.setBindingCount(1)
		.setPBindings(&xPreviewBinding);
	m_xImGuiPreviewLayout = m_xDevice.createDescriptorSetLayout(xPreviewLayoutInfo);

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
	xInitInfo.Instance = m_xInstance;
	xInitInfo.PhysicalDevice = m_xPhysicalDevice;
	xInitInfo.Device = m_xDevice;
	xInitInfo.QueueFamily = m_auQueueIndices[COMMANDTYPE_GRAPHICS];
	xInitInfo.Queue = m_axQueues[COMMANDTYPE_GRAPHICS];
	xInitInfo.DescriptorPool = m_xImGuiDescriptorPool;  // Use dedicated ImGui pool
	xInitInfo.MinImageCount = MAX_FRAMES_IN_FLIGHT;
	xInitInfo.ImageCount = MAX_FRAMES_IN_FLIGHT;
	
	// Set up pipeline info for main viewport (newer ImGui API)
	xInitInfo.PipelineInfoMain.RenderPass = m_xImGuiRenderPass;
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
		.setFormat(m_pxVulkanSwapchain->GetFormat())
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
	
	m_xImGuiRenderPass = m_xDevice.createRenderPass(xRenderPassInfo);
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
	VkCheck(m_xDevice.waitIdle());

	// Shutdown ImGui backends
	ImGui_ImplVulkan_Shutdown();
#ifdef ZENITH_WINDOWS
	ImGui_ImplGlfw_Shutdown();
#endif
	ImGui::DestroyContext();

	// Destroy ImGui Vulkan resources
	m_xDevice.destroyRenderPass(m_xImGuiRenderPass);
	m_xDevice.destroyDescriptorPool(m_xImGuiDescriptorPool);
	m_xDevice.destroyDescriptorSetLayout(m_xImGuiPreviewLayout);
	m_xImGuiPreviewLayout = vk::DescriptorSetLayout();

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

	Zenith_Vulkan& xVulkan = g_xEngine.FluxBackend();

	for (vk::DescriptorPool& xPool : m_axDescriptorPools)
	{
		xPool = VkUnwrap(xVulkan.GetDevice().createDescriptorPool(xPoolInfo));
	}

	// Create per-worker-thread command pools for multithreaded command buffer recording
	for (u_int i = 0; i < NUM_WORKER_THREADS; i++)
	{
		m_axCommandPools[i] = VkUnwrap(xVulkan.GetDevice().createCommandPool(
			vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			xVulkan.GetQueueIndex(COMMANDTYPE_GRAPHICS))));

		// Initialize worker command buffers with their dedicated command pools and worker index
		m_axWorkerCommandBuffers[i].InitialiseWithCustomPool(m_axCommandPools[i], i);
	}

	vk::FenceCreateInfo xFenceInfo = vk::FenceCreateInfo()
		.setFlags(vk::FenceCreateFlagBits::eSignaled);
	m_xFence = VkUnwrap(xVulkan.GetDevice().createFence(xFenceInfo));

	// Create persistent semaphore for memory submit synchronization (fixes per-frame semaphore leak)
	m_xMemorySemaphore = VkUnwrap(xVulkan.GetDevice().createSemaphore(vk::SemaphoreCreateInfo()));
}

void Zenith_Vulkan_PerFrame::InitialisePerFrameResources()
{
	// Create scratch buffer for push constant replacement
	Zenith_Vulkan_MemoryManager::PersistentBuffer xScratch = g_xEngine.FluxMemory().CreatePersistentlyMappedBuffer(
		uSCRATCH_BUFFER_SIZE,
		vk::BufferUsageFlagBits::eUniformBuffer);
	m_xScratchBuffer = xScratch.m_xBuffer;
	m_xScratchAllocation = xScratch.m_xAllocation;
	m_pScratchBufferMapped = xScratch.m_pMappedPtr;

	// Query alignment requirement
	m_uMinAlignment = static_cast<u_int>(g_xEngine.FluxBackend().GetPhysicalDevice().getProperties().limits.minUniformBufferOffsetAlignment);

	// Initialize worker offsets
	for (u_int i = 0; i < NUM_WORKER_THREADS; i++)
	{
		m_auWorkerScratchOffsets[i] = i * uWORKER_PARTITION_SIZE;
	}
}

void Zenith_Vulkan_PerFrame::ShutdownScratchBuffer()
{
	// Scratch buffer is created via CreatePersistentlyMappedBuffer which bypasses
	// the VRAM registry, so it must be destroyed directly here before the VMA
	// allocator goes away in Zenith_Vulkan_MemoryManager::Shutdown.
	if (m_xScratchAllocation != VK_NULL_HANDLE)
	{
		vmaDestroyBuffer(g_xEngine.FluxMemory().GetVMAAllocator(), m_xScratchBuffer, m_xScratchAllocation);
		m_xScratchBuffer = VK_NULL_HANDLE;
		m_xScratchAllocation = VK_NULL_HANDLE;
		m_pScratchBufferMapped = nullptr;
	}
}

void Zenith_Vulkan_PerFrame::BeginFrame()
{
	const vk::Device& xDevice = g_xEngine.FluxBackend().GetDevice();

	// Pre-flight fence status check — under healthy operation the fence is
	// either signalled (previous use of this slot completed) or initial-signalled
	// (very first use). If a ring-advance bug lets the counter wrap past valid
	// submissions, the fence will be unsignalled and the waitForFences below
	// would block indefinitely. Catch that upstream with a clear message.
#ifdef ZENITH_DEBUG
	{
		const vk::Result eStatus = xDevice.getFenceStatus(m_xFence);
		Zenith_Assert(eStatus == vk::Result::eSuccess || eStatus == vk::Result::eNotReady,
			"Zenith_Vulkan_PerFrame::BeginFrame: fence in unexpected state %d. "
			"Expected eSuccess (signalled) or eNotReady (GPU still working); anything else "
			"suggests the ring counter advanced past a valid submission — see H3 in the code review.",
			static_cast<int>(eStatus));
	}
#endif

	Zenith_Profiling& xProfiling = g_xEngine.Profiling();

	xProfiling.BeginProfile(ZENITH_PROFILE_INDEX__VULKAN_WAIT_FOR_GPU);
	vk::Result eResult = xDevice.waitForFences(1, &m_xFence, VK_TRUE, UINT64_MAX);
	Zenith_Assert(eResult == vk::Result::eSuccess, "Failed to wait for fence");
	xProfiling.EndProfile(ZENITH_PROFILE_INDEX__VULKAN_WAIT_FOR_GPU);
	eResult = xDevice.resetFences(1, &m_xFence);
	Zenith_Assert(eResult == vk::Result::eSuccess, "Failed to reset fence");

	xProfiling.BeginProfile(ZENITH_PROFILE_INDEX__VULKAN_RESET_DESCRIPTOR_POOLS);
	for (vk::DescriptorPool& xPool : m_axDescriptorPools)
	{
		xDevice.resetDescriptorPool(xPool);
	}
	xProfiling.EndProfile(ZENITH_PROFILE_INDEX__VULKAN_RESET_DESCRIPTOR_POOLS);
	// Destroy framebuffers and render passes from the previous use of this frame slot
	for (vk::Framebuffer& xFB : m_axPendingFramebuffers)
	{
		xDevice.destroyFramebuffer(xFB);
	}
	m_axPendingFramebuffers.clear();

	for (vk::RenderPass& xRP : m_axPendingRenderPasses)
	{
		xDevice.destroyRenderPass(xRP);
	}
	m_axPendingRenderPasses.clear();

	// Reset scratch buffer offsets for each worker
	for (u_int i = 0; i < NUM_WORKER_THREADS; i++)
	{
		m_auWorkerScratchOffsets[i] = i * uWORKER_PARTITION_SIZE;
	}
}

void Zenith_Vulkan_PerFrame::DeferDestroyFramebuffer(vk::Framebuffer xFramebuffer)
{
	m_xDeferredDestroyMutex.Lock();
	m_axPendingFramebuffers.push_back(xFramebuffer);
	m_xDeferredDestroyMutex.Unlock();
}

void Zenith_Vulkan_PerFrame::DeferDestroyRenderPass(vk::RenderPass xRenderPass)
{
	m_xDeferredDestroyMutex.Lock();
	m_axPendingRenderPasses.push_back(xRenderPass);
	m_xDeferredDestroyMutex.Unlock();
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
	// TODO: graceful spill — fall back to a VMA sub-allocation when a worker
	// partition fills. Today an assert, i.e. crash.
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
	if (!m_xFreeVRAMHandles.empty())
	{
		uint32_t uFreeIndex = m_xFreeVRAMHandles.back();
		m_xFreeVRAMHandles.pop_back();

		xHandle.SetValue(uFreeIndex);
		m_xVRAMRegistry[uFreeIndex] = pxVRAM;
	}
	else
	{
		// No free handles, grow the registry
		xHandle.SetValue(static_cast<u_int>(m_xVRAMRegistry.size()));
		m_xVRAMRegistry.push_back(pxVRAM);
	}
	
	return xHandle;
}

Zenith_Vulkan_VRAM* Zenith_Vulkan::GetVRAM(const Flux_VRAMHandle xHandle)
{
	// Headless mode (Zenith_CommandLine::IsHeadless()): Create*VRAM returns
	// invalid (UINT32_MAX) handles, and asset-load paths still propagate them
	// to view-creation / upload sites that read via GetVRAM. Return nullptr
	// instead of asserting so those sites' existing null-pxVRAM guards run.
	if (!xHandle.IsValid())
	{
		return nullptr;
	}
	Zenith_Assert(xHandle.AsUInt() < m_xVRAMRegistry.size(), "Invalid VRAM handle");
	return m_xVRAMRegistry[xHandle.AsUInt()];
}

void Zenith_Vulkan::ReleaseVRAMHandle(const Flux_VRAMHandle xHandle)
{
	if (!xHandle.IsValid())
	{
		return;
	}
	
	Zenith_Assert(xHandle.AsUInt() < m_xVRAMRegistry.size(), "Invalid VRAM handle");

	// Mark slot as free by setting to nullptr
	m_xVRAMRegistry[xHandle.AsUInt()] = nullptr;

	// Add index to free list for recycling
	m_xFreeVRAMHandles.push_back(xHandle.AsUInt());
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
	case TEXTURE_FORMAT_RGBA8_SRGB:
		return vk::Format::eR8G8B8A8Srgb;
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
	// Single-channel formats
	case TEXTURE_FORMAT_R8_UNORM:
		return vk::Format::eR8Unorm;
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
	case SHADER_DATA_TYPE_HALF2:				return vk::Format::eR16G16Sfloat;
	case SHADER_DATA_TYPE_SNORM10_10_10_2:		return vk::Format::eA2B10G10R10SnormPack32;
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

	return VkUnwrap(m_xDevice.allocateDescriptorSets(xInfo))[0];
}
