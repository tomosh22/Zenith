#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Flux/Flux_ImGuiIntegration.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "backends/imgui_impl_vulkan.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

#include <vector>

//=============================================================================
// Vulkan Implementation of Flux ImGui Integration
//=============================================================================

namespace
{
	// Pending descriptor set deletion entry
	struct PendingDeletion
	{
		VkDescriptorSet xDescriptorSet;
		u_int uFramesRemaining;
	};

	std::vector<PendingDeletion> s_xPendingDeletions;
}

Flux_ImGuiTextureHandle Flux_ImGuiIntegration::RegisterTexture(
	const Flux_ShaderResourceView& xSRV,
	const Flux_Sampler& xSampler)
{
	Flux_ImGuiTextureHandle xHandle;

	if (!xSRV.m_xImageViewHandle.IsValid())
	{
		return xHandle; // Return invalid handle
	}

	// Get the Vulkan image view from the handle registry
	vk::ImageView xImageView = Zenith_Vulkan_MemoryManager::GetImageView(xSRV.m_xImageViewHandle);

	// Allocate ImGui descriptor set for this texture
	VkDescriptorSet xDescriptorSet = ImGui_ImplVulkan_AddTexture(
		xSampler.GetSampler(),
		xImageView,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);

	// Store the descriptor set as the handle value
	xHandle.SetValue(reinterpret_cast<u_int64>(xDescriptorSet));

	return xHandle;
}

void Flux_ImGuiIntegration::UnregisterTexture(Flux_ImGuiTextureHandle xHandle, u_int uFramesToWait)
{
	if (!xHandle.IsValid())
	{
		return;
	}

	VkDescriptorSet xDescriptorSet = reinterpret_cast<VkDescriptorSet>(xHandle.AsUInt64());

	// Queue for deferred deletion
	// We can't free immediately because the GPU may still be using it in in-flight command buffers
	s_xPendingDeletions.push_back({ xDescriptorSet, uFramesToWait });
}

void Flux_ImGuiIntegration::ProcessDeferredUnregistrations()
{
	// Process pending deletions - decrement counters and remove expired ones
	for (auto it = s_xPendingDeletions.begin(); it != s_xPendingDeletions.end(); )
	{
		if (it->uFramesRemaining == 0)
		{
			// Safe to delete now
			ImGui_ImplVulkan_RemoveTexture(it->xDescriptorSet);
			it = s_xPendingDeletions.erase(it);
		}
		else
		{
			it->uFramesRemaining--;
			++it;
		}
	}
}

void* Flux_ImGuiIntegration::GetImTextureID(Flux_ImGuiTextureHandle xHandle)
{
	// The descriptor set can be cast directly to ImTextureID
	return reinterpret_cast<void*>(xHandle.AsUInt64());
}

#endif // ZENITH_TOOLS
