#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_Editor.h"
#include "Zenith_SelectionSystem.h"
#include "Zenith_Gizmo.h"
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

// Static member initialization
EditorMode Zenith_Editor::s_eEditorMode = EditorMode::Stopped;
EditorGizmoMode Zenith_Editor::s_eGizmoMode = EditorGizmoMode::Translate;
Zenith_EntityID Zenith_Editor::s_uSelectedEntityID = INVALID_ENTITY_ID;
Zenith_Maths::Vector2 Zenith_Editor::s_xViewportSize = { 1280, 720 };
Zenith_Maths::Vector2 Zenith_Editor::s_xViewportPos = { 0, 0 };
bool Zenith_Editor::s_bViewportHovered = false;
bool Zenith_Editor::s_bViewportFocused = false;
Zenith_Scene* Zenith_Editor::s_pxBackupScene = nullptr;
bool Zenith_Editor::s_bPendingSceneLoad = false;
std::string Zenith_Editor::s_strPendingSceneLoadPath = "";
bool Zenith_Editor::s_bPendingSceneSave = false;
std::string Zenith_Editor::s_strPendingSceneSavePath = "";

// Content Browser state
std::string Zenith_Editor::s_strCurrentDirectory = ASSETS_ROOT;
std::vector<ContentBrowserEntry> Zenith_Editor::s_xDirectoryContents;
bool Zenith_Editor::s_bDirectoryNeedsRefresh = true;

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

	// Initialize editor subsystems
	Zenith_SelectionSystem::Initialise();
	Zenith_Gizmo::Initialise();
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

	// Shutdown editor subsystems
	Flux_Gizmos::Shutdown();
	Zenith_Gizmo::Shutdown();
	Zenith_SelectionSystem::Shutdown();
}

