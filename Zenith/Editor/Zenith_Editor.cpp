#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_Editor.h"
#include "AssetHandling/Zenith_MaterialAsset.h"

// Bridge function called from Zenith_Log macro to add to editor console
// NOTE: Must be defined after including Zenith_Editor.h
void Zenith_EditorAddLogMessage(const char* szMessage, int eLevel, Zenith_LogCategory eCategory)
{
	// Convert int to log level enum
	ConsoleLogEntry::LogLevel xLevel = ConsoleLogEntry::LogLevel::Info;
	switch (eLevel)
	{
	case 0: xLevel = ConsoleLogEntry::LogLevel::Info; break;
	case 1: xLevel = ConsoleLogEntry::LogLevel::Warning; break;
	case 2: xLevel = ConsoleLogEntry::LogLevel::Error; break;
	}
	Zenith_Editor::AddLogMessage(szMessage, xLevel, eCategory);
}

#include "Zenith_SelectionSystem.h"
#include "Zenith_Gizmo.h"
#include "Zenith_UndoSystem.h"
#include "Flux/Gizmos/Flux_Gizmos.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_ComponentRegistry.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "Input/Zenith_Input.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_ImGuiIntegration.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"

// Extracted panel implementations
#include "Panels/Zenith_EditorPanel_Console.h"
#include "Panels/Zenith_EditorPanel_ContentBrowser.h"
#include "Panels/Zenith_EditorPanel_Hierarchy.h"
#include "Panels/Zenith_EditorPanel_MaterialEditor.h"
#include "Panels/Zenith_EditorPanel_Memory.h"
#include "Panels/Zenith_EditorPanel_Properties.h"
#include "Panels/Zenith_EditorPanel_Toolbar.h"
#include "Panels/Zenith_EditorPanel_Viewport.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

#include <filesystem>
#include <algorithm>

// Windows file dialog support
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>
#pragma comment(lib, "Comdlg32.lib")

// Helper function to show Windows Open File dialog
// Returns empty string if cancelled
static std::string ShowOpenFileDialog(const char* szFilter, const char* szDefaultExt)
{
	char szFilePath[MAX_PATH] = { 0 };

	OPENFILENAMEA ofn = {};
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = nullptr;
	ofn.lpstrFilter = szFilter;
	ofn.lpstrFile = szFilePath;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = szDefaultExt;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

	if (GetOpenFileNameA(&ofn))
	{
		return std::string(szFilePath);
	}
	return "";
}

// Helper function to show Windows Save File dialog
// Returns empty string if cancelled
static std::string ShowSaveFileDialog(const char* szFilter, const char* szDefaultExt, const char* szDefaultFilename)
{
	char szFilePath[MAX_PATH] = { 0 };
	if (szDefaultFilename)
	{
		strncpy(szFilePath, szDefaultFilename, MAX_PATH - 1);
	}

	OPENFILENAMEA ofn = {};
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = nullptr;
	ofn.lpstrFilter = szFilter;
	ofn.lpstrFile = szFilePath;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = *szDefaultExt == '.' ? szDefaultExt+1 : szDefaultExt;
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

	if (GetSaveFileNameA(&ofn))
	{
		return std::string(szFilePath);
	}
	return "";
}
#endif // _WIN32

// Static member initialization
EditorMode Zenith_Editor::s_eEditorMode = EditorMode::Stopped;
EditorGizmoMode Zenith_Editor::s_eGizmoMode = EditorGizmoMode::Translate;

// Multi-select state
std::unordered_set<Zenith_EntityID> Zenith_Editor::s_xSelectedEntityIDs;
Zenith_EntityID Zenith_Editor::s_uPrimarySelectedEntityID = INVALID_ENTITY_ID;
Zenith_EntityID Zenith_Editor::s_uLastClickedEntityID = INVALID_ENTITY_ID;
Zenith_Maths::Vector2 Zenith_Editor::s_xViewportSize = { 1280, 720 };
Zenith_Maths::Vector2 Zenith_Editor::s_xViewportPos = { 0, 0 };
bool Zenith_Editor::s_bViewportHovered = false;
bool Zenith_Editor::s_bViewportFocused = false;
bool Zenith_Editor::s_bHasSceneBackup = false;
std::string Zenith_Editor::s_strBackupScenePath = "";
bool Zenith_Editor::s_bPendingSceneLoad = false;
std::string Zenith_Editor::s_strPendingSceneLoadPath = "";
bool Zenith_Editor::s_bPendingSceneSave = false;
std::string Zenith_Editor::s_strPendingSceneSavePath = "";
bool Zenith_Editor::s_bPendingSceneReset = false;

// Content Browser state
std::string Zenith_Editor::s_strCurrentDirectory;
std::vector<ContentBrowserEntry> Zenith_Editor::s_xDirectoryContents;
std::vector<ContentBrowserEntry> Zenith_Editor::s_xFilteredContents;
bool Zenith_Editor::s_bDirectoryNeedsRefresh = true;
char Zenith_Editor::s_szSearchBuffer[256] = "";
int Zenith_Editor::s_iAssetTypeFilter = 0;
int Zenith_Editor::s_iSelectedContentIndex = -1;

// Console state
std::vector<ConsoleLogEntry> Zenith_Editor::s_xConsoleLogs;
bool Zenith_Editor::s_bConsoleAutoScroll = true;
bool Zenith_Editor::s_bShowConsoleInfo = true;
bool Zenith_Editor::s_bShowConsoleWarnings = true;
bool Zenith_Editor::s_bShowConsoleErrors = true;
std::bitset<LOG_CATEGORY_COUNT> Zenith_Editor::s_xCategoryFilters = std::bitset<LOG_CATEGORY_COUNT>().set();

// Material Editor state
Zenith_MaterialAsset* Zenith_Editor::s_pxSelectedMaterial = nullptr;
bool Zenith_Editor::s_bShowMaterialEditor = true;

// Editor camera state is defined in Zenith_EditorCamera.cpp

// Cache the ImGui texture handle for the game viewport texture
static Flux_ImGuiTextureHandle s_xCachedGameTextureHandle;
static Flux_ImageViewHandle s_xCachedImageViewHandle;

// Deferred deletion queue for ImGui textures
// GPU requires waiting for resources to finish before freeing them
// (PendingImGuiTextureDeletion struct is defined in Zenith_EditorPanel_Viewport.h)
static std::vector<PendingImGuiTextureDeletion> s_xPendingDeletions;

