#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#define VMA_IMPLEMENTATION

#include "Zenith_Vulkan_MemoryManager.h"
#include "Zenith_Vulkan_MemoryManager_Internal.h"
#include "Zenith_Vulkan_CommandBuffer.h"

#include "Zenith_Vulkan.h"

#include "Collections/Zenith_HashMap.h"
#include "Core/Zenith_CommandLine.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_PerFrame.h"
#include "Flux/Flux_RendererImpl.h"


// Phase 6b: memory-manager state moved to Zenith_Vulkan_MemoryManager
// held by Zenith_Engine. Access via g_xEngine.FluxMemory().m_xXxx.

Zenith_Vulkan_MemoryManager::PerFrameStaging& Zenith_Vulkan_MemoryManager::CurrentStaging()
{
	const u_int uFrameIndex = m_pxVulkanSwapchain->GetCurrentFrameIndex();
	Zenith_Assert(uFrameIndex < MAX_FRAMES_IN_FLIGHT,
		"CurrentStaging: frame index %u out of range (max %u). Swapchain not initialised before staging access?",
		uFrameIndex, MAX_FRAMES_IN_FLIGHT);
	return m_axStaging[uFrameIndex];
}

void Zenith_Vulkan_MemoryManager::IncreaseImageMemoryUsage(u_int64 ulSize)  { m_ulImageMemoryUsed += ulSize; }
void Zenith_Vulkan_MemoryManager::DecreaseImageMemoryUsage(u_int64 ulSize)  { m_ulImageMemoryUsed -= ulSize; }
const u_int64* Zenith_Vulkan_MemoryManager::GetImageMemoryUsagePtr()        { return &m_ulImageMemoryUsed; }
void Zenith_Vulkan_MemoryManager::IncreaseBufferMemoryUsage(u_int64 ulSize) { m_ulBufferMemoryUsed += ulSize; }
void Zenith_Vulkan_MemoryManager::DecreaseBufferMemoryUsage(u_int64 ulSize) { m_ulBufferMemoryUsed -= ulSize; }
const u_int64* Zenith_Vulkan_MemoryManager::GetBufferMemoryUsagePtr()       { return &m_ulBufferMemoryUsed; }
void Zenith_Vulkan_MemoryManager::IncreaseMemoryUsage(u_int64 ulSize)       { m_ulMemoryUsed += ulSize; }
void Zenith_Vulkan_MemoryManager::DecreaseMemoryUsage(u_int64 ulSize)       { m_ulMemoryUsed -= ulSize; }
const u_int64* Zenith_Vulkan_MemoryManager::GetMemoryUsagePtr()             { return &m_ulMemoryUsed; }
VmaAllocator   Zenith_Vulkan_MemoryManager::GetVMAAllocator()               { return m_xAllocator; }

void Zenith_Vulkan_MemoryManager::InitialiseStagingBuffer()
{
	const vk::Device& xDevice = m_pxVulkan->GetDevice();
	const vk::PhysicalDevice& xPhysicalDevice = m_pxVulkan->GetPhysicalDevice();
	const vk::PhysicalDeviceMemoryProperties xMemProps = xPhysicalDevice.getMemoryProperties();

	// Staging is a host-coherent memcpy target — every upload path here maps
	// the slot's memory, memcpys data, and unmaps. Without HOST_COHERENT we'd
	// need explicit vkFlushMappedMemoryRanges calls after every memcpy and a
	// matching vkInvalidateMappedMemoryRanges before the GPU reads (none of
	// which the upload paths do today). Require both bits in the predicate.
	//
	// The previous predicate read
	//     (props & HostVisible | HostCoherent) == (HostVisible | HostCoherent)
	// which due to operator precedence evaluates as
	//     ((props & HostVisible) | HostCoherent) == (HostVisible | HostCoherent)
	// — i.e. it forced the right-hand bits onto the left without actually
	// testing for HOST_COHERENT, so a HOST_VISIBLE-only memory type would
	// silently pass. Parenthesised correctly below.
	const vk::MemoryPropertyFlags eRequired =
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

	for (u_int uSlot = 0; uSlot < MAX_FRAMES_IN_FLIGHT; uSlot++)
	{
		PerFrameStaging& xSlot = m_axStaging[uSlot];

		vk::BufferCreateInfo xInfo = vk::BufferCreateInfo()
			.setSize(g_uStagingPoolSize)
			.setUsage(vk::BufferUsageFlagBits::eTransferSrc)
			.setSharingMode(vk::SharingMode::eExclusive);

		xSlot.m_xBuffer = VkUnwrap(xDevice.createBuffer(xInfo));

		const vk::MemoryRequirements xRequirements = xDevice.getBufferMemoryRequirements(xSlot.m_xBuffer);

		uint32_t uMemoryType = ~0u;
		for (uint32_t i = 0; i < xMemProps.memoryTypeCount; i++)
		{
			if (!(xRequirements.memoryTypeBits & (1u << i))) continue;
			if ((xMemProps.memoryTypes[i].propertyFlags & eRequired) != eRequired) continue;
			uMemoryType = i;
			break;
		}
		Zenith_Assert(uMemoryType != ~0u,
			"InitialiseStagingBuffer: no HOST_VISIBLE | HOST_COHERENT memory type satisfies the staging buffer's memoryTypeBits mask");

		const vk::MemoryAllocateInfo xAllocInfo = vk::MemoryAllocateInfo()
			.setAllocationSize(ALIGN(xRequirements.size, 4096))
			.setMemoryTypeIndex(uMemoryType);

		xSlot.m_xMemory = VkUnwrap(xDevice.allocateMemory(xAllocInfo));
		VkCheck(xDevice.bindBufferMemory(xSlot.m_xBuffer, xSlot.m_xMemory, 0));

		xSlot.m_uNextFreeOffset      = 0;
		xSlot.m_uHighWaterMark       = 0;
		xSlot.m_uMidFrameFlushCount  = 0;
		xSlot.m_xAllocations.Clear();
	}
}

