#pragma once

#ifdef ZENITH_TOOLS

#include "AssetHandling/Zenith_AssetHandle.h"  // MaterialHandle
#include "ZenithECS/Zenith_SceneData.h"  // Zenith_EntityID (by value) + entity/component access (no longer transitive via the now-opaque Scene.h)
#include "Collections/Zenith_Vector.h"
#include "Editor/Zenith_EditorCommands.h"   // Zenith_EditorInspectorUndoTracker
#include "Editor/Zenith_EditorPrefs.h"
#include <string>
#include <unordered_set>
#include <bitset>

//=============================================================================
// Zenith_EditorState
//
// Centralized state container for the editor. Previously scattered across
// 30+ static member variables in Zenith_Editor. This struct groups related
// state for better organization and enables easier testing/serialization.
//=============================================================================

//-----------------------------------------------------------------------------
// Selection State
//-----------------------------------------------------------------------------
struct Zenith_EditorSelectionState
{
	std::unordered_set<Zenith_EntityID> m_xSelectedEntityIDs;
	Zenith_EntityID m_uPrimarySelectedEntityID = INVALID_ENTITY_ID;
	Zenith_EntityID m_uLastClickedEntityID = INVALID_ENTITY_ID;

	void Clear()
	{
		m_xSelectedEntityIDs.clear();
		m_uPrimarySelectedEntityID = INVALID_ENTITY_ID;
		m_uLastClickedEntityID = INVALID_ENTITY_ID;
	}

	bool HasSelection() const { return !m_xSelectedEntityIDs.empty(); }
	bool HasMultiSelection() const { return m_xSelectedEntityIDs.size() > 1; }
	size_t GetCount() const { return m_xSelectedEntityIDs.size(); }
	bool IsSelected(Zenith_EntityID uID) const { return m_xSelectedEntityIDs.count(uID) > 0; }
};

//-----------------------------------------------------------------------------
// Viewport State
//-----------------------------------------------------------------------------
struct Zenith_EditorViewportState
{
	Zenith_Maths::Vector2 m_xSize = { 1280, 720 };
	Zenith_Maths::Vector2 m_xPosition = { 0, 0 };
	bool m_bHovered = false;
	bool m_bFocused = false;
};

//-----------------------------------------------------------------------------
// Deferred Operations State
// Operations that must wait until Update() to execute safely
//-----------------------------------------------------------------------------
struct Zenith_EditorDeferredOpsState
{
	bool m_bPendingSceneLoad = false;
	std::string m_strPendingSceneLoadPath;

	// Loading a registered scene by build index
	bool m_bPendingRegisteredSceneLoad = false;
	int m_iPendingRegisteredSceneBuildIndex = -1;

	// Loading a scene from an arbitrary file path
	bool m_bPendingSceneLoadFromFile = false;
	std::string m_strPendingSceneLoadFromFilePath;

	void Reset()
	{
		m_bPendingSceneLoad = false;
		m_strPendingSceneLoadPath.clear();
		m_bPendingRegisteredSceneLoad = false;
		m_iPendingRegisteredSceneBuildIndex = -1;
		m_bPendingSceneLoadFromFile = false;
		m_strPendingSceneLoadFromFilePath.clear();
	}
};

//-----------------------------------------------------------------------------
// Play-mode Scene Backup State
// Captured on Stopped -> Playing, restored on Playing -> Stopped.
//-----------------------------------------------------------------------------
struct Zenith_EditorPlayBackupState
{
	bool m_bHasBackup = false;
	std::string m_strBackupScenePath;
	int m_iBackupSceneHandle = -1;
	std::string m_strBackupSceneName;
	std::string m_strBackupOriginalPath;
	int m_iBackupBuildIndex = -1;

	void Reset()
	{
		m_bHasBackup = false;
		m_strBackupScenePath.clear();
		m_iBackupSceneHandle = -1;
		m_strBackupSceneName.clear();
		m_strBackupOriginalPath.clear();
		m_iBackupBuildIndex = -1;
	}
};

