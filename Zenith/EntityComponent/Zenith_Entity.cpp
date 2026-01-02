#include "Zenith.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "DataStream/Zenith_DataStream.h"

Zenith_Entity::Zenith_Entity(Zenith_Scene* pxScene, const std::string& strName)
	: m_pxParentScene(pxScene)
{
	m_uEntityID = m_pxParentScene->CreateEntity();
	pxScene->SetEntityName(m_uEntityID, strName);
	AddComponent<Zenith_TransformComponent>();
	pxScene->m_xEntityMap.insert({ m_uEntityID, *this });
}

Zenith_Entity::Zenith_Entity(Zenith_Scene* pxScene, Zenith_EntityID uID, Zenith_EntityID uParentID, const std::string& strName)
	: m_pxParentScene(pxScene)
	, m_uEntityID(uID)
	, m_uParentEntityID(uParentID)
{
	pxScene->SetEntityName(m_uEntityID, strName);
	AddComponent<Zenith_TransformComponent>();
	pxScene->m_xEntityMap.insert({ m_uEntityID, *this });
}

void Zenith_Entity::Initialise(Zenith_Scene* pxScene, const std::string& strName)
{
	m_pxParentScene = pxScene;
	m_uEntityID = m_pxParentScene->CreateEntity();
	pxScene->SetEntityName(m_uEntityID, strName);
	AddComponent<Zenith_TransformComponent>();
	pxScene->m_xEntityMap.insert({ m_uEntityID, *this });
}

void Zenith_Entity::Initialise(Zenith_Scene* pxScene, Zenith_EntityID uID, Zenith_EntityID uParentID, const std::string& strName)
{
	m_pxParentScene = pxScene;
	m_uParentEntityID = uParentID;
	m_uEntityID = uID;
	pxScene->SetEntityName(m_uEntityID, strName);
	AddComponent<Zenith_TransformComponent>();
	pxScene->m_xEntityMap.insert({ m_uEntityID, *this });
}

const std::string& Zenith_Entity::GetName() const
{
	return m_pxParentScene->GetEntityName(m_uEntityID);
}

void Zenith_Entity::SetName(const std::string& strName)
{
	m_pxParentScene->SetEntityName(m_uEntityID, strName);
}

void Zenith_Entity::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Write entity metadata
	xStream << m_uEntityID;
	xStream << m_uParentEntityID;
	xStream << GetName();

	// Serialize all components using the ComponentMeta registry
	Zenith_ComponentMetaRegistry::Get().SerializeEntityComponents(
		*const_cast<Zenith_Entity*>(this), xStream);
}

void Zenith_Entity::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Read entity metadata
	xStream >> m_uEntityID;
	xStream >> m_uParentEntityID;
	std::string strName;
	xStream >> strName;
	SetName(strName);

	// Deserialize all components using the ComponentMeta registry
	Zenith_ComponentMetaRegistry::Get().DeserializeEntityComponents(*this, xStream);
}