void Zenith_Vulkan_MemoryManager::Initialise()
{
	// Self-wire cross-subsystem deps into member pointers; Initialise() stays
	// no-arg to satisfy the FluxBackendMemoryAlloc concept. Vulkan is already
	// Initialised by this point (Flux.cpp orders Vulkan().Initialise() first);
	// the others are only USED later at runtime, so caching their up-front
	// pointers here is safe.
	m_pxVulkan          = &g_xEngine.FluxBackend();
	m_pxVulkanSwapchain = &g_xEngine.FluxSwapchain();
	m_pxFluxGraphics    = &g_xEngine.FluxGraphics();

	VmaAllocatorCreateInfo xCreateInfo = {};
	xCreateInfo.device = m_pxVulkan->GetDevice();
	xCreateInfo.physicalDevice = m_pxVulkan->GetPhysicalDevice();
	xCreateInfo.instance = m_pxVulkan->GetInstance();
#ifdef ZENITH_ANDROID
	xCreateInfo.vulkanApiVersion = VK_API_VERSION_1_1;
#elif defined(VK_VERSION_1_3)
	xCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
#else
#error check vulkan version
#endif

	vmaCreateAllocator(&xCreateInfo, &m_xAllocator);

	// Probe cache starts empty on each engine init. Populated lazily inside
	// ProbeImageMemoryRequirements; the cache is valid for the device's
	// lifetime and is cleared in Shutdown below.
	Zenith_VulkanMemory_ProbeCache().Clear();

	m_xCommandBuffer.Initialise(COMMANDTYPE_COPY);
	m_bRecording = false; // in-process re-init: recording state must not leak across Initialise

	InitialiseStagingBuffer();

	#ifdef ZENITH_DEBUG_VARIABLES
	g_xEngine.DebugVariables().AddUInt64_ReadOnly({ "Vulkan", "Memory Manager", "Image Memory Used" }, m_ulImageMemoryUsed);
	g_xEngine.DebugVariables().AddUInt64_ReadOnly({ "Vulkan", "Memory Manager", "Buffer Memory Used" }, m_ulBufferMemoryUsed);
	g_xEngine.DebugVariables().AddUInt64_ReadOnly({ "Vulkan", "Memory Manager", "Total Memory Used" }, m_ulMemoryUsed);

	// Per-slot staging counters. The high-water mark surfaces how close the
	// bump allocator is getting to the pool size (any value approaching
	// g_uStagingPoolSize indicates pressure that could trigger mid-frame
	// flushes); the flush count is a direct read of how often the staging-full
	// path fired on each slot since boot.
	for (u_int uSlot = 0; uSlot < MAX_FRAMES_IN_FLIGHT; uSlot++)
	{
		PerFrameStaging& xSlot = m_axStaging[uSlot];
		const std::string strSlotLabel = "Slot " + std::to_string(uSlot);
		// AddUInt64_ReadOnly takes a uint64_t&. size_t is uint64_t on x64,
		// but reinterpret to be safe across platforms.
		g_xEngine.DebugVariables().AddUInt64_ReadOnly(
			{ "Vulkan", "Memory Manager", "Staging", strSlotLabel, "High Water Mark (bytes)" },
			reinterpret_cast<uint64_t&>(xSlot.m_uHighWaterMark));
		g_xEngine.DebugVariables().AddUInt32_ReadOnly(
			{ "Vulkan", "Memory Manager", "Staging", strSlotLabel, "Mid-Frame Flushes" },
			xSlot.m_uMidFrameFlushCount);
	}
	#endif

	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan memory manager initialised");
}


Zenith_Vulkan_MemoryManager::VMAStats Zenith_Vulkan_MemoryManager::GetVMAStats()
{
	VMAStats xStats = {};

	if (m_xAllocator == nullptr)
	{
		return xStats;
	}

	VmaTotalStatistics xVmaStats;
	vmaCalculateStatistics(m_xAllocator, &xVmaStats);

	// Sum up all heap statistics
	xStats.m_ulTotalAllocatedBytes = xVmaStats.total.statistics.blockBytes;
	xStats.m_ulTotalUsedBytes = xVmaStats.total.statistics.allocationBytes;
	xStats.m_ulAllocationCount = xVmaStats.total.statistics.allocationCount;

	return xStats;
}

