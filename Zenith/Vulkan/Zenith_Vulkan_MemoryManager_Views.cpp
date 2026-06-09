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
// View creation: RTV / DSV / SRV / UAV over VRAM handles.
//=============================================================================

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

	const vk::Device& xDevice = m_pxVulkan->GetDevice();
	Zenith_Vulkan_VRAM* pxVRAM = m_pxVulkan->GetVRAM(xVRAMHandle);
	Zenith_Assert(pxVRAM != nullptr || Zenith_CommandLine::IsHeadless(), "GetVRAM returned null in CreateRenderTargetView");
	if (!pxVRAM) return xView;  // Safety guard for release builds

	vk::Format xFormat = m_pxVulkan->ConvertToVkFormat_Colour(xInfo.m_eFormat);

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

	const vk::Device& xDevice = m_pxVulkan->GetDevice();
	Zenith_Vulkan_VRAM* pxVRAM = m_pxVulkan->GetVRAM(xVRAMHandle);
	Zenith_Assert(pxVRAM != nullptr || Zenith_CommandLine::IsHeadless(), "GetVRAM returned null in CreateRenderTargetViewForLayer");
	if (!pxVRAM) return xView;

	vk::Format xFormat = m_pxVulkan->ConvertToVkFormat_Colour(xInfo.m_eFormat);

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

	const vk::Device& xDevice = m_pxVulkan->GetDevice();
	Zenith_Vulkan_VRAM* pxVRAM = m_pxVulkan->GetVRAM(xVRAMHandle);
	Zenith_Assert(pxVRAM != nullptr || Zenith_CommandLine::IsHeadless(), "GetVRAM returned null in CreateDepthStencilView");
	if (!pxVRAM) return xView;  // Safety guard for release builds

	vk::Format xFormat = m_pxVulkan->ConvertToVkFormat_DepthStencil(xInfo.m_eFormat);

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

	const vk::Device& xDevice = m_pxVulkan->GetDevice();
	Zenith_Vulkan_VRAM* pxVRAM = m_pxVulkan->GetVRAM(xVRAMHandle);
	Zenith_Assert(pxVRAM != nullptr || Zenith_CommandLine::IsHeadless(), "GetVRAM returned null in CreateShaderResourceView");
	if (!pxVRAM) return xView;  // Safety guard for release builds

	const bool bIsDepth = xInfo.m_eFormat > TEXTURE_FORMAT_DEPTH_STENCIL_BEGIN && xInfo.m_eFormat < TEXTURE_FORMAT_DEPTH_STENCIL_END;
	vk::Format xFormat = bIsDepth ? m_pxVulkan->ConvertToVkFormat_DepthStencil(xInfo.m_eFormat) : m_pxVulkan->ConvertToVkFormat_Colour(xInfo.m_eFormat);

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
		m_pxVulkan->WriteBindlessDescriptor(
			xView.m_xImageViewHandle.AsUInt(),
			xVkView,
			m_pxFluxGraphics->m_xClampSampler.GetSampler());
	}

	return xView;
}

Flux_ShaderResourceView Zenith_Vulkan_MemoryManager::CreateShaderResourceViewForLayer(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uLayer, uint32_t uBaseMip, uint32_t uMipCount)
{
	Flux_ShaderResourceView xView;
	xView.m_xVRAMHandle = xVRAMHandle;

	const vk::Device& xDevice = m_pxVulkan->GetDevice();
	Zenith_Vulkan_VRAM* pxVRAM = m_pxVulkan->GetVRAM(xVRAMHandle);
	Zenith_Assert(pxVRAM != nullptr || Zenith_CommandLine::IsHeadless(), "GetVRAM returned null in CreateShaderResourceViewForLayer");
	if (!pxVRAM) return xView;

	const bool bIsDepth = xInfo.m_eFormat > TEXTURE_FORMAT_DEPTH_STENCIL_BEGIN && xInfo.m_eFormat < TEXTURE_FORMAT_DEPTH_STENCIL_END;
	vk::Format xFormat = bIsDepth ? m_pxVulkan->ConvertToVkFormat_DepthStencil(xInfo.m_eFormat) : m_pxVulkan->ConvertToVkFormat_Colour(xInfo.m_eFormat);

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
		m_pxVulkan->WriteBindlessDescriptor(
			xView.m_xImageViewHandle.AsUInt(),
			xVkView,
			m_pxFluxGraphics->m_xClampSampler.GetSampler());
	}

	return xView;
}

Flux_UnorderedAccessView_Texture Zenith_Vulkan_MemoryManager::CreateUnorderedAccessView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel)
{
	Flux_UnorderedAccessView_Texture xView;
	xView.m_xVRAMHandle = xVRAMHandle;

	const vk::Device& xDevice = m_pxVulkan->GetDevice();
	Zenith_Vulkan_VRAM* pxVRAM = m_pxVulkan->GetVRAM(xVRAMHandle);
	Zenith_Assert(pxVRAM != nullptr || Zenith_CommandLine::IsHeadless(), "GetVRAM returned null in CreateUnorderedAccessView");
	if (!pxVRAM) return xView;  // Safety guard for release builds

	vk::Format xFormat = m_pxVulkan->ConvertToVkFormat_Colour(xInfo.m_eFormat);

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

	const vk::Device& xDevice = m_pxVulkan->GetDevice();
	Zenith_Vulkan_VRAM* pxVRAM = m_pxVulkan->GetVRAM(xVRAMHandle);
	Zenith_Assert(pxVRAM != nullptr || Zenith_CommandLine::IsHeadless(), "GetVRAM returned null in CreateUnorderedAccessViewForSlice");
	if (!pxVRAM) return xView;

	vk::Format xFormat = m_pxVulkan->ConvertToVkFormat_Colour(xInfo.m_eFormat);

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
