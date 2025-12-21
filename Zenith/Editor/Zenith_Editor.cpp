#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_Editor.h"
#include "Flux/Flux_MaterialAsset.h"

// Bridge function called from Zenith_Log macro to add to editor console
// NOTE: Must be defined after including Zenith_Editor.h
void Zenith_EditorAddLogMessage(const char* szMessage, int eLevel)
{
	// Convert int to log level enum
	ConsoleLogEntry::LogLevel xLevel = ConsoleLogEntry::LogLevel::Info;
	switch (eLevel)
	{
	case 0: xLevel = ConsoleLogEntry::LogLevel::Info; break;
	case 1: xLevel = ConsoleLogEntry::LogLevel::Warning; break;
	case 2: xLevel = ConsoleLogEntry::LogLevel::Error; break;
	}
	Zenith_Editor::AddLogMessage(szMessage, xLevel);
}

#include "Zenith_SelectionSystem.h"
#include "Zenith_Gizmo.h"
#include "Zenith_UndoSystem.h"
#include "Flux/Gizmos/Flux_Gizmos.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_ComponentRegistry.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TextComponent.h"
#include "Input/Zenith_Input.h"
#include "Flux/Flux_Graphics.h"
#include "Vulkan/Zenith_Vulkan.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
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
Zenith_EntityID Zenith_Editor::s_uSelectedEntityID = INVALID_ENTITY_ID;
Zenith_Maths::Vector2 Zenith_Editor::s_xViewportSize = { 1280, 720 };
Zenith_Maths::Vector2 Zenith_Editor::s_xViewportPos = { 0, 0 };
bool Zenith_Editor::s_bViewportHovered = false;
bool Zenith_Editor::s_bViewportFocused = false;
Zenith_Scene* Zenith_Editor::s_pxBackupScene = nullptr;
bool Zenith_Editor::s_bHasSceneBackup = false;
std::string Zenith_Editor::s_strBackupScenePath = "";
bool Zenith_Editor::s_bPendingSceneLoad = false;
std::string Zenith_Editor::s_strPendingSceneLoadPath = "";
bool Zenith_Editor::s_bPendingSceneSave = false;
std::string Zenith_Editor::s_strPendingSceneSavePath = "";
bool Zenith_Editor::s_bPendingSceneReset = false;

// Content Browser state
std::string Zenith_Editor::s_strCurrentDirectory = ASSETS_ROOT;
std::vector<ContentBrowserEntry> Zenith_Editor::s_xDirectoryContents;
bool Zenith_Editor::s_bDirectoryNeedsRefresh = true;

// Console state
std::vector<ConsoleLogEntry> Zenith_Editor::s_xConsoleLogs;
bool Zenith_Editor::s_bConsoleAutoScroll = true;
bool Zenith_Editor::s_bShowConsoleInfo = true;
bool Zenith_Editor::s_bShowConsoleWarnings = true;
bool Zenith_Editor::s_bShowConsoleErrors = true;

// Material Editor state
Flux_MaterialAsset* Zenith_Editor::s_pxSelectedMaterial = nullptr;
bool Zenith_Editor::s_bShowMaterialEditor = true;

// Editor camera state (standalone, not part of entity/scene system)
static constexpr Zenith_Maths::Vector3 xINITIAL_EDITOR_CAMERA_POSITION = { 0, 100, 0 };
static constexpr float xINITIAL_EDITOR_CAMERA_PITCH = 0.f;
static constexpr float xINITIAL_EDITOR_CAMERA_YAW = 0.f;
static constexpr float xINITIAL_EDITOR_CAMERA_FOV = 45.f;
static constexpr float xINITIAL_EDITOR_CAMERA_NEAR = 1.f;
static constexpr float xINITIAL_EDITOR_CAMERA_FAR = 2000.f;


Zenith_Maths::Vector3 Zenith_Editor::s_xEditorCameraPosition = xINITIAL_EDITOR_CAMERA_POSITION;
double Zenith_Editor::s_fEditorCameraPitch = xINITIAL_EDITOR_CAMERA_PITCH;
double Zenith_Editor::s_fEditorCameraYaw = xINITIAL_EDITOR_CAMERA_YAW;
float Zenith_Editor::s_fEditorCameraFOV = xINITIAL_EDITOR_CAMERA_FOV;
float Zenith_Editor::s_fEditorCameraNear = xINITIAL_EDITOR_CAMERA_NEAR;
float Zenith_Editor::s_fEditorCameraFar = xINITIAL_EDITOR_CAMERA_FAR;
Zenith_Entity* Zenith_Editor::s_pxGameCameraEntity = nullptr;
float Zenith_Editor::s_fEditorCameraMoveSpeed = 50.0f;
float Zenith_Editor::s_fEditorCameraRotateSpeed = 0.1f;
bool Zenith_Editor::s_bEditorCameraInitialized = false;

// Cache the ImGui descriptor set for the game viewport texture
static vk::DescriptorSet s_xCachedGameTextureDescriptorSet = VK_NULL_HANDLE;
static vk::ImageView s_xCachedImageView = VK_NULL_HANDLE;

// Deferred deletion queue for descriptor sets
// Vulkan requires waiting for GPU to finish using resources before freeing them
struct PendingDescriptorSetDeletion
{
	VkDescriptorSet descriptorSet;
	u_int framesUntilDeletion;
};
static std::vector<PendingDescriptorSetDeletion> s_xPendingDeletions;

void Zenith_Editor::Initialise()
{
	s_eEditorMode = EditorMode::Stopped;
	s_uSelectedEntityID = INVALID_ENTITY_ID;
	s_eGizmoMode = EditorGizmoMode::Translate;

	// Initialize material system
	Flux_MaterialAsset::Initialize();

	// Initialize editor subsystems
	Zenith_SelectionSystem::Initialise();
	Zenith_Gizmo::Initialise();

	// Initialize editor camera
	InitializeEditorCamera();
}