void Zenith_Vulkan_MemoryManager::Shutdown()
{
	// Drain any straggler staged uploads / open recording while the copy
	// queue is still alive (the memory manager shuts down before the backend).
	Flush();

	const vk::Device& xDevice = m_pxVulkan->GetDevice();

	// Drain all pending deletions. Each iteration decrements every entry's
	// counter by 1, so we need enough iterations for the longest-lived counter
	// (pool VRAMs queued with uExtraFrameDelay=1 → counter = MAX_FRAMES_IN_FLIGHT + 2)
	// to reach zero. Looping until empty is robust to any future caller bumping
	// uExtraFrameDelay without needing to update this constant in lockstep.
	// ProcessDeferredDeletions never re-queues (each entry terminates via delete /
	// ReleaseVRAMHandle / destroyImageView), so this cannot infinite-loop — the
	// safety cap is belt-and-braces in case a re-queueing path gets introduced.
	constexpr u_int uDrainSafetyCap = 64;
	u_int uDrainIterations = 0;
	while (m_xPendingDeletions.GetSize() > 0 && uDrainIterations < uDrainSafetyCap)
	{
		ProcessDeferredDeletions();
		uDrainIterations++;
	}
	Zenith_Assert(m_xPendingDeletions.GetSize() == 0,
		"Zenith_Vulkan_MemoryManager::Shutdown: drain exceeded %u iterations with %u entries still pending",
		uDrainSafetyCap, m_xPendingDeletions.GetSize());

	// Destroy all remaining VRAM allocations that weren't explicitly freed
	// This handles game resources that weren't cleaned up before shutdown
	std::vector<Zenith_Vulkan_VRAM*>& xVRAMRegistry = m_pxVulkan->m_xVRAMRegistry;
	u_int uLeakedAllocations = 0;
	for (size_t i = 0; i < xVRAMRegistry.size(); i++)
	{
		if (xVRAMRegistry[i] != nullptr)
		{
			uLeakedAllocations++;
			delete xVRAMRegistry[i];
			xVRAMRegistry[i] = nullptr;
		}
	}
	if (uLeakedAllocations > 0)
	{
		Zenith_Log(LOG_CATEGORY_VULKAN, "Warning: Cleaned up %u leaked VRAM allocations during shutdown", uLeakedAllocations);
	}
	xVRAMRegistry.clear();

	// Destroy per-frame staging buffers + memory blocks. Each slot owns its
	// own pair so this teardown loop must visit all of them; relying on a
	// single shared pair would leave MFIF-1 buffers leaked.
	for (u_int uSlot = 0; uSlot < MAX_FRAMES_IN_FLIGHT; uSlot++)
	{
		PerFrameStaging& xSlot = m_axStaging[uSlot];
		if (xSlot.m_xBuffer)
		{
			xDevice.destroyBuffer(xSlot.m_xBuffer);
			xSlot.m_xBuffer = nullptr;
		}
		if (xSlot.m_xMemory)
		{
			xDevice.freeMemory(xSlot.m_xMemory);
			xSlot.m_xMemory = nullptr;
		}
		xSlot.m_uNextFreeOffset      = 0;
		xSlot.m_uHighWaterMark       = 0;
		xSlot.m_uMidFrameFlushCount  = 0;
		xSlot.m_xAllocations.Clear();
	}

	// Destroy per-frame scratch buffers that bypass the VRAM registry
	// (CreatePersistentlyMappedBuffer allocates via VMA but doesn't register).
	// Must happen before vmaDestroyAllocator or the blocks leak.
	for (u_int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		m_pxVulkan->m_axPerFrame[i].ShutdownScratchBuffer();
	}

	// Destroy VMA allocator
	vmaDestroyAllocator(m_xAllocator);
	m_xAllocator = nullptr;

	// Probe cache is tied to the VMA allocator's device lifetime; drop it now
	// so a subsequent Initialise starts from a known empty state.
	Zenith_VulkanMemory_ProbeCache().Clear();

	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan memory manager shut down");
}

void Zenith_Vulkan_MemoryManager::EnsureRecording()
{
	if (!m_bRecording)
	{
		m_xCommandBuffer.BeginRecording();
		m_bRecording = true;
	}
}

void Zenith_Vulkan_MemoryManager::Flush()
{
	if (m_xAllocator == VK_NULL_HANDLE)
	{
		// Headless / pre-Initialise: there is no GPU work to drain.
		return;
	}
	if (!m_bRecording && CurrentStaging().m_xAllocations.GetSize() == 0)
	{
		return;
	}

	FlushStagingBuffer();
	m_xCommandBuffer.EndAndCpuWait(false);
	m_bRecording = false;
}

void Zenith_Vulkan_MemoryManager::SubmitFrameMemoryWork()
{
	if (m_xAllocator == VK_NULL_HANDLE)
	{
		return;
	}
	if (!m_bRecording && CurrentStaging().m_xAllocations.GetSize() == 0)
	{
		// Nothing recorded this frame. Zenith_Vulkan::EndFrame null-guards
		// m_pxMemoryUpdateCmdBuf and still submits the memory SubmitInfo with
		// zero command buffers, so the memory-semaphore chain is unaffected.
		return;
	}

	// ProcessDeferredDeletions is driven directly from Flux_PerFrame's
	// end-of-frame work, not here. The +1 buffer in MAX_FRAMES_IN_FLIGHT + 1
	// keeps deletion safe — the per-resource counter only reaches zero after
	// the GPU has fully drained any in-flight frame that could have
	// referenced it.

	FlushStagingBuffer();
	VkCheck(m_xCommandBuffer.GetCurrentCmdBuffer().end());
	m_bRecording = false;
	m_pxVulkan->m_pxMemoryUpdateCmdBuf = &m_xCommandBuffer;
}

void Zenith_Vulkan_MemoryManager::ImageTransitionBarrier(vk::Image xImage, vk::ImageLayout eOldLayout, vk::ImageLayout eNewLayout, vk::ImageAspectFlags eAspect, vk::PipelineStageFlags eSrcStage, vk::PipelineStageFlags eDstStage, uint32_t uMipLevel, uint32_t uLayer)
{
	EnsureRecording();
	m_xCommandBuffer.ImageTransitionBarrier(xImage, eOldLayout, eNewLayout, eAspect, eSrcStage, eDstStage, uMipLevel, uLayer);
}


static void EmitTransferWriteBarrier(vk::CommandBuffer& xCmdBuffer, vk::Buffer xBuffer, vk::DeviceSize uOffset, vk::DeviceSize uSize)
{
	vk::BufferMemoryBarrier xBarrier = vk::BufferMemoryBarrier()
		.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
		.setDstAccessMask(vk::AccessFlagBits::eTransferWrite)
		.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setBuffer(xBuffer)
		.setOffset(uOffset)
		.setSize(uSize);

	xCmdBuffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer,
		vk::PipelineStageFlagBits::eTransfer,
		vk::DependencyFlags(),
		0, nullptr,
		1, &xBarrier,
		0, nullptr);
}

