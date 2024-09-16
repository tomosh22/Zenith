#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"

struct TextEntry
{
	std::string m_strText;
	Zenith_Maths::Vector2 m_xPosition = { 0,0 }; //in pixels
	float m_fScale = 1.;
};

struct TextEntry_World
{
	std::string m_strText;
	Zenith_Maths::Vector3 m_xPosition = { 0,0,0 };
	float m_fScale = 1.;
};

class Zenith_TextComponent
{
public:
	Zenith_TextComponent() = delete;
	Zenith_TextComponent(Zenith_Entity& xParentEntity);
	~Zenith_TextComponent() = default;

	void AddText(TextEntry& xEntry);
	void AddText_World(TextEntry_World& xEntry);

private:
	friend class Flux_Text;
	std::vector<TextEntry> m_xEntries;
	std::vector<TextEntry_World> m_xEntries_World;

	Zenith_Entity m_xParentEntity;
};
