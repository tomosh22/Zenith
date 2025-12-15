#include "Zenith.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "DataStream/Zenith_DataStream.h"

void Zenith_ScriptComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Write whether a behavior is attached
	bool bHasBehaviour = (m_pxScriptBehaviour != nullptr);
	xStream << bHasBehaviour;

	if (bHasBehaviour)
	{
		// Write the behavior type name for recreation during deserialization
		std::string strTypeName = m_pxScriptBehaviour->GetBehaviourTypeName();
		xStream << strTypeName;

		Zenith_Log("ScriptComponent serialized with behaviour: %s", strTypeName.c_str());
	}
}

void Zenith_ScriptComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Read whether a behavior was attached
	bool bHasBehaviour;
	xStream >> bHasBehaviour;

	if (bHasBehaviour)
	{
		// Read the behavior type name
		std::string strTypeName;
		xStream >> strTypeName;

		// Try to recreate the behavior using the registry
		m_pxScriptBehaviour = Zenith_BehaviourRegistry::Get().CreateBehaviour(strTypeName.c_str(), m_xParentEntity);

		if (m_pxScriptBehaviour)
		{
			Zenith_Log("ScriptComponent deserialized and recreated behaviour: %s", strTypeName.c_str());
			// Call OnCreate to initialize the behavior
			m_pxScriptBehaviour->OnCreate();
		}
		else
		{
			Zenith_Log("WARNING: ScriptComponent could not recreate behaviour '%s' - not registered in Zenith_BehaviourRegistry", strTypeName.c_str());
		}
	}
}

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