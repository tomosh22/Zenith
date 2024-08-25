#include "Zenith.h"

#include "EntityComponent/Components/Zenith_TextComponent.h"
#include "Input/Zenith_Input.h"
#include "Zenith_OS_Include.h"

Zenith_TextComponent::Zenith_TextComponent(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void Zenith_TextComponent::AddText(TextEntry& xEntry)
{
	m_xEntries.push_back(xEntry);
}