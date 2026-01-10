#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_Hierarchy.h"
#include "Editor/Zenith_Editor.h"
#include "EntityComponent/Zenith_ComponentRegistry.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

//=============================================================================
// Hierarchy Panel Implementation
//
// Displays the scene entity hierarchy as a tree view.
//=============================================================================

namespace Zenith_EditorPanelHierarchy
{

//-----------------------------------------------------------------------------
// Helper: Render a single entity tree node recursively
//-----------------------------------------------------------------------------
static void RenderEntityTreeNode(
	Zenith_Scene& xScene,
	Zenith_Entity xEntity,
	Zenith_EntityID& uEntityToDelete,
	Zenith_EntityID& uDraggedEntityID,
	Zenith_EntityID& uDropTargetEntityID)
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

//-----------------------------------------------------------------------------
// Main panel render function
//-----------------------------------------------------------------------------
void Render(Zenith_Scene& xScene, Zenith_EntityID& uGameCameraEntityID)
{
	ImGui::Begin("Hierarchy");

	ImGui::Text("Scene Entities:");
	ImGui::Separator();

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
		if (uEntityToDelete == uGameCameraEntityID)
		{
			uGameCameraEntityID = INVALID_ENTITY_ID;
		}
		xScene.RemoveEntity(uEntityToDelete);
	}

	// Add button to create new entity
	ImGui::Separator();
	if (ImGui::Button("+ Create Entity"))
	{
		Zenith_Entity xNewEntity(&xScene, "New Entity");
		xNewEntity.SetTransient(false);  // Editor-created entities are persistent
		Zenith_Editor::SelectEntity(xNewEntity.GetEntityID());
	}

	ImGui::End();
}

} // namespace Zenith_EditorPanelHierarchy

#endif // ZENITH_TOOLS
