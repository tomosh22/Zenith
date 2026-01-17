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

// Drag-drop payload type identifiers (max 32 chars per ImGui)
#define DRAGDROP_PAYLOAD_TEXTURE  "ZENITH_TEXTURE"
#define DRAGDROP_PAYLOAD_MESH    "ZENITH_MESH"
#define DRAGDROP_PAYLOAD_MATERIAL "ZENITH_MATERIAL"
#define DRAGDROP_PAYLOAD_PREFAB   "ZENITH_PREFAB"
#define DRAGDROP_PAYLOAD_MODEL    "ZENITH_MODEL"
#define DRAGDROP_PAYLOAD_ANIMATION "ZENITH_ANIMATION"
#define DRAGDROP_PAYLOAD_FILE_GENERIC "ZENITH_FILE_GENERIC"

// Content browser file entry
struct ContentBrowserEntry
{
	std::string m_strName;           // Display name (filename without path)
	std::string m_strFullPath;       // Full absolute path
	std::string m_strExtension;      // File extension (e.g., ZENITH_TEXTURE_EXT)
	bool m_bIsDirectory;             // true for folders, false for files
};

// Drag-drop payload data structure
struct DragDropFilePayload
{
	char m_szFilePath[512];          // Absolute path to file
};

class Zenith_Entity;
class Zenith_Scene;

// Console log entry
struct ConsoleLogEntry
{
	enum class LogLevel { Info, Warning, Error };
	LogLevel m_eLevel;
	Zenith_LogCategory m_eCategory;
	std::string m_strMessage;
	std::string m_strTimestamp;
};

enum class EditorMode
{
	Stopped,
	Playing,
	Paused
};

enum class EditorGizmoMode
{
	Translate,
	Rotate,
	Scale
};

class Zenith_Editor
{
public:
	static void Initialise();
	static void Shutdown();
	static bool Update();
	static void Render();

	// Editor state
	static EditorMode GetEditorMode() { return s_eEditorMode; }
	static void SetEditorMode(EditorMode eMode);
	
	//--------------------------------------------------------------------------
	// Multi-Select System
	//--------------------------------------------------------------------------

	/**
	 * Select an entity
	 * @param uEntityID Entity to select
	 * @param bAddToSelection If true, add to existing selection (Ctrl+click). If false, replace selection.
	 */
	static void SelectEntity(Zenith_EntityID uEntityID, bool bAddToSelection = false);

	/**
	 * Select a range of entities (for Shift+click in hierarchy)
	 * Selects all entities between the last selected and the specified entity
	 * @param uEndEntityID The end point of the range selection
	 */
	static void SelectRange(Zenith_EntityID uEndEntityID);

	/**
	 * Toggle selection state of an entity (Ctrl+click)
	 * If selected, deselect. If not selected, add to selection.
	 */
	static void ToggleEntitySelection(Zenith_EntityID uEntityID);

	/**
	 * Clear all selected entities
	 */
	static void ClearSelection();

	/**
	 * Check if a specific entity is selected
	 */
	static bool IsSelected(Zenith_EntityID uEntityID);

	/**
	 * Get the primary selected entity ID (first in selection, or last clicked)
	 * Returns INVALID_ENTITY_ID if no selection
	 */
	static Zenith_EntityID GetSelectedEntityID() { return s_uPrimarySelectedEntityID; }

	/**
	 * Get the primary selected entity (for backwards compatibility and property panel)
	 */
	static Zenith_Entity* GetSelectedEntity();

	/**
	 * Get all selected entity IDs
	 */
	static const std::unordered_set<Zenith_EntityID>& GetSelectedEntityIDs() { return s_xSelectedEntityIDs; }

	/**
	 * Get the number of selected entities
	 */
	static size_t GetSelectionCount() { return s_xSelectedEntityIDs.size(); }

	/**
	 * Check if any entities are selected
	 */
	static bool HasSelection() { return !s_xSelectedEntityIDs.empty(); }

	/**
	 * Check if multiple entities are selected
	 */
	static bool HasMultiSelection() { return s_xSelectedEntityIDs.size() > 1; }

	/**
	 * Get the last clicked entity ID (for range selection)
	 */
	static Zenith_EntityID GetLastClickedEntityID() { return s_uLastClickedEntityID; }

	/**
	 * Remove an entity from selection
	 */
	static void DeselectEntity(Zenith_EntityID uEntityID);

	// Gizmo
	static EditorGizmoMode GetGizmoMode() { return s_eGizmoMode; }
	static void SetGizmoMode(EditorGizmoMode eMode) { s_eGizmoMode = eMode; }

	// Console log
	static void AddLogMessage(const char* szMessage, ConsoleLogEntry::LogLevel eLevel, Zenith_LogCategory eCategory);
	static void ClearConsole();
	
