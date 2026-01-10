#pragma once

#ifdef ZENITH_TOOLS

#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_Entity.h"

//=============================================================================
// Hierarchy Panel
//
// Displays the scene entity hierarchy as a tree view with:
// - Drag-drop reparenting
// - Multi-selection (Ctrl+click, Shift+click)
// - Context menu for entity creation/deletion
//=============================================================================

namespace Zenith_EditorPanelHierarchy
{
	/**
	 * Render the hierarchy panel
	 *
	 * @param xScene Reference to the current scene
	 * @param uGameCameraEntityID Reference to game camera entity (set to invalid if deleted)
	 */
	void Render(Zenith_Scene& xScene, Zenith_EntityID& uGameCameraEntityID);
}

#endif // ZENITH_TOOLS
