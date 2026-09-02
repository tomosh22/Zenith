#pragma once

#ifdef ZENITH_TOOLS

#include "ZenithECS/Zenith_SceneData.h"
#include <string>

//=============================================================================
// Zenith_EditorActions — the verbs the editor exposes.
//
// Every user-facing operation lives here ONCE and is reached from wherever it is
// triggered: a menu item, a keyboard shortcut, a toolbar button, a hierarchy
// context menu, a drag-drop. That is what keeps "Delete" behaving identically
// whether it came from the Delete key or the right-click menu, and it is where
// undo recording and scene-dirtying happen, so no panel has to remember them.
//=============================================================================
namespace Zenith_EditorActions
{
	//-------------------------------------------------------------------------
	// Entities
	//-------------------------------------------------------------------------

	enum class CreateKind
	{
		Empty,
		Camera,
		PointLight,
		SpotLight,
		DirectionalLight,
	};
	const char* GetCreateKindName(CreateKind eKind);

	// Creates an entity of the given kind in the active scene (parented to
	// xParent when valid), placed at the editor camera's placement point,
	// selects it, and records an undoable "Create" command. INVALID on failure.
	Zenith_EntityID CreateEntity(CreateKind eKind, Zenith_EntityID xParent = INVALID_ENTITY_ID);

	// Deletes every selected entity (children go with their parents) as ONE
	// undo step. Returns the number of root entities deleted.
	u_int DeleteSelection();

	// Duplicates every selected root entity beside its original (children
	// included), selects the copies, one undo step. Returns copies made.
	u_int DuplicateSelection();

	void RenameEntity(Zenith_EntityID xEntity, const std::string& strNewName);
	void SetEntityEnabled(Zenith_EntityID xEntity, bool bEnabled);
	// Reparents (INVALID = make root). Refuses cycles and self-parenting.
	bool ReparentEntity(Zenith_EntityID xEntity, Zenith_EntityID xNewParent);

	bool AddComponent(Zenith_EntityID xEntity, const char* szDisplayName);
	bool RemoveComponent(Zenith_EntityID xEntity, const char* szDisplayName);

	// Selection helpers
	void SelectAll();
	void DeselectAll();
	void FocusSelection();

	//-------------------------------------------------------------------------
	// Scenes
	//-------------------------------------------------------------------------

	// Each of these checks for unsaved changes first and, when there are any,
	// opens the "Save changes?" prompt instead of acting; the prompt re-enters
	// the action once the user has decided.
	void NewScene();
	void OpenSceneDialog();
	void OpenScenePath(const std::string& strPath);
	void OpenSceneAdditiveDialog();
	void RequestExit();

	// Save to the scene's own path (falls back to Save As when it has none).
	// Returns true when a file was written.
	bool SaveScene();
	bool SaveSceneAs();

	// True when any loaded scene has unsaved changes.
	bool HasUnsavedChanges();
	// Name of the active scene ("Untitled" when unnamed).
	std::string GetActiveSceneDisplayName();

	//-------------------------------------------------------------------------
	// Play mode
	//-------------------------------------------------------------------------
	void TogglePlay();     // Stopped -> Playing, Playing/Paused -> Stopped
	void TogglePause();    // Playing <-> Paused
	void Stop();

	//-------------------------------------------------------------------------
	// Bookkeeping shared by panels
	//-------------------------------------------------------------------------
	// Marks the scene that owns xEntity as having unsaved changes.
	void MarkSceneDirtyForEntity(Zenith_EntityID xEntity);
}

#endif // ZENITH_TOOLS
