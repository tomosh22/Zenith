#pragma once

#ifdef ZENITH_TOOLS

#include "Maths/Zenith_Maths.h"
#include "EntityComponent/Zenith_Scene.h"
#include <vector>
#include <string>
#include <unordered_set>
#include <bitset>

// Forward declarations
class Zenith_MaterialAsset;
struct ContentBrowserEntry;
struct ConsoleLogEntry;

enum class EditorMode;
enum class EditorGizmoMode;

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

	bool m_bPendingSceneSave = false;
	std::string m_strPendingSceneSavePath;

	bool m_bPendingSceneReset = false;

	// Scene backup for play mode
	bool m_bHasSceneBackup = false;
	std::string m_strBackupScenePath;

	void Reset()
	{
		m_bPendingSceneLoad = false;
		m_strPendingSceneLoadPath.clear();
		m_bPendingSceneSave = false;
		m_strPendingSceneSavePath.clear();
		m_bPendingSceneReset = false;
	}
};

//-----------------------------------------------------------------------------
// Content Browser State
//-----------------------------------------------------------------------------
struct Zenith_EditorContentBrowserState
{
	std::string m_strCurrentDirectory;
	std::vector<ContentBrowserEntry> m_xDirectoryContents;
	std::vector<ContentBrowserEntry> m_xFilteredContents;
	bool m_bDirectoryNeedsRefresh = true;
	char m_szSearchBuffer[256] = "";
	int m_iAssetTypeFilter = 0;      // 0 = All, then asset types
	int m_iSelectedContentIndex = -1;
};

//-----------------------------------------------------------------------------
// Console State
//-----------------------------------------------------------------------------
struct Zenith_EditorConsoleState
{
	std::vector<ConsoleLogEntry> m_xLogs;
	bool m_bAutoScroll = true;
	bool m_bShowInfo = true;
	bool m_bShowWarnings = true;
	bool m_bShowErrors = true;
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

	// Movement
	float m_fMoveSpeed = 50.0f;
	float m_fRotateSpeed = 0.1f;

	// State
	bool m_bInitialized = false;
	Zenith_EntityID m_uGameCameraEntity = INVALID_ENTITY_ID;

	void ResetToDefaults()
	{
		m_xPosition = { 0, 100, 0 };
		m_fPitch = 0.0;
		m_fYaw = 0.0;
		m_fFOV = 45.0f;
		m_fNear = 1.0f;
		m_fFar = 2000.0f;
	}
};

//-----------------------------------------------------------------------------
// Material Editor State
//-----------------------------------------------------------------------------
struct Zenith_EditorMaterialState
{
	Zenith_MaterialAsset* m_pxSelectedMaterial = nullptr;
	bool m_bShowEditor = true;
};

//-----------------------------------------------------------------------------
// Combined Editor State
//-----------------------------------------------------------------------------
struct Zenith_EditorState
{
	// Mode
	EditorMode m_eEditorMode;
	EditorGizmoMode m_eGizmoMode;

	// Sub-states
	Zenith_EditorSelectionState m_xSelection;
	Zenith_EditorViewportState m_xViewport;
	Zenith_EditorDeferredOpsState m_xDeferredOps;
	Zenith_EditorContentBrowserState m_xContentBrowser;
	Zenith_EditorConsoleState m_xConsole;
	Zenith_EditorCameraState m_xCamera;
	Zenith_EditorMaterialState m_xMaterial;
};

#endif // ZENITH_TOOLS
