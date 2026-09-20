#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_CommandLine.h"   // --indirect-count-mode (converted to Flux_IndirectDrawOverride at device init)
#include "Vulkan/Zenith_Vulkan_DeviceSelection.h"
#include "Vulkan/Zenith_Vulkan_IndirectCount.h"
#if defined(ZENITH_TOOLS) && defined(ZENITH_INPUT_SIMULATOR)
#include "Core/Zenith_ImGuiBridgeHook.h"
#endif
#include "Flux/Flux_PerFrame.h"
#include "Flux/Flux_RendererImpl.h"

#include "Vulkan/Zenith_Vulkan.h"
#include "Vulkan/Zenith_Vulkan_Platform.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include "Flux/Flux.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_PersistentSetLayouts.h"   // VIEW-set binding indices (Phase 5.4)
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include <algorithm>
#include <set>

#ifdef ZENITH_WINDOWS
#include "Zenith_Windows_Window.h"
#include "Core/Zenith_EditorFontHook.h"
#endif

#ifdef ZENITH_TOOLS
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#ifdef ZENITH_WINDOWS
#include "backends/imgui_impl_glfw.h"
#endif //ZENITH_WINDOWS
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
#ifdef ZENITH_RAYTRACING
				VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
				VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
				VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
				VK_KHR_RAY_QUERY_EXTENSION_NAME,
#endif
};

static bool s_bHasExtension(const std::vector<vk::ExtensionProperties>& axAvailable,
	const char* szExtensionName)
{
	for (const vk::ExtensionProperties& xExt : axAvailable)
	{
		if (strcmp(xExt.extensionName, szExtensionName) == 0)
		{
			return true;
		}
	}
	return false;
}

// Required names exactly mirror CreateDevice's enabled-extension list. Features
// promoted into core stay extension-free at/above their promotion version.
static bool s_bSupportsRequiredDeviceExtensions(const vk::PhysicalDevice& xDevice,
	uint32_t uApiVersion)
{
	const std::vector<vk::ExtensionProperties> axAvailable =
		VkUnwrap(xDevice.enumerateDeviceExtensionProperties());
	for (const char* szRequired : s_aszDeviceExtensions)
	{
		if (!s_bHasExtension(axAvailable, szRequired))
		{
			return false;
		}
	}
	if (uApiVersion < VK_API_VERSION_1_2 &&
		!s_bHasExtension(axAvailable, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME))
	{
		return false;
	}
#ifdef ZENITH_RAYTRACING
	if (uApiVersion < VK_API_VERSION_1_2)
	{
		if (!s_bHasExtension(axAvailable, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME)) return false;
		if (!s_bHasExtension(axAvailable, VK_KHR_SPIRV_1_4_EXTENSION_NAME)) return false;
		if (!s_bHasExtension(axAvailable, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)) return false;
	}
#endif
	return true;
}

// Optional extensions must be queried before they are enabled: requesting a
// name the device does not enumerate makes vkCreateDevice fail with
// VK_ERROR_EXTENSION_NOT_PRESENT.
static bool IsDeviceExtensionSupported(const vk::PhysicalDevice& xPhysicalDevice, const char* szExtensionName)
{
	std::vector<vk::ExtensionProperties> xAvailable = VkUnwrap(xPhysicalDevice.enumerateDeviceExtensionProperties());
	return s_bHasExtension(xAvailable, szExtensionName);
}

// All previously here-defined statics moved to Zenith_Vulkan.

const vk::Instance&         Zenith_Vulkan::GetInstance()                  { return Zenith_Vulkan::m_xInstance; }
const vk::PhysicalDevice&   Zenith_Vulkan::GetPhysicalDevice()            { return Zenith_Vulkan::m_xPhysicalDevice; }
const vk::Device&           Zenith_Vulkan::GetDevice()                    { return Zenith_Vulkan::m_xDevice; }
// A separate presentation-only family never receives a pool (see
// CreateCommandPools), so asking for one is a caller bug rather than a
// silently-null allocation source.
const vk::CommandPool&      Zenith_Vulkan::GetCommandPool(CommandType eType)
{
	Zenith_Assert(Zenith_Vulkan::m_axCommandPools[eType],
		"GetCommandPool(%u): no command pool exists for this command type. Presentation "
		"records no commands, so PRESENT has a pool only when it shares the graphics family.",
		static_cast<unsigned>(eType));
	return Zenith_Vulkan::m_axCommandPools[eType];
}
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

void Zenith_Vulkan::ResetCurrentInFlightFence()
{
	// Paired 1:1 with the submit that passes GetCurrentInFlightFence(). Deliberately
	// NOT done in Zenith_Vulkan_PerFrame::BeginFrame — see the lifecycle note there:
	// a frame can begin and then be abandoned by an out-of-date acquire without ever
	// submitting, and a fence reset on such a frame never gets re-signalled.
#ifdef ZENITH_DEBUG
	// BeginFrame waited on this fence and (by the rule above) did not reset it, so
	// it must still be signalled here. Unsignalled means the pairing broke — either
	// a second reset without an intervening wait, or a submit that never happened.
	{
		const vk::Result eStatus = m_xDevice.getFenceStatus(m_pxCurrentFrame->m_xFence);
		Zenith_Assert(eStatus == vk::Result::eSuccess,
			"Zenith_Vulkan::ResetCurrentInFlightFence: fence is not signalled (%d). "
			"Every reset must be preceded by the BeginFrame wait and followed by exactly "
			"one submit — see the lifecycle note in Zenith_Vulkan_PerFrame::BeginFrame.",
			static_cast<int>(eStatus));
	}
#endif
	// ArrayProxy overload, NOT resetFences(1, &fence): the pointer form always
	// returns a [[nodiscard]] vk::Result, which VkCheck discards when exceptions
	// are enabled (warnings are errors here). The ArrayProxy form returns void in
	// that mode and vk::Result when VULKAN_HPP_NO_EXCEPTIONS is set, so VkCheck is
	// correct either way.
	VkCheck(m_xDevice.resetFences(m_pxCurrentFrame->m_xFence));
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
	auto& xEngine = g_xEngine;
	m_pxFluxRenderer = &xEngine.FluxRenderer();
	m_pxTasks = &xEngine.Tasks();
	m_pxVulkanSwapchain = &xEngine.FluxSwapchain();
	m_pxVulkanMemory = &xEngine.FluxMemory();

#ifdef ZENITH_TESTING
	// Device initialisation precedes worker recording, so relaxed stores are
	// sufficient and ensure a backend recreation cannot leak lifetime totals
	// into a later compatibility run.
	m_xTelemetry.m_uNativeCount.store(0u, std::memory_order_relaxed);
	m_xTelemetry.m_uPaddedMulti.store(0u, std::memory_order_relaxed);
	m_xTelemetry.m_uPaddedSingle.store(0u, std::memory_order_relaxed);
	m_xTelemetry.m_uFailClosed.store(0u, std::memory_order_relaxed);
	m_xTelemetry.m_uFixedIndirect.store(0u, std::memory_order_relaxed);
#endif

	CreateInstance();
#ifdef ZENITH_DEBUG
	CreateDebugMessenger();
#endif
	CreateSurface();
	CreatePhysicalDevice();
	LogFormatSupport();
	CreateQueueFamilies();
	CreateDevice();
	AssertVertexFetchFormatSupport();
#ifdef ZENITH_FLUX_PROFILING
	m_xDispatchLoader = vk::DispatchLoaderDynamic(m_xInstance, vkGetInstanceProcAddr, m_xDevice, vkGetDeviceProcAddr);
#endif
	CreateCommandPools();
	CreateDefaultDescriptorPool();
	QueryDescriptorIndexingLimits();
	CreateBindlessTexturesDescriptorPool();
	CreatePersistentDescriptorSets();

	for (Zenith_Vulkan_PerFrame& xFrame : m_axPerFrame)
	{
		xFrame.Initialise();
	}

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables& xDebugVariables = xEngine.DebugVariables();
	xDebugVariables.AddBoolean({ "Vulkan", "Submit Draw Calls" }, dbg_bSubmitDrawCalls);
	xDebugVariables.AddBoolean({ "Vulkan", "Use Descriptor Set Cache" }, dbg_bUseDescSetCache);
	xDebugVariables.AddBoolean({ "Vulkan", "Only Update Dirty Descriptors" }, dbg_bOnlyUpdateDirtyDescriptors);

	xDebugVariables.AddUInt32_ReadOnly({ "Vulkan", "Descriptor Sets Allocated" }, dbg_uNumDescSetAllocations);
#endif

	m_pxCurrentFrame = &m_axPerFrame[0];
}

void Zenith_Vulkan::InitialisePerFrameResources()
{
	for (Zenith_Vulkan_PerFrame& xFrame : m_axPerFrame)
	{
		xFrame.InitialisePerFrameResources();
	}
}

void Zenith_Vulkan::PerFrameBegin(u_int uRingIndex)
{
	// Frame index / ring index is owned by FrameContext (g_xEngine.Frame());
	// this method receives the current ring index directly (no longer pulled
	// from the swapchain). The swapchain itself reads GetCurrentFrameIndex()
	// which is a thin wrapper over g_xEngine.Frame().GetRingIndex().
	m_pxCurrentFrame = &m_axPerFrame[uRingIndex];
	m_pxCurrentFrame->BeginFrame();

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
// QueueRenderPass; we no-op if absent so this helper is safe to call early.
static void EmitGraphPrologueBarriers(Zenith_Vulkan_CommandBuffer& xCommandBuffer, const Flux_RenderPassEntry& xEntry)
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
// emit barriers, then record the pass directly (its callback issues vkCmdDispatch).
static void ProcessComputePass(Zenith_Vulkan_CommandBuffer& xCommandBuffer, RenderPassRecordingState& xState,
	const Flux_RenderPassEntry& xEntry, u_int i)
{
	if (xCommandBuffer.m_xCurrentRenderPass != VK_NULL_HANDLE)
	{
		xCommandBuffer.EndRendering();
		ResetRenderPassState(xState);
	}
	EmitGraphPrologueBarriers(xCommandBuffer, xEntry);
	Flux_RenderGraph::RecordPassInto(xEntry.m_pxPass, xEntry.m_pxGraph, &xCommandBuffer, i);
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
	const Flux_RenderPassEntry& xEntry, u_int uInvocationIndex, u_int i)
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
		Flux_RenderGraph::RecordPassInto(xEntry.m_pxPass, xEntry.m_pxGraph, &xCommandBuffer, i);
		return;
	}

	Zenith_Assert(xCommandBuffer.m_xCurrentRenderPass != VK_NULL_HANDLE,
		"RecordCommandBuffersTask: Attempting to continue render pass for '%s' but no render pass is active (worker %u, index %u)",
		xEntry.m_pxPass->DebugName(), uInvocationIndex, i);

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
	Flux_RenderGraph::RecordPassInto(xEntry.m_pxPass, xEntry.m_pxGraph, &xCommandBuffer, i);
}

