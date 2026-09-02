#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_Properties.h"
#include "Core/Zenith_EditorWindowNames.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorActions.h"
#include "Editor/Zenith_EditorState.h"
#include "Editor/Zenith_EditorUI.h"
#include "EntityComponent/Zenith_ComponentEditorRegistry.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"

#include "imgui.h"

#include <cctype>
#include <cstring>

namespace
{
	Zenith_EditorIcon IconForComponent(const std::string& strDisplayName)
	{
		struct Row { const char* m_szName; Zenith_EditorIcon m_eIcon; };
		static const Row axRows[] =
		{
			{ "Transform",       Zenith_EditorIcon::Translate },
			{ "Model",           Zenith_EditorIcon::Mesh },
			{ "InstancedMesh",   Zenith_EditorIcon::Mesh },
			{ "Camera",          Zenith_EditorIcon::Camera },
			{ "Light",           Zenith_EditorIcon::Light },
			{ "Sun",             Zenith_EditorIcon::Sun },
			{ "Atmosphere",      Zenith_EditorIcon::World },
			{ "Terrain",         Zenith_EditorIcon::Terrain },
			{ "Collider",        Zenith_EditorIcon::Collider },
			{ "Graph",           Zenith_EditorIcon::Graph },
			{ "AIAgent",         Zenith_EditorIcon::Graph },
			{ "UI",              Zenith_EditorIcon::UI },
			{ "ParticleEmitter", Zenith_EditorIcon::Particles },
			{ "Animator",        Zenith_EditorIcon::Animation },
			{ "Tween",           Zenith_EditorIcon::Animation },
			{ "Attachment",      Zenith_EditorIcon::Attachment },
			{ "NavMesh",         Zenith_EditorIcon::Terrain },
		};
		for (const Row& xRow : axRows)
		{
			if (strDisplayName == xRow.m_szName) return xRow.m_eIcon;
		}
		return Zenith_EditorIcon::Entity;
	}

