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
#include "Flux/Flux_Fwd.h"                                // Flux_PlatformAPI alias (Initialise dep param)
#include "AssetHandling/Zenith_AssetHandle.h"             // MaterialHandle
#include "Collections/Zenith_Vector.h"
#include "Core/Zenith_DragDropPayloads.h"   // DRAGDROP_PAYLOAD_* + DragDropFilePayload (L0, no deps)
#include <string>
#include <unordered_set>
#include <bitset>

// Forward declarations
class Zenith_MaterialAsset;
class Flux_GraphicsImpl;
class FrameContext;
class Zenith_DebugVariables;
class Zenith_Profiling;
class Zenith_TerrainEditor;

// Content browser view mode
enum class ContentBrowserViewMode
{
	Grid,
	List
};

// DRAGDROP_PAYLOAD_* identifiers now live in Core/Zenith_DragDropPayloads.h
// (included above); they remain available here transitively.

// Content browser file entry
struct ContentBrowserEntry
{
	std::string m_strName;           // Display name (filename without path)
	std::string m_strFullPath;       // Full absolute path
	std::string m_strExtension;      // File extension (e.g., ZENITH_TEXTURE_EXT)
	bool m_bIsDirectory;             // true for folders, false for files
	uint64_t m_ulFileSize = 0;       // File size in bytes
};

// DragDropFilePayload now lives in Core/Zenith_DragDropPayloads.h (included above).

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

	// Frame deps are injected and cached as members (m_px* below) so the
	// per-frame ImGui composition never reaches for g_xEngine from this TU
	// (engine-singleton ratchet: Zenith_Editor.cpp is a counted file).
	void Initialise(Flux_PlatformAPI& xFluxBackend, Flux_GraphicsImpl& xFluxGraphics, FrameContext& xFrame,
		Zenith_DebugVariables& xDebugVariables, Zenith_Profiling& xProfiling, Zenith_TerrainEditor& xTerrainEditor);
	void Shutdown();
	bool Update();
	void Render();

	// Redirects ImGui layout persistence to %LOCALAPPDATA%/Zenith/<GameName>/
	// imgui.ini for interactive runs, and DISABLES it (nullptr ini) for
	// headless/automated/--no-imgui-ini runs so tests always get the
	// deterministic code-built dock layout. Called from Initialise — must run
	// between ImGui::CreateContext and the first NewFrame.
	void ConfigureImGuiIniPath();

	// Composes the whole per-frame ImGui pass: backend ImGuiBeginFrame ->
	// editor panels (Render) -> legacy "Zenith Tools" debug window ->
	// profiling window -> ImGui::Render(). Called from the main loop's
	// render-work block; only reachable windowed in tools builds.
	void RenderImGuiFrame();

	// Editor state
	EditorMode GetEditorMode();
	void SetEditorMode(EditorMode eMode);

	// Synchronously process the pending deferred scene load (mode-transition restore)
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

	// Open a terrain-editing session on a terrain entity and show the panel.
	// Forwarder so component editors (EntityComponent layer) can launch the
	// terrain editor without including Editor/TerrainEditor headers.
	void OpenTerrainEditor(Zenith_EntityID uTerrainEntity);

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

	// ===== Data members =====

	// ALL editor domain state lives here (mode, selection, viewport, deferred
	// ops, play backup, content browser, console, camera, material, panel
	// visibility). See Zenith_EditorState.h.
	Zenith_EditorState m_xEditorState;

	// Runtime GPU/frame caches — deliberately NOT part of Zenith_EditorState
	// (tied to the current frame's graphics resources, not editor state).
	Flux_ImGuiTextureHandle           m_xCachedGameTextureHandle;
	Flux_ImageViewHandle              m_xCachedImageViewHandle;

	// Deferred-deletion queue for ImGui textures (GPU must finish before freeing).
	Zenith_Vector<PendingImGuiTextureDeletion> m_xPendingDeletions;

	// Frame deps injected via Initialise() — see the comment on Initialise.
	Flux_PlatformAPI*      m_pxFluxBackend    = nullptr;
	Flux_GraphicsImpl*     m_pxFluxGraphics   = nullptr;
	FrameContext*          m_pxFrame          = nullptr;
	Zenith_DebugVariables* m_pxDebugVariables = nullptr;
	Zenith_Profiling*      m_pxProfiling      = nullptr;
	Zenith_TerrainEditor*  m_pxTerrainEditor  = nullptr;

	// Counts down after a default-dock-layout build; on hitting 0 the
	// intended front tabs are re-selected (late-created windows steal tab
	// selection during the build frame — see Render()).
	u_int m_uFrontDefaultTabsCountdown = 0;
};

#endif // ZENITH_TOOLS
