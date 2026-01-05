#include "Zenith.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "EntityComponent/Zenith_Scene.h"
#include "DataStream/Zenith_DataStream.h"

ZENITH_REGISTER_COMPONENT(Zenith_ScriptComponent, "Script")

// Force link function - ensures this translation unit is included by the linker
// Called from Zenith_Scene.cpp to guarantee static initializer runs
void Zenith_ScriptComponent_ForceLink() {}

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
#include "imgui.h"

void Zenith_ScriptComponent::RenderPropertiesPanel()
{
	if (ImGui::CollapsingHeader("Script Component", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// Behavior selection dropdown
		std::vector<std::string> xBehaviourNames = Zenith_BehaviourRegistry::Get().GetRegisteredBehaviourNames();
		
		if (xBehaviourNames.empty())
		{
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No behaviours registered!");
			ImGui::TextWrapped("Call YourBehaviour::RegisterBehaviour() at startup.");
		}
		else
		{
			// Build combo items
			static int s_iSelectedBehaviourIndex = 0;
			std::vector<const char*> xItems;
			xItems.push_back("(None)");
			for (const auto& name : xBehaviourNames)
			{
				xItems.push_back(name.c_str());
			}

			// Find current selection index
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

			if (ImGui::Combo("Behaviour", &iCurrentIndex, xItems.data(), static_cast<int>(xItems.size())))
			{
				// Delete old behaviour
				if (m_pxScriptBehaviour)
				{
					delete m_pxScriptBehaviour;
					m_pxScriptBehaviour = nullptr;
				}

				// Create new behaviour
				if (iCurrentIndex > 0)
				{
					const char* szSelectedName = xBehaviourNames[iCurrentIndex - 1].c_str();
					m_pxScriptBehaviour = Zenith_BehaviourRegistry::Get().CreateBehaviour(szSelectedName, m_xParentEntity);
					if (m_pxScriptBehaviour)
					{
						m_pxScriptBehaviour->m_xParentEntity = m_xParentEntity;
						m_pxScriptBehaviour->OnAwake();
						Zenith_Log(LOG_CATEGORY_ECS, "[ScriptComponent] Set behaviour to: %s", szSelectedName);
					}
				}
			}
		}

		ImGui::Separator();

		if (m_pxScriptBehaviour != nullptr)
		{
			ImGui::Text("Active Behaviour: %s", m_pxScriptBehaviour->GetBehaviourTypeName());
			
			// Display GUID references if any
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

			// Render behavior-specific properties
			if (ImGui::TreeNode("Behaviour Properties"))
			{
				m_pxScriptBehaviour->RenderPropertiesPanel();
				ImGui::TreePop();
			}
		}
		else
		{
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No behaviour set");
		}
	}
}
#endif