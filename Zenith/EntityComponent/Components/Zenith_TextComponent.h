#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#include "EntityComponent/Zenith_ComponentRegistry.h"
#endif

struct TextEntry
{
	std::string m_strText;
	Zenith_Maths::Vector2 m_xPosition = { 0,0 }; //in pixels
	float m_fScale = 1.;

	// Serialization methods for Zenith_DataStream
	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		xStream << m_strText;
		xStream << m_xPosition;
		xStream << m_fScale;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		xStream >> m_strText;
		xStream >> m_xPosition;
		xStream >> m_fScale;
	}
};

struct TextEntry_World
{
	std::string m_strText;
	Zenith_Maths::Vector3 m_xPosition = { 0,0,0 };
	float m_fScale = 1.;

	// Serialization methods for Zenith_DataStream
	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		xStream << m_strText;
		xStream << m_xPosition;
		xStream << m_fScale;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		xStream >> m_strText;
		xStream >> m_xPosition;
		xStream >> m_fScale;
	}
};

class Zenith_TextComponent
{
public:
	Zenith_TextComponent() = delete;
	Zenith_TextComponent(Zenith_Entity& xParentEntity);
	~Zenith_TextComponent() = default;

	void AddText(TextEntry& xEntry);
	void AddText_World(TextEntry_World& xEntry);

	// Serialization methods for Zenith_DataStream
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

#ifdef ZENITH_TOOLS
	//--------------------------------------------------------------------------
	// Editor UI - Renders component properties in the Properties panel
	//--------------------------------------------------------------------------
	void RenderPropertiesPanel()
	{
		if (ImGui::CollapsingHeader("Text", ImGuiTreeNodeFlags_DefaultOpen))
		{
			// Screen-space text entries
			if (m_xEntries.size() > 0)
			{
				if (ImGui::TreeNode("Screen-Space Text", "Screen-Space Text (%zu entries)", m_xEntries.size()))
				{
					for (size_t i = 0; i < m_xEntries.size(); ++i)
					{
						ImGui::PushID((int)i);
						const TextEntry& xEntry = m_xEntries[i];
						ImGui::Text("Text: \"%s\"", xEntry.m_strText.c_str());
						ImGui::Text("Position: (%.1f, %.1f) px", xEntry.m_xPosition.x, xEntry.m_xPosition.y);
						ImGui::Text("Scale: %.2f", xEntry.m_fScale);
						ImGui::Separator();
						ImGui::PopID();
					}
					ImGui::TreePop();
				}
			}
			else
			{
				ImGui::TextDisabled("No screen-space text");
			}

			// World-space text entries
			if (m_xEntries_World.size() > 0)
			{
				if (ImGui::TreeNode("World-Space Text", "World-Space Text (%zu entries)", m_xEntries_World.size()))
				{
					for (size_t i = 0; i < m_xEntries_World.size(); ++i)
					{
						ImGui::PushID((int)i + 10000);
						const TextEntry_World& xEntry = m_xEntries_World[i];
						ImGui::Text("Text: \"%s\"", xEntry.m_strText.c_str());
						ImGui::Text("Position: (%.2f, %.2f, %.2f)", xEntry.m_xPosition.x, xEntry.m_xPosition.y, xEntry.m_xPosition.z);
						ImGui::Text("Scale: %.2f", xEntry.m_fScale);
						ImGui::Separator();
						ImGui::PopID();
					}
					ImGui::TreePop();
				}
			}
			else
			{
				ImGui::TextDisabled("No world-space text");
			}

			if (m_xEntries.empty() && m_xEntries_World.empty())
			{
				ImGui::TextDisabled("No text entries");
			}
		}
	}
#endif

private:
	friend class Flux_Text;
	std::vector<TextEntry> m_xEntries;
	std::vector<TextEntry_World> m_xEntries_World;

	Zenith_Entity m_xParentEntity;

public:
#ifdef ZENITH_TOOLS
	// Static registration function called by ComponentRegistry::Initialise()
	static void RegisterWithEditor()
	{
		Zenith_ComponentRegistry::Get().RegisterComponent<Zenith_TextComponent>("Text");
	}
#endif
};
