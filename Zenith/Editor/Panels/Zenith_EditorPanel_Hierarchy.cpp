#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_Hierarchy.h"
#include "Core/Zenith_EditorWindowNames.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorActions.h"
#include "Editor/Zenith_EditorSceneAccess.h"
#include "Editor/Zenith_EditorUI.h"
#include "EntityComponent/Zenith_ComponentEditorRegistry.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "FileAccess/Zenith_FileAccess.h"

#include "imgui.h"

#include <cctype>
#include <cstring>

//=============================================================================
// Hierarchy Panel Implementation (Unity-Style Multi-Scene)
//=============================================================================

namespace Zenith_EditorPanelHierarchy
{

//-----------------------------------------------------------------------------
// IsAncestorOf - Check whether uCandidateAncestor appears in uTarget's
// parent chain. Returns false when candidate == target (self is not ancestor).
//-----------------------------------------------------------------------------
bool IsAncestorOf(Zenith_EntityID uCandidateAncestor, Zenith_EntityID uTarget)
{
	if (!uCandidateAncestor.IsValid() || !uTarget.IsValid() || uCandidateAncestor == uTarget)
	{
		return false;
	}
	Zenith_Entity xTarget = g_xEngine.Scenes().ResolveEntity(uTarget);
	return xTarget.IsValid() && xTarget.IsDescendantOf(uCandidateAncestor);
}

//-----------------------------------------------------------------------------
// MatchesSearch — case-insensitive substring over name + component names.
//-----------------------------------------------------------------------------
bool MatchesSearch(const char* szEntityName, const char* szComponentSummary, const char* szQuery)
{
	if (szQuery == nullptr || szQuery[0] == '\0') return true;
	return Zenith_EditorUI::ContainsCaseInsensitive(szEntityName, szQuery) || Zenith_EditorUI::ContainsCaseInsensitive(szComponentSummary, szQuery);
}

void BeginRename(Zenith_EditorHierarchyState& xState, Zenith_EntityID xEntity)
{
	Zenith_Entity xResolved = g_xEngine.Scenes().ResolveEntity(xEntity);
	if (!xResolved.IsValid())
	{
		return;
	}
	xState.m_xRenaming = xEntity;
	strncpy_s(xState.m_szRenameBuffer, sizeof(xState.m_szRenameBuffer), xResolved.GetName().c_str(), _TRUNCATE);
	xState.m_bRenameFocusPending = true;
}

//-----------------------------------------------------------------------------
// Per-frame scratch threaded through the recursion.
//-----------------------------------------------------------------------------
namespace
{
	struct TreeContext
	{
		Zenith_EditorHierarchyState& m_xState;
		Zenith_SceneData& m_xSceneData;
		Zenith_EntityID& m_uEntityToDelete;
		Zenith_EntityID& m_uDraggedEntityID;
		Zenith_EntityID& m_uDropTargetEntityID;
		const char* m_szQuery;
	};

	struct EntityDisplayInfo
	{
		std::string m_strComponentSummary;
		Zenith_EditorIcon m_eIcon = Zenith_EditorIcon::Entity;
		ImU32 m_uIconColour = 0;
		u_int m_uComponentCount = 0;
	};

	// The icon an entity shows in the tree: its most "characteristic" component
	// (a camera is a camera even though it also has a transform).
	void PickEntityIcon(const std::string& strDisplayName, EntityDisplayInfo& xInfo, int& iBestRank)
	{
		struct Row { const char* m_szName; Zenith_EditorIcon m_eIcon; int m_iRank; };
		static const Row axRows[] =
		{
			{ "Camera",          Zenith_EditorIcon::Camera,     10 },
			{ "Sun",             Zenith_EditorIcon::Sun,        9 },
			{ "Light",           Zenith_EditorIcon::Light,      9 },
			{ "Terrain",         Zenith_EditorIcon::Terrain,    8 },
			{ "Model",           Zenith_EditorIcon::Mesh,       7 },
			{ "InstancedMesh",   Zenith_EditorIcon::Mesh,       7 },
			{ "ParticleEmitter", Zenith_EditorIcon::Particles,  6 },
			{ "UI",              Zenith_EditorIcon::UI,         5 },
			{ "Graph",           Zenith_EditorIcon::Graph,      4 },
			{ "AIAgent",         Zenith_EditorIcon::Graph,      3 },
			{ "Collider",        Zenith_EditorIcon::Collider,   2 },
			{ "NavMesh",         Zenith_EditorIcon::Terrain,    2 },
			{ "Animator",        Zenith_EditorIcon::Animation,  2 },
		};
		for (const Row& xRow : axRows)
		{
			if (strDisplayName == xRow.m_szName && xRow.m_iRank > iBestRank)
			{
				iBestRank = xRow.m_iRank;
				xInfo.m_eIcon = xRow.m_eIcon;
			}
		}
	}

