#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_Editor.h"
#include "Flux/Flux_MaterialAsset.h"

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
#include "EntityComponent/Components/Zenith_TextComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "Input/Zenith_Input.h"
#include "Flux/Flux_Graphics.h"
#include "Vulkan/Zenith_Vulkan.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"

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
Zenith_EntityID Zenith_Editor::s_uGameCameraEntity = INVALID_ENTITY_ID;
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
	// Initialize content browser to game assets directory
	s_strCurrentDirectory = Project_GetGameAssetsDirectory();

	s_eEditorMode = EditorMode::Stopped;
	s_xSelectedEntityIDs.clear();
	s_uPrimarySelectedEntityID = INVALID_ENTITY_ID;
	s_uLastClickedEntityID = INVALID_ENTITY_ID;
	s_eGizmoMode = EditorGizmoMode::Translate;

	// Initialize material system
	Flux_MaterialAsset::Initialize();

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

	// Reset editor camera state
	s_bEditorCameraInitialized = false;
	
	// Clear material selection and shutdown material system
	s_pxSelectedMaterial = nullptr;
	Flux_MaterialAsset::Shutdown();

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
		Zenith_Vulkan::WaitForGPUIdle();

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
		Zenith_Vulkan::WaitForGPUIdle();  // GPU synchronization

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

			// CRITICAL: Dispatch full lifecycle for all entities after backup restore
			// In Stopped mode, Scene::Update() is not called, so lifecycle hooks would never run.
			// Must dispatch OnAwake (which calls OnCreate to set up resources), OnEnable, then OnStart.
			// This allows behaviours (like Sokoban_Behaviour) to initialize and regenerate transient entities.
			Zenith_Scene::DispatchFullLifecycleInit();
			Zenith_Log(LOG_CATEGORY_EDITOR, "Full lifecycle dispatched for all entities after backup restore");

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