void Zenith_Editor::Initialise()
{
	// Initialize content browser to game assets directory
	s_strCurrentDirectory = Project_GetGameAssetsDirectory();

	s_eEditorMode = EditorMode::Stopped;
	s_xSelectedEntityIDs.clear();
	s_uPrimarySelectedEntityID = INVALID_ENTITY_ID;
	s_uLastClickedEntityID = INVALID_ENTITY_ID;
	s_eGizmoMode = EditorGizmoMode::Translate;

	// Material system is now managed by Zenith_AssetRegistry

	// Initialize editor subsystems
	Zenith_SelectionSystem::Initialise();
	Zenith_Gizmo::Initialise();
	// Zenith_AnimationStateMachineEditor::Initialize();  // TEMPORARILY DISABLED

	// Initialize editor camera
	InitializeEditorCamera();
}

void Zenith_Editor::Shutdown()
{
	// Process all pending deletions immediately on shutdown
	// At shutdown, we can safely assume all GPU work is done or will be waited for
	for (auto& pending : s_xPendingDeletions)
	{
		Flux_ImGuiIntegration::UnregisterTexture(pending.xHandle, 0); // Immediate deletion at shutdown
	}
	s_xPendingDeletions.clear();

	// Free the cached ImGui texture handle
	if (s_xCachedGameTextureHandle.IsValid())
	{
		Flux_ImGuiIntegration::UnregisterTexture(s_xCachedGameTextureHandle, 0); // Immediate deletion at shutdown
		s_xCachedGameTextureHandle.Invalidate();
		s_xCachedImageViewHandle = Flux_ImageViewHandle();
	}

	// Reset editor camera state
	s_bEditorCameraInitialized = false;
	
	// Clear material selection (material system managed by Zenith_AssetRegistry)
	s_pxSelectedMaterial = nullptr;

	// Shutdown editor subsystems
	// Zenith_AnimationStateMachineEditor::Shutdown();  // TEMPORARILY DISABLED
	Flux_Gizmos::Shutdown();
	Zenith_Gizmo::Shutdown();
	Zenith_SelectionSystem::Shutdown();
}