// Data-parallel task function to record command buffers in parallel.
//
// Consumes the render-graph-produced Flux_RenderPassEntry layout. The clear flag
// and target setup come directly from the entry (populated from the graph pass at
// queue time); each pass's record callback is invoked directly via
// Flux_RenderGraph::RecordPassInto.
void Zenith_Vulkan::RecordCommandBuffersTask(void* pData, u_int uInvocationIndex, u_int)
{
	const Flux_WorkDistribution* pWorkDistribution = static_cast<const Flux_WorkDistribution*>(pData);

	// Static task entry point (no 'this'): recover the Vulkan singleton once.
	Zenith_Vulkan& xSelf = g_xEngine.FluxBackend();
	Zenith_Vulkan_CommandBuffer& xCommandBuffer = xSelf.m_pxCurrentFrame->GetWorkerCommandBuffer(uInvocationIndex);
	xCommandBuffer.BeginRecording();

#ifdef ZENITH_FLUX_PROFILING
	// GPU profiler: worker 0's command buffer is submitted first, so resetting the
	// timestamp pool here (before any pass records) guarantees the reset precedes
	// every pass's timestamp writes on the GPU timeline. Outside any render pass.
	if (uInvocationIndex == 0)
	{
		xSelf.m_pxCurrentFrame->CmdResetGPUTimers(xCommandBuffer);
	}
#endif

	RenderPassRecordingState xState;

	const u_int uStartIndex = pWorkDistribution->auStartIndex[uInvocationIndex];
	const u_int uEndIndex = pWorkDistribution->auEndIndex[uInvocationIndex];

	for (u_int i = uStartIndex; i < uEndIndex; i++)
	{
		const Flux_RenderPassEntry& xEntry = g_xEngine.FluxRenderer().GetPendingRenderPasses().Get(i);
		const bool bIsComputePass = (xEntry.m_uNumColourAttachments == 0 && !xEntry.m_xDepthStencil.IsValid());
		if (bIsComputePass)
		{
			ProcessComputePass(xCommandBuffer, xState, xEntry, i);
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

void Zenith_Vulkan::RecordFrame(const Flux_WorkDistribution& xWorkDistribution)
{
	// Record every queued render pass directly into the per-worker command
	// buffers, in parallel. Each worker records its contiguous topological slice
	// [auStartIndex, auEndIndex) (RecordCommandBuffersTask → RecordPassInto), so
	// the worker command buffers — submitted in order by EndFrame — preserve the
	// graph's dependency ordering. Driven from Flux_RenderGraph::Execute (in the
	// render-task safe window, before the frame memory submit); the recorded
	// command buffers are retained until EndFrame submits them. The task only
	// reads the distribution, so the const_cast to the void* task payload is safe.
	Zenith_DataParallelTask xRecordingTask(
		ZENITH_PROFILE_ZONE("Vulkan Record Command Buffers"),
		RecordCommandBuffersTask,
		const_cast<Flux_WorkDistribution*>(&xWorkDistribution),
		FLUX_NUM_WORKER_THREADS,
		true
	);

	m_pxTasks->SubmitDataParallelTask(&xRecordingTask);
	xRecordingTask.WaitUntilComplete();
}

void Zenith_Vulkan::EndFrame(bool bSubmitRenderWork)
{
	// SUBMIT ONLY. This frame's render command buffers (if any) were already
	// recorded by RecordFrame (driven from Flux_RenderGraph::Execute, before the
	// frame memory submit). Here we submit the lazily-recorded memory work first
	// (signalling the memory semaphore), then the pre-recorded worker command
	// buffers (waiting on it), in worker order.

	vk::PipelineStageFlags eMemWaitStages = vk::PipelineStageFlagBits::eAllCommands;
	vk::PipelineStageFlags eRenderWaitStages = vk::PipelineStageFlagBits::eAllCommands;

	const vk::Semaphore& xMemorySemaphore = m_pxCurrentFrame->GetMemorySemaphore();

	std::vector<vk::CommandBuffer> xPlatformMemoryCmdBufs;
	if (m_pxMemoryUpdateCmdBuf)
	{
		xPlatformMemoryCmdBufs.push_back(m_pxMemoryUpdateCmdBuf->GetCurrentCmdBuffer());
		m_pxMemoryUpdateCmdBuf = nullptr;
	}

	// Whether render command buffers were recorded this frame. RecordFrame set
	// HasRecordedFrameWork() during Execute; combined with bSubmitRenderWork so a
	// non-rendering frame (scene transition) skips the render submit
	// and the memory submit doesn't signal a semaphore nobody waits on. The
	// pending queue was already drained by RecordFrame, so there's nothing to
	// clear here.
	const bool bHasRenderWork = bSubmitRenderWork && m_pxFluxRenderer->HasRecordedFrameWork();

#ifdef ZENITH_FLUX_PROFILING
	// Remember how many GPU timers this slot recorded so the deferred readback
	// (when this slot is reused MAX_FRAMES_IN_FLIGHT frames hence) knows the count.
	// Only count it when render work is actually submitted — otherwise the
	// timestamp writes never execute on the GPU and the readback must skip.
	m_pxCurrentFrame->m_uGPUTimerReadbackCount = bHasRenderWork
		? m_pxCurrentFrame->m_uGPUTimerCount.load(std::memory_order_relaxed) : 0;
#endif

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
		return; // No render work to submit this frame
	}

	// Submit all worker command buffers in order (0 to N-1).
	// This maintains correct render order since work is distributed contiguously
	// in topological order and pipeline barriers synchronise across the boundaries.
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
		.setApiVersion(uZENITH_VULKAN_REQUESTED_API_VERSION);

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

	// Resolve the platform's mandatory WSI extensions plus optional diagnostics
	// against what the loader and enabled layers actually provide.
	//
	// This filter is load-bearing on Android. VK_EXT_debug_utils is requested
	// whenever ZENITH_FLUX_PROFILING is defined -- which Zenith.h does
	// UNCONDITIONALLY, release as well as debug -- but on Android that
	// extension comes from the VALIDATION LAYER, not the platform loader.
	// Requesting absent debug utilities is not a soft failure: the whole
	// vkCreateInstance returns VK_ERROR_EXTENSION_NOT_PRESENT. That extension is
	// optional and may be dropped, but VK_KHR_surface and the platform surface
	// extension are required to render and must fail initialisation clearly.
	//
	// The availability set must include the extensions the layers we are about
	// to ENABLE provide, which is why this runs after the layer selection above:
	// a plain enumerateInstanceExtensionProperties() lists only the loader's own
	// and implicit layers', so filtering against it alone would discard
	// debug_utils even when the validation layer is present and would have
	// supplied it.
	std::vector<vk::ExtensionProperties> xAvailableExtensions = VkUnwrap(vk::enumerateInstanceExtensionProperties());
#ifdef ZENITH_DEBUG
	for (const char* szLayerName : xEnabledLayers)
	{
		const std::vector<vk::ExtensionProperties> xLayerExtensions =
			VkUnwrap(vk::enumerateInstanceExtensionProperties(std::string(szLayerName)));
		xAvailableExtensions.insert(xAvailableExtensions.end(), xLayerExtensions.begin(), xLayerExtensions.end());
	}
#endif

	std::vector<const char*> xRequestedExtensions = Zenith_Vulkan_Platform::GetRequiredInstanceExtensions();
#ifdef ZENITH_DEBUG
	// VkValidationFeaturesEXT is owned by VK_EXT_validation_features. Request it
	// only when a validation layer is active; if the layer does not expose it,
	// ordinary validation remains usable without the feature pNext chain.
	if (!xEnabledLayers.empty())
	{
		xRequestedExtensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
	}
	bool bValidationFeaturesEnabled = false;
#endif
	m_bDebugUtilsEnabled = false;
	bool bMissingRequiredExtension = false;
	std::vector<const char*> xExtensions;
	for (const char* szExtensionName : xRequestedExtensions)
	{
		bool bFound = false;
		for (const vk::ExtensionProperties& xExtension : xAvailableExtensions)
		{
			if (strcmp(szExtensionName, xExtension.extensionName) == 0)
			{
				bFound = true;
				break;
			}
		}
		if (bFound)
		{
			xExtensions.push_back(szExtensionName);
			if (strcmp(szExtensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
			{
				// Gates the debug messenger AND the per-pass debug markers --
				// both dispatch through vkGetInstanceProcAddr, which returns
				// null for a disabled extension, so calling them unguarded is a
				// jump to address 0.
				m_bDebugUtilsEnabled = true;
			}
#ifdef ZENITH_DEBUG
			else if (strcmp(szExtensionName, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME) == 0)
			{
				bValidationFeaturesEnabled = true;
			}
#endif
		}
		else
		{
			const bool bOptionalDebugUtils = strcmp(szExtensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0;
#ifdef ZENITH_DEBUG
			const bool bOptionalValidationFeatures =
				strcmp(szExtensionName, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME) == 0;
#else
			const bool bOptionalValidationFeatures = false;
#endif
			if (bOptionalDebugUtils || bOptionalValidationFeatures)
			{
				Zenith_Warning(LOG_CATEGORY_VULKAN, "Optional instance extension not available, skipping: %s", szExtensionName);
			}
			else
			{
				bMissingRequiredExtension = true;
				Zenith_Error(LOG_CATEGORY_VULKAN, "Required instance extension not available: %s", szExtensionName);
			}
		}
	}
	Zenith_Assert(!bMissingRequiredExtension, "Required Vulkan WSI extension is unavailable");
	if (bMissingRequiredExtension)
	{
		return;
	}

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
		.setPpEnabledLayerNames(xEnabledLayers.data());
	// The pNext structure is legal only when its owning extension was enabled.
	if (bValidationFeaturesEnabled)
	{
		xInstanceInfo.setPNext(&xValidationFeatures);
	}
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
	// CreateInstance drops instance extensions that will not be there, so
	// VK_EXT_debug_utils may not be enabled even in a debug build -- on Android
	// it exists only when a validation layer .so shipped in the APK. Creating a
	// messenger for a disabled extension is invalid usage, so check first.
	if (!m_bDebugUtilsEnabled)
	{
		Zenith_Log(LOG_CATEGORY_VULKAN, "VK_EXT_debug_utils unavailable; no debug messenger (validation output will be silent)");
		return;
	}

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

// ---- Shared queue-family selection (Phase 4 of the terrain indirect-count
// compatibility plan). Used by BOTH s_bIsPhysicalDeviceHardSuitable and
// CreateQueueFamilies so the suitability check and actual creation agree
// exactly. The render graph records graphics and compute commands into its
// graphics-family workers, so graphics MUST also support compute. Prefer a
// combined graphics/compute/present family, reuse graphics for copy (graphics
// queues support transfer operations), and otherwise select a dedicated family
// for explicit compute buffers. Returns false if any semantic queue is unavailable.
static bool s_bResolveQueueFamilies(
	const vk::PhysicalDevice& xDevice,
	const vk::SurfaceKHR& xSurface,
	uint32_t (&auIndicesOut)[COMMANDTYPE_MAX])
{
	for (uint32_t& u : auIndicesOut) u = UINT32_MAX;

	const std::vector<vk::QueueFamilyProperties> xQF = xDevice.getQueueFamilyProperties();
	if (!xSurface || xQF.empty())
	{
		return false;
	}
	std::vector<Zenith_Vulkan_QueueFamilySupport> axSupport(xQF.size());
	for (uint32_t i = 0; i < xQF.size(); ++i)
	{
		axSupport[i].m_bGraphics = static_cast<bool>(xQF[i].queueFlags & vk::QueueFlagBits::eGraphics);
		axSupport[i].m_bCompute = static_cast<bool>(xQF[i].queueFlags & vk::QueueFlagBits::eCompute);
		axSupport[i].m_bPresent = VkUnwrap(xDevice.getSurfaceSupportKHR(i, xSurface)) == VK_TRUE;
	}

	const Zenith_Vulkan_QueueFamilySelection xSelection =
		Zenith_Vulkan_SelectQueueFamilies(axSupport.data(), static_cast<uint32_t>(axSupport.size()));
	auIndicesOut[COMMANDTYPE_GRAPHICS] = xSelection.m_uGraphics;
	auIndicesOut[COMMANDTYPE_COMPUTE] = xSelection.m_uCompute;
	auIndicesOut[COMMANDTYPE_COPY] = xSelection.m_uCopy;
	auIndicesOut[COMMANDTYPE_PRESENT] = xSelection.m_uPresent;
	return xSelection.IsComplete();
}
// Hard physical-device suitability. Verify every extension and feature that
// CreateDevice enables, plus the documented remaining terrain minimum
// (drawIndirectFirstInstance + shaderDrawParameters) AND the always-on
// compressed-vertex pipeline's samplerAnisotropy/depthBiasClamp/fillModeNonSolid/
// tessellationShader. multiDrawIndirect and the core/KHR draw-indirect-count
// path stay OPTIONAL — their absence is a performance downgrade (the PADDED_SINGLE
// tier is the legal fallback), not a hard miss. Also verifies:
//   - every build-specific device extension is enumerated (including promoted
//     feature extensions only on API versions where they are still required);
//   - the selected graphics family also supports compute because render-graph
//     workers mix both command types; explicit compute may use another family;
//     graphics also supplies the implicit transfer capability;
//   - at least one queue family supports presentation to the surface;
//   - the descriptor-indexing features the engine force-enables in CreateDevice
//     (descriptorBindingSampledImageUpdateAfterBind, descriptorBindingPartiallyBound,
//     runtimeDescriptorArray, shaderSampledImageArrayNonUniformIndexing) are
//     actually advertised — otherwise vkCreateDevice would fail with
//     VK_ERROR_FEATURE_NOT_PRESENT.
// Returns true iff every hard requirement is satisfied.
static bool s_bIsPhysicalDeviceHardSuitable(const vk::PhysicalDevice& xDevice,
	const vk::SurfaceKHR& xSurface)
{
	const uint32_t uApiVersion = xDevice.getProperties().apiVersion;
	// Reject 1.0 before the core getFeatures2 query below; there is deliberately
	// no KHR alias or SPIR-V 1.0 shader branch in this backend.
	if (!Zenith_Vulkan_IsDeviceAPIVersionSupported(uApiVersion)) return false;
	if (!s_bSupportsRequiredDeviceExtensions(xDevice, uApiVersion)) return false;

	// --- Core features ---
	vk::PhysicalDeviceFeatures2 xFeatures;
	vk::PhysicalDeviceShaderDrawParameterFeatures xShaderDrawParameters;
	vk::PhysicalDeviceDescriptorIndexingFeatures xDescriptorIndexing;
#ifdef ZENITH_RAYTRACING
	vk::PhysicalDeviceBufferDeviceAddressFeatures xBufferDeviceAddress;
	vk::PhysicalDeviceAccelerationStructureFeaturesKHR xAccelerationStructure;
	vk::PhysicalDeviceRayTracingPipelineFeaturesKHR xRayTracingPipeline;
	vk::PhysicalDeviceRayQueryFeaturesKHR xRayQuery;
#endif
	xFeatures.setPNext(&xShaderDrawParameters);
	xShaderDrawParameters.setPNext(&xDescriptorIndexing);
#ifdef ZENITH_RAYTRACING
	xDescriptorIndexing.setPNext(&xBufferDeviceAddress);
	xBufferDeviceAddress.setPNext(&xAccelerationStructure);
	xAccelerationStructure.setPNext(&xRayTracingPipeline);
	xRayTracingPipeline.setPNext(&xRayQuery);
#endif
	xDevice.getFeatures2(&xFeatures);

	const vk::PhysicalDeviceFeatures& xCore = xFeatures.features;
	if (xCore.samplerAnisotropy           != VK_TRUE) return false;
	if (xCore.tessellationShader          != VK_TRUE) return false;
	if (xCore.depthBiasClamp              != VK_TRUE) return false;
	if (xCore.fillModeNonSolid            != VK_TRUE) return false;
	if (xCore.drawIndirectFirstInstance   != VK_TRUE) return false; // terrain minimum
	if (xShaderDrawParameters.shaderDrawParameters != VK_TRUE) return false; // terrain minimum
	// multiDrawIndirect is OPTIONAL — the PADDED_SINGLE tier is the legal
	// fallback for devices that lack it. Do NOT reject such adapters here.

	// --- Descriptor-indexing features (engine force-enables them in CreateDevice) ---
	if (xDescriptorIndexing.descriptorBindingSampledImageUpdateAfterBind != VK_TRUE) return false;
	if (xDescriptorIndexing.descriptorBindingPartiallyBound               != VK_TRUE) return false;
	if (xDescriptorIndexing.runtimeDescriptorArray                        != VK_TRUE) return false;
	if (xDescriptorIndexing.shaderSampledImageArrayNonUniformIndexing      != VK_TRUE) return false;
#ifdef ZENITH_RAYTRACING
	if (xBufferDeviceAddress.bufferDeviceAddress != VK_TRUE) return false;
	if (xAccelerationStructure.accelerationStructure != VK_TRUE) return false;
	if (xRayTracingPipeline.rayTracingPipeline != VK_TRUE) return false;
	if (xRayQuery.rayQuery != VK_TRUE) return false;
#endif

	// --- Queue family: run the SAME shared selection routine that
	// CreateQueueFamilies will use, so suitability and creation agree
	// exactly. Returns false if any queue type is unresolvable (the shared
	// routine applies Vulkan's implicit graphics-transfer capability).
	{
		uint32_t auIndices[COMMANDTYPE_MAX] = {};
		if (!s_bResolveQueueFamilies(xDevice, xSurface, auIndices))
			return false;
	}

	return true;
}

// Prefer discrete GPUs, then integrated, then anything else (matches the
// "score suitable adapters normally" rule of the plan: a faster class beats a
// slower class only when both are suitable — an unsuitable discrete is never
// picked over a suitable integrated).
static int32_t s_iScorePhysicalDeviceClass(const vk::PhysicalDeviceProperties& xProps)
{
	switch (xProps.deviceType)
	{
	case vk::PhysicalDeviceType::eDiscreteGpu:   return 3;
	case vk::PhysicalDeviceType::eIntegratedGpu: return 2;
	case vk::PhysicalDeviceType::eVirtualGpu:    return 1;
	default:                                     return 0;
	}
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

	// Score every adapter that satisfies the HARD requirement check, then pick
	// the highest-scoring one. Previously this loop picked the first adapter
	// unconditionally, so CreateDevice's `setMultiDrawIndirect(VK_TRUE)` etc.
	// could enable a feature the device does not advertise — vkCreateDevice then
	// returns VK_ERROR_FEATURE_NOT_PRESENT, or worse, silently downgrades state
	// on a broken ICD. A truly-unsuitable adapter set now fails cleanly here,
	// before vkCreateDevice, naming the failure mode rather than crashing later.
	const vk::PhysicalDevice* pxPicked = nullptr;
	int32_t iBestScore = -1;
	uint32_t uUnsuitableCount = 0;
	for (const vk::PhysicalDevice& xDevice : xDevices)
	{
		if (!s_bIsPhysicalDeviceHardSuitable(xDevice, m_xSurface))
		{
			++uUnsuitableCount;
			const vk::PhysicalDeviceProperties xProps = xDevice.getProperties();
			Zenith_Warning(LOG_CATEGORY_VULKAN,
				"Skipping unsuitable adapter '%s': missing a hard renderer requirement "
				"(samplerAnisotropy, tessellationShader, depthBiasClamp, fillModeNonSolid, "
				"drawIndirectFirstInstance, shaderDrawParameters, descriptor-indexing features, "
				"Vulkan 1.1+, build-required device extensions/features, a graphics+compute queue, "
				"or surface presentation support). "
				"multiDrawIndirect and drawIndirectCount are optional (padded fallback).",
				xProps.deviceName);
			continue;
		}

		const vk::PhysicalDeviceProperties xProps = xDevice.getProperties();
		const int32_t iScore = s_iScorePhysicalDeviceClass(xProps);
		if (iScore > iBestScore)
		{
			iBestScore = iScore;
			pxPicked = &xDevice;
		}
	}

	if (pxPicked == nullptr)
	{
		// Fail cleanly before vkCreateDevice: name the failure mode rather than
		// crash deep in driver init. The plan explicitly forbids retaining the
		// old "pick first, force unsupported feature bits" behaviour.
		Zenith_Assert(false,
			"No physical device satisfies the engine's hard renderer requirements "
			"(samplerAnisotropy, tessellationShader, depthBiasClamp, fillModeNonSolid, "
			"drawIndirectFirstInstance, shaderDrawParameters, descriptor-indexing features, "
			"Vulkan 1.1+, build-required device extensions/features, a graphics+compute queue, "
			"or surface presentation support). "
			"%u adapter(s) were enumerated and skipped. multiDrawIndirect and "
			"drawIndirectCount are NOT hard requirements — the padded fallback tier "
			"handles their absence. An adapter that only misses those is still suitable.",
			uUnsuitableCount);
		return;
	}
	m_xPhysicalDevice = *pxPicked;

	const vk::PhysicalDeviceProperties& xProps = m_xPhysicalDevice.getProperties();
	m_xGPUCapabilities.m_uMaxTextureWidth = xProps.limits.maxImageDimension2D;
	m_xGPUCapabilities.m_uMaxTextureHeight = xProps.limits.maxImageDimension2D;
	m_xGPUCapabilities.m_uMaxFramebufferWidth = xProps.limits.maxFramebufferWidth;
	m_xGPUCapabilities.m_uMaxFramebufferHeight = xProps.limits.maxFramebufferHeight;

#ifdef ZENITH_FLUX_PROFILING
	// GPU profiler: nanoseconds-per-tick for timestamp queries. A device that
	// reports 0 here can't produce meaningful timestamps; CreateQueueFamilies
	// completes the support decision with the graphics queue's validBits.
	m_fTimestampPeriod = xProps.limits.timestampPeriod;
	Zenith_Log(LOG_CATEGORY_VULKAN, "GPU timestamp period: %f ns/tick", m_fTimestampPeriod);
#endif

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

void Zenith_Vulkan::AssertVertexFetchFormatSupport()
{
	// Every ShaderDataType that ShaderDataTypeToVulkanFormat maps is reachable as a
	// vertex-input attribute format, so the device has to advertise
	// VERTEX_BUFFER_BIT for it in bufferFeatures. Checked once at boot over the
	// WHOLE vocabulary rather than per-pipeline: an unsupported packed format
	// otherwise surfaces as an opaque driver failure in whichever pipeline first
	// adopts it, arbitrarily far from the vertex layout that chose the format.
	// The four non-fetch tags (MAT3 / MAT4 / BOOL / NONE) have no format at all --
	// ShaderDataTypeToVulkanFormat asserts on them -- so they are skipped here.
	u_int uUnsupported = 0;
	for (u_int uType = 0; uType <= static_cast<u_int>(SHADER_DATA_TYPE_UINT16X4); uType++)
	{
		const ShaderDataType eType = static_cast<ShaderDataType>(uType);
		if (eType == SHADER_DATA_TYPE_MAT3 || eType == SHADER_DATA_TYPE_MAT4
			|| eType == SHADER_DATA_TYPE_BOOL || eType == SHADER_DATA_TYPE_NONE)
		{
			continue;
		}

		const vk::Format eFormat = ShaderDataTypeToVulkanFormat(eType);
		const vk::FormatProperties xProps = m_xPhysicalDevice.getFormatProperties(eFormat);
		if ((xProps.bufferFeatures & vk::FormatFeatureFlagBits::eVertexBuffer) != vk::FormatFeatureFlags{})
		{
			continue;
		}

		uUnsupported++;
		Zenith_Assert(false,
			"ShaderDataType %u (VkFormat %u) cannot be fetched as a vertex attribute on this device (bufferFeatures 0x%x)",
			uType, static_cast<uint32_t>(eFormat), static_cast<uint32_t>(xProps.bufferFeatures));
	}
	Zenith_Log(LOG_CATEGORY_VULKAN, "Vertex-fetch format support: %u unsupported of the mapped ShaderDataType vocabulary", uUnsupported);
}

void Zenith_Vulkan::CreateQueueFamilies()
{
	// Delegate to the shared selection routine so the suitability check
	// (CreatePhysicalDevice) and actual assignment agree exactly —
	// including the family-reuse fallbacks (COMPUTE/COPY/Present) that
	// the old per-iteration picker couldn't reach. The suitability check
	// has already verified this will succeed (that's its job), so the
	// assert at the bottom is a pure safety check.
	if (!s_bResolveQueueFamilies(m_xPhysicalDevice, m_xSurface, m_auQueueIndices))
	{
		for (uint32_t u = 0; u < COMMANDTYPE_MAX; u++)
		{
			Zenith_Assert(m_auQueueIndices[u] != UINT32_MAX,
				"CreateQueueFamilies: failed to resolve queue index %u (the shared "
				"selection routine returned false — the physical-device suitability "
				"check should have rejected this adapter before CreateDevice).", u);
		}
		return;
	}

	std::vector<vk::QueueFamilyProperties> xQueueFamilyProperties = m_xPhysicalDevice.getQueueFamilyProperties();

	// The shared routine already populated m_auQueueIndices; the old per-
	// family loop is gone. The assert below is a pure safety check.

#ifdef ZENITH_FLUX_PROFILING
	// GPU profiler: the graphics queue family must support timestamp writes
	// (timestampValidBits > 0) and the device must report a non-zero period.
	// Both true => GPU per-pass profiling is live; otherwise it's a clean no-op.
	m_uTimestampValidBits = xQueueFamilyProperties[m_auQueueIndices[COMMANDTYPE_GRAPHICS]].timestampValidBits;
	m_bGPUTimestampsSupported = (m_uTimestampValidBits > 0) && (m_fTimestampPeriod > 0.0f);
	Zenith_Log(LOG_CATEGORY_VULKAN, "GPU timestamp profiling: %s (validBits=%u)",
		m_bGPUTimestampsSupported ? "ENABLED" : "disabled", m_uTimestampValidBits);
#endif

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


	const vk::PhysicalDeviceProperties& xDeviceProps = m_xPhysicalDevice.getProperties();
	std::vector<const char*> xEnabledExtensions(s_aszDeviceExtensions, s_aszDeviceExtensions + COUNT_OF(s_aszDeviceExtensions));
	if (xDeviceProps.apiVersion < VK_API_VERSION_1_2)
	{
		xEnabledExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
	}
#ifdef ZENITH_RAYTRACING
	if (xDeviceProps.apiVersion < VK_API_VERSION_1_2)
	{
		xEnabledExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
		xEnabledExtensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
		xEnabledExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
	}
#endif

	const bool bDrawIndirectCountExtensionAdvertised =
		IsDeviceExtensionSupported(m_xPhysicalDevice, VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME);

	// Prefer the Vulkan 1.2 core route when its feature is available, otherwise
	// use the KHR extension route. Merely resolving either proc address is not a
	// capability test; the selected route must also be advertised and enabled.
	vk::PhysicalDeviceVulkan12Features xAvailableVulkan12Features;
	if (xDeviceProps.apiVersion >= VK_API_VERSION_1_2)
	{
		vk::PhysicalDeviceFeatures2 xAvailableFeatures;
		xAvailableFeatures.setPNext(&xAvailableVulkan12Features);
		m_xPhysicalDevice.getFeatures2(&xAvailableFeatures);
	}
	const bool bDrawIndirectCountCoreAdvertised =
		xDeviceProps.apiVersion >= VK_API_VERSION_1_2 &&
		xAvailableVulkan12Features.drawIndirectCount == VK_TRUE;
	const Zenith_Vulkan_IndirectCountSelection xCountSelection =
		Zenith_Vulkan_SelectIndirectCountRoute(
			xDeviceProps.apiVersion >= VK_API_VERSION_1_2,
			bDrawIndirectCountCoreAdvertised,
			bDrawIndirectCountExtensionAdvertised);

	if (xCountSelection.m_bEnableKHRExtension)
	{
		xEnabledExtensions.push_back(VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME);
	}

	vk::DeviceCreateInfo xDeviceCreateInfo = vk::DeviceCreateInfo()
		.setPQueueCreateInfos(xQueueInfos.data())
		.setQueueCreateInfoCount(static_cast<uint32_t>(xQueueInfos.size()))
		.setEnabledExtensionCount(static_cast<uint32_t>(xEnabledExtensions.size()))
		.setPpEnabledExtensionNames(xEnabledExtensions.data())
		.setEnabledLayerCount(0);

	// Advertised feature query: the engine force-enables a baseline set in the
	// chain below; the hard-suitability check in CreatePhysicalDevice guarantees
	// each of these is advertised, but we read the advertised bits defensively
	// so a broken ICD that claims a feature in enumerate but rejects it at
	// vkCreateDevice is logged with a clear failure mode rather than silently
	// downgrading usable state. The plan forbids silently forcing unsupported
	// feature bits to VK_TRUE — and these are all advertised, so VK_TRUE is the
	// honest request, but the cap record is populated from the post-create
	// resolved entry point (the only authority for "usable").
	vk::PhysicalDeviceFeatures2 xAdvertisedFeatures;
	vk::PhysicalDeviceShaderDrawParameterFeatures xAdvertisedShaderDrawParameters;
	xAdvertisedFeatures.setPNext(&xAdvertisedShaderDrawParameters);
	m_xPhysicalDevice.getFeatures2(&xAdvertisedFeatures);
	const bool bAdvertMultiDrawIndirect     = (xAdvertisedFeatures.features.multiDrawIndirect           == VK_TRUE);
	const bool bAdvertIndirectFirstInstance = (xAdvertisedFeatures.features.drawIndirectFirstInstance   == VK_TRUE);
	const bool bAdvertShaderDrawParameters  = (xAdvertisedShaderDrawParameters.shaderDrawParameters    == VK_TRUE);

	// Enable every advertised bit. multiDrawIndirect is the engine's MULTI-
	// DRAW path; drawIndirectFirstInstance and shaderDrawParameters are the
	// documented remaining terrain minimum. None of these gets force-enabled
	// if unsuitable (CreatePhysicalDevice already rejected such adapters as
	// hard-misses, so all three are advertised on the picked adapter — but the
	// defensive clamp keeps the failure localised on a broken ICD).
	vk::PhysicalDeviceFeatures xDeviceFeatures = vk::PhysicalDeviceFeatures()
		.setSamplerAnisotropy(VK_TRUE)
		.setTessellationShader(VK_TRUE)
		.setDepthBiasClamp(VK_TRUE)
		.setMultiDrawIndirect(bAdvertMultiDrawIndirect ? VK_TRUE : VK_FALSE)
		// Required so the terrain compute shader can write a non-zero
		// firstInstance to its indirect draw commands. firstInstance carries
		// the stable per-chunk index that the terrain vertex shader reads via
		// SV_StartInstanceLocation to look up LODLevelBuffer[chunkIndex] —
		// without this feature, any non-zero firstInstance in an indirect
		// draw is undefined behaviour (manifests as holes / device-lost on
		// chunks past the first). Hard-suitability already guarantees this is
		// advertised; a broken-ICD downgrade is logged but the feature request
		// stays honest rather than forcing VK_TRUE on a device that does not
		// advertise it.
		.setDrawIndirectFirstInstance(bAdvertIndirectFirstInstance ? VK_TRUE : VK_FALSE)
		.setFillModeNonSolid(VK_TRUE);


	vk::PhysicalDeviceFeatures2 xDeviceFeatures2 = vk::PhysicalDeviceFeatures2()
		.setFeatures(xDeviceFeatures);

	vk::PhysicalDeviceShaderDrawParameterFeatures xShaderDrawFeatures = vk::PhysicalDeviceShaderDrawParameterFeatures()
	.setShaderDrawParameters(bAdvertShaderDrawParameters ? VK_TRUE : VK_FALSE)
		.setPNext(&xDeviceFeatures2);

	vk::PhysicalDeviceDescriptorIndexingFeatures xIndexingFeatures = vk::PhysicalDeviceDescriptorIndexingFeatures()
		.setDescriptorBindingSampledImageUpdateAfterBind(true)
		.setDescriptorBindingPartiallyBound(true)
		.setRuntimeDescriptorArray(true)
		// Required so per-pixel/per-instance bindless indices (terrain splatmap,
		// Quads/UI) are legal: the index into g_axTextures[] is non-uniform
		// across a wave. Combined with runtimeDescriptorArray this is the
		// full bindless-table negotiation. (Hard min-spec; the Android
		// capability gate + table clamp land in the hardening phase.)
		.setShaderSampledImageArrayNonUniformIndexing(true)
		.setPNext(&xShaderDrawFeatures);

	// VkPhysicalDeviceVulkan12Features and the promoted descriptor-indexing
	// feature struct must not coexist in one device-create chain. On the core
	// draw-indirect-count path, enable both that feature and the four existing
	// bindless requirements through the Vulkan 1.2 aggregate instead.
	vk::PhysicalDeviceVulkan12Features xVulkan12Features = vk::PhysicalDeviceVulkan12Features()
		.setDrawIndirectCount(xCountSelection.m_bEnableCoreFeature ? VK_TRUE : VK_FALSE)
		.setDescriptorBindingSampledImageUpdateAfterBind(true)
		.setDescriptorBindingPartiallyBound(true)
		.setRuntimeDescriptorArray(true)
		.setShaderSampledImageArrayNonUniformIndexing(true)
		.setPNext(&xShaderDrawFeatures);
#ifdef ZENITH_RAYTRACING
	if (xCountSelection.m_bEnableCoreFeature)
	{
		xVulkan12Features.setBufferDeviceAddress(VK_TRUE);
	}
#endif
	void* pFeatureChain = xCountSelection.m_bEnableCoreFeature
		? static_cast<void*>(&xVulkan12Features)
		: static_cast<void*>(&xIndexingFeatures);

#ifdef ZENITH_RAYTRACING
	vk::PhysicalDeviceBufferDeviceAddressFeatures xBufferDeviceAddress;
	vk::PhysicalDeviceAccelerationStructureFeaturesKHR xAccelerationStructure;
	vk::PhysicalDeviceRayTracingPipelineFeaturesKHR xRayTracingPipeline;
	vk::PhysicalDeviceRayQueryFeaturesKHR xRayQuery;
	xAccelerationStructure.setAccelerationStructure(VK_TRUE).setPNext(&xRayTracingPipeline);
	xRayTracingPipeline.setRayTracingPipeline(VK_TRUE).setPNext(&xRayQuery);
	xRayQuery.setRayQuery(VK_TRUE).setPNext(pFeatureChain);
	if (xCountSelection.m_bEnableCoreFeature)
	{
		// VkPhysicalDeviceVulkan12Features already carries bufferDeviceAddress;
		// do not duplicate its promoted feature struct in the same pNext chain.
		pFeatureChain = &xAccelerationStructure;
	}
	else
	{
		xBufferDeviceAddress.setBufferDeviceAddress(VK_TRUE).setPNext(&xAccelerationStructure);
		pFeatureChain = &xBufferDeviceAddress;
	}
#endif
	xDeviceCreateInfo.setPNext(pFeatureChain);

	m_xDevice = VkUnwrap(m_xPhysicalDevice.createDevice(xDeviceCreateInfo));

	for (uint32_t i = 0; i < COMMANDTYPE_MAX; i++)
	{
		m_axQueues[i] = m_xDevice.getQueue(m_auQueueIndices[i], 0);
	}

	// Resolve exactly the alias whose capability was enabled above. A null result
	// remains a defensive final guard for broken ICDs, but it never promotes an
	// unsupported device to supported status.
	m_pfnDrawIndexedIndirectCount = nullptr;
	bool bCoreCountProcResolved = false;
	bool bKHRCountProcResolved = false;
	if (xCountSelection.m_eRoute == Zenith_Vulkan_IndirectCountRoute::KHR_EXTENSION)
	{
		m_pfnDrawIndexedIndirectCount = reinterpret_cast<PFN_vkCmdDrawIndexedIndirectCount>(
			vkGetDeviceProcAddr(static_cast<VkDevice>(m_xDevice), "vkCmdDrawIndexedIndirectCountKHR"));
		bKHRCountProcResolved = m_pfnDrawIndexedIndirectCount != nullptr;
	}
	else if (xCountSelection.m_eRoute == Zenith_Vulkan_IndirectCountRoute::CORE_1_2)
	{
		m_pfnDrawIndexedIndirectCount = reinterpret_cast<PFN_vkCmdDrawIndexedIndirectCount>(
			vkGetDeviceProcAddr(static_cast<VkDevice>(m_xDevice), "vkCmdDrawIndexedIndirectCount"));
		bCoreCountProcResolved = m_pfnDrawIndexedIndirectCount != nullptr;
	}
	m_bDrawIndirectCountSupported = Zenith_Vulkan_IsIndirectCountRouteUsable(
		xCountSelection.m_eRoute, bCoreCountProcResolved, bKHRCountProcResolved);

	// Advertised vs enabled vs usable state are distinct. The advertised bits
	// were queried pre-create; the enabled bits are what CreateDevice requested
	// (defensively clamped to advertised); the usable bits are what resolved
	// after device creation (advertised && enabled && entry point resolved for
	// the count path). The cap struct reports the USABLE semantic booleans plus
	// the raw physical-device maxDrawIndirectCount; fixed callers apply their
	// multi-draw clamp separately. The recorder reads these plus the boot-time
	// override to select an effective mode per counted-indirect request.
	const bool bUsableMultiDrawIndirect     = bAdvertMultiDrawIndirect;     // enable mirrors advertised
	const bool bUsableIndirectFirstInstance = bAdvertIndirectFirstInstance; // enable mirrors advertised
	const bool bUsableShaderDrawParameters  = bAdvertShaderDrawParameters;  // enable mirrors advertised
	const uint32_t uRawMaxDrawIndirectCount = xDeviceProps.limits.maxDrawIndirectCount;
	m_xIndirectDrawCaps = Flux_IndirectDrawCapabilities{
		m_bDrawIndirectCountSupported,
		bUsableMultiDrawIndirect,
		bUsableIndirectFirstInstance,
		bUsableShaderDrawParameters,
		uRawMaxDrawIndirectCount,
	};

	// Boot-time CLI override: parsed by Zenith_CommandLine (Core, no Flux deps)
	// and converted here at device init into the Flux enum. Auto is the
	// shipping default; native/padded/single are test assertions that fail
	// closed at the recorder when their tier cannot legally run. The override
	// never falsifies m_xIndirectDrawCaps.
	// Android never calls Zenith_CommandLine::Parse, so GetIndirectCountMode
	// returns Auto there — the device's raw capability selects the effective
	// mode. The override is immutable after this point: worker recording never
	// mutates it.
	switch (Zenith_CommandLine::GetIndirectCountMode())
	{
	case Zenith_IndirectCountMode::Native: m_eIndirectDrawOverride = Flux_IndirectDrawOverride::NATIVE; break;
	case Zenith_IndirectCountMode::Padded: m_eIndirectDrawOverride = Flux_IndirectDrawOverride::PADDED; break;
	case Zenith_IndirectCountMode::Single: m_eIndirectDrawOverride = Flux_IndirectDrawOverride::SINGLE; break;
	case Zenith_IndirectCountMode::Auto:   m_eIndirectDrawOverride = Flux_IndirectDrawOverride::AUTO;  break;
	}

	// Truthful capability log. The retired "terrain streaming will be
	// disabled" wording was inaccurate: only the counted draw is skipped, and
	// the padded fallback tier (which the recorder will now use) renders
	// terrain on every sufficient device. Logs show raw / enabled / usable
	// state where they differ; the override and the effective fallback are
	// recorded separately so a CI log unambiguously names the mode.
	Zenith_Log(LOG_CATEGORY_VULKAN, "vkCmdDrawIndexedIndirectCount: advertised(ext=%s core1.2=%s) enabled=%s usable=%s (driver apiVersion %u.%u.%u)",
		bDrawIndirectCountExtensionAdvertised ? "yes" : "no",
		bDrawIndirectCountCoreAdvertised      ? "yes" : "no",
		(xCountSelection.m_eRoute != Zenith_Vulkan_IndirectCountRoute::NONE) ? "yes" : "no",
		m_bDrawIndirectCountSupported ? "yes" : "NO (terrain draws via padded fallback)",
		VK_API_VERSION_MAJOR(xDeviceProps.apiVersion), VK_API_VERSION_MINOR(xDeviceProps.apiVersion), VK_API_VERSION_PATCH(xDeviceProps.apiVersion));
	Zenith_Log(LOG_CATEGORY_VULKAN, "  multiDrawIndirect: raw=%s usable=%s | drawIndirectFirstInstance: raw=%s usable=%s | shaderDrawParameters: raw=%s usable=%s",
		bAdvertMultiDrawIndirect     ? "yes" : "no", bUsableMultiDrawIndirect     ? "yes" : "no",
		bAdvertIndirectFirstInstance ? "yes" : "no", bUsableIndirectFirstInstance ? "yes" : "no",
		bAdvertShaderDrawParameters  ? "yes" : "no", bUsableShaderDrawParameters  ? "yes" : "no");
	Zenith_Log(LOG_CATEGORY_VULKAN,
		"  maxDrawIndirectCount: raw/native=%u fixedPerCall=%u (fixed clamps to 1 when multi-draw off)",
		uRawMaxDrawIndirectCount, Flux_ResolveFixedDrawPerCallLimit(m_xIndirectDrawCaps));
	{
		const char* szOverrideName = "auto";
		switch (m_eIndirectDrawOverride)
		{
		case Flux_IndirectDrawOverride::NATIVE: szOverrideName = "native"; break;
		case Flux_IndirectDrawOverride::PADDED: szOverrideName = "padded"; break;
		case Flux_IndirectDrawOverride::SINGLE: szOverrideName = "single"; break;
		case Flux_IndirectDrawOverride::AUTO:   szOverrideName = "auto";   break;
		}
		Zenith_Log(LOG_CATEGORY_VULKAN, "  --indirect-count-mode: %s (effective fallback negotiated per-request by the recorder)", szOverrideName);
	}

	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan device created");
}

void Zenith_Vulkan::CreateCommandPools()
{
	// A command pool is only meaningful on a queue family that can OWN command
	// buffers. Presentation is not a command-buffer capability: vkQueuePresentKHR
	// takes no command buffer, and a WSI-only family selected purely because
	// vkGetPhysicalDeviceSurfaceSupportKHR returned true may expose none of
	// graphics/compute/transfer. Creating a pool against such a family is at best
	// useless and at worst invalid, so PRESENT is skipped whenever it resolved to
	// a family of its own. Nothing asks for it: GetCommandPool is only ever
	// called with GRAPHICS (recorders, staging, screenshot) and COPY (the memory
	// manager's internal buffer), and COMPUTE is a real command-capable family.
	// A present slot that shares the graphics family keeps that family's pool so
	// the array stays fully populated in the common single-family case.
	//
	// The decision itself lives in the pure Zenith_Vulkan_PlanCommandPools seam
	// so it is unit-testable: a development machine whose present and graphics
	// families coincide can never execute the skip branch at runtime.
	static_assert(COMMANDTYPE_GRAPHICS == 0 && COMMANDTYPE_COMPUTE == 1 &&
		COMMANDTYPE_COPY == 2 && COMMANDTYPE_PRESENT == 3 && COMMANDTYPE_MAX == 4,
		"CreateCommandPools maps the plan's four flags onto CommandType by position");
	const Zenith_Vulkan_CommandPoolPlan xPoolPlan = Zenith_Vulkan_PlanCommandPools(
		m_auQueueIndices[COMMANDTYPE_GRAPHICS],
		m_auQueueIndices[COMMANDTYPE_COMPUTE],
		m_auQueueIndices[COMMANDTYPE_COPY],
		m_auQueueIndices[COMMANDTYPE_PRESENT]);
	const bool abWantsPool[COMMANDTYPE_MAX] = {
		xPoolPlan.m_bGraphics, xPoolPlan.m_bCompute, xPoolPlan.m_bCopy, xPoolPlan.m_bPresent };

	for (uint32_t i = 0; i < COMMANDTYPE_MAX; i++)
	{
		if (!abWantsPool[i])
		{
			// In practice only PRESENT reaches this branch, and only when it
			// resolved to a family of its own — the other three slots always
			// resolve to command-capable families (the suitability check
			// rejects an adapter where they cannot).
			Zenith_Log(LOG_CATEGORY_VULKAN,
				"  command type %u (queue family %u) owns no command pool%s",
				i, m_auQueueIndices[i],
				i == COMMANDTYPE_PRESENT
					? " — presentation records no commands and this is a separate present family"
					: "");
			continue;
		}
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

void Zenith_Vulkan::QueryDescriptorIndexingLimits()
{
	// The bindless table is an update-after-bind combined-image-sampler array, so it
	// is bounded by the device's update-after-bind limits — both the per-set and
	// per-stage sampled-image AND sampler ceilings (a combined-image-sampler counts
	// against both), plus the all-pools cap. Clamp the target to the tightest of them.
	vk::PhysicalDeviceDescriptorIndexingProperties xIndexingProps;
	vk::PhysicalDeviceProperties2 xProps2;
	xProps2.pNext = &xIndexingProps;
	m_xPhysicalDevice.getProperties2(&xProps2);

	uint32_t uDeviceLimit = xIndexingProps.maxDescriptorSetUpdateAfterBindSampledImages;
	uDeviceLimit = std::min(uDeviceLimit, xIndexingProps.maxPerStageDescriptorUpdateAfterBindSampledImages);
	uDeviceLimit = std::min(uDeviceLimit, xIndexingProps.maxDescriptorSetUpdateAfterBindSamplers);
	uDeviceLimit = std::min(uDeviceLimit, xIndexingProps.maxPerStageDescriptorUpdateAfterBindSamplers);
	uDeviceLimit = std::min(uDeviceLimit, xIndexingProps.maxUpdateAfterBindDescriptorsInAllPools);

	m_uBindlessTableSize = std::min<uint32_t>(FLUX_BINDLESS_TABLE_SIZE_TARGET, uDeviceLimit);

	// Min-spec gate: a device that cannot host the legacy floor is rejected outright
	// (the renderer has no non-bindless fallback path).
	Zenith_Assert(m_uBindlessTableSize >= FLUX_BINDLESS_TABLE_SIZE_MIN,
		"Device bindless update-after-bind limit (%u) is below the min-spec floor (%u) — unsupported GPU",
		uDeviceLimit, FLUX_BINDLESS_TABLE_SIZE_MIN);

	Zenith_Log(LOG_CATEGORY_VULKAN, "Bindless table size: %u (target %u, device limit %u)",
		m_uBindlessTableSize, FLUX_BINDLESS_TABLE_SIZE_TARGET, uDeviceLimit);
}

void Zenith_Vulkan::CreateBindlessTexturesDescriptorPool()
{
	vk::DescriptorPoolSize axPoolSizes[] =
	{
		{ vk::DescriptorType::eCombinedImageSampler, m_uBindlessTableSize },
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
		.setDescriptorCount(m_uBindlessTableSize)
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

void Zenith_Vulkan::CreatePersistentDescriptorSets()
{
	// Pool sized for the GLOBAL set + one VIEW set PER RENDER-VIEW SLOT across all
	// frames in flight. The sets are written BEFORE bind each frame
	// (PreparePersistentSets, ahead of worker recording), so no update-after-bind is
	// needed — a plain pool/layout keeps them simple.
	// Per frame: 1× GLOBAL { g_xGlobal (uniform) + g_axMaterials (storage) } +
	// FLUX_MAX_RENDER_VIEWS× VIEW { g_xView (uniform) + g_xCSM + IBL trio (4 combined
	// image samplers) + g_xShadowMatrices + light/cluster buffers (4 storage) }.
	// Bump the per-set member counts in lockstep when a VIEW/GLOBAL member is added.
	constexpr u_int kuViewUniformPerSet  = 1u; // g_xView
	constexpr u_int kuViewStoragePerSet  = 4u; // g_xShadowMatrices + g_xLightBuffer + cluster counts/indices
	constexpr u_int kuViewCombinedPerSet = 4u; // g_xCSM + IBL trio
	vk::DescriptorPoolSize axPoolSizes[] =
	{
		{ vk::DescriptorType::eUniformBuffer,        MAX_FRAMES_IN_FLIGHT * (1u + FLUX_MAX_RENDER_VIEWS * kuViewUniformPerSet) },
		{ vk::DescriptorType::eStorageBuffer,        MAX_FRAMES_IN_FLIGHT * (1u + FLUX_MAX_RENDER_VIEWS * kuViewStoragePerSet) },
		{ vk::DescriptorType::eCombinedImageSampler, MAX_FRAMES_IN_FLIGHT * (FLUX_MAX_RENDER_VIEWS * kuViewCombinedPerSet) },
	};
	vk::DescriptorPoolCreateInfo xPoolInfo = vk::DescriptorPoolCreateInfo()
		.setPoolSizeCount(COUNT_OF(axPoolSizes))
		.setPPoolSizes(axPoolSizes)
		.setMaxSets(MAX_FRAMES_IN_FLIGHT * (1u + FLUX_MAX_RENDER_VIEWS));
	m_xPersistentDescriptorPool = VkUnwrap(m_xDevice.createDescriptorPool(xPoolInfo));

	// Layouts must match exactly what the spine reflects so every pipeline's RootSig can
	// borrow them (RootSigBuilder::FromSpecification, GLOBAL/VIEW classes):
	//   GLOBAL = { b0: uniform (g_xGlobal), b1: storage (g_axMaterials, Phase 5.3) }
	//   VIEW   = { b0: uniform (g_xView), b1: combined image sampler (g_xCSM),
	//             b2: storage (g_xShadowMatrices), b3-5: storage (g_xLightBuffer +
	//             g_xClusterLightCounts + g_xClusterLightIndices), b6-8: combined image
	//             samplers (IBL BRDF LUT + irradiance/prefiltered), all Phase 5.4 }
	{
		vk::DescriptorSetLayoutBinding axBinds[2];
		axBinds[0] = vk::DescriptorSetLayoutBinding().setBinding(0)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eAll);
		axBinds[1] = vk::DescriptorSetLayoutBinding().setBinding(1)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer).setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eAll);
		vk::DescriptorSetLayoutCreateInfo xInfo = vk::DescriptorSetLayoutCreateInfo().setBindingCount(2).setPBindings(axBinds);
		m_xGlobalSetLayout = VkUnwrap(m_xDevice.createDescriptorSetLayout(xInfo));
	}
	{
		// VIEW = { b0: uniform (g_xView), b1: combined image sampler (g_xCSM), b2: storage
		// (g_xShadowMatrices), b3-5: storage (g_xLightBuffer + g_xClusterLightCounts +
		// g_xClusterLightIndices), b6-8: combined image samplers (IBL BRDF LUT +
		// irradiance/prefiltered) — all Phase 5.4 }. Grows in lockstep with the ViewParams
		// block in Common/Bindings.slang and the canonical check in
		// Flux_PersistentSetLayouts::ValidateCanonicalGroup.
		vk::DescriptorSetLayoutBinding axBinds[Flux_PersistentSetLayouts::kuViewBindingCount];
		axBinds[Flux_PersistentSetLayouts::kuViewBinding_View] = vk::DescriptorSetLayoutBinding()
			.setBinding(Flux_PersistentSetLayouts::kuViewBinding_View)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eAll);
		axBinds[Flux_PersistentSetLayouts::kuViewBinding_CSM] = vk::DescriptorSetLayoutBinding()
			.setBinding(Flux_PersistentSetLayouts::kuViewBinding_CSM)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eAll);
		axBinds[Flux_PersistentSetLayouts::kuViewBinding_ShadowMatrices] = vk::DescriptorSetLayoutBinding()
			.setBinding(Flux_PersistentSetLayouts::kuViewBinding_ShadowMatrices)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer).setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eAll);
		// Bindings 3-5 (Phase 5.4): clustered-lighting read buffers — all single storage buffers.
		for (u_int uB = Flux_PersistentSetLayouts::kuViewBinding_LightBuffer; uB <= Flux_PersistentSetLayouts::kuViewBinding_ClusterLightIndices; uB++)
		{
			axBinds[uB] = vk::DescriptorSetLayoutBinding().setBinding(uB)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer).setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eAll);
		}
		// Bindings 6-8 (Phase 5.4): the IBL trio — combined image samplers (BRDF LUT 2D + 2 cubes).
		for (u_int uB = Flux_PersistentSetLayouts::kuViewBinding_BRDFLUT; uB <= Flux_PersistentSetLayouts::kuViewBinding_PrefilteredMap; uB++)
		{
			axBinds[uB] = vk::DescriptorSetLayoutBinding().setBinding(uB)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eAll);
		}
		vk::DescriptorSetLayoutCreateInfo xInfo = vk::DescriptorSetLayoutCreateInfo()
			.setBindingCount(Flux_PersistentSetLayouts::kuViewBindingCount).setPBindings(axBinds);
		m_xViewSetLayout = VkUnwrap(m_xDevice.createDescriptorSetLayout(xInfo));
	}

	// One GLOBAL + one VIEW set per render-view slot, per frame in flight
	// (allocated once; never reset). Every slot's set exists up front so a view
	// activating later (preview panel opening) never allocates mid-frame.
	for (u_int u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		vk::DescriptorSetLayout axLayouts[1u + FLUX_MAX_RENDER_VIEWS];
		axLayouts[0] = m_xGlobalSetLayout;
		for (u_int uView = 0; uView < FLUX_MAX_RENDER_VIEWS; uView++) { axLayouts[1u + uView] = m_xViewSetLayout; }
		vk::DescriptorSetAllocateInfo xAlloc = vk::DescriptorSetAllocateInfo()
			.setDescriptorPool(m_xPersistentDescriptorPool)
			.setDescriptorSetCount(1u + FLUX_MAX_RENDER_VIEWS)
			.setPSetLayouts(axLayouts);
		std::vector<vk::DescriptorSet> axSets = VkUnwrap(m_xDevice.allocateDescriptorSets(xAlloc));
		m_axPerFrame[u].m_xGlobalSet = axSets[0];
		for (u_int uView = 0; uView < FLUX_MAX_RENDER_VIEWS; uView++) { m_axPerFrame[u].m_axViewSets[uView] = axSets[1u + uView]; }
	}

	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan persistent GLOBAL/VIEW descriptor sets created (%u frames x %u views)",
		(u_int)MAX_FRAMES_IN_FLIGHT, (u_int)FLUX_MAX_RENDER_VIEWS);
}

