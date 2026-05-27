#include "Zenith.h"
#define VMA_IMPLEMENTATION

#include "Zenith_Vulkan_MemoryManager.h"
#include "Zenith_Vulkan_MemoryManager.h"
#include "Zenith_Vulkan_MemoryManager_Internal.h"
#include "Zenith_Vulkan_CommandBuffer.h"

#include "Zenith_Vulkan.h"
#include "Zenith_Vulkan.h"

#include "Collections/Zenith_HashMap.h"
#include "Core/Zenith_CommandLine.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_PerFrame.h"
#include "Flux/Flux_RendererImpl.h"

// Probe cache storage. ProbeCacheEntry itself lives in
// Zenith_Vulkan_MemoryManager_Internal.h so a future sibling TU (the
// aliasing split from T2.1) can share the type. The cache is cleared in
// Initialise and Shutdown; populated lazily on misses inside
// ProbeImageMemoryRequirements.
static Zenith_HashMap<u_int64, ProbeCacheEntry> s_xProbeCache;

// Phase 6b: memory-manager state moved to Zenith_Vulkan_MemoryManager
// held by Zenith_Engine. Access via g_xEngine.VulkanMemory().m_xXxx.

Zenith_Vulkan_MemoryManager::PerFrameStaging& Zenith_Vulkan_MemoryManager::CurrentStaging()
{
	const u_int uFrameIndex = g_xEngine.VulkanSwapchain().GetCurrentFrameIndex();
	Zenith_Assert(uFrameIndex < MAX_FRAMES_IN_FLIGHT,
		"CurrentStaging: frame index %u out of range (max %u). Swapchain not initialised before staging access?",
		uFrameIndex, MAX_FRAMES_IN_FLIGHT);
	return g_xEngine.VulkanMemory().m_axStaging[uFrameIndex];
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
	const vk::Device& xDevice = g_xEngine.Vulkan().GetDevice();
	const vk::PhysicalDevice& xPhysicalDevice = g_xEngine.Vulkan().GetPhysicalDevice();
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
		PerFrameStaging& xSlot = g_xEngine.VulkanMemory().m_axStaging[uSlot];

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
	

	VmaAllocatorCreateInfo xCreateInfo = {};
	xCreateInfo.device = g_xEngine.Vulkan().GetDevice();
	xCreateInfo.physicalDevice = g_xEngine.Vulkan().GetPhysicalDevice();
	xCreateInfo.instance = g_xEngine.Vulkan().GetInstance();
#ifdef ZENITH_ANDROID
	xCreateInfo.vulkanApiVersion = VK_API_VERSION_1_1;
#elif defined(VK_VERSION_1_3)
	xCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
#else
#error check vulkan version
#endif

	vmaCreateAllocator(&xCreateInfo, &g_xEngine.VulkanMemory().m_xAllocator);

	// Probe cache starts empty on each engine init. Populated lazily inside
	// ProbeImageMemoryRequirements; the cache is valid for the device's
	// lifetime and is cleared in Shutdown below.
	s_xProbeCache.Clear();

	g_xEngine.VulkanMemory().m_xCommandBuffer.Initialise(COMMANDTYPE_COPY);

	InitialiseStagingBuffer();

	#ifdef ZENITH_DEBUG_VARIABLES
	g_xEngine.DebugVariables().AddUInt64_ReadOnly({ "Vulkan", "Memory Manager", "Image Memory Used" }, g_xEngine.VulkanMemory().m_ulImageMemoryUsed);
	g_xEngine.DebugVariables().AddUInt64_ReadOnly({ "Vulkan", "Memory Manager", "Buffer Memory Used" }, g_xEngine.VulkanMemory().m_ulBufferMemoryUsed);
	g_xEngine.DebugVariables().AddUInt64_ReadOnly({ "Vulkan", "Memory Manager", "Total Memory Used" }, g_xEngine.VulkanMemory().m_ulMemoryUsed);

	// Per-slot staging counters. The high-water mark surfaces how close the
	// bump allocator is getting to the pool size (any value approaching
	// g_uStagingPoolSize indicates pressure that could trigger mid-frame
	// flushes); the flush count is a direct read of how often the staging-full
	// path fired on each slot since boot.
	for (u_int uSlot = 0; uSlot < MAX_FRAMES_IN_FLIGHT; uSlot++)
	{
		PerFrameStaging& xSlot = g_xEngine.VulkanMemory().m_axStaging[uSlot];
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

	// Register the deferred-VRAM-deletion countdown as an end-frame callback.
	// Fires unconditionally each main loop iteration (skipped frames included)
	// to match the pre-extraction behaviour where MemoryManager::EndFrame ran
	// on every iteration. Registered AFTER Zenith_Vulkan's begin callback so
	// the natural begin-then-end ordering is preserved. Counted in
	// FLUX_PERFRAME_END_SUBSCRIBER_TALLY (Flux_PerFrame.cpp): bump that tally
	// if you add another end callback.
	g_xEngine.FluxRenderer().RegisterEndFrameCallback(&Zenith_Vulkan_MemoryManager::OnFluxPerFrameEnd, nullptr);

	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan memory manager initialised");
}

bool Zenith_Vulkan_MemoryManager::SupportsTransientAliasing()
{
	// Vulkan supports aliasing via VMA's aliasing-image primitive (VMA 3.x).
	// The render graph uses this flag to decide whether to run the per-frame
	// packer; when false, every transient gets its own vmaCreateImage call
	// exactly as before aliasing was introduced.
	return true;
}

Flux_VRAMHandle Zenith_Vulkan_MemoryManager::CreateAliasPoolVRAM(u_int64 ulSize, u_int64 ulAlignment,
                                                                  AliasPoolMemoryKind eMemoryKind)
{
	// Headless guard: see CreateBufferVRAM for rationale.
	if (g_xEngine.VulkanMemory().m_xAllocator == VK_NULL_HANDLE)
	{
		return Flux_VRAMHandle();
	}

	Zenith_Assert(ulSize > 0, "CreateAliasPoolVRAM: zero-size pool");

	// VMA's AUTO_* usages are forbidden with vmaAllocateMemory (no image/buffer
	// to infer memory type from) — specify requiredFlags explicitly instead.
	// For render-graph transients we always want DEVICE_LOCAL memory;
	// memoryTypeBits is UINT32_MAX so VMA picks any matching heap. Alignment
	// is the max across all transients the packer will bind into this pool
	// (queried via ProbeImageMemoryRequirements); fall back to 64 KB if the
	// caller passes 0 (probe failed).
	VkMemoryRequirements xReqs = {};
	xReqs.size           = ulSize;
	xReqs.alignment      = (ulAlignment > 0) ? ulAlignment : 65536ull;
	xReqs.memoryTypeBits = UINT32_MAX;

	// Translate the memory-kind enum to the vk::MemoryPropertyFlags VMA needs
	// as requiredFlags. The AliasPoolMemoryKind enum is the public contract;
	// switching here keeps the Vulkan bitmask local to the backend.
	VkMemoryPropertyFlags eRequiredFlags = 0;
	switch (eMemoryKind)
	{
		case AliasPoolMemoryKind::DeviceLocal:
			eRequiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			break;
		case AliasPoolMemoryKind::HostVisibleCoherent:
			eRequiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			break;
	}
	Zenith_Assert(eRequiredFlags != 0, "CreateAliasPoolVRAM: unknown AliasPoolMemoryKind %d", static_cast<int>(eMemoryKind));

	VmaAllocationCreateInfo xCreateInfo = {};
	xCreateInfo.usage         = VMA_MEMORY_USAGE_UNKNOWN;
	xCreateInfo.requiredFlags = eRequiredFlags;
	xCreateInfo.flags         = VMA_ALLOCATION_CREATE_CAN_ALIAS_BIT;

	VmaAllocation xAllocation = VK_NULL_HANDLE;
	VkResult eResult = vmaAllocateMemory(g_xEngine.VulkanMemory().m_xAllocator, &xReqs, &xCreateInfo, &xAllocation, nullptr);
	Zenith_Assert(eResult == VK_SUCCESS, "vmaAllocateMemory (alias pool) failed: %d", static_cast<int>(eResult));
	if (eResult != VK_SUCCESS)
	{
		return Flux_VRAMHandle();
	}

	Zenith_Vulkan_VRAM* pxVRAM = new Zenith_Vulkan_VRAM(Zenith_Vulkan_VRAM::PoolTag{}, xAllocation, g_xEngine.VulkanMemory().m_xAllocator, ulSize);
	return g_xEngine.Vulkan().RegisterVRAM(pxVRAM);
}

// Shared helper — builds the VkImageCreateInfo used by both the probe path
// and the aliased-image creation path. Centralising the logic guarantees the
// two callers cannot drift (mip/layer clamp, usage flags, texture-type
// handling), which is the invariant the packer relies on: the probe's
// reported size must match the creation path's actual allocation.
static vk::ImageCreateInfo BuildAliasedImageCreateInfo(const Flux_SurfaceInfo& xInfo)
{
	const bool bIsColour       = xInfo.m_eFormat > TEXTURE_FORMAT_COLOUR_BEGIN        && xInfo.m_eFormat < TEXTURE_FORMAT_COLOUR_END;
	const bool bIsDepthStencil = xInfo.m_eFormat > TEXTURE_FORMAT_DEPTH_STENCIL_BEGIN && xInfo.m_eFormat < TEXTURE_FORMAT_DEPTH_STENCIL_END;
	Zenith_Assert(bIsColour ^ bIsDepthStencil, "BuildAliasedImageCreateInfo: format must be colour XOR depth/stencil");

	vk::Format xFormat;
	vk::ImageUsageFlags eUsageFlags;
	if (bIsColour)
	{
		xFormat = g_xEngine.Vulkan().ConvertToVkFormat_Colour(xInfo.m_eFormat);
		eUsageFlags = vk::ImageUsageFlagBits::eColorAttachment;
		if (xInfo.m_uMemoryFlags & 1 << MEMORY_FLAGS__UNORDERED_ACCESS)
			eUsageFlags |= vk::ImageUsageFlagBits::eStorage;
	}
	else
	{
		xFormat = g_xEngine.Vulkan().ConvertToVkFormat_DepthStencil(xInfo.m_eFormat);
		eUsageFlags = vk::ImageUsageFlagBits::eDepthStencilAttachment;
	}
	if (xInfo.m_uMemoryFlags & 1 << MEMORY_FLAGS__SHADER_READ)
		eUsageFlags |= vk::ImageUsageFlagBits::eSampled;

	vk::ImageType eImageType = vk::ImageType::e2D;
	vk::Extent3D xExtent = { xInfo.m_uWidth, xInfo.m_uHeight, 1 };
	vk::ImageCreateFlags eCreateFlags = {};
	if (xInfo.m_eTextureType == TEXTURE_TYPE_3D)
	{
		eImageType = vk::ImageType::e3D;
		xExtent = vk::Extent3D(xInfo.m_uWidth, xInfo.m_uHeight, xInfo.m_uDepth);
	}
	else if (xInfo.m_eTextureType == TEXTURE_TYPE_CUBE)
	{
		eCreateFlags |= vk::ImageCreateFlagBits::eCubeCompatible;
	}

	return vk::ImageCreateInfo()
		.setImageType(eImageType)
		.setFormat(xFormat)
		.setTiling(vk::ImageTiling::eOptimal)
		.setExtent(xExtent)
		.setMipLevels(xInfo.m_uNumMips == 0 ? 1u : xInfo.m_uNumMips)
		.setArrayLayers(xInfo.m_uNumLayers == 0 ? 1u : xInfo.m_uNumLayers)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setUsage(eUsageFlags)
		.setSharingMode(vk::SharingMode::eExclusive)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setFlags(eCreateFlags);
}

// Signature that uniquely identifies the memory requirements of an image
// probed via ProbeImageMemoryRequirements. Includes every field that affects
// the driver's reported size or alignment (format, dimensions, mips/layers,
// texture type, and memory-flag-derived usage bits that flip the VkImage
// usage mask). The cache below is keyed on this signature so repeated probes
// across recompiles (typical after a window resize) don't re-issue the
// vkCreateImage / vkGetImageMemoryRequirements / vkDestroyImage trio.
static u_int64 MakeProbeSignature(const Flux_SurfaceInfo& xInfo)
{
	// Pack the scalar fields into a single u_int64. Dimensions dominate the
	// entropy budget; format / texture type / flags fit in the remaining bits.
	// If any of these widths is exceeded the shift goes out of range and the
	// cache keys begin to collide — trip a static_assert-style runtime check.
	u_int64 ulSig = 0;
	ulSig ^= static_cast<u_int64>(xInfo.m_uWidth);
	ulSig ^= static_cast<u_int64>(xInfo.m_uHeight) << 16;
	ulSig ^= static_cast<u_int64>(xInfo.m_uDepth) << 32;
	ulSig ^= static_cast<u_int64>(xInfo.m_uNumMips) << 40;
	ulSig ^= static_cast<u_int64>(xInfo.m_uNumLayers) << 44;
	ulSig ^= static_cast<u_int64>(xInfo.m_eFormat) << 48;
	ulSig ^= static_cast<u_int64>(xInfo.m_eTextureType) << 56;
	// Memory flags fold into a secondary mixing word to avoid colliding with
	// the primary extent+format bits.
	ulSig ^= static_cast<u_int64>(xInfo.m_uMemoryFlags) * 0x9e3779b97f4a7c15ull;
	return ulSig;
}

// s_xProbeCache / ProbeCacheEntry are declared at the top of this TU — they
// must be defined before Initialise references them. The lookup / populate
// logic below uses the cache transparently.

void Zenith_Vulkan_MemoryManager::ProbeImageMemoryRequirements(const Flux_SurfaceInfo& xInfo,
                                                                u_int64& ulSizeOut,
                                                                u_int64& ulAlignmentOut)
{
	ulSizeOut = 0;
	ulAlignmentOut = 0;

	// Hard format check — the packer depends on the probe returning accurate
	// sizes. A "neither colour nor depth-stencil" format can only reach here
	// via an uninitialised / newly-added format that no existing path knows
	// how to size, and silently falling through to (0, 0) caused
	// CreateAliasPoolVRAM to apply the 64 KB fallback alignment — a silent
	// under-alignment risk that would only surface as an intermittent VMA
	// binding failure on specific drivers. Trip instead so the offending
	// format is named at the call site.
	const bool bIsColour       = xInfo.m_eFormat > TEXTURE_FORMAT_COLOUR_BEGIN        && xInfo.m_eFormat < TEXTURE_FORMAT_COLOUR_END;
	const bool bIsDepthStencil = xInfo.m_eFormat > TEXTURE_FORMAT_DEPTH_STENCIL_BEGIN && xInfo.m_eFormat < TEXTURE_FORMAT_DEPTH_STENCIL_END;
	Zenith_Assert(bIsColour ^ bIsDepthStencil,
		"ProbeImageMemoryRequirements: format '%d' is neither colour nor depth-stencil — the aliasing packer cannot size it. Add the format to the colour or depth-stencil range in Flux_Enums.h, or route this transient away from the aliasing path.",
		static_cast<int>(xInfo.m_eFormat));
	Zenith_Assert(xInfo.m_uWidth > 0 && xInfo.m_uHeight > 0,
		"ProbeImageMemoryRequirements: zero-size extent (%ux%ux%u)", xInfo.m_uWidth, xInfo.m_uHeight, xInfo.m_uDepth);

	// Cache lookup. Repeated compiles (resize → rebuild graph → probe same
	// transients) skip the full create/getRequirements/destroy trio; typical
	// probe counts of ~20 per compile turn into zero Vulkan calls after the
	// first compile.
	const u_int64 ulSig = MakeProbeSignature(xInfo);
	if (const ProbeCacheEntry* pxHit = s_xProbeCache.TryGet(ulSig))
	{
		ulSizeOut      = pxHit->m_ulSize;
		ulAlignmentOut = pxHit->m_ulAlignment;
		return;
	}

	const vk::ImageCreateInfo xImageInfo = BuildAliasedImageCreateInfo(xInfo);

	const vk::Device& xDevice = g_xEngine.Vulkan().GetDevice();
	vk::Image xProbeImage;
	vk::Result eCreateResult = xDevice.createImage(&xImageInfo, nullptr, &xProbeImage);
	if (eCreateResult != vk::Result::eSuccess)
	{
		// createImage failure is surfaced to the caller via (0, 0) so the
		// packer can still fall through to a safer standalone allocation
		// (e.g. platforms with tighter usage-flag restrictions). The assert
		// above already catches the format-range cause; this path covers
		// driver-reported failures we don't diagnose here. Do NOT populate
		// the cache on failure — the next call should retry (the driver may
		// have been in a transient state).
		return;
	}

	vk::MemoryRequirements xReqs = xDevice.getImageMemoryRequirements(xProbeImage);
	xDevice.destroyImage(xProbeImage);

	ulSizeOut      = xReqs.size;
	ulAlignmentOut = xReqs.alignment;

	ProbeCacheEntry xEntry;
	xEntry.m_ulSize      = ulSizeOut;
	xEntry.m_ulAlignment = ulAlignmentOut;
	s_xProbeCache.Insert(ulSig, xEntry);
}

Flux_VRAMHandle Zenith_Vulkan_MemoryManager::CreateAliasedImageVRAM(const Flux_SurfaceInfo& xInfo,
                                                                    Flux_VRAMHandle xPoolHandle,
                                                                    u_int64 ulOffsetInPool)
{
	// Headless guard: see CreateBufferVRAM for rationale.
	if (g_xEngine.VulkanMemory().m_xAllocator == VK_NULL_HANDLE)
	{
		return Flux_VRAMHandle();
	}

	Zenith_Assert(xPoolHandle.IsValid(), "CreateAliasedImageVRAM: invalid pool handle");
	Zenith_Vulkan_VRAM* pxPoolVRAM = g_xEngine.Vulkan().GetVRAM(xPoolHandle);
	Zenith_Assert(pxPoolVRAM != nullptr && pxPoolVRAM->IsPool(),
		"CreateAliasedImageVRAM: pool handle does not reference a pool VRAM");

	// Same image-create-info as the probe path — BuildAliasedImageCreateInfo is
	// the single source of truth so the packer's reported size is guaranteed
	// to match what we actually bind into the pool.
	const vk::ImageCreateInfo xImageInfo = BuildAliasedImageCreateInfo(xInfo);

	const vk::ImageCreateInfo::NativeType xImageInfoNative = xImageInfo;

	VkImage xImage = VK_NULL_HANDLE;
	VkResult eResult = vmaCreateAliasingImage2(
		g_xEngine.VulkanMemory().m_xAllocator,
		pxPoolVRAM->GetAllocation(),
		ulOffsetInPool,
		&xImageInfoNative,
		&xImage);
	Zenith_Assert(eResult == VK_SUCCESS, "vmaCreateAliasingImage2 failed: %d (offset=%llu)",
		static_cast<int>(eResult), static_cast<unsigned long long>(ulOffsetInPool));
	if (eResult != VK_SUCCESS)
	{
		return Flux_VRAMHandle();
	}

	Zenith_Vulkan_VRAM* pxVRAM = new Zenith_Vulkan_VRAM(
		Zenith_Vulkan_VRAM::AliasedImageTag{},
		vk::Image(xImage),
		pxPoolVRAM->GetAllocation(),
		g_xEngine.VulkanMemory().m_xAllocator);
	return g_xEngine.Vulkan().RegisterVRAM(pxVRAM);
}

void Zenith_Vulkan_MemoryManager::OnFluxPerFrameEnd(u_int /*uRingIndex*/, void* /*pUserData*/)
{
	// Decrement the per-resource frames-remaining counter on every queued
	// deletion; destroy resources whose counter has reached 0. Lives here as
	// the Flux_PerFrame end-frame callback rather than inside EndFrame()
	// because the per-frame ring is the natural owner of "advance the
	// deferred-deletion clock by one tick." Resolves the engine singleton
	// because this is a static callback (no implicit this).
	g_xEngine.VulkanMemory().ProcessDeferredDeletions();
}

Zenith_Vulkan_MemoryManager::VMAStats Zenith_Vulkan_MemoryManager::GetVMAStats()
{
	VMAStats xStats = {};

	if (g_xEngine.VulkanMemory().m_xAllocator == nullptr)
	{
		return xStats;
	}

	VmaTotalStatistics xVmaStats;
	vmaCalculateStatistics(g_xEngine.VulkanMemory().m_xAllocator, &xVmaStats);

	// Sum up all heap statistics
	xStats.m_ulTotalAllocatedBytes = xVmaStats.total.statistics.blockBytes;
	xStats.m_ulTotalUsedBytes = xVmaStats.total.statistics.allocationBytes;
	xStats.m_ulAllocationCount = xVmaStats.total.statistics.allocationCount;

	return xStats;
}

void Zenith_Vulkan_MemoryManager::Shutdown()
{
	const vk::Device& xDevice = g_xEngine.Vulkan().GetDevice();

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
	while (g_xEngine.VulkanMemory().m_xPendingDeletions.GetSize() > 0 && uDrainIterations < uDrainSafetyCap)
	{
		ProcessDeferredDeletions();
		uDrainIterations++;
	}
	Zenith_Assert(g_xEngine.VulkanMemory().m_xPendingDeletions.GetSize() == 0,
		"Zenith_Vulkan_MemoryManager::Shutdown: drain exceeded %u iterations with %u entries still pending",
		uDrainSafetyCap, g_xEngine.VulkanMemory().m_xPendingDeletions.GetSize());

	// Destroy all remaining VRAM allocations that weren't explicitly freed
	// This handles game resources that weren't cleaned up before shutdown
	std::vector<Zenith_Vulkan_VRAM*>& xVRAMRegistry = g_xEngine.Vulkan().m_xVRAMRegistry;
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
		PerFrameStaging& xSlot = g_xEngine.VulkanMemory().m_axStaging[uSlot];
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
		g_xEngine.Vulkan().m_axPerFrame[i].ShutdownScratchBuffer();
	}

	// Destroy VMA allocator
	vmaDestroyAllocator(g_xEngine.VulkanMemory().m_xAllocator);
	g_xEngine.VulkanMemory().m_xAllocator = nullptr;

	// Probe cache is tied to the VMA allocator's device lifetime; drop it now
	// so a subsequent Initialise starts from a known empty state.
	s_xProbeCache.Clear();

	Zenith_Log(LOG_CATEGORY_VULKAN, "Vulkan memory manager shut down");
}

