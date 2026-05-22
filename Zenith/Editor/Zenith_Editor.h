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

// Content browser view mode
enum class ContentBrowserViewMode
{
	Grid,
	List
};

// Drag-drop payload type identifiers (max 32 chars per ImGui)
#define DRAGDROP_PAYLOAD_TEXTURE  "ZENITH_TEXTURE"
#define DRAGDROP_PAYLOAD_MESH    "ZENITH_MESH"
#define DRAGDROP_PAYLOAD_MATERIAL "ZENITH_MATERIAL"
#define DRAGDROP_PAYLOAD_PREFAB   "ZENITH_PREFAB"
#define DRAGDROP_PAYLOAD_MODEL    "ZENITH_MODEL"
#define DRAGDROP_PAYLOAD_ANIMATION "ZENITH_ANIMATION"
#define DRAGDROP_PAYLOAD_SCRIPT_ASSET "ZSCRIPT_ASSET"
#define DRAGDROP_PAYLOAD_FILE_GENERIC "ZENITH_FILE_GENERIC"

// Content browser file entry
struct ContentBrowserEntry
{
	std::string m_strName;           // Display name (filename without path)
	std::string m_strFullPath;       // Full absolute path
	std::string m_strExtension;      // File extension (e.g., ZENITH_TEXTURE_EXT)
	bool m_bIsDirectory;             // true for folders, false for files
	uint64_t m_ulFileSize = 0;       // File size in bytes
};

// Drag-drop payload data structure
struct DragDropFilePayload
{
	char m_szFilePath[512];          // Absolute path to file
};

// File type metadata for content browser display
struct EditorFileTypeInfo
{
	const char* m_szExtension;       // e.g., ".ztxtr"
	const char* m_szIconText;        // e.g., "[TEX]"
	const char* m_szDisplayName;     // e.g., "Texture"
	const char* m_szDragDropType;    // e.g., DRAGDROP_PAYLOAD_TEXTURE
};

// Look up file type info by extension (returns nullptr for non-Zenith types)
const EditorFileTypeInfo* GetFileTypeInfo(const std::string& strExtension);

class Zenith_Entity;
struct Zenith_Scene;

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

namespace Zenith_Editor
{
	void Initialise();
	void Shutdown();
	bool Update();
	void Render();

	// Editor state
	EditorMode GetEditorMode();
	void SetEditorMode(EditorMode eMode);

	// Synchronously process pending scene operations (load/save/reset)
	// Used by unit tests to ensure scene state is consistent after mode transitions
	void FlushPendingSceneOperations();

	// Request loading a registered scene by build index (deferred to next Update)
	void RequestLoadRegisteredScene(int iBuildIndex);

	// Request loading a scene from a file path (deferred to next Update)
	void RequestLoadSceneFromFile(const std::string& strPath);

	//--------------------------------------------------------------------------
	// Multi-Select System
	//--------------------------------------------------------------------------

	/**
	 * Select an entity
	 * @param uEntityID Entity to select
	 * @param bAddToSelection If true, add to existing selection (Ctrl+click). If false, replace selection.
	 */
	void SelectEntity(Zenith_EntityID uEntityID, bool bAddToSelection = false);

	/**
	 * Select a range of entities (for Shift+click in hierarchy)
	 * Selects all entities between the last selected and the specified entity
	 * @param uEndEntityID The end point of the range selection
	 */
	void SelectRange(Zenith_EntityID uEndEntityID);

	/**
	 * Toggle selection state of an entity (Ctrl+click)
	 * If selected, deselect. If not selected, add to selection.
	 */
	void ToggleEntitySelection(Zenith_EntityID uEntityID);

	/**
	 * Clear all selected entities
	 */
	void ClearSelection();

	/**
	 * Check if a specific entity is selected
	 */
	bool IsSelected(Zenith_EntityID uEntityID);

	/**
	 * Get the primary selected entity ID (first in selection, or last clicked)
	 * Returns INVALID_ENTITY_ID if no selection
	 */
	Zenith_EntityID GetSelectedEntityID();

	/**
	 * Get the primary selected entity (for backwards compatibility and property panel)
	 */
	Zenith_Entity* GetSelectedEntity();

	/**
	 * Get all selected entity IDs
	 */
	const std::unordered_set<Zenith_EntityID>& GetSelectedEntityIDs();

	/**
	 * Get the number of selected entities
	 */
	size_t GetSelectionCount();

	/**
	 * Check if any entities are selected
	 */
	bool HasSelection();

	/**
	 * Check if multiple entities are selected
	 */
	bool HasMultiSelection();

	/**
	 * Get the last clicked entity ID (for range selection)
	 */
	Zenith_EntityID GetLastClickedEntityID();

	/**
	 * Remove an entity from selection
	 */
	void DeselectEntity(Zenith_EntityID uEntityID);

	// Viewport
	Zenith_Maths::Vector2 GetViewportPos();
	Zenith_Maths::Vector2 GetViewportSize();

	// Gizmo
	EditorGizmoMode GetGizmoMode();
	void SetGizmoMode(EditorGizmoMode eMode);

	// Console log
	void AddLogMessage(const char* szMessage, ConsoleLogEntry::LogLevel eLevel, Zenith_LogCategory eCategory);
	void ClearConsole();
	
	// Material Editor
	void SelectMaterial(Zenith_MaterialAsset* pMaterial);
	void ClearMaterialSelection();
	Zenith_MaterialAsset* GetSelectedMaterial();

