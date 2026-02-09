#pragma once

#ifdef ZENITH_TOOLS

#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_Entity.h"

// Forward declaration
class Zenith_SceneData;

//=============================================================================
// Hierarchy Panel (Unity-Style Multi-Scene)
//
// Displays all loaded scenes as collapsible divider bars with:
// - Scene headers showing active (bold), dirty (*), entity count
// - Per-scene context menus (Set Active, Save, Unload, Create Entity)
// - Entity tree view with drag-drop reparenting
// - Cross-scene entity operations (Move To Scene, DontDestroyOnLoad)
// - Multi-selection (Ctrl+click, Shift+click)
//=============================================================================

namespace Zenith_EditorPanelHierarchy
{
	/**
	 * Render the hierarchy panel showing all loaded scenes
	 *
	 * @param uGameCameraEntityID Reference to game camera entity (set to invalid if deleted)
	 */
	void Render(Zenith_EntityID& uGameCameraEntityID);
}

#endif // ZENITH_TOOLS