void Zenith_Editor::Shutdown()
{
	// Process all pending deletions immediately on shutdown
	// At shutdown, we can safely assume all GPU work is done or will be waited for
	for (auto& pending : s_xPendingDeletions)
	{
		ImGui_ImplVulkan_RemoveTexture(pending.descriptorSet);
	}
	s_xPendingDeletions.clear();

	// Free the cached ImGui descriptor set
	if (s_xCachedGameTextureDescriptorSet != VK_NULL_HANDLE)
	{
		ImGui_ImplVulkan_RemoveTexture(s_xCachedGameTextureDescriptorSet);
		s_xCachedGameTextureDescriptorSet = VK_NULL_HANDLE;
		s_xCachedImageView = VK_NULL_HANDLE;
	}

	if (s_pxBackupScene)
	{
		delete s_pxBackupScene;
		s_pxBackupScene = nullptr;
	}

	// Reset editor camera state
	s_bEditorCameraInitialized = false;
	
	// Clear material selection and shutdown material system
	s_pxSelectedMaterial = nullptr;
	Flux_MaterialAsset::Shutdown();

	// Shutdown editor subsystems
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
		Zenith_Log("Waiting for all render tasks to complete before resetting scene...");
		Zenith_Core::WaitForAllRenderTasks();

		Zenith_Log("Waiting for GPU to become idle before resetting scene...");
		Zenith_Vulkan::WaitForGPUIdle();

		// Force process any pending deferred deletions
		Zenith_Log("Processing deferred resource deletions...");
		for (u_int u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
		{
			Flux_MemoryManager::ProcessDeferredDeletions();
		}

		// CRITICAL: Clear any pending command lists before resetting scene
		Zenith_Log("Clearing pending command lists...");
		Flux::ClearPendingCommandLists();

		// Safe to reset now - no render tasks active, GPU idle, old resources deleted
		Zenith_Scene::GetCurrentScene().Reset();
		Zenith_Log("Scene reset complete");

		// Clear selection as entity pointers are now invalid
		ClearSelection();

		// Clear game camera reference as it now points to deleted memory
		s_pxGameCameraEntity = nullptr;

		// Reset editor camera to initial state
		s_xEditorCameraPosition = xINITIAL_EDITOR_CAMERA_POSITION;
		s_fEditorCameraPitch = xINITIAL_EDITOR_CAMERA_PITCH;
		s_fEditorCameraYaw = xINITIAL_EDITOR_CAMERA_YAW;
		s_fEditorCameraFOV = xINITIAL_EDITOR_CAMERA_FOV;
		s_fEditorCameraNear = xINITIAL_EDITOR_CAMERA_NEAR;
		s_fEditorCameraFar = xINITIAL_EDITOR_CAMERA_FAR;

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
			Zenith_Log("Scene saved to %s", s_strPendingSceneSavePath.c_str());
		}
		catch (const std::exception& e)
		{
			Zenith_Log("Failed to save scene: %s", e.what());
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
		Zenith_Log("Waiting for all render tasks to complete before loading scene...");
		Zenith_Core::WaitForAllRenderTasks();  // CPU synchronization


		Zenith_Log("Waiting for GPU to become idle before loading scene...");
		Zenith_Vulkan::WaitForGPUIdle();  // GPU synchronization

		// Force process any pending deferred deletions to ensure old descriptors are destroyed
		// Without this, descriptor handles might collide between old/new scenes
		Zenith_Log("Processing deferred resource deletions...");
		for (u_int u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
		{
			Flux_MemoryManager::ProcessDeferredDeletions();
		}

		// CRITICAL: Clear any pending command lists before loading scene
		// This prevents stale command list entries from previous frames that may
		// contain pointers to resources that are about to be destroyed
		Zenith_Log("Clearing pending command lists...");
		Flux::ClearPendingCommandLists();

		// Safe to load now - no render tasks active, GPU idle, old resources deleted
		Zenith_Scene::GetCurrentScene().LoadFromFile(s_strPendingSceneLoadPath);
		Zenith_Log("Scene loaded from %s", s_strPendingSceneLoadPath.c_str());

		// Clear selection as entity pointers are now invalid
		ClearSelection();

		// Clear undo/redo history as entity IDs are now invalid
		Zenith_UndoSystem::Clear();

		// Clear game camera entity pointer as it's now invalid (entity from old scene)
		s_pxGameCameraEntity = nullptr;

		// If this was a backup scene restore (Play -> Stop transition), clean up
		if (s_bHasSceneBackup && s_strPendingSceneLoadPath == s_strBackupScenePath)
		{
			// Delete the temporary backup file
			std::filesystem::remove(s_strBackupScenePath);
			s_bHasSceneBackup = false;
			s_strBackupScenePath = "";
			Zenith_Log("Backup scene file cleaned up");

			// After restoring scene, initialize editor camera state from the game's camera
			if (s_bEditorCameraInitialized)
			{
				SwitchToEditorCamera();
				Zenith_Log("Editor camera state updated after scene restore");
			}
		}
		else
		{
			// For regular scene loads, also sync editor camera with the new scene's camera (if it has one)
			if (s_bEditorCameraInitialized)
			{
				SwitchToEditorCamera();
				Zenith_Log("Editor camera synced with loaded scene");
			}
		}

		s_strPendingSceneLoadPath.clear();

		return false;
	}

	// Process deferred descriptor set deletions
	// We wait N frames before freeing to ensure GPU has finished using them
	for (auto it = s_xPendingDeletions.begin(); it != s_xPendingDeletions.end(); )
	{
		if (it->framesUntilDeletion == 0)
		{
			// Safe to delete now - GPU has finished with this descriptor set
			ImGui_ImplVulkan_RemoveTexture(it->descriptorSet);
			it = s_xPendingDeletions.erase(it);
		}
		else
		{
			// Decrement frame counter
			it->framesUntilDeletion--;
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
		if (xScene.m_pxMainCameraEntity)
		{
			// Initialize editor camera from game camera position
			try
			{
				Zenith_CameraComponent& xGameCamera = xScene.m_pxMainCameraEntity->GetComponent<Zenith_CameraComponent>();
				xGameCamera.GetPosition(s_xEditorCameraPosition);
				s_fEditorCameraPitch = xGameCamera.GetPitch();
				s_fEditorCameraYaw = xGameCamera.GetYaw();

				// Save reference to game camera for later
				s_pxGameCameraEntity = xScene.m_pxMainCameraEntity;

				Zenith_Log("Editor camera synced from game camera at (%.1f, %.1f, %.1f)", 
					s_xEditorCameraPosition.x, s_xEditorCameraPosition.y, s_xEditorCameraPosition.z);
			}
			catch (...)
			{
				Zenith_Log("Could not sync editor camera from game camera");
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
				Zenith_Log("Scene reset queued (will reset next frame)");
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
					Zenith_Log("Scene load queued: %s (will load next frame)", s_strPendingSceneLoadPath.c_str());
				}
#else
				// Fallback for non-Windows platforms
				s_strPendingSceneLoadPath = "scene.zscen";
				s_bPendingSceneLoad = true;
				Zenith_Log("Scene load queued: %s (will load next frame)", s_strPendingSceneLoadPath.c_str());
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
					Zenith_Log("Scene save queued: %s (will save next frame)", s_strPendingSceneSavePath.c_str());
				}
#else
				// Fallback for non-Windows platforms
				s_strPendingSceneSavePath = "scene.zscen";
				s_bPendingSceneSave = true;
				Zenith_Log("Scene save queued: %s (will save next frame)", s_strPendingSceneSavePath.c_str());
#endif
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Exit"))
			{
				// TODO: Implement graceful shutdown
				// Would trigger application exit
				Zenith_Log("Exit - Not yet implemented");
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
				Zenith_Log("Toggle Hierarchy - Not yet implemented");
			}

			if (ImGui::MenuItem("Properties"))
			{
				// TODO: Toggle properties panel visibility
				Zenith_Log("Toggle Properties - Not yet implemented");
			}

			if (ImGui::MenuItem("Console"))
			{
				// TODO: Toggle console panel visibility
				Zenith_Log("Toggle Console - Not yet implemented");
			}

			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}
}

void Zenith_Editor::RenderToolbar()
{
	ImGui::Begin("Toolbar");
	
	// Play/Pause/Stop buttons
	const char* playText = (s_eEditorMode == EditorMode::Playing) ? "Pause" : "Play";
	if (ImGui::Button(playText))
	{
		if (s_eEditorMode == EditorMode::Stopped)
		{
			SetEditorMode(EditorMode::Playing);
		}
		else if (s_eEditorMode == EditorMode::Playing)
		{
			SetEditorMode(EditorMode::Paused);
		}
		else if (s_eEditorMode == EditorMode::Paused)
		{
			SetEditorMode(EditorMode::Playing);
		}
	}
	
	ImGui::SameLine();
	
	if (ImGui::Button("Stop"))
	{
		SetEditorMode(EditorMode::Stopped);
	}
	
	ImGui::Separator();
	
	// Gizmo mode buttons
	if (ImGui::RadioButton("Translate", s_eGizmoMode == EditorGizmoMode::Translate))
	{
		SetGizmoMode(EditorGizmoMode::Translate);
	}
	ImGui::SameLine();
	
	if (ImGui::RadioButton("Rotate", s_eGizmoMode == EditorGizmoMode::Rotate))
	{
		SetGizmoMode(EditorGizmoMode::Rotate);
	}
	ImGui::SameLine();
	
	if (ImGui::RadioButton("Scale", s_eGizmoMode == EditorGizmoMode::Scale))
	{
		SetGizmoMode(EditorGizmoMode::Scale);
	}
	
	ImGui::End();
}

void Zenith_Editor::RenderHierarchyPanel()
{
	ImGui::Begin("Hierarchy");

	ImGui::Text("Scene Entities:");
	ImGui::Separator();

	// Get reference to current scene
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Iterate through all entities in the scene
	for (auto& [entityID, entity] : xScene.m_xEntityMap)
	{
		// Check if this entity is currently selected
		bool bIsSelected = (s_uSelectedEntityID == entityID);

		// Create selectable item for entity
		// Use entity name if available, otherwise show ID
		std::string strDisplayName = entity.m_strName.empty() ?
			("Entity_" + std::to_string(entityID)) : entity.m_strName;

		// Count components on this entity
		uint32_t uComponentCount = 0;
		std::string strComponentSummary;
		Zenith_ComponentRegistry& xRegistry = Zenith_ComponentRegistry::Get();
		const auto& xEntries = xRegistry.GetEntries();
		for (const Zenith_ComponentRegistryEntry& xEntry : xEntries)
		{
			if (xEntry.m_fnHasComponent(entity))
			{
				if (uComponentCount > 0)
					strComponentSummary += ", ";
				strComponentSummary += xEntry.m_strDisplayName;
				uComponentCount++;
			}
		}

		// Add component count to display name
		if (uComponentCount > 0)
		{
			strDisplayName += " [" + std::to_string(uComponentCount) + "]";
		}

		// Add unique ID to avoid ImGui label collisions
		std::string strLabel = strDisplayName + "##" + std::to_string(entityID);

		if (ImGui::Selectable(strLabel.c_str(), bIsSelected))
		{
			// Select by EntityID for safer memory management
			SelectEntity(entityID);
		}

		// Show component list in tooltip on hover
		if (ImGui::IsItemHovered() && uComponentCount > 0)
		{
			ImGui::SetTooltip("Components: %s", strComponentSummary.c_str());
		}

		// Show context menu on right-click
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Delete Entity"))
			{
				// Clear selection if this entity is selected
				if (s_uSelectedEntityID == entityID)
				{
					ClearSelection();
				}
				// Remove entity from scene
				xScene.RemoveEntity(entityID);
			}
			ImGui::EndPopup();
		}
	}

	// Add button to create new entity
	ImGui::Separator();
	if (ImGui::Button("+ Create Entity"))
	{
		// Create new entity with default name and TransformComponent
		// The constructor handles adding to the scene and adding TransformComponent
		Zenith_Entity xNewEntity(&xScene, "New Entity");
		SelectEntity(xNewEntity.GetEntityID());
	}

	ImGui::End();
}

void Zenith_Editor::RenderPropertiesPanel()
{
	ImGui::Begin("Properties");
	
	Zenith_Entity* pxSelectedEntity = GetSelectedEntity();
	
	if (pxSelectedEntity)
	{
		// Entity name editing
		char nameBuffer[256];
		strncpy(nameBuffer, pxSelectedEntity->m_strName.c_str(), sizeof(nameBuffer));
		nameBuffer[sizeof(nameBuffer) - 1] = '\0';
		if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
		{
			pxSelectedEntity->m_strName = nameBuffer;
		}
		
		ImGui::Separator();
		
		//----------------------------------------------------------------------
		// Component Properties Section
		//----------------------------------------------------------------------
		// Iterate over all registered components and render their properties
		// if the selected entity has that component type.
		// This replaces the previous manual component-by-component checks.
		//----------------------------------------------------------------------
		Zenith_ComponentRegistry& xRegistry = Zenith_ComponentRegistry::Get();
		const auto& xEntries = xRegistry.GetEntries();
		
		for (const Zenith_ComponentRegistryEntry& xEntry : xEntries)
		{
			// Check if entity has this component and render its properties panel
			if (xEntry.m_fnHasComponent(*pxSelectedEntity))
			{
				xEntry.m_fnRenderPropertiesPanel(*pxSelectedEntity);
			}
		}

		//----------------------------------------------------------------------
		// Add Component Section
		//----------------------------------------------------------------------
		ImGui::Separator();
		ImGui::Spacing();
		
		// Center the button
		float fButtonWidth = 200.0f;
		float fWindowWidth = ImGui::GetWindowWidth();
		ImGui::SetCursorPosX((fWindowWidth - fButtonWidth) * 0.5f);
		
		if (ImGui::Button("Add Component", ImVec2(fButtonWidth, 0)))
		{
			ImGui::OpenPopup("AddComponentPopup");
			Zenith_Log("[Editor] Add Component button clicked for Entity %u", s_uSelectedEntityID);
		}
		
		// Add Component popup menu
		if (ImGui::BeginPopup("AddComponentPopup"))
		{
			bool bAnyAvailable = false;
			
			for (size_t i = 0; i < xEntries.size(); ++i)
			{
				const Zenith_ComponentRegistryEntry& xEntry = xEntries[i];
				
				// Check if entity already has this component
				bool bHasComponent = xRegistry.EntityHasComponent(i, *pxSelectedEntity);
				
				if (bHasComponent)
				{
					// Show disabled/grayed out for components the entity already has
					ImGui::BeginDisabled();
					ImGui::MenuItem(xEntry.m_strDisplayName.c_str(), nullptr, false, false);
					ImGui::EndDisabled();
				}
				else
				{
					bAnyAvailable = true;
					if (ImGui::MenuItem(xEntry.m_strDisplayName.c_str()))
					{
						Zenith_Log("[Editor] User selected to add component: %s to Entity %u",
							xEntry.m_strDisplayName.c_str(), s_uSelectedEntityID);
						
						// Add the component through the registry
						bool bSuccess = xRegistry.TryAddComponent(i, *pxSelectedEntity);
						
						if (bSuccess)
						{
							Zenith_Log("[Editor] Successfully added %s component to Entity %u",
								xEntry.m_strDisplayName.c_str(), s_uSelectedEntityID);
						}
						else
						{
							Zenith_Log("[Editor] ERROR: Failed to add %s component to Entity %u",
								xEntry.m_strDisplayName.c_str(), s_uSelectedEntityID);
						}
					}
				}
			}
			
			// If all components are already added, show a message
			if (!bAnyAvailable)
			{
				ImGui::TextDisabled("All available components already added");
			}
			
			ImGui::EndPopup();
		}
	}
	else
	{
		ImGui::Text("No entity selected");
	}
	
	ImGui::End();
}

void Zenith_Editor::RenderViewport()
{
	ImGui::Begin("Viewport");

	// Track viewport position for mouse picking
	ImVec2 xViewportPanelPos = ImGui::GetCursorScreenPos();
	s_xViewportPos = { xViewportPanelPos.x, xViewportPanelPos.y };

	// Get the final render target SRV
	Flux_ShaderResourceView& xGameRenderSRV = Flux_Graphics::s_xFinalRenderTarget.m_axColourAttachments[0].m_pxSRV;

	if (xGameRenderSRV.m_xImageView != VK_NULL_HANDLE)
	{
		// Check if the image view has changed (e.g., due to window resize)
		// Only allocate a new descriptor set if necessary to avoid exhausting the pool
		if (s_xCachedImageView != xGameRenderSRV.m_xImageView)
		{
			// Queue old descriptor set for deferred deletion
			// We can't free it immediately because the GPU may still be using it in in-flight command buffers
			// Vulkan spec requires waiting for all commands referencing the descriptor set to complete
			if (s_xCachedGameTextureDescriptorSet != VK_NULL_HANDLE)
			{
				// Wait 3 frames before deletion to ensure GPU has finished
				// This accounts for frames in flight (typically 2-3 frames buffered)
				constexpr u_int FRAMES_TO_WAIT = 3;
				s_xPendingDeletions.push_back({
					s_xCachedGameTextureDescriptorSet,
					FRAMES_TO_WAIT
				});
			}

			// Allocate new descriptor set for the game viewport texture
			s_xCachedGameTextureDescriptorSet = ImGui_ImplVulkan_AddTexture(
				Flux_Graphics::s_xRepeatSampler.GetSampler(),
				xGameRenderSRV.m_xImageView,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			);

			// Cache the image view so we know when it changes
			s_xCachedImageView = xGameRenderSRV.m_xImageView;
		}

		// Get available content region size
		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();

		// Store viewport size for object picking
		s_xViewportSize = { viewportPanelSize.x, viewportPanelSize.y };

		// Track viewport hover/focus state for input handling
		s_bViewportHovered = ImGui::IsWindowHovered();
		s_bViewportFocused = ImGui::IsWindowFocused();

		// Display the game render target as an image using the cached descriptor set
		if (s_xCachedGameTextureDescriptorSet != VK_NULL_HANDLE)
		{
			ImGui::Image((ImTextureID)(uintptr_t)static_cast<VkDescriptorSet>(s_xCachedGameTextureDescriptorSet), viewportPanelSize);
		}
		else
		{
			ImGui::Text("Viewport texture not yet initialized");
		}
	}
	else
	{
		ImGui::Text("Game render target not available");
	}

	ImGui::End();
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
	if (!s_bViewportHovered || s_uSelectedEntityID == INVALID_ENTITY_ID)
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
			Zenith_Log("Mouse: Global=(%.1f,%.1f), Viewport=(%.1f,%.1f)",
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
		Zenith_Log("Mouse left pressed - viewport hovered=%d, selected=%d", s_bViewportHovered, s_uSelectedEntityID);
		Flux_Gizmos::BeginInteraction(xRayOrigin, xRayDir);
		Zenith_Log("After BeginInteraction: IsInteracting=%d", Flux_Gizmos::IsInteracting());
	}
	
	// Update interaction while dragging (can happen same frame as BeginInteraction)
	bool bIsKeyDown = Zenith_Input::IsKeyDown(ZENITH_MOUSE_BUTTON_LEFT);
	bool bIsInteracting = Flux_Gizmos::IsInteracting();
	
	if (bIsKeyDown || bIsInteracting)
	{
		Zenith_Log("Check UpdateInteraction: IsKeyDown=%d, IsInteracting=%d", bIsKeyDown, bIsInteracting);
	}
	
	if (bIsKeyDown && bIsInteracting)
	{
		Zenith_Log("Calling UpdateInteraction: ViewportMouse=(%.1f,%.1f)",
			xViewportMousePos.x, xViewportMousePos.y);
		Flux_Gizmos::UpdateInteraction(xRayOrigin, xRayDir);
	}
	
	// End interaction when mouse released
	if (!Zenith_Input::IsKeyDown(ZENITH_MOUSE_BUTTON_LEFT) && Flux_Gizmos::IsInteracting())
	{
		Zenith_Log("Ending interaction");
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
		Zenith_Log("Editor: Entering Play Mode");

		// Generate backup file path in temp directory
		s_strBackupScenePath = std::filesystem::temp_directory_path().string() + "/zenith_scene_backup.zscen";

		// Save current scene state to backup file
		// This is safe to do synchronously since we're only saving (not destroying/creating)
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		xScene.SaveToFile(s_strBackupScenePath);
		s_bHasSceneBackup = true;

		Zenith_Log("Scene state backed up to: %s", s_strBackupScenePath.c_str());

		// Check if a main camera has already been set via the editor
		s_pxGameCameraEntity = xScene.GetMainCameraEntity();

		// If no main camera is set, search for one
		if (!s_pxGameCameraEntity)
		{
			Zenith_Vector<Zenith_CameraComponent*> xCameras;
			xScene.GetAllOfComponentType<Zenith_CameraComponent>(xCameras);

			for (Zenith_Vector<Zenith_CameraComponent*>::Iterator xIt(xCameras); !xIt.Done(); xIt.Next())
			{
				Zenith_CameraComponent* pxCam = xIt.GetData();
				Zenith_Entity* pxEntity = &pxCam->GetParentEntity();
				// Just use the first camera we find (there should only be one game camera)
				s_pxGameCameraEntity = pxEntity;
				xScene.SetMainCameraEntity(*s_pxGameCameraEntity);
				Zenith_Log("No main camera set, using first camera found: %s", s_pxGameCameraEntity->m_strName.c_str());
				break;
			}
		}
		else
		{
			Zenith_Log("Using existing main camera: %s", s_pxGameCameraEntity->m_strName.c_str());
		}

		if (!s_pxGameCameraEntity)
		{
			Zenith_Log("Warning: No game camera found, staying on editor camera");
		}
	}

	// PLAYING/PAUSED -> STOPPED: Restore scene state and switch to editor camera
	else if (oldMode != EditorMode::Stopped && eMode == EditorMode::Stopped)
	{
		Zenith_Log("Editor: Stopping Play Mode");

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

			Zenith_Log("Scene restore queued for next frame: %s", s_strBackupScenePath.c_str());

			// Note: We don't clear s_bHasSceneBackup or s_strBackupScenePath here
			// They'll be cleared in Update() after the load completes
		}
		else
		{
			Zenith_Log("Warning: No scene backup available to restore");
		}

		// Legacy cleanup
		if (s_pxBackupScene)
		{
			delete s_pxBackupScene;
			s_pxBackupScene = nullptr;
		}

		// Clear the game camera reference since scene will be reloaded
		s_pxGameCameraEntity = nullptr;
	}

	// PAUSED state - suspend scene updates but stay on game camera
	else if (eMode == EditorMode::Paused)
	{
		Zenith_Log("Editor: Pausing - physics and scene updates suspended");
		// Stay on game camera during pause so player can see game state
	}

	// PAUSED -> PLAYING: Resume scene updates
	else if (oldMode == EditorMode::Paused && eMode == EditorMode::Playing)
	{
		Zenith_Log("Editor: Resuming - physics and scene updates resumed");
	}
}