	//--------------------------------------------------------------------------
	// Editor Operations
	// Called by BOTH ImGui panel handlers AND the editor automation system.
	// These encapsulate the multi-step operations that correspond to user
	// interactions in the editor UI.
	//--------------------------------------------------------------------------

	/// Corresponds to: Hierarchy panel > right-click > "Create Empty Entity"
	/// Creates entity, sets non-transient, and selects it (matching editor UI behaviour).
	Zenith_EntityID CreateEntity(const char* szName);

	/// Corresponds to: clicking an entity by name in the Hierarchy panel.
	void SelectEntityByName(const char* szName);

	/// Corresponds to: toggling the "Transient" checkbox in Properties panel.
	void SetSelectedEntityTransient(bool bTransient);

	/// Corresponds to: Properties panel > "Add Component" popup > selecting a component.
	bool AddComponentToSelected(const char* szDisplayName);

	/// Corresponds to: Properties panel > Camera section > "Set As Main Camera" button.
	void SetSelectedAsMainCamera();

	/// Append a script slot to the selected entity at runtime (Unity-style: multiple scripts allowed).
	/// Adds the ScriptComponent if missing. Calls OnAwake on the new behaviour, marks entity awoken.
	/// Used by editor drag-drop and the "Add Script" popup in the script properties panel.
	void AttachScriptToSelectedAndAwake(const char* szBehaviourTypeName);

	/// Append a script slot for build-time scene serialization (no lifecycle hooks called).
	/// Adds the ScriptComponent if missing. Lifecycle dispatched when scene enters Play mode.
	/// Used by EditorAutomation's ATTACH_SCRIPT action.
	void AttachScriptForSerializationToSelected(const char* szBehaviourTypeName);

	/// Corresponds to: File > New Scene menu item.
	/// Creates empty scene, sets active, clears selection.
	void CreateNewScene(const char* szName);

	/// Corresponds to: File > Save Scene menu item (with specific path).
	void SaveActiveScene(const char* szPath);

	/// Corresponds to: File > Unload Scene menu item.
	/// Clears selection, then unloads the active scene.
	void UnloadActiveScene();

	// SetEditorMode transition helpers — split out so each transition's
	// scene-state shuffling lives in one place. SetEditorMode owns the mode
	// state itself; these helpers are pure transition routines.
	// EnterPlayMode: backup scene → locate game camera → dispatch
	//   OnAwake/OnEnable/OnStart for every entity. Returns false if no scene
	//   data is loaded — caller is expected to revert s_eEditorMode.
	// EnterStopMode: queue the deferred scene-restore from backup. The actual
	//   load runs in next frame's Update() before any render tasks.
	bool EnterPlayMode();
	void EnterStopMode();

	void RenderConsolePanel();
	void RenderMainMenuBar();
	void RenderFileMenu();
	void RenderEditMenu();
	void RenderViewMenu();
	void RenderToolbar();
	void RenderHierarchyPanel();
	void RenderPropertiesPanel();
	void RenderViewport();
	void HandleObjectPicking();
	void RenderGizmos();
	void HandleGizmoInteraction();  // New method for Flux_Gizmos integration

	// Deferred scene operations (extracted from Update)
	bool ProcessDeferredSceneOperations();
	bool HandlePendingSceneLoad();

	// FlushPendingSceneOperations branches — split out so each pending
	// operation lives in one place. All three may run in the same frame
	// (e.g. save+load), so they're called sequentially from the dispatcher.
	void WaitForGPUAndFlushDeferred(const char* szReason);
	void HandlePendingSceneReset();
	void HandlePendingSceneSave();
	void HandlePendingSceneLoadDeferred();

	// Editor input (extracted from Update)
	void UpdateEditorInput();

	// Content Browser
	void RenderContentBrowser();
	void RefreshDirectoryContents();
	void NavigateToDirectory(const std::string& strPath);
	void NavigateToParent();
	
	// Material Editor
	void RenderMaterialEditorPanel();
	void RenderMaterialTextureSlot(const char* szLabel, Zenith_MaterialAsset* pMaterial,
		const std::string& strCurrentPath,
		void (*SetPathFunc)(Zenith_MaterialAsset*, const std::string&));

	// Phase 5.5c: 36 class-static data members + 10 camera statics + 3
	// file-statics moved off this class onto Zenith_EditorImpl (held by
	// Zenith_Engine). Method bodies, the 42 external readers, and the
	// EditorCamera.cpp camera storage now all read/write through
	// g_xEngine.Editor().m_xXxx.
	inline constexpr size_t MAX_CONSOLE_ENTRIES = 1000;

	// Editor theme
	void ApplyEditorTheme();

	// Editor camera control
	void InitializeEditorCamera();
	void UpdateEditorCamera(float fDt);
	// UpdateEditorCamera implementation broken into focused steps so callers
	// don't have to read 100+ lines of input-handling mixed with scene writes.
	void UpdateEditorCameraLook();
	void UpdateEditorCameraMovement(float fDt);
	void ApplyEditorCameraToScene();
	void SwitchToEditorCamera();
	void SwitchToGameCamera();
	void ResetEditorCameraToDefaults();
	// Camera data access for Flux_Graphics (delegates to appropriate camera based on mode)
	void BuildViewMatrix(Zenith_Maths::Matrix4& xOutMatrix);
	void BuildProjectionMatrix(Zenith_Maths::Matrix4& xOutMatrix);
	void GetCameraPosition(Zenith_Maths::Vector4& xOutPosition);
	float GetCameraNearPlane();
	float GetCameraFarPlane();
	float GetCameraFOV();
	float GetCameraAspectRatio();
}

#endif // ZENITH_TOOLS