void Zenith_Vulkan::PreparePersistentSets(Flux_BufferDescriptorHandle xGlobalCBV, Flux_BufferDescriptorHandle xMaterialsSSBO, const Flux_BufferDescriptorHandle* axViewCBVs, u_int uNumViewCBVs)
{
	// Main thread, once per frame, before any worker records. Rewrites the CURRENT frame
	// slot's GLOBAL (g_xGlobal CB + g_axMaterials SSBO, Phase 5.3) descriptors plus
	// binding 0 (g_xView CB) of EVERY render-view slot's VIEW set — each slot gets its
	// own per-view constants buffer (inactive slots' sets are written but never bound).
	// One set per frame-in-flight → safe to write without a barrier (the slot's prior
	// GPU use already completed via the frame fence in PerFrameBegin).
	Zenith_Assert(m_pxCurrentFrame != nullptr, "PreparePersistentSets: no current frame slot");
	Zenith_Assert(axViewCBVs != nullptr && uNumViewCBVs == FLUX_MAX_RENDER_VIEWS,
		"PreparePersistentSets: expected one view CBV per render-view slot (%u), got %u", FLUX_MAX_RENDER_VIEWS, uNumViewCBVs);

	auto& xEngine = g_xEngine;
	const vk::DescriptorBufferInfo xGlobalInfo    = xEngine.FluxMemory().GetBufferDescriptor(xGlobalCBV);
	const vk::DescriptorBufferInfo xMaterialsInfo = xEngine.FluxMemory().GetBufferDescriptor(xMaterialsSSBO);
	vk::DescriptorBufferInfo axViewInfos[FLUX_MAX_RENDER_VIEWS];
	for (u_int u = 0; u < FLUX_MAX_RENDER_VIEWS; u++)
	{
		axViewInfos[u] = xEngine.FluxMemory().GetBufferDescriptor(axViewCBVs[u]);
	}

	vk::WriteDescriptorSet axWrites[2u + FLUX_MAX_RENDER_VIEWS];
	axWrites[0] = vk::WriteDescriptorSet()
		.setDstSet(m_pxCurrentFrame->m_xGlobalSet).setDstBinding(0).setDstArrayElement(0)
		.setDescriptorCount(1).setDescriptorType(vk::DescriptorType::eUniformBuffer)
		.setPBufferInfo(&xGlobalInfo);
	axWrites[1] = vk::WriteDescriptorSet()
		.setDstSet(m_pxCurrentFrame->m_xGlobalSet).setDstBinding(1).setDstArrayElement(0)
		.setDescriptorCount(1).setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setPBufferInfo(&xMaterialsInfo);
	for (u_int u = 0; u < FLUX_MAX_RENDER_VIEWS; u++)
	{
		axWrites[2u + u] = vk::WriteDescriptorSet()
			.setDstSet(m_pxCurrentFrame->m_axViewSets[u]).setDstBinding(0).setDstArrayElement(0)
			.setDescriptorCount(1).setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setPBufferInfo(&axViewInfos[u]);
	}
	m_xDevice.updateDescriptorSets(2u + FLUX_MAX_RENDER_VIEWS, axWrites, 0, nullptr);
}