void Zenith_Vulkan_MemoryManager::FlushStagingBufferAllocation(const StagingMemoryAllocation& xAlloc)
{
	const StagingBufferMetadata& xMeta = xAlloc.m_xBufferMetadata;
	const vk::DeviceSize uSrcOffset = static_cast<vk::DeviceSize>(xAlloc.m_uOffset);
	const vk::DeviceSize uDestOffset = static_cast<vk::DeviceSize>(xMeta.m_uDestOffset);
	const vk::DeviceSize uSize = static_cast<vk::DeviceSize>(xAlloc.m_uSize);

	EmitTransferWriteBarrier(m_xCommandBuffer.GetCurrentCmdBuffer(), xMeta.m_xBuffer, uDestOffset, uSize);

	// Source comes from the current frame's staging slot — the same slot the
	// allocation was bumped from in UploadBufferData / UploadBufferDataAtOffset.
	vk::BufferCopy xCopyRegion(uSrcOffset, uDestOffset, uSize);
	m_xCommandBuffer.GetCurrentCmdBuffer().copyBuffer(CurrentStaging().m_xBuffer, xMeta.m_xBuffer, xCopyRegion);
}

void Zenith_Vulkan_MemoryManager::FlushStagingTextureAllocation(const StagingMemoryAllocation& xAlloc)
{
	const StagingTextureMetadata& xMeta = xAlloc.m_xTextureMetadata;
	const vk::Image& xImage = xMeta.m_xImage;

	// Transition all mips to transfer-dst for the copy
	for (uint32_t uLayer = 0; uLayer < xMeta.m_uNumLayers; uLayer++)
	{
		for (uint32_t uMip = 0; uMip < xMeta.m_uNumMips; uMip++)
		{
			m_xCommandBuffer.ImageTransitionBarrier(xImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, uMip, uLayer);
		}
	}

	if (xMeta.m_bPreBakedMips)
	{
		// Pre-baked: the staging data holds every mip packed contiguously (mip 0
		// first, per CalculateTotalMipChainSize). Copy one region per mip at its
		// own buffer offset/extent, then transition all mips to shader-read — NO
		// blit generation (BC formats can't be blitted and the data is already
		// correct). Single-layer only; multi-layer pre-baked is out of scope.
		Zenith_Assert(xMeta.m_uNumLayers == 1, "Pre-baked mip upload supports a single layer only");

		const uint32_t uBlockOrPixel = IsCompressedFormat(xMeta.m_eFormat)
			? CompressedFormatBytesPerBlock(xMeta.m_eFormat)
			: ColourFormatBytesPerPixel(xMeta.m_eFormat);

		size_t ulMipOffset = 0;
		for (uint32_t uMip = 0; uMip < xMeta.m_uNumMips; uMip++)
		{
			const uint32_t uMipW = std::max(1u, xMeta.m_uWidth >> uMip);
			const uint32_t uMipH = std::max(1u, xMeta.m_uHeight >> uMip);
			const uint32_t uMipD = std::max(1u, xMeta.m_uDepth >> uMip);
			const uint64_t ulBufferOffset = static_cast<uint64_t>(xAlloc.m_uOffset) + ulMipOffset;

			// vkCmdCopyBufferToImage requires bufferOffset be a multiple of the
			// texel block size. The staging base is 16-aligned and every prior
			// mip is a whole number of blocks, so this holds — assert to catch a
			// future format/packing that breaks the invariant.
			Zenith_Assert((ulBufferOffset % uBlockOrPixel) == 0,
				"Pre-baked mip %u buffer offset %llu is not a multiple of block/pixel size %u",
				uMip, ulBufferOffset, uBlockOrPixel);

			vk::ImageSubresourceLayers xMipSubresource = vk::ImageSubresourceLayers()
				.setAspectMask(vk::ImageAspectFlagBits::eColor)
				.setMipLevel(uMip)
				.setBaseArrayLayer(0)
				.setLayerCount(1);

			vk::BufferImageCopy xMipRegion = vk::BufferImageCopy()
				.setBufferOffset(ulBufferOffset)
				.setBufferRowLength(0)
				.setBufferImageHeight(0)
				.setImageSubresource(xMipSubresource)
				.setImageOffset({ 0, 0, 0 })
				.setImageExtent({ uMipW, uMipH, uMipD });

			m_xCommandBuffer.GetCurrentCmdBuffer().copyBufferToImage(CurrentStaging().m_xBuffer, xImage, vk::ImageLayout::eTransferDstOptimal, 1, &xMipRegion);

			ulMipOffset += CalculateMipDataSize(xMeta.m_eFormat, xMeta.m_uWidth, xMeta.m_uHeight, uMip);
		}

		for (uint32_t uMip = 0; uMip < xMeta.m_uNumMips; uMip++)
		{
			m_xCommandBuffer.ImageTransitionBarrier(xImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, uMip, 0);
		}
		return;
	}

	// Copy staging buffer to image
	vk::ImageSubresourceLayers xSubresource = vk::ImageSubresourceLayers()
		.setAspectMask(vk::ImageAspectFlagBits::eColor)
		.setMipLevel(0)
		.setBaseArrayLayer(0)
		.setLayerCount(xMeta.m_uNumLayers);

	vk::BufferImageCopy region = vk::BufferImageCopy()
		.setBufferOffset(xAlloc.m_uOffset)
		.setBufferRowLength(0)
		.setBufferImageHeight(0)
		.setImageSubresource(xSubresource)
		.setImageOffset({ 0, 0, 0 })
		.setImageExtent({ xMeta.m_uWidth, xMeta.m_uHeight, xMeta.m_uDepth });

	m_xCommandBuffer.GetCurrentCmdBuffer().copyBufferToImage(CurrentStaging().m_xBuffer, xImage, vk::ImageLayout::eTransferDstOptimal, 1, &region);

	// Generate mipmaps (via blit for non-compressed) and transition to shader-read
	const bool bIsCompressed = IsCompressedFormat(xMeta.m_eFormat);
	for (uint32_t uLayer = 0; uLayer < xMeta.m_uNumLayers; uLayer++)
	{
		GenerateMipmapsAndTransitionToShaderRead(xImage, xMeta.m_uWidth, xMeta.m_uHeight, xMeta.m_uNumMips, uLayer, bIsCompressed);
	}
}

