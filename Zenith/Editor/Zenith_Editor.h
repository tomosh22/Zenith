#pragma once

#ifdef ZENITH_TOOLS

#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_Scene.h"
// Scene.h is now an opaque handle and no longer transitively provides the full
// Zenith_EntityID definition / Zenith_SceneData / entity-component templates (it
// used to via its bottom SceneData include, removed in the 7b API contraction).
// The editor stores EntityID by value (m_uPrimarySelectedEntityID) and drives
// entity component access, so include SceneData.h explicitly.
#include "ZenithECS/Zenith_SceneData.h"
// NOTE: Zenith_EditorState.h is included AFTER the types it depends on
// (ContentBrowserEntry, ConsoleLogEntry, EditorMode, etc.) — see below the
// type definitions.
#include "Editor/Panels/Zenith_EditorPanel_Viewport.h"   // PendingImGuiTextureDeletion
#include "Flux/Flux_Types.h"                              // Flux_ImGuiTextureHandle, Flux_ImageViewHandle
#include "AssetHandling/Zenith_AssetHandle.h"             // MaterialHandle
#include "Collections/Zenith_Vector.h"
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

// EditorState includes — must come AFTER ContentBrowserEntry, ConsoleLogEntry,
// EditorMode, EditorGizmoMode, ContentBrowserViewMode are defined above.
#include "Editor/Zenith_EditorState.h"

// Per-Engine state + behaviour for the Editor subsystem. Replaces the
// `namespace Zenith_Editor` facade (deleted) and the data-only
// `Zenith_Editor` (folded in here). Accessed via g_xEngine.Editor().
// Compiled only when ZENITH_TOOLS is defined.
class Zenith_Editor
{
public:
	Zenith_Editor() = default;
	~Zenith_Editor() = default;
	Zenith_Editor(const Zenith_Editor&) = delete;
	Zenith_Editor& operator=(const Zenith_Editor&) = delete;

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

	void SelectEntity(Zenith_EntityID uEntityID, bool bAddToSelection = false);
	void SelectRange(Zenith_EntityID uEndEntityID);
	void ToggleEntitySelection(Zenith_EntityID uEntityID);
	void ClearSelection();
	bool IsSelected(Zenith_EntityID uEntityID);
	Zenith_EntityID GetSelectedEntityID();
	Zenith_Entity* GetSelectedEntity();
	const std::unordered_set<Zenith_EntityID>& GetSelectedEntityIDs();
	size_t GetSelectionCount();
	bool HasSelection();
	bool HasMultiSelection();
	Zenith_EntityID GetLastClickedEntityID();
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
	//--------------------------------------------------------------------------

	Zenith_EntityID CreateEntity(const char* szName);
	void SelectEntityByName(const char* szName);
	void SetSelectedEntityTransient(bool bTransient);
	bool AddComponentToSelected(const char* szDisplayName);
	void SetSelectedAsMainCamera();
	void AttachScriptToSelectedAndAwake(const char* szBehaviourTypeName);
	void AttachScriptForSerializationToSelected(const char* szBehaviourTypeName);
	void CreateNewScene(const char* szName);
	void SaveActiveScene(const char* szPath);
	void UnloadActiveScene();

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
	void HandleGizmoInteraction();

	bool ProcessDeferredSceneOperations();
	bool HandlePendingSceneLoad();

	void WaitForGPUAndFlushDeferred(const char* szReason);
	void HandlePendingSceneReset();
	void HandlePendingSceneSave();
	void HandlePendingSceneLoadDeferred();

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

	static constexpr size_t MAX_CONSOLE_ENTRIES = 1000;

	// Editor theme
	void ApplyEditorTheme();

	// Editor camera control
	void InitializeEditorCamera();
	void UpdateEditorCamera(float fDt);
	void UpdateEditorCameraLook();
	void UpdateEditorCameraMovement(float fDt);
	void ApplyEditorCameraToScene();
	void SwitchToEditorCamera();
	void SwitchToGameCamera();
	void ResetEditorCameraToDefaults();
	void BuildViewMatrix(Zenith_Maths::Matrix4& xOutMatrix);
	void BuildProjectionMatrix(Zenith_Maths::Matrix4& xOutMatrix);
	void GetCameraPosition(Zenith_Maths::Vector4& xOutPosition);
	float GetCameraNearPlane();
	float GetCameraFarPlane();
	float GetCameraFOV();
	float GetCameraAspectRatio();