void Zenith_Vulkan::WritePersistentViewImage(u_int uBinding, const Flux_ShaderResourceView& xSRV, const Zenith_Vulkan_Sampler& xSampler)
{
	// Main thread, once per frame, after PreparePersistentSets and before any worker
	// records. Writes a VIEW-set combined-image-sampler (e.g. g_xCSM) for the CURRENT
	// frame slot. The image view is stable across the frame; the graph's per-pass
	// Read() declarations drive the layout/contents barriers (so the descriptor write
	// is safe at frame start). The descriptor's imageLayout must match what the graph
	// transitions the resource to for sampling — DEPTH_STENCIL_READ_ONLY for a depth
	// SRV (e.g. the CSM array), SHADER_READ_ONLY otherwise — mirroring the per-pass
	// BindSRV path (BuildDescriptorWritesForSet).
	Zenith_Assert(m_pxCurrentFrame != nullptr, "WritePersistentViewImage: no current frame slot");
	// Fail loud on an invalid view at the write site (the per-pass BindSRV path this
	// replaces asserted this; GetImageView would otherwise silently return VK_NULL_HANDLE
	// and poison the persistently-bound VIEW set for every consumer this frame).
	Zenith_Assert(xSRV.m_xImageViewHandle.IsValid(),
		"WritePersistentViewImage: SRV for VIEW binding %u has a null image view", uBinding);
	const vk::ImageView xVkView = m_pxVulkanMemory->GetImageView(xSRV.m_xImageViewHandle);
	const vk::ImageLayout eLayout = xSRV.m_bIsDepthStencil
		? vk::ImageLayout::eDepthStencilReadOnlyOptimal
		: vk::ImageLayout::eShaderReadOnlyOptimal;
	vk::DescriptorImageInfo xImageInfo = vk::DescriptorImageInfo()
		.setImageLayout(eLayout)
		.setImageView(xVkView)
		.setSampler(xSampler.GetSampler());
	// Replicated-shared across views: fan the SAME descriptor out to every render-
	// view slot's VIEW set (bindings 1-8 are view-invariant; see Flux_ViewSetBinding.h).
	vk::WriteDescriptorSet axWrites[FLUX_MAX_RENDER_VIEWS];
	for (u_int u = 0; u < FLUX_MAX_RENDER_VIEWS; u++)
	{
		axWrites[u] = vk::WriteDescriptorSet()
			.setDstSet(m_pxCurrentFrame->m_axViewSets[u]).setDstBinding(uBinding).setDstArrayElement(0)
			.setDescriptorCount(1).setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setPImageInfo(&xImageInfo);
	}
	m_xDevice.updateDescriptorSets(FLUX_MAX_RENDER_VIEWS, axWrites, 0, nullptr);
}

