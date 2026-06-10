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
// Texture + render-target VRAM: image creation, texture upload, mip
// generation.
//=============================================================================

Flux_VRAMHandle Zenith_Vulkan_MemoryManager::CreateRenderTargetVRAM(const Flux_SurfaceInfo& xInfo)
{
	// Headless guard: see CreateBufferVRAM for rationale.
	if (m_xAllocator == VK_NULL_HANDLE)
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
		xFormat = m_pxVulkan->ConvertToVkFormat_Colour(xInfo.m_eFormat);
		eUsageFlags = vk::ImageUsageFlagBits::eColorAttachment;
		eAspectFlags = vk::ImageAspectFlagBits::eColor;
		eInitialLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		
		if (xInfo.m_uMemoryFlags & 1 << MEMORY_FLAGS__UNORDERED_ACCESS) 
			eUsageFlags |= vk::ImageUsageFlagBits::eStorage;
	}
	else // bIsDepthStencil
	{
		xFormat = m_pxVulkan->ConvertToVkFormat_DepthStencil(xInfo.m_eFormat);
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
	VkResult eResult = vmaCreateImage(m_xAllocator, &xImageInfo_Native, &xAllocInfo, &xImage, &xAllocation, nullptr);
	// Wave14 B3 / mirrors WS9.2 CreateBufferVRAM: surface a GPU-OOM (or any
	// vmaCreateImage failure) via the release-survivable check tier — logged for
	// diagnosability in all shipping configs — but keep the existing invalid-handle
	// contract that callers already tolerate. Do NOT promote this to a hard
	// assert/return change.
	Zenith_Check(eResult == VK_SUCCESS, "vmaCreateImage failed (result=%d) - returning invalid handle (Zenith_ErrorCode::GPU_UPLOAD_FAILED)", (int)eResult);
	if (eResult != VK_SUCCESS)
	{
		// Return invalid handle on allocation failure
		return Flux_VRAMHandle();
	}

	Zenith_Vulkan_VRAM* pxVRAM = new Zenith_Vulkan_VRAM(vk::Image(xImage), xAllocation, m_xAllocator);
	Flux_VRAMHandle xHandle = m_pxVulkan->RegisterVRAM(pxVRAM);

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
	vk::Format xFormat = m_pxVulkan->ConvertToVkFormat_Colour(xInfo.m_eFormat);

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
	if (m_xAllocator == VK_NULL_HANDLE)
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
	VkResult eResult = vmaCreateImage(m_xAllocator, &xImageInfo_Native, &xAllocInfo, &xImageOut, &xAllocationOut, nullptr);
	// Wave14 B3 / mirrors WS9.2 CreateBufferVRAM: surface a GPU-OOM (or any
	// vmaCreateImage failure) via the release-survivable check tier — logged for
	// diagnosability in all shipping configs — but keep the existing invalid-handle
	// contract that callers already tolerate. Do NOT promote this to a hard
	// assert/return change.
	Zenith_Check(eResult == VK_SUCCESS, "vmaCreateImage failed (result=%d) - returning invalid handle (Zenith_ErrorCode::GPU_UPLOAD_FAILED)", (int)eResult);
	if (eResult != VK_SUCCESS)
	{
		return Flux_VRAMHandle();
	}

	Zenith_Vulkan_VRAM* pxVRAM = new Zenith_Vulkan_VRAM(vk::Image(xImageOut), xAllocationOut, m_xAllocator);
	return m_pxVulkan->RegisterVRAM(pxVRAM);
}