	// Material Editor
	static void SelectMaterial(Zenith_MaterialAsset* pMaterial);
	static void ClearMaterialSelection();
	static Zenith_MaterialAsset* GetSelectedMaterial() { return s_pxSelectedMaterial; }

private:
	static void RenderConsolePanel();
	static void RenderMainMenuBar();
	static void RenderToolbar();
	static void RenderHierarchyPanel();
	static void RenderPropertiesPanel();
	static void RenderViewport();
	static void HandleObjectPicking();
	static void RenderGizmos();
	static void HandleGizmoInteraction();  // New method for Flux_Gizmos integration

	// Content Browser
	static void RenderContentBrowser();
	static void RefreshDirectoryContents();
	static void NavigateToDirectory(const std::string& strPath);
	static void NavigateToParent();
	
	// Material Editor
	static void RenderMaterialEditorPanel();
	static void RenderMaterialTextureSlot(const char* szLabel, Zenith_MaterialAsset* pMaterial,
		const std::string& strCurrentPath,
		void (*SetPathFunc)(Zenith_MaterialAsset*, const std::string&));

	static EditorMode s_eEditorMode;
	static EditorGizmoMode s_eGizmoMode;

	// Multi-select state
	static std::unordered_set<Zenith_EntityID> s_xSelectedEntityIDs;
	static Zenith_EntityID s_uPrimarySelectedEntityID;  // The "primary" selection for property panel
	static Zenith_EntityID s_uLastClickedEntityID;      // For range selection (shift+click)

	// Viewport
	static Zenith_Maths::Vector2 s_xViewportSize;
	static Zenith_Maths::Vector2 s_xViewportPos;
	static bool s_bViewportHovered;
	static bool s_bViewportFocused;

	// Scene state backup (for play mode)
	static bool s_bHasSceneBackup;
	static std::string s_strBackupScenePath;

	// Deferred scene operations (to avoid concurrent access during render tasks)
	static bool s_bPendingSceneLoad;
	static std::string s_strPendingSceneLoadPath;
	static bool s_bPendingSceneSave;
	static std::string s_strPendingSceneSavePath;
	static bool s_bPendingSceneReset;

	// Content Browser state
	static std::string s_strCurrentDirectory;
	static std::vector<ContentBrowserEntry> s_xDirectoryContents;
	static std::vector<ContentBrowserEntry> s_xFilteredContents;  // After search/filter applied
	static bool s_bDirectoryNeedsRefresh;
	static char s_szSearchBuffer[256];  // Search text input buffer
	static int s_iAssetTypeFilter;      // 0 = All, then asset types
	static int s_iSelectedContentIndex; // Currently selected item for context menu

	// Console state
	static std::vector<ConsoleLogEntry> s_xConsoleLogs;
	static bool s_bConsoleAutoScroll;
	static bool s_bShowConsoleInfo;
	static bool s_bShowConsoleWarnings;
	static bool s_bShowConsoleErrors;
	static std::bitset<LOG_CATEGORY_COUNT> s_xCategoryFilters;
	static constexpr size_t MAX_CONSOLE_ENTRIES = 1000;

	// Editor camera (separate from game camera, not part of entity/scene system)
	static Zenith_Maths::Vector3 s_xEditorCameraPosition;
	static double s_fEditorCameraPitch;
	static double s_fEditorCameraYaw;
	static float s_fEditorCameraFOV;
	static float s_fEditorCameraNear;
	static float s_fEditorCameraFar;
	static Zenith_EntityID s_uGameCameraEntity;  // Saved when entering play mode
	static float s_fEditorCameraMoveSpeed;
	static float s_fEditorCameraRotateSpeed;
	static bool s_bEditorCameraInitialized;

	// Material Editor state
	static Zenith_MaterialAsset* s_pxSelectedMaterial;
	static bool s_bShowMaterialEditor;

	// Editor camera control
	static void InitializeEditorCamera();
	static void UpdateEditorCamera(float fDt);
	static void SwitchToEditorCamera();
	static void SwitchToGameCamera();
	static void ResetEditorCameraToDefaults();
public:
	// Camera data access for Flux_Graphics (delegates to appropriate camera based on mode)
	static void BuildViewMatrix(Zenith_Maths::Matrix4& xOutMatrix);
	static void BuildProjectionMatrix(Zenith_Maths::Matrix4& xOutMatrix);
	static void GetCameraPosition(Zenith_Maths::Vector4& xOutPosition);
	static float GetCameraNearPlane();
	static float GetCameraFarPlane();
	static float GetCameraFOV();
	static float GetCameraAspectRatio();
};

#endif // ZENITH_TOOLS