void Zenith_Vulkan::WritePersistentViewBuffer(u_int uBinding, const Flux_ShaderResourceView_Buffer& xSRV)
{
	// Main thread, once per frame, after PreparePersistentSets. Writes a VIEW-set storage
	// buffer (e.g. the all-cascade ShadowMatrices SSBO) for the CURRENT frame slot. The
	// buffer descriptor is stable across the frame; for a frame-indexed source buffer the
	// accessor yields this frame's view (see the Flux.cpp hook). Mirrors the SSBO write in
	// PreparePersistentSets (eStorageBuffer, setPBufferInfo).
	Zenith_Assert(m_pxCurrentFrame != nullptr, "WritePersistentViewBuffer: no current frame slot");
	Zenith_Assert(xSRV.m_xBufferDescHandle.IsValid(),
		"WritePersistentViewBuffer: SRV for VIEW binding %u has an invalid buffer descriptor", uBinding);
	const vk::DescriptorBufferInfo xInfo = g_xEngine.FluxMemory().GetBufferDescriptor(xSRV.m_xBufferDescHandle);
	// Replicated-shared across views (see WritePersistentViewImage).
	vk::WriteDescriptorSet axWrites[FLUX_MAX_RENDER_VIEWS];
	for (u_int u = 0; u < FLUX_MAX_RENDER_VIEWS; u++)
	{
		axWrites[u] = vk::WriteDescriptorSet()
			.setDstSet(m_pxCurrentFrame->m_axViewSets[u]).setDstBinding(uBinding).setDstArrayElement(0)
			.setDescriptorCount(1).setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setPBufferInfo(&xInfo);
	}
	m_xDevice.updateDescriptorSets(FLUX_MAX_RENDER_VIEWS, axWrites, 0, nullptr);
}

