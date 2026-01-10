#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_Viewport.h"
#include "Flux/Flux_Graphics.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

void Zenith_EditorPanelViewport::Render(ViewportState& xState)
{
	ImGui::Begin("Viewport");

	// Track viewport position for mouse picking
	ImVec2 xViewportPanelPos = ImGui::GetCursorScreenPos();
	xState.m_xViewportPos = { xViewportPanelPos.x, xViewportPanelPos.y };

	// Get the final render target SRV
	Flux_ShaderResourceView& xGameRenderSRV = Flux_Graphics::s_xFinalRenderTarget.m_axColourAttachments[0].m_pxSRV;

	if (xGameRenderSRV.m_xImageViewHandle.IsValid())
	{
		// Check if the image view has changed (e.g., due to window resize)
		// Only allocate a new descriptor set if necessary to avoid exhausting the pool
		if (xState.m_xCachedImageViewHandle.AsUInt() != xGameRenderSRV.m_xImageViewHandle.AsUInt())
		{
			// Queue old handle for deferred deletion
			// We can't free it immediately because the GPU may still be using it in in-flight command buffers
			if (xState.m_xCachedGameTextureHandle.IsValid())
			{
				// Wait 3 frames before deletion to ensure GPU has finished
				// This accounts for frames in flight (typically 2-3 frames buffered)
				constexpr u_int FRAMES_TO_WAIT = 3;
				xState.m_xPendingDeletions.push_back({
					xState.m_xCachedGameTextureHandle,
					FRAMES_TO_WAIT
				});
			}

			// Allocate new ImGui texture handle for the game viewport texture
			xState.m_xCachedGameTextureHandle = Flux_ImGuiIntegration::RegisterTexture(
				xGameRenderSRV,
				Flux_Graphics::s_xRepeatSampler
			);

			// Cache the image view handle so we know when it changes
			xState.m_xCachedImageViewHandle = xGameRenderSRV.m_xImageViewHandle;
		}

		// Get available content region size
		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();

		// Store viewport size for object picking
		xState.m_xViewportSize = { viewportPanelSize.x, viewportPanelSize.y };

		// Track viewport hover/focus state for input handling
		xState.m_bViewportHovered = ImGui::IsWindowHovered();
		xState.m_bViewportFocused = ImGui::IsWindowFocused();

		// Display the game render target as an image using the cached handle
		if (xState.m_xCachedGameTextureHandle.IsValid())
		{
			ImGui::Image((ImTextureID)Flux_ImGuiIntegration::GetImTextureID(xState.m_xCachedGameTextureHandle), viewportPanelSize);
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
