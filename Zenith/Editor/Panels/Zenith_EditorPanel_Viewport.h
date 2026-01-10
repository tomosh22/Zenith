#pragma once

#ifdef ZENITH_TOOLS

#include "Maths/Zenith_Maths.h"
#include "Vulkan/Zenith_Vulkan.h"
#include <vector>

//=============================================================================
// Viewport Panel
//
// Displays the game render target and handles viewport state tracking.
//=============================================================================

// Pending descriptor set deletion entry
struct PendingDescriptorSetDeletion
{
	VkDescriptorSet descriptorSet;
	u_int framesUntilDeletion;
};

// Viewport state structure
struct ViewportState
{
	Zenith_Maths::Vector2& m_xViewportSize;
	Zenith_Maths::Vector2& m_xViewportPos;
	bool& m_bViewportHovered;
	bool& m_bViewportFocused;
	vk::DescriptorSet& m_xCachedGameTextureDescriptorSet;
	vk::ImageView& m_xCachedImageView;
	std::vector<PendingDescriptorSetDeletion>& m_xPendingDeletions;
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