void Zenith_Vulkan::WriteBindlessDescriptor(uint32_t uIndex, vk::ImageView xImageView, vk::Sampler xSampler)
{
	// Today the bindless slot IS the image-view registry handle, which is dense over
	// ALL image views (RTs/depth/UAVs/transients/cubemaps), not just bindless textures.
	// A heavy scene can therefore push the handle past the table — assert rather than
	// silently writing out of range (Phase 3's allocator decouples slot from handle).
	Zenith_Assert(uIndex < m_uBindlessTableSize,
		"Bindless slot %u >= table size %u — bindless table exhausted", uIndex, m_uBindlessTableSize);

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

	// The editor's font set goes into the atlas before the renderer backend is
	// initialised (see Core/Zenith_EditorFontHook.h).
	Zenith_EditorFonts_Load();

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
#if defined(ZENITH_TOOLS) && defined(ZENITH_INPUT_SIMULATOR)
	// Simulated test input must be queued AFTER the GLFW backend's NewFrame
	// (which polls the real OS cursor every focused frame) and BEFORE
	// ImGui::NewFrame consumes the queue - last event wins, so simulated input
	// deterministically overrides the hardware mouse while the simulator is
	// enabled. See Core/Zenith_ImGuiBridgeHook.h.
	if (g_pfnZenithImGuiSimulatedInput)
	{
		g_pfnZenithImGuiSimulatedInput();
	}
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

#ifdef ZENITH_FLUX_PROFILING
	CreateTimestampQueryPool();
#endif
}