Zenith_Vulkan_CommandBuffer& Zenith_Vulkan_MemoryManager::GetCommandBuffer() {
	return g_xEngine.VulkanMemory().m_xCommandBuffer;
}

// ImageView handle registry implementation
Flux_ImageViewHandle Zenith_Vulkan_MemoryManager::RegisterImageView(vk::ImageView xView)
{
	Flux_ImageViewHandle xHandle;
	if (g_xEngine.VulkanMemory().m_xFreeImageViewHandles.GetSize() > 0)
	{
		u_int uIndex = g_xEngine.VulkanMemory().m_xFreeImageViewHandles.GetBack();
		g_xEngine.VulkanMemory().m_xFreeImageViewHandles.PopBack();
		g_xEngine.VulkanMemory().m_xImageViewRegistry.Get(uIndex) = xView;
		xHandle.SetValue(uIndex);
	}
	else
	{
		xHandle.SetValue(g_xEngine.VulkanMemory().m_xImageViewRegistry.GetSize());
		g_xEngine.VulkanMemory().m_xImageViewRegistry.PushBack(xView);
	}
	return xHandle;
}

vk::ImageView Zenith_Vulkan_MemoryManager::GetImageView(Flux_ImageViewHandle xHandle)
{
	if (!xHandle.IsValid() || xHandle.AsUInt() >= g_xEngine.VulkanMemory().m_xImageViewRegistry.GetSize())
	{
		return VK_NULL_HANDLE;
	}
	return g_xEngine.VulkanMemory().m_xImageViewRegistry.Get(xHandle.AsUInt());
}

void Zenith_Vulkan_MemoryManager::ReleaseImageViewHandle(Flux_ImageViewHandle xHandle)
{
	if (!xHandle.IsValid() || xHandle.AsUInt() >= g_xEngine.VulkanMemory().m_xImageViewRegistry.GetSize())
	{
		return;
	}
	g_xEngine.VulkanMemory().m_xImageViewRegistry.Get(xHandle.AsUInt()) = VK_NULL_HANDLE;
	g_xEngine.VulkanMemory().m_xFreeImageViewHandles.PushBack(xHandle.AsUInt());
}

// BufferDescriptor handle registry implementation
Flux_BufferDescriptorHandle Zenith_Vulkan_MemoryManager::RegisterBufferDescriptor(const vk::DescriptorBufferInfo& xInfo)
{
	Flux_BufferDescriptorHandle xHandle;
	if (g_xEngine.VulkanMemory().m_xFreeBufferDescHandles.GetSize() > 0)
	{
		u_int uIndex = g_xEngine.VulkanMemory().m_xFreeBufferDescHandles.GetBack();
		g_xEngine.VulkanMemory().m_xFreeBufferDescHandles.PopBack();
		g_xEngine.VulkanMemory().m_xBufferDescriptorRegistry.Get(uIndex) = xInfo;
		xHandle.SetValue(uIndex);
	}
	else
	{
		xHandle.SetValue(g_xEngine.VulkanMemory().m_xBufferDescriptorRegistry.GetSize());
		g_xEngine.VulkanMemory().m_xBufferDescriptorRegistry.PushBack(xInfo);
	}
	return xHandle;
}

vk::DescriptorBufferInfo Zenith_Vulkan_MemoryManager::GetBufferDescriptor(Flux_BufferDescriptorHandle xHandle)
{
	if (!xHandle.IsValid() || xHandle.AsUInt() >= g_xEngine.VulkanMemory().m_xBufferDescriptorRegistry.GetSize())
	{
		return vk::DescriptorBufferInfo();
	}
	return g_xEngine.VulkanMemory().m_xBufferDescriptorRegistry.Get(xHandle.AsUInt());
}

void Zenith_Vulkan_MemoryManager::ReleaseBufferDescriptorHandle(Flux_BufferDescriptorHandle xHandle)
{
	if (!xHandle.IsValid() || xHandle.AsUInt() >= g_xEngine.VulkanMemory().m_xBufferDescriptorRegistry.GetSize())
	{
		return;
	}
	g_xEngine.VulkanMemory().m_xBufferDescriptorRegistry.Get(xHandle.AsUInt()) = vk::DescriptorBufferInfo();
	g_xEngine.VulkanMemory().m_xFreeBufferDescHandles.PushBack(xHandle.AsUInt());
}

