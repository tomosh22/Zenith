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

	/**
	 * Check whether uCandidateAncestor is an ancestor of uTarget in the entity hierarchy.
	 * Returns false if uCandidateAncestor == uTarget (self is not an ancestor of self).
	 *
	 * @param uCandidateAncestor The entity to test as a potential ancestor
	 * @param uTarget            The entity whose ancestry chain is walked
	 * @return true if uCandidateAncestor appears in uTarget's parent chain
	 */
	bool IsAncestorOf(Zenith_EntityID uCandidateAncestor, Zenith_EntityID uTarget);

	//-------------------------------------------------------------------------
	// Internal per-section helpers invoked by Render(). Declared here so they
	// can be unit-testable and to keep Render() a thin dispatcher.
	//-------------------------------------------------------------------------

	/**
	 * Iterate all loaded scenes, rendering each as a collapsible section with
	 * its header styling, drag-drop target, context menu, and entity tree.
	 * Shared drag/drop scratch state is collected into the reference parameters
	 * so the main Render() function can act on it after the loop completes.
	 */
	void RenderScenesSection(
		Zenith_EntityID& uEntityToDelete,
		Zenith_EntityID& uDraggedEntityID,
		Zenith_EntityID& uDropTargetEntityID,
		Zenith_Scene& xDropTargetScene);

	/**
	 * Render the empty drop-zone at the bottom of the panel used to unparent
	 * entities by dragging them to empty space and to load scene files from
	 * the Content Browser additively.
	 */
	void RenderRootDropTargetSection();

	/**
	 * Apply a drag-drop reparent operation collected by RenderScenesSection.
	 * Skips the reparent if it would create a cycle in the hierarchy.
	 */
	void ProcessDeferredReparenting(
		Zenith_EntityID uDraggedEntityID,
		Zenith_EntityID uDropTargetEntityID);

	/**
	 * Destroy the entity flagged for deletion this frame (via the context
	 * menu). If it was the active game camera the reference is cleared.
	 */
	void ProcessDeferredEntityDeletion(
		Zenith_EntityID uEntityToDelete,
		Zenith_EntityID& uGameCameraEntityID);

	/**
	 * Render the bottom separator and the "+ Create Entity" button that
	 * creates a new entity in the active scene.
	 */
	void RenderCreateEntityFooter();
}

#endif // ZENITH_TOOLS
