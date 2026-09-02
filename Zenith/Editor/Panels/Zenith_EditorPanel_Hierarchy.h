#pragma once

#ifdef ZENITH_TOOLS

#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_Entity.h"

// Forward declarations
class Zenith_SceneData;
struct Zenith_EditorHierarchyState;

//=============================================================================
// Hierarchy Panel (Unity-Style Multi-Scene)
//
// Displays all loaded scenes as collapsible divider bars with:
// - Scene headers showing active (bright), dirty (*), entity count
// - Per-scene context menus (Set Active, Save, Unload, Create Entity)
// - Entity tree view: component icon, name, enabled toggle; drag-drop
//   reparenting (undoable); inline rename (double-click / F2)
// - Search that filters the tree while keeping matching subtrees visible
// - Cross-scene entity operations (Move To Scene, DontDestroyOnLoad)
// - Multi-selection (Ctrl+click, Shift+click)
//=============================================================================

namespace Zenith_EditorPanelHierarchy
{
	// Render the hierarchy panel showing all loaded scenes.
	// uGameCameraEntityID is cleared if that entity is deleted from the panel.
	void Render(Zenith_EditorHierarchyState& xState, Zenith_EntityID& uGameCameraEntityID);

	// Check whether uCandidateAncestor is an ancestor of uTarget in the entity
	// hierarchy. Returns false if uCandidateAncestor == uTarget.
	bool IsAncestorOf(Zenith_EntityID uCandidateAncestor, Zenith_EntityID uTarget);

	// PURE: the search rule. An entity matches when its name (or one of its
	// component display names) contains szQuery, case-insensitively; an empty
	// query matches everything.
	bool MatchesSearch(const char* szEntityName, const char* szComponentSummary, const char* szQuery);

	// Begins an inline rename of xEntity (the field grabs focus next frame).
	void BeginRename(Zenith_EditorHierarchyState& xState, Zenith_EntityID xEntity);
}

#endif // ZENITH_TOOLS
