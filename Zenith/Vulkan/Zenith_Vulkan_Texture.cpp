#include "Zenith.h"

#include "Zenith_Vulkan_Texture.h"

#include "Zenith_Vulkan.h"
#include "Zenith_Vulkan_MemoryManager.h"

void Zenith_Vulkan_Texture::Reset()
{
	Zenith_Vulkan_MemoryManager::FreeTexture(this);
}

vk::Format Zenith_Vulkan_Texture::ConvertToVkFormat_Colour(ColourFormat eFormat) {
	switch (eFormat)
	{
	case COLOUR_FORMAT_RGB8_UNORM:
		return vk::Format::eR8G8B8Unorm;
	case COLOUR_FORMAT_RGBA8_UNORM:
		return vk::Format::eR8G8B8A8Unorm;
	case COLOUR_FORMAT_BGRA8_SRGB:
		return vk::Format::eB8G8R8A8Srgb;
	case COLOUR_FORMAT_R16G16B16A16_SFLOAT:
		return vk::Format::eR16G16B16A16Sfloat;
	case COLOUR_FORMAT_R16G16B16A16_UNORM:
		return vk::Format::eR16G16B16A16Unorm;
	case COLOUR_FORMAT_BGRA8_UNORM:
		return vk::Format::eB8G8R8A8Unorm;
	default:
		Zenith_Assert(false, "Invalid format");
	}
}

vk::Format Zenith_Vulkan_Texture::ConvertToVkFormat_DepthStencil(DepthStencilFormat eFormat) {
	switch (eFormat)
	{
	case DEPTHSTENCIL_FORMAT_D32_SFLOAT:
		return vk::Format::eD32Sfloat;
	default:
		Zenith_Assert(false, "Invalid format");
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
	}
}

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

void Zenith_Vulkan_Sampler::InitialiseDefault(Zenith_Vulkan_Sampler& xSampler)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	const vk::PhysicalDevice& xPhysDevice = Zenith_Vulkan::GetPhysicalDevice();

	vk::SamplerCreateInfo xInfo = vk::SamplerCreateInfo()
		.setMagFilter(vk::Filter::eLinear)
		.setMinFilter(vk::Filter::eLinear)
		.setAddressModeU(vk::SamplerAddressMode::eRepeat)
		.setAddressModeV(vk::SamplerAddressMode::eRepeat)
		.setAddressModeW(vk::SamplerAddressMode::eRepeat)
		//.setAnisotropyEnable(VK_TRUE)
		//.setMaxAnisotropy(xPhysDevice.getProperties().limits.maxSamplerAnisotropy)
		.setBorderColor(vk::BorderColor::eIntOpaqueBlack)
		.setUnnormalizedCoordinates(VK_FALSE)
		.setCompareEnable(VK_FALSE)
		.setCompareOp(vk::CompareOp::eAlways)
		.setMipmapMode(vk::SamplerMipmapMode::eLinear)
		.setMaxLod(FLT_MAX);

	xSampler.m_xSampler = xDevice.createSampler(xInfo);
}