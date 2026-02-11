#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_Hierarchy.h"
#include "Editor/Zenith_Editor.h"
#include "EntityComponent/Zenith_ComponentRegistry.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "FileAccess/Zenith_FileAccess.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

// Windows file dialog helper (defined in Zenith_Editor.cpp)
#ifdef _WIN32
extern std::string ShowSaveFileDialog(const char* szFilter, const char* szDefaultExt, const char* szDefaultFilename);
#endif

//=============================================================================
// Hierarchy Panel Implementation (Unity-Style Multi-Scene)
//
// Displays all loaded scenes as collapsible divider bars.
// Active scene is bold, dirty scenes show *, DontDestroyOnLoad only shows
// when it has entities.
//=============================================================================

namespace Zenith_EditorPanelHierarchy
{

//-----------------------------------------------------------------------------
// Helper: Render a single entity tree node recursively
//-----------------------------------------------------------------------------
static void RenderEntityTreeNode(
	Zenith_SceneData& xSceneData,
	Zenith_Entity xEntity,
	Zenith_EntityID& uEntityToDelete,
	Zenith_EntityID& uDraggedEntityID,
	Zenith_EntityID& uDropTargetEntityID,
	Zenith_Scene xDropTargetScene)
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
		if (xEntry.m_pfnHasComponent(xEntity))
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

	// Drop target for reparenting (within same scene)
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
			Zenith_Entity xNewEntity(&xSceneData, "New Child");
			xNewEntity.SetTransient(false);
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

		// Move To Scene submenu (only for root entities)
		if (!xEntity.HasParent())
		{
			Zenith_Scene xEntityScene = xEntity.GetScene();

			if (ImGui::BeginMenu("Move To Scene"))
			{
				uint32_t uSceneCount = Zenith_SceneManager::GetLoadedSceneCount();
				for (uint32_t i = 0; i < uSceneCount; ++i)
				{
					Zenith_Scene xScene = Zenith_SceneManager::GetSceneAt(i);
					if (!xScene.IsValid() || xScene == xEntityScene)
						continue;

					if (ImGui::MenuItem(xScene.GetName().c_str()))
					{
						Zenith_SceneManager::MoveEntityToScene(xEntity, xScene);
					}
				}
				ImGui::EndMenu();
			}

			// Move to DontDestroyOnLoad (only if not already in persistent scene)
			Zenith_Scene xPersistentScene = Zenith_SceneManager::GetPersistentScene();
			if (xEntityScene != xPersistentScene)
			{
				if (ImGui::MenuItem("Move to DontDestroyOnLoad"))
				{
					Zenith_SceneManager::MarkEntityPersistent(xEntity);
				}
			}

			ImGui::Separator();
		}

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
			if (xSceneData.EntityExists(xChildID))
			{
				RenderEntityTreeNode(xSceneData, xSceneData.GetEntity(xChildID), uEntityToDelete, uDraggedEntityID, uDropTargetEntityID, xDropTargetScene);
			}
		}
		ImGui::TreePop();
	}
}

