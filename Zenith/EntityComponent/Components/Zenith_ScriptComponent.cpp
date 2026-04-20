#include "Zenith.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "DataStream/Zenith_DataStream.h"

ZENITH_REGISTER_COMPONENT(Zenith_ScriptComponent, "Script")

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

		// Write behavior-specific parameters
		m_pxScriptBehaviour->WriteParametersToDataStream(xStream);

		Zenith_Log(LOG_CATEGORY_ECS, "ScriptComponent serialized with behaviour: %s", strTypeName.c_str());
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
			// Set the parent entity reference
			m_pxScriptBehaviour->m_xParentEntity = m_xParentEntity;

			// Read behavior-specific parameters
			m_pxScriptBehaviour->ReadParametersFromDataStream(xStream);

			Zenith_Log(LOG_CATEGORY_ECS, "ScriptComponent deserialized and recreated behaviour: %s", strTypeName.c_str());

			// OnAwake is ONLY called at runtime when behavior is attached.
			// NOT called during scene deserialization.
			// OnStart will be called by Zenith_Scene on the first frame for ALL entities.
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_ECS, "WARNING: ScriptComponent could not recreate behaviour '%s' - not registered in Zenith_BehaviourRegistry", strTypeName.c_str());
		}
	}
}

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "Editor/Zenith_Editor.h"

void Zenith_ScriptComponent::ClearCurrentBehaviour()
{
	if (!m_pxScriptBehaviour)
		return;

	m_pxScriptBehaviour->OnDestroy();
	delete m_pxScriptBehaviour;
	m_pxScriptBehaviour = nullptr;
}

void Zenith_ScriptComponent::RenderBehaviourSelector()
{
	std::vector<std::string> xBehaviourNames = Zenith_BehaviourRegistry::Get().GetRegisteredBehaviourNames();

	if (xBehaviourNames.empty())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No behaviours registered!");
		ImGui::TextWrapped("Call YourBehaviour::RegisterBehaviour() at startup.");
		return;
	}

	// Combo items: "(None)" followed by each registered behaviour name.
	std::vector<const char*> xItems;
	xItems.push_back("(None)");
	for (const auto& name : xBehaviourNames)
	{
		xItems.push_back(name.c_str());
	}

	int iCurrentIndex = 0;
	if (m_pxScriptBehaviour != nullptr)
	{
		const char* szCurrentName = m_pxScriptBehaviour->GetBehaviourTypeName();
		for (size_t i = 0; i < xBehaviourNames.size(); ++i)
		{
			if (xBehaviourNames[i] == szCurrentName)
			{
				iCurrentIndex = static_cast<int>(i) + 1; // +1 because of "(None)" at index 0
				break;
			}
		}
	}

	if (!ImGui::Combo("Behaviour", &iCurrentIndex, xItems.data(), static_cast<int>(xItems.size())))
		return;

	if (iCurrentIndex > 0)
	{
		const char* szSelectedName = xBehaviourNames[iCurrentIndex - 1].c_str();
		Zenith_Editor::SetBehaviourOnSelected(szSelectedName);
	}
	else
	{
		ClearCurrentBehaviour();
	}
}

void Zenith_ScriptComponent::RenderActiveBehaviour()
{
	if (m_pxScriptBehaviour == nullptr)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No behaviour set");
		return;
	}

	ImGui::Text("Active Behaviour: %s", m_pxScriptBehaviour->GetBehaviourTypeName());

	if (m_pxScriptBehaviour->m_axGUIDRefs.size() > 0)
	{
		ImGui::Text("GUID References: %zu", m_pxScriptBehaviour->m_axGUIDRefs.size());
		if (ImGui::TreeNode("GUID References"))
		{
			for (size_t i = 0; i < m_pxScriptBehaviour->m_axGUIDRefs.size(); ++i)
			{
				ImGui::Text("[%zu] GUID: %llu", i, m_pxScriptBehaviour->m_axGUIDRefs[i].m_uGUID);
			}
			ImGui::TreePop();
		}
	}

	ImGui::Separator();

	if (ImGui::TreeNode("Behaviour Properties"))
	{
		m_pxScriptBehaviour->RenderPropertiesPanel();
		ImGui::TreePop();
	}
}

void Zenith_ScriptComponent::RenderPropertiesPanel()
{
	if (!ImGui::CollapsingHeader("Script Component", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	RenderBehaviourSelector();
	ImGui::Separator();
	RenderActiveBehaviour();
}
#endif