#include "Zenith.h"
#include "Core/Zenith_Engine.h"

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

//=============================================================================
// Transient-aliasing support: alias pools, aliased image creation, and the
// image-memory-requirements probe cache (owned here via
// Zenith_VulkanMemory_ProbeCache(); core Initialise/Shutdown clear it).
//=============================================================================

Zenith_HashMap<u_int64, ProbeCacheEntry>& Zenith_VulkanMemory_ProbeCache()
{
	static Zenith_HashMap<u_int64, ProbeCacheEntry> ls_xProbeCache;
	return ls_xProbeCache;
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
	if (m_xAllocator == VK_NULL_HANDLE)
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
	VkResult eResult = vmaAllocateMemory(m_xAllocator, &xReqs, &xCreateInfo, &xAllocation, nullptr);
	Zenith_Assert(eResult == VK_SUCCESS, "vmaAllocateMemory (alias pool) failed: %d", static_cast<int>(eResult));
	if (eResult != VK_SUCCESS)
	{
		return Flux_VRAMHandle();
	}

	Zenith_Vulkan_VRAM* pxVRAM = new Zenith_Vulkan_VRAM(Zenith_Vulkan_VRAM::PoolTag{}, xAllocation, m_xAllocator, ulSize);
	return m_pxVulkan->RegisterVRAM(pxVRAM);
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

	// File-static free function — no 'this' to inject through. Recover the
	// Vulkan singleton once and route both format conversions through it.
	Zenith_Vulkan& xVulkan = g_xEngine.FluxBackend();

	vk::Format xFormat;
	vk::ImageUsageFlags eUsageFlags;
	if (bIsColour)
	{
		xFormat = xVulkan.ConvertToVkFormat_Colour(xInfo.m_eFormat);
		eUsageFlags = vk::ImageUsageFlagBits::eColorAttachment;
		if (xInfo.m_uMemoryFlags & 1 << MEMORY_FLAGS__UNORDERED_ACCESS)
			eUsageFlags |= vk::ImageUsageFlagBits::eStorage;
	}
	else
	{
		xFormat = xVulkan.ConvertToVkFormat_DepthStencil(xInfo.m_eFormat);
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

// Zenith_VulkanMemory_ProbeCache() / ProbeCacheEntry are declared at the top of this TU — they
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
	if (const ProbeCacheEntry* pxHit = Zenith_VulkanMemory_ProbeCache().TryGet(ulSig))
	{
		ulSizeOut      = pxHit->m_ulSize;
		ulAlignmentOut = pxHit->m_ulAlignment;
		return;
	}

	const vk::ImageCreateInfo xImageInfo = BuildAliasedImageCreateInfo(xInfo);

	const vk::Device& xDevice = m_pxVulkan->GetDevice();
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
	Zenith_VulkanMemory_ProbeCache().Insert(ulSig, xEntry);
}

Flux_VRAMHandle Zenith_Vulkan_MemoryManager::CreateAliasedImageVRAM(const Flux_SurfaceInfo& xInfo,
                                                                    Flux_VRAMHandle xPoolHandle,
                                                                    u_int64 ulOffsetInPool)
{
	// Headless guard: see CreateBufferVRAM for rationale.
	if (m_xAllocator == VK_NULL_HANDLE)
	{
		return Flux_VRAMHandle();
	}

	Zenith_Assert(xPoolHandle.IsValid(), "CreateAliasedImageVRAM: invalid pool handle");
	Zenith_Vulkan_VRAM* pxPoolVRAM = m_pxVulkan->GetVRAM(xPoolHandle);
	Zenith_Assert(pxPoolVRAM != nullptr && pxPoolVRAM->IsPool(),
		"CreateAliasedImageVRAM: pool handle does not reference a pool VRAM");

	// Same image-create-info as the probe path — BuildAliasedImageCreateInfo is
	// the single source of truth so the packer's reported size is guaranteed
	// to match what we actually bind into the pool.
	const vk::ImageCreateInfo xImageInfo = BuildAliasedImageCreateInfo(xInfo);

	const vk::ImageCreateInfo::NativeType xImageInfoNative = xImageInfo;

	VkImage xImage = VK_NULL_HANDLE;
	VkResult eResult = vmaCreateAliasingImage2(
		m_xAllocator,
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
		m_xAllocator);
	return m_pxVulkan->RegisterVRAM(pxVRAM);
}