void Zenith_Editor::SelectEntity(Zenith_EntityID uEntityID)
{
	s_uSelectedEntityID = uEntityID;
	
	if (uEntityID != INVALID_ENTITY_ID)
	{
		Zenith_Log("Editor: Selected entity %u", uEntityID);
		
		// Update Flux_Gizmos target entity
		Zenith_Entity* pxEntity = GetSelectedEntity();
		if (pxEntity)
		{
			Flux_Gizmos::SetTargetEntity(pxEntity);
		}
	}
}

void Zenith_Editor::ClearSelection()
{
	s_uSelectedEntityID = INVALID_ENTITY_ID;
	Flux_Gizmos::SetTargetEntity(nullptr);
}

Zenith_Entity* Zenith_Editor::GetSelectedEntity()
{
	if (s_uSelectedEntityID == INVALID_ENTITY_ID)
		return nullptr;

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Check if entity still exists in the scene
	auto it = xScene.m_xEntityMap.find(s_uSelectedEntityID);
	if (it == xScene.m_xEntityMap.end())
	{
		// Entity no longer exists - clear selection
		s_uSelectedEntityID = INVALID_ENTITY_ID;
		return nullptr;
	}

	return &it->second;
}

//------------------------------------------------------------------------------
// Content Browser Implementation
//------------------------------------------------------------------------------

