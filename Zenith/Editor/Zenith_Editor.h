#pragma once

#ifdef ZENITH_TOOLS

#include "Maths/Zenith_Maths.h"
#include "EntityComponent/Zenith_Scene.h"
#include <vector>
#include <string>

// Forward declarations
class Flux_MaterialAsset;

// Drag-drop payload type identifiers (max 32 chars per ImGui)
#define DRAGDROP_PAYLOAD_TEXTURE  "ZENITH_TEXTURE"
#define DRAGDROP_PAYLOAD_MESH_ZMSH    "ZENITH_MESH_ZMSH"
#define DRAGDROP_PAYLOAD_MATERIAL_ZMAT "ZENITH_MATERIAL_ZMAT"
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
	
	// Object selection - now uses EntityID for safer memory management
	static void SelectEntity(Zenith_EntityID uEntityID);
	static void ClearSelection();
	static Zenith_EntityID GetSelectedEntityID() { return s_uSelectedEntityID; }
	static Zenith_Entity* GetSelectedEntity();  // Helper to safely get entity from ID
	static bool HasSelection() { return s_uSelectedEntityID != INVALID_ENTITY_ID; }
	
	// Gizmo
	static EditorGizmoMode GetGizmoMode() { return s_eGizmoMode; }
	static void SetGizmoMode(EditorGizmoMode eMode) { s_eGizmoMode = eMode; }

	// Console log
	static void AddLogMessage(const char* szMessage, ConsoleLogEntry::LogLevel eLevel = ConsoleLogEntry::LogLevel::Info);
	static void ClearConsole();
	
	// Material Editor
	static void SelectMaterial(Flux_MaterialAsset* pMaterial);
	static void ClearMaterialSelection();
	static Flux_MaterialAsset* GetSelectedMaterial() { return s_pxSelectedMaterial; }

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
	static void RenderMaterialTextureSlot(const char* szLabel, Flux_MaterialAsset* pMaterial,
		const std::string& strCurrentPath,
		void (*SetPathFunc)(Flux_MaterialAsset*, const std::string&));

	static EditorMode s_eEditorMode;
	static EditorGizmoMode s_eGizmoMode;
	static Zenith_EntityID s_uSelectedEntityID;  // Changed from pointer to ID

	// Viewport
	static Zenith_Maths::Vector2 s_xViewportSize;
	static Zenith_Maths::Vector2 s_xViewportPos;
	static bool s_bViewportHovered;
	static bool s_bViewportFocused;

	// Scene state backup (for play mode)
	static Zenith_Scene* s_pxBackupScene;  // Legacy - unused
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
	static bool s_bDirectoryNeedsRefresh;

	// Console state
	static std::vector<ConsoleLogEntry> s_xConsoleLogs;
	static bool s_bConsoleAutoScroll;
	static bool s_bShowConsoleInfo;
	static bool s_bShowConsoleWarnings;
	static bool s_bShowConsoleErrors;
	static constexpr size_t MAX_CONSOLE_ENTRIES = 1000;

	// Editor camera (separate from game camera, not part of entity/scene system)
	static Zenith_Maths::Vector3 s_xEditorCameraPosition;
	static double s_fEditorCameraPitch;
	static double s_fEditorCameraYaw;
	static float s_fEditorCameraFOV;
	static float s_fEditorCameraNear;
	static float s_fEditorCameraFar;
	static Zenith_Entity* s_pxGameCameraEntity;  // Saved when entering play mode
	static float s_fEditorCameraMoveSpeed;
	static float s_fEditorCameraRotateSpeed;
	static bool s_bEditorCameraInitialized;

	// Material Editor state
	static Flux_MaterialAsset* s_pxSelectedMaterial;
	static bool s_bShowMaterialEditor;

	// Editor camera control
	static void InitializeEditorCamera();
	static void UpdateEditorCamera(float fDt);
	static void SwitchToEditorCamera();
	static void SwitchToGameCamera();
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