void Zenith_Vulkan_MemoryManager::BeginFrame()
{
	g_xEngine.VulkanMemory().m_xCommandBuffer.BeginRecording();

	//#TO_TODO: asset handler
	//AssetHandler* pxAssetHandler = pxApp->m_pxAssetHandler;
	//pxAssetHandler->ProcessPendingDeletes();
}

void Zenith_Vulkan_MemoryManager::EndFrame(bool bDefer /*= true*/)
{
	FlushStagingBuffer();

	// ProcessDeferredDeletions used to be called here; it is now driven by
	// Flux_PerFrame::EndFrame's end-frame callback (registered in Initialise).
	// The relative timing shifts later in the frame but the +1 buffer in
	// MAX_FRAMES_IN_FLIGHT + 1 keeps the deletion safe — the per-resource
	// counter only reaches zero after the GPU has fully drained any in-flight
	// frame that could have referenced it.

	if (bDefer)
	{
		VkCheck(g_xEngine.VulkanMemory().m_xCommandBuffer.GetCurrentCmdBuffer().end());
		g_xEngine.Vulkan().m_pxMemoryUpdateCmdBuf = &g_xEngine.VulkanMemory().m_xCommandBuffer;
	}
	else
	{
		g_xEngine.VulkanMemory().m_xCommandBuffer.EndAndCpuWait(false);
	}
}

void Zenith_Vulkan_MemoryManager::ImageTransitionBarrier(vk::Image xImage, vk::ImageLayout eOldLayout, vk::ImageLayout eNewLayout, vk::ImageAspectFlags eAspect, vk::PipelineStageFlags eSrcStage, vk::PipelineStageFlags eDstStage, uint32_t uMipLevel, uint32_t uLayer)
{
	g_xEngine.VulkanMemory().m_xCommandBuffer.ImageTransitionBarrier(xImage, eOldLayout, eNewLayout, eAspect, eSrcStage, eDstStage, uMipLevel, uLayer);
}

void Zenith_Vulkan_MemoryManager::InitialiseVertexBuffer(const void* pData, size_t uSize, Flux_VertexBuffer& xBufferOut, bool bDeviceLocal /*= true*/)
{
	Flux_VRAMHandle xHandle = CreateBufferVRAM(static_cast<u_int>(uSize), static_cast<MemoryFlags>(1 << MEMORY_FLAGS__VERTEX_BUFFER), bDeviceLocal ? MEMORY_RESIDENCY_GPU : MEMORY_RESIDENCY_CPU);
	xBufferOut.GetBuffer().m_xVRAMHandle = xHandle;
	xBufferOut.GetBuffer().m_ulSize = uSize;

	if (pData)
	{
		UploadBufferData(xHandle, pData, uSize);
	}
}

void Zenith_Vulkan_MemoryManager::InitialiseDynamicVertexBuffer(const void* pData, size_t uSize, Flux_DynamicVertexBuffer& xBufferOut, bool bDeviceLocal /*= true*/)
{
	for (uint32_t u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		Flux_VRAMHandle xHandle = CreateBufferVRAM(static_cast<u_int>(uSize), static_cast<MemoryFlags>(1 << MEMORY_FLAGS__VERTEX_BUFFER), bDeviceLocal ? MEMORY_RESIDENCY_GPU : MEMORY_RESIDENCY_CPU);
		xBufferOut.GetBufferForFrameInFlight(u).m_xVRAMHandle = xHandle;
		xBufferOut.GetBufferForFrameInFlight(u).m_ulSize = uSize;
		
		if (pData)
		{
			UploadBufferData(xHandle, pData, uSize);
		}
	}
}

void Zenith_Vulkan_MemoryManager::InitialiseIndexBuffer(const void* pData, size_t uSize, Flux_IndexBuffer& xBufferOut)
{
	Flux_VRAMHandle xHandle = CreateBufferVRAM(static_cast<u_int>(uSize), static_cast<MemoryFlags>(1 << MEMORY_FLAGS__INDEX_BUFFER), MEMORY_RESIDENCY_GPU);
	xBufferOut.GetBuffer().m_xVRAMHandle = xHandle;
	xBufferOut.GetBuffer().m_ulSize = uSize;
	
	if (pData)
	{
		UploadBufferData(xHandle, pData, uSize);
	}
}

void Zenith_Vulkan_MemoryManager::InitialiseConstantBuffer(const void* pData, size_t uSize, Flux_ConstantBuffer& xBufferOut)
{
	Flux_VRAMHandle xHandle = CreateBufferVRAM(static_cast<u_int>(uSize), static_cast<MemoryFlags>(1 << MEMORY_FLAGS__SHADER_READ), MEMORY_RESIDENCY_CPU);
	Flux_Buffer& xBuffer = xBufferOut.GetBuffer();
	xBuffer.m_xVRAMHandle = xHandle;
	xBuffer.m_ulSize = uSize;

	Zenith_Vulkan_VRAM* pxVRAM = g_xEngine.Vulkan().GetVRAM(xHandle);
	Zenith_Assert(pxVRAM, "Invalid buffer VRAM handle");

	vk::DescriptorBufferInfo xBufferInfo;
	xBufferInfo.setBuffer(pxVRAM->GetBuffer());
	xBufferInfo.setOffset(0);
	xBufferInfo.setRange(uSize);

	Flux_ConstantBufferView& xCBV = xBufferOut.GetCBV();
	xCBV.m_xBufferDescHandle = RegisterBufferDescriptor(xBufferInfo);
	xCBV.m_xVRAMHandle = xHandle;

	if (pData)
	{
		UploadBufferData(xHandle, pData, uSize);
	}
}

// Set up one frame's VRAM + descriptor for a dynamic (per-frame, host-visible)
// buffer and populate the caller's frame buffer struct. Returns the
// registered descriptor handle so callers can install it into the view types
// they expose (CBV-only for constant buffers, UAV+SRV mirror for RW buffers).
// Both variants share residency (CPU) and the same staging-buffer-free upload
// pattern; only the usage flags and view structures differ.
static Flux_BufferDescriptorHandle InitialiseDynamicBufferFrame(
	const void* pData, size_t uSize, MemoryFlags eFlags, Flux_Buffer& xBufferOut)
{
	Flux_VRAMHandle xHandle = g_xEngine.VulkanMemory().CreateBufferVRAM(
		static_cast<u_int>(uSize), eFlags, MEMORY_RESIDENCY_CPU);
	xBufferOut.m_xVRAMHandle = xHandle;
	xBufferOut.m_ulSize = uSize;

	Zenith_Vulkan_VRAM* pxVRAM = g_xEngine.Vulkan().GetVRAM(xHandle);
	Zenith_Assert(pxVRAM, "Invalid buffer VRAM handle");

	vk::DescriptorBufferInfo xBufferInfo;
	xBufferInfo.setBuffer(pxVRAM->GetBuffer());
	xBufferInfo.setOffset(0);
	xBufferInfo.setRange(uSize);

	const Flux_BufferDescriptorHandle xDesc = g_xEngine.VulkanMemory().RegisterBufferDescriptor(xBufferInfo);

	if (pData)
	{
		g_xEngine.VulkanMemory().UploadBufferData(xHandle, pData, uSize);
	}
	return xDesc;
}

void Zenith_Vulkan_MemoryManager::InitialiseDynamicConstantBuffer(const void* pData, size_t uSize, Flux_DynamicConstantBuffer& xBufferOut)
{
	for (uint32_t u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		Flux_Buffer& xBuffer = xBufferOut.GetBufferForFrameInFlight(u);
		const Flux_BufferDescriptorHandle xDesc = InitialiseDynamicBufferFrame(
			pData, uSize, static_cast<MemoryFlags>(1 << MEMORY_FLAGS__SHADER_READ), xBuffer);

		Flux_ConstantBufferView& xCBV = xBufferOut.GetCBVForFrameInFlight(u);
		xCBV.m_xBufferDescHandle = xDesc;
		xCBV.m_xVRAMHandle = xBuffer.m_xVRAMHandle;
	}
}

void Zenith_Vulkan_MemoryManager::InitialiseIndirectBuffer(size_t uSize, Flux_IndirectBuffer& xBufferOut)
{
	Flux_VRAMHandle xHandle = CreateBufferVRAM(static_cast<u_int>(uSize), static_cast<MemoryFlags>(1 << MEMORY_FLAGS__INDIRECT_BUFFER | 1 << MEMORY_FLAGS__UNORDERED_ACCESS), MEMORY_RESIDENCY_GPU);
	xBufferOut.GetBuffer().m_xVRAMHandle = xHandle;
	xBufferOut.GetBuffer().m_ulSize = uSize;

	Zenith_Vulkan_VRAM* pxVRAM = g_xEngine.Vulkan().GetVRAM(xHandle);
	Zenith_Assert(pxVRAM, "Invalid buffer VRAM handle");

	vk::DescriptorBufferInfo xBufferInfo;
	xBufferInfo.setBuffer(pxVRAM->GetBuffer());
	xBufferInfo.setOffset(0);
	xBufferInfo.setRange(uSize);

	Flux_UnorderedAccessView_Buffer& xUAV = xBufferOut.GetUAV();
	xUAV.m_xBufferDescHandle = RegisterBufferDescriptor(xBufferInfo);
	xUAV.m_xVRAMHandle = xHandle;
}

void Zenith_Vulkan_MemoryManager::InitialiseReadWriteBuffer(const void* pData, size_t uSize, Flux_ReadWriteBuffer& xBufferOut)
{
	Flux_VRAMHandle xHandle = CreateBufferVRAM(static_cast<u_int>(uSize), static_cast<MemoryFlags>(1 << MEMORY_FLAGS__UNORDERED_ACCESS | 1 << MEMORY_FLAGS__SHADER_READ), MEMORY_RESIDENCY_GPU);
	xBufferOut.GetBuffer().m_xVRAMHandle = xHandle;
	xBufferOut.GetBuffer().m_ulSize = uSize;

	Zenith_Vulkan_VRAM* pxVRAM = g_xEngine.Vulkan().GetVRAM(xHandle);
	Zenith_Assert(pxVRAM, "Invalid buffer VRAM handle");

	vk::DescriptorBufferInfo xBufferInfo;
	xBufferInfo.setBuffer(pxVRAM->GetBuffer());
	xBufferInfo.setOffset(0);
	xBufferInfo.setRange(uSize);

	const Flux_BufferDescriptorHandle xBufferDescHandle = RegisterBufferDescriptor(xBufferInfo);

	Flux_UnorderedAccessView_Buffer& xUAV = xBufferOut.GetUAV();
	xUAV.m_xBufferDescHandle = xBufferDescHandle;
	xUAV.m_xVRAMHandle = xHandle;

	// Mirror the same descriptor / VRAM handles into the read-only SSBO view.
	// vk::DescriptorType::eStorageBuffer is identical for StructuredBuffer<T>
	// and RWStructuredBuffer<T>; the distinct CPU view type only exists so the
	// render-graph access classifier and the shader binder can tell apart the
	// read-only and read-write paths.
	Flux_ShaderResourceView_Buffer& xSRV = xBufferOut.GetSRV();
	xSRV.m_xBufferDescHandle = xBufferDescHandle;
	xSRV.m_xVRAMHandle = xHandle;

	if (pData)
	{
		UploadBufferData(xHandle, pData, uSize);
	}
}

void Zenith_Vulkan_MemoryManager::InitialiseDynamicReadWriteBuffer(const void* pData, size_t uSize, Flux_DynamicReadWriteBuffer& xBufferOut)
{
	// CPU-resident (host-visible) to allow direct writes without staging buffer.
	// Per-frame uploads through the GPU staging buffer are unsafe because the staging
	// buffer can be overwritten by the next frame before the GPU executes the copy.
	for (uint32_t u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		Flux_Buffer& xBuffer = xBufferOut.GetBufferForFrameInFlight(u);
		const Flux_BufferDescriptorHandle xDesc = InitialiseDynamicBufferFrame(
			pData, uSize,
			static_cast<MemoryFlags>(1 << MEMORY_FLAGS__UNORDERED_ACCESS | 1 << MEMORY_FLAGS__SHADER_READ),
			xBuffer);

		Flux_UnorderedAccessView_Buffer& xUAV = xBufferOut.GetUAVForFrameInFlight(u);
		xUAV.m_xBufferDescHandle = xDesc;
		xUAV.m_xVRAMHandle = xBuffer.m_xVRAMHandle;

		// Mirror the same descriptor / VRAM handles into the read-only SSBO view.
		// vk::DescriptorType::eStorageBuffer is identical for StructuredBuffer<T>
		// and RWStructuredBuffer<T>; the distinct CPU view type only exists so the
		// render-graph access classifier and the shader binder can tell apart the
		// read-only and read-write paths.
		Flux_ShaderResourceView_Buffer& xSRV = xBufferOut.GetSRVForFrameInFlight(u);
		xSRV.m_xBufferDescHandle = xDesc;
		xSRV.m_xVRAMHandle = xBuffer.m_xVRAMHandle;
	}
}