bool Zenith_Editor::Update()
{
	// CRITICAL: Handle pending scene operations FIRST, before any rendering
	// This must happen here (not during RenderMainMenuBar) to avoid concurrent access
	// to scene data while render tasks are active.
	//
	// Both save and load operations iterate through scene data structures.
	// If render tasks are active while these operations occur, we risk:
	// - Reading corrupted data during save (render tasks modifying while we read)
	// - Crashes during load (destroying pools while render tasks access them)

	// Handle pending scene reset
	if (s_bPendingSceneReset)
	{
		s_bPendingSceneReset = false;

		// CRITICAL: Wait for CPU render tasks AND GPU to finish before destroying scene resources
		// This matches the synchronization used for scene loading
		Zenith_Log(LOG_CATEGORY_EDITOR, "Waiting for all render tasks to complete before resetting scene...");
		Zenith_Core::WaitForAllRenderTasks();

		Zenith_Log(LOG_CATEGORY_EDITOR, "Waiting for GPU to become idle before resetting scene...");
		Flux_PlatformAPI::WaitForGPUIdle();

		// Force process any pending deferred deletions
		Zenith_Log(LOG_CATEGORY_EDITOR, "Processing deferred resource deletions...");
		for (u_int u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
		{
			Flux_MemoryManager::ProcessDeferredDeletions();
		}

		// CRITICAL: Clear any pending command lists before resetting scene
		Zenith_Log(LOG_CATEGORY_EDITOR, "Clearing pending command lists...");
		Flux::ClearPendingCommandLists();

		// Safe to reset now - no render tasks active, GPU idle, old resources deleted
		Zenith_Scene::GetCurrentScene().Reset();
		Zenith_Log(LOG_CATEGORY_EDITOR, "Scene reset complete");

		// Clear selection as entity pointers are now invalid
		ClearSelection();

		// Clear game camera reference as it now points to deleted memory
		s_uGameCameraEntity = INVALID_ENTITY_ID;

		// Reset editor camera to initial state
		ResetEditorCameraToDefaults();

		// Clear undo/redo history as entity IDs are now invalid
		Zenith_UndoSystem::Clear();

		return false;
	}

	// Handle pending scene save
	if (s_bPendingSceneSave)
	{
		s_bPendingSceneSave = false;

		try
		{
			// Safe to save now - no render tasks are accessing scene data
			Zenith_Scene::GetCurrentScene().SaveToFile(s_strPendingSceneSavePath);
			Zenith_Log(LOG_CATEGORY_EDITOR, "Scene saved to %s", s_strPendingSceneSavePath.c_str());
		}
		catch (const std::exception& e)
		{
			Zenith_Log(LOG_CATEGORY_EDITOR, "Failed to save scene: %s", e.what());
		}

		s_strPendingSceneSavePath.clear();
	}

	// Handle pending scene load
	// Timeline when loading scene:
	// 1. User clicks "Open Scene" in menu OR "Stop" button -> sets s_bPendingSceneLoad flag
	// 2. Frame continues, ImGui rendered, render tasks submitted and complete
	// 3. Next frame starts -> Update() called BEFORE any rendering
	// 4. Scene loaded here when no render tasks are accessing scene data
	if (s_bPendingSceneLoad)
	{
		s_bPendingSceneLoad = false;

		// CRITICAL: Wait for CPU render tasks AND GPU to finish before destroying scene resources
		// Two-phase synchronization:
		// 1. Wait for CPU-side render tasks (worker threads recording commands into command lists)
		// 2. Wait for GPU to finish executing command buffers
		// Without both, we get access violations when LoadFromFile resets command lists or destroys resources
		Zenith_Log(LOG_CATEGORY_EDITOR, "Waiting for all render tasks to complete before loading scene...");
		Zenith_Core::WaitForAllRenderTasks();  // CPU synchronization


		Zenith_Log(LOG_CATEGORY_EDITOR, "Waiting for GPU to become idle before loading scene...");
		Flux_PlatformAPI::WaitForGPUIdle();  // GPU synchronization

		// Force process any pending deferred deletions to ensure old descriptors are destroyed
		// Without this, descriptor handles might collide between old/new scenes
		Zenith_Log(LOG_CATEGORY_EDITOR, "Processing deferred resource deletions...");
		for (u_int u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
		{
			Flux_MemoryManager::ProcessDeferredDeletions();
		}

		// CRITICAL: Clear any pending command lists before loading scene
		// This prevents stale command list entries from previous frames that may
		// contain pointers to resources that are about to be destroyed
		Zenith_Log(LOG_CATEGORY_EDITOR, "Clearing pending command lists...");
		Flux::ClearPendingCommandLists();

		// Safe to load now - no render tasks active, GPU idle, old resources deleted
		Zenith_Scene::GetCurrentScene().LoadFromFile(s_strPendingSceneLoadPath);
		Zenith_Log(LOG_CATEGORY_EDITOR, "Scene loaded from %s", s_strPendingSceneLoadPath.c_str());

		// Clear selection as entity pointers are now invalid
		ClearSelection();

		// Clear undo/redo history as entity IDs are now invalid
		Zenith_UndoSystem::Clear();

		// Clear game camera entity pointer as it's now invalid (entity from old scene)
		s_uGameCameraEntity = INVALID_ENTITY_ID;

		// If this was a backup scene restore (Play -> Stop transition), clean up
		if (s_bHasSceneBackup && s_strPendingSceneLoadPath == s_strBackupScenePath)
		{
			// Delete the temporary backup file
			std::filesystem::remove(s_strBackupScenePath);
			s_bHasSceneBackup = false;
			s_strBackupScenePath = "";
			Zenith_Log(LOG_CATEGORY_EDITOR, "Backup scene file cleaned up");

			// Unity-style: In Stopped mode, scene is restored but scripts remain "dormant"
			// OnAwake/OnEnable/OnStart will only be dispatched when Play is clicked again.
			// DO NOT dispatch lifecycle here - it would cause OnStart to run which may
			// create runtime entities (like enemies) that shouldn't exist in Stopped mode.

			// After restoring scene, initialize editor camera state from the game's camera
			if (s_bEditorCameraInitialized)
			{
				SwitchToEditorCamera();
				Zenith_Log(LOG_CATEGORY_EDITOR, "Editor camera state updated after scene restore");
			}
		}
		else
		{
			// For regular scene loads, also sync editor camera with the new scene's camera (if it has one)
			if (s_bEditorCameraInitialized)
			{
				SwitchToEditorCamera();
				Zenith_Log(LOG_CATEGORY_EDITOR, "Editor camera synced with loaded scene");
			}
		}

		s_strPendingSceneLoadPath.clear();

		return false;
	}

	// Process deferred ImGui texture deletions
	// We wait N frames before freeing to ensure GPU has finished using them
	for (auto it = s_xPendingDeletions.begin(); it != s_xPendingDeletions.end(); )
	{
		if (it->uFramesUntilDeletion == 0)
		{
			// Safe to delete now - GPU has finished with this texture
			Flux_ImGuiIntegration::UnregisterTexture(it->xHandle, 0);
			it = s_xPendingDeletions.erase(it);
		}
		else
		{
			// Decrement frame counter
			it->uFramesUntilDeletion--;
			++it;
		}
	}

	// One-time initialization: copy game camera position to editor camera on first frame
	// This happens after the game's OnEnter has set up the scene camera
	static bool s_bFirstFrameAfterInit = true;
	if (s_bFirstFrameAfterInit && s_eEditorMode == EditorMode::Stopped)
	{
		s_bFirstFrameAfterInit = false;

		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		if (xScene.m_xMainCameraEntity != INVALID_ENTITY_ID)
		{
			// Initialize editor camera from game camera position
			Zenith_Entity xCameraEntity = xScene.TryGetEntity(xScene.m_xMainCameraEntity);
			if (xCameraEntity.IsValid() && xCameraEntity.HasComponent<Zenith_CameraComponent>())
			{
				Zenith_CameraComponent& xGameCamera = xCameraEntity.GetComponent<Zenith_CameraComponent>();
				xGameCamera.GetPosition(s_xEditorCameraPosition);
				s_fEditorCameraPitch = xGameCamera.GetPitch();
				s_fEditorCameraYaw = xGameCamera.GetYaw();

				// Save reference to game camera for later
				s_uGameCameraEntity = xScene.m_xMainCameraEntity;

				Zenith_Log(LOG_CATEGORY_EDITOR, "Editor camera synced from game camera at (%.1f, %.1f, %.1f)",
					s_xEditorCameraPosition.x, s_xEditorCameraPosition.y, s_xEditorCameraPosition.z);
			}
			else
			{
				Zenith_Log(LOG_CATEGORY_EDITOR, "Could not sync editor camera from game camera");
			}
		}
	}

	// Update bounding boxes for all entities (needed for selection)
	Zenith_SelectionSystem::UpdateBoundingBoxes();

	// Update editor camera controls (when not playing)
	UpdateEditorCamera(1.0f / 60.0f);  // Assume 60fps for now, could use actual delta time

	// Handle editor mode changes
	if (s_eEditorMode == EditorMode::Playing)
	{
		// Game is running normally
	}
	else if (s_eEditorMode == EditorMode::Paused)
	{
		// Game is paused - don't update game logic
	}

	// Handle gizmo mode keyboard shortcuts (when viewport is focused and not playing)
	if (s_eEditorMode == EditorMode::Playing)
	{
		return true;
	}

	if (s_bViewportFocused)
	{
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_W))
		{
			SetGizmoMode(EditorGizmoMode::Translate);
			Flux_Gizmos::SetGizmoMode(GizmoMode::Translate);
		}
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_E))
		{
			SetGizmoMode(EditorGizmoMode::Rotate);
			Flux_Gizmos::SetGizmoMode(GizmoMode::Rotate);
		}
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_R))
		{
			SetGizmoMode(EditorGizmoMode::Scale);
			Flux_Gizmos::SetGizmoMode(GizmoMode::Scale);
		}
	}

	// Handle undo/redo keyboard shortcuts (Ctrl+Z / Ctrl+Y)
	// Check for Ctrl key being held down
	bool bCtrlDown = Zenith_Input::IsKeyDown(ZENITH_KEY_LEFT_CONTROL) ||
	                 Zenith_Input::IsKeyDown(ZENITH_KEY_RIGHT_CONTROL);

	if (bCtrlDown)
	{
		// Ctrl+Z: Undo
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_Z))
		{
			Zenith_UndoSystem::Undo();
		}

		// Ctrl+Y: Redo
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_Y))
		{
			Zenith_UndoSystem::Redo();
		}
	}

	// Handle gizmo interaction first (before object picking)
	HandleGizmoInteraction();

	// Handle object picking (only when not manipulating gizmo)
	if (!Flux_Gizmos::IsInteracting() && !Zenith_Gizmo::IsManipulating())
	{
		HandleObjectPicking();
	}

	return true;
}

void Zenith_Editor::Render()
{
	// Create the main docking space
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
	window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
	window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	window_flags |= ImGuiWindowFlags_NoBackground;
	
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	
	ImGui::Begin("DockSpace", nullptr, window_flags);
	ImGui::PopStyleVar(3);
	
	// Create dockspace
	ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
	
	RenderMainMenuBar();
	ImGui::End();
	
	// Render editor panels
	RenderToolbar();
	RenderHierarchyPanel();
	RenderPropertiesPanel();
	RenderViewport();
	RenderContentBrowser();
	RenderConsolePanel();
	RenderMaterialEditorPanel();

#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	Zenith_EditorPanelMemory::Render();
#endif

	// Animation state machine editor
	// Zenith_AnimationStateMachineEditor::Render();  // TEMPORARILY DISABLED

	// Render gizmos and overlays (after viewport so they appear on top)
	RenderGizmos();
}

