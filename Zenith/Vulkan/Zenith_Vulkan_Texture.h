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
	//#TO don't do this, will break memory manager as it relies on addresses of instances of this class to track them as allocations
	//#TO_TODO: implement some sort of texture registry to get around this
	Zenith_Vulkan_Texture(Zenith_Vulkan_Texture& xOther) = delete;
	Zenith_Vulkan_Texture(const Zenith_Vulkan_Texture& xOther) = delete;

	static vk::Format ConvertToVkFormat_Colour(ColourFormat eFormat);
	static vk::Format ConvertToVkFormat_DepthStencil(DepthStencilFormat eFormat);
	static vk::AttachmentLoadOp ConvertToVkLoadAction(LoadAction eAction);
	static vk::AttachmentStoreOp ConvertToVkStoreAction(StoreAction eAction);
	static vk::ImageLayout ConvertToVkTargetUsage(RenderTargetUsage eUsage, RenderTargetType eColourDepthStencil);

	const vk::Image& GetImage() const { return m_xImage; }
	const vk::ImageView& const GetImageView() { return m_xImageView; }
	const uint32_t GetNumMips() const { return m_uNumMips; }
	const uint32_t GetNumLayers() const { return m_uNumLayers; }

	void SetImage(const vk::Image xImage) { m_xImage = xImage; }
	void SetImageView(const vk::ImageView xView) { m_xImageView = xView; }

	const uint32_t GetWidth() const { return m_uWidth; }
	const uint32_t GetHeight() const { return m_uHeight; }

	void SetWidth(const uint32_t uWidth) { m_uWidth = uWidth; }
	void SetHeight(const uint32_t uHeight) { m_uHeight = uHeight; }
	void SetNumMips(const uint32_t uNumMips) { m_uNumMips = uNumMips; }
	void SetNumLayers(const uint32_t uNumLayers) { m_uNumLayers = uNumLayers; }
private:
	vk::Image m_xImage;
	vk::ImageView m_xImageView;
	uint32_t m_uNumMips = 0;
	uint32_t m_uWidth = 0;
	uint32_t m_uHeight = 0;
	uint32_t m_uNumLayers = 0;
};

class Zenith_Vulkan_Sampler
{
public:
	const vk::Sampler& GetSampler() const { return m_xSampler; }

	static void InitialiseDefault(Zenith_Vulkan_Sampler& xSampler);
private:
	vk::Sampler m_xSampler;
};