Flux_VRAMHandle Zenith_Vulkan_MemoryManager::CreateBufferVRAM(const u_int uSize, const MemoryFlags eFlags, MemoryResidency eResidency)
{
	// Headless guard: when Flux::EarlyInitialise was skipped (Zenith_CommandLine::IsHeadless()),
	// g_xEngine.VulkanMemory().m_xAllocator stays VK_NULL_HANDLE. Upstream asset-load paths still
	// invoke buffer creation; return an empty handle so they get a benign
	// invalid VRAM handle to store rather than asserting in vmaCreateBuffer.
	if (g_xEngine.VulkanMemory().m_xAllocator == VK_NULL_HANDLE)
	{
		return Flux_VRAMHandle();
	}

	vk::BufferUsageFlags eUsageFlags = vk::BufferUsageFlagBits::eTransferDst;
	
	if (eFlags & 1 << MEMORY_FLAGS__VERTEX_BUFFER)
		eUsageFlags |= vk::BufferUsageFlagBits::eVertexBuffer;
	if (eFlags & 1 << MEMORY_FLAGS__INDEX_BUFFER)
		eUsageFlags |= vk::BufferUsageFlagBits::eIndexBuffer;
	if (eFlags & 1 << MEMORY_FLAGS__INDIRECT_BUFFER)
		eUsageFlags |= vk::BufferUsageFlagBits::eIndirectBuffer;
	if (eFlags & 1 << MEMORY_FLAGS__UNORDERED_ACCESS)
		eUsageFlags |= vk::BufferUsageFlagBits::eStorageBuffer;
	if (eFlags & 1 << MEMORY_FLAGS__SHADER_READ)
		eUsageFlags |= vk::BufferUsageFlagBits::eUniformBuffer;

	vk::BufferCreateInfo xBufferInfo = vk::BufferCreateInfo()
		.setSize(uSize)
		.setUsage(eUsageFlags)
		.setSharingMode(vk::SharingMode::eExclusive);

	VmaAllocationCreateInfo xAllocInfo = {};
	if (eResidency == MEMORY_RESIDENCY_CPU)
	{
		xAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		xAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
		xAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	}
	else if (eResidency == MEMORY_RESIDENCY_GPU)
	{
		xAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	}

	const vk::BufferCreateInfo::NativeType xBufferInfo_Native = xBufferInfo;

	VkBuffer xBuffer = VK_NULL_HANDLE;
	VmaAllocation xAllocation = VK_NULL_HANDLE;
	VkResult eResult = vmaCreateBuffer(g_xEngine.VulkanMemory().m_xAllocator, &xBufferInfo_Native, &xAllocInfo, &xBuffer, &xAllocation, nullptr);
	Zenith_Assert(eResult == VK_SUCCESS, "vmaCreateBuffer failed with result %d", static_cast<int>(eResult));
	if (eResult != VK_SUCCESS)
	{
		// Return invalid handle on allocation failure
		return Flux_VRAMHandle();
	}

	Zenith_Vulkan_VRAM* pxVRAM = new Zenith_Vulkan_VRAM(vk::Buffer(xBuffer), xAllocation, g_xEngine.VulkanMemory().m_xAllocator, uSize);
	Flux_VRAMHandle xHandle = g_xEngine.Vulkan().RegisterVRAM(pxVRAM);

	return xHandle;
}

Zenith_Vulkan_MemoryManager::PersistentBuffer Zenith_Vulkan_MemoryManager::CreatePersistentlyMappedBuffer(u_int uSize, vk::BufferUsageFlags eUsageFlags)
{
	PersistentBuffer xResult = {};
	xResult.m_uSize = uSize;

	// Headless guard: see CreateBufferVRAM for rationale.
	if (g_xEngine.VulkanMemory().m_xAllocator == VK_NULL_HANDLE)
	{
		return xResult;
	}

	vk::BufferCreateInfo xBufferInfo = vk::BufferCreateInfo()
		.setSize(uSize)
		.setUsage(eUsageFlags)
		.setSharingMode(vk::SharingMode::eExclusive);

	VmaAllocationCreateInfo xAllocInfo = {};
	xAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	xAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	xAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	const vk::BufferCreateInfo::NativeType xBufferInfo_Native = xBufferInfo;

	VkBuffer xBuffer = VK_NULL_HANDLE;
	VmaAllocationInfo xAllocationInfo = {};
	VkResult eResult = vmaCreateBuffer(g_xEngine.VulkanMemory().m_xAllocator, &xBufferInfo_Native, &xAllocInfo, &xBuffer, &xResult.m_xAllocation, &xAllocationInfo);
	Zenith_Assert(eResult == VK_SUCCESS, "vmaCreateBuffer failed for persistent buffer with result %d", static_cast<int>(eResult));

	xResult.m_xBuffer = vk::Buffer(xBuffer);
	xResult.m_pMappedPtr = xAllocationInfo.pMappedData;

	Zenith_Assert(xResult.m_pMappedPtr != nullptr, "Persistent buffer mapping failed");

	return xResult;
}

Flux_VRAMHandle Zenith_Vulkan_MemoryManager::CreateRenderTargetVRAM(const Flux_SurfaceInfo& xInfo)
{
	// Headless guard: see CreateBufferVRAM for rationale.
	if (g_xEngine.VulkanMemory().m_xAllocator == VK_NULL_HANDLE)
	{
		return Flux_VRAMHandle();
	}

	const bool bIsColour = xInfo.m_eFormat > TEXTURE_FORMAT_COLOUR_BEGIN && xInfo.m_eFormat < TEXTURE_FORMAT_COLOUR_END;
	const bool bIsDepthStencil = xInfo.m_eFormat > TEXTURE_FORMAT_DEPTH_STENCIL_BEGIN && xInfo.m_eFormat < TEXTURE_FORMAT_DEPTH_STENCIL_END;
	Zenith_Assert(bIsColour ^ bIsDepthStencil, "Invalid texture format for render target");
	
	vk::Format xFormat;
	vk::ImageUsageFlags eUsageFlags;
	vk::ImageAspectFlags eAspectFlags;
	vk::ImageLayout eInitialLayout;
	
	if (bIsColour)
	{
		xFormat = g_xEngine.Vulkan().ConvertToVkFormat_Colour(xInfo.m_eFormat);
		eUsageFlags = vk::ImageUsageFlagBits::eColorAttachment;
		eAspectFlags = vk::ImageAspectFlagBits::eColor;
		eInitialLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		
		if (xInfo.m_uMemoryFlags & 1 << MEMORY_FLAGS__UNORDERED_ACCESS) 
			eUsageFlags |= vk::ImageUsageFlagBits::eStorage;
	}
	else // bIsDepthStencil
	{
		xFormat = g_xEngine.Vulkan().ConvertToVkFormat_DepthStencil(xInfo.m_eFormat);
		eUsageFlags = vk::ImageUsageFlagBits::eDepthStencilAttachment;
		eAspectFlags = vk::ImageAspectFlagBits::eDepth;
		eInitialLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
	}
	
	if (xInfo.m_uMemoryFlags & 1 << MEMORY_FLAGS__SHADER_READ)
		eUsageFlags |= vk::ImageUsageFlagBits::eSampled;

	// Determine image type and extent based on texture type
	vk::ImageType eImageType = vk::ImageType::e2D;
	vk::Extent3D xExtent = { xInfo.m_uWidth, xInfo.m_uHeight, 1 };
	vk::ImageCreateFlags eCreateFlags = {};

	if (xInfo.m_eTextureType == TEXTURE_TYPE_3D)
	{
		eImageType = vk::ImageType::e3D;
		xExtent = vk::Extent3D(xInfo.m_uWidth, xInfo.m_uHeight, xInfo.m_uDepth);
	}
	else if (xInfo.m_eTextureType == TEXTURE_TYPE_CUBE)
	{
		// Cubemaps require the cube-compatible flag
		eCreateFlags |= vk::ImageCreateFlagBits::eCubeCompatible;
	}

	vk::ImageCreateInfo xImageInfo = vk::ImageCreateInfo()
		.setImageType(eImageType)
		.setFormat(xFormat)
		.setTiling(vk::ImageTiling::eOptimal)
		.setExtent(xExtent)
		.setMipLevels(xInfo.m_uNumMips)
		.setArrayLayers(xInfo.m_uNumLayers)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setUsage(eUsageFlags)
		.setSharingMode(vk::SharingMode::eExclusive)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setFlags(eCreateFlags);

	VmaAllocationCreateInfo xAllocInfo = {};
	xAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	const vk::ImageCreateInfo::NativeType xImageInfo_Native = xImageInfo;

	VkImage xImage = VK_NULL_HANDLE;
	VmaAllocation xAllocation = VK_NULL_HANDLE;
	VkResult eResult = vmaCreateImage(g_xEngine.VulkanMemory().m_xAllocator, &xImageInfo_Native, &xAllocInfo, &xImage, &xAllocation, nullptr);
	Zenith_Assert(eResult == VK_SUCCESS, "vmaCreateImage failed with result %d", static_cast<int>(eResult));
	if (eResult != VK_SUCCESS)
	{
		// Return invalid handle on allocation failure
		return Flux_VRAMHandle();
	}

	Zenith_Vulkan_VRAM* pxVRAM = new Zenith_Vulkan_VRAM(vk::Image(xImage), xAllocation, g_xEngine.VulkanMemory().m_xAllocator);
	Flux_VRAMHandle xHandle = g_xEngine.Vulkan().RegisterVRAM(pxVRAM);

	if (bIsDepthStencil)
	{
		Zenith_Assert(xInfo.m_eFormat == TEXTURE_FORMAT_D32_SFLOAT, "#TO_TODO: layouts for just depth without stencil");
	}

	// Transition all layers to initial layout (important for cubemaps/array textures)
	for (uint32_t uLayer = 0; uLayer < xInfo.m_uNumLayers; uLayer++)
	{
		ImageTransitionBarrier(vk::Image(xImage), vk::ImageLayout::eUndefined, eInitialLayout, eAspectFlags, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, 0, uLayer);
	}

	return xHandle;
}

void Zenith_Vulkan_MemoryManager::NormalizeTextureInfo(Flux_SurfaceInfo& xInfo, bool bCreateMips)
{
	xInfo.m_uNumMips = bCreateMips
		? static_cast<u_int>(std::floor(std::log2((std::max)(xInfo.m_uWidth, xInfo.m_uHeight))) + 1)
		: 1;
	// Clamp depth/layers to a min of 1 so the downstream byte-size math doesn't
	// silently produce zero (which would mask uninitialised input fields).
	xInfo.m_uDepth = std::max(1u, xInfo.m_uDepth);
	xInfo.m_uNumLayers = std::max(1u, xInfo.m_uNumLayers);
}

vk::ImageCreateInfo Zenith_Vulkan_MemoryManager::BuildImageCreateInfo(const Flux_SurfaceInfo& xInfo)
{
	vk::Format xFormat = g_xEngine.Vulkan().ConvertToVkFormat_Colour(xInfo.m_eFormat);

	vk::ImageUsageFlags eUsageFlags = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc;
	if (xInfo.m_uMemoryFlags & 1 << MEMORY_FLAGS__SHADER_READ) eUsageFlags |= vk::ImageUsageFlagBits::eSampled;
	if (xInfo.m_uMemoryFlags & 1 << MEMORY_FLAGS__UNORDERED_ACCESS) eUsageFlags |= vk::ImageUsageFlagBits::eStorage;

	vk::ImageType eImageType = vk::ImageType::e2D;
	vk::Extent3D xExtent = { xInfo.m_uWidth, xInfo.m_uHeight, 1 };
	if (xInfo.m_eTextureType == TEXTURE_TYPE_3D)
	{
		eImageType = vk::ImageType::e3D;
		xExtent = vk::Extent3D(xInfo.m_uWidth, xInfo.m_uHeight, xInfo.m_uDepth);
	}

	vk::ImageCreateInfo xImageInfo = vk::ImageCreateInfo()
		.setImageType(eImageType)
		.setFormat(xFormat)
		.setTiling(vk::ImageTiling::eOptimal)
		.setExtent(xExtent)
		.setMipLevels(xInfo.m_uNumMips)
		.setArrayLayers(xInfo.m_uNumLayers)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setUsage(eUsageFlags)
		.setSharingMode(vk::SharingMode::eExclusive)
		.setSamples(vk::SampleCountFlagBits::e1);

	if (xInfo.m_eTextureType == TEXTURE_TYPE_CUBE || xInfo.m_uNumLayers == 6)
	{
		xImageInfo.setFlags(vk::ImageCreateFlagBits::eCubeCompatible);
	}
	return xImageInfo;
}

Flux_VRAMHandle Zenith_Vulkan_MemoryManager::AllocateAndRegisterImage(const vk::ImageCreateInfo& xImageInfo,
	VkImage& xImageOut, VmaAllocation& xAllocationOut)
{
	// Headless guard: see CreateBufferVRAM for rationale.
	if (g_xEngine.VulkanMemory().m_xAllocator == VK_NULL_HANDLE)
	{
		xImageOut = VK_NULL_HANDLE;
		xAllocationOut = VK_NULL_HANDLE;
		return Flux_VRAMHandle();
	}

	VmaAllocationCreateInfo xAllocInfo = {};
	xAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	const vk::ImageCreateInfo::NativeType xImageInfo_Native = xImageInfo;

	xImageOut = VK_NULL_HANDLE;
	xAllocationOut = VK_NULL_HANDLE;
	VkResult eResult = vmaCreateImage(g_xEngine.VulkanMemory().m_xAllocator, &xImageInfo_Native, &xAllocInfo, &xImageOut, &xAllocationOut, nullptr);
	Zenith_Assert(eResult == VK_SUCCESS, "vmaCreateImage failed with result %d", static_cast<int>(eResult));
	if (eResult != VK_SUCCESS)
	{
		return Flux_VRAMHandle();
	}

	Zenith_Vulkan_VRAM* pxVRAM = new Zenith_Vulkan_VRAM(vk::Image(xImageOut), xAllocationOut, g_xEngine.VulkanMemory().m_xAllocator);
	return g_xEngine.Vulkan().RegisterVRAM(pxVRAM);
}