void Zenith_Editor::RenderMainMenuBar()
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New Scene"))
			{
				// CRITICAL: Do NOT reset the scene immediately here!
				// This menu item is rendered during SubmitRenderTasks(), which means
				// render tasks are currently active and may be accessing scene data.
				// Resetting the scene now would destroy component pools and entities,
				// causing crashes due to concurrent access.
				//
				// Instead, we defer the reset to the next frame's Update() call,
				// which happens BEFORE any render tasks are submitted.

				s_bPendingSceneReset = true;
				Zenith_Log(LOG_CATEGORY_EDITOR, "Scene reset queued (will reset next frame)");
			}

			if (ImGui::MenuItem("Open Scene", "Ctrl+O"))
			{
				// CRITICAL: Do NOT load the scene immediately here!
				// This menu item is rendered during SubmitRenderTasks(), which means
				// render tasks are currently active and may be accessing scene data.
				// Loading the scene now would call Reset() which destroys component pools,
				// causing crashes due to concurrent access.
				//
				// Instead, we defer the load to the next frame's Update() call,
				// which happens BEFORE any render tasks are submitted.

#ifdef _WIN32
				std::string strFilePath = ShowOpenFileDialog(
					"Zenith Scene Files (*.zscen)\0*.zscen\0All Files (*.*)\0*.*\0",
					"zscen");
				if (!strFilePath.empty())
				{
					s_strPendingSceneLoadPath = strFilePath;
					s_bPendingSceneLoad = true;
					Zenith_Log(LOG_CATEGORY_EDITOR, "Scene load queued: %s (will load next frame)", s_strPendingSceneLoadPath.c_str());
				}
#else
				// Fallback for non-Windows platforms
				s_strPendingSceneLoadPath = "scene.zscen";
				s_bPendingSceneLoad = true;
				Zenith_Log(LOG_CATEGORY_EDITOR, "Scene load queued: %s (will load next frame)", s_strPendingSceneLoadPath.c_str());
#endif
			}

			if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
			{
				// CRITICAL: Do NOT save the scene immediately here!
				// This menu item is rendered during SubmitRenderTasks(), which means
				// render tasks are currently active and may be accessing scene data.
				// While saving doesn't call Reset() like loading does, it's still safer
				// to defer the save operation to maintain consistency with the deferred
				// loading pattern and avoid any potential concurrent access issues.
				//
				// Instead, we defer the save to the next frame's Update() call,
				// which happens BEFORE any render tasks are submitted.

#ifdef _WIN32
				std::string strFilePath = ShowSaveFileDialog(
					"Zenith Scene Files (*.zscen)\0*.zscen\0All Files (*.*)\0*.*\0",
					"zscen",
					"scene.zscen");
				if (!strFilePath.empty())
				{
					s_strPendingSceneSavePath = strFilePath;
					s_bPendingSceneSave = true;
					Zenith_Log(LOG_CATEGORY_EDITOR, "Scene save queued: %s (will save next frame)", s_strPendingSceneSavePath.c_str());
				}
#else
				// Fallback for non-Windows platforms
				s_strPendingSceneSavePath = "scene.zscen";
				s_bPendingSceneSave = true;
				Zenith_Log(LOG_CATEGORY_EDITOR, "Scene save queued: %s (will save next frame)", s_strPendingSceneSavePath.c_str());
#endif
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Exit"))
			{
				// TODO: Implement graceful shutdown
				// Would trigger application exit
				Zenith_Log(LOG_CATEGORY_EDITOR, "Exit - Not yet implemented");
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit"))
		{
			bool bCanUndo = Zenith_UndoSystem::CanUndo();
			bool bCanRedo = Zenith_UndoSystem::CanRedo();

			if (ImGui::MenuItem("Undo", "Ctrl+Z", false, bCanUndo))
			{
				Zenith_UndoSystem::Undo();
			}

			// Show tooltip with undo description
			if (bCanUndo && ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Undo: %s", Zenith_UndoSystem::GetUndoDescription());
			}

			if (ImGui::MenuItem("Redo", "Ctrl+Y", false, bCanRedo))
			{
				Zenith_UndoSystem::Redo();
			}

			// Show tooltip with redo description
			if (bCanRedo && ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Redo: %s", Zenith_UndoSystem::GetRedoDescription());
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View"))
		{
			if (ImGui::MenuItem("Hierarchy"))
			{
				// TODO: Toggle hierarchy panel visibility
				Zenith_Log(LOG_CATEGORY_EDITOR, "Toggle Hierarchy - Not yet implemented");
			}

			if (ImGui::MenuItem("Properties"))
			{
				// TODO: Toggle properties panel visibility
				Zenith_Log(LOG_CATEGORY_EDITOR, "Toggle Properties - Not yet implemented");
			}

			if (ImGui::MenuItem("Console"))
			{
				// TODO: Toggle console panel visibility
				Zenith_Log(LOG_CATEGORY_EDITOR, "Toggle Console - Not yet implemented");
			}

#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
			if (ImGui::MenuItem("Memory Profiler", nullptr, Zenith_EditorPanelMemory::IsVisible()))
			{
				Zenith_EditorPanelMemory::SetVisible(!Zenith_EditorPanelMemory::IsVisible());
			}
#endif

			ImGui::Separator();

			if (ImGui::MenuItem("Animation State Machine Editor"))
			{
				// Zenith_AnimationStateMachineEditor::Toggle();  // TEMPORARILY DISABLED
			}

			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}
}

void Zenith_Editor::RenderToolbar()
{
	Zenith_EditorPanelToolbar::Render(s_eEditorMode, s_eGizmoMode);
}

void Zenith_Editor::RenderHierarchyPanel()
{
	Zenith_EditorPanelHierarchy::Render(Zenith_Scene::GetCurrentScene(), s_uGameCameraEntity);
}

void Zenith_Editor::RenderPropertiesPanel()
{
	Zenith_EditorPanelProperties::Render(GetSelectedEntity(), s_uPrimarySelectedEntityID);
}

void Zenith_Editor::RenderViewport()
{
	ViewportState xState = {
		s_xViewportSize,
		s_xViewportPos,
		s_bViewportHovered,
		s_bViewportFocused,
		s_xCachedGameTextureHandle,
		s_xCachedImageViewHandle,
		s_xPendingDeletions
	};
	Zenith_EditorPanelViewport::Render(xState);
}

void Zenith_Editor::HandleObjectPicking()
{
	// Only pick when viewport is hovered
	if (!s_bViewportHovered)
		return;

	// Only pick on left mouse button press (not held)
	if (!Zenith_Input::WasKeyPressedThisFrame(ZENITH_MOUSE_BUTTON_LEFT))
		return;

	// Get mouse position in screen space
	Zenith_Maths::Vector2_64 xGlobalMousePos;
	Zenith_Input::GetMousePosition(xGlobalMousePos);

	// Convert to viewport-relative coordinates
	Zenith_Maths::Vector2 xViewportMousePos = {
		static_cast<float>(xGlobalMousePos.x - s_xViewportPos.x),
		static_cast<float>(xGlobalMousePos.y - s_xViewportPos.y)
	};

	// Check if mouse is within viewport bounds
	if (xViewportMousePos.x < 0 || xViewportMousePos.x > s_xViewportSize.x ||
		xViewportMousePos.y < 0 || xViewportMousePos.y > s_xViewportSize.y)
		return;

	// Get camera matrices for ray casting
	Zenith_Maths::Matrix4 xViewMatrix, xProjMatrix;
	BuildViewMatrix(xViewMatrix);
	BuildProjectionMatrix(xProjMatrix);

	// Convert screen position to world-space ray
	Zenith_Maths::Vector3 xRayDir = Zenith_Gizmo::ScreenToWorldRay(
		xViewportMousePos,
		{ 0, 0 },  // Viewport relative, so offset is 0
		s_xViewportSize,
		xViewMatrix,
		xProjMatrix
	);

	// Ray origin is camera position
	Zenith_Maths::Vector4 xCameraPos;
	GetCameraPosition(xCameraPos);
	Zenith_Maths::Vector3 xRayOrigin(xCameraPos.x, xCameraPos.y, xCameraPos.z);

	// Perform raycast to find entity under mouse - now returns EntityID
	Zenith_EntityID uHitEntityID = Zenith_SelectionSystem::RaycastSelect(xRayOrigin, xRayDir);

	if (uHitEntityID != INVALID_ENTITY_ID)
	{
		SelectEntity(uHitEntityID);
	}
	else
	{
		ClearSelection();
	}
}

void Zenith_Editor::RenderGizmos()
{
	// Set target entity and gizmo mode for Flux_Gizmos
	// Task must always be submitted once per frame (even if null) for proper synchronization
	Zenith_Entity* pxSelectedEntity = nullptr;

	// Only render gizmos in Stopped or Paused mode (not during active play)
	if (s_eEditorMode != EditorMode::Playing)
	{
		pxSelectedEntity = GetSelectedEntity();
	}

	// CRITICAL: Only update target/mode when NOT interacting!
	// SetTargetEntity and SetGizmoMode reset s_bIsInteracting, which would
	// break mid-drag operations. Only update when safe to do so.
	if (!Flux_Gizmos::IsInteracting())
	{
		Flux_Gizmos::SetTargetEntity(pxSelectedEntity);
		Flux_Gizmos::SetGizmoMode(static_cast<GizmoMode>(s_eGizmoMode));
	}

	// Submit Flux_Gizmos render task (renders 3D gizmos in Vulkan)
	// Must always submit exactly once per frame, task will early-out if no target entity
	Flux_Gizmos::SubmitRenderTask();

	// Optionally render selection bounding box for visual feedback
	// Zenith_SelectionSystem::RenderSelectedBoundingBox(pxSelectedEntity);
}

void Zenith_Editor::HandleGizmoInteraction()
{
	// Only handle gizmo interaction when viewport is hovered and entity selected
	if (!s_bViewportHovered || !HasSelection())
		return;

	// Only handle in Stopped or Paused mode
	if (s_eEditorMode == EditorMode::Playing)
		return;

	// Get camera matrices for ray casting
	Zenith_Maths::Matrix4 xViewMatrix, xProjMatrix;
	BuildViewMatrix(xViewMatrix);
	BuildProjectionMatrix(xProjMatrix);

	// Get mouse position
	Zenith_Maths::Vector2_64 xGlobalMousePos;
	Zenith_Input::GetMousePosition(xGlobalMousePos);

	Zenith_Maths::Vector2 xViewportMousePos = {
		static_cast<float>(xGlobalMousePos.x - s_xViewportPos.x),
		static_cast<float>(xGlobalMousePos.y - s_xViewportPos.y)
	};

	// Debug: Log mouse position every frame during interaction
	static int s_iFrameCounter = 0;
	if (Flux_Gizmos::IsInteracting())
	{
		if (++s_iFrameCounter % 60 == 0) // Log every 60 frames
		{
			Zenith_Log(LOG_CATEGORY_EDITOR, "Mouse: Global=(%.1f,%.1f), Viewport=(%.1f,%.1f)",
				xGlobalMousePos.x, xGlobalMousePos.y,
				xViewportMousePos.x, xViewportMousePos.y);
		}
	}
	else
	{
		s_iFrameCounter = 0;
	}

	// Convert screen position to world-space ray
	Zenith_Maths::Vector3 xRayDir = Zenith_Gizmo::ScreenToWorldRay(
		xViewportMousePos,
		{ 0, 0 },
		s_xViewportSize,
		xViewMatrix,
		xProjMatrix
	);

	// Ray origin is camera position
	Zenith_Maths::Vector4 xCameraPos;
	GetCameraPosition(xCameraPos);
	Zenith_Maths::Vector3 xRayOrigin(xCameraPos.x, xCameraPos.y, xCameraPos.z);

	// Handle mouse input for gizmo interaction
	if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_MOUSE_BUTTON_LEFT))
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Mouse left pressed - viewport hovered=%d, selected=%zu", s_bViewportHovered, s_xSelectedEntityIDs.size());
		Flux_Gizmos::BeginInteraction(xRayOrigin, xRayDir);
		Zenith_Log(LOG_CATEGORY_EDITOR, "After BeginInteraction: IsInteracting=%d", Flux_Gizmos::IsInteracting());
	}
	
	// Update interaction while dragging (can happen same frame as BeginInteraction)
	bool bIsKeyDown = Zenith_Input::IsKeyDown(ZENITH_MOUSE_BUTTON_LEFT);
	bool bIsInteracting = Flux_Gizmos::IsInteracting();
	
	if (bIsKeyDown || bIsInteracting)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Check UpdateInteraction: IsKeyDown=%d, IsInteracting=%d", bIsKeyDown, bIsInteracting);
	}
	
	if (bIsKeyDown && bIsInteracting)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Calling UpdateInteraction: ViewportMouse=(%.1f,%.1f)",
			xViewportMousePos.x, xViewportMousePos.y);
		Flux_Gizmos::UpdateInteraction(xRayOrigin, xRayDir);
	}
	
	// End interaction when mouse released
	if (!Zenith_Input::IsKeyDown(ZENITH_MOUSE_BUTTON_LEFT) && Flux_Gizmos::IsInteracting())
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Ending interaction");
		Flux_Gizmos::EndInteraction();
	}
}