	//-------------------------------------------------------------------------
	// Entity header: icon, name (undoable rename), enabled toggle, scene line.
	//-------------------------------------------------------------------------
	void RenderEntityNameField(Zenith_EditorInspectorState& xState, Zenith_Entity& xEntity)
	{
		const Zenith_EntityID xID = xEntity.GetEntityID();
		// Refresh the buffer from the entity unless the user is mid-edit.
		if (xState.m_xNameEditEntity != xID || !ImGui::IsAnyItemActive())
		{
			strncpy_s(xState.m_szNameBuffer, sizeof(xState.m_szNameBuffer), xEntity.GetName().c_str(), _TRUNCATE);
			xState.m_xNameEditEntity = xID;
		}
		ImGui::SetNextItemWidth(-FLT_MIN);
		Zenith_EditorUI::PushHeadingFont();
		ImGui::InputText("##name", xState.m_szNameBuffer, sizeof(xState.m_szNameBuffer));
		Zenith_EditorUI::PopFont();
		if (ImGui::IsItemActivated())
		{
			xState.m_strNameBeforeEdit = xEntity.GetName();
		}
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			// Apply live edits back through the undo action so Ctrl+Z restores.
			const std::string strNew = xState.m_szNameBuffer;
			if (strNew != xState.m_strNameBeforeEdit && !strNew.empty())
			{
				Zenith_EditorActions::RenameEntity(xID, strNew);
			}
		}
	}

	void RenderEntityHeader(Zenith_EditorInspectorState& xState, Zenith_Entity& xEntity, size_t uSelectionCount)
	{
		const Zenith_EditorPalette& xP = Zenith_EditorUI::Palette();

		bool bEnabled = xEntity.IsEnabled();
		if (ImGui::Checkbox("##enabled", &bEnabled))
		{
			Zenith_EditorActions::SetEntityEnabled(xEntity.GetEntityID(), bEnabled);
		}
		ImGui::SetItemTooltip(bEnabled ? "Enabled (click to disable)" : "Disabled (click to enable)");
		ImGui::SameLine();
		RenderEntityNameField(xState, xEntity);

		// Second line: scene, id, transient flag, multi-select note.
		Zenith_Scene xEntityScene = xEntity.GetScene();
		const char* szSceneName = "";
		if (xEntityScene.IsValid())
		{
			const bool bPersistent = (xEntityScene == g_xEngine.Scenes().GetPersistentScene());
			szSceneName = bPersistent ? "DontDestroyOnLoad" : g_xEngine.Scenes().GetSceneInfo(xEntityScene).m_strName.c_str();
		}
		Zenith_EditorUI::PushSmallFont();
		ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(xP.m_uTextDim), "%s   id %u", szSceneName, xEntity.GetEntityID().m_uIndex);
		ImGui::SameLine();
		bool bTransient = xEntity.IsTransient();
		if (ImGui::Checkbox("Transient", &bTransient))
		{
			xEntity.SetTransient(bTransient);
			Zenith_EditorActions::MarkSceneDirtyForEntity(xEntity.GetEntityID());
		}
		ImGui::SetItemTooltip("Transient entities are runtime-only and are not written to the scene file");
		if (uSelectionCount > 1)
		{
			ImGui::SameLine();
			char acNote[64];
			snprintf(acNote, sizeof(acNote), "1 of %zu selected", uSelectionCount);
			Zenith_EditorUI::Badge(acNote, xP.m_uFrame, xP.m_uTextDim);
		}
		Zenith_EditorUI::PopFont();
	}

	//-------------------------------------------------------------------------
	// Component sections
	//-------------------------------------------------------------------------
	void RenderComponentSections(Zenith_EditorInspectorState& xState, Zenith_Entity& xEntity)
	{
		Zenith_ComponentEditorRegistry& xRegistry = Zenith_ComponentEditorRegistry::Get();
		const Zenith_Vector<Zenith_ComponentEditorRegistryEntry>& xEntries = xRegistry.GetEntries();
		const Zenith_EntityID xID = xEntity.GetEntityID();

		// Labels to the right of a field get ~42% of the panel so they never clip.
		ImGui::PushItemWidth(-ImGui::GetContentRegionAvail().x * 0.42f);

		for (u_int u = 0; u < xEntries.GetSize(); ++u)
		{
			const Zenith_ComponentEditorRegistryEntry& xEntry = xEntries.Get(u);
			if (!xEntry.m_pfnHasComponent(xEntity))
			{
				continue;
			}
			const bool bIsTransform = (xEntry.m_strDisplayName == "Transform");
			bool bRemove = false;
			const bool bOpen = Zenith_EditorUI::ComponentHeader(xEntry.m_strDisplayName.c_str(),
				IconForComponent(xEntry.m_strDisplayName), xEntry.m_strDisplayName.c_str(), !bIsTransform, &bRemove);
			if (bRemove)
			{
				xState.m_xUndoTracker.Cancel();
				Zenith_EditorActions::RemoveComponent(xID, xEntry.m_strDisplayName.c_str());
				break;   // the entry list is unchanged but the entity is: stop this frame's walk
			}
			if (bOpen)
			{
				ImGui::PushID(xEntry.m_strDisplayName.c_str());
				ImGui::Indent(Zenith_EditorUI::Px(6.0f));
				xEntry.m_pfnRenderPropertiesPanel(xEntity);
				ImGui::Unindent(Zenith_EditorUI::Px(6.0f));
				ImGui::PopID();
				ImGui::Spacing();
			}
		}

		ImGui::PopItemWidth();
	}

	//-------------------------------------------------------------------------
	// Add Component: a searchable popup. Enter adds the first match.
	//-------------------------------------------------------------------------
	void RenderAddComponentPopup(Zenith_EditorInspectorState& xState, Zenith_Entity& xEntity)
	{
		if (!ImGui::BeginPopup("AddComponentPopup"))
		{
			return;
		}
		if (ImGui::IsWindowAppearing())
		{
			xState.m_szAddComponentSearch[0] = '\0';
			ImGui::SetKeyboardFocusHere();
		}
		Zenith_EditorUI::SearchBox("addsearch", xState.m_szAddComponentSearch, sizeof(xState.m_szAddComponentSearch), "Search components...", Zenith_EditorUI::Px(240.0f));
		const bool bEnter = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
		ImGui::Separator();

		Zenith_ComponentEditorRegistry& xRegistry = Zenith_ComponentEditorRegistry::Get();
		const Zenith_Vector<Zenith_ComponentEditorRegistryEntry>& xEntries = xRegistry.GetEntries();
		const char* szChosen = nullptr;
		bool bAnyListed = false;

		ImGui::BeginChild("AddComponentList", ImVec2(Zenith_EditorUI::Px(240.0f), Zenith_EditorUI::Px(260.0f)));
		for (u_int u = 0; u < xEntries.GetSize(); ++u)
		{
			const Zenith_ComponentEditorRegistryEntry& xEntry = xEntries.Get(u);
			if (!Zenith_EditorPanelProperties::MatchesComponentSearch(xEntry.m_strDisplayName.c_str(), xState.m_szAddComponentSearch))
			{
				continue;
			}
			const bool bHas = xEntry.m_pfnHasComponent(xEntity);
			ImGui::BeginDisabled(bHas);
			ImGui::PushID(static_cast<int>(u));
			Zenith_EditorUI::IconLabel(IconForComponent(xEntry.m_strDisplayName), "");
			ImGui::SameLine(0.0f, 0.0f);
			const bool bClicked = ImGui::Selectable(xEntry.m_strDisplayName.c_str(), false, ImGuiSelectableFlags_None);
			ImGui::PopID();
			ImGui::EndDisabled();
			if (bHas)
			{
				continue;
			}
			if (bClicked || (bEnter && !bAnyListed))
			{
				szChosen = xEntry.m_strDisplayName.c_str();
			}
			bAnyListed = true;
		}
		if (!bAnyListed)
		{
			ImGui::TextDisabled("No matching components");
		}
		ImGui::EndChild();

		if (szChosen != nullptr)
		{
			xState.m_xUndoTracker.Cancel();
			Zenith_EditorActions::AddComponent(xEntity.GetEntityID(), szChosen);
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	void RenderAddComponentButton(Zenith_EditorInspectorState& xState, Zenith_Entity& xEntity)
	{
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		const float fWidth = ImGui::GetContentRegionAvail().x * 0.7f;
		ImGui::SetCursorPosX((ImGui::GetWindowWidth() - fWidth) * 0.5f);
		Zenith_EditorIconButtonOptions xOpts;
		xOpts.m_bFrameless = false;
		xOpts.m_fSize = ImGui::GetFrameHeight() + Zenith_EditorUI::Px(6.0f);
		if (ImGui::Button("Add Component", ImVec2(fWidth, xOpts.m_fSize)))
		{
			ImGui::OpenPopup("AddComponentPopup");
		}
		RenderAddComponentPopup(xState, xEntity);
	}

	void RenderEmptyState(size_t uSelectionCount)
	{
		const Zenith_EditorPalette& xP = Zenith_EditorUI::Palette();
		ImGui::Dummy(ImVec2(0, Zenith_EditorUI::Px(24.0f)));
		const float fIcon = Zenith_EditorUI::Px(40.0f);
		const ImVec2 xPos = ImGui::GetCursorScreenPos();
		const float fWidth = ImGui::GetContentRegionAvail().x;
		Zenith_EditorUI::DrawIcon(ImGui::GetWindowDrawList(), Zenith_EditorIcon::Entity, ImVec2(xPos.x + fWidth * 0.5f, xPos.y + fIcon * 0.5f), fIcon, xP.m_uFrameActive);
		ImGui::Dummy(ImVec2(0, fIcon + Zenith_EditorUI::Px(8.0f)));
		const char* szText = (uSelectionCount == 0) ? "Select an entity to inspect it" : "The selected entity no longer exists";
		const float fTextWidth = ImGui::CalcTextSize(szText).x;
		ImGui::SetCursorPosX((ImGui::GetWindowWidth() - fTextWidth) * 0.5f);
		ImGui::TextDisabled("%s", szText);
	}
}

namespace Zenith_EditorPanelProperties
{

bool MatchesComponentSearch(const char* szDisplayName, const char* szQuery)
{
	if (szQuery == nullptr || szQuery[0] == '\0') return true;
	return szDisplayName != nullptr && Zenith_EditorUI::ContainsCaseInsensitive(szDisplayName, szQuery);
}

void Render(Zenith_EditorInspectorState& xState, Zenith_Entity* pxSelectedEntity, size_t uSelectionCount)
{
	ImGui::Begin(szEDITOR_WINDOW_PROPERTIES);

	if (!pxSelectedEntity || !pxSelectedEntity->IsValid())
	{
		xState.m_xUndoTracker.Cancel();
		RenderEmptyState(uSelectionCount);
		ImGui::End();
		return;
	}

	// Any click inside the panel (or Tab navigation) may begin an edit; the
	// tracker snapshots the entity so the edit can be undone when it ends.
	const bool bTrigger = (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByPopup)
			&& (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
		|| (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Tab, false));
	xState.m_xUndoTracker.BeginFrame(*pxSelectedEntity, bTrigger);

	RenderEntityHeader(xState, *pxSelectedEntity, uSelectionCount);
	ImGui::Spacing();
	RenderComponentSections(xState, *pxSelectedEntity);
	RenderAddComponentButton(xState, *pxSelectedEntity);

	const bool bUIIdle = !ImGui::IsAnyItemActive() && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
	xState.m_xUndoTracker.EndFrame(*pxSelectedEntity, bUIIdle);

	ImGui::End();
}

} // namespace Zenith_EditorPanelProperties

#endif // ZENITH_TOOLS