void Zenith_Vulkan_MemoryManager::UploadTextureData(VkImage xImage, VmaAllocation xAllocation,
	const void* pData, const Flux_SurfaceInfo& xInfo, size_t ulDataSize)
{
	// Headless guard: see CreateBufferVRAM for rationale.
	if (g_xEngine.VulkanMemory().m_xAllocator == VK_NULL_HANDLE)
	{
		return;
	}

	const vk::Device& xDevice = g_xEngine.Vulkan().GetDevice();

	g_xEngine.VulkanMemory().m_xMutex.Lock();

	VkMemoryPropertyFlags eMemoryProps;
	vmaGetAllocationMemoryProperties(g_xEngine.VulkanMemory().m_xAllocator, xAllocation, &eMemoryProps);

	if (eMemoryProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		// Direct upload to host-visible memory.
		VmaAllocationInfo xImageAllocInfo;
		vmaGetAllocationInfo(g_xEngine.VulkanMemory().m_xAllocator, xAllocation, &xImageAllocInfo);
		memcpy(xImageAllocInfo.pMappedData, pData, ulDataSize);
		vmaFlushAllocation(g_xEngine.VulkanMemory().m_xAllocator, xAllocation, 0, VK_WHOLE_SIZE);
	}
	else if (ulDataSize > g_uStagingPoolSize)
	{
		// Allocation larger than the staging buffer — unlock and chunk.
		g_xEngine.VulkanMemory().m_xMutex.Unlock();
		UploadTextureDataChunked(vk::Image(xImage), pData, ulDataSize, xInfo.m_uWidth, xInfo.m_uHeight, xInfo.m_uNumMips, xInfo.m_uNumLayers);
		return;
	}
	else
	{
		// Upload via the current frame's staging slot. CurrentStaging() resolves
		// to g_xEngine.VulkanMemory().m_axStaging[CurrentFrameIndex], so the bump-allocate + memcpy +
		// queue-copy sequence below only ever touches memory the GPU is allowed
		// to consume in this frame.
		PerFrameStaging& xStaging = CurrentStaging();
		if (xStaging.m_uNextFreeOffset + ulDataSize > g_uStagingPoolSize)
		{
			// HandleStagingBufferFull does an EndFrame(false) -> EndAndCpuWait
			// which blocks for multi-second GPU work. Holding g_xEngine.VulkanMemory().m_xMutex across
			// that wait stalls every other thread doing uploads (smoke runs
			// showed 20-second "Wait for Mutex" events from worker threads
			// piling up here). Release the mutex during the GPU wait.
			g_xEngine.VulkanMemory().m_xMutex.Unlock();
			HandleStagingBufferFull();
			g_xEngine.VulkanMemory().m_xMutex.Lock();
		}

		StagingMemoryAllocation xStagingAlloc;
		xStagingAlloc.m_eType = ALLOCATION_TYPE_TEXTURE;
		xStagingAlloc.m_xTextureMetadata.m_xImage = vk::Image(xImage);
		xStagingAlloc.m_xTextureMetadata.m_uWidth = xInfo.m_uWidth;
		xStagingAlloc.m_xTextureMetadata.m_uHeight = xInfo.m_uHeight;
		xStagingAlloc.m_xTextureMetadata.m_uDepth = xInfo.m_uDepth;
		xStagingAlloc.m_xTextureMetadata.m_uNumMips = xInfo.m_uNumMips;
		xStagingAlloc.m_xTextureMetadata.m_uNumLayers = xInfo.m_uNumLayers;
		xStagingAlloc.m_xTextureMetadata.m_eFormat = xInfo.m_eFormat;
		xStagingAlloc.m_uSize = ulDataSize;
		// Re-resolve staging slot in case HandleStagingBufferFull recycled it
		// (m_uNextFreeOffset is back to 0 if so).
		PerFrameStaging& xStagingAfter = CurrentStaging();
		xStagingAlloc.m_uOffset = xStagingAfter.m_uNextFreeOffset;
		xStagingAfter.m_xAllocations.PushBack(xStagingAlloc);

		void* pMap = VkUnwrap(xDevice.mapMemory(xStagingAfter.m_xMemory, xStagingAfter.m_uNextFreeOffset, ulDataSize));
		memcpy(pMap, pData, ulDataSize);
		xDevice.unmapMemory(xStagingAfter.m_xMemory);
		xStagingAfter.m_uNextFreeOffset += ulDataSize;
		// Align to 16 bytes — the maximum texel block size we ship (BC3 / BC5 /
		// BC7 are all 16-byte blocks, and Vulkan requires bufferOffset to be a
		// multiple of the texel block size on vkCmdCopyBufferToImage). 8 was
		// only enough for BC1 (8-byte block) and got away with BC3 by luck
		// when prior allocations happened to leave the cursor 16-aligned.
		xStagingAfter.m_uNextFreeOffset = ALIGN(xStagingAfter.m_uNextFreeOffset, 16);
		if (xStagingAfter.m_uNextFreeOffset > xStagingAfter.m_uHighWaterMark)
			xStagingAfter.m_uHighWaterMark = xStagingAfter.m_uNextFreeOffset;
	}
	g_xEngine.VulkanMemory().m_xMutex.Unlock();
}

Flux_VRAMHandle Zenith_Vulkan_MemoryManager::CreateTextureVRAM(const void* pData, const Flux_SurfaceInfo& xInfo, bool bCreateMips)
{
	Flux_SurfaceInfo xInfoCopy = xInfo;
	NormalizeTextureInfo(xInfoCopy, bCreateMips);

	const vk::ImageCreateInfo xImageInfo = BuildImageCreateInfo(xInfoCopy);

	VkImage xImage = VK_NULL_HANDLE;
	VmaAllocation xAllocation = VK_NULL_HANDLE;
	Flux_VRAMHandle xHandle = AllocateAndRegisterImage(xImageInfo, xImage, xAllocation);
	if (!xHandle.IsValid())
	{
		return xHandle;  // Allocation failure already asserted inside the helper.
	}

	if (pData)
	{
		size_t ulDataSize;
		if (IsCompressedFormat(xInfoCopy.m_eFormat))
		{
			ulDataSize = CalculateCompressedTextureSize(xInfoCopy.m_eFormat, xInfoCopy.m_uWidth, xInfoCopy.m_uHeight) * xInfoCopy.m_uNumLayers;
		}
		else
		{
			ulDataSize = ColourFormatBytesPerPixel(xInfoCopy.m_eFormat) * xInfoCopy.m_uWidth * xInfoCopy.m_uHeight * xInfoCopy.m_uDepth * xInfoCopy.m_uNumLayers;
		}
		UploadTextureData(xImage, xAllocation, pData, xInfoCopy, ulDataSize);
	}
	else
	{
		// No data to upload — the caller will fill via render passes / compute.
		// Transition directly to shader-read so the next sampler bind works
		// without an explicit barrier in calling code.
		ImageTransitionBarrier(vk::Image(xImage), vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands);
	}

	return xHandle;
}

vk::ImageViewType Zenith_Vulkan_MemoryManager::DetermineImageViewType(const Flux_SurfaceInfo& xInfo)
{
	const bool bIs3D = xInfo.m_eTextureType == TEXTURE_TYPE_3D;
	const bool bIsCube = xInfo.m_eTextureType == TEXTURE_TYPE_CUBE || xInfo.m_uNumLayers == 6;

	if (bIs3D)
		return vk::ImageViewType::e3D;
	if (bIsCube)
		return vk::ImageViewType::eCube;

	return vk::ImageViewType::e2D;
}

Flux_RenderTargetView Zenith_Vulkan_MemoryManager::CreateRenderTargetView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel)
{
	Zenith_Assert(uMipLevel < (xInfo.m_uNumMips == 0 ? 1u : xInfo.m_uNumMips),
		"CreateRenderTargetView: uMipLevel %u out of range (surface has %u mips)", uMipLevel, xInfo.m_uNumMips);

	Flux_RenderTargetView xView;
	xView.m_xVRAMHandle = xVRAMHandle;

	const vk::Device& xDevice = g_xEngine.Vulkan().GetDevice();
	Zenith_Vulkan_VRAM* pxVRAM = g_xEngine.Vulkan().GetVRAM(xVRAMHandle);
	Zenith_Assert(pxVRAM != nullptr || Zenith_CommandLine::IsHeadless(), "GetVRAM returned null in CreateRenderTargetView");
	if (!pxVRAM) return xView;  // Safety guard for release builds

	vk::Format xFormat = g_xEngine.Vulkan().ConvertToVkFormat_Colour(xInfo.m_eFormat);

	// Determine view type based on texture type
	const bool bIs3D = xInfo.m_eTextureType == TEXTURE_TYPE_3D;
	const bool bIsCube = xInfo.m_eTextureType == TEXTURE_TYPE_CUBE || xInfo.m_uNumLayers == 6;
	const uint32_t uLayerCount = bIsCube ? 6 : (xInfo.m_uNumLayers > 0 ? xInfo.m_uNumLayers : 1);

	vk::ImageViewType eViewType = DetermineImageViewType(xInfo);

	vk::ImageSubresourceRange xSubresourceRange = vk::ImageSubresourceRange()
		.setAspectMask(vk::ImageAspectFlagBits::eColor)
		.setBaseMipLevel(uMipLevel)
		.setLevelCount(1)
		.setBaseArrayLayer(0)
		.setLayerCount(bIs3D ? 1 : uLayerCount);

	vk::ImageViewCreateInfo xViewCreate = vk::ImageViewCreateInfo()
		.setImage(pxVRAM->GetImage())
		.setViewType(eViewType)
		.setFormat(xFormat)
		.setSubresourceRange(xSubresourceRange);

	vk::ImageView xVkView = VkUnwrap(xDevice.createImageView(xViewCreate));
	xView.m_xImageViewHandle = RegisterImageView(xVkView);
	return xView;
}

Flux_RenderTargetView Zenith_Vulkan_MemoryManager::CreateRenderTargetViewForLayer(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uLayer, uint32_t uMipLevel)
{
	Flux_RenderTargetView xView;
	xView.m_xVRAMHandle = xVRAMHandle;

	const vk::Device& xDevice = g_xEngine.Vulkan().GetDevice();
	Zenith_Vulkan_VRAM* pxVRAM = g_xEngine.Vulkan().GetVRAM(xVRAMHandle);
	Zenith_Assert(pxVRAM != nullptr || Zenith_CommandLine::IsHeadless(), "GetVRAM returned null in CreateRenderTargetViewForLayer");
	if (!pxVRAM) return xView;

	vk::Format xFormat = g_xEngine.Vulkan().ConvertToVkFormat_Colour(xInfo.m_eFormat);

	// Create a 2D view for a single layer/face of a cubemap or array texture
	vk::ImageSubresourceRange xSubresourceRange = vk::ImageSubresourceRange()
		.setAspectMask(vk::ImageAspectFlagBits::eColor)
		.setBaseMipLevel(uMipLevel)
		.setLevelCount(1)
		.setBaseArrayLayer(uLayer)
		.setLayerCount(1);

	vk::ImageViewCreateInfo xViewCreate = vk::ImageViewCreateInfo()
		.setImage(pxVRAM->GetImage())
		.setViewType(vk::ImageViewType::e2D)
		.setFormat(xFormat)
		.setSubresourceRange(xSubresourceRange);

	vk::ImageView xVkView = VkUnwrap(xDevice.createImageView(xViewCreate));
	xView.m_xImageViewHandle = RegisterImageView(xVkView);
	return xView;
}

Flux_DepthStencilView Zenith_Vulkan_MemoryManager::CreateDepthStencilView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel)
{
	Flux_DepthStencilView xView;
	xView.m_xVRAMHandle = xVRAMHandle;

	const vk::Device& xDevice = g_xEngine.Vulkan().GetDevice();
	Zenith_Vulkan_VRAM* pxVRAM = g_xEngine.Vulkan().GetVRAM(xVRAMHandle);
	Zenith_Assert(pxVRAM != nullptr || Zenith_CommandLine::IsHeadless(), "GetVRAM returned null in CreateDepthStencilView");
	if (!pxVRAM) return xView;  // Safety guard for release builds

	vk::Format xFormat = g_xEngine.Vulkan().ConvertToVkFormat_DepthStencil(xInfo.m_eFormat);

	const bool bIsCube = xInfo.m_uNumLayers == 6;
	const uint32_t uLayerCount = bIsCube ? 6 : (xInfo.m_uNumLayers > 0 ? xInfo.m_uNumLayers : 1);
	vk::ImageSubresourceRange xSubresourceRange = vk::ImageSubresourceRange()
		.setAspectMask(vk::ImageAspectFlagBits::eDepth)
		.setBaseMipLevel(uMipLevel)
		.setLevelCount(1)
		.setBaseArrayLayer(0)
		.setLayerCount(uLayerCount);

	vk::ImageViewCreateInfo xViewCreate = vk::ImageViewCreateInfo()
		.setImage(pxVRAM->GetImage())
		.setViewType(bIsCube ? vk::ImageViewType::eCube : vk::ImageViewType::e2D)
		.setFormat(xFormat)
		.setSubresourceRange(xSubresourceRange);

	vk::ImageView xVkView = VkUnwrap(xDevice.createImageView(xViewCreate));
	xView.m_xImageViewHandle = RegisterImageView(xVkView);
	return xView;
}