void Zenith_Editor::RenderContentBrowser()
{
	ImGui::Begin("Content Browser");

	// Refresh directory contents if needed
	if (s_bDirectoryNeedsRefresh)
	{
		RefreshDirectoryContents();
		s_bDirectoryNeedsRefresh = false;
	}

	// Navigation buttons
	if (ImGui::Button("<- Back"))
	{
		NavigateToParent();
	}
	ImGui::SameLine();
	if (ImGui::Button("Refresh"))
	{
		s_bDirectoryNeedsRefresh = true;
	}
	ImGui::SameLine();

	// Display current path (truncated if too long)
	std::string strDisplayPath = s_strCurrentDirectory;
	if (strDisplayPath.length() > 50)
	{
		strDisplayPath = "..." + strDisplayPath.substr(strDisplayPath.length() - 47);
	}
	ImGui::Text("Path: %s", strDisplayPath.c_str());

	ImGui::Separator();

	// Display directory contents in a table/grid
	float fPanelWidth = ImGui::GetContentRegionAvail().x;
	float fCellSize = 80.0f;  // Size of each item cell
	int iColumnCount = std::max(1, (int)(fPanelWidth / fCellSize));

	if (ImGui::BeginTable("ContentBrowserTable", iColumnCount))
	{
		for (size_t i = 0; i < s_xDirectoryContents.size(); ++i)
		{
			const ContentBrowserEntry& xEntry = s_xDirectoryContents[i];

			ImGui::TableNextColumn();
			ImGui::PushID((int)i);

			// Icon representation (using text for now)
			const char* szIcon = xEntry.m_bIsDirectory ? "[DIR]" : "[FILE]";

			ImGui::BeginGroup();

			if (xEntry.m_bIsDirectory)
			{
				// Directory - click to enter
				if (ImGui::Button(szIcon, ImVec2(fCellSize - 10, fCellSize - 30)))
				{
					NavigateToDirectory(xEntry.m_strFullPath);
				}
			}
			else
			{
				// File - can be dragged
				ImGui::Button(szIcon, ImVec2(fCellSize - 10, fCellSize - 30));

				// Drag source for files
				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
				{
					DragDropFilePayload xPayload;
					strncpy(xPayload.m_szFilePath, xEntry.m_strFullPath.c_str(),
						sizeof(xPayload.m_szFilePath) - 1);
					xPayload.m_szFilePath[sizeof(xPayload.m_szFilePath) - 1] = '\0';

					// Determine payload type based on extension
					const char* szPayloadType = DRAGDROP_PAYLOAD_FILE_GENERIC;
					if (xEntry.m_strExtension == ZENITH_TEXTURE_EXT)
					{
						szPayloadType = DRAGDROP_PAYLOAD_TEXTURE;
					}
					else if (xEntry.m_strExtension == ZENITH_MESH_EXT)
					{
						szPayloadType = DRAGDROP_PAYLOAD_MESH;
					}
					else if (xEntry.m_strExtension == ZENITH_MATERIAL_EXT)
					{
						szPayloadType = DRAGDROP_PAYLOAD_MATERIAL;
					}

					ImGui::SetDragDropPayload(szPayloadType, &xPayload, sizeof(xPayload));

					// Drag preview tooltip
					ImGui::Text("Drag: %s", xEntry.m_strName.c_str());

					ImGui::EndDragDropSource();

					Zenith_Log("[ContentBrowser] Started dragging: %s", xEntry.m_strName.c_str());
				}
				
				// Double-click to open material files in editor
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
				{
					if (xEntry.m_strExtension == ZENITH_MATERIAL_EXT)
					{
						Flux_MaterialAsset* pMaterial = Flux_MaterialAsset::LoadFromFile(xEntry.m_strFullPath);
						if (pMaterial)
						{
							SelectMaterial(pMaterial);
						}
					}
				}
			}

			// Display truncated filename below icon
			std::string strDisplayName = xEntry.m_strName;
			if (strDisplayName.length() > 10)
			{
				strDisplayName = strDisplayName.substr(0, 7) + "...";
			}
			ImGui::TextWrapped("%s", strDisplayName.c_str());

			// Tooltip with full filename
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", xEntry.m_strName.c_str());
			}

			ImGui::EndGroup();
			ImGui::PopID();
		}

		ImGui::EndTable();
	}

	ImGui::End();
}