	EntityDisplayInfo BuildDisplayInfo(Zenith_Entity xEntity)
	{
		EntityDisplayInfo xInfo;
		const Zenith_EditorPalette& xP = Zenith_EditorUI::Palette();
		xInfo.m_uIconColour = xP.m_uTextDim;
		int iBestRank = -1;

		const Zenith_Vector<Zenith_ComponentEditorRegistryEntry>& xEntries = Zenith_ComponentEditorRegistry::Get().GetEntries();
		for (u_int u = 0; u < xEntries.GetSize(); ++u)
		{
			const Zenith_ComponentEditorRegistryEntry& xEntry = xEntries.Get(u);
			if (!xEntry.m_pfnHasComponent(xEntity))
			{
				continue;
			}
			if (xInfo.m_uComponentCount > 0)
			{
				xInfo.m_strComponentSummary += ", ";
			}
			xInfo.m_strComponentSummary += xEntry.m_strDisplayName;
			++xInfo.m_uComponentCount;
			PickEntityIcon(xEntry.m_strDisplayName, xInfo, iBestRank);
		}
		if (iBestRank >= 0)
		{
			xInfo.m_uIconColour = xP.m_uAccentHover;
		}
		return xInfo;
	}

	// True when the entity or any descendant matches the query.
	bool SubtreeMatches(Zenith_SceneData& xSceneData, Zenith_Entity xEntity, const char* szQuery)
	{
		if (szQuery == nullptr || szQuery[0] == '\0')
		{
			return true;
		}
		const EntityDisplayInfo xInfo = BuildDisplayInfo(xEntity);
		if (MatchesSearch(xEntity.GetName().c_str(), xInfo.m_strComponentSummary.c_str(), szQuery))
		{
			return true;
		}
		const Zenith_Vector<Zenith_EntityID>& xChildren = xEntity.GetChildEntityIDs();
		for (u_int u = 0; u < xChildren.GetSize(); ++u)
		{
			if (xSceneData.EntityExists(xChildren.Get(u)) && SubtreeMatches(xSceneData, xSceneData.GetEntity(xChildren.Get(u)), szQuery))
			{
				return true;
			}
		}
		return false;
	}

	//-------------------------------------------------------------------------
	// Selection
	//-------------------------------------------------------------------------
	void HandleNodeSelection(Zenith_EntityID uEntityID)
	{
		if (!ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemToggledOpen())
			return;

		const bool bCtrlHeld = ImGui::GetIO().KeyCtrl;
		const bool bShiftHeld = ImGui::GetIO().KeyShift;
		Zenith_Editor& xEditor = g_xEngine.Editor();

		if (bShiftHeld && xEditor.GetLastClickedEntityID() != INVALID_ENTITY_ID)
		{
			xEditor.SelectRange(uEntityID);
		}
		else if (bCtrlHeld)
		{
			xEditor.ToggleEntitySelection(uEntityID);
		}
		else
		{
			xEditor.SelectEntity(uEntityID, false);
		}
	}

	//-------------------------------------------------------------------------
	// Drag-drop: source = the entity row; target = another row (reparent).
	//-------------------------------------------------------------------------
	void HandleEntityDragDrop(Zenith_Entity xEntity, TreeContext& xCtx)
	{
		Zenith_EntityID uEntityID = xEntity.GetEntityID();

		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
		{
			ImGui::SetDragDropPayload("HIERARCHY_ENTITY", &uEntityID, sizeof(Zenith_EntityID));
			Zenith_EditorUI::IconLabel(Zenith_EditorIcon::Entity, xEntity.GetName().c_str());
			xCtx.m_uDraggedEntityID = uEntityID;
			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY"))
			{
				xCtx.m_uDropTargetEntityID = uEntityID;
				xCtx.m_uDraggedEntityID = *(const Zenith_EntityID*)pPayload->Data;
			}
			ImGui::EndDragDropTarget();
		}
	}

