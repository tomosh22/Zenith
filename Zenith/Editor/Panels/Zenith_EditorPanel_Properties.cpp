#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_Properties.h"
#include "Editor/Zenith_Editor.h"
#include "EntityComponent/Zenith_ComponentRegistry.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

//=============================================================================
// Properties Panel Implementation
//
// Displays properties for the selected entity and allows adding components.
//=============================================================================

namespace Zenith_EditorPanelProperties
{

void Render(Zenith_Entity* pxSelectedEntity, Zenith_EntityID uPrimarySelectedEntityID)
{
	ImGui::Begin("Properties");

	if (pxSelectedEntity)
	{
		// Scene label (shows which scene this entity belongs to)
		Zenith_Scene xEntityScene = pxSelectedEntity->GetScene();
		if (xEntityScene.IsValid())
		{
			Zenith_Scene xPersistentScene = Zenith_SceneManager::GetPersistentScene();
			const char* szSceneName = (xEntityScene == xPersistentScene) ?
				"DontDestroyOnLoad" : xEntityScene.GetName().c_str();
			ImGui::TextDisabled("Scene: %s", szSceneName);
		}

		// Entity name editing
		char nameBuffer[256];
		strncpy_s(nameBuffer, sizeof(nameBuffer), pxSelectedEntity->GetName().c_str(), _TRUNCATE);
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
		//----------------------------------------------------------------------
		Zenith_ComponentRegistry& xRegistry = Zenith_ComponentRegistry::Get();
		const auto& xEntries = xRegistry.GetEntries();

		for (const Zenith_ComponentRegistryEntry& xEntry : xEntries)
		{
			// Check if entity has this component and render its properties panel
			if (xEntry.m_pfnHasComponent(*pxSelectedEntity))
			{
				xEntry.m_pfnRenderPropertiesPanel(*pxSelectedEntity);
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
			Zenith_Log(LOG_CATEGORY_EDITOR, "[Editor] Add Component button clicked for Entity %u", uPrimarySelectedEntityID);
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
						Zenith_Editor::AddComponentToSelected(xEntry.m_strDisplayName.c_str());
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

} // namespace Zenith_EditorPanelProperties

#endif // ZENITH_TOOLS