	// ===== Data members (was Zenith_Editor) =====

	// Mode flags.
	EditorMode      m_eEditorMode = EditorMode::Stopped;
	EditorGizmoMode m_eGizmoMode  = EditorGizmoMode::Translate;

	// Multi-select state.
	std::unordered_set<Zenith_EntityID> m_xSelectedEntityIDs;
	Zenith_EntityID m_uPrimarySelectedEntityID = INVALID_ENTITY_ID;
	Zenith_EntityID m_uLastClickedEntityID     = INVALID_ENTITY_ID;

	// Viewport state.
	Zenith_Maths::Vector2 m_xViewportSize = { 1280, 720 };
	Zenith_Maths::Vector2 m_xViewportPos  = { 0, 0 };
	bool                  m_bViewportHovered = false;
	bool                  m_bViewportFocused = false;

	// Aggregate state container (mid-migration target — pre-existing).
	Zenith_EditorState    m_xEditorState;

	// Content Browser state.
	std::string                       m_strCurrentDirectory;
	Zenith_Vector<ContentBrowserEntry> m_xDirectoryContents;
	Zenith_Vector<ContentBrowserEntry> m_xFilteredContents;
	bool                              m_bDirectoryNeedsRefresh = true;
	char                              m_szSearchBuffer[256]    = "";
	int                               m_iAssetTypeFilter       = 0;
	int                               m_iSelectedContentIndex  = -1;
	float                             m_fThumbnailSize         = 80.0f;
	Zenith_Vector<std::string>        m_axNavigationHistory;
	int                               m_iHistoryIndex          = -1;
	ContentBrowserViewMode            m_eViewMode              = ContentBrowserViewMode::Grid;

	// Console state.
	Zenith_Vector<ConsoleLogEntry>    m_xConsoleLogs;
	bool                              m_bConsoleAutoScroll     = true;
	bool                              m_bShowConsoleInfo       = true;
	bool                              m_bShowConsoleWarnings   = true;
	bool                              m_bShowConsoleErrors     = true;
	std::bitset<LOG_CATEGORY_COUNT>   m_xCategoryFilters       = std::bitset<LOG_CATEGORY_COUNT>().set();

	// Panel visibility (View menu toggles).
	bool                              m_bShowHierarchyPanel    = true;
	bool                              m_bShowPropertiesPanel   = true;
	bool                              m_bShowConsolePanel      = true;

	// Material Editor state.
	MaterialHandle                    m_xSelectedMaterial;
	bool                              m_bShowMaterialEditor    = true;

	// Editor camera state.
	Zenith_Maths::Vector3             m_xEditorCameraPosition  = { 0, 100, 0 };
	double                            m_fEditorCameraPitch     = 0.0;
	double                            m_fEditorCameraYaw       = 0.0;
	float                             m_fEditorCameraFOV       = 45.0f;
	float                             m_fEditorCameraNear      = 1.0f;
	float                             m_fEditorCameraFar       = 2000.0f;
	Zenith_EntityID                   m_uGameCameraEntity      = INVALID_ENTITY_ID;
	float                             m_fEditorCameraMoveSpeed = 50.0f;
	float                             m_fEditorCameraRotateSpeed = 0.1f;
	bool                              m_bEditorCameraInitialized = false;

	// ImGui texture handle caching for the game viewport.
	Flux_ImGuiTextureHandle           m_xCachedGameTextureHandle;
	Flux_ImageViewHandle              m_xCachedImageViewHandle;

	// Deferred-deletion queue for ImGui textures (GPU must finish before freeing).
	Zenith_Vector<PendingImGuiTextureDeletion> m_xPendingDeletions;
};

#endif // ZENITH_TOOLS
