#pragma once
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "vulkan/vulkan.hpp"
#include "vma/vk_mem_alloc.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

#include "Flux/Flux_Enums.h"

class Zenith_Vulkan_Texture
{
public:
	Zenith_Vulkan_Texture() = default;
	~Zenith_Vulkan_Texture()
	{
		Reset();
	}
	//#TO don't do this, will break memory manager as it relies on addresses of instances of this class to track them as allocations
	//#TO_TODO: implement some sort of texture registry to get around this
	Zenith_Vulkan_Texture(Zenith_Vulkan_Texture& xOther) = delete;
	Zenith_Vulkan_Texture(const Zenith_Vulkan_Texture& xOther) = delete;
	void operator=(Zenith_Vulkan_Texture& xOther) = delete;
	void operator=(const Zenith_Vulkan_Texture& xOther) = delete;

	void Reset();

	static vk::Format ConvertToVkFormat_Colour(TextureFormat eFormat);
	static vk::Format ConvertToVkFormat_DepthStencil(TextureFormat eFormat);
	static vk::AttachmentLoadOp ConvertToVkLoadAction(LoadAction eAction);
	static vk::AttachmentStoreOp ConvertToVkStoreAction(StoreAction eAction);

	#if 0
	static vk::ImageLayout ConvertToVkTargetUsage(RenderTargetUsage eUsage);
	#endif

	const vk::Image GetImage() const { return m_xImage; }
	VkImage* GetImage_Ptr() { return &m_xImage; }
	const vk::ImageView GetImageView() const { return m_xImageView; }
	const uint32_t GetNumMips() const { return m_uNumMips; }
	const uint32_t GetNumLayers() const { return m_uNumLayers; }
	const VmaAllocation& GetAllocation() const { return m_xAllocation; }
	VmaAllocation* GetAllocation_Ptr() { return &m_xAllocation; }
	VmaAllocationInfo* GetAllocationInfo_Ptr() { return &m_xAllocationInfo; }
	const vk::Format GetTextureFormat() const { return m_eFormat; }

	void SetImage(const vk::Image xImage) { m_xImage = xImage; }
	void SetImageView(const vk::ImageView xView) { m_xImageView = xView; }
	void SetAllocation(const VmaAllocation& xAlloc) { m_xAllocation = xAlloc; }
	void SetFormat(const vk::Format eFormat) { m_eFormat = eFormat; }

	const uint32_t GetWidth() const { return m_uWidth; }
	const uint32_t GetHeight() const { return m_uHeight; }

	void SetWidth(const uint32_t uWidth) { m_uWidth = uWidth; }
	void SetHeight(const uint32_t uHeight) { m_uHeight = uHeight; }
	void SetNumMips(const uint32_t uNumMips) { m_uNumMips = uNumMips; }
	void SetNumLayers(const uint32_t uNumLayers) { m_uNumLayers = uNumLayers; }

	bool IsValid() const
	{
		return m_xImage != VK_NULL_HANDLE;
	}
private:
	//#TO native type to support vma
	vk::Image::NativeType m_xImage = VK_NULL_HANDLE;
	vk::ImageView m_xImageView = VK_NULL_HANDLE;
	uint32_t m_uNumMips = 0;
	uint32_t m_uWidth = 0;
	uint32_t m_uHeight = 0;
	uint32_t m_uNumLayers = 0;
	VmaAllocation m_xAllocation;
	VmaAllocationInfo m_xAllocationInfo;

	vk::Format m_eFormat = vk::Format::eUndefined;
};
