#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_Editor.h"
#include "Zenith_SelectionSystem.h"
#include "Zenith_Gizmo.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Core/Zenith_Core.h"
#include "Input/Zenith_Input.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_RenderTargets.h"
#include "Vulkan/Zenith_Vulkan.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

// Static member initialization
EditorMode Zenith_Editor::s_eEditorMode = EditorMode::Stopped;
GizmoMode Zenith_Editor::s_eGizmoMode = GizmoMode::Translate;
Zenith_Entity* Zenith_Editor::s_pxSelectedEntity = nullptr;
Zenith_Maths::Vector2 Zenith_Editor::s_xViewportSize = { 1280, 720 };
Zenith_Maths::Vector2 Zenith_Editor::s_xViewportPos = { 0, 0 };
bool Zenith_Editor::s_bViewportHovered = false;
bool Zenith_Editor::s_bViewportFocused = false;
Zenith_Scene* Zenith_Editor::s_pxBackupScene = nullptr;

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
	s_pxSelectedEntity = nullptr;
	s_eGizmoMode = GizmoMode::Translate;

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
	Zenith_Gizmo::Shutdown();
	Zenith_SelectionSystem::Shutdown();
}

void Zenith_Editor::Update()
{
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

	// Handle object picking (only when not manipulating gizmo)
	if (!Zenith_Gizmo::IsManipulating())
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
				// TODO: Implement new scene creation
				// Would clear current scene and create a default setup
				Zenith_Log("New Scene - Not yet implemented");
			}

			if (ImGui::MenuItem("Open Scene"))
			{
				// TODO: Implement scene loading with file dialog
				// Would show native file picker and load .zscene file
				Zenith_Log("Open Scene - Not yet implemented");
				// Example: Zenith_Scene::GetCurrentScene().Deserialize("scene.zscene");
			}

			if (ImGui::MenuItem("Save Scene"))
			{
				// TODO: Implement scene saving with file dialog
				// Would show native file picker and save to .zscene file
				Zenith_Log("Save Scene - Not yet implemented");
				// Example: Zenith_Scene::GetCurrentScene().Serialize("scene.zscene");
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
	if (ImGui::RadioButton("Translate", s_eGizmoMode == GizmoMode::Translate))
	{
		SetGizmoMode(GizmoMode::Translate);
	}
	ImGui::SameLine();
	
	if (ImGui::RadioButton("Rotate", s_eGizmoMode == GizmoMode::Rotate))
	{
		SetGizmoMode(GizmoMode::Rotate);
	}
	ImGui::SameLine();
	
	if (ImGui::RadioButton("Scale", s_eGizmoMode == GizmoMode::Scale))
	{
		SetGizmoMode(GizmoMode::Scale);
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
		bool bIsSelected = (s_pxSelectedEntity && s_pxSelectedEntity->GetEntityID() == entityID);

		// Create selectable item for entity
		// Use entity name if available, otherwise show ID
		std::string strDisplayName = entity.m_strName.empty() ?
			("Entity_" + std::to_string(entityID)) : entity.m_strName;

		// Add unique ID to avoid ImGui label collisions
		std::string strLabel = strDisplayName + "##" + std::to_string(entityID);

		if (ImGui::Selectable(strLabel.c_str(), bIsSelected))
		{
			// Entity map stores entities by value, so we need to get a pointer
			// WARNING: This pointer is only valid until the entity map is modified
			SelectEntity(&entity);
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
	
	if (s_pxSelectedEntity)
	{
		ImGui::Text("Selected Entity");
		ImGui::Separator();
		
		// Transform component editing
		// Currently only shows Transform - need to add other components
		if (s_pxSelectedEntity->HasComponent<Zenith_TransformComponent>())
		{
			if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
			{
				Zenith_TransformComponent& transform = s_pxSelectedEntity->GetComponent<Zenith_TransformComponent>();
				
				Zenith_Maths::Vector3 pos, scale;
				Zenith_Maths::Quat rot;
				transform.GetPosition(pos);
				transform.GetRotation(rot);
				transform.GetScale(scale);
				
				float position[3] = { pos.x, pos.y, pos.z };
				if (ImGui::DragFloat3("Position", position, 0.1f))
				{
					transform.SetPosition({ position[0], position[1], position[2] });
				}
				
				float scaleValues[3] = { scale.x, scale.y, scale.z };
				if (ImGui::DragFloat3("Scale", scaleValues, 0.1f))
				{
					transform.SetScale({ scaleValues[0], scaleValues[1], scaleValues[2] });
				}
				
				// TODO: Add rotation editing (currently missing)
				// Convert quaternion to Euler angles for editing
				// Update quaternion when Euler angles change
			}
		}
		
		// TODO: Add component editor for other component types
		// - ModelComponent: Show model/material selection
		// - CameraComponent: Show FOV, near/far planes, etc.
		// - ColliderComponent: Show collision shape settings
		// - ScriptComponent: Show script assignment
		// Each needs custom ImGui widgets for its properties
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

	// Perform raycast to find entity under mouse
	Zenith_Entity* pxHitEntity = Zenith_SelectionSystem::RaycastSelect(xRayOrigin, xRayDir);

	if (pxHitEntity)
	{
		SelectEntity(pxHitEntity);
	}
	else
	{
		ClearSelection();
	}
}

void Zenith_Editor::RenderGizmos()
{
	// Only render if an entity is selected
	if (!s_pxSelectedEntity)
		return;

	// Only render gizmos in Stopped or Paused mode (not during active play)
	if (s_eEditorMode == EditorMode::Playing)
		return;

	// Get camera matrices for gizmo rendering
	Zenith_CameraComponent& xCamera = Zenith_Scene::GetCurrentScene().GetMainCamera();
	Zenith_Maths::Matrix4 xViewMatrix, xProjMatrix;
	xCamera.BuildViewMatrix(xViewMatrix);
	xCamera.BuildProjectionMatrix(xProjMatrix);

	// Convert GizmoMode to GizmoOperation
	GizmoOperation eOperation = static_cast<GizmoOperation>(s_eGizmoMode);

	// Call gizmo manipulation (handles both rendering and interaction)
	bool bWasManipulated = Zenith_Gizmo::Manipulate(
		s_pxSelectedEntity,
		eOperation,
		xViewMatrix,
		xProjMatrix,
		s_xViewportPos,
		s_xViewportSize
	);

	// Optionally render selection bounding box for visual feedback
	// (Currently disabled - can enable for debugging)
	// Zenith_SelectionSystem::RenderSelectedBoundingBox(s_pxSelectedEntity);
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

void Zenith_Editor::SelectEntity(Zenith_Entity* pxEntity)
{
	s_pxSelectedEntity = pxEntity;
	
	if (pxEntity)
	{
		Zenith_Log("Editor: Selected entity");
	}
}

void Zenith_Editor::ClearSelection()
{
	s_pxSelectedEntity = nullptr;
}

#endif // ZENITH_TOOLS