void Zenith_Editor::SetEditorMode(EditorMode eMode)
{
	if (s_eEditorMode == eMode)
		return;

	EditorMode oldMode = s_eEditorMode;
	s_eEditorMode = eMode;

	// Handle mode transitions

	// STOPPED -> PLAYING: Backup scene state and switch to game camera
	if (oldMode == EditorMode::Stopped && eMode == EditorMode::Playing)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Editor: Entering Play Mode");

		// Generate backup file path in temp directory
		s_strBackupScenePath = std::filesystem::temp_directory_path().string() + "/zenith_scene_backup.zscen";

		// Save current scene state to backup file (persistent entities only)
		// Transient entities are NOT included because:
		// 1. They often have runtime-only resources (procedural meshes) that can't be serialized
		// 2. Behaviour scripts will regenerate them in OnStart (which runs after restore)
		// 3. Including them causes duplicate entities after OnStart regenerates
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		xScene.SaveToFile(s_strBackupScenePath, false);
		s_bHasSceneBackup = true;

		Zenith_Log(LOG_CATEGORY_EDITOR, "Scene state backed up to: %s", s_strBackupScenePath.c_str());

		// Check if a main camera has already been set via the editor
		s_uGameCameraEntity = xScene.GetMainCameraEntity();

		// If no main camera is set, search for one
		if (s_uGameCameraEntity == INVALID_ENTITY_ID)
		{
			Zenith_Vector<Zenith_CameraComponent*> xCameras;
			xScene.GetAllOfComponentType<Zenith_CameraComponent>(xCameras);

			for (Zenith_Vector<Zenith_CameraComponent*>::Iterator xIt(xCameras); !xIt.Done(); xIt.Next())
			{
				Zenith_CameraComponent* pxCam = xIt.GetData();
				Zenith_Entity* pxEntity = &pxCam->GetParentEntity();
				// Just use the first camera we find (there should only be one game camera)
				s_uGameCameraEntity = pxEntity->GetEntityID();
				xScene.SetMainCameraEntity(s_uGameCameraEntity);
				break;
			}
		}

		// Unity-style lifecycle: Dispatch OnAwake/OnEnable for all entities when entering Play mode
		// In Stopped mode, scene was loaded but scripts were "dormant" - now we wake them up
		Zenith_Log(LOG_CATEGORY_EDITOR, "Editor: Dispatching OnAwake/OnEnable for %u entities", xScene.GetEntityCount());
		Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();

		// First pass: OnAwake for all entities
		const Zenith_Vector<Zenith_EntityID>& xEntityIDs = xScene.GetActiveEntities();
		for (u_int u = 0; u < xEntityIDs.GetSize(); ++u)
		{
			Zenith_EntityID uID = xEntityIDs.Get(u);
			if (xScene.EntityExists(uID))
			{
				Zenith_Entity xEntity = xScene.GetEntity(uID);
				xRegistry.DispatchOnAwake(xEntity);
			}
		}

		// Second pass: OnEnable for enabled entities, and mark all as awoken
		for (u_int u = 0; u < xEntityIDs.GetSize(); ++u)
		{
			Zenith_EntityID uID = xEntityIDs.Get(u);
			if (xScene.EntityExists(uID))
			{
				Zenith_Entity xEntity = xScene.GetEntity(uID);
				if (xEntity.IsEnabled())
				{
					xRegistry.DispatchOnEnable(xEntity);
				}
				xScene.MarkEntityAwoken(uID);
			}
		}

		// Third pass: OnStart for enabled entities (Unity-style: called before first Update)
		// Re-fetch entity list since OnAwake/OnEnable may have created new entities
		const Zenith_Vector<Zenith_EntityID>& xStartEntityIDs = xScene.GetActiveEntities();
		for (u_int u = 0; u < xStartEntityIDs.GetSize(); ++u)
		{
			Zenith_EntityID uID = xStartEntityIDs.Get(u);
			if (xScene.EntityExists(uID))
			{
				Zenith_Entity xEntity = xScene.GetEntity(uID);
				if (xEntity.IsEnabled())
				{
					xRegistry.DispatchOnStart(xEntity);
				}
				xScene.MarkEntityStarted(uID);
			}
		}
	}

	// PLAYING/PAUSED -> STOPPED: Restore scene state and switch to editor camera
	else if (oldMode != EditorMode::Stopped && eMode == EditorMode::Stopped)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Editor: Stopping Play Mode");

		// CRITICAL: Defer scene restore to next frame's Update() call
		// Loading scenes mid-frame causes issues:
		// 1. New terrain components created during load
		// 2. Same frame's SubmitRenderTasks tries to render them
		// 3. But render systems haven't properly registered new components yet
		// By deferring to Update(), the load happens BEFORE any rendering

		if (s_bHasSceneBackup && !s_strBackupScenePath.empty())
		{
			// Queue the scene restore for next frame
			s_bPendingSceneLoad = true;
			s_strPendingSceneLoadPath = s_strBackupScenePath;

			Zenith_Log(LOG_CATEGORY_EDITOR, "Scene restore queued for next frame: %s", s_strBackupScenePath.c_str());

			// Note: We don't clear s_bHasSceneBackup or s_strBackupScenePath here
			// They'll be cleared in Update() after the load completes
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_EDITOR, "Warning: No scene backup available to restore");
		}

		// Clear the game camera reference since scene will be reloaded
		s_uGameCameraEntity = INVALID_ENTITY_ID;
	}

	// PAUSED state - suspend scene updates but stay on game camera
	else if (eMode == EditorMode::Paused)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Editor: Pausing - physics and scene updates suspended");
		// Stay on game camera during pause so player can see game state
	}

	// PAUSED -> PLAYING: Resume scene updates
	else if (oldMode == EditorMode::Paused && eMode == EditorMode::Playing)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Editor: Resuming - physics and scene updates resumed");
	}
}

