#pragma once
#include "vulkan/vulkan.hpp"
#include "Flux/Flux_Enums.h"

class Zenith_Vulkan_Texture
{
public:
	Zenith_Vulkan_Texture() = default;
	~Zenith_Vulkan_Texture()
	{
		STUBBED
	}

	static vk::Format ConvertToVkFormat_Colour(ColourFormat eFormat);
	static vk::Format ConvertToVkFormat_DepthStencil(DepthStencilFormat eFormat);
	static vk::AttachmentLoadOp ConvertToVkLoadAction(LoadAction eAction);
	static vk::AttachmentStoreOp ConvertToVkStoreAction(StoreAction eAction);
	static vk::ImageLayout ConvertToVkTargetUsage(RenderTargetUsage eUsage, RenderTargetType eColourDepthStencil);

	static Zenith_Vulkan_Texture* CreateColourAttachment(uint32_t uWidth, uint32_t uHeight, ColourFormat eFormat, uint32_t uBitsPerPixel);
	static Zenith_Vulkan_Texture* CreateDepthStencilAttachment(uint32_t uWidth, uint32_t uHeight, DepthStencilFormat eFormat, uint32_t uBitsPerPixel);

	const vk::Image& GetImage() { return m_xImage; }
	const vk::ImageView& GetImageView() { return m_xImageView; }
	const uint32_t GetNumMips() const { return m_uNumMips; }

	void SetImage(const vk::Image xImage) { m_xImage = xImage; }
	void SetImageView(const vk::ImageView xView) { m_xImageView = xView; }
private:
	vk::Image m_xImage;
	vk::ImageView m_xImageView;
	uint32_t m_uNumMips = 0;
};

class Zenith_Vulkan_Sampler
{
public:
	const vk::Sampler& GetSampler() const { return m_xSampler; }

	static void InitialiseDefault(Zenith_Vulkan_Sampler& xSampler);
private:
	vk::Sampler m_xSampler;
};