void Zenith_Vulkan_MemoryManager::FlushStagingBuffer()
{
	Zenith_Profiling::ScopeZone xProfileScope(ZENITH_PROFILE_ZONE("Vulkan Memory Manager Flush"));

	EnsureRecording();

	// Flush only the current frame slot's pending allocations. Other slots
	// own their own allocation lists and offsets — they get flushed when the
	// ring index lands on them in a future frame's EndFrame.
	PerFrameStaging& xStaging = CurrentStaging();
	for (u_int i = 0; i < xStaging.m_xAllocations.GetSize(); i++)
	{
		const StagingMemoryAllocation& xAlloc = xStaging.m_xAllocations.Get(i);
		if (xAlloc.m_eType == ALLOCATION_TYPE_BUFFER)
		{
			FlushStagingBufferAllocation(xAlloc);
		}
		else if (xAlloc.m_eType == ALLOCATION_TYPE_TEXTURE)
		{
			FlushStagingTextureAllocation(xAlloc);
		}
	}

	xStaging.m_xAllocations.Clear();
	xStaging.m_uNextFreeOffset = 0;
}

// Callers MUST hold g_xEngine.FluxMemory().m_xMutex and this function MUST NOT take it — Flush()
// is the only callee and it does not reacquire the mutex (FlushStagingBuffer
// and EndAndCpuWait do GPU work lock-free). Adding a lock here, or to any
// function reached from here, will deadlock every staging upload path because
// Zenith_Mutex is non-recursive.
//
// Flush() drains the *current slot's* pending allocations synchronously and
// closes the wrapper's command buffer; recording restarts lazily on the next
// memory operation. The slot identity (the chosen index in
// g_xEngine.FluxMemory().m_axStaging) does not change across this — the
// swapchain's current frame index only advances on a real frame boundary,
// not when we mid-frame-flush. So callers that re-resolve CurrentStaging()
// after this returns get the same slot back, just with m_uNextFreeOffset
// reset to 0.
void Zenith_Vulkan_MemoryManager::HandleStagingBufferFull()
{
	const u_int uFrameSlot = m_pxVulkanSwapchain->GetCurrentFrameIndex();
	PerFrameStaging& xSlot = m_axStaging[uFrameSlot];
	xSlot.m_uMidFrameFlushCount++;
	Zenith_Log(LOG_CATEGORY_VULKAN,
		"Staging buffer full on slot %u (offset=%llu / pool=%llu, mid-frame flush #%u). Forcing mid-frame submit.",
		uFrameSlot,
		static_cast<unsigned long long>(xSlot.m_uNextFreeOffset),
		static_cast<unsigned long long>(g_uStagingPoolSize),
		xSlot.m_uMidFrameFlushCount);
	Flush();
}


void Zenith_Vulkan_MemoryManager::UploadBufferDataChunked(vk::Buffer xDestBuffer, const void* pData, size_t uSize)
{
	Zenith_Profiling::ScopeZone xProfileScope(ZENITH_PROFILE_ZONE("Vulkan Memory Manager Upload"));
	Zenith_Log(LOG_CATEGORY_VULKAN, "Uploading large buffer in chunks: %llu bytes (staging buffer size: %llu bytes)", uSize, g_uStagingPoolSize);

	const vk::Device& xDevice = m_pxVulkan->GetDevice();
	const uint8_t* pSrcData = static_cast<const uint8_t*>(pData);
	size_t uRemainingSize = uSize;
	size_t uCurrentOffset = 0;

	// Process in chunks that fit in the staging buffer
	while (uRemainingSize > 0)
	{
		// Calculate chunk size (leave some headroom for alignment)
		const size_t uChunkSize = (std::min)(uRemainingSize, g_uStagingPoolSize - 4096);

		m_xMutex.Lock();

		PerFrameStaging& xStaging = CurrentStaging();
		// Ensure the current slot is empty before this synchronous chunk; this
		// path issues an EndAndCpuWait per chunk, so the slot's bump allocator
		// always returns to zero between iterations.
		if (xStaging.m_uNextFreeOffset != 0)
		{
			HandleStagingBufferFull();
		}

		// Create staging allocation for this chunk
		StagingMemoryAllocation xAllocation;
		xAllocation.m_eType = ALLOCATION_TYPE_BUFFER;
		xAllocation.m_xBufferMetadata.m_xBuffer = xDestBuffer;
		xAllocation.m_xBufferMetadata.m_uDestOffset = uCurrentOffset;
		xAllocation.m_uSize = uChunkSize;
		xAllocation.m_uOffset = 0; // Always use offset 0 since we flush between chunks
		xStaging.m_xAllocations.PushBack(xAllocation);

		// Map, copy, and unmap
		void* pMap = VkUnwrap(xDevice.mapMemory(xStaging.m_xMemory, 0, uChunkSize));
		memcpy(pMap, pSrcData + uCurrentOffset, uChunkSize);
		xDevice.unmapMemory(xStaging.m_xMemory);

		EnsureRecording();
		EmitTransferWriteBarrier(m_xCommandBuffer.GetCurrentCmdBuffer(), xDestBuffer, uCurrentOffset, uChunkSize);

		vk::BufferCopy xCopyRegion(0, uCurrentOffset, uChunkSize);
		m_xCommandBuffer.GetCurrentCmdBuffer().copyBuffer(xStaging.m_xBuffer, xDestBuffer, xCopyRegion);

		m_xCommandBuffer.EndAndCpuWait(false);
		m_bRecording = false; // recording restarts lazily on the next chunk / operation

		// Clear staging allocations after flush
		xStaging.m_xAllocations.Clear();
		xStaging.m_uNextFreeOffset = 0;

		// Move to next chunk
		uCurrentOffset += uChunkSize;
		uRemainingSize -= uChunkSize;

		m_xMutex.Unlock();
	}

	Zenith_Log(LOG_CATEGORY_VULKAN, "Chunked buffer upload complete");
}