//-----------------------------------------------------------------------------
// Helper: Render entities for a single scene section
//-----------------------------------------------------------------------------
static void RenderSceneEntities(
	Zenith_SceneData& xSceneData,
	Zenith_EntityID& uEntityToDelete,
	Zenith_EntityID& uDraggedEntityID,
	Zenith_EntityID& uDropTargetEntityID,
	Zenith_Scene& xDropTargetScene)
{
	// Render only root entities (entities without parents)
	const Zenith_Vector<Zenith_EntityID>& xActiveEntities = xSceneData.GetActiveEntities();
	for (u_int u = 0; u < xActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xEntityID = xActiveEntities.Get(u);
		if (xSceneData.EntityExists(xEntityID))
		{
			Zenith_Entity xEntity = xSceneData.GetEntity(xEntityID);
			if (!xEntity.HasParent())
			{
				RenderEntityTreeNode(xSceneData, xEntity, uEntityToDelete, uDraggedEntityID, uDropTargetEntityID, xDropTargetScene);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Main panel render function
//-----------------------------------------------------------------------------
void Render(Zenith_EntityID& uGameCameraEntityID)
{
	ImGui::Begin("Hierarchy");

	// Track entity to delete and drag/drop targets
	Zenith_EntityID uEntityToDelete = INVALID_ENTITY_ID;
	Zenith_EntityID uDraggedEntityID = INVALID_ENTITY_ID;
	Zenith_EntityID uDropTargetEntityID = INVALID_ENTITY_ID;
	Zenith_Scene xDropTargetScene;

	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_Scene xPersistentScene = Zenith_SceneManager::GetPersistentScene();

	// Iterate all loaded scenes and render each as a collapsible section
	uint32_t uSceneCount = Zenith_SceneManager::GetLoadedSceneCount();
	for (uint32_t i = 0; i < uSceneCount; ++i)
	{
		Zenith_Scene xScene = Zenith_SceneManager::GetSceneAt(i);
		if (!xScene.IsValid())
			continue;

		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xScene);
		if (!pxSceneData)
			continue;

		bool bIsActiveScene = (xScene == xActiveScene);
		bool bIsPersistentScene = (xScene == xPersistentScene);

		// Skip persistent scene if it has no entities
		if (bIsPersistentScene && pxSceneData->GetEntityCount() == 0)
			continue;

		// Build scene header text
		std::string strSceneName = bIsPersistentScene ? "DontDestroyOnLoad" : pxSceneData->GetName();
		if (strSceneName.empty())
			strSceneName = "Untitled";

		// Add dirty indicator
		if (xScene.HasUnsavedChanges())
			strSceneName += "*";

		// Add entity count
		strSceneName += " (" + std::to_string(pxSceneData->GetEntityCount()) + ")";

		// Active scene gets bold styling
		if (bIsActiveScene)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
		}

		ImGui::PushID(xScene.m_iHandle);

		ImGuiTreeNodeFlags eHeaderFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap;
		bool bHeaderOpen = ImGui::CollapsingHeader(strSceneName.c_str(), eHeaderFlags);

		ImGui::PopStyleColor();

		// Scene header drag-drop target (drop entity onto scene header to move it)
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY"))
			{
				Zenith_EntityID uSourceEntityID = *(const Zenith_EntityID*)pPayload->Data;
				// Move entity to this scene (unparent + cross-scene move)
				Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneDataForEntity(uSourceEntityID);
				if (pxSourceData && pxSourceData->EntityExists(uSourceEntityID))
				{
					Zenith_Entity xSourceEntity = pxSourceData->GetEntity(uSourceEntityID);
					// Unparent first if has parent
					if (xSourceEntity.HasParent())
					{
						xSourceEntity.SetParent(INVALID_ENTITY_ID);
					}
					// Move to target scene if different
					Zenith_Scene xSourceScene = xSourceEntity.GetScene();
					if (xSourceScene != xScene)
					{
						Zenith_SceneManager::MoveEntityToScene(xSourceEntity, xScene);
					}
				}
			}
			// Accept scene file drops from Content Browser for additive loading
			if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_FILE_GENERIC))
			{
				const DragDropFilePayload* pxFilePayload = (const DragDropFilePayload*)pPayload->Data;
				std::string strPath(pxFilePayload->m_szFilePath);
				// Check if it's a scene file
				if (strPath.ends_with(ZENITH_SCENE_EXT))
				{
					Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
				}
			}
			ImGui::EndDragDropTarget();
		}

		// Scene header context menu
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Set Active Scene", nullptr, false, !bIsActiveScene && !bIsPersistentScene))
			{
				Zenith_SceneManager::SetActiveScene(xScene);
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Save Scene", nullptr, false, !pxSceneData->GetPath().empty()))
			{
				pxSceneData->SaveToFile(pxSceneData->GetPath());
				Zenith_Log(LOG_CATEGORY_EDITOR, "Scene saved: %s", pxSceneData->GetPath().c_str());
			}

			if (ImGui::MenuItem("Save Scene As..."))
			{
#ifdef _WIN32
				std::string strFilePath = ::ShowSaveFileDialog(
					"Zenith Scene Files (*" ZENITH_SCENE_EXT ")\0*" ZENITH_SCENE_EXT "\0All Files (*.*)\0*.*\0",
					ZENITH_SCENE_EXT + 1,
					(pxSceneData->GetName() + ZENITH_SCENE_EXT).c_str());
				if (!strFilePath.empty())
				{
					pxSceneData->SaveToFile(strFilePath);
					Zenith_Log(LOG_CATEGORY_EDITOR, "Scene saved as: %s", strFilePath.c_str());
				}
#endif
			}

			ImGui::Separator();

			// Only allow unloading if not persistent scene and not the only loaded scene
			bool bCanUnload = !bIsPersistentScene && uSceneCount > 1;
			if (ImGui::MenuItem("Unload Scene", nullptr, false, bCanUnload))
			{
				// Clear selection for entities in this scene
				Zenith_Editor::ClearSelection();
				Zenith_SceneManager::UnloadScene(xScene);
				ImGui::EndPopup();
				ImGui::PopID();
				// Scene list changed, break out of iteration
				goto end_scene_loop;
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Create Empty Entity"))
			{
				Zenith_Entity xNewEntity(pxSceneData, "New Entity");
				xNewEntity.SetTransient(false);
				Zenith_Editor::SelectEntity(xNewEntity.GetEntityID());
			}

			if (!bIsPersistentScene)
			{
				bool bIsPaused = Zenith_SceneManager::IsScenePaused(xScene);
				if (ImGui::MenuItem(bIsPaused ? "Unpause Scene" : "Pause Scene"))
				{
					Zenith_SceneManager::SetScenePaused(xScene, !bIsPaused);
				}
			}

			ImGui::EndPopup();
		}

		// Render entities if header is open
		if (bHeaderOpen)
		{
			ImGui::Indent(4.0f);
			RenderSceneEntities(*pxSceneData, uEntityToDelete, uDraggedEntityID, uDropTargetEntityID, xDropTargetScene);
			ImGui::Unindent(4.0f);
		}

		ImGui::PopID();
	}

end_scene_loop:

	// Drop target for root level (unparent) - empty space at bottom
	ImGui::Dummy(ImVec2(0, 20));
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY"))
		{
			Zenith_EntityID xSourceEntityID = *(const Zenith_EntityID*)pPayload->Data;
			Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneDataForEntity(xSourceEntityID);
			if (pxSourceData && pxSourceData->EntityExists(xSourceEntityID))
			{
				pxSourceData->GetEntity(xSourceEntityID).SetParent(INVALID_ENTITY_ID);
			}
		}
		// Accept scene file drops for additive loading
		if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_FILE_GENERIC))
		{
			const DragDropFilePayload* pxFilePayload = (const DragDropFilePayload*)pPayload->Data;
			std::string strPath(pxFilePayload->m_szFilePath);
			if (strPath.ends_with(ZENITH_SCENE_EXT))
			{
				Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
			}
		}
		ImGui::EndDragDropTarget();
	}

	// Handle reparenting from drag-drop (within same scene)
	if (uDraggedEntityID.IsValid() && uDropTargetEntityID.IsValid())
	{
		Zenith_SceneData* pxDraggedData = Zenith_SceneManager::GetSceneDataForEntity(uDraggedEntityID);
		if (pxDraggedData && pxDraggedData->EntityExists(uDraggedEntityID) && uDraggedEntityID != uDropTargetEntityID)
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
				Zenith_SceneData* pxCheckData = Zenith_SceneManager::GetSceneDataForEntity(xCheckID);
				if (pxCheckData && pxCheckData->EntityExists(xCheckID))
				{
					xCheckID = pxCheckData->GetEntity(xCheckID).GetParentEntityID();
				}
				else
				{
					break;
				}
			}

			if (!bIsAncestor)
			{
				pxDraggedData->GetEntity(uDraggedEntityID).SetParent(uDropTargetEntityID);
			}
		}
	}

	// Perform deferred entity deletion
	if (uEntityToDelete != INVALID_ENTITY_ID)
	{
		if (uEntityToDelete == uGameCameraEntityID)
		{
			uGameCameraEntityID = INVALID_ENTITY_ID;
		}
		Zenith_SceneData* pxDeleteData = Zenith_SceneManager::GetSceneDataForEntity(uEntityToDelete);
		if (pxDeleteData)
		{
			pxDeleteData->RemoveEntity(uEntityToDelete);
		}
	}

	// Add button to create new entity (creates in active scene)
	ImGui::Separator();
	if (ImGui::Button("+ Create Entity"))
	{
		Zenith_Scene xCreateScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxCreateData = Zenith_SceneManager::GetSceneData(xCreateScene);
		if (pxCreateData)
		{
			Zenith_Entity xNewEntity(pxCreateData, "New Entity");
			xNewEntity.SetTransient(false);
			Zenith_Editor::SelectEntity(xNewEntity.GetEntityID());
		}
	}

	ImGui::End();
}

} // namespace Zenith_EditorPanelHierarchy

#endif // ZENITH_TOOLS
