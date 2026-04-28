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
// IsAncestorOf - Check whether uCandidateAncestor appears in uTarget's
// parent chain. Returns false when candidate == target (self is not ancestor).
//-----------------------------------------------------------------------------
bool IsAncestorOf(Zenith_EntityID uCandidateAncestor, Zenith_EntityID uTarget)
{
	if (!uCandidateAncestor.IsValid() || !uTarget.IsValid())
	{
		return false;
	}
	if (uCandidateAncestor == uTarget)
	{
		return false;
	}

	Zenith_EntityID uCheckID = uTarget;
	while (uCheckID.IsValid())
	{
		Zenith_SceneData* pxCheckData = Zenith_SceneManager::GetSceneDataForEntity(uCheckID);
		if (!pxCheckData || !pxCheckData->EntityExists(uCheckID))
		{
			break;
		}
		Zenith_EntityID uParentID = pxCheckData->GetEntity(uCheckID).GetParentEntityID();
		if (uParentID == uCandidateAncestor)
		{
			return true;
		}
		uCheckID = uParentID;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Helper: Accept a scene-file drag-drop payload and load it additively.
// Call this between BeginDragDropTarget / EndDragDropTarget.
//-----------------------------------------------------------------------------
static void HandleSceneFileDragDrop()
{
	if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_FILE_GENERIC))
	{
		const DragDropFilePayload* pxFilePayload = (const DragDropFilePayload*)pPayload->Data;
		std::string strPath(pxFilePayload->m_szFilePath);
		if (strPath.ends_with(ZENITH_SCENE_EXT))
		{
			Zenith_SceneManager::LoadSceneBlocking_ToolsOnly(strPath, SCENE_LOAD_ADDITIVE);
		}
	}
}

