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
// Handle registries: image-view and buffer-descriptor handle tables.
//=============================================================================

Flux_ImageViewHandle Zenith_Vulkan_MemoryManager::RegisterImageView(vk::ImageView xView)
{
	Flux_ImageViewHandle xHandle;
	if (m_xFreeImageViewHandles.GetSize() > 0)
	{
		u_int uIndex = m_xFreeImageViewHandles.GetBack();
		m_xFreeImageViewHandles.PopBack();
		m_xImageViewRegistry.Get(uIndex) = xView;
		xHandle.SetValue(uIndex);
	}
	else
	{
		xHandle.SetValue(m_xImageViewRegistry.GetSize());
		m_xImageViewRegistry.PushBack(xView);
	}
	return xHandle;
}

vk::ImageView Zenith_Vulkan_MemoryManager::GetImageView(Flux_ImageViewHandle xHandle)
{
	if (!xHandle.IsValid() || xHandle.AsUInt() >= m_xImageViewRegistry.GetSize())
	{
		return VK_NULL_HANDLE;
	}
	return m_xImageViewRegistry.Get(xHandle.AsUInt());
}

void Zenith_Vulkan_MemoryManager::ReleaseImageViewHandle(Flux_ImageViewHandle xHandle)
{
	if (!xHandle.IsValid() || xHandle.AsUInt() >= m_xImageViewRegistry.GetSize())
	{
		return;
	}
	m_xImageViewRegistry.Get(xHandle.AsUInt()) = VK_NULL_HANDLE;
	m_xFreeImageViewHandles.PushBack(xHandle.AsUInt());
}

// BufferDescriptor handle registry implementation
Flux_BufferDescriptorHandle Zenith_Vulkan_MemoryManager::RegisterBufferDescriptor(const vk::DescriptorBufferInfo& xInfo)
{
	Flux_BufferDescriptorHandle xHandle;
	if (m_xFreeBufferDescHandles.GetSize() > 0)
	{
		u_int uIndex = m_xFreeBufferDescHandles.GetBack();
		m_xFreeBufferDescHandles.PopBack();
		m_xBufferDescriptorRegistry.Get(uIndex) = xInfo;
		xHandle.SetValue(uIndex);
	}
	else
	{
		xHandle.SetValue(m_xBufferDescriptorRegistry.GetSize());
		m_xBufferDescriptorRegistry.PushBack(xInfo);
	}
	return xHandle;
}

vk::DescriptorBufferInfo Zenith_Vulkan_MemoryManager::GetBufferDescriptor(Flux_BufferDescriptorHandle xHandle)
{
	if (!xHandle.IsValid() || xHandle.AsUInt() >= m_xBufferDescriptorRegistry.GetSize())
	{
		return vk::DescriptorBufferInfo();
	}
	return m_xBufferDescriptorRegistry.Get(xHandle.AsUInt());
}

void Zenith_Vulkan_MemoryManager::ReleaseBufferDescriptorHandle(Flux_BufferDescriptorHandle xHandle)
{
	if (!xHandle.IsValid() || xHandle.AsUInt() >= m_xBufferDescriptorRegistry.GetSize())
	{
		return;
	}
	m_xBufferDescriptorRegistry.Get(xHandle.AsUInt()) = vk::DescriptorBufferInfo();
	m_xFreeBufferDescHandles.PushBack(xHandle.AsUInt());
}