void Zenith_Vulkan_MemoryManager::UploadTextureDataChunked(vk::Image xDestImage, const void* pData, size_t uSize, uint32_t uWidth, uint32_t uHeight, uint32_t uNumMips, uint32_t uNumLayers, bool bPreBakedMips)
{
	Zenith_Profiling::ScopeZone xProfileScope(ZENITH_PROFILE_ZONE("Vulkan Memory Manager Upload"));

	// The chunked path scanline-copies into mip 0 only and then runtime-generates
	// the rest — it cannot honour a pre-baked multi-mip chain. Fail loudly in ALL
	// builds rather than silently mis-uploading a chain bigger than the staging
	// pool. (Material BC textures are far below the pool and never reach here.)
	if (bPreBakedMips)
	{
		Zenith_Error(LOG_CATEGORY_VULKAN, "Pre-baked multi-mip texture (%llu bytes) exceeds the staging pool — chunked pre-baked upload is unsupported; texture skipped", uSize);
		return;
	}

	Zenith_Log(LOG_CATEGORY_VULKAN, "Uploading large texture in chunks: %llu bytes (staging buffer size: %llu bytes)", uSize, g_uStagingPoolSize);

	const vk::Device& xDevice = m_pxVulkan->GetDevice();
	const uint8_t* pSrcData = static_cast<const uint8_t*>(pData);
	size_t uCurrentOffset = 0;

	// For simplicity, chunk by scanlines to avoid partial row uploads
	// This assumes mip level 0 only for now (no mipmap support in chunked path)
	const size_t uBytesPerRow = uSize / (uHeight * uNumLayers);
	const size_t uRowsPerChunk = std::max(size_t(1), (g_uStagingPoolSize - 4096) / uBytesPerRow);
	const size_t uChunkHeight = std::min(size_t(uHeight), uRowsPerChunk);

	uint32_t uCurrentRow = 0;

	// Transition entire image to transfer dst layout first
	EnsureRecording();
	for (uint32_t uLayer = 0; uLayer < uNumLayers; uLayer++)
	{
		for (uint32_t uMip = 0; uMip < uNumMips; uMip++)
		{
			m_xCommandBuffer.ImageTransitionBarrier(xDestImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, uMip, uLayer);
		}
	}

	while (uCurrentRow < uHeight * uNumLayers)
	{
		const uint32_t uCurrentLayer = uCurrentRow / uHeight;
		const uint32_t uRowInLayer = uCurrentRow % uHeight;
		const uint32_t uRemainingRows = static_cast<uint32_t>(std::min(uChunkHeight, static_cast<size_t>(uHeight - uRowInLayer)));
		const size_t uChunkSize = uRemainingRows * uBytesPerRow;

		m_xMutex.Lock();

		// Ensure the current slot's staging is empty before this chunk row
		if (CurrentStaging().m_uNextFreeOffset != 0)
		{
			m_xMutex.Unlock();
			HandleStagingBufferFull();
			m_xMutex.Lock();
		}

		PerFrameStaging& xStaging = CurrentStaging();

		// Map, copy, and unmap
		void* pMap = VkUnwrap(xDevice.mapMemory(xStaging.m_xMemory, 0, uChunkSize));
		memcpy(pMap, pSrcData + uCurrentOffset, uChunkSize);
		xDevice.unmapMemory(xStaging.m_xMemory);

		m_xMutex.Unlock();

		// Copy this chunk to the image
		vk::ImageSubresourceLayers xSubresource = vk::ImageSubresourceLayers()
			.setAspectMask(vk::ImageAspectFlagBits::eColor)
			.setMipLevel(0)
			.setBaseArrayLayer(uCurrentLayer)
			.setLayerCount(1);

		vk::BufferImageCopy region = vk::BufferImageCopy()
			.setBufferOffset(0)
			.setBufferRowLength(0)
			.setBufferImageHeight(0)
			.setImageSubresource(xSubresource)
			.setImageOffset({ 0, static_cast<int32_t>(uRowInLayer), 0 })
			.setImageExtent({ uWidth, uRemainingRows, 1 });

		// A staging-full Flush above may have closed the command buffer —
		// reopen before recording this chunk's copy.
		EnsureRecording();
		m_xCommandBuffer.GetCurrentCmdBuffer().copyBufferToImage(xStaging.m_xBuffer, xDestImage, vk::ImageLayout::eTransferDstOptimal, 1, &region);

		uCurrentOffset += uChunkSize;
		uCurrentRow += uRemainingRows;
	}

	// Generate mipmaps (non-compressed only) and transition to shader-read
	for (uint32_t uLayer = 0; uLayer < uNumLayers; uLayer++)
	{
		GenerateMipmapsAndTransitionToShaderRead(xDestImage, uWidth, uHeight, uNumMips, uLayer, false);
	}

	// Execute and wait
	m_xCommandBuffer.EndAndCpuWait(false);
	m_bRecording = false; // recording restarts lazily on the next operation

	// Clean up the current slot
	{
		PerFrameStaging& xStaging = CurrentStaging();
		xStaging.m_xAllocations.Clear();
		xStaging.m_uNextFreeOffset = 0;
	}

	Zenith_Log(LOG_CATEGORY_VULKAN, "Chunked texture upload complete");
}