Flux_ShaderResourceView Zenith_Vulkan_MemoryManager::CreateShaderResourceView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uBaseMip, uint32_t uMipCount)
{
	Flux_ShaderResourceView xView;
	xView.m_xVRAMHandle = xVRAMHandle;

	const vk::Device& xDevice = g_xEngine.Vulkan().GetDevice();
	Zenith_Vulkan_VRAM* pxVRAM = g_xEngine.Vulkan().GetVRAM(xVRAMHandle);
	Zenith_Assert(pxVRAM != nullptr || Zenith_CommandLine::IsHeadless(), "GetVRAM returned null in CreateShaderResourceView");
	if (!pxVRAM) return xView;  // Safety guard for release builds

	const bool bIsDepth = xInfo.m_eFormat > TEXTURE_FORMAT_DEPTH_STENCIL_BEGIN && xInfo.m_eFormat < TEXTURE_FORMAT_DEPTH_STENCIL_END;
	vk::Format xFormat = bIsDepth ? g_xEngine.Vulkan().ConvertToVkFormat_DepthStencil(xInfo.m_eFormat) : g_xEngine.Vulkan().ConvertToVkFormat_Colour(xInfo.m_eFormat);

	// Determine view type based on texture type
	const bool bIs3D = xInfo.m_eTextureType == TEXTURE_TYPE_3D;
	const bool bIsCube = xInfo.m_eTextureType == TEXTURE_TYPE_CUBE || xInfo.m_uNumLayers == 6;
	const uint32_t uLayerCount = bIsCube ? 6 : (xInfo.m_uNumLayers > 0 ? xInfo.m_uNumLayers : 1);

	vk::ImageViewType eViewType = DetermineImageViewType(xInfo);

	vk::ImageSubresourceRange xSubresourceRange = vk::ImageSubresourceRange()
		.setAspectMask(bIsDepth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor)
		.setBaseMipLevel(uBaseMip)
		.setLevelCount(uMipCount)
		.setBaseArrayLayer(0)
		.setLayerCount(bIs3D ? 1 : uLayerCount);

	vk::ImageViewCreateInfo xViewCreate = vk::ImageViewCreateInfo()
		.setImage(pxVRAM->GetImage())
		.setViewType(eViewType)
		.setFormat(xFormat)
		.setSubresourceRange(xSubresourceRange);

	vk::ImageView xVkView = VkUnwrap(xDevice.createImageView(xViewCreate));
	xView.m_xImageViewHandle = RegisterImageView(xVkView);
	xView.m_bIsDepthStencil = bIsDepth;
	xView.m_uBaseMip = uBaseMip;    // Store mip level for barrier tracking
	xView.m_uMipCount = uMipCount;  // Store mip count for barrier tracking

	if ((xInfo.m_uMemoryFlags & (1 << MEMORY_FLAGS__BINDLESS)) && xView.m_xImageViewHandle.IsValid())
	{
		g_xEngine.Vulkan().WriteBindlessDescriptor(
			xView.m_xImageViewHandle.AsUInt(),
			xVkView,
			g_xEngine.FluxGraphics().m_xClampSampler.GetSampler());
	}

	return xView;
}

Flux_ShaderResourceView Zenith_Vulkan_MemoryManager::CreateShaderResourceViewForLayer(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uLayer, uint32_t uBaseMip, uint32_t uMipCount)
{
	Flux_ShaderResourceView xView;
	xView.m_xVRAMHandle = xVRAMHandle;

	const vk::Device& xDevice = g_xEngine.Vulkan().GetDevice();
	Zenith_Vulkan_VRAM* pxVRAM = g_xEngine.Vulkan().GetVRAM(xVRAMHandle);
	Zenith_Assert(pxVRAM != nullptr || Zenith_CommandLine::IsHeadless(), "GetVRAM returned null in CreateShaderResourceViewForLayer");
	if (!pxVRAM) return xView;

	const bool bIsDepth = xInfo.m_eFormat > TEXTURE_FORMAT_DEPTH_STENCIL_BEGIN && xInfo.m_eFormat < TEXTURE_FORMAT_DEPTH_STENCIL_END;
	vk::Format xFormat = bIsDepth ? g_xEngine.Vulkan().ConvertToVkFormat_DepthStencil(xInfo.m_eFormat) : g_xEngine.Vulkan().ConvertToVkFormat_Colour(xInfo.m_eFormat);

	// Create a 2D view for a single layer/face of a cubemap or array texture
	vk::ImageSubresourceRange xSubresourceRange = vk::ImageSubresourceRange()
		.setAspectMask(bIsDepth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor)
		.setBaseMipLevel(uBaseMip)
		.setLevelCount(uMipCount)
		.setBaseArrayLayer(uLayer)
		.setLayerCount(1);

	vk::ImageViewCreateInfo xViewCreate = vk::ImageViewCreateInfo()
		.setImage(pxVRAM->GetImage())
		.setViewType(vk::ImageViewType::e2D)
		.setFormat(xFormat)
		.setSubresourceRange(xSubresourceRange);

	vk::ImageView xVkView = VkUnwrap(xDevice.createImageView(xViewCreate));
	xView.m_xImageViewHandle = RegisterImageView(xVkView);
	xView.m_bIsDepthStencil = bIsDepth;
	xView.m_uBaseMip = uBaseMip;    // Store mip level for barrier tracking
	xView.m_uMipCount = uMipCount;  // Store mip count for barrier tracking

	if ((xInfo.m_uMemoryFlags & (1 << MEMORY_FLAGS__BINDLESS)) && xView.m_xImageViewHandle.IsValid())
	{
		g_xEngine.Vulkan().WriteBindlessDescriptor(
			xView.m_xImageViewHandle.AsUInt(),
			xVkView,
			g_xEngine.FluxGraphics().m_xClampSampler.GetSampler());
	}

	return xView;
}

Flux_UnorderedAccessView_Texture Zenith_Vulkan_MemoryManager::CreateUnorderedAccessView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel)
{
	Flux_UnorderedAccessView_Texture xView;
	xView.m_xVRAMHandle = xVRAMHandle;

	const vk::Device& xDevice = g_xEngine.Vulkan().GetDevice();
	Zenith_Vulkan_VRAM* pxVRAM = g_xEngine.Vulkan().GetVRAM(xVRAMHandle);
	Zenith_Assert(pxVRAM != nullptr || Zenith_CommandLine::IsHeadless(), "GetVRAM returned null in CreateUnorderedAccessView");
	if (!pxVRAM) return xView;  // Safety guard for release builds

	vk::Format xFormat = g_xEngine.Vulkan().ConvertToVkFormat_Colour(xInfo.m_eFormat);

	// Determine view type based on texture type
	const bool bIs3D = xInfo.m_eTextureType == TEXTURE_TYPE_3D;
	const bool bIsCube = xInfo.m_eTextureType == TEXTURE_TYPE_CUBE || xInfo.m_uNumLayers == 6;
	const uint32_t uLayerCount = bIsCube ? 6 : (xInfo.m_uNumLayers > 0 ? xInfo.m_uNumLayers : 1);

	vk::ImageViewType eViewType = DetermineImageViewType(xInfo);

	vk::ImageSubresourceRange xSubresourceRange = vk::ImageSubresourceRange()
		.setAspectMask(vk::ImageAspectFlagBits::eColor)
		.setBaseMipLevel(uMipLevel)
		.setLevelCount(1)
		.setBaseArrayLayer(0)
		.setLayerCount(bIs3D ? 1 : uLayerCount);

	vk::ImageViewCreateInfo xViewCreate = vk::ImageViewCreateInfo()
		.setImage(pxVRAM->GetImage())
		.setViewType(eViewType)
		.setFormat(xFormat)
		.setSubresourceRange(xSubresourceRange);

	vk::ImageView xVkView = VkUnwrap(xDevice.createImageView(xViewCreate));
	xView.m_xImageViewHandle = RegisterImageView(xVkView);
	xView.m_uMipLevel = uMipLevel;  // Store mip level for barrier tracking
	return xView;
}

Flux_UnorderedAccessView_Texture Zenith_Vulkan_MemoryManager::CreateUnorderedAccessViewForSlice(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uSlice, uint32_t uMipLevel)
{
	Flux_UnorderedAccessView_Texture xView;
	xView.m_xVRAMHandle = xVRAMHandle;
	xView.m_uMipLevel = uMipLevel;

	const vk::Device& xDevice = g_xEngine.Vulkan().GetDevice();
	Zenith_Vulkan_VRAM* pxVRAM = g_xEngine.Vulkan().GetVRAM(xVRAMHandle);
	Zenith_Assert(pxVRAM != nullptr || Zenith_CommandLine::IsHeadless(), "GetVRAM returned null in CreateUnorderedAccessViewForSlice");
	if (!pxVRAM) return xView;

	vk::Format xFormat = g_xEngine.Vulkan().ConvertToVkFormat_Colour(xInfo.m_eFormat);

	vk::ImageSubresourceRange xSubresourceRange = vk::ImageSubresourceRange()
		.setAspectMask(vk::ImageAspectFlagBits::eColor)
		.setBaseMipLevel(uMipLevel)
		.setLevelCount(1)
		.setBaseArrayLayer(uSlice)
		.setLayerCount(1);

	vk::ImageViewCreateInfo xViewCreate = vk::ImageViewCreateInfo()
		.setImage(pxVRAM->GetImage())
		.setViewType(vk::ImageViewType::e2D)
		.setFormat(xFormat)
		.setSubresourceRange(xSubresourceRange);

	vk::ImageView xVkView = VkUnwrap(xDevice.createImageView(xViewCreate));
	xView.m_xImageViewHandle = RegisterImageView(xVkView);
	return xView;
}

void Zenith_Vulkan_MemoryManager::UploadBufferData(Flux_VRAMHandle xBufferHandle, const void* pData, size_t uSize)
{
	Zenith_Profiling::Scope xProfileScope(ZENITH_PROFILE_INDEX__VULKAN_MEMORY_MANAGER_UPLOAD);

	// Headless guard: see CreateBufferVRAM for rationale. With no allocator, all
	// upstream CreateBufferVRAM calls returned invalid handles, so GetVRAM would
	// hit the null-assert below — bail before that.
	if (g_xEngine.VulkanMemory().m_xAllocator == VK_NULL_HANDLE)
	{
		return;
	}

	g_xEngine.VulkanMemory().m_xMutex.Lock();
	const vk::Device& xDevice = g_xEngine.Vulkan().GetDevice();

	Zenith_Vulkan_VRAM* pxVRAM = g_xEngine.Vulkan().GetVRAM(xBufferHandle);
	Zenith_Assert(pxVRAM != nullptr || Zenith_CommandLine::IsHeadless(), "GetVRAM returned null in UploadBufferData");
	if (!pxVRAM)
	{
		g_xEngine.VulkanMemory().m_xMutex.Unlock();
		return;  // Safety guard for release builds
	}
	const VmaAllocation& xAlloc = pxVRAM->GetAllocation();
	VkMemoryPropertyFlags eMemoryProps;
	vmaGetAllocationMemoryProperties(g_xEngine.VulkanMemory().m_xAllocator, xAlloc, &eMemoryProps);

	if (eMemoryProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		void* pMap = nullptr;
		vmaMapMemory(g_xEngine.VulkanMemory().m_xAllocator, xAlloc, &pMap);
		Zenith_Assert(pMap != nullptr, "Memory isn't mapped");
		memcpy(pMap, pData, uSize);
#ifdef ZENITH_ASSERT
		VkResult eResult =
#endif
		vmaFlushAllocation(g_xEngine.VulkanMemory().m_xAllocator, xAlloc, 0, uSize);
		Zenith_Assert(eResult == VK_SUCCESS, "Failed to flush allocation");

		vmaUnmapMemory(g_xEngine.VulkanMemory().m_xAllocator, xAlloc);
	}
	else
	{
		// If the allocation is larger than the entire staging buffer, use chunked upload
		if (uSize > g_uStagingPoolSize)
		{
			g_xEngine.VulkanMemory().m_xMutex.Unlock();
			UploadBufferDataChunked(pxVRAM->GetBuffer(), pData, uSize);
			return;
		}

		PerFrameStaging& xStaging = CurrentStaging();
		if (xStaging.m_uNextFreeOffset + uSize > g_uStagingPoolSize)
		{
			// HandleStagingBufferFull blocks multi-seconds on GPU; release
			// the mutex so other threads' uploads aren't stalled. See
			// matching comment in UploadTextureData above.
			g_xEngine.VulkanMemory().m_xMutex.Unlock();
			HandleStagingBufferFull();
			g_xEngine.VulkanMemory().m_xMutex.Lock();
		}

		// Re-resolve staging slot in case HandleStagingBufferFull recycled it.
		PerFrameStaging& xStagingAfter = CurrentStaging();
		StagingMemoryAllocation xAllocation;
		xAllocation.m_eType = ALLOCATION_TYPE_BUFFER;
		xAllocation.m_xBufferMetadata.m_xBuffer = pxVRAM->GetBuffer();
		xAllocation.m_xBufferMetadata.m_uDestOffset = 0;
		xAllocation.m_uSize = uSize;
		xAllocation.m_uOffset = xStagingAfter.m_uNextFreeOffset;
		xStagingAfter.m_xAllocations.PushBack(xAllocation);

		void* pMap = VkUnwrap(xDevice.mapMemory(xStagingAfter.m_xMemory, xStagingAfter.m_uNextFreeOffset, uSize));
		memcpy(pMap, pData, uSize);
		xDevice.unmapMemory(xStagingAfter.m_xMemory);
		xStagingAfter.m_uNextFreeOffset += uSize;
		// Align to 16 bytes — see InitialiseTextureFromData for the rationale.
		// Buffer uploads are followed by texture uploads in the same staging
		// pool, so any allocator that leaves the cursor sub-16-aligned will
		// break vkCmdCopyBufferToImage for BC3/BC5/BC7.
		xStagingAfter.m_uNextFreeOffset = ALIGN(xStagingAfter.m_uNextFreeOffset, 16);
		if (xStagingAfter.m_uNextFreeOffset > xStagingAfter.m_uHighWaterMark)
			xStagingAfter.m_uHighWaterMark = xStagingAfter.m_uNextFreeOffset;
	}
	g_xEngine.VulkanMemory().m_xMutex.Unlock();
}