	//-------------------------------------------------------------------------
	// Context menu
	//-------------------------------------------------------------------------
	void RenderCreateChildItems(Zenith_EntityID uParent)
	{
		using Zenith_EditorActions::CreateKind;
		if (ImGui::MenuItem("Empty"))             Zenith_EditorActions::CreateEntity(CreateKind::Empty, uParent);
		if (ImGui::MenuItem("Camera"))            Zenith_EditorActions::CreateEntity(CreateKind::Camera, uParent);
		if (ImGui::MenuItem("Point Light"))       Zenith_EditorActions::CreateEntity(CreateKind::PointLight, uParent);
		if (ImGui::MenuItem("Spot Light"))        Zenith_EditorActions::CreateEntity(CreateKind::SpotLight, uParent);
		if (ImGui::MenuItem("Directional Light")) Zenith_EditorActions::CreateEntity(CreateKind::DirectionalLight, uParent);
	}

	// Cross-scene moves: "Move To Scene" submenu listing every other loaded
	// scene + an explicit "Move to DontDestroyOnLoad" entry. Only meaningful
	// for root entities — children move with their parent's scene.
	void RenderContextMenu_MoveToScene(Zenith_Entity xEntity)
	{
		Zenith_Scene xEntityScene = xEntity.GetScene();

		if (ImGui::BeginMenu("Move To Scene"))
		{
			for (uint32_t i = 0; ; ++i)
			{
				Zenith_Scene xScene = g_xEngine.Scenes().GetSceneAt(i);
				if (!xScene.IsValid())
					break;
				if (xScene == xEntityScene)
					continue;

				const Zenith_SceneInfo xInfo = g_xEngine.Scenes().GetSceneInfo(xScene);
				if (ImGui::MenuItem(xInfo.m_strName.c_str()))
				{
					xEntity.MoveToScene(xScene);
				}
			}
			ImGui::EndMenu();
		}

		Zenith_Scene xPersistentScene = g_xEngine.Scenes().GetPersistentScene();
		if (xEntityScene != xPersistentScene && ImGui::MenuItem("Move to DontDestroyOnLoad"))
		{
			// MarkEntityPersistent is strict root-only. Walk up to the hierarchy
			// root so the whole subtree gets promoted.
			Zenith_Entity xRoot = xEntity;
			Zenith_SceneData* pxRootScene = xRoot.GetSceneData();
			while (pxRootScene && xRoot.GetParentEntityID().IsValid())
			{
				Zenith_EntityID xParentID = xRoot.GetParentEntityID();
				if (!pxRootScene->EntityExists(xParentID)) break;
				xRoot = pxRootScene->GetEntity(xParentID);
				pxRootScene = xRoot.GetSceneData();
			}
			xRoot.DontDestroyOnLoad();
		}
	}

	void RenderEntityContextMenu(Zenith_Entity xEntity, TreeContext& xCtx)
	{
		if (!ImGui::BeginPopupContextItem())
		{
			return;
		}
		const Zenith_EntityID uEntityID = xEntity.GetEntityID();
		Zenith_Editor& xEditor = g_xEngine.Editor();
		// A right-click on an unselected row acts on that row.
		if (!xEditor.IsSelected(uEntityID))
		{
			xEditor.SelectEntity(uEntityID, false);
		}

		if (ImGui::MenuItem("Rename", "F2"))       BeginRename(xCtx.m_xState, uEntityID);
		if (ImGui::MenuItem("Duplicate", "Ctrl+D")) Zenith_EditorActions::DuplicateSelection();
		if (ImGui::MenuItem("Delete", "Del"))       xCtx.m_uEntityToDelete = uEntityID;
		if (ImGui::MenuItem("Focus", "F"))          Zenith_EditorActions::FocusSelection();

		ImGui::Separator();
		if (ImGui::BeginMenu("Create Child"))
		{
			RenderCreateChildItems(uEntityID);
			ImGui::EndMenu();
		}
		if (xEntity.HasParent() && ImGui::MenuItem("Unparent"))
		{
			Zenith_EditorActions::ReparentEntity(uEntityID, INVALID_ENTITY_ID);
		}
		if (ImGui::MenuItem(xEntity.IsEnabled() ? "Disable" : "Enable"))
		{
			Zenith_EditorActions::SetEntityEnabled(uEntityID, !xEntity.IsEnabled());
		}

		if (!xEntity.HasParent())
		{
			ImGui::Separator();
			RenderContextMenu_MoveToScene(xEntity);
		}
		ImGui::EndPopup();
	}