void Zenith_Vulkan_MemoryManager::QueueVRAMDeletion(Flux_VRAMHandle& xHandle,
	Flux_ImageViewHandle xRTV, Flux_ImageViewHandle xDSV, Flux_ImageViewHandle xSRV, Flux_ImageViewHandle xUAV,
	u_int uExtraFrameDelay)
{
	// Resolve the VRAM record from the handle internally (nullptr for an
	// invalid handle, e.g. the QueueImageViewDeletion path that frees only a
	// view). Engine callers pass just the handle -- they never see Flux_VRAM*.
	Zenith_Vulkan_VRAM* pxVRAM = m_pxVulkan->GetVRAM(xHandle);

	if (pxVRAM == nullptr && !xRTV.IsValid() && !xDSV.IsValid() &&
		!xSRV.IsValid() && !xUAV.IsValid())
	{
		return;
	}

	// Pool VRAMs MUST be queued with uExtraFrameDelay >= 1 — aliased images
	// bound into this pool rely on their own (shorter) deletion delay draining
	// BEFORE the pool's VkDeviceMemory is freed. Forgetting the extra delay
	// produces a silent one-frame use-after-free on the aliased VkImages, which
	// the validation layer catches only intermittently. Trip the assert at the
	// queue call instead so the offending caller is obvious from the stack.
	Zenith_Assert(pxVRAM == nullptr || !pxVRAM->IsPool() || uExtraFrameDelay >= 1,
		"QueueVRAMDeletion: pool VRAM requires uExtraFrameDelay >= 1 to outlive its aliased images");

	PendingVRAMDeletion xDeletion;
	xDeletion.m_pxVRAM = pxVRAM;
	xDeletion.m_xHandle = xHandle;
	xDeletion.m_xRTV = xRTV;
	xDeletion.m_xDSV = xDSV;
	xDeletion.m_xSRV = xSRV;
	xDeletion.m_xUAV = xUAV;
	// Wait MAX_FRAMES_IN_FLIGHT + 1 to ensure GPU has finished with resource.
	// +1 because the resource might still be in use by command buffers being built this frame.
	// Aliasing pool callers pass uExtraFrameDelay=1 so the pool's VkDeviceMemory is freed
	// strictly AFTER any aliased images bound to it (ProcessDeferredDeletions's RemoveSwap
	// iteration doesn't preserve within-frame order). The assert above enforces this
	// invariant at the queue point rather than waiting for the validation layer.
	xDeletion.m_uFramesRemaining = MAX_FRAMES_IN_FLIGHT + 1 + uExtraFrameDelay;
	m_xPendingDeletions.PushBack(xDeletion);

	// Auto-invalidate the caller's handle to prevent double-free
	xHandle = Flux_VRAMHandle();
}

void Zenith_Vulkan_MemoryManager::QueueImageViewDeletion(Flux_ImageViewHandle xImageViewHandle)
{
	if (!xImageViewHandle.IsValid())
	{
		return;
	}

	// Queue for deletion without VRAM - just destroy the image view
	Flux_VRAMHandle xInvalidHandle;  // Default constructed handle is invalid
	QueueVRAMDeletion(xInvalidHandle, xImageViewHandle);
}

void Zenith_Vulkan_MemoryManager::DestroyImageViewIfValid(const vk::Device& xDevice, Flux_ImageViewHandle& xHandle)
{
	if (!xHandle.IsValid())
	{
		return;
	}

	vk::ImageView xView = GetImageView(xHandle);
	xDevice.destroyImageView(xView);
	ReleaseImageViewHandle(xHandle);
}

void Zenith_Vulkan_MemoryManager::ProcessDeferredDeletions()
{
	const vk::Device& xDevice = m_pxVulkan->GetDevice();

	// Note on within-frame ordering: RemoveSwap reorders the vector by moving
	// the last element into the slot of the removed one. Any ordering between
	// deletions that all hit zero on the same call is LOST. The cross-frame
	// invariant that keeps this safe — pool VRAMs outlive their aliased images
	// by >=1 frame — is enforced at queue time in QueueVRAMDeletion via the
	// uExtraFrameDelay >= 1 assertion on pool VRAMs. If that assertion ever
	// has to be relaxed, either introduce a second pool-only deletion queue
	// or replace RemoveSwap here with order-preserving removal.
	for (u_int i = 0; i < m_xPendingDeletions.GetSize();)
	{
		PendingVRAMDeletion& xDeletion = m_xPendingDeletions.Get(i);
		xDeletion.m_uFramesRemaining--;

		if (xDeletion.m_uFramesRemaining == 0)
		{
			// Destroy all image views before deleting VRAM
			DestroyImageViewIfValid(xDevice, xDeletion.m_xRTV);
			DestroyImageViewIfValid(xDevice, xDeletion.m_xDSV);
			DestroyImageViewIfValid(xDevice, xDeletion.m_xSRV);
			DestroyImageViewIfValid(xDevice, xDeletion.m_xUAV);

			// Delete VRAM and release handle (only if VRAM exists)
			if (xDeletion.m_pxVRAM != nullptr)
			{
				delete xDeletion.m_pxVRAM;
				m_pxVulkan->ReleaseVRAMHandle(xDeletion.m_xHandle);
			}

			m_xPendingDeletions.RemoveSwap(i);
			// Don't increment - check the swapped-in element
		}
		else
		{
			i++;
		}
	}
}

Zenith_Vulkan_VRAM::Zenith_Vulkan_VRAM(AliasedImageTag, const vk::Image xImage, const VmaAllocation xPoolAllocation, VmaAllocator xAllocator)
	: m_xAllocation(xPoolAllocation), m_xAllocator(xAllocator), m_xImage(xImage), m_bAliased(true)
{
	// No memory accounting bump here — the pool VRAM already counted the
	// allocation's size when it was created; multiple aliased images sharing
	// that allocation must not double-count.
}

