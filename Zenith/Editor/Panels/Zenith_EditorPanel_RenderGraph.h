#pragma once

#ifdef ZENITH_TOOLS

//=============================================================================
// Render Graph Panel
//
// Displays the current state of Flux_RenderGraph, showing all passes,
// their execution order (topological levels), resource dependencies,
// and enable/disable states.
//=============================================================================

namespace Zenith_EditorPanelRenderGraph
{
	void Render();

	void SetVisible(bool bVisible);
	bool IsVisible();
}

#endif // ZENITH_TOOLS