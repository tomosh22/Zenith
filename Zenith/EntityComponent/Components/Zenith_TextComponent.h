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
			ImGui::Text("Text component attached");
			// TODO: Add text editing UI
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