	//-------------------------------------------------------------------------
	// Row decorations: icon on the left, enabled-eye on the right.
	//-------------------------------------------------------------------------
	void DrawRowIcon(const ImVec2& xRowMin, float fRowHeight, const EntityDisplayInfo& xInfo)
	{
		const float fArrowSpace = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.x * 2.0f;
		const float fIcon = ImGui::GetFontSize();
		Zenith_EditorUI::DrawIcon(ImGui::GetWindowDrawList(), xInfo.m_eIcon,
			ImVec2(xRowMin.x + fArrowSpace + fIcon * 0.5f, xRowMin.y + fRowHeight * 0.5f), fIcon * 0.85f, xInfo.m_uIconColour);
	}

	// The eye at the right edge of a row. Drawn into the draw list and hit-tested
	// by hand (no ImGui item) so the row stays the only item and the cursor is
	// never moved. Returns true when the eye is hovered, so the caller can keep
	// the row's own click from selecting on the same press.
	bool DrawEnabledToggle(Zenith_Entity xEntity, const ImVec2& xRowMin, const ImVec2& xRowMax, bool bRowHovered)
	{
		const bool bEnabled = xEntity.IsEnabled();
		if (bEnabled && !bRowHovered)
		{
			return false;
		}
		const Zenith_EditorPalette& xP = Zenith_EditorUI::Palette();
		const float fSize = xRowMax.y - xRowMin.y - Zenith_EditorUI::Px(2.0f);
		const ImVec2 xMin(xRowMax.x - fSize - Zenith_EditorUI::Px(4.0f), xRowMin.y + Zenith_EditorUI::Px(1.0f));
		const ImVec2 xMax(xMin.x + fSize, xMin.y + fSize);
		const bool bHovered = bRowHovered && ImGui::IsMouseHoveringRect(xMin, xMax);
		ImDrawList* pxDraw = ImGui::GetWindowDrawList();
		if (bHovered)
		{
			pxDraw->AddRectFilled(xMin, xMax, xP.m_uFrameActive, ImGui::GetStyle().FrameRounding);
		}
		Zenith_EditorUI::DrawIcon(pxDraw, bEnabled ? Zenith_EditorIcon::Eye : Zenith_EditorIcon::EyeOff,
			ImVec2((xMin.x + xMax.x) * 0.5f, (xMin.y + xMax.y) * 0.5f), fSize * 0.62f,
			bEnabled ? (bHovered ? xP.m_uTextBright : xP.m_uTextDim) : xP.m_uWarning);
		if (bHovered)
		{
			ImGui::SetTooltip(bEnabled ? "Disable entity" : "Enable entity");
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				Zenith_EditorActions::SetEntityEnabled(xEntity.GetEntityID(), !bEnabled);
			}
		}
		return bHovered;
	}