void Zenith_Editor::RefreshDirectoryContents()
{
	s_xDirectoryContents.clear();

	try
	{
		for (const auto& xEntry : std::filesystem::directory_iterator(s_strCurrentDirectory))
		{
			ContentBrowserEntry xBrowserEntry;
			xBrowserEntry.m_strFullPath = xEntry.path().string();
			xBrowserEntry.m_strName = xEntry.path().filename().string();
			xBrowserEntry.m_strExtension = xEntry.path().extension().string();
			xBrowserEntry.m_bIsDirectory = xEntry.is_directory();

			s_xDirectoryContents.push_back(xBrowserEntry);
		}

		// Sort: directories first, then files, alphabetically within each group
		std::sort(s_xDirectoryContents.begin(), s_xDirectoryContents.end(),
			[](const ContentBrowserEntry& a, const ContentBrowserEntry& b) {
				if (a.m_bIsDirectory != b.m_bIsDirectory)
					return a.m_bIsDirectory > b.m_bIsDirectory;
				return a.m_strName < b.m_strName;
			});

		Zenith_Log("[ContentBrowser] Refreshed directory: %s (%zu items)",
			s_strCurrentDirectory.c_str(), s_xDirectoryContents.size());
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		Zenith_Log("[ContentBrowser] Error reading directory: %s", e.what());
	}
}

void Zenith_Editor::NavigateToDirectory(const std::string& strPath)
{
	s_strCurrentDirectory = strPath;
	s_bDirectoryNeedsRefresh = true;
	Zenith_Log("[ContentBrowser] Navigated to: %s", strPath.c_str());
}

void Zenith_Editor::NavigateToParent()
{
	std::filesystem::path xPath(s_strCurrentDirectory);
	std::filesystem::path xParent = xPath.parent_path();

	// Don't navigate above ASSETS_ROOT
	std::string strAssetsRoot = ASSETS_ROOT;
	// Remove trailing slash if present for comparison
	if (!strAssetsRoot.empty() && (strAssetsRoot.back() == '/' || strAssetsRoot.back() == '\\'))
	{
		strAssetsRoot.pop_back();
	}

	if (xParent.string().length() >= strAssetsRoot.length())
	{
		s_strCurrentDirectory = xParent.string();
		s_bDirectoryNeedsRefresh = true;
		Zenith_Log("[ContentBrowser] Navigated to parent: %s", s_strCurrentDirectory.c_str());
	}
	else
	{
		Zenith_Log("[ContentBrowser] Already at root directory");
	}
}

//------------------------------------------------------------------------------
// Console Implementation
//------------------------------------------------------------------------------

void Zenith_Editor::AddLogMessage(const char* szMessage, ConsoleLogEntry::LogLevel eLevel)
{
	ConsoleLogEntry xEntry;
	xEntry.m_eLevel = eLevel;
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
	ImGui::Begin("Console");

	// Toolbar
	if (ImGui::Button("Clear"))
	{
		ClearConsole();
	}
	ImGui::SameLine();
	ImGui::Checkbox("Auto-scroll", &s_bConsoleAutoScroll);
	ImGui::SameLine();
	ImGui::Separator();
	ImGui::SameLine();

	// Filter checkboxes
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
	ImGui::Checkbox("Info", &s_bShowConsoleInfo);
	ImGui::PopStyleColor();
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
	ImGui::Checkbox("Warnings", &s_bShowConsoleWarnings);
	ImGui::PopStyleColor();
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
	ImGui::Checkbox("Errors", &s_bShowConsoleErrors);
	ImGui::PopStyleColor();

	ImGui::Separator();

	// Log entries
	ImGui::BeginChild("ConsoleScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

	for (const auto& xEntry : s_xConsoleLogs)
	{
		// Filter by log level
		bool bShow = false;
		ImVec4 xColor;
		switch (xEntry.m_eLevel)
		{
		case ConsoleLogEntry::LogLevel::Info:
			bShow = s_bShowConsoleInfo;
			xColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
			break;
		case ConsoleLogEntry::LogLevel::Warning:
			bShow = s_bShowConsoleWarnings;
			xColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
			break;
		case ConsoleLogEntry::LogLevel::Error:
			bShow = s_bShowConsoleErrors;
			xColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
			break;
		}

		if (bShow)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, xColor);
			ImGui::TextUnformatted(("[" + xEntry.m_strTimestamp + "] " + xEntry.m_strMessage).c_str());
			ImGui::PopStyleColor();
		}
	}

	// Auto-scroll to bottom
	if (s_bConsoleAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
	{
		ImGui::SetScrollHereY(1.0f);
	}

	ImGui::EndChild();
	ImGui::End();
}