#ifdef ZENITH_FLUX_PROFILING
void Zenith_Vulkan_PerFrame::CreateTimestampQueryPool()
{
	Zenith_Vulkan& xVulkan = g_xEngine.FluxBackend();
	if (!xVulkan.IsGPUTimestampsSupported())
	{
		return; // graphics queue can't timestamp — GPU profiling stays a no-op
	}

	vk::QueryPoolCreateInfo xQueryInfo = vk::QueryPoolCreateInfo()
		.setQueryType(vk::QueryType::eTimestamp)
		.setQueryCount(uMAX_GPU_TIMERS * 2);   // a start + end query per timer
	m_xTimestampQueryPool = VkUnwrap(xVulkan.GetDevice().createQueryPool(xQueryInfo));
}

// Claim one timer slot for a pass (called concurrently from recording workers).
// Returns the timer index, or UINT32_MAX if the per-frame budget is exhausted —
// in which case the caller skips its timestamp writes (that pass goes untimed).
u_int Zenith_Vulkan_PerFrame::ClaimGPUTimer(const char* szName, u_int uExecutionIndex)
{
	const u_int uIdx = m_uGPUTimerCount.fetch_add(1, std::memory_order_relaxed);
	if (uIdx >= uMAX_GPU_TIMERS)
	{
		return UINT32_MAX;
	}
	m_aszGPUTimerNames[uIdx] = szName;            // static-lifetime DebugName() literal — pointer is safe
	m_auGPUTimerExecIndex[uIdx] = uExecutionIndex; // for execution-order sort at readback
	return uIdx;
}