	// The inline rename field replaces the row; commits on Enter or focus loss.
	void RenderRenameField(Zenith_Entity xEntity, TreeContext& xCtx)
	{
		Zenith_EditorHierarchyState& xState = xCtx.m_xState;
		if (xState.m_bRenameFocusPending)
		{
			ImGui::SetKeyboardFocusHere();
			xState.m_bRenameFocusPending = false;
		}
		ImGui::SetNextItemWidth(-FLT_MIN);
		const bool bCommit = ImGui::InputText("##rename", xState.m_szRenameBuffer, sizeof(xState.m_szRenameBuffer),
			ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
		const bool bCancelled = ImGui::IsKeyPressed(ImGuiKey_Escape);
		if (bCommit || (ImGui::IsItemDeactivated() && !bCancelled))
		{
			Zenith_EditorActions::RenameEntity(xEntity.GetEntityID(), xState.m_szRenameBuffer);
			xState.m_xRenaming = INVALID_ENTITY_ID;
		}
		else if (bCancelled || (ImGui::IsItemDeactivated()))
		{
			xState.m_xRenaming = INVALID_ENTITY_ID;
		}
	}

	//-------------------------------------------------------------------------
	// One entity row, recursing into children.
	//-------------------------------------------------------------------------
	void RenderEntityTreeNode(Zenith_Entity xEntity, TreeContext& xCtx)
	{
		Zenith_EditorHierarchyState& xState = xCtx.m_xState;
		const Zenith_EntityID uEntityID = xEntity.GetEntityID();
		const bool bFiltering = (xCtx.m_szQuery[0] != '\0');
		if (bFiltering && !SubtreeMatches(xCtx.m_xSceneData, xEntity, xCtx.m_szQuery))
		{
			return;
		}

		Zenith_Editor& xEditor = g_xEngine.Editor();
		const bool bIsSelected = xEditor.IsSelected(uEntityID);
		const bool bHasChildren = xEntity.HasChildren();
		const EntityDisplayInfo xInfo = BuildDisplayInfo(xEntity);

		ImGuiTreeNodeFlags eFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_OpenOnDoubleClick;
		if (bIsSelected) eFlags |= ImGuiTreeNodeFlags_Selected;
		if (!bHasChildren) eFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		if (bFiltering && bHasChildren) ImGui::SetNextItemOpen(true);

		ImGui::PushID(static_cast<int>(uEntityID.GetPacked()));

		if (xState.m_xScrollTo == uEntityID)
		{
			ImGui::SetScrollHereY(0.5f);
			xState.m_xScrollTo = INVALID_ENTITY_ID;
		}

		const bool bRenaming = (xState.m_xRenaming == uEntityID);
		bool bNodeOpen = false;
		if (bRenaming)
		{
			// Keep the arrow + indent; the label becomes the text field.
			bNodeOpen = ImGui::TreeNodeEx("##row", eFlags, "%s", "");
			ImGui::SameLine(ImGui::GetFontSize() * 2.6f);
			RenderRenameField(xEntity, xCtx);
		}
		else
		{
			const bool bDimmed = !xEntity.IsEnabled();
			if (bDimmed) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
			// The leading spaces reserve room for the icon.
			bNodeOpen = ImGui::TreeNodeEx("##row", eFlags, "      %s", xEntity.GetName().empty() ? "Entity" : xEntity.GetName().c_str());
			if (bDimmed) ImGui::PopStyleColor();

			const ImVec2 xRowMin = ImGui::GetItemRectMin();
			const ImVec2 xRowMax = ImGui::GetItemRectMax();
			const bool bRowHovered = ImGui::IsItemHovered();

			// Decorations first: the eye's hit test decides whether the click was
			// for the row or for the toggle.
			DrawRowIcon(xRowMin, xRowMax.y - xRowMin.y, xInfo);
			const bool bEyeHovered = DrawEnabledToggle(xEntity, xRowMin, xRowMax, bRowHovered);

			if (!bEyeHovered)
			{
				HandleNodeSelection(uEntityID);
				if (bRowHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
				{
					BeginRename(xState, uEntityID);
				}
			}
			HandleEntityDragDrop(xEntity, xCtx);
			if (bRowHovered && !bEyeHovered && xInfo.m_uComponentCount > 0 && !ImGui::IsMouseDragging(ImGuiMouseButton_Left))
			{
				ImGui::SetTooltip("%s", xInfo.m_strComponentSummary.c_str());
			}
			RenderEntityContextMenu(xEntity, xCtx);
		}

		if (bNodeOpen && bHasChildren)
		{
			// Copy: a child may be deleted/reparented by a nested context menu.
			const Zenith_Vector<Zenith_EntityID> xChildren = xEntity.GetChildEntityIDs();
			for (u_int u = 0; u < xChildren.GetSize(); ++u)
			{
				const Zenith_EntityID xChildID = xChildren.Get(u);
				if (xCtx.m_xSceneData.EntityExists(xChildID))
				{
					RenderEntityTreeNode(xCtx.m_xSceneData.GetEntity(xChildID), xCtx);
				}
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	void RenderSceneEntities(TreeContext& xCtx)
	{
		const Zenith_Vector<Zenith_EntityID>& xActiveEntities = xCtx.m_xSceneData.GetActiveEntities();
		for (u_int u = 0; u < xActiveEntities.GetSize(); ++u)
		{
			const Zenith_EntityID xEntityID = xActiveEntities.Get(u);
			if (xCtx.m_xSceneData.EntityExists(xEntityID))
			{
				Zenith_Entity xEntity = xCtx.m_xSceneData.GetEntity(xEntityID);
				if (!xEntity.HasParent())
				{
					RenderEntityTreeNode(xEntity, xCtx);
				}
			}
		}
	}

	//-------------------------------------------------------------------------
	// Scene header + context menu
	//-------------------------------------------------------------------------
	void HandleSceneFileDragDrop()
	{
		if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_FILE_GENERIC))
		{
			const DragDropFilePayload* pxFilePayload = (const DragDropFilePayload*)pPayload->Data;
			std::string strPath(pxFilePayload->m_szFilePath);
			if (strPath.ends_with(ZENITH_SCENE_EXT))
			{
				g_xEngine.Scenes().LoadScene(strPath, SCENE_LOAD_ADDITIVE);
			}
		}
	}

	// Drop an entity onto a scene header: unparent it and move it into that scene.
	void HandleSceneHeaderDrop(Zenith_Scene xScene)
	{
		if (!ImGui::BeginDragDropTarget())
		{
			return;
		}
		if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY"))
		{
			const Zenith_EntityID uSourceEntityID = *(const Zenith_EntityID*)pPayload->Data;
			Zenith_Entity xSourceEntity = g_xEngine.Scenes().ResolveEntity(uSourceEntityID);
			if (xSourceEntity.IsValid())
			{
				if (xSourceEntity.HasParent())
				{
					Zenith_EditorActions::ReparentEntity(uSourceEntityID, INVALID_ENTITY_ID);
				}
				if (xSourceEntity.GetScene() != xScene)
				{
					xSourceEntity.MoveToScene(xScene);
				}
			}
		}
		HandleSceneFileDragDrop();
		ImGui::EndDragDropTarget();
	}

	// Returns true when the scene list was mutated (scene unloaded) so the
	// caller must stop iterating.
	bool RenderSceneContextMenu(Zenith_Scene xScene, Zenith_SceneData& xSceneData, bool bIsActiveScene, bool bIsPersistentScene, uint32_t uSceneCount)
	{
		if (!ImGui::BeginPopupContextItem())
			return false;

		const Zenith_SceneInfo xSceneInfo = g_xEngine.Scenes().GetSceneInfo(xScene);
		bool bUnloaded = false;

		if (ImGui::MenuItem("Set Active Scene", nullptr, false, !bIsActiveScene && !bIsPersistentScene))
		{
			g_xEngine.Scenes().SetActiveScene(xScene);
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Save Scene", "Ctrl+S", false, !xSceneInfo.m_strPath.empty()))
		{
			Zenith_EditorSceneAccess::SaveToFile(&xSceneData, xSceneInfo.m_strPath);
			Zenith_Log(LOG_CATEGORY_EDITOR, "Scene saved: %s", xSceneInfo.m_strPath.c_str());
		}
		if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S"))
		{
			g_xEngine.Scenes().SetActiveScene(xScene);
			Zenith_EditorActions::SaveSceneAs();
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Unload Scene", nullptr, false, !bIsPersistentScene && uSceneCount > 1))
		{
			g_xEngine.Editor().ClearSelection();
			g_xEngine.Scenes().UnloadScene(xScene);
			bUnloaded = true;
		}
		ImGui::Separator();
		if (ImGui::BeginMenu("Create"))
		{
			g_xEngine.Scenes().SetActiveScene(xScene);
			RenderCreateChildItems(INVALID_ENTITY_ID);
			ImGui::EndMenu();
		}
		if (!bIsPersistentScene)
		{
			const bool bIsPaused = g_xEngine.Scenes().IsScenePaused(xScene);
			if (ImGui::MenuItem(bIsPaused ? "Unpause Scene" : "Pause Scene"))
			{
				g_xEngine.Scenes().SetScenePaused(xScene, !bIsPaused);
			}
		}
		ImGui::EndPopup();
		return bUnloaded;
	}

	struct SceneRowContext
	{
		Zenith_Scene m_xScene;
		Zenith_SceneData* m_pxSceneData;
		bool m_bIsActiveScene;
		bool m_bIsPersistentScene;
		uint32_t m_uSceneCount;
	};

	// Renders one scene's header and, when open, its entity tree. Returns true
	// when the scene list was mutated.
	bool RenderSceneRow(const SceneRowContext& xRow, TreeContext& xCtx)
	{
		const Zenith_EditorPalette& xP = Zenith_EditorUI::Palette();
		const Zenith_SceneInfo xSceneInfo = g_xEngine.Scenes().GetSceneInfo(xRow.m_xScene);
		std::string strSceneName = xRow.m_bIsPersistentScene ? "DontDestroyOnLoad" : xSceneInfo.m_strName;
		if (strSceneName.empty()) strSceneName = "Untitled";
		if (xSceneInfo.m_bHasUnsavedChanges) strSceneName += "*";

		ImGui::PushID(xRow.m_xScene.GetHandle());
		ImGui::PushStyleColor(ImGuiCol_Header, xP.m_uPanelBgAlt);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, xP.m_uFrameHover);
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, xP.m_uFrameActive);
		ImGui::PushStyleColor(ImGuiCol_Text, xRow.m_bIsActiveScene ? ImGui::ColorConvertU32ToFloat4(xP.m_uTextBright) : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

		const ImGuiTreeNodeFlags eHeaderFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed
			| ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap;
		const bool bHeaderOpen = ImGui::CollapsingHeader(("     " + strSceneName).c_str(), eHeaderFlags);
		const ImVec2 xRowMin = ImGui::GetItemRectMin();
		const ImVec2 xRowMax = ImGui::GetItemRectMax();
		ImGui::PopStyleColor(4);

		const float fIcon = ImGui::GetFontSize();
		const float fArrowSpace = fIcon + ImGui::GetStyle().FramePadding.x * 2.0f;
		Zenith_EditorUI::DrawIcon(ImGui::GetWindowDrawList(), Zenith_EditorIcon::Scene,
			ImVec2(xRowMin.x + fArrowSpace + fIcon * 0.5f, (xRowMin.y + xRowMax.y) * 0.5f), fIcon * 0.9f,
			xRow.m_bIsActiveScene ? xP.m_uTypeScene : xP.m_uTextDim);

		// Entity count badge at the right edge of the header (draw-list only).
		char acCount[32];
		snprintf(acCount, sizeof(acCount), "%u", Zenith_EditorSceneAccess::GetEntityCount(xRow.m_pxSceneData));
		Zenith_EditorUI::PushSmallFont();
		const float fBadgeWidth = ImGui::CalcTextSize(acCount).x + Zenith_EditorUI::Px(12.0f);
		Zenith_EditorUI::PopFont();
		Zenith_EditorUI::DrawBadge(ImGui::GetWindowDrawList(),
			ImVec2(xRowMax.x - fBadgeWidth - Zenith_EditorUI::Px(6.0f), xRowMin.y + Zenith_EditorUI::Px(4.0f)),
			acCount, xP.m_uFrame, xP.m_uTextDim);

		HandleSceneHeaderDrop(xRow.m_xScene);
		const bool bUnloaded = RenderSceneContextMenu(xRow.m_xScene, *xRow.m_pxSceneData, xRow.m_bIsActiveScene, xRow.m_bIsPersistentScene, xRow.m_uSceneCount);

		if (!bUnloaded && bHeaderOpen)
		{
			ImGui::Indent(Zenith_EditorUI::Px(4.0f));
			RenderSceneEntities(xCtx);
			ImGui::Unindent(Zenith_EditorUI::Px(4.0f));
		}
		ImGui::PopID();
		return bUnloaded;
	}

	void RenderScenesSection(Zenith_EditorHierarchyState& xState, Zenith_EntityID& uEntityToDelete,
		Zenith_EntityID& uDraggedEntityID, Zenith_EntityID& uDropTargetEntityID)
	{
		Zenith_SceneSystem& xScenes = g_xEngine.Scenes();
		Zenith_Scene xActiveScene = xScenes.GetActiveScene();
		Zenith_Scene xPersistentScene = xScenes.GetPersistentScene();

		uint32_t uSceneCount = 0;
		while (xScenes.GetSceneAt(uSceneCount).IsValid())
			++uSceneCount;

		for (uint32_t i = 0; i < uSceneCount; ++i)
		{
			Zenith_Scene xScene = xScenes.GetSceneAt(i);
			Zenith_SceneData* pxSceneData = xScene.IsValid() ? xScenes.GetSceneData(xScene) : nullptr;
			if (pxSceneData == nullptr)
				continue;

			const bool bIsPersistentScene = (xScene == xPersistentScene);
			if (bIsPersistentScene && Zenith_EditorSceneAccess::GetEntityCount(pxSceneData) == 0)
				continue;

			SceneRowContext xRow{ xScene, pxSceneData, xScene == xActiveScene, bIsPersistentScene, uSceneCount };
			TreeContext xCtx{ xState, *pxSceneData, uEntityToDelete, uDraggedEntityID, uDropTargetEntityID, xState.m_szSearch };
			if (RenderSceneRow(xRow, xCtx))
			{
				break;   // scene list changed mid-iteration
			}
		}
	}

	// Empty space at the bottom: drop here to unparent, or to load a scene file.
	void RenderRootDropTargetSection()
	{
		ImGui::Dummy(ImVec2(0, Zenith_EditorUI::Px(24.0f)));
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY"))
			{
				Zenith_EditorActions::ReparentEntity(*(const Zenith_EntityID*)pPayload->Data, INVALID_ENTITY_ID);
			}
			HandleSceneFileDragDrop();
			ImGui::EndDragDropTarget();
		}
	}

	void RenderHeaderRow(Zenith_EditorHierarchyState& xState)
	{
		const float fButton = ImGui::GetFrameHeight();
		const float fSearchWidth = ImGui::GetContentRegionAvail().x - fButton - ImGui::GetStyle().ItemSpacing.x;
		Zenith_EditorUI::SearchBox("search", xState.m_szSearch, sizeof(xState.m_szSearch), "Search entities...", fSearchWidth);
		ImGui::SameLine();
		Zenith_EditorIconButtonOptions xOpts;
		xOpts.m_fSize = fButton;
		xOpts.m_bFrameless = false;
		if (Zenith_EditorUI::IconButton("create", Zenith_EditorIcon::Plus, "Create entity", xOpts))
		{
			ImGui::OpenPopup("CreateEntityPopup");
		}
		if (ImGui::BeginPopup("CreateEntityPopup"))
		{
			ImGui::TextDisabled("Create");
			ImGui::Separator();
			RenderCreateChildItems(INVALID_ENTITY_ID);
			ImGui::EndPopup();
		}
	}

	// Keys that act on the panel's own focus: F2 renames the primary selection.
	void HandlePanelKeys(Zenith_EditorHierarchyState& xState)
	{
		xState.m_bFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		if (!xState.m_bFocused || ImGui::GetIO().WantTextInput)
		{
			return;
		}
		Zenith_Editor& xEditor = g_xEngine.Editor();
		if (ImGui::IsKeyPressed(ImGuiKey_F2, false) && xEditor.HasSelection())
		{
			BeginRename(xState, xEditor.GetSelectedEntityID());
		}
	}
}

//-----------------------------------------------------------------------------
// Main panel render function.
//-----------------------------------------------------------------------------
void Render(Zenith_EditorHierarchyState& xState, Zenith_EntityID& uGameCameraEntityID)
{
	ImGui::Begin(szEDITOR_WINDOW_HIERARCHY);

	Zenith_EntityID uEntityToDelete = INVALID_ENTITY_ID;
	Zenith_EntityID uDraggedEntityID = INVALID_ENTITY_ID;
	Zenith_EntityID uDropTargetEntityID = INVALID_ENTITY_ID;

	RenderHeaderRow(xState);
	ImGui::Spacing();

	ImGui::BeginChild("HierarchyTree", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_None);
	HandlePanelKeys(xState);
	RenderScenesSection(xState, uEntityToDelete, uDraggedEntityID, uDropTargetEntityID);
	RenderRootDropTargetSection();
	ImGui::EndChild();

	// Deferred mutations: applied after the tree has been walked.
	if (uDraggedEntityID.IsValid() && uDropTargetEntityID.IsValid())
	{
		Zenith_EditorActions::ReparentEntity(uDraggedEntityID, uDropTargetEntityID);
	}
	if (uEntityToDelete.IsValid())
	{
		if (uEntityToDelete == uGameCameraEntityID)
		{
			uGameCameraEntityID = INVALID_ENTITY_ID;
		}
		Zenith_Editor& xEditor = g_xEngine.Editor();
		xEditor.SelectEntity(uEntityToDelete, false);
		Zenith_EditorActions::DeleteSelection();
	}

	ImGui::End();
}

} // namespace Zenith_EditorPanelHierarchy

#endif // ZENITH_TOOLS
