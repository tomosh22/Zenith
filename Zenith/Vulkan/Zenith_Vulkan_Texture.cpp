#include "Zenith.h"

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

vk::Format Zenith_Vulkan_Texture::ConvertToVkFormat_Colour(TextureFormat eFormat) {
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

vk::Format Zenith_Vulkan_Texture::ConvertToVkFormat_DepthStencil(TextureFormat eFormat) {
	switch (eFormat)
	{
	case TEXTURE_FORMAT_D32_SFLOAT:
		return vk::Format::eD32Sfloat;
	default:
		Zenith_Assert(false, "Invalid format");
		return vk::Format::eUndefined;
	}
}

vk::AttachmentLoadOp Zenith_Vulkan_Texture::ConvertToVkLoadAction(LoadAction eAction) {
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

vk::AttachmentStoreOp Zenith_Vulkan_Texture::ConvertToVkStoreAction(StoreAction eAction) {
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

#if 0
vk::ImageLayout Zenith_Vulkan_Texture::ConvertToVkTargetUsage(RenderTargetUsage eUsage, RenderTargetType eColourDepthStencil) {
	switch (eUsage)
	{
	case RENDER_TARGET_USAGE_RENDERTARGET:
		switch (eColourDepthStencil)
		{
		case RENDER_TARGET_TYPE_COLOUR:
			return vk::ImageLayout::eColorAttachmentOptimal;
		case RENDER_TARGET_TYPE_DEPTH:
			return vk::ImageLayout::eDepthAttachmentOptimal;
		case RENDER_TARGET_TYPE_DEPTHSTENCIL:
			return vk::ImageLayout::eDepthStencilAttachmentOptimal;
		}
		break;
	case RENDER_TARGET_USAGE_SHADERREAD:
		return vk::ImageLayout::eShaderReadOnlyOptimal;
	case RENDER_TARGET_USAGE_PRESENT:
		return vk::ImageLayout::ePresentSrcKHR;
	default:
		Zenith_Assert(false, "Invalid usage");
	}
}
#endif

static void InitialiseSampler(Zenith_Vulkan_Sampler& xSampler, vk::SamplerAddressMode eAddressMode)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
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
		.setMaxLod(FLT_MAX);

	xSampler.m_xSampler = xDevice.createSampler(xInfo);
}

void Zenith_Vulkan_Sampler::InitialiseRepeat(Zenith_Vulkan_Sampler& xSampler)
{
	InitialiseSampler(xSampler, vk::SamplerAddressMode::eRepeat);
}

void Zenith_Vulkan_Sampler::InitialiseClamp(Zenith_Vulkan_Sampler& xSampler)
{
	InitialiseSampler(xSampler, vk::SamplerAddressMode::eClampToEdge);
}