//-----------------------------------------------------------------------------
// Helper: Render the right-click context menu for a single entity node.
// Called inside RenderEntityTreeNode after the tree node item is rendered.
//-----------------------------------------------------------------------------
static void RenderEntityContextMenu(
	Zenith_SceneData& xSceneData,
	Zenith_Entity xEntity,
	Zenith_EntityID& uEntityToDelete)
{
	if (!ImGui::BeginPopupContextItem())
	{
		return;
	}

	Zenith_EntityID uEntityID = xEntity.GetEntityID();

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
				// B5: MarkEntityPersistent is strict root-only. Walk up to the
				// hierarchy root from the right-clicked entity so the whole
				// subtree (root + descendants) gets promoted, matching the
				// user's "move this group to DontDestroyOnLoad" intent.
				Zenith_Entity xRoot = xEntity;
				Zenith_SceneData* pxRootScene = xRoot.GetSceneData();
				while (pxRootScene && xRoot.GetParentEntityID().IsValid())
				{
					Zenith_EntityID xParentID = xRoot.GetParentEntityID();
					if (!pxRootScene->EntityExists(xParentID)) break;
					xRoot = pxRootScene->GetEntity(xParentID);
					pxRootScene = xRoot.GetSceneData();
				}
				Zenith_SceneManager::MarkEntityPersistent(xRoot);
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

//-----------------------------------------------------------------------------
// Scratch state threaded through recursion so RenderEntityTreeNode has a thin
// signature instead of 5+ references.
//-----------------------------------------------------------------------------
struct TreeNodeRenderContext
{
	Zenith_SceneData& xSceneData;
	Zenith_EntityID& uEntityToDelete;
	Zenith_EntityID& uDraggedEntityID;
	Zenith_EntityID& uDropTargetEntityID;
};

struct EntityDisplayLabel
{
	std::string strDisplayName;
	std::string strComponentSummary;
	uint32_t uComponentCount = 0;
};

static EntityDisplayLabel BuildEntityDisplayLabel(Zenith_Entity xEntity)
{
	EntityDisplayLabel xLabel;
	Zenith_EntityID uEntityID = xEntity.GetEntityID();
	xLabel.strDisplayName = xEntity.GetName().empty()
		? ("Entity_" + std::to_string(uEntityID.m_uIndex))
		: xEntity.GetName();

	Zenith_ComponentRegistry& xRegistry = Zenith_ComponentRegistry::Get();
	const auto& xEntries = xRegistry.GetEntries();
	for (const Zenith_ComponentRegistryEntry& xEntry : xEntries)
	{
		if (xEntry.m_pfnHasComponent(xEntity))
		{
			if (xLabel.uComponentCount > 0)
				xLabel.strComponentSummary += ", ";
			xLabel.strComponentSummary += xEntry.m_strDisplayName;
			xLabel.uComponentCount++;
		}
	}

	if (xLabel.uComponentCount > 0)
	{
		xLabel.strDisplayName += " [" + std::to_string(xLabel.uComponentCount) + "]";
	}
	return xLabel;
}

static void HandleNodeSelection(Zenith_EntityID uEntityID)
{
	if (!ImGui::IsItemClicked() || ImGui::IsItemToggledOpen())
		return;

	const bool bCtrlHeld = ImGui::GetIO().KeyCtrl;
	const bool bShiftHeld = ImGui::GetIO().KeyShift;

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

static void HandleEntityDragDrop(
	Zenith_Entity xEntity,
	Zenith_EntityID& uDraggedEntityID,
	Zenith_EntityID& uDropTargetEntityID)
{
	Zenith_EntityID uEntityID = xEntity.GetEntityID();

	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
	{
		ImGui::SetDragDropPayload("HIERARCHY_ENTITY", &uEntityID, sizeof(Zenith_EntityID));
		ImGui::Text("Move: %s", xEntity.GetName().c_str());
		uDraggedEntityID = uEntityID;
		ImGui::EndDragDropSource();
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY"))
		{
			Zenith_EntityID uSourceEntityID = *(const Zenith_EntityID*)pPayload->Data;
			uDropTargetEntityID = uEntityID;
			uDraggedEntityID = uSourceEntityID;
		}
		ImGui::EndDragDropTarget();
	}
}

//-----------------------------------------------------------------------------
// Helper: Render a single entity tree node recursively
//-----------------------------------------------------------------------------
static void RenderEntityTreeNode(Zenith_Entity xEntity, TreeNodeRenderContext& xCtx)
{
	Zenith_EntityID uEntityID = xEntity.GetEntityID();
	bool bIsSelected = Zenith_Editor::IsSelected(uEntityID);
	bool bHasChildren = xEntity.HasChildren();

	EntityDisplayLabel xLabel = BuildEntityDisplayLabel(xEntity);

	ImGuiTreeNodeFlags eFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (bIsSelected)
	{
		eFlags |= ImGuiTreeNodeFlags_Selected;
	}
	if (!bHasChildren)
	{
		eFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	bool bNodeOpen = ImGui::TreeNodeEx((void*)(uintptr_t)uEntityID.GetPacked(), eFlags, "%s", xLabel.strDisplayName.c_str());

	HandleNodeSelection(uEntityID);
	HandleEntityDragDrop(xEntity, xCtx.uDraggedEntityID, xCtx.uDropTargetEntityID);

	if (ImGui::IsItemHovered() && xLabel.uComponentCount > 0)
	{
		ImGui::SetTooltip("Components: %s", xLabel.strComponentSummary.c_str());
	}

	RenderEntityContextMenu(xCtx.xSceneData, xEntity, xCtx.uEntityToDelete);

	if (bNodeOpen && bHasChildren)
	{
		Zenith_Vector<Zenith_EntityID> xChildren = xEntity.GetChildEntityIDs();
		for (u_int u = 0; u < xChildren.GetSize(); ++u)
		{
			Zenith_EntityID xChildID = xChildren.Get(u);
			if (xCtx.xSceneData.EntityExists(xChildID))
			{
				RenderEntityTreeNode(xCtx.xSceneData.GetEntity(xChildID), xCtx);
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
	Zenith_EntityID& uDropTargetEntityID)
{
	TreeNodeRenderContext xCtx{ xSceneData, uEntityToDelete, uDraggedEntityID, uDropTargetEntityID };

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
				RenderEntityTreeNode(xEntity, xCtx);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Right-click context menu attached to a scene collapsing header.
// Offers Set Active, Save, Save As..., Unload, Create Entity and Pause.
// Sets bSceneUnloaded when the user picks "Unload Scene" so the caller knows
// the scene list has been mutated mid-iteration and must break.
//-----------------------------------------------------------------------------
static void RenderSceneContextMenu(
	Zenith_Scene xScene,
	Zenith_SceneData& xSceneData,
	bool bIsActiveScene,
	bool bIsPersistentScene,
	uint32_t uSceneCount,
	bool& bSceneUnloaded)
{
	if (!ImGui::BeginPopupContextItem())
		return;

	if (ImGui::MenuItem("Set Active Scene", nullptr, false, !bIsActiveScene && !bIsPersistentScene))
	{
		Zenith_SceneManager::SetActiveScene(xScene);
	}

	ImGui::Separator();

	if (ImGui::MenuItem("Save Scene", nullptr, false, !xSceneData.GetPath().empty()))
	{
		xSceneData.SaveToFile(xSceneData.GetPath());
		Zenith_Log(LOG_CATEGORY_EDITOR, "Scene saved: %s", xSceneData.GetPath().c_str());
	}

	if (ImGui::MenuItem("Save Scene As..."))
	{
#ifdef _WIN32
		std::string strFilePath = ::ShowSaveFileDialog(
			"Zenith Scene Files (*" ZENITH_SCENE_EXT ")\0*" ZENITH_SCENE_EXT "\0All Files (*.*)\0*.*\0",
			ZENITH_SCENE_EXT + 1,
			(xSceneData.GetName() + ZENITH_SCENE_EXT).c_str());
		if (!strFilePath.empty())
		{
			xSceneData.SaveToFile(strFilePath);
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
		// Scene list changed, signal caller to break out of iteration
		bSceneUnloaded = true;
		return;
	}

	ImGui::Separator();

	if (ImGui::MenuItem("Create Empty Entity"))
	{
		Zenith_Editor::CreateEntity("New Entity");
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

//-----------------------------------------------------------------------------
// Render a single scene's header: styling, collapsing header, drag-drop target
// and the context menu. Returns true via bSceneUnloaded if the scene list was
// mutated (scene unloaded) so the caller can break out of iteration safely.
//
// The PushID() / PopID() pair wrapping this call ensures per-scene imgui IDs
// do not collide across scenes.
//-----------------------------------------------------------------------------
static void RenderSceneHeaderAndBody(
	Zenith_Scene xScene,
	Zenith_SceneData& xSceneData,
	bool bIsActiveScene,
	bool bIsPersistentScene,
	uint32_t uSceneCount,
	Zenith_EntityID& uEntityToDelete,
	Zenith_EntityID& uDraggedEntityID,
	Zenith_EntityID& uDropTargetEntityID,
	bool& bSceneUnloaded)
{
	// Build scene header text
	std::string strSceneName = bIsPersistentScene ? "DontDestroyOnLoad" : xSceneData.GetName();
	if (strSceneName.empty())
		strSceneName = "Untitled";

	// Add dirty indicator
	if (xScene.HasUnsavedChanges())
		strSceneName += "*";

	// Add entity count
	strSceneName += " (" + std::to_string(xSceneData.GetEntityCount()) + ")";

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
		HandleSceneFileDragDrop();
		ImGui::EndDragDropTarget();
	}

	// Scene header context menu (Set Active / Save / Unload / Create / Pause)
	RenderSceneContextMenu(
		xScene,
		xSceneData,
		bIsActiveScene,
		bIsPersistentScene,
		uSceneCount,
		bSceneUnloaded);

	if (bSceneUnloaded)
	{
		ImGui::PopID();
		return;
	}

	// Render entities if header is open
	if (bHeaderOpen)
	{
		ImGui::Indent(4.0f);
		RenderSceneEntities(xSceneData, uEntityToDelete, uDraggedEntityID, uDropTargetEntityID);
		ImGui::Unindent(4.0f);
	}

	ImGui::PopID();
}

//-----------------------------------------------------------------------------
// Iterate all loaded scenes, rendering each as a collapsible section.
// Writes drag/drop scratch state into the supplied references so the caller
// can apply deferred reparenting and deletion after the loop.
//-----------------------------------------------------------------------------
void RenderScenesSection(
	Zenith_EntityID& uEntityToDelete,
	Zenith_EntityID& uDraggedEntityID,
	Zenith_EntityID& uDropTargetEntityID)
{
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_Scene xPersistentScene = Zenith_SceneManager::GetPersistentScene();

	// Flag to break out of scene loop (replaces goto)
	bool bSceneUnloaded = false;

	uint32_t uSceneCount = Zenith_SceneManager::GetLoadedSceneCount();
	for (uint32_t i = 0; i < uSceneCount && !bSceneUnloaded; ++i)
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

		RenderSceneHeaderAndBody(
			xScene,
			*pxSceneData,
			bIsActiveScene,
			bIsPersistentScene,
			uSceneCount,
			uEntityToDelete,
			uDraggedEntityID,
			uDropTargetEntityID,
			bSceneUnloaded);
	}
}

//-----------------------------------------------------------------------------
// Drop target for root level (unparent) - empty space at bottom of panel.
// Also accepts scene file drops from the Content Browser for additive loading.
//-----------------------------------------------------------------------------
void RenderRootDropTargetSection()
{
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
		HandleSceneFileDragDrop();
		ImGui::EndDragDropTarget();
	}
}

//-----------------------------------------------------------------------------
// Apply a drag-drop reparent operation captured by RenderScenesSection.
// Skips reparenting that would introduce a cycle in the hierarchy.
//-----------------------------------------------------------------------------
void ProcessDeferredReparenting(
	Zenith_EntityID uDraggedEntityID,
	Zenith_EntityID uDropTargetEntityID)
{
	if (!uDraggedEntityID.IsValid() || !uDropTargetEntityID.IsValid())
		return;

	Zenith_SceneData* pxDraggedData = Zenith_SceneManager::GetSceneDataForEntity(uDraggedEntityID);
	if (pxDraggedData && pxDraggedData->EntityExists(uDraggedEntityID) && uDraggedEntityID != uDropTargetEntityID)
	{
		if (!IsAncestorOf(uDraggedEntityID, uDropTargetEntityID))
		{
			pxDraggedData->GetEntity(uDraggedEntityID).SetParent(uDropTargetEntityID);
		}
	}
}

//-----------------------------------------------------------------------------
// Destroy the entity flagged for deletion via the right-click context menu.
// Clears the game camera reference if the deleted entity was the camera.
//-----------------------------------------------------------------------------
void ProcessDeferredEntityDeletion(
	Zenith_EntityID uEntityToDelete,
	Zenith_EntityID& uGameCameraEntityID)
{
	if (uEntityToDelete == INVALID_ENTITY_ID)
		return;

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

//-----------------------------------------------------------------------------
// Render the bottom separator and the "+ Create Entity" button that creates
// a new entity in the active scene.
//-----------------------------------------------------------------------------
void RenderCreateEntityFooter()
{
	ImGui::Separator();
	if (ImGui::Button("+ Create Entity"))
	{
		Zenith_Editor::CreateEntity("New Entity");
	}
}

//-----------------------------------------------------------------------------
// Main panel render function. Thin dispatcher that delegates to per-section
// helpers so each section stays focused on its own concern.
//-----------------------------------------------------------------------------
void Render(Zenith_EntityID& uGameCameraEntityID)
{
	ImGui::Begin("Hierarchy");

	// Track entity to delete and drag/drop targets
	Zenith_EntityID uEntityToDelete = INVALID_ENTITY_ID;
	Zenith_EntityID uDraggedEntityID = INVALID_ENTITY_ID;
	Zenith_EntityID uDropTargetEntityID = INVALID_ENTITY_ID;

	RenderScenesSection(uEntityToDelete, uDraggedEntityID, uDropTargetEntityID);

	RenderRootDropTargetSection();

	ProcessDeferredReparenting(uDraggedEntityID, uDropTargetEntityID);

	ProcessDeferredEntityDeletion(uEntityToDelete, uGameCameraEntityID);

	RenderCreateEntityFooter();

	ImGui::End();
}

} // namespace Zenith_EditorPanelHierarchy

#endif // ZENITH_TOOLS