void Zenith_Editor::Update()
{
	// CRITICAL: Handle pending scene operations FIRST, before any rendering
	// This must happen here (not during RenderMainMenuBar) to avoid concurrent access
	// to scene data while render tasks are active.
	//
	// Both save and load operations iterate through scene data structures.
	// If render tasks are active while these operations occur, we risk:
	// - Reading corrupted data during save (render tasks modifying while we read)
	// - Crashes during load (destroying pools while render tasks access them)

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
	// 1. User clicks "Open Scene" in menu -> sets s_bPendingSceneLoad flag
	// 2. Frame continues, ImGui rendered, render tasks submitted and complete
	// 3. Next frame starts -> Update() called BEFORE any rendering
	// 4. Scene loaded here when no render tasks are accessing scene data
	if (s_bPendingSceneLoad)
	{
		s_bPendingSceneLoad = false;

		try
		{
			// Safe to load now - no render tasks are active
			Zenith_Scene::GetCurrentScene().LoadFromFile(s_strPendingSceneLoadPath);
			Zenith_Log("Scene loaded from %s", s_strPendingSceneLoadPath.c_str());

			// Clear selection as entity pointers are now invalid
			ClearSelection();
		}
		catch (const std::exception& e)
		{
			Zenith_Log("Failed to load scene: %s", e.what());
		}

		s_strPendingSceneLoadPath.clear();
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

	// Update bounding boxes for all entities (needed for selection)
	Zenith_SelectionSystem::UpdateBoundingBoxes();

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
	if (s_bViewportFocused && s_eEditorMode != EditorMode::Playing)
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

	// Handle gizmo interaction first (before object picking)
	HandleGizmoInteraction();

	// Handle object picking (only when not manipulating gizmo)
	if (!Flux_Gizmos::IsInteracting() && !Zenith_Gizmo::IsManipulating())
	{
		HandleObjectPicking();
	}
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
				// Clear the current scene
				Zenith_Scene::GetCurrentScene().Reset();
				Zenith_Log("New scene created");
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

				// For now, use a hardcoded path
				// TODO: Implement native file dialog
				s_strPendingSceneLoadPath = "scene.zscen";
				s_bPendingSceneLoad = true;

				Zenith_Log("Scene load queued: %s (will load next frame)", s_strPendingSceneLoadPath.c_str());
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

				// For now, use a hardcoded path
				// TODO: Implement native file dialog
				s_strPendingSceneSavePath = "scene.zscen";
				s_bPendingSceneSave = true;

				Zenith_Log("Scene save queued: %s (will save next frame)", s_strPendingSceneSavePath.c_str());
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
			if (ImGui::MenuItem("Undo", "Ctrl+Z"))
			{
				// TODO: Implement undo system
				Zenith_Log("Undo - Not yet implemented");
			}

			if (ImGui::MenuItem("Redo", "Ctrl+Y"))
			{
				// TODO: Implement redo system
				Zenith_Log("Redo - Not yet implemented");
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

		// Add unique ID to avoid ImGui label collisions
		std::string strLabel = strDisplayName + "##" + std::to_string(entityID);

		if (ImGui::Selectable(strLabel.c_str(), bIsSelected))
		{
			// Select by EntityID for safer memory management
			SelectEntity(entityID);
		}

		// Show context menu on right-click
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Delete Entity"))
			{
				// TODO: Implement entity deletion
				// Need to handle cleanup and deselection
			}
			ImGui::EndPopup();
		}
	}

	// Add button to create new entity
	ImGui::Separator();
	if (ImGui::Button("+ Create Entity"))
	{
		// TODO: Implement entity creation
		// Create new entity with default name
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
	Zenith_CameraComponent& xCamera = Zenith_Scene::GetCurrentScene().GetMainCamera();
	Zenith_Maths::Matrix4 xViewMatrix, xProjMatrix;
	xCamera.BuildViewMatrix(xViewMatrix);
	xCamera.BuildProjectionMatrix(xProjMatrix);

	// Convert screen position to world-space ray
	Zenith_Maths::Vector3 xRayDir = Zenith_Gizmo::ScreenToWorldRay(
		xViewportMousePos,
		{ 0, 0 },  // Viewport relative, so offset is 0
		s_xViewportSize,
		xViewMatrix,
		xProjMatrix
	);

	// Ray origin is camera position
	Zenith_Maths::Vector3 xRayOrigin;
	xCamera.GetPosition(xRayOrigin);

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
	Zenith_CameraComponent& xCamera = Zenith_Scene::GetCurrentScene().GetMainCamera();
	Zenith_Maths::Matrix4 xViewMatrix, xProjMatrix;
	xCamera.BuildViewMatrix(xViewMatrix);
	xCamera.BuildProjectionMatrix(xProjMatrix);

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
	Zenith_Maths::Vector3 xRayOrigin;
	xCamera.GetPosition(xRayOrigin);

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

	// STOPPED -> PLAYING: Backup scene state
	if (oldMode == EditorMode::Stopped && eMode == EditorMode::Playing)
	{
		Zenith_Log("Editor: Entering Play Mode");

		// TODO: FULL IMPLEMENTATION NEEDED
		// Currently this is a simplified implementation that does NOT preserve scene state
		// A full implementation would need to:
		// 1. Deep copy all entities and components to s_pxBackupScene
		// 2. Preserve entity relationships, component data, and resource references
		// 3. Handle pointers and references between entities
		//
		// RECOMMENDED APPROACHES:
		// A) Implement Scene::Clone() with component-level copy constructors
		// B) Use serialization/deserialization to memory (most robust)
		// C) Implement copy-on-write for modified entities only
		//
		// For now, we'll just log the transition and let the game run
		// When Stop is pressed, scene will NOT revert to pre-play state

		s_pxBackupScene = nullptr;  // Placeholder - would store cloned scene here

		Zenith_Log("WARNING: Scene state backup not yet implemented - changes during play will persist!");
	}

	// PLAYING/PAUSED -> STOPPED: Restore scene state
	else if (oldMode != EditorMode::Stopped && eMode == EditorMode::Stopped)
	{
		Zenith_Log("Editor: Stopping Play Mode");

		// TODO: FULL IMPLEMENTATION NEEDED
		// Currently this doesn't restore scene state
		// A full implementation would:
		// 1. Reset current scene: Zenith_Scene::GetCurrentScene().Reset()
		// 2. Restore entities and components from s_pxBackupScene
		// 3. Restore camera and selection references
		// 4. Clean up backup scene
		//
		// For now, we just clean up and note that state wasn't restored

		if (s_pxBackupScene)
		{
			delete s_pxBackupScene;
			s_pxBackupScene = nullptr;
		}

		// Clear selection as entity pointers may no longer be valid
		// (In full implementation, we'd restore selection by EntityID)
		ClearSelection();

		Zenith_Log("Scene returned to edit mode (state preservation not yet implemented)");
	}

	// PAUSED state - suspend scene updates
	else if (eMode == EditorMode::Paused)
	{
		Zenith_Log("Editor: Pausing");
		// Scene updates will be skipped in main loop when paused
	}

	// PAUSED -> PLAYING: Resume scene updates
	else if (oldMode == EditorMode::Paused && eMode == EditorMode::Playing)
	{
		Zenith_Log("Editor: Resuming");
		// Scene updates will resume in main loop
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
					if (xEntry.m_strExtension == ".ztx")
					{
						szPayloadType = DRAGDROP_PAYLOAD_TEXTURE_ZTX;
					}
					else if (xEntry.m_strExtension == ".zmsh")
					{
						szPayloadType = DRAGDROP_PAYLOAD_MESH_ZMSH;
					}

					ImGui::SetDragDropPayload(szPayloadType, &xPayload, sizeof(xPayload));

					// Drag preview tooltip
					ImGui::Text("Drag: %s", xEntry.m_strName.c_str());

					ImGui::EndDragDropSource();

					Zenith_Log("[ContentBrowser] Started dragging: %s", xEntry.m_strName.c_str());
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

#endif // ZENITH_TOOLS
