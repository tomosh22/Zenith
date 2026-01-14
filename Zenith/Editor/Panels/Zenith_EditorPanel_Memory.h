#pragma once

#ifdef ZENITH_TOOLS
#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED

//=============================================================================
// Memory Debug Panel
//
// Displays memory statistics, allocation list, and budget configuration.
//=============================================================================

namespace Zenith_EditorPanelMemory
{
	// Render the memory debug panel
	void Render();

	// Show/hide the panel
	void SetVisible(bool bVisible);
	bool IsVisible();
}

#endif // ZENITH_MEMORY_MANAGEMENT_ENABLED
#endif // ZENITH_TOOLS