//------------------------------------------------------------------------------
// Material Editor Implementation
//------------------------------------------------------------------------------

void Zenith_Editor::SelectMaterial(Flux_MaterialAsset* pMaterial)
{
	s_pxSelectedMaterial = pMaterial;
	s_bShowMaterialEditor = true;
	if (pMaterial)
	{
		Zenith_Log("[Editor] Selected material: %s", pMaterial->GetName().c_str());
	}
}

void Zenith_Editor::ClearMaterialSelection()
{
	s_pxSelectedMaterial = nullptr;
}

void Zenith_Editor::RenderMaterialEditorPanel()
{
	if (!s_bShowMaterialEditor)
		return;
		
	ImGui::Begin("Material Editor", &s_bShowMaterialEditor);
	
	// Create New Material button
	if (ImGui::Button("Create New Material"))
	{
		Flux_MaterialAsset* pNewMaterial = Flux_MaterialAsset::Create();
		SelectMaterial(pNewMaterial);
		Zenith_Log("[MaterialEditor] Created new material: %s", pNewMaterial->GetName().c_str());
	}
	
	ImGui::SameLine();
	
	// Load Material button
	if (ImGui::Button("Load Material"))
	{
#ifdef _WIN32
		std::string strFilePath = ShowOpenFileDialog(
			"Zenith Material Files (*" ZENITH_MATERIAL_EXT ")\0 * " ZENITH_MATERIAL_EXT "\0All Files (*.*)\0 * .*\0",
			ZENITH_MATERIAL_EXT);
		if (!strFilePath.empty())
		{
			Flux_MaterialAsset* pMaterial = Flux_MaterialAsset::LoadFromFile(strFilePath);
			if (pMaterial)
			{
				SelectMaterial(pMaterial);
				Zenith_Log("[MaterialEditor] Loaded material: %s", strFilePath.c_str());
			}
			else
			{
				Zenith_Log("[MaterialEditor] ERROR: Failed to load material: %s", strFilePath.c_str());
			}
		}
#endif
	}
	
	ImGui::Separator();

	// Display ALL materials (both file-cached and runtime-created)
	std::vector<Flux_MaterialAsset*> allMaterials;
	Flux_MaterialAsset::GetAllMaterials(allMaterials);

	if (ImGui::CollapsingHeader("All Materials", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Total: %zu materials", allMaterials.size());
		ImGui::Separator();

		for (Flux_MaterialAsset* pMat : allMaterials)
		{
			if (pMat)
			{
				bool bIsSelected = (s_pxSelectedMaterial == pMat);
				std::string strDisplayName = pMat->GetName();
				if (pMat->IsDirty())
				{
					strDisplayName += " *";  // Unsaved changes indicator
				}

				// Show file path indicator for saved materials
				if (!pMat->GetAssetPath().empty())
				{
					strDisplayName += " [saved]";
				}

				if (ImGui::Selectable(strDisplayName.c_str(), bIsSelected))
				{
					SelectMaterial(pMat);
				}

				// Tooltip with more details
				if (ImGui::IsItemHovered())
				{
					std::string strTooltip = "Name: " + pMat->GetName();
					if (!pMat->GetAssetPath().empty())
					{
						strTooltip += "\nPath: " + pMat->GetAssetPath();
					}
					else
					{
						strTooltip += "\n(Runtime-created, not saved to file)";
					}
					ImGui::SetTooltip("%s", strTooltip.c_str());
				}
			}
		}

		if (allMaterials.empty())
		{
			ImGui::TextDisabled("No materials loaded");
		}
	}
	
	ImGui::Separator();
	
	// Material properties editor
	if (s_pxSelectedMaterial)
	{
		Flux_MaterialAsset* pMat = s_pxSelectedMaterial;
		
		ImGui::Text("Editing: %s", pMat->GetName().c_str());
		
		if (!pMat->GetAssetPath().empty())
		{
			ImGui::TextDisabled("Path: %s", pMat->GetAssetPath().c_str());
		}
		else
		{
			ImGui::TextDisabled("(Unsaved)");
		}
		
		ImGui::Separator();
		
		// Name
		char szNameBuffer[256];
		strncpy(szNameBuffer, pMat->GetName().c_str(), sizeof(szNameBuffer));
		szNameBuffer[sizeof(szNameBuffer) - 1] = '\0';
		if (ImGui::InputText("Name", szNameBuffer, sizeof(szNameBuffer)))
		{
			pMat->SetName(szNameBuffer);
		}
		
		ImGui::Separator();
		ImGui::Text("Material Properties");
		
		// Base Color
		Zenith_Maths::Vector4 xBaseColor = pMat->GetBaseColor();
		float fColor[4] = { xBaseColor.x, xBaseColor.y, xBaseColor.z, xBaseColor.w };
		if (ImGui::ColorEdit4("Base Color", fColor))
		{
			pMat->SetBaseColor({ fColor[0], fColor[1], fColor[2], fColor[3] });
		}
		
		// Metallic
		float fMetallic = pMat->GetMetallic();
		if (ImGui::SliderFloat("Metallic", &fMetallic, 0.0f, 1.0f))
		{
			pMat->SetMetallic(fMetallic);
		}
		
		// Roughness
		float fRoughness = pMat->GetRoughness();
		if (ImGui::SliderFloat("Roughness", &fRoughness, 0.0f, 1.0f))
		{
			pMat->SetRoughness(fRoughness);
		}
		
		// Emissive
		Zenith_Maths::Vector3 xEmissive = pMat->GetEmissiveColor();
		float fEmissiveColor[3] = { xEmissive.x, xEmissive.y, xEmissive.z };
		if (ImGui::ColorEdit3("Emissive Color", fEmissiveColor))
		{
			pMat->SetEmissiveColor({ fEmissiveColor[0], fEmissiveColor[1], fEmissiveColor[2] });
		}
		
		float fEmissiveIntensity = pMat->GetEmissiveIntensity();
		if (ImGui::SliderFloat("Emissive Intensity", &fEmissiveIntensity, 0.0f, 10.0f))
		{
			pMat->SetEmissiveIntensity(fEmissiveIntensity);
		}
		
		// Transparency
		bool bTransparent = pMat->IsTransparent();
		if (ImGui::Checkbox("Transparent", &bTransparent))
		{
			pMat->SetTransparent(bTransparent);
		}
		
		if (bTransparent)
		{
			float fAlphaCutoff = pMat->GetAlphaCutoff();
			if (ImGui::SliderFloat("Alpha Cutoff", &fAlphaCutoff, 0.0f, 1.0f))
			{
				pMat->SetAlphaCutoff(fAlphaCutoff);
			}
		}
		
		ImGui::Separator();
		ImGui::Text("Textures");
		
		// Texture slots with drag-drop support
		RenderMaterialTextureSlot("Diffuse", pMat, pMat->GetDiffuseTexturePath(),
			[](Flux_MaterialAsset* p, const std::string& s) { p->SetDiffuseTexturePath(s); });
		RenderMaterialTextureSlot("Normal", pMat, pMat->GetNormalTexturePath(),
			[](Flux_MaterialAsset* p, const std::string& s) { p->SetNormalTexturePath(s); });
		RenderMaterialTextureSlot("Roughness/Metallic", pMat, pMat->GetRoughnessMetallicTexturePath(),
			[](Flux_MaterialAsset* p, const std::string& s) { p->SetRoughnessMetallicTexturePath(s); });
		RenderMaterialTextureSlot("Occlusion", pMat, pMat->GetOcclusionTexturePath(),
			[](Flux_MaterialAsset* p, const std::string& s) { p->SetOcclusionTexturePath(s); });
		RenderMaterialTextureSlot("Emissive", pMat, pMat->GetEmissiveTexturePath(),
			[](Flux_MaterialAsset* p, const std::string& s) { p->SetEmissiveTexturePath(s); });
		
		ImGui::Separator();
		
		// Save button
		if (ImGui::Button("Save Material"))
		{
			if (pMat->GetAssetPath().empty())
			{
				// Show save dialog for new material
#ifdef _WIN32
				std::string strFilePath = ShowSaveFileDialog(
					"Zenith Material Files (*" ZENITH_MATERIAL_EXT ")\0 * " ZENITH_MATERIAL_EXT "\0All Files (*.*)\0 * .*\0",
					ZENITH_MATERIAL_EXT,
					(pMat->GetName() + ZENITH_MATERIAL_EXT).c_str());
				if (!strFilePath.empty())
				{
					if (pMat->SaveToFile(strFilePath))
					{
						Zenith_Log("[MaterialEditor] Saved material to: %s", strFilePath.c_str());
					}
				}
#endif
			}
			else
			{
				// Save to existing path
				if (pMat->SaveToFile(pMat->GetAssetPath()))
				{
					Zenith_Log("[MaterialEditor] Saved material: %s", pMat->GetAssetPath().c_str());
				}
			}
		}
		
		ImGui::SameLine();
		
		if (ImGui::Button("Save As..."))
		{
#ifdef _WIN32
			std::string strFilePath = ShowSaveFileDialog(
				"Zenith Material Files (*" ZENITH_MATERIAL_EXT ")\0 * " ZENITH_MATERIAL_EXT "\0All Files (*.*)\0 * .*\0",
				ZENITH_MATERIAL_EXT,
				(pMat->GetName() + ZENITH_MATERIAL_EXT).c_str());
			if (!strFilePath.empty())
			{
				if (pMat->SaveToFile(strFilePath))
				{
					Zenith_Log("[MaterialEditor] Saved material to: %s", strFilePath.c_str());
				}
			}
#endif
		}
		
		ImGui::SameLine();
		
		if (ImGui::Button("Reload"))
		{
			pMat->Reload();
			Zenith_Log("[MaterialEditor] Reloaded material: %s", pMat->GetName().c_str());
		}
	}
	else
	{
		ImGui::TextDisabled("No material selected");
		ImGui::TextDisabled("Create a new material or load an existing one");
	}
	
	ImGui::End();
}

void Zenith_Editor::RenderMaterialTextureSlot(const char* szLabel, Flux_MaterialAsset* pMaterial,
	const std::string& strCurrentPath,
	void (*SetPathFunc)(Flux_MaterialAsset*, const std::string&))
{
	ImGui::PushID(szLabel);
	
	std::string strDisplayName = "(none)";
	if (!strCurrentPath.empty())
	{
		std::filesystem::path xPath(strCurrentPath);
		strDisplayName = xPath.filename().string();
	}
	
	ImGui::Text("%s:", szLabel);
	ImGui::SameLine();
	
	// Drop zone button
	ImVec2 xButtonSize(200, 20);
	ImGui::Button(strDisplayName.c_str(), xButtonSize);
	
	// Drag-drop target
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_TEXTURE))
		{
			const DragDropFilePayload* pFilePayload =
				static_cast<const DragDropFilePayload*>(pPayload->Data);
			
			SetPathFunc(pMaterial, pFilePayload->m_szFilePath);
			Zenith_Log("[MaterialEditor] Set %s texture: %s", szLabel, pFilePayload->m_szFilePath);
		}
		ImGui::EndDragDropTarget();
	}
	
	// Tooltip
	if (ImGui::IsItemHovered())
	{
		if (!strCurrentPath.empty())
		{
			ImGui::SetTooltip("Path: %s\nDrop a .ztxtr texture here to change", strCurrentPath.c_str());
		}
		else
		{
			ImGui::SetTooltip("Drop a .ztxtr texture here");
		}
	}
	
	// Clear button
	if (!strCurrentPath.empty())
	{
		ImGui::SameLine();
		if (ImGui::SmallButton("X"))
		{
			SetPathFunc(pMaterial, "");
			Zenith_Log("[MaterialEditor] Cleared %s texture", szLabel);
		}
	}
	
	ImGui::PopID();
}