Zenith_Vulkan_VRAM::Zenith_Vulkan_VRAM(PoolTag, const VmaAllocation xAllocation, VmaAllocator xAllocator, u_int64 ulPoolSize)
	: m_xAllocation(xAllocation), m_xAllocator(xAllocator), m_bPool(true), m_ulPoolSize(ulPoolSize)
{
	// Trust the VMA-reported allocation size over the caller-provided ulPoolSize
	// for memory accounting: VMA may align the backing allocation above the
	// requested size, and using its value keeps engine counters in sync with
	// what's actually on the heap. Store that same value in m_ulPoolSize so
	// the destructor's DecreaseImageMemoryUsage call matches the increase.
	if (xAllocation != VK_NULL_HANDLE)
	{
		VmaAllocationInfo xInfo = {};
		vmaGetAllocationInfo(xAllocator, xAllocation, &xInfo);
		if (xInfo.size > 0)
			m_ulPoolSize = xInfo.size;
	}
	auto& xVulkanMemory = g_xEngine.FluxMemory();
	xVulkanMemory.IncreaseImageMemoryUsage(m_ulPoolSize);
	xVulkanMemory.IncreaseMemoryUsage(m_ulPoolSize);
}

// VMA treats VmaAllocation (= VmaAllocation_T*) as an OPAQUE handle; its internal
// GetSize() member is only visible in a TU where the VMA *implementation* is
// compiled. That happens to be true on Windows here, but VmaAllocation_T is an
// incomplete type under the Android/clang TU, so GetVmaAllocationSizeBytes(m_xAllocator, m_xAllocation) fails to
// compile there. Query the size through the public vmaGetAllocationInfo API
// instead (works on every platform; mirrors the pool-size path above).
static VkDeviceSize GetVmaAllocationSizeBytes(VmaAllocator xAllocator, VmaAllocation xAllocation)
{
	VmaAllocationInfo xInfo = {};
	vmaGetAllocationInfo(xAllocator, xAllocation, &xInfo);
	return xInfo.size;
}

Zenith_Vulkan_VRAM::Zenith_Vulkan_VRAM(const vk::Image xImage, const VmaAllocation xAllocation, VmaAllocator xAllocator)
	: m_xImage(xImage), m_xAllocation(xAllocation), m_xAllocator(xAllocator)
{
	auto& xVulkanMemory = g_xEngine.FluxMemory();
	xVulkanMemory.IncreaseImageMemoryUsage(GetVmaAllocationSizeBytes(m_xAllocator, m_xAllocation));
	xVulkanMemory.IncreaseMemoryUsage(GetVmaAllocationSizeBytes(m_xAllocator, m_xAllocation));
}

Zenith_Vulkan_VRAM::Zenith_Vulkan_VRAM(const vk::Buffer xBuffer, const VmaAllocation xAllocation, VmaAllocator xAllocator, const u_int uSize)
	: m_xBuffer(xBuffer), m_xAllocation(xAllocation), m_xAllocator(xAllocator), m_uBufferSize(uSize)
{
	auto& xVulkanMemory = g_xEngine.FluxMemory();
	xVulkanMemory.IncreaseBufferMemoryUsage(GetVmaAllocationSizeBytes(m_xAllocator, m_xAllocation));
	xVulkanMemory.IncreaseMemoryUsage(GetVmaAllocationSizeBytes(m_xAllocator, m_xAllocation));
}

Zenith_Vulkan_VRAM::~Zenith_Vulkan_VRAM()
{
	// Three destruction paths: aliased-image (image only), pool (allocation
	// only), or regular owned resource (image+allocation or buffer+allocation).

	auto& xVulkanMemory = g_xEngine.FluxMemory();

	if (m_bAliased)
	{
		Zenith_Assert(m_xImage != VK_NULL_HANDLE, "Aliased VRAM missing image");
		// Destroy the image alone — the allocation is owned by the pool
		// VRAM and will be freed when the pool is destroyed. No memory-usage
		// accounting bump here either; the pool counted for everyone.
		g_xEngine.FluxBackend().GetDevice().destroyImage(m_xImage);
		return;
	}

	if (m_bPool)
	{
		Zenith_Assert(m_xAllocation != VK_NULL_HANDLE, "Pool VRAM missing allocation");
		xVulkanMemory.DecreaseImageMemoryUsage(m_ulPoolSize);
		xVulkanMemory.DecreaseMemoryUsage(m_ulPoolSize);
		vmaFreeMemory(m_xAllocator, m_xAllocation);
		return;
	}

	if (m_xAllocation != VK_NULL_HANDLE && m_xAllocator != VK_NULL_HANDLE)
	{
		// Query the allocation size ONCE, up front: vmaDestroyImage / vmaDestroyBuffer
		// below FREE m_xAllocation, so querying it after the destroy (as the final
		// DecreaseMemoryUsage call used to) is a use-after-free of the VMA allocation.
		// (The previous m_xAllocation->GetSize() read a stale-but-intact cached field
		// and happened not to fault; vmaGetAllocationInfo dereferences the freed
		// allocation's memory handle and crashes.) Cache it before any destroy.
		const VkDeviceSize ulAllocationSize = GetVmaAllocationSizeBytes(m_xAllocator, m_xAllocation);
		if (m_xImage != VK_NULL_HANDLE)
		{
			xVulkanMemory.DecreaseImageMemoryUsage(ulAllocationSize);
			vmaDestroyImage(m_xAllocator, m_xImage, m_xAllocation);
		}
		else if (m_xBuffer != VK_NULL_HANDLE)
		{
			xVulkanMemory.DecreaseBufferMemoryUsage(ulAllocationSize);
			vmaDestroyBuffer(m_xAllocator, m_xBuffer, m_xAllocation);
		}
		xVulkanMemory.DecreaseMemoryUsage(ulAllocationSize);
	}
	else
	{
		Zenith_Assert(false, "Deleting dodgy allocation");
	}
}
