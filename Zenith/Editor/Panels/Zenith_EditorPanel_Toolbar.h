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

	//-------------------------------------------------------------------------
	// Private helpers - split from Render() by toolbar section
	//-------------------------------------------------------------------------

	/** Row 1: Play/Pause and Stop buttons (centered). */
	void RenderPlayModeButtons(EditorMode& eEditorMode, float fButtonWidth, float fButtonHeight, float fSpacing, float fWindowWidth);

	/** Scene selector combo plus the "Load Scene" build-registry combo. */
	void RenderSceneSelectors(EditorMode& eEditorMode);

	/** Active scene dropdown (picks between currently loaded scenes). */
	void RenderActiveSceneCombo();

	/** Registered build-index scenes dropdown (triggers a deferred load). */
	void RenderRegisteredScenesCombo();

	/** Row 2: Translate/Rotate/Scale gizmo radio buttons (centered). */
	void RenderGizmoModeRadios(EditorGizmoMode& eGizmoMode, float fSpacing, float fWindowWidth);
}

#endif // ZENITH_TOOLS