void Zenith_Vulkan_MemoryManager::DestroySimpleBuffer(Flux_VRAMHandle& xHandle)
{
	if (!xHandle.IsValid())
	{
		return;
	}

	Zenith_Vulkan_VRAM* pxVRAM = g_xEngine.Vulkan().GetVRAM(xHandle);
	if (pxVRAM)
	{
		QueueVRAMDeletion(pxVRAM, xHandle);
	}
}

void Zenith_Vulkan_MemoryManager::DestroyVertexBuffer(Flux_VertexBuffer& xBuffer)
{
	Flux_VRAMHandle xHandle = xBuffer.GetBuffer().m_xVRAMHandle;
	DestroySimpleBuffer(xHandle);
	xBuffer.Reset();
}

void Zenith_Vulkan_MemoryManager::DestroyDynamicVertexBuffer(Flux_DynamicVertexBuffer& xBuffer)
{
	for (uint32_t u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		Flux_VRAMHandle xHandle = xBuffer.GetBufferForFrameInFlight(u).m_xVRAMHandle;
		DestroySimpleBuffer(xHandle);
	}
	xBuffer.Reset();
}

void Zenith_Vulkan_MemoryManager::DestroyIndexBuffer(Flux_IndexBuffer& xBuffer)
{
	Flux_VRAMHandle xHandle = xBuffer.GetBuffer().m_xVRAMHandle;
	DestroySimpleBuffer(xHandle);
	xBuffer.Reset();
}

void Zenith_Vulkan_MemoryManager::DestroyConstantBuffer(Flux_ConstantBuffer& xBuffer)
{
	Flux_VRAMHandle xHandle = xBuffer.GetBuffer().m_xVRAMHandle;
	DestroySimpleBuffer(xHandle);
	xBuffer.Reset();
}

void Zenith_Vulkan_MemoryManager::DestroyDynamicConstantBuffer(Flux_DynamicConstantBuffer& xBuffer)
{
	for (uint32_t u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		Flux_VRAMHandle xHandle = xBuffer.GetBufferForFrameInFlight(u).m_xVRAMHandle;
		DestroySimpleBuffer(xHandle);
	}
	xBuffer.Reset();
}

void Zenith_Vulkan_MemoryManager::DestroyIndirectBuffer(Flux_IndirectBuffer& xBuffer)
{
	Flux_VRAMHandle xHandle = xBuffer.GetBuffer().m_xVRAMHandle;
	DestroySimpleBuffer(xHandle);
	xBuffer.Reset();
}

void Zenith_Vulkan_MemoryManager::DestroyReadWriteBuffer(Flux_ReadWriteBuffer& xBuffer)
{
	Flux_VRAMHandle xHandle = xBuffer.GetBuffer().m_xVRAMHandle;
	DestroySimpleBuffer(xHandle);
	xBuffer.Reset();
}

void Zenith_Vulkan_MemoryManager::DestroyDynamicReadWriteBuffer(Flux_DynamicReadWriteBuffer& xBuffer)
{
	for (uint32_t u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		Flux_VRAMHandle xHandle = xBuffer.GetBufferForFrameInFlight(u).m_xVRAMHandle;
		DestroySimpleBuffer(xHandle);
	}
	xBuffer.Reset();
}

void Zenith_Vulkan_MemoryManager::UploadBufferDataAtOffset(Flux_VRAMHandle xBufferHandle, const void* pData, size_t uSize, size_t uDestOffset)
{
	Zenith_Profiling::Scope xProfileScope(ZENITH_PROFILE_INDEX__VULKAN_MEMORY_MANAGER_UPLOAD);

	// Headless guard: see CreateBufferVRAM for rationale.
	if (g_xEngine.VulkanMemory().m_xAllocator == VK_NULL_HANDLE)
	{
		return;
	}

	const vk::Device& xDevice = g_xEngine.Vulkan().GetDevice();
	Zenith_Vulkan_VRAM* pxVRAM = g_xEngine.Vulkan().GetVRAM(xBufferHandle);
	Zenith_Assert(pxVRAM != nullptr || Zenith_CommandLine::IsHeadless(), "GetVRAM returned null in UploadBufferDataAtOffset");
	if (!pxVRAM) return;  // Safety guard for release builds
	const VmaAllocation& xAlloc = pxVRAM->GetAllocation();
	VkMemoryPropertyFlags eMemoryProps;
	vmaGetAllocationMemoryProperties(g_xEngine.VulkanMemory().m_xAllocator, xAlloc, &eMemoryProps);

	// For host-visible memory, map and copy directly with offset
	if (eMemoryProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		g_xEngine.VulkanMemory().m_xMutex.Lock();
		void* pMap = nullptr;
		vmaMapMemory(g_xEngine.VulkanMemory().m_xAllocator, xAlloc, &pMap);
		Zenith_Assert(pMap != nullptr, "Memory isn't mapped");

		// Copy to the destination offset
		memcpy(static_cast<uint8_t*>(pMap) + uDestOffset, pData, uSize);

#ifdef ZENITH_ASSERT
		VkResult eResult =
#endif
		vmaFlushAllocation(g_xEngine.VulkanMemory().m_xAllocator, xAlloc, uDestOffset, uSize);
		Zenith_Assert(eResult == VK_SUCCESS, "Failed to flush allocation");

		vmaUnmapMemory(g_xEngine.VulkanMemory().m_xAllocator, xAlloc);
		g_xEngine.VulkanMemory().m_xMutex.Unlock();
	}
	else
	{
		if (uSize == 0)
			return;

		const uint8_t* pSrcData = static_cast<const uint8_t*>(pData);
		size_t uRemainingSize = uSize;
		size_t uCurrentSrcOffset = 0;
		size_t uCurrentDestOffset = uDestOffset;

		while (uRemainingSize > 0)
		{
			const size_t uChunkSize = (std::min)(uRemainingSize, g_uStagingPoolSize - 4096);

			g_xEngine.VulkanMemory().m_xMutex.Lock();

			PerFrameStaging& xStaging = CurrentStaging();
			if (xStaging.m_uNextFreeOffset + uChunkSize > g_uStagingPoolSize)
				HandleStagingBufferFull();

			// Re-resolve after the potential mid-frame flush — HandleStagingBufferFull
			// resets the slot's offset but keeps the slot identity, so the
			// reference is still valid.
			PerFrameStaging& xStagingPost = CurrentStaging();

			StagingMemoryAllocation xAllocation;
			xAllocation.m_eType = ALLOCATION_TYPE_BUFFER;
			xAllocation.m_xBufferMetadata.m_xBuffer = pxVRAM->GetBuffer();
			xAllocation.m_xBufferMetadata.m_uDestOffset = uCurrentDestOffset;
			xAllocation.m_uSize = uChunkSize;
			xAllocation.m_uOffset = xStagingPost.m_uNextFreeOffset;
			xStagingPost.m_xAllocations.PushBack(xAllocation);

			void* pMap = VkUnwrap(xDevice.mapMemory(xStagingPost.m_xMemory, xStagingPost.m_uNextFreeOffset, uChunkSize));
			memcpy(pMap, pSrcData + uCurrentSrcOffset, uChunkSize);
			xDevice.unmapMemory(xStagingPost.m_xMemory);

			xStagingPost.m_uNextFreeOffset += uChunkSize;
			// 16-byte alignment — see InitialiseTextureFromData / UploadBufferData.
			xStagingPost.m_uNextFreeOffset = ALIGN(xStagingPost.m_uNextFreeOffset, 16);
			if (xStagingPost.m_uNextFreeOffset > xStagingPost.m_uHighWaterMark)
				xStagingPost.m_uHighWaterMark = xStagingPost.m_uNextFreeOffset;

			g_xEngine.VulkanMemory().m_xMutex.Unlock();

			uCurrentSrcOffset += uChunkSize;
			uCurrentDestOffset += uChunkSize;
			uRemainingSize -= uChunkSize;
		}
	}
}

void Zenith_Vulkan_MemoryManager::GenerateMipmapsAndTransitionToShaderRead(vk::Image xImage, uint32_t uWidth, uint32_t uHeight, uint32_t uNumMips, uint32_t uLayer, bool bIsCompressed)
{
	// Mip 0 is already in TRANSFER_DST_OPTIMAL from the copy. Transition to TRANSFER_SRC for blit source.
	g_xEngine.VulkanMemory().m_xCommandBuffer.ImageTransitionBarrier(xImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, 0, uLayer);

	// Compressed formats (BC1, BC3, BC5, BC7) cannot use blit for mipmap generation;
	// they must have pre-generated mipmaps in the source data.
	if (!bIsCompressed)
	{
		for (uint32_t uMip = 1; uMip < uNumMips; uMip++)
		{
			std::array<vk::Offset3D, 2> axSrcOffsets;
			axSrcOffsets.at(0) = vk::Offset3D(0, 0, 0);
			axSrcOffsets.at(1) = vk::Offset3D(uWidth >> (uMip - 1), uHeight >> (uMip - 1), 1);

			vk::ImageSubresourceLayers xSrcSubresource = vk::ImageSubresourceLayers()
				.setAspectMask(vk::ImageAspectFlagBits::eColor)
				.setMipLevel(uMip - 1)
				.setBaseArrayLayer(uLayer)
				.setLayerCount(1);

			std::array<vk::Offset3D, 2> axDstOffsets;
			axDstOffsets.at(0) = vk::Offset3D(0, 0, 0);
			axDstOffsets.at(1) = vk::Offset3D(uWidth >> uMip, uHeight >> uMip, 1);

			vk::ImageSubresourceLayers xDstSubresource = vk::ImageSubresourceLayers()
				.setAspectMask(vk::ImageAspectFlagBits::eColor)
				.setMipLevel(uMip)
				.setBaseArrayLayer(uLayer)
				.setLayerCount(1);

			vk::ImageBlit xBlit = vk::ImageBlit()
				.setSrcOffsets(axSrcOffsets)
				.setSrcSubresource(xSrcSubresource)
				.setDstOffsets(axDstOffsets)
				.setDstSubresource(xDstSubresource);

			g_xEngine.VulkanMemory().m_xCommandBuffer.GetCurrentCmdBuffer().blitImage(xImage, vk::ImageLayout::eTransferSrcOptimal, xImage, vk::ImageLayout::eTransferDstOptimal, 1, &xBlit, vk::Filter::eLinear);
			g_xEngine.VulkanMemory().m_xCommandBuffer.ImageTransitionBarrier(xImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, uMip, uLayer);
		}
	}

	// Transition all mips to shader-read layout
	g_xEngine.VulkanMemory().m_xCommandBuffer.ImageTransitionBarrier(xImage, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, 0, uLayer);

	for (uint32_t uMip = 1; uMip < uNumMips; uMip++)
	{
		// Compressed mips 1+ are still in TRANSFER_DST (no blit was done).
		// Non-compressed mips were transitioned to TRANSFER_SRC after each blit.
		vk::ImageLayout eSrcLayout = bIsCompressed ? vk::ImageLayout::eTransferDstOptimal : vk::ImageLayout::eTransferSrcOptimal;
		g_xEngine.VulkanMemory().m_xCommandBuffer.ImageTransitionBarrier(xImage, eSrcLayout, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, uMip, uLayer);
	}
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

	EmitTransferWriteBarrier(g_xEngine.VulkanMemory().m_xCommandBuffer.GetCurrentCmdBuffer(), xMeta.m_xBuffer, uDestOffset, uSize);

	// Source comes from the current frame's staging slot — the same slot the
	// allocation was bumped from in UploadBufferData / UploadBufferDataAtOffset.
	vk::BufferCopy xCopyRegion(uSrcOffset, uDestOffset, uSize);
	g_xEngine.VulkanMemory().m_xCommandBuffer.GetCurrentCmdBuffer().copyBuffer(CurrentStaging().m_xBuffer, xMeta.m_xBuffer, xCopyRegion);
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
			g_xEngine.VulkanMemory().m_xCommandBuffer.ImageTransitionBarrier(xImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, uMip, uLayer);
		}
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

	g_xEngine.VulkanMemory().m_xCommandBuffer.GetCurrentCmdBuffer().copyBufferToImage(CurrentStaging().m_xBuffer, xImage, vk::ImageLayout::eTransferDstOptimal, 1, &region);

	// Generate mipmaps (via blit for non-compressed) and transition to shader-read
	const bool bIsCompressed = IsCompressedFormat(xMeta.m_eFormat);
	for (uint32_t uLayer = 0; uLayer < xMeta.m_uNumLayers; uLayer++)
	{
		GenerateMipmapsAndTransitionToShaderRead(xImage, xMeta.m_uWidth, xMeta.m_uHeight, xMeta.m_uNumMips, uLayer, bIsCompressed);
	}
}

