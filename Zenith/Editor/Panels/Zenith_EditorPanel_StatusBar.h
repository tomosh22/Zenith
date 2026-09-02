#pragma once

#ifdef ZENITH_TOOLS

//=============================================================================
// Status Bar
//
// The strip along the bottom of the editor: the latest log line (click to
// reveal the Console), then scene / selection / frame-time facts on the right.
// Drawn inside the dockspace host window by Zenith_Editor::Render, never as a
// dockable window of its own.
//=============================================================================

class Zenith_Editor;

namespace Zenith_EditorPanelStatusBar
{
	// Height of the strip in pixels at the current DPI.
	float GetHeight();

	// Draws the strip in the current window at the cursor, fHeight tall.
	void Render(Zenith_Editor& xEditor, float fHeight);
}

#endif // ZENITH_TOOLS
