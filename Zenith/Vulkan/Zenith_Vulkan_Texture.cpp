#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Zenith_Vulkan_Texture.h"

#include "Zenith_Vulkan.h"
#include "Zenith_Vulkan_MemoryManager.h"

void Zenith_Vulkan_Texture::Reset()
{
	// Clear handles to mark texture as invalid
	// Note: Actual GPU memory cleanup is handled by Zenith_Vulkan_MemoryManager
	// through the deferred deletion system (QueueVRAMDeletion)
	m_xImage = VK_NULL_HANDLE;
	m_xImageView = VK_NULL_HANDLE;
	m_xAllocation = VK_NULL_HANDLE;
	m_uNumMips = 0;
	m_uWidth = 0;
	m_uHeight = 0;
	m_uNumLayers = 0;
	m_eFormat = vk::Format::eUndefined;
}

// Format/action converter functions that used to live here were dead
// duplicates of Zenith_Vulkan::ConvertToVkFormat_Colour / _DepthStencil /
// ConvertToVkLoadAction / ConvertToVkStoreAction. They had zero external
// callers and have been removed. Use the Zenith_Vulkan:: versions.

static void InitialiseSampler(Zenith_Vulkan_Sampler& xSampler, vk::SamplerAddressMode eAddressMode)
{
	const vk::Device& xDevice = g_xEngine.FluxBackend().GetDevice();
	// Anisotropic filtering: the device feature is enabled at init
	// (Zenith_Vulkan.cpp setSamplerAnisotropy(VK_TRUE)), but every sampler
	// previously sampled trilinear-only, blurring + shimmering all textures at
	// grazing angles -- a top-tier non-photoreal tell. 16x is universally
	// supported on desktop Vulkan (maxSamplerAnisotropy >= 16). Mip-LOD bias is
	// 0 (no sharpening): a negative bias samples higher-detail mips, which
	// without TAA aliases fine albedo detail into crawling speckle (visible on
	// the StickFigure skin). Reintroduce a small negative bias once TAA lands.
	vk::SamplerCreateInfo xInfo = vk::SamplerCreateInfo()
		.setMagFilter(vk::Filter::eLinear)
		.setMinFilter(vk::Filter::eLinear)
		.setAddressModeU(eAddressMode)
		.setAddressModeV(eAddressMode)
		.setAddressModeW(eAddressMode)
		.setBorderColor(vk::BorderColor::eIntOpaqueBlack)
		.setUnnormalizedCoordinates(VK_FALSE)
		.setCompareEnable(VK_FALSE)
		.setCompareOp(vk::CompareOp::eAlways)
		.setMipmapMode(vk::SamplerMipmapMode::eLinear)
		.setAnisotropyEnable(VK_TRUE)
		.setMaxAnisotropy(16.0f)
		.setMipLodBias(0.0f)
		.setMaxLod(FLT_MAX);

	xSampler.m_xSampler = VkUnwrap(xDevice.createSampler(xInfo));
}

void Zenith_Vulkan_Sampler::InitialiseRepeat(Zenith_Vulkan_Sampler& xSampler)
{
	InitialiseSampler(xSampler, vk::SamplerAddressMode::eRepeat);
}

void Zenith_Vulkan_Sampler::InitialiseClamp(Zenith_Vulkan_Sampler& xSampler)
{
	InitialiseSampler(xSampler, vk::SamplerAddressMode::eClampToEdge);
}