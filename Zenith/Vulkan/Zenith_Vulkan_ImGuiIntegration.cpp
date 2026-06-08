#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Flux/Flux_ImGuiIntegration.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "backends/imgui_impl_vulkan.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

#include "Collections/Zenith_Vector.h"

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

	Zenith_Vector<PendingDeletion> s_xPendingDeletions;
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
	vk::ImageView xImageView = g_xEngine.FluxMemory().GetImageView(xSRV.m_xImageViewHandle);

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
	s_xPendingDeletions.PushBack({ xDescriptorSet, uFramesToWait });
}

void Flux_ImGuiIntegration::ProcessDeferredUnregistrations()
{
	// Process pending deletions - decrement counters and remove expired ones.
	// Order doesn't matter for a deletion queue, so use O(1) swap-and-pop removal.
	// On RemoveSwap(u) the last element moves into slot u, so re-process that slot
	// (do NOT advance u); otherwise advance to the next slot.
	for (u_int u = 0; u < s_xPendingDeletions.GetSize(); )
	{
		PendingDeletion& xEntry = s_xPendingDeletions.Get(u);
		if (xEntry.uFramesRemaining == 0)
		{
			// Safe to delete now
			ImGui_ImplVulkan_RemoveTexture(xEntry.xDescriptorSet);
			s_xPendingDeletions.RemoveSwap(u);
		}
		else
		{
			xEntry.uFramesRemaining--;
			u++;
		}
	}
}

void* Flux_ImGuiIntegration::GetImTextureID(Flux_ImGuiTextureHandle xHandle)
{
	// The descriptor set can be cast directly to ImTextureID
	return reinterpret_cast<void*>(xHandle.AsUInt64());
}

#endif // ZENITH_TOOLS