void Zenith_Vulkan_MemoryManager::FlushStagingBuffer()
{
	Zenith_Profiling::Scope xProfileScope(ZENITH_PROFILE_INDEX__VULKAN_MEMORY_MANAGER_FLUSH);

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

// Callers MUST hold g_xEngine.VulkanMemory().m_xMutex and this function MUST NOT take it — EndFrame()
// and BeginFrame() below are the only callees and neither reacquires the
// mutex (FlushStagingBuffer and BeginRecording do GPU work lock-free). Adding
// a lock here, or to any function reached from here, will deadlock every
// staging upload path because Zenith_Mutex is non-recursive.
//
// EndFrame(false) flushes the *current slot's* pending allocations and ends
// the wrapper's command buffer; BeginFrame restarts recording. The slot
// identity (the chosen index in g_xEngine.VulkanMemory().m_axStaging) does not change across this —
// the swapchain's current frame index only advances on a real frame
// boundary, not when we mid-frame-flush. So callers that re-resolve
// CurrentStaging() after this returns get the same slot back, just with
// m_uNextFreeOffset reset to 0.
void Zenith_Vulkan_MemoryManager::HandleStagingBufferFull()
{
	const u_int uFrameSlot = g_xEngine.VulkanSwapchain().GetCurrentFrameIndex();
	PerFrameStaging& xSlot = g_xEngine.VulkanMemory().m_axStaging[uFrameSlot];
	xSlot.m_uMidFrameFlushCount++;
	Zenith_Log(LOG_CATEGORY_VULKAN,
		"Staging buffer full on slot %u (offset=%llu / pool=%llu, mid-frame flush #%u). Forcing mid-frame submit.",
		uFrameSlot,
		static_cast<unsigned long long>(xSlot.m_uNextFreeOffset),
		static_cast<unsigned long long>(g_uStagingPoolSize),
		xSlot.m_uMidFrameFlushCount);
	EndFrame(false);
	BeginFrame();
}

void Zenith_Vulkan_MemoryManager::UploadBufferDataChunked(vk::Buffer xDestBuffer, const void* pData, size_t uSize)
{
	Zenith_Profiling::Scope xProfileScope(ZENITH_PROFILE_INDEX__VULKAN_MEMORY_MANAGER_UPLOAD);
	Zenith_Log(LOG_CATEGORY_VULKAN, "Uploading large buffer in chunks: %llu bytes (staging buffer size: %llu bytes)", uSize, g_uStagingPoolSize);

	const vk::Device& xDevice = g_xEngine.Vulkan().GetDevice();
	const uint8_t* pSrcData = static_cast<const uint8_t*>(pData);
	size_t uRemainingSize = uSize;
	size_t uCurrentOffset = 0;

	// Process in chunks that fit in the staging buffer
	while (uRemainingSize > 0)
	{
		// Calculate chunk size (leave some headroom for alignment)
		const size_t uChunkSize = (std::min)(uRemainingSize, g_uStagingPoolSize - 4096);

		g_xEngine.VulkanMemory().m_xMutex.Lock();

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

		EmitTransferWriteBarrier(g_xEngine.VulkanMemory().m_xCommandBuffer.GetCurrentCmdBuffer(), xDestBuffer, uCurrentOffset, uChunkSize);

		vk::BufferCopy xCopyRegion(0, uCurrentOffset, uChunkSize);
		g_xEngine.VulkanMemory().m_xCommandBuffer.GetCurrentCmdBuffer().copyBuffer(xStaging.m_xBuffer, xDestBuffer, xCopyRegion);

		g_xEngine.VulkanMemory().m_xCommandBuffer.EndAndCpuWait(false);

		// Clear staging allocations after flush
		xStaging.m_xAllocations.Clear();
		xStaging.m_uNextFreeOffset = 0;

		// Move to next chunk
		uCurrentOffset += uChunkSize;
		uRemainingSize -= uChunkSize;

		// Restart command buffer for next chunk
		g_xEngine.VulkanMemory().m_xCommandBuffer.BeginRecording();

		g_xEngine.VulkanMemory().m_xMutex.Unlock();
	}

	Zenith_Log(LOG_CATEGORY_VULKAN, "Chunked buffer upload complete");
}

void Zenith_Vulkan_MemoryManager::UploadTextureDataChunked(vk::Image xDestImage, const void* pData, size_t uSize, uint32_t uWidth, uint32_t uHeight, uint32_t uNumMips, uint32_t uNumLayers)
{
	Zenith_Profiling::Scope xProfileScope(ZENITH_PROFILE_INDEX__VULKAN_MEMORY_MANAGER_UPLOAD);
	Zenith_Log(LOG_CATEGORY_VULKAN, "Uploading large texture in chunks: %llu bytes (staging buffer size: %llu bytes)", uSize, g_uStagingPoolSize);

	const vk::Device& xDevice = g_xEngine.Vulkan().GetDevice();
	const uint8_t* pSrcData = static_cast<const uint8_t*>(pData);
	size_t uCurrentOffset = 0;

	// For simplicity, chunk by scanlines to avoid partial row uploads
	// This assumes mip level 0 only for now (no mipmap support in chunked path)
	const size_t uBytesPerRow = uSize / (uHeight * uNumLayers);
	const size_t uRowsPerChunk = std::max(size_t(1), (g_uStagingPoolSize - 4096) / uBytesPerRow);
	const size_t uChunkHeight = std::min(size_t(uHeight), uRowsPerChunk);

	uint32_t uCurrentRow = 0;

	// Transition entire image to transfer dst layout first
	g_xEngine.VulkanMemory().m_xCommandBuffer.BeginRecording();
	for (uint32_t uLayer = 0; uLayer < uNumLayers; uLayer++)
	{
		for (uint32_t uMip = 0; uMip < uNumMips; uMip++)
		{
			g_xEngine.VulkanMemory().m_xCommandBuffer.ImageTransitionBarrier(xDestImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, uMip, uLayer);
		}
	}

	while (uCurrentRow < uHeight * uNumLayers)
	{
		const uint32_t uCurrentLayer = uCurrentRow / uHeight;
		const uint32_t uRowInLayer = uCurrentRow % uHeight;
		const uint32_t uRemainingRows = static_cast<uint32_t>(std::min(uChunkHeight, static_cast<size_t>(uHeight - uRowInLayer)));
		const size_t uChunkSize = uRemainingRows * uBytesPerRow;

		g_xEngine.VulkanMemory().m_xMutex.Lock();

		// Ensure the current slot's staging is empty before this chunk row
		if (CurrentStaging().m_uNextFreeOffset != 0)
		{
			g_xEngine.VulkanMemory().m_xMutex.Unlock();
			HandleStagingBufferFull();
			g_xEngine.VulkanMemory().m_xMutex.Lock();
		}

		PerFrameStaging& xStaging = CurrentStaging();

		// Map, copy, and unmap
		void* pMap = VkUnwrap(xDevice.mapMemory(xStaging.m_xMemory, 0, uChunkSize));
		memcpy(pMap, pSrcData + uCurrentOffset, uChunkSize);
		xDevice.unmapMemory(xStaging.m_xMemory);

		g_xEngine.VulkanMemory().m_xMutex.Unlock();

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

		g_xEngine.VulkanMemory().m_xCommandBuffer.GetCurrentCmdBuffer().copyBufferToImage(xStaging.m_xBuffer, xDestImage, vk::ImageLayout::eTransferDstOptimal, 1, &region);

		uCurrentOffset += uChunkSize;
		uCurrentRow += uRemainingRows;
	}

	// Generate mipmaps (non-compressed only) and transition to shader-read
	for (uint32_t uLayer = 0; uLayer < uNumLayers; uLayer++)
	{
		GenerateMipmapsAndTransitionToShaderRead(xDestImage, uWidth, uHeight, uNumMips, uLayer, false);
	}

	// Execute and wait
	g_xEngine.VulkanMemory().m_xCommandBuffer.EndAndCpuWait(false);

	// Clean up the current slot
	{
		PerFrameStaging& xStaging = CurrentStaging();
		xStaging.m_xAllocations.Clear();
		xStaging.m_uNextFreeOffset = 0;
	}

	// Restart command buffer for next operations
	g_xEngine.VulkanMemory().m_xCommandBuffer.BeginRecording();

	Zenith_Log(LOG_CATEGORY_VULKAN, "Chunked texture upload complete");
}

void Zenith_Vulkan_MemoryManager::QueueVRAMDeletion(Zenith_Vulkan_VRAM* pxVRAM, Flux_VRAMHandle& xHandle,
	Flux_ImageViewHandle xRTV, Flux_ImageViewHandle xDSV, Flux_ImageViewHandle xSRV, Flux_ImageViewHandle xUAV,
	u_int uExtraFrameDelay)
{
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
	g_xEngine.VulkanMemory().m_xPendingDeletions.PushBack(xDeletion);

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
	QueueVRAMDeletion(nullptr, xInvalidHandle, xImageViewHandle);
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
	const vk::Device& xDevice = g_xEngine.Vulkan().GetDevice();

	// Note on within-frame ordering: RemoveSwap reorders the vector by moving
	// the last element into the slot of the removed one. Any ordering between
	// deletions that all hit zero on the same call is LOST. The cross-frame
	// invariant that keeps this safe — pool VRAMs outlive their aliased images
	// by >=1 frame — is enforced at queue time in QueueVRAMDeletion via the
	// uExtraFrameDelay >= 1 assertion on pool VRAMs. If that assertion ever
	// has to be relaxed, either introduce a second pool-only deletion queue
	// or replace RemoveSwap here with order-preserving removal.
	for (u_int i = 0; i < g_xEngine.VulkanMemory().m_xPendingDeletions.GetSize();)
	{
		PendingVRAMDeletion& xDeletion = g_xEngine.VulkanMemory().m_xPendingDeletions.Get(i);
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
				g_xEngine.Vulkan().ReleaseVRAMHandle(xDeletion.m_xHandle);
			}

			g_xEngine.VulkanMemory().m_xPendingDeletions.RemoveSwap(i);
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
	g_xEngine.VulkanMemory().IncreaseImageMemoryUsage(m_ulPoolSize);
	g_xEngine.VulkanMemory().IncreaseMemoryUsage(m_ulPoolSize);
}

Zenith_Vulkan_VRAM::Zenith_Vulkan_VRAM(const vk::Image xImage, const VmaAllocation xAllocation, VmaAllocator xAllocator)
	: m_xImage(xImage), m_xAllocation(xAllocation), m_xAllocator(xAllocator)
{
	g_xEngine.VulkanMemory().IncreaseImageMemoryUsage(m_xAllocation->GetSize());
	g_xEngine.VulkanMemory().IncreaseMemoryUsage(m_xAllocation->GetSize());
}

Zenith_Vulkan_VRAM::Zenith_Vulkan_VRAM(const vk::Buffer xBuffer, const VmaAllocation xAllocation, VmaAllocator xAllocator, const u_int uSize)
	: m_xBuffer(xBuffer), m_xAllocation(xAllocation), m_xAllocator(xAllocator), m_uBufferSize(uSize)
{
	g_xEngine.VulkanMemory().IncreaseBufferMemoryUsage(m_xAllocation->GetSize());
	g_xEngine.VulkanMemory().IncreaseMemoryUsage(m_xAllocation->GetSize());
}

Zenith_Vulkan_VRAM::~Zenith_Vulkan_VRAM()
{
	// Three destruction paths: aliased-image (image only), pool (allocation
	// only), or regular owned resource (image+allocation or buffer+allocation).

	if (m_bAliased)
	{
		Zenith_Assert(m_xImage != VK_NULL_HANDLE, "Aliased VRAM missing image");
		// Destroy the image alone — the allocation is owned by the pool
		// VRAM and will be freed when the pool is destroyed. No memory-usage
		// accounting bump here either; the pool counted for everyone.
		g_xEngine.Vulkan().GetDevice().destroyImage(m_xImage);
		return;
	}

	if (m_bPool)
	{
		Zenith_Assert(m_xAllocation != VK_NULL_HANDLE, "Pool VRAM missing allocation");
		g_xEngine.VulkanMemory().DecreaseImageMemoryUsage(m_ulPoolSize);
		g_xEngine.VulkanMemory().DecreaseMemoryUsage(m_ulPoolSize);
		vmaFreeMemory(m_xAllocator, m_xAllocation);
		return;
	}

	if (m_xAllocation != VK_NULL_HANDLE && m_xAllocator != VK_NULL_HANDLE)
	{
		if (m_xImage != VK_NULL_HANDLE)
		{
			g_xEngine.VulkanMemory().DecreaseImageMemoryUsage(m_xAllocation->GetSize());
			vmaDestroyImage(m_xAllocator, m_xImage, m_xAllocation);
		}
		else if (m_xBuffer != VK_NULL_HANDLE)
		{
			g_xEngine.VulkanMemory().DecreaseBufferMemoryUsage(m_xAllocation->GetSize());
			vmaDestroyBuffer(m_xAllocator, m_xBuffer, m_xAllocation);
		}
		g_xEngine.VulkanMemory().DecreaseMemoryUsage(m_xAllocation->GetSize());
	}
	else
	{
		Zenith_Assert(false, "Deleting dodgy allocation");
	}
}
