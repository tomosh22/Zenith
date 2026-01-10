#pragma once

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_Editor.h"

//=============================================================================
// Toolbar Panel
//
// Displays play/pause/stop controls and gizmo mode selection.
//=============================================================================

namespace Zenith_EditorPanelToolbar
{
	/**
	 * Render the toolbar panel
	 *
	 * @param eEditorMode Current editor mode (reference allows modification)
	 * @param eGizmoMode Current gizmo mode (reference allows modification)
	 */
	void Render(EditorMode& eEditorMode, EditorGizmoMode& eGizmoMode);
}

#endif // ZENITH_TOOLS