// Reset the whole pool to its initial state at the head of worker 0's command
// buffer. Worker 0 is submitted first (EndFrame submits 0..N-1 in order), so on
// the GPU timeline this reset precedes every pass's timestamp writes this frame.
// Must be outside any render pass — it runs right after BeginRecording.
void Zenith_Vulkan_PerFrame::CmdResetGPUTimers(Zenith_Vulkan_CommandBuffer& xCmd)
{
	if (!m_xTimestampQueryPool)
	{
		return;
	}
	xCmd.GetCurrentCmdBuffer().resetQueryPool(m_xTimestampQueryPool, 0, uMAX_GPU_TIMERS * 2);
}

// Deferred read of this slot's previous-frame timestamps (the fence waited in
// BeginFrame guarantees that GPU work is complete), converted to per-pass
// milliseconds and pushed into the CPU profiler's GPU channel. Runs on the main
// thread, so the profiler's GPU lists are touched single-threaded.
void Zenith_Vulkan_PerFrame::ReadbackGPUTimers()
{
	if (!m_xTimestampQueryPool || m_uGPUTimerReadbackCount == 0)
	{
		return;
	}

	// Clamp to the budget: ClaimGPUTimer keeps fetch_add-ing past the cap (only the
	// first uMAX_GPU_TIMERS claims actually wrote queries), so a frame with more
	// passes than the budget must never read past the pool / the stack buffer.
	const u_int uTimers = (m_uGPUTimerReadbackCount < uMAX_GPU_TIMERS) ? m_uGPUTimerReadbackCount : uMAX_GPU_TIMERS;

	// Consume the count: these queries are read exactly once. Zenith_Vulkan::EndFrame
	// rewrites it for the next submission, but an ABANDONED frame (out-of-date acquire
	// -> Swapchain::BeginFrame returns false) never reaches EndFrame and does not
	// advance the ring index, so without this the NEXT frame re-enters the same slot
	// and re-reads the identical, never-reset query pool -- republishing the previous
	// frame's pass timings as if they were new.
	m_uGPUTimerReadbackCount = 0;
	const u_int uQueries = uTimers * 2;
	u_int64 auResults[uMAX_GPU_TIMERS * 2];

	const vk::Device& xDevice = g_xEngine.FluxBackend().GetDevice();
	const vk::Result eResult = xDevice.getQueryPoolResults(
		m_xTimestampQueryPool, 0, uQueries,
		uQueries * sizeof(u_int64), auResults, sizeof(u_int64),
		vk::QueryResultFlagBits::e64);
	// eSuccess = all available (fence guaranteed). Anything else: skip this frame.
	if (eResult != vk::Result::eSuccess)
	{
		return;
	}

	// Order the claimed timer slots by their pass execution index. Slots are claimed
	// in record-race order (whichever worker fetch_add'd first), so without this the
	// report/timeline would shuffle every frame; sorting on the stable topological
	// index presents passes in Flux_RenderGraph execution order.
	u_int auOrder[uMAX_GPU_TIMERS];
	for (u_int i = 0; i < uTimers; i++) auOrder[i] = i;
	std::sort(auOrder, auOrder + uTimers, [this](u_int a, u_int b)
		{ return m_auGPUTimerExecIndex[a] < m_auGPUTimerExecIndex[b]; });

	auto& xEngine = g_xEngine;
	const double fPeriodNs = static_cast<double>(xEngine.FluxBackend().GetTimestampPeriod());
	Zenith_Profiling& xProfiling = xEngine.Profiling();
	xProfiling.BeginGPUCapture();
	for (u_int u = 0; u < uTimers; u++)
	{
		const u_int i = auOrder[u];
		const u_int64 uBegin = auResults[i * 2 + 0];
		const u_int64 uEnd   = auResults[i * 2 + 1];
		// Monotonic within a frame; guard against an unwritten/duplicate pair.
		const double fMs = (uEnd > uBegin) ? (static_cast<double>(uEnd - uBegin) * fPeriodNs * 1e-6) : 0.0;
		xProfiling.AddGPUPass(m_aszGPUTimerNames[i], fMs, m_auGPUTimerExecIndex[i]);
	}
	xProfiling.EndGPUCapture();
}
#endif

void Zenith_Vulkan_PerFrame::InitialisePerFrameResources()
{
	auto& xEngine = g_xEngine;

	// Create scratch buffer for push constant replacement
	Zenith_Vulkan_MemoryManager::PersistentBuffer xScratch = xEngine.FluxMemory().CreatePersistentlyMappedBuffer(
		uSCRATCH_BUFFER_SIZE,
		vk::BufferUsageFlagBits::eUniformBuffer);
	m_xScratchBuffer = xScratch.m_xBuffer;
	m_xScratchAllocation = xScratch.m_xAllocation;
	m_pScratchBufferMapped = xScratch.m_pMappedPtr;

	// Query alignment requirement
	m_uMinAlignment = static_cast<u_int>(xEngine.FluxBackend().GetPhysicalDevice().getProperties().limits.minUniformBufferOffsetAlignment);

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

	xProfiling.BeginProfileZone(ZENITH_PROFILE_ZONE("Vulkan Wait For GPU"));
	const vk::Result eResult = xDevice.waitForFences(1, &m_xFence, VK_TRUE, UINT64_MAX);
	Zenith_Assert(eResult == vk::Result::eSuccess, "Failed to wait for fence");
	xProfiling.EndProfileZone(ZENITH_PROFILE_ZONE("Vulkan Wait For GPU"));

	// NO resetFences here. The reset belongs with the submit that re-signals the
	// fence (Zenith_Vulkan::ResetCurrentInFlightFence, called from
	// Zenith_Vulkan_Swapchain::EndFrame), because a frame can BEGIN and then be
	// ABANDONED without ever submitting: an out-of-date acquire makes
	// Zenith_Vulkan_Swapchain::BeginFrame return false, and Zenith_MainLoop then
	// returns before EndFrameSubmitAndPresent — the only path that signals this
	// fence. Resetting here would leave this slot's fence permanently unsignalled
	// and the next BeginFrame on the same ring slot would block forever in the
	// waitForFences above. Leaving it signalled makes the abandoned-frame wait a
	// no-op, which is exactly right: nothing was submitted, so there is nothing
	// to wait for.

#ifdef ZENITH_FLUX_PROFILING
	// GPU profiler: the fence is now signalled, so this slot's previous-frame
	// timestamps are readable. Read them (→ profiler GPU channel) BEFORE worker 0
	// cmd-resets the pool for the new frame, then reset the claim counter.
	ReadbackGPUTimers();
	m_uGPUTimerCount.store(0, std::memory_order_relaxed);
#endif

	xProfiling.BeginProfileZone(ZENITH_PROFILE_ZONE("Vulkan Reset Descriptor Pools"));
	for (vk::DescriptorPool& xPool : m_axDescriptorPools)
	{
		xDevice.resetDescriptorPool(xPool);
	}
	xProfiling.EndProfileZone(ZENITH_PROFILE_ZONE("Vulkan Reset Descriptor Pools"));
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
	// A failed / not-yet-initialised allocator makes Create*VRAM return invalid
	// (UINT32_MAX) handles, and asset-load paths still propagate them to
	// view-creation / upload sites that read via GetVRAM. Return nullptr instead
	// of asserting so those sites' existing null-pxVRAM guards run.
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
	// BC compressed sRGB formats -- same block payload, EOTF applied on fetch
	case TEXTURE_FORMAT_BC1_RGB_SRGB:
		return vk::Format::eBc1RgbSrgbBlock;
	case TEXTURE_FORMAT_BC1_RGBA_SRGB:
		return vk::Format::eBc1RgbaSrgbBlock;
	case TEXTURE_FORMAT_BC3_RGBA_SRGB:
		return vk::Format::eBc3SrgbBlock;
	case TEXTURE_FORMAT_BC7_RGBA_SRGB:
		return vk::Format::eBc7SrgbBlock;
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
	case SHADER_DATA_TYPE_HALF4:				return vk::Format::eR16G16B16A16Sfloat;
	case SHADER_DATA_TYPE_SNORM16X2:			return vk::Format::eR16G16Snorm;
	case SHADER_DATA_TYPE_SNORM16X4:			return vk::Format::eR16G16B16A16Snorm;
	case SHADER_DATA_TYPE_UNORM16X2:			return vk::Format::eR16G16Unorm;
	case SHADER_DATA_TYPE_UNORM16X4:			return vk::Format::eR16G16B16A16Unorm;
	case SHADER_DATA_TYPE_UNORM8X4:				return vk::Format::eR8G8B8A8Unorm;
	case SHADER_DATA_TYPE_UINT8X4:				return vk::Format::eR8G8B8A8Uint;
	case SHADER_DATA_TYPE_UINT16X4:				return vk::Format::eR16G16B16A16Uint;
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