void Zenith_Vulkan_MemoryManager::UploadTextureData(VkImage xImage, VmaAllocation xAllocation,
	const void* pData, const Flux_SurfaceInfo& xInfo, size_t ulDataSize)
{
	// Headless guard: see CreateBufferVRAM for rationale.
	if (m_xAllocator == VK_NULL_HANDLE)
	{
		return;
	}

	const vk::Device& xDevice = m_pxVulkan->GetDevice();

	m_xMutex.Lock();

	VkMemoryPropertyFlags eMemoryProps;
	vmaGetAllocationMemoryProperties(m_xAllocator, xAllocation, &eMemoryProps);

	if (eMemoryProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		// Direct upload to host-visible memory.
		VmaAllocationInfo xImageAllocInfo;
		vmaGetAllocationInfo(m_xAllocator, xAllocation, &xImageAllocInfo);
		memcpy(xImageAllocInfo.pMappedData, pData, ulDataSize);
		vmaFlushAllocation(m_xAllocator, xAllocation, 0, VK_WHOLE_SIZE);
	}
	else if (ulDataSize > g_uStagingPoolSize)
	{
		// Allocation larger than the staging buffer — unlock and chunk.
		m_xMutex.Unlock();
		UploadTextureDataChunked(vk::Image(xImage), pData, ulDataSize, xInfo.m_uWidth, xInfo.m_uHeight, xInfo.m_uNumMips, xInfo.m_uNumLayers);
		return;
	}
	else
	{
		// Upload via the current frame's staging slot. CurrentStaging() resolves
		// to g_xEngine.FluxMemory().m_axStaging[CurrentFrameIndex], so the bump-allocate + memcpy +
		// queue-copy sequence below only ever touches memory the GPU is allowed
		// to consume in this frame.
		PerFrameStaging& xStaging = CurrentStaging();
		if (xStaging.m_uNextFreeOffset + ulDataSize > g_uStagingPoolSize)
		{
			// HandleStagingBufferFull does a Flush() -> EndAndCpuWait which
			// blocks for multi-second GPU work. Holding m_xMutex across
			// that wait stalls every other thread doing uploads (smoke runs
			// showed 20-second "Wait for Mutex" events from worker threads
			// piling up here). Release the mutex during the GPU wait.
			m_xMutex.Unlock();
			HandleStagingBufferFull();
			m_xMutex.Lock();
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
	m_xMutex.Unlock();
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
		return xHandle;  // Allocation failure already logged (Zenith_Check) inside the helper.
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

void Zenith_Vulkan_MemoryManager::UpdateTextureVRAM(Flux_VRAMHandle xHandle, const void* pData, const Flux_SurfaceInfo& xInfo)
{
	// Headless guard: see CreateBufferVRAM for rationale.
	if (m_xAllocator == VK_NULL_HANDLE)
	{
		return;
	}
	if (!xHandle.IsValid() || pData == nullptr)
	{
		return;
	}

	Zenith_Vulkan_VRAM* pxVRAM = m_pxVulkan->GetVRAM(xHandle);
	Zenith_Check(pxVRAM != nullptr, "UpdateTextureVRAM: GetVRAM returned null - skipping upload");
	if (pxVRAM == nullptr)
	{
		return;
	}

	// Defensive clamps mirroring NormalizeTextureInfo so the byte-size math
	// can't silently zero out on uninitialised depth/layer fields.
	Flux_SurfaceInfo xInfoCopy = xInfo;
	xInfoCopy.m_uDepth = std::max(1u, xInfoCopy.m_uDepth);
	xInfoCopy.m_uNumLayers = std::max(1u, xInfoCopy.m_uNumLayers);
	xInfoCopy.m_uNumMips = std::max(1u, xInfoCopy.m_uNumMips);

	size_t ulDataSize;
	if (IsCompressedFormat(xInfoCopy.m_eFormat))
	{
		ulDataSize = CalculateCompressedTextureSize(xInfoCopy.m_eFormat, xInfoCopy.m_uWidth, xInfoCopy.m_uHeight) * xInfoCopy.m_uNumLayers;
	}
	else
	{
		ulDataSize = ColourFormatBytesPerPixel(xInfoCopy.m_eFormat) * xInfoCopy.m_uWidth * xInfoCopy.m_uHeight * xInfoCopy.m_uDepth * xInfoCopy.m_uNumLayers;
	}

	UploadTextureData(static_cast<VkImage>(pxVRAM->GetImage()), pxVRAM->GetAllocation(), pData, xInfoCopy, ulDataSize);
}

void Zenith_Vulkan_MemoryManager::GenerateMipmapsAndTransitionToShaderRead(vk::Image xImage, uint32_t uWidth, uint32_t uHeight, uint32_t uNumMips, uint32_t uLayer, bool bIsCompressed)
{
	EnsureRecording();

	// Mip 0 is already in TRANSFER_DST_OPTIMAL from the copy. Transition to TRANSFER_SRC for blit source.
	m_xCommandBuffer.ImageTransitionBarrier(xImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, 0, uLayer);

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

			m_xCommandBuffer.GetCurrentCmdBuffer().blitImage(xImage, vk::ImageLayout::eTransferSrcOptimal, xImage, vk::ImageLayout::eTransferDstOptimal, 1, &xBlit, vk::Filter::eLinear);
			m_xCommandBuffer.ImageTransitionBarrier(xImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, uMip, uLayer);
		}
	}

	// Transition all mips to shader-read layout
	m_xCommandBuffer.ImageTransitionBarrier(xImage, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, 0, uLayer);

	for (uint32_t uMip = 1; uMip < uNumMips; uMip++)
	{
		// Compressed mips 1+ are still in TRANSFER_DST (no blit was done).
		// Non-compressed mips were transitioned to TRANSFER_SRC after each blit.
		vk::ImageLayout eSrcLayout = bIsCompressed ? vk::ImageLayout::eTransferDstOptimal : vk::ImageLayout::eTransferSrcOptimal;
		m_xCommandBuffer.ImageTransitionBarrier(xImage, eSrcLayout, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, uMip, uLayer);
	}
}