void Zenith_Editor::FlushPendingSceneOperations()
{
	// Handle pending scene reset
	if (s_bPendingSceneReset)
	{
		s_bPendingSceneReset = false;

		Zenith_Log(LOG_CATEGORY_EDITOR, "[FlushPending] Waiting for render tasks before scene reset...");
		Zenith_Core::WaitForAllRenderTasks();

		// Flush staging buffer to complete any pending copy operations before destroying buffers
		// Must use BeginFrame/EndFrame to properly bracket the staging buffer flush with command buffer recording
		Zenith_Log(LOG_CATEGORY_EDITOR, "[FlushPending] Flushing staging buffer...");
		Flux_MemoryManager::BeginFrame();
		Flux_MemoryManager::EndFrame(false);  // false = don't defer, wait synchronously

		Zenith_Log(LOG_CATEGORY_EDITOR, "[FlushPending] Waiting for GPU idle before scene reset...");
		Flux_PlatformAPI::WaitForGPUIdle();

		for (u_int u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
		{
			Flux_MemoryManager::ProcessDeferredDeletions();
		}

		Flux::ClearPendingCommandLists();

		Zenith_Scene::GetCurrentScene().Reset();
		Zenith_Log(LOG_CATEGORY_EDITOR, "[FlushPending] Scene reset complete");

		ClearSelection();
		s_uGameCameraEntity = INVALID_ENTITY_ID;
		ResetEditorCameraToDefaults();
		Zenith_UndoSystem::Clear();
	}

	// Handle pending scene save
	if (s_bPendingSceneSave)
	{
		s_bPendingSceneSave = false;

		try
		{
			Zenith_Scene::GetCurrentScene().SaveToFile(s_strPendingSceneSavePath);
			Zenith_Log(LOG_CATEGORY_EDITOR, "[FlushPending] Scene saved to %s", s_strPendingSceneSavePath.c_str());
		}
		catch (const std::exception& e)
		{
			Zenith_Log(LOG_CATEGORY_EDITOR, "[FlushPending] Failed to save scene: %s", e.what());
		}

		s_strPendingSceneSavePath.clear();
	}

	// Handle pending scene load
	if (s_bPendingSceneLoad)
	{
		s_bPendingSceneLoad = false;

		Zenith_Log(LOG_CATEGORY_EDITOR, "[FlushPending] Waiting for render tasks before scene load...");
		Zenith_Core::WaitForAllRenderTasks();

		// Flush staging buffer to complete any pending copy operations before destroying buffers
		// Must use BeginFrame/EndFrame to properly bracket the staging buffer flush with command buffer recording
		Zenith_Log(LOG_CATEGORY_EDITOR, "[FlushPending] Flushing staging buffer...");
		Flux_MemoryManager::BeginFrame();
		Flux_MemoryManager::EndFrame(false);  // false = don't defer, wait synchronously

		Zenith_Log(LOG_CATEGORY_EDITOR, "[FlushPending] Waiting for GPU idle before scene load...");
		Flux_PlatformAPI::WaitForGPUIdle();

		for (u_int u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
		{
			Flux_MemoryManager::ProcessDeferredDeletions();
		}

		Flux::ClearPendingCommandLists();

		Zenith_Scene::GetCurrentScene().LoadFromFile(s_strPendingSceneLoadPath);
		Zenith_Log(LOG_CATEGORY_EDITOR, "[FlushPending] Scene loaded from %s", s_strPendingSceneLoadPath.c_str());

		ClearSelection();
		Zenith_UndoSystem::Clear();
		s_uGameCameraEntity = INVALID_ENTITY_ID;

		// If this was a backup scene restore, clean up
		if (s_bHasSceneBackup && s_strPendingSceneLoadPath == s_strBackupScenePath)
		{
			std::filesystem::remove(s_strBackupScenePath);
			s_bHasSceneBackup = false;
			s_strBackupScenePath = "";
			Zenith_Log(LOG_CATEGORY_EDITOR, "[FlushPending] Backup scene file cleaned up");

			// Unity-style: In Stopped mode, scene is restored but scripts remain "dormant"
			// OnAwake/OnEnable/OnStart will only be dispatched when Play is clicked again
			// DO NOT dispatch lifecycle here - it would cause OnStart to run which may create entities

			if (s_bEditorCameraInitialized)
			{
				SwitchToEditorCamera();
			}
		}
		else if (s_bEditorCameraInitialized)
		{
			SwitchToEditorCamera();
		}

		s_strPendingSceneLoadPath.clear();
	}
}

//------------------------------------------------------------------------------
// Multi-Select System Implementation
//------------------------------------------------------------------------------

void Zenith_Editor::SelectEntity(Zenith_EntityID uEntityID, bool bAddToSelection)
{
	if (uEntityID == INVALID_ENTITY_ID)
	{
		return;
	}

	if (bAddToSelection)
	{
		// Add to existing selection
		s_xSelectedEntityIDs.insert(uEntityID);
	}
	else
	{
		// Replace selection
		s_xSelectedEntityIDs.clear();
		s_xSelectedEntityIDs.insert(uEntityID);
	}

	// Update primary selection and last clicked
	s_uPrimarySelectedEntityID = uEntityID;
	s_uLastClickedEntityID = uEntityID;

	Zenith_Log(LOG_CATEGORY_EDITOR, "Editor: Selected entity %u (total: %zu)", uEntityID, s_xSelectedEntityIDs.size());

	// Update Flux_Gizmos target entity (primary selection)
	Zenith_Entity* pxEntity = GetSelectedEntity();
	if (pxEntity)
	{
		Flux_Gizmos::SetTargetEntity(pxEntity);
	}
}

void Zenith_Editor::SelectRange(Zenith_EntityID uEndEntityID)
{
	if (s_uLastClickedEntityID == INVALID_ENTITY_ID || uEndEntityID == INVALID_ENTITY_ID)
	{
		// No start point for range, just select the end entity
		SelectEntity(uEndEntityID, false);
		return;
	}

	// For range selection, we need to select all entities "between" start and end
	// Since entity IDs may not be contiguous, we iterate through the active entities
	// and select all entities with indices in the range [min(start,end), max(start,end)]
	uint32_t uStartIndex = std::min(s_uLastClickedEntityID.m_uIndex, uEndEntityID.m_uIndex);
	uint32_t uEndIndex = std::max(s_uLastClickedEntityID.m_uIndex, uEndEntityID.m_uIndex);

	// Clear existing selection for shift+click (standard behavior)
	s_xSelectedEntityIDs.clear();

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Select all entities in the index range that exist in the scene
	const Zenith_Vector<Zenith_EntityID>& xActiveEntities = xScene.GetActiveEntities();
	for (u_int u = 0; u < xActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xEntityID = xActiveEntities.Get(u);
		if (xEntityID.m_uIndex >= uStartIndex && xEntityID.m_uIndex <= uEndIndex)
		{
			s_xSelectedEntityIDs.insert(xEntityID);
		}
	}

	// Update primary selection to the end entity
	s_uPrimarySelectedEntityID = uEndEntityID;
	// Keep s_uLastClickedEntityID unchanged for further range selections

	Zenith_Log(LOG_CATEGORY_EDITOR, "Editor: Range selected %zu entities", s_xSelectedEntityIDs.size());

	// Update Flux_Gizmos target entity
	Zenith_Entity* pxEntity = GetSelectedEntity();
	if (pxEntity)
	{
		Flux_Gizmos::SetTargetEntity(pxEntity);
	}
}

void Zenith_Editor::ToggleEntitySelection(Zenith_EntityID uEntityID)
{
	if (uEntityID == INVALID_ENTITY_ID)
	{
		return;
	}

	auto it = s_xSelectedEntityIDs.find(uEntityID);
	if (it != s_xSelectedEntityIDs.end())
	{
		// Already selected - deselect
		s_xSelectedEntityIDs.erase(it);

		// Update primary selection if we just removed it
		if (s_uPrimarySelectedEntityID == uEntityID)
		{
			s_uPrimarySelectedEntityID = s_xSelectedEntityIDs.empty() ?
				INVALID_ENTITY_ID : *s_xSelectedEntityIDs.begin();
		}

		Zenith_Log(LOG_CATEGORY_EDITOR, "Editor: Deselected entity %u (total: %zu)", uEntityID, s_xSelectedEntityIDs.size());
	}
	else
	{
		// Not selected - add to selection
		s_xSelectedEntityIDs.insert(uEntityID);
		s_uPrimarySelectedEntityID = uEntityID;

		Zenith_Log(LOG_CATEGORY_EDITOR, "Editor: Added entity %u to selection (total: %zu)", uEntityID, s_xSelectedEntityIDs.size());
	}

	// Update last clicked for range selection
	s_uLastClickedEntityID = uEntityID;

	// Update Flux_Gizmos target entity
	Zenith_Entity* pxEntity = GetSelectedEntity();
	Flux_Gizmos::SetTargetEntity(pxEntity);
}

void Zenith_Editor::ClearSelection()
{
	s_xSelectedEntityIDs.clear();
	s_uPrimarySelectedEntityID = INVALID_ENTITY_ID;
	s_uLastClickedEntityID = INVALID_ENTITY_ID;
	Flux_Gizmos::SetTargetEntity(nullptr);
}

void Zenith_Editor::DeselectEntity(Zenith_EntityID uEntityID)
{
	s_xSelectedEntityIDs.erase(uEntityID);

	// Update primary selection if we deselected it
	if (s_uPrimarySelectedEntityID == uEntityID)
	{
		s_uPrimarySelectedEntityID = s_xSelectedEntityIDs.empty() ?
			INVALID_ENTITY_ID : *s_xSelectedEntityIDs.begin();
	}

	// Update gizmo target
	Zenith_Entity* pxEntity = GetSelectedEntity();
	Flux_Gizmos::SetTargetEntity(pxEntity);
}

bool Zenith_Editor::IsSelected(Zenith_EntityID uEntityID)
{
	return s_xSelectedEntityIDs.find(uEntityID) != s_xSelectedEntityIDs.end();
}

Zenith_Entity* Zenith_Editor::GetSelectedEntity()
{
	if (s_uPrimarySelectedEntityID == INVALID_ENTITY_ID)
		return nullptr;

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Check if entity still exists in the scene
	if (!xScene.EntityExists(s_uPrimarySelectedEntityID))
	{
		// Entity no longer exists - remove from selection
		s_xSelectedEntityIDs.erase(s_uPrimarySelectedEntityID);
		s_uPrimarySelectedEntityID = s_xSelectedEntityIDs.empty() ?
			INVALID_ENTITY_ID : *s_xSelectedEntityIDs.begin();
		return nullptr;
	}

	// Return pointer to static entity handle (valid until next call)
	static Zenith_Entity s_xSelectedEntity;
	s_xSelectedEntity = xScene.GetEntity(s_uPrimarySelectedEntityID);
	return &s_xSelectedEntity;
}

//------------------------------------------------------------------------------
// Content Browser Implementation
//------------------------------------------------------------------------------

void Zenith_Editor::RenderContentBrowser()
{
	ContentBrowserState xState = {
		s_strCurrentDirectory,
		s_xDirectoryContents,
		s_xFilteredContents,
		s_bDirectoryNeedsRefresh,
		s_szSearchBuffer,
		sizeof(s_szSearchBuffer),
		s_iAssetTypeFilter,
		s_iSelectedContentIndex
	};
	Zenith_EditorPanelContentBrowser::Render(xState);
}

//------------------------------------------------------------------------------
// Console Implementation
//------------------------------------------------------------------------------

void Zenith_Editor::AddLogMessage(const char* szMessage, ConsoleLogEntry::LogLevel eLevel, Zenith_LogCategory eCategory)
{
	ConsoleLogEntry xEntry;
	xEntry.m_eLevel = eLevel;
	xEntry.m_eCategory = eCategory;
	xEntry.m_strMessage = szMessage;

	// Get current time for timestamp
	auto now = std::chrono::system_clock::now();
	auto time = std::chrono::system_clock::to_time_t(now);
	char timeBuffer[32];
	struct tm localTime;
	localtime_s(&localTime, &time);
	strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &localTime);
	xEntry.m_strTimestamp = timeBuffer;

	s_xConsoleLogs.push_back(xEntry);

	// Limit console entries
	if (s_xConsoleLogs.size() > MAX_CONSOLE_ENTRIES)
	{
		s_xConsoleLogs.erase(s_xConsoleLogs.begin());
	}
}

void Zenith_Editor::ClearConsole()
{
	s_xConsoleLogs.clear();
}

void Zenith_Editor::RenderConsolePanel()
{
	Zenith_EditorPanelConsole::Render(
		s_xConsoleLogs,
		s_bConsoleAutoScroll,
		s_bShowConsoleInfo,
		s_bShowConsoleWarnings,
		s_bShowConsoleErrors,
		s_xCategoryFilters);
}

//------------------------------------------------------------------------------
// Material Editor Implementation
//------------------------------------------------------------------------------

void Zenith_Editor::SelectMaterial(Zenith_MaterialAsset* pMaterial)
{
	s_pxSelectedMaterial = pMaterial;
	s_bShowMaterialEditor = true;
	if (pMaterial)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[Editor] Selected material: %s", pMaterial->GetName().c_str());
	}
}

void Zenith_Editor::ClearMaterialSelection()
{
	s_pxSelectedMaterial = nullptr;
}

void Zenith_Editor::RenderMaterialEditorPanel()
{
	MaterialEditorState xState = {
		s_pxSelectedMaterial,
		s_bShowMaterialEditor
	};
	Zenith_EditorPanelMaterialEditor::Render(xState);
}

// Editor Camera System is implemented in Zenith_EditorCamera.cpp

#endif // ZENITH_TOOLS
