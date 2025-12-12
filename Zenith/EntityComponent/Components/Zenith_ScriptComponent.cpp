#include "Zenith.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"

void Zenith_ScriptComponent::RenderPropertiesPanel()
{
	if (ImGui::CollapsingHeader("Script Component", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (m_pxScriptBehaviour != nullptr)
		{
			ImGui::Text("Script behaviour attached");
			ImGui::Text("GUID References: %zu", m_pxScriptBehaviour->m_axGUIDRefs.size());
		}
		else
		{
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No behaviour set (use SetBehaviour<T>())");
		}
	}
}
#endif