//-----------------------------------------------------------------------------
// Content Browser State
//-----------------------------------------------------------------------------
struct Zenith_EditorContentBrowserState
{
	std::string m_strCurrentDirectory;
	Zenith_Vector<ContentBrowserEntry> m_xDirectoryContents;
	Zenith_Vector<ContentBrowserEntry> m_xFilteredContents;
	bool m_bDirectoryNeedsRefresh = true;
	char m_szSearchBuffer[256] = "";
	int m_iAssetTypeFilter = 0;      // 0 = All, then asset types
	int m_iSelectedContentIndex = -1;
	float m_fThumbnailSize = 88.0f;  // Range: 48-200 pixels (at 1x DPI)
	Zenith_Vector<std::string> m_axNavigationHistory;
	int m_iHistoryIndex = -1;
	ContentBrowserViewMode m_eViewMode = ContentBrowserViewMode::Grid;
	// Folder tree pane on the left (Unreal-style); width is user-resizable.
	bool m_bShowFolderTree = true;
	float m_fFolderTreeWidth = 190.0f;
	static constexpr int MAX_HISTORY_SIZE = 50;
};

//-----------------------------------------------------------------------------
// Console State
//-----------------------------------------------------------------------------
struct Zenith_EditorConsoleState
{
	Zenith_Vector<ConsoleLogEntry> m_xLogs;
	bool m_bAutoScroll = true;
	bool m_bShowInfo = true;
	bool m_bShowWarnings = true;
	bool m_bShowErrors = true;
	// Collapse consecutive identical messages into one row with a count.
	bool m_bCollapse = false;
	char m_szSearch[128] = "";
	std::bitset<LOG_CATEGORY_COUNT> m_xCategoryFilters;

	static constexpr size_t MAX_ENTRIES = 1000;

	Zenith_EditorConsoleState()
	{
		m_xCategoryFilters.set(); // Enable all categories by default
	}
};

//-----------------------------------------------------------------------------
// Editor Camera State
// Standalone camera not part of entity/scene system
//-----------------------------------------------------------------------------
struct Zenith_EditorCameraState
{
	// Position and orientation
	Zenith_Maths::Vector3 m_xPosition = { 0, 100, 0 };
	double m_fPitch = 0.0;
	double m_fYaw = 0.0;

	// Projection
	float m_fFOV = 45.0f;
	float m_fNear = 1.0f;
	float m_fFar = 2000.0f;

	// Movement. Look sensitivity is NOT mirrored here: it has one home in
	// Zenith_EditorPrefs, because no gesture changes it live (the wheel changes
	// the move speed, which is why that one IS mirrored).
	float m_fMoveSpeed = 50.0f;

	// Navigation: the point orbit and dolly work around. Set by Focus (F) and
	// refreshed from the selection; kept when nothing is selected.
	Zenith_Maths::Vector3 m_xPivot = { 0, 0, 0 };
	float m_fPivotDistance = 20.0f;

	// Smooth focus flight (F). Interpolates m_xPosition from start to end over
	// a short fixed duration.
	bool m_bFocusAnimating = false;
	float m_fFocusT = 0.0f;
	Zenith_Maths::Vector3 m_xFocusStart = { 0, 0, 0 };
	Zenith_Maths::Vector3 m_xFocusEnd = { 0, 0, 0 };

	// Which navigation gesture is in flight (they are mutually exclusive).
	bool m_bLooking = false;    // RMB held: mouse look + WASD fly
	bool m_bOrbiting = false;   // Alt+LMB: orbit the pivot
	bool m_bPanning = false;    // MMB: pan the view plane

	// This frame's pointer delta, recorded where the input reference already
	// exists so the viewport overlay can report it without reaching for the
	// engine singleton. Diagnostic only — the look maths reads the input.
	Zenith_Maths::Vector2_64 m_xLastMouseDelta = { 0.0, 0.0 };

	// State
	bool m_bInitialized = false;
	Zenith_EntityID m_uGameCameraEntity = INVALID_ENTITY_ID;
};

//-----------------------------------------------------------------------------
// Gizmo interaction bookkeeping (the drag itself lives in Flux_Gizmos).
//-----------------------------------------------------------------------------
struct Zenith_EditorGizmoState
{
	// A drag is in flight on this entity; on release its (initial -> final)
	// transform becomes one undo command.
	bool m_bDragActive = false;
	Zenith_EntityID m_xDragEntity = INVALID_ENTITY_ID;
};

