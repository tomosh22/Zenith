#include "Zenith.h"

#include "EntityComponent/Components/Zenith_TextComponent.h"
#include "Input/Zenith_Input.h"
#include "Zenith_OS_Include.h"
#include "DataStream/Zenith_DataStream.h"

Zenith_TextComponent::Zenith_TextComponent(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void Zenith_TextComponent::AddText(TextEntry& xEntry)
{
	m_xEntries.push_back(xEntry);
}

void Zenith_TextComponent::AddText_World(TextEntry_World& xEntry)
{
	m_xEntries_World.push_back(xEntry);
}

void Zenith_TextComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Write 2D text entries
	xStream << m_xEntries;

	// Write 3D world text entries
	xStream << m_xEntries_World;

	// m_xParentEntity reference is not serialized - will be restored during deserialization
}

void Zenith_TextComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Read 2D text entries
	xStream >> m_xEntries;

	// Read 3D world text entries
	xStream >> m_xEntries_World;

	// m_xParentEntity will be set by the entity deserialization system
}