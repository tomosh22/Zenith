#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_Editor.h"
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
static VkDescriptorSet s_xCachedGameTextureDescriptorSet = VK_NULL_HANDLE;
static VkImageView s_xCachedImageView = VK_NULL_HANDLE;

void Zenith_Editor::Initialise()
{
	s_eEditorMode = EditorMode::Stopped;
	s_pxSelectedEntity = nullptr;
	s_eGizmoMode = GizmoMode::Translate;
}

void Zenith_Editor::Shutdown()
{
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
}

void Zenith_Editor::Update()
{
	// Handle editor mode changes
	if (s_eEditorMode == EditorMode::Playing)
	{
		// Game is running normally
	}
	else if (s_eEditorMode == EditorMode::Paused)
	{
		// Game is paused - don't update game logic
	}
	
	// Handle object picking
	HandleObjectPicking();
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
}

void Zenith_Editor::RenderMainMenuBar()
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New Scene")) { }
			if (ImGui::MenuItem("Open Scene")) { }
			if (ImGui::MenuItem("Save Scene")) { }
			ImGui::Separator();
			if (ImGui::MenuItem("Exit")) { }
			ImGui::EndMenu();
		}
		
		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Undo", "Ctrl+Z")) { }
			if (ImGui::MenuItem("Redo", "Ctrl+Y")) { }
			ImGui::EndMenu();
		}
		
		if (ImGui::BeginMenu("View"))
		{
			if (ImGui::MenuItem("Hierarchy")) { }
			if (ImGui::MenuItem("Properties")) { }
			if (ImGui::MenuItem("Console")) { }
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
	
	// TODO: Display all entities in the scene
	ImGui::Text("Scene Entities:");
	ImGui::Separator();
	
	ImGui::End();
}

void Zenith_Editor::RenderPropertiesPanel()
{
	ImGui::Begin("Properties");
	
	if (s_pxSelectedEntity)
	{
		ImGui::Text("Selected Entity");
		ImGui::Separator();
		
		// Transform component
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
			}
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
	
	// Get the final render target SRV
	Flux_ShaderResourceView& xGameRenderSRV = Flux_Graphics::s_xFinalRenderTarget.m_axColourAttachments[0].m_pxSRV;
	
	if (xGameRenderSRV.m_xImageView != VK_NULL_HANDLE)
	{
		// Ensure the image is in the correct layout for sampling
		// After all game rendering is done, the image should already be in ShaderReadOnlyOptimal
		// from TransitionTargetsAfterRenderPass, but we'll verify it's ready for sampling
			
		// Register the texture with ImGui
		VkDescriptorSet xDescriptorSet = ImGui_ImplVulkan_AddTexture(
				Flux_Graphics::s_xRepeatSampler.GetSampler(),
				xGameRenderSRV.m_xImageView,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			);
			
		// Get available content region size
		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
		
		// Display the game render target as an image
		ImGui::Image((ImTextureID)(uintptr_t)xDescriptorSet, viewportPanelSize);
	}
	else
	{
		ImGui::Text("Game render target not available");
	}
	
	ImGui::End();
}

void Zenith_Editor::HandleObjectPicking()
{
	// TODO: Implement raycasting to pick objects
	// This requires:
	// 1. Get mouse position in viewport
	// 2. Convert to world ray
	// 3. Test ray against all entity bounding boxes
	// 4. Select the closest hit entity
}

void Zenith_Editor::RenderGizmos()
{
	// TODO: Implement gizmo rendering
}

void Zenith_Editor::SetEditorMode(EditorMode eMode)
{
	if (s_eEditorMode == eMode)
		return;
	
	EditorMode oldMode = s_eEditorMode;
	s_eEditorMode = eMode;
	
	// Handle mode transitions
	if (oldMode == EditorMode::Stopped && eMode == EditorMode::Playing)
	{
		Zenith_Log("Editor: Entering Play Mode");
	}
	else if (oldMode != EditorMode::Stopped && eMode == EditorMode::Stopped)
	{
		Zenith_Log("Editor: Stopping Play Mode");
		
		if (s_pxBackupScene)
		{
			delete s_pxBackupScene;
		 s_pxBackupScene = nullptr;
		}
	}
	else if (eMode == EditorMode::Paused)
	{
		Zenith_Log("Editor: Pausing");
	}
	else if (oldMode == EditorMode::Paused && eMode == EditorMode::Playing)
	{
		Zenith_Log("Editor: Resuming");
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
