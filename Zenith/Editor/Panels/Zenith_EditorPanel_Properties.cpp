#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_Properties.h"
#include "Editor/Zenith_Editor.h"
#include "EntityComponent/Zenith_ComponentRegistry.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

//=============================================================================
// Properties Panel Implementation
//
// Displays properties for the selected entity and allows adding components.
//=============================================================================

namespace
{
	void RenderSceneLabel(const Zenith_Entity& xEntity)
	{
		Zenith_Scene xEntityScene = xEntity.GetScene();
		if (!xEntityScene.IsValid())
			return;

		Zenith_Scene xPersistentScene = g_xEngine.Scenes().GetPersistentScene();
		const Zenith_SceneInfo xInfo = g_xEngine.Scenes().GetSceneInfo(xEntityScene);
		const char* szSceneName = (xEntityScene == xPersistentScene)
			? "DontDestroyOnLoad"
			: xInfo.m_strName.c_str();
		ImGui::TextDisabled("Scene: %s", szSceneName);
	}

	void RenderEntityNameEditor(Zenith_Entity& xEntity)
	{
		char nameBuffer[256];
		strncpy_s(nameBuffer, sizeof(nameBuffer), xEntity.GetName().c_str(), _TRUNCATE);
		if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
		{
			xEntity.SetName(nameBuffer);
		}
	}

	void RenderComponentProperties(Zenith_Entity& xEntity)
	{
		Zenith_ComponentRegistry& xRegistry = Zenith_ComponentRegistry::Get();
		const auto& xEntries = xRegistry.GetEntries();
		for (const Zenith_ComponentRegistryEntry& xEntry : xEntries)
		{
			if (xEntry.m_pfnHasComponent(xEntity))
			{
				xEntry.m_pfnRenderPropertiesPanel(xEntity);
			}
		}
	}

	void RenderAddComponentSection(Zenith_Entity& xEntity, Zenith_EntityID uPrimarySelectedEntityID)
	{
		ImGui::Separator();
		ImGui::Spacing();

		const float fButtonWidth = 200.0f;
		const float fWindowWidth = ImGui::GetWindowWidth();
		ImGui::SetCursorPosX((fWindowWidth - fButtonWidth) * 0.5f);

		if (ImGui::Button("Add Component", ImVec2(fButtonWidth, 0)))
		{
			ImGui::OpenPopup("AddComponentPopup");
			Zenith_Log(LOG_CATEGORY_EDITOR, "[Editor] Add Component button clicked for Entity %u", uPrimarySelectedEntityID);
		}

		if (!ImGui::BeginPopup("AddComponentPopup"))
			return;

		Zenith_ComponentRegistry& xRegistry = Zenith_ComponentRegistry::Get();
		const auto& xEntries = xRegistry.GetEntries();
		bool bAnyAvailable = false;

		for (size_t i = 0; i < xEntries.size(); ++i)
		{
			const Zenith_ComponentRegistryEntry& xEntry = xEntries[i];
			const bool bHasComponent = xRegistry.EntityHasComponent(i, xEntity);

			if (bHasComponent)
			{
				ImGui::BeginDisabled();
				ImGui::MenuItem(xEntry.m_strDisplayName.c_str(), nullptr, false, false);
				ImGui::EndDisabled();
			}
			else
			{
				bAnyAvailable = true;
				if (ImGui::MenuItem(xEntry.m_strDisplayName.c_str()))
				{
					g_xEngine.Editor().AddComponentToSelected(xEntry.m_strDisplayName.c_str());
				}
			}
		}

		if (!bAnyAvailable)
		{
			ImGui::TextDisabled("All available components already added");
		}

		ImGui::EndPopup();
	}
}

namespace Zenith_EditorPanelProperties
{

void Render(Zenith_Entity* pxSelectedEntity, Zenith_EntityID uPrimarySelectedEntityID)
{
	ImGui::Begin("Properties");

	if (!pxSelectedEntity)
	{
		ImGui::Text("No entity selected");
		ImGui::End();
		return;
	}

	RenderSceneLabel(*pxSelectedEntity);
	RenderEntityNameEditor(*pxSelectedEntity);
	ImGui::Separator();
	RenderComponentProperties(*pxSelectedEntity);
	RenderAddComponentSection(*pxSelectedEntity, uPrimarySelectedEntityID);

	ImGui::End();
}

} // namespace Zenith_EditorPanelProperties

#endif // ZENITH_TOOLS