//------------------------------------------------------------------------------
// Editor Camera System
//------------------------------------------------------------------------------

void Zenith_Editor::InitializeEditorCamera()
{
	if (s_bEditorCameraInitialized)
		return;

	// Initialize editor camera from scene's main camera if available
	// Otherwise use default values
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	if (xScene.m_pxMainCameraEntity)
	{
		try
		{
			Zenith_CameraComponent& xSceneCamera = xScene.m_pxMainCameraEntity->GetComponent<Zenith_CameraComponent>();
			xSceneCamera.GetPosition(s_xEditorCameraPosition);
			s_fEditorCameraPitch = xSceneCamera.GetPitch();
			s_fEditorCameraYaw = xSceneCamera.GetYaw();
			s_fEditorCameraFOV = xSceneCamera.GetFOV();
			s_fEditorCameraNear = xSceneCamera.GetNearPlane();
			s_fEditorCameraFar = xSceneCamera.GetFarPlane();
			Zenith_Log("Editor camera initialized from scene camera position");
		}
		catch (...)
		{
			Zenith_Log("Scene camera not available, using default position");
		}
	}

	s_bEditorCameraInitialized = true;
	Zenith_Log("Editor camera initialized at position (%.1f, %.1f, %.1f)", s_xEditorCameraPosition.x, s_xEditorCameraPosition.y, s_xEditorCameraPosition.z);
}

