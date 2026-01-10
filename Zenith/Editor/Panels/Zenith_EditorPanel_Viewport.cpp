#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_Viewport.h"
#include "Flux/Flux_Graphics.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

void Zenith_EditorPanelViewport::Render(ViewportState& xState)
{
	ImGui::Begin("Viewport");

	// Track viewport position for mouse picking
	ImVec2 xViewportPanelPos = ImGui::GetCursorScreenPos();
	xState.m_xViewportPos = { xViewportPanelPos.x, xViewportPanelPos.y };

	// Get the final render target SRV
	Flux_ShaderResourceView& xGameRenderSRV = Flux_Graphics::s_xFinalRenderTarget.m_axColourAttachments[0].m_pxSRV;

	if (xGameRenderSRV.m_xImageView != VK_NULL_HANDLE)
	{
		// Check if the image view has changed (e.g., due to window resize)
		// Only allocate a new descriptor set if necessary to avoid exhausting the pool
		if (xState.m_xCachedImageView != xGameRenderSRV.m_xImageView)
		{
			// Queue old descriptor set for deferred deletion
			// We can't free it immediately because the GPU may still be using it in in-flight command buffers
			// Vulkan spec requires waiting for all commands referencing the descriptor set to complete
			if (xState.m_xCachedGameTextureDescriptorSet != VK_NULL_HANDLE)
			{
				// Wait 3 frames before deletion to ensure GPU has finished
				// This accounts for frames in flight (typically 2-3 frames buffered)
				constexpr u_int FRAMES_TO_WAIT = 3;
				xState.m_xPendingDeletions.push_back({
					xState.m_xCachedGameTextureDescriptorSet,
					FRAMES_TO_WAIT
				});
			}

			// Allocate new descriptor set for the game viewport texture
			xState.m_xCachedGameTextureDescriptorSet = ImGui_ImplVulkan_AddTexture(
				Flux_Graphics::s_xRepeatSampler.GetSampler(),
				xGameRenderSRV.m_xImageView,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			);

			// Cache the image view so we know when it changes
			xState.m_xCachedImageView = xGameRenderSRV.m_xImageView;
		}

		// Get available content region size
		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();

		// Store viewport size for object picking
		xState.m_xViewportSize = { viewportPanelSize.x, viewportPanelSize.y };

		// Track viewport hover/focus state for input handling
		xState.m_bViewportHovered = ImGui::IsWindowHovered();
		xState.m_bViewportFocused = ImGui::IsWindowFocused();

		// Display the game render target as an image using the cached descriptor set
		if (xState.m_xCachedGameTextureDescriptorSet != VK_NULL_HANDLE)
		{
			ImGui::Image((ImTextureID)(uintptr_t)static_cast<VkDescriptorSet>(xState.m_xCachedGameTextureDescriptorSet), viewportPanelSize);
		}
		else
		{
			ImGui::Text("Viewport texture not yet initialized");
		}
	}
	else
	{
		ImGui::Text("Game render target not available");
	}

	ImGui::End();
}

#endif // ZENITH_TOOLS
