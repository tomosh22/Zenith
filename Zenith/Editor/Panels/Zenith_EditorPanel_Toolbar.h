#pragma once

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_Editor.h"

//=============================================================================
// Toolbar Strip
//
// The single row under the menu bar: undo/redo, gizmo mode, gizmo space and
// snapping on the left, transport (play / pause / stop) in the centre, scene
// selectors on the right. Drawn inside the dockspace host window by
// Zenith_Editor::Render, so it can never be docked away, closed or clipped.
//=============================================================================

namespace Zenith_EditorPanelToolbar
{
	// Height of the strip in pixels at the current DPI.
	float GetHeight();

	// Draws the strip in the current window at the cursor, fHeight tall.
	void Render(Zenith_Editor& xEditor, float fHeight);

	//-------------------------------------------------------------------------
	// Sections (split out so each owns one coherent group of controls)
	//-------------------------------------------------------------------------
	void RenderHistoryButtons();
	void RenderGizmoButtons(Zenith_Editor& xEditor);
	void RenderTransportButtons(Zenith_Editor& xEditor, float fStripWidth, float fHeight);
	void RenderSceneSelectors(Zenith_Editor& xEditor, float fRightEdge, float fRowTop);
	void RenderActiveSceneCombo();
	void RenderRegisteredScenesCombo();
}

#endif // ZENITH_TOOLS