void Zenith_Editor::UpdateEditorCamera(float fDt)
{
	if (!s_bEditorCameraInitialized)
		return;

	// Only update editor camera when in Stopped or Paused mode and viewport is focused
	if (s_eEditorMode == EditorMode::Playing)
		return;

	if (!s_bViewportFocused)
		return;

	// Mouse look (Right click key held for camera rotation)
	if (Zenith_Input::IsKeyDown(ZENITH_MOUSE_BUTTON_2))
	{
		Zenith_Maths::Vector2_64 xMouseDelta;
		Zenith_Input::GetMouseDelta(xMouseDelta);

		// Update yaw and pitch (values are stored in radians, matching camera component)
		// Convert rotate speed from degrees to radians for consistency
		const double fRotateSpeedRad = glm::radians(s_fEditorCameraRotateSpeed);
		s_fEditorCameraYaw -= xMouseDelta.x * fRotateSpeedRad;
		s_fEditorCameraPitch -= xMouseDelta.y * fRotateSpeedRad;

		// Clamp pitch to prevent flipping (use radians like PlayerController_Behaviour)
		s_fEditorCameraPitch = std::min(s_fEditorCameraPitch, glm::pi<double>() / 2.0);
		s_fEditorCameraPitch = std::max(s_fEditorCameraPitch, -glm::pi<double>() / 2.0);

		// Wrap yaw around 0 to 2π (like PlayerController_Behaviour)
		if (s_fEditorCameraYaw < 0.0)
		{
			s_fEditorCameraYaw += Zenith_Maths::Pi * 2.0;
		}
		if (s_fEditorCameraYaw > Zenith_Maths::Pi * 2.0)
		{
			s_fEditorCameraYaw -= Zenith_Maths::Pi * 2.0;
		}
		// Yaw is already in radians, no conversion needed
	}

	// Speed modifier (shift = faster)
	float fMoveSpeed = s_fEditorCameraMoveSpeed;
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_LEFT_SHIFT))
		fMoveSpeed *= 3.0f;

	// WASD movement (only when right click is held for FPS-style control)
	// Movement uses only yaw (not pitch) to keep movement on horizontal plane
	// This matches PlayerController_Behaviour behavior
	if (Zenith_Input::IsKeyDown(ZENITH_MOUSE_BUTTON_2))
	{
		const double fYawRad = glm::radians(s_fEditorCameraYaw);
		
		if (Zenith_Input::IsKeyDown(ZENITH_KEY_W))
		{
			// Forward movement based on yaw only (stays level)
			Zenith_Maths::Matrix4_64 xRotation = glm::rotate(-s_fEditorCameraYaw, Zenith_Maths::Vector3_64(0, 1, 0));
			Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(0, 0, 1, 1);
			s_xEditorCameraPosition += Zenith_Maths::Vector3(xResult) * fMoveSpeed * fDt;
		}
		if (Zenith_Input::IsKeyDown(ZENITH_KEY_S))
		{
			// Backward movement based on yaw only (stays level)
			Zenith_Maths::Matrix4_64 xRotation = glm::rotate(-s_fEditorCameraYaw, Zenith_Maths::Vector3_64(0, 1, 0));
			Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(0, 0, 1, 1);
			s_xEditorCameraPosition -= Zenith_Maths::Vector3(xResult) * fMoveSpeed * fDt;
		}
		if (Zenith_Input::IsKeyDown(ZENITH_KEY_A))
		{
			// Left strafe based on yaw only (stays level)
			Zenith_Maths::Matrix4_64 xRotation = glm::rotate(-s_fEditorCameraYaw, Zenith_Maths::Vector3_64(0, 1, 0));
			Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(-1, 0, 0, 1);
			s_xEditorCameraPosition += Zenith_Maths::Vector3(xResult) * fMoveSpeed * fDt;
		}
		if (Zenith_Input::IsKeyDown(ZENITH_KEY_D))
		{
			// Right strafe based on yaw only (stays level)
			Zenith_Maths::Matrix4_64 xRotation = glm::rotate(-s_fEditorCameraYaw, Zenith_Maths::Vector3_64(0, 1, 0));
			Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(-1, 0, 0, 1);
			s_xEditorCameraPosition -= Zenith_Maths::Vector3(xResult) * fMoveSpeed * fDt;
		}
		if (Zenith_Input::IsKeyDown(ZENITH_KEY_Q))
		{
			// Vertical down (world space)
			s_xEditorCameraPosition.y -= fMoveSpeed * fDt;
		}
		if (Zenith_Input::IsKeyDown(ZENITH_KEY_E))
		{
			// Vertical up (world space)
			s_xEditorCameraPosition.y += fMoveSpeed * fDt;
		}
	}

	// Apply editor camera state to the scene's main camera
	// (In stopped/paused mode, the game camera is being controlled by editor values)
	if (s_pxGameCameraEntity)
	{
		Zenith_CameraComponent& xCamera = s_pxGameCameraEntity->GetComponent<Zenith_CameraComponent>();
		xCamera.SetPosition(s_xEditorCameraPosition);
		xCamera.SetPitch(s_fEditorCameraPitch);
		xCamera.SetYaw(s_fEditorCameraYaw);
	}
}

void Zenith_Editor::SwitchToEditorCamera()
{
	if (!s_bEditorCameraInitialized)
	{
		Zenith_Log("Warning: Cannot switch to editor camera - not initialized");
		return;
	}

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Save the game's current main camera entity
	s_pxGameCameraEntity = xScene.m_pxMainCameraEntity;

	// Copy game camera state to editor camera
	if (s_pxGameCameraEntity)
	{
		try
		{
			Zenith_CameraComponent& xGameCamera = s_pxGameCameraEntity->GetComponent<Zenith_CameraComponent>();
			xGameCamera.GetPosition(s_xEditorCameraPosition);
			s_fEditorCameraPitch = xGameCamera.GetPitch();
			s_fEditorCameraYaw = xGameCamera.GetYaw();
		}
		catch (...)
		{
			Zenith_Log("Warning: Could not copy game camera state to editor camera");
		}
	}

	Zenith_Log("Switched to editor camera");
}

void Zenith_Editor::SwitchToGameCamera()
{
	if (!s_pxGameCameraEntity)
	{
		Zenith_Log("Warning: Cannot switch to game camera - no game camera saved");
		return;
	}

	// Game camera is already the main camera in the scene
	// We just stop applying editor camera overrides
	Zenith_Log("Switched to game camera");
}

void Zenith_Editor::BuildViewMatrix(Zenith_Maths::Matrix4& xOutMatrix)
{
	// In Playing mode, use the scene's camera (game controls it)
	if (s_eEditorMode == EditorMode::Playing)
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		if (xScene.m_pxMainCameraEntity)
		{
			xScene.GetMainCamera().BuildViewMatrix(xOutMatrix);
			return;
		}
	}
	
	// In Stopped/Paused mode (or no scene camera), build view matrix from editor state
	// Use the same approach as Zenith_CameraComponent for consistency
	Zenith_Maths::Matrix4_64 xPitchMat = glm::rotate(s_fEditorCameraPitch, glm::dvec3(1, 0, 0));
	Zenith_Maths::Matrix4_64 xYawMat = glm::rotate(s_fEditorCameraYaw, glm::dvec3(0, 1, 0));
	Zenith_Maths::Matrix4_64 xTransMat = glm::translate(-s_xEditorCameraPosition);
	xOutMatrix = xPitchMat * xYawMat * xTransMat;
}

void Zenith_Editor::BuildProjectionMatrix(Zenith_Maths::Matrix4& xOutMatrix)
{
	Zenith_Assert(s_eEditorMode != EditorMode::Playing, "Should be going through scene camera if we are in playing mode");
	
	float fAspectRatio = s_xViewportSize.x / s_xViewportSize.y;
	xOutMatrix = glm::perspective(glm::radians(s_fEditorCameraFOV), fAspectRatio, s_fEditorCameraNear, s_fEditorCameraFar);
	// Flip Y for Vulkan coordinate system (same as CameraComponent)
	xOutMatrix[1][1] *= -1;
}

void Zenith_Editor::GetCameraPosition(Zenith_Maths::Vector4& xOutPosition)
{
	// In Playing mode, use the scene's camera (game controls it)
	if (s_eEditorMode == EditorMode::Playing)
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		if (xScene.m_pxMainCameraEntity)
		{
			xScene.GetMainCamera().GetPosition(xOutPosition);
			return;
		}
	}
	
	// In Stopped/Paused mode (or no scene camera), return editor position
	xOutPosition = Zenith_Maths::Vector4(s_xEditorCameraPosition.x, s_xEditorCameraPosition.y, s_xEditorCameraPosition.z, 0.0f);
}

float Zenith_Editor::GetCameraNearPlane()
{
	// In Playing mode, use the scene's camera (game controls it)
	if (s_eEditorMode == EditorMode::Playing)
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		if (xScene.m_pxMainCameraEntity)
		{
			return xScene.GetMainCamera().GetNearPlane();
		}
	}
	
	// In Stopped/Paused mode (or no scene camera), return editor value
	return s_fEditorCameraNear;
}

float Zenith_Editor::GetCameraFarPlane()
{
	// In Playing mode, use the scene's camera (game controls it)
	if (s_eEditorMode == EditorMode::Playing)
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		if (xScene.m_pxMainCameraEntity)
		{
			return xScene.GetMainCamera().GetFarPlane();
		}
	}
	
	// In Stopped/Paused mode (or no scene camera), return editor value
	return s_fEditorCameraFar;
}

float Zenith_Editor::GetCameraFOV()
{
	// In Playing mode, use the scene's camera (game controls it)
	if (s_eEditorMode == EditorMode::Playing)
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		if (xScene.m_pxMainCameraEntity)
		{
			return xScene.GetMainCamera().GetFOV();
		}
	}
	
	// In Stopped/Paused mode (or no scene camera), return editor value
	return s_fEditorCameraFOV;
}

float Zenith_Editor::GetCameraAspectRatio()
{
	// In Playing mode, use the scene's camera (game controls it)
	if (s_eEditorMode == EditorMode::Playing)
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		if (xScene.m_pxMainCameraEntity)
		{
			return xScene.GetMainCamera().GetAspectRatio();
		}
	}
	
	// In Stopped/Paused mode (or no scene camera), calculate from viewport
	return s_xViewportSize.x / s_xViewportSize.y;
}

#endif // ZENITH_TOOLS
