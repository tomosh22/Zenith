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
class Zenith_Input;

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

	// Drop every scrap of editor SESSION state so the next automated test starts
	// from the same editor as the first one did.
	//
	// The harness destroys the world between tests, which invalidates every
	// EntityID the editor is holding — the selection set, the selection system's
	// entity-keyed bounds cache, the undo/redo stacks (raw command pointers
	// keyed by EntityID), the camera's game-camera entity, and any play-mode
	// scene backup. It also has to clear the editor's OWN pending-scene-load
	// flags: those bypass the scene system's pending-load slot entirely (they go
	// through Zenith_EditorSceneAccess), so the scene system's mid-reset load
	// guard cannot see them, and a queued editor load would fire into the next
	// test's world.
	//
	// Scope is exactly "references that are DEAD after the reset". The editor
	// MODE and the play-mode scene backup are deliberately left alone: they are
	// two halves of one state machine, and normalising either in isolation
	// desynchronises it in a way that makes EnterPlayMode re-dispatch OnAwake
	// over live entities (see the body for the concrete failure).
	void ResetSessionForNextTest();

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
	// The Material Editor panel's show/hide flag (Window menu toggle + window
	// close box). Exposed by reference so the panel can clear it on close.
	bool& GetMaterialEditorShowFlag() { return m_xEditorState.m_xMaterial.m_bShowEditor; }

	//--------------------------------------------------------------------------
	// Editor Operations
	//--------------------------------------------------------------------------

	Zenith_EntityID CreateEntity(const char* szName);
	void SelectEntityByName(const char* szName);
	void SetSelectedEntityTransient(bool bTransient);
	bool AddComponentToSelected(const char* szDisplayName);
	void SetSelectedAsMainCamera();
	void AttachGraphToSelected(const char* szGraphAssetPath);
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
	void RenderEntityMenu();
	void RenderViewMenu();
	void RenderHelpMenu();
	void RenderHierarchyPanel();
	void RenderPropertiesPanel();
	void RenderViewport();
	// The "Save changes?" modal and the Keyboard Shortcuts window.
	void RenderPrompts();
	void RenderShortcutsWindow();
	void HandleObjectPicking();
	void RenderGizmos();
	void HandleGizmoInteraction();
	// Hover highlight for the gizmo handles while no drag is in flight.
	void HandleGizmoHover(const Zenith_Maths::Vector3& xRayOrigin, const Zenith_Maths::Vector3& xRayDir);
	// Turns a finished gizmo drag into one undo step.
	void RecordGizmoDragUndo(Zenith_EntityID xEntityID);
	// The selected entities' bounds, drawn as wireframe boxes in the viewport.
	void DrawSelectionBounds();
	// Pushes gizmo space + snapping from the prefs (and a held Ctrl) each frame.
	void SyncGizmoSettings();

	bool ProcessDeferredSceneOperations();
	bool HandlePendingSceneLoad();

	void WaitForGPUAndFlushDeferred(const char* szReason);
	void HandlePendingSceneLoadDeferred();

	// The scene-load body itself — backup-restore teardown, load-into-active,
	// selection/undo/camera invalidation, backup-file cleanup. Shared by the
	// in-frame path (HandlePendingSceneLoad) and the out-of-frame test entry
	// point (HandlePendingSceneLoadDeferred), which differ in exactly two
	// things, both parameters here:
	//   bWaitForGPU — the in-frame path runs before render-task submission, so
	//     the QueueVRAMDeletion grace period already covers it and it must NOT
	//     stall; the test entry point runs outside the frame loop where the
	//     per-frame deletion tick is not running, so it must.
	//   szLogPrefix — "" in-frame, "[FlushPending] " for the test path.
	// Assumes m_bPendingSceneLoad has already been consumed by the caller.
	void LoadPendingSceneIntoActiveScene(bool bWaitForGPU, const char* szLogPrefix);

	// Keyboard shortcuts (gizmo modes, undo/redo, delete, duplicate, focus,
	// save/open/new, play). Routed through Zenith_EditorActions; suppressed
	// while ImGui has a text field focused.
	void UpdateEditorInput();
	void HandleGlobalShortcuts(Zenith_Input& xInput, bool bCtrl, bool bShift);
	void HandleEntityShortcuts(Zenith_Input& xInput, bool bCtrl);
	void HandleViewportShortcuts(Zenith_Input& xInput);

	// Keeps the OS window title in step with the active scene / dirty state.
	void UpdateWindowTitle();

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

	// Editor camera control (Zenith_EditorCamera.cpp)
	void InitializeEditorCamera();
	void UpdateEditorCamera(float fDt);
	void UpdateEditorCameraGestures();
	void ApplyMouseLookDelta();
	void UpdateEditorCameraLook();
	void UpdateEditorCameraMovement(float fDt);
	void UpdateEditorCameraOrbit();
	void UpdateEditorCameraPan();
	void UpdateEditorCameraDolly(float fWheel);
	void AdvanceCameraFocus(float fDt);
	void EndCameraGestures();
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
	// Smoothly flies the camera to frame a sphere / the selection's bounds.
	void FocusCameraOn(const Zenith_Maths::Vector3& xCentre, float fRadius);
	void FocusCameraOnSelection();
	// Moves the orbit pivot onto the primary selection (when not navigating).
	void RefreshCameraPivotFromSelection();
	Zenith_Maths::Vector3 GetEditorCameraForward() const;
	Zenith_Maths::Vector3 GetEditorCameraRight() const;
	Zenith_Maths::Vector3 GetEditorCameraUp() const;
	// Where a newly created entity is placed: the pivot, or ahead of the camera.
	Zenith_Maths::Vector3 GetCameraPlacementPoint() const;
	// True while a look / orbit / pan gesture owns the mouse.
	bool IsCameraNavigating() const;

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
