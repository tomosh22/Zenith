#pragma once

#ifdef ZENITH_TOOLS

#include "Maths/Zenith_Maths.h"
#include "Flux/Flux_Types.h"
#include "Flux/Flux_ImGuiIntegration.h"
#include "Collections/Zenith_Vector.h"

//=============================================================================
// Viewport Panel
//
// Displays the game render target, tracks the viewport rect / hover / focus
// for picking, and draws the editor overlays on top of the image: the mode
// badge, the statistics block, the navigation hint and the axis widget.
//=============================================================================

class Zenith_Editor;

// Pending ImGui texture deletion entry
struct PendingImGuiTextureDeletion
{
	Flux_ImGuiTextureHandle xHandle;
	u_int uFramesUntilDeletion;
};

namespace Zenith_EditorPanelViewport
{
	void Render(Zenith_Editor& xEditor);

	// PURE: projects a world-space direction into the 2D screen direction the
	// axis widget draws it along, using the view rotation only. Returns the
	// view-space Z (depth) so the caller can draw far axes first.
	float ProjectAxisForWidget(const Zenith_Maths::Matrix4& xViewMatrix, const Zenith_Maths::Vector3& xWorldAxis, Zenith_Maths::Vector2& xOutScreenDir);
}

#endif // ZENITH_TOOLS