// Helper function to render a single entity in the hierarchy tree
void Zenith_Editor::RenderEntityTreeNode(Zenith_Scene& xScene, Zenith_Entity xEntity, Zenith_EntityID& uEntityToDelete, Zenith_EntityID& uDraggedEntityID, Zenith_EntityID& uDropTargetEntityID)
{
	Zenith_EntityID uEntityID = xEntity.GetEntityID();
	bool bIsSelected = Zenith_Editor::IsSelected(uEntityID);
	bool bHasChildren = xEntity.HasChildren();

	// Build display name
	std::string strDisplayName = xEntity.GetName().empty() ?
		("Entity_" + std::to_string(uEntityID.m_uIndex)) : xEntity.GetName();

	// Count components for display
	uint32_t uComponentCount = 0;
	std::string strComponentSummary;
	Zenith_ComponentRegistry& xRegistry = Zenith_ComponentRegistry::Get();
	const auto& xEntries = xRegistry.GetEntries();
	for (const Zenith_ComponentRegistryEntry& xEntry : xEntries)
	{
		if (xEntry.m_fnHasComponent(xEntity))
		{
			if (uComponentCount > 0)
				strComponentSummary += ", ";
			strComponentSummary += xEntry.m_strDisplayName;
			uComponentCount++;
		}
	}

	if (uComponentCount > 0)
	{
		strDisplayName += " [" + std::to_string(uComponentCount) + "]";
	}

	// Tree node flags
	ImGuiTreeNodeFlags eFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (bIsSelected)
	{
		eFlags |= ImGuiTreeNodeFlags_Selected;
	}
	if (!bHasChildren)
	{
		eFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	// Render tree node
	bool bNodeOpen = ImGui::TreeNodeEx((void*)(uintptr_t)uEntityID.GetPacked(), eFlags, "%s", strDisplayName.c_str());

	// Handle selection on click
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
	{
		bool bCtrlHeld = ImGui::GetIO().KeyCtrl;
		bool bShiftHeld = ImGui::GetIO().KeyShift;

		if (bShiftHeld && Zenith_Editor::GetLastClickedEntityID() != INVALID_ENTITY_ID)
		{
			Zenith_Editor::SelectRange(uEntityID);
		}
		else if (bCtrlHeld)
		{
			Zenith_Editor::ToggleEntitySelection(uEntityID);
		}
		else
		{
			Zenith_Editor::SelectEntity(uEntityID, false);
		}
	}

	// Drag source for reparenting
	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
	{
		ImGui::SetDragDropPayload("HIERARCHY_ENTITY", &uEntityID, sizeof(Zenith_EntityID));
		ImGui::Text("Move: %s", xEntity.GetName().c_str());
		uDraggedEntityID = uEntityID;
		ImGui::EndDragDropSource();
	}

	// Drop target for reparenting
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY"))
		{
			Zenith_EntityID uSourceEntityID = *(const Zenith_EntityID*)pPayload->Data;
			// Set as child of this entity
			uDropTargetEntityID = uEntityID;
			uDraggedEntityID = uSourceEntityID;
		}
		ImGui::EndDragDropTarget();
	}

	// Show component list in tooltip on hover
	if (ImGui::IsItemHovered() && uComponentCount > 0)
	{
		ImGui::SetTooltip("Components: %s", strComponentSummary.c_str());
	}

	// Context menu
	if (ImGui::BeginPopupContextItem())
	{
		if (ImGui::MenuItem("Create Child Entity"))
		{
			Zenith_Entity xNewEntity(&xScene, "New Child");
			xNewEntity.SetTransient(false);  // Editor-created entities are persistent
			xNewEntity.SetParent(uEntityID);
			Zenith_Editor::SelectEntity(xNewEntity.GetEntityID());
		}

		if (xEntity.HasParent())
		{
			if (ImGui::MenuItem("Unparent"))
			{
				xEntity.SetParent(INVALID_ENTITY_ID);
			}
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Delete Entity"))
		{
			if (Zenith_Editor::IsSelected(uEntityID))
			{
				Zenith_Editor::DeselectEntity(uEntityID);
			}
			uEntityToDelete = uEntityID;
		}
		ImGui::EndPopup();
	}

	// Recursively render children if node is open and has children
	if (bNodeOpen && bHasChildren)
	{
		Zenith_Vector<Zenith_EntityID> xChildren = xEntity.GetChildEntityIDs();
		for (u_int u = 0; u < xChildren.GetSize(); ++u)
		{
			Zenith_EntityID xChildID = xChildren.Get(u);
			if (xScene.EntityExists(xChildID))
			{
				RenderEntityTreeNode(xScene, xScene.GetEntity(xChildID), uEntityToDelete, uDraggedEntityID, uDropTargetEntityID);
			}
		}
		ImGui::TreePop();
	}
}

void Zenith_Editor::RenderHierarchyPanel()
{
	ImGui::Begin("Hierarchy");

	ImGui::Text("Scene Entities:");
	ImGui::Separator();

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Track entity to delete and drag/drop targets
	Zenith_EntityID uEntityToDelete = INVALID_ENTITY_ID;
	Zenith_EntityID uDraggedEntityID = INVALID_ENTITY_ID;
	Zenith_EntityID uDropTargetEntityID = INVALID_ENTITY_ID;

	// Render only root entities (entities without parents)
	const Zenith_Vector<Zenith_EntityID>& xActiveEntities = xScene.GetActiveEntities();
	for (u_int u = 0; u < xActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xEntityID = xActiveEntities.Get(u);
		if (xScene.EntityExists(xEntityID))
		{
			Zenith_Entity xEntity = xScene.GetEntity(xEntityID);
			if (!xEntity.HasParent())
			{
				RenderEntityTreeNode(xScene, xEntity, uEntityToDelete, uDraggedEntityID, uDropTargetEntityID);
			}
		}
	}

	// Drop target for root level (unparent)
	ImGui::Dummy(ImVec2(0, 20));
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY"))
		{
			Zenith_EntityID xSourceEntityID = *(const Zenith_EntityID*)pPayload->Data;
			if (xScene.EntityExists(xSourceEntityID))
			{
				xScene.GetEntity(xSourceEntityID).SetParent(INVALID_ENTITY_ID);
			}
		}
		ImGui::EndDragDropTarget();
	}

	// Handle reparenting from drag-drop
	if (uDraggedEntityID.IsValid() && uDropTargetEntityID.IsValid())
	{
		if (xScene.EntityExists(uDraggedEntityID) && uDraggedEntityID != uDropTargetEntityID)
		{
			// Prevent creating circular parent-child relationships
			bool bIsAncestor = false;
			Zenith_EntityID xCheckID = uDropTargetEntityID;
			while (xCheckID.IsValid())
			{
				if (xCheckID == uDraggedEntityID)
				{
					bIsAncestor = true;
					break;
				}
				if (xScene.EntityExists(xCheckID))
				{
					xCheckID = xScene.GetEntity(xCheckID).GetParentEntityID();
				}
				else
				{
					break;
				}
			}

			if (!bIsAncestor)
			{
				xScene.GetEntity(uDraggedEntityID).SetParent(uDropTargetEntityID);
			}
		}
	}

	// Perform deferred entity deletion
	if (uEntityToDelete != INVALID_ENTITY_ID)
	{
		if (uEntityToDelete == s_uGameCameraEntity)
		{
			s_uGameCameraEntity = INVALID_ENTITY_ID;
		}
		xScene.RemoveEntity(uEntityToDelete);
	}

	// Add button to create new entity
	ImGui::Separator();
	if (ImGui::Button("+ Create Entity"))
	{
		Zenith_Entity xNewEntity(&xScene, "New Entity");
		xNewEntity.SetTransient(false);  // Editor-created entities are persistent
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
		strncpy(nameBuffer, pxSelectedEntity->GetName().c_str(), sizeof(nameBuffer));
		nameBuffer[sizeof(nameBuffer) - 1] = '\0';
		if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
		{
			pxSelectedEntity->SetName(nameBuffer);
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
			Zenith_Log(LOG_CATEGORY_EDITOR, "[Editor] Add Component button clicked for Entity %u", s_uPrimarySelectedEntityID);
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
						Zenith_Log(LOG_CATEGORY_EDITOR, "[Editor] User selected to add component: %s to Entity %u",
							xEntry.m_strDisplayName.c_str(), s_uPrimarySelectedEntityID);

						// Add the component through the registry
						bool bSuccess = xRegistry.TryAddComponent(i, *pxSelectedEntity);

						if (bSuccess)
						{
							Zenith_Log(LOG_CATEGORY_EDITOR, "[Editor] Successfully added %s component to Entity %u",
								xEntry.m_strDisplayName.c_str(), s_uPrimarySelectedEntityID);
						}
						else
						{
							Zenith_Log(LOG_CATEGORY_EDITOR, "[Editor] ERROR: Failed to add %s component to Entity %u",
								xEntry.m_strDisplayName.c_str(), s_uPrimarySelectedEntityID);
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

	// Search and Filter bar
	ImGui::Separator();

	// Search input
	ImGui::SetNextItemWidth(200.0f);
	bool bSearchChanged = ImGui::InputTextWithHint("##Search", "Search...", s_szSearchBuffer, sizeof(s_szSearchBuffer));

	ImGui::SameLine();

	// Asset type filter dropdown
	const char* aszFilterTypes[] = { "All Types", "Textures", "Materials", "Meshes", "Models", "Prefabs", "Scenes", "Animations" };
	ImGui::SetNextItemWidth(120.0f);
	bool bFilterChanged = ImGui::Combo("##TypeFilter", &s_iAssetTypeFilter, aszFilterTypes, IM_ARRAYSIZE(aszFilterTypes));

	// Apply filtering
	if (bSearchChanged || bFilterChanged || s_xFilteredContents.empty())
	{
		s_xFilteredContents.clear();
		std::string strSearch(s_szSearchBuffer);

		// Convert search to lowercase for case-insensitive matching
		std::transform(strSearch.begin(), strSearch.end(), strSearch.begin(), ::tolower);

		for (const auto& xEntry : s_xDirectoryContents)
		{
			// Search filter
			if (!strSearch.empty())
			{
				std::string strNameLower = xEntry.m_strName;
				std::transform(strNameLower.begin(), strNameLower.end(), strNameLower.begin(), ::tolower);
				if (strNameLower.find(strSearch) == std::string::npos)
				{
					continue;
				}
			}

			// Type filter (directories always pass)
			if (s_iAssetTypeFilter > 0 && !xEntry.m_bIsDirectory)
			{
				bool bPassFilter = false;
				switch (s_iAssetTypeFilter)
				{
				case 1: bPassFilter = (xEntry.m_strExtension == ZENITH_TEXTURE_EXT); break;
				case 2: bPassFilter = (xEntry.m_strExtension == ZENITH_MATERIAL_EXT); break;
				case 3: bPassFilter = (xEntry.m_strExtension == ZENITH_MESH_EXT); break;
				case 4: bPassFilter = (xEntry.m_strExtension == ZENITH_MODEL_EXT); break;
				case 5: bPassFilter = (xEntry.m_strExtension == ZENITH_PREFAB_EXT); break;
				case 6: bPassFilter = (xEntry.m_strExtension == ".zscn"); break;
				case 7: bPassFilter = (xEntry.m_strExtension == ZENITH_ANIMATION_EXT); break;
				}
				if (!bPassFilter)
				{
					continue;
				}
			}

			s_xFilteredContents.push_back(xEntry);
		}
	}

	ImGui::Separator();

	// Context menu for creating new assets (right-click on empty area)
	if (ImGui::BeginPopupContextWindow("ContentBrowserContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (ImGui::BeginMenu("Create"))
		{
			if (ImGui::MenuItem("Folder"))
			{
				// Create new folder
				std::string strNewFolder = s_strCurrentDirectory + "/NewFolder";
				int iCounter = 1;
				while (std::filesystem::exists(strNewFolder))
				{
					strNewFolder = s_strCurrentDirectory + "/NewFolder" + std::to_string(iCounter++);
				}
				std::filesystem::create_directory(strNewFolder);
				s_bDirectoryNeedsRefresh = true;
			}
			if (ImGui::MenuItem("Material"))
			{
				// Create new material
				std::string strNewMaterial = s_strCurrentDirectory + "/NewMaterial" + ZENITH_MATERIAL_EXT;
				int iCounter = 1;
				while (std::filesystem::exists(strNewMaterial))
				{
					strNewMaterial = s_strCurrentDirectory + "/NewMaterial" + std::to_string(iCounter++) + ZENITH_MATERIAL_EXT;
				}
				Flux_MaterialAsset* pxNewMat = Flux_MaterialAsset::Create("NewMaterial");
				if (pxNewMat)
				{
					pxNewMat->SaveToFile(strNewMaterial);
					s_bDirectoryNeedsRefresh = true;
				}
			}
			ImGui::EndMenu();
		}
		ImGui::EndPopup();
	}

	// Display directory contents in a table/grid
	float fPanelWidth = ImGui::GetContentRegionAvail().x;
	float fCellSize = 80.0f;  // Size of each item cell
	int iColumnCount = std::max(1, (int)(fPanelWidth / fCellSize));

	if (ImGui::BeginTable("ContentBrowserTable", iColumnCount))
	{
		for (size_t i = 0; i < s_xFilteredContents.size(); ++i)
		{
			const ContentBrowserEntry& xEntry = s_xFilteredContents[i];

			ImGui::TableNextColumn();
			ImGui::PushID((int)i);

			// Icon representation (using text for now)
			const char* szIcon = xEntry.m_bIsDirectory ? "[DIR]" : "[FILE]";

			// File type specific icons
			if (!xEntry.m_bIsDirectory)
			{
				if (xEntry.m_strExtension == ZENITH_TEXTURE_EXT) szIcon = "[TEX]";
				else if (xEntry.m_strExtension == ZENITH_MATERIAL_EXT) szIcon = "[MAT]";
				else if (xEntry.m_strExtension == ZENITH_MESH_EXT) szIcon = "[MSH]";
				else if (xEntry.m_strExtension == ZENITH_MODEL_EXT) szIcon = "[MDL]";
				else if (xEntry.m_strExtension == ZENITH_PREFAB_EXT) szIcon = "[PRE]";
				else if (xEntry.m_strExtension == ".zscn") szIcon = "[SCN]";
				else if (xEntry.m_strExtension == ZENITH_ANIMATION_EXT) szIcon = "[ANM]";
			}

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
					else if (xEntry.m_strExtension == ZENITH_PREFAB_EXT)
					{
						szPayloadType = DRAGDROP_PAYLOAD_PREFAB;
					}
					else if (xEntry.m_strExtension == ZENITH_MODEL_EXT)
					{
						szPayloadType = DRAGDROP_PAYLOAD_MODEL;
					}
					else if (xEntry.m_strExtension == ZENITH_ANIMATION_EXT)
					{
						szPayloadType = DRAGDROP_PAYLOAD_ANIMATION;
					}

					ImGui::SetDragDropPayload(szPayloadType, &xPayload, sizeof(xPayload));

					// Drag preview tooltip
					ImGui::Text("Drag: %s", xEntry.m_strName.c_str());

					ImGui::EndDragDropSource();
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

			// Context menu for individual items
			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Show in Explorer"))
				{
#ifdef _WIN32
					std::string strCmd = "explorer /select,\"" + xEntry.m_strFullPath + "\"";
					system(strCmd.c_str());
#endif
				}
				if (!xEntry.m_bIsDirectory)
				{
					if (ImGui::MenuItem("Delete"))
					{
						if (std::filesystem::remove(xEntry.m_strFullPath))
						{
							// Also try to remove the .zmeta file
							std::string strMetaPath = xEntry.m_strFullPath + ".zmeta";
							std::filesystem::remove(strMetaPath);
							s_bDirectoryNeedsRefresh = true;
						}
					}
					if (ImGui::MenuItem("Duplicate"))
					{
						std::filesystem::path xPath(xEntry.m_strFullPath);
						std::string strNewPath = xPath.parent_path().string() + "/" +
							xPath.stem().string() + "_copy" + xPath.extension().string();
						int iCounter = 1;
						while (std::filesystem::exists(strNewPath))
						{
							strNewPath = xPath.parent_path().string() + "/" +
								xPath.stem().string() + "_copy" + std::to_string(iCounter++) + xPath.extension().string();
						}
						std::filesystem::copy(xEntry.m_strFullPath, strNewPath);
						s_bDirectoryNeedsRefresh = true;
					}
				}
				else
				{
					if (ImGui::MenuItem("Delete Folder"))
					{
						// Only delete empty folders for safety
						if (std::filesystem::is_empty(xEntry.m_strFullPath))
						{
							std::filesystem::remove(xEntry.m_strFullPath);
							s_bDirectoryNeedsRefresh = true;
						}
						else
						{
							Zenith_Log(LOG_CATEGORY_EDITOR, "[ContentBrowser] Cannot delete non-empty folder");
						}
					}
				}
				ImGui::EndPopup();
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
	s_xFilteredContents.clear();

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

		Zenith_Log(LOG_CATEGORY_EDITOR, "[ContentBrowser] Refreshed directory: %s (%zu items)",
			s_strCurrentDirectory.c_str(), s_xDirectoryContents.size());
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[ContentBrowser] Error reading directory: %s", e.what());
	}
}

void Zenith_Editor::NavigateToDirectory(const std::string& strPath)
{
	s_strCurrentDirectory = strPath;
	s_bDirectoryNeedsRefresh = true;
	Zenith_Log(LOG_CATEGORY_EDITOR, "[ContentBrowser] Navigated to: %s", strPath.c_str());
}

void Zenith_Editor::NavigateToParent()
{
	std::filesystem::path xPath(s_strCurrentDirectory);
	std::filesystem::path xParent = xPath.parent_path();

	// Don't navigate above game assets directory
	std::string strAssetsRoot = Project_GetGameAssetsDirectory();
	// Remove trailing slash if present for comparison
	if (!strAssetsRoot.empty() && (strAssetsRoot.back() == '/' || strAssetsRoot.back() == '\\'))
	{
		strAssetsRoot.pop_back();
	}

	if (xParent.string().length() >= strAssetsRoot.length())
	{
		s_strCurrentDirectory = xParent.string();
		s_bDirectoryNeedsRefresh = true;
		Zenith_Log(LOG_CATEGORY_EDITOR, "[ContentBrowser] Navigated to parent: %s", s_strCurrentDirectory.c_str());
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[ContentBrowser] Already at root directory");
	}
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
	ImGui::SameLine();
	ImGui::Separator();
	ImGui::SameLine();

	// Category filter dropdown
	if (ImGui::Button("Categories..."))
	{
		ImGui::OpenPopup("CategoryFilterPopup");
	}
	if (ImGui::BeginPopup("CategoryFilterPopup"))
	{
		if (ImGui::Button("All"))
		{
			s_xCategoryFilters.set();
		}
		ImGui::SameLine();
		if (ImGui::Button("None"))
		{
			s_xCategoryFilters.reset();
		}
		ImGui::Separator();
		for (u_int8 i = 0; i < LOG_CATEGORY_COUNT; ++i)
		{
			bool bEnabled = s_xCategoryFilters.test(i);
			if (ImGui::Checkbox(Zenith_LogCategoryNames[i], &bEnabled))
			{
				s_xCategoryFilters.set(i, bEnabled);
			}
		}
		ImGui::EndPopup();
	}

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

		// Also filter by category
		if (bShow && !s_xCategoryFilters.test(static_cast<size_t>(xEntry.m_eCategory)))
		{
			bShow = false;
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
		Zenith_Log(LOG_CATEGORY_EDITOR, "[Editor] Selected material: %s", pMaterial->GetName().c_str());
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
		Zenith_Log(LOG_CATEGORY_EDITOR, "[MaterialEditor] Created new material: %s", pNewMaterial->GetName().c_str());
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
				Zenith_Log(LOG_CATEGORY_EDITOR, "[MaterialEditor] Loaded material: %s", strFilePath.c_str());
			}
			else
			{
				Zenith_Log(LOG_CATEGORY_EDITOR, "[MaterialEditor] ERROR: Failed to load material: %s", strFilePath.c_str());
			}
		}
#endif
	}
	
	ImGui::Separator();

	// Display ALL materials (both file-cached and runtime-created)
	Zenith_Vector<Flux_MaterialAsset*> allMaterials;
	Flux_MaterialAsset::GetAllMaterials(allMaterials);

	if (ImGui::CollapsingHeader("All Materials", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Total: %u materials", allMaterials.GetSize());
		ImGui::Separator();

		for (Zenith_Vector<Flux_MaterialAsset*>::Iterator xIt(allMaterials); !xIt.Done(); xIt.Next())
		{
			Flux_MaterialAsset* pMat = xIt.GetData();
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

		if (allMaterials.GetSize() == 0)
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
						Zenith_Log(LOG_CATEGORY_EDITOR, "[MaterialEditor] Saved material to: %s", strFilePath.c_str());
					}
				}
#endif
			}
			else
			{
				// Save to existing path
				if (pMat->SaveToFile(pMat->GetAssetPath()))
				{
					Zenith_Log(LOG_CATEGORY_EDITOR, "[MaterialEditor] Saved material: %s", pMat->GetAssetPath().c_str());
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
					Zenith_Log(LOG_CATEGORY_EDITOR, "[MaterialEditor] Saved material to: %s", strFilePath.c_str());
				}
			}
#endif
		}
		
		ImGui::SameLine();
		
		if (ImGui::Button("Reload"))
		{
			pMat->Reload();
			Zenith_Log(LOG_CATEGORY_EDITOR, "[MaterialEditor] Reloaded material: %s", pMat->GetName().c_str());
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
			Zenith_Log(LOG_CATEGORY_EDITOR, "[MaterialEditor] Set %s texture: %s", szLabel, pFilePayload->m_szFilePath);
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
			Zenith_Log(LOG_CATEGORY_EDITOR, "[MaterialEditor] Cleared %s texture", szLabel);
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
	if (xScene.m_xMainCameraEntity != INVALID_ENTITY_ID)
	{
		try
		{
			Zenith_CameraComponent& xSceneCamera = xScene.GetMainCamera();
			xSceneCamera.GetPosition(s_xEditorCameraPosition);
			s_fEditorCameraPitch = xSceneCamera.GetPitch();
			s_fEditorCameraYaw = xSceneCamera.GetYaw();
			s_fEditorCameraFOV = xSceneCamera.GetFOV();
			s_fEditorCameraNear = xSceneCamera.GetNearPlane();
			s_fEditorCameraFar = xSceneCamera.GetFarPlane();
			Zenith_Log(LOG_CATEGORY_EDITOR, "Editor camera initialized from scene camera position");
		}
		catch (...)
		{
			Zenith_Log(LOG_CATEGORY_EDITOR, "Scene camera not available, using default position");
		}
	}

	s_bEditorCameraInitialized = true;
	Zenith_Log(LOG_CATEGORY_EDITOR, "Editor camera initialized at position (%.1f, %.1f, %.1f)", s_xEditorCameraPosition.x, s_xEditorCameraPosition.y, s_xEditorCameraPosition.z);
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
	if (s_uGameCameraEntity != INVALID_ENTITY_ID)
	{
		Zenith_Entity xCameraEntity = Zenith_Scene::GetCurrentScene().TryGetEntity(s_uGameCameraEntity);
		if (xCameraEntity.IsValid() && xCameraEntity.HasComponent<Zenith_CameraComponent>())
		{
			Zenith_CameraComponent& xCamera = xCameraEntity.GetComponent<Zenith_CameraComponent>();
			xCamera.SetPosition(s_xEditorCameraPosition);
			xCamera.SetPitch(s_fEditorCameraPitch);
			xCamera.SetYaw(s_fEditorCameraYaw);
		}
	}
}

void Zenith_Editor::SwitchToEditorCamera()
{
	if (!s_bEditorCameraInitialized)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Warning: Cannot switch to editor camera - not initialized");
		return;
	}

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Save the game's current main camera entity
	s_uGameCameraEntity = xScene.m_xMainCameraEntity;

	// Copy game camera state to editor camera
	if (s_uGameCameraEntity != INVALID_ENTITY_ID)
	{
		Zenith_Entity xEntity = xScene.TryGetEntity(s_uGameCameraEntity);
		if (xEntity.IsValid() && xEntity.HasComponent<Zenith_CameraComponent>())
		{
			Zenith_CameraComponent& xGameCamera = xEntity.GetComponent<Zenith_CameraComponent>();
			xGameCamera.GetPosition(s_xEditorCameraPosition);
			s_fEditorCameraPitch = xGameCamera.GetPitch();
			s_fEditorCameraYaw = xGameCamera.GetYaw();
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_EDITOR, "Warning: Could not copy game camera state to editor camera");
		}
	}

	Zenith_Log(LOG_CATEGORY_EDITOR, "Switched to editor camera");
}

void Zenith_Editor::SwitchToGameCamera()
{
	if (s_uGameCameraEntity == INVALID_ENTITY_ID)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Warning: Cannot switch to game camera - no game camera saved");
		return;
	}

	// Game camera is already the main camera in the scene
	// We just stop applying editor camera overrides
	Zenith_Log(LOG_CATEGORY_EDITOR, "Switched to game camera");
}

void Zenith_Editor::BuildViewMatrix(Zenith_Maths::Matrix4& xOutMatrix)
{
	// In Playing mode, use the scene's camera (game controls it)
	if (s_eEditorMode == EditorMode::Playing)
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		if (xScene.m_xMainCameraEntity != INVALID_ENTITY_ID)
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
		if (xScene.m_xMainCameraEntity != INVALID_ENTITY_ID)
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
		if (xScene.m_xMainCameraEntity != INVALID_ENTITY_ID)
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
		if (xScene.m_xMainCameraEntity != INVALID_ENTITY_ID)
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
		if (xScene.m_xMainCameraEntity != INVALID_ENTITY_ID)
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
		if (xScene.m_xMainCameraEntity != INVALID_ENTITY_ID)
		{
			return xScene.GetMainCamera().GetAspectRatio();
		}
	}
	
	// In Stopped/Paused mode (or no scene camera), calculate from viewport
	return s_xViewportSize.x / s_xViewportSize.y;
}

#endif // ZENITH_TOOLS