//-----------------------------------------------------------------------------
// Hierarchy panel state
//-----------------------------------------------------------------------------
struct Zenith_EditorHierarchyState
{
	char m_szSearch[128] = "";
	// Inline rename: the entity being renamed and the text buffer; the field
	// grabs keyboard focus on the frame the rename starts.
	Zenith_EntityID m_xRenaming = INVALID_ENTITY_ID;
	char m_szRenameBuffer[256] = "";
	bool m_bRenameFocusPending = false;
	// Set by anything that selects an entity from outside the panel so the tree
	// scrolls it into view once.
	Zenith_EntityID m_xScrollTo = INVALID_ENTITY_ID;
	// The panel (or a child of it) had keyboard focus last frame; entity
	// shortcuts (Delete, Ctrl+D, F) are scoped to it and the viewport.
	bool m_bFocused = false;
};

//-----------------------------------------------------------------------------
// Properties (inspector) panel state
//-----------------------------------------------------------------------------
struct Zenith_EditorInspectorState
{
	Zenith_EditorInspectorUndoTracker m_xUndoTracker;
	char m_szAddComponentSearch[64] = "";
	// The entity name field edits a copy; the rename command is recorded when
	// the field is deactivated after an edit.
	Zenith_EntityID m_xNameEditEntity = INVALID_ENTITY_ID;
	char m_szNameBuffer[256] = "";
	std::string m_strNameBeforeEdit;
};

//-----------------------------------------------------------------------------
// Modal prompt state ("Save changes?" before an action that would drop them).
//-----------------------------------------------------------------------------
struct Zenith_EditorPromptState
{
	enum class Action
	{
		None,
		NewScene,
		OpenScene,       // m_strPath holds the file
		Exit,
	};
	Action m_eAction = Action::None;
	std::string m_strPath;
	bool m_bOpenRequested = false;
};

//-----------------------------------------------------------------------------
// Material Editor State
//-----------------------------------------------------------------------------
struct Zenith_EditorMaterialState
{
	// A MaterialHandle (not a raw pointer) so the asset survives
	// UnloadUnusedAssets cycles while selected.
	MaterialHandle m_xSelectedMaterial;
	bool m_bShowEditor = true;
};

//-----------------------------------------------------------------------------
// Panel Visibility (Window menu toggles)
//-----------------------------------------------------------------------------
struct Zenith_EditorPanelVisibility
{
	bool m_bShowHierarchy = true;
	bool m_bShowProperties = true;
	bool m_bShowConsole = true;
	bool m_bShowContentBrowser = true;
	bool m_bShowTerrainEditor = false;
	bool m_bShowShortcuts = false;
};

//-----------------------------------------------------------------------------
// Combined Editor State — the single source of truth for editor domain state.
// (Zenith_Editor keeps only runtime GPU/frame caches outside this struct.)
//-----------------------------------------------------------------------------
struct Zenith_EditorState
{
	// Mode
	EditorMode m_eEditorMode = EditorMode::Stopped;
	EditorGizmoMode m_eGizmoMode = EditorGizmoMode::Translate;

	// Sub-states
	Zenith_EditorSelectionState m_xSelection;
	Zenith_EditorViewportState m_xViewport;
	Zenith_EditorDeferredOpsState m_xDeferredOps;
	Zenith_EditorPlayBackupState m_xPlayBackup;
	Zenith_EditorContentBrowserState m_xContentBrowser;
	Zenith_EditorConsoleState m_xConsole;
	Zenith_EditorCameraState m_xCamera;
	Zenith_EditorGizmoState m_xGizmo;
	Zenith_EditorHierarchyState m_xHierarchy;
	Zenith_EditorInspectorState m_xInspector;
	Zenith_EditorPromptState m_xPrompt;
	Zenith_EditorMaterialState m_xMaterial;
	Zenith_EditorPanelVisibility m_xPanels;

	// User preferences (recent scenes, camera speed, snapping ...). Loaded at
	// Initialise, saved whenever one of them changes.
	Zenith_EditorPrefs m_xPrefs;

	// Window > Reset Layout: consumed by the next Render(), which rebuilds the
	// code-defined default dock layout (also recaptures floating windows).
	bool m_bResetDockLayout = false;

	// The last title pushed to the OS window, so it is only re-set on change.
	std::string m_strWindowTitle;

	// Interactive runs open maximised once, like every other editor.
	bool m_bMaximiseRequested = false;
};

#endif // ZENITH_TOOLS
