#pragma once

#ifdef ZENITH_TOOLS

#include "Maths/Zenith_Maths.h"
#include "Flux/Flux_Types.h"
#include "Flux/Flux_ImGuiIntegration.h"
#include <vector>

//=============================================================================
// Viewport Panel
//
// Displays the game render target and handles viewport state tracking.
//=============================================================================

// Pending ImGui texture deletion entry
struct PendingImGuiTextureDeletion
{
	Flux_ImGuiTextureHandle xHandle;
	u_int uFramesUntilDeletion;
};

// Viewport state structure
struct ViewportState
{
	Zenith_Maths::Vector2& m_xViewportSize;
	Zenith_Maths::Vector2& m_xViewportPos;
	bool& m_bViewportHovered;
	bool& m_bViewportFocused;
	Flux_ImGuiTextureHandle& m_xCachedGameTextureHandle;
	Flux_ImageViewHandle& m_xCachedImageViewHandle;
	std::vector<PendingImGuiTextureDeletion>& m_xPendingDeletions;
};

namespace Zenith_EditorPanelViewport
{
	/**
	 * Render the viewport panel
	 *
	 * @param xState Reference to viewport state
	 */
	void Render(ViewportState& xState);
}

#endif // ZENITH_TOOLS
