#include "Zenith.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TextComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "DataStream/Zenith_DataStream.h"

Zenith_Entity::Zenith_Entity(Zenith_Scene* pxScene, const std::string& strName)
	: m_pxParentScene(pxScene)
	, m_strName(strName)
{
		m_uEntityID = m_pxParentScene->CreateEntity();
	AddComponent<Zenith_TransformComponent>();
	pxScene->m_xEntityMap.insert({ m_uEntityID, *this });
}

Zenith_Entity::Zenith_Entity(Zenith_Scene* pxScene, Zenith_EntityID uID, Zenith_EntityID uParentID, const std::string& strName) : m_pxParentScene(pxScene), m_uEntityID(uID), m_uParentEntityID(uParentID), m_strName(strName)
{
	m_uEntityID = uID;
	AddComponent<Zenith_TransformComponent>();
	pxScene->m_xEntityMap.insert({ m_uEntityID, *this });
}

void Zenith_Entity::Initialise(Zenith_Scene* pxScene, const std::string& strName)
{
	m_pxParentScene = pxScene;
	m_strName = strName;
	m_uEntityID = m_pxParentScene->CreateEntity();
	AddComponent<Zenith_TransformComponent>();
	pxScene->m_xEntityMap.insert({ m_uEntityID, *this });
}

void Zenith_Entity::Initialise(Zenith_Scene* pxScene, Zenith_EntityID uID, Zenith_EntityID uParentID, const std::string& strName)
{
	m_pxParentScene = pxScene;
	m_strName = strName;
	m_uParentEntityID = uParentID;
	m_uEntityID = uID;
	AddComponent<Zenith_TransformComponent>();
	pxScene->m_xEntityMap.insert({ m_uEntityID, *this });
}

void Zenith_Entity::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Write entity metadata
	xStream << m_uEntityID;
	xStream << m_uParentEntityID;
	xStream << m_strName;

	// Serialize all components
	// We write a list of component type names followed by their data
	std::vector<std::string> xComponentTypes;

	// Check for each component type and add to list
	// IMPORTANT: Order matters for deserialization!
	// Components with dependencies must be serialized AFTER their dependencies
	if (const_cast<Zenith_Entity*>(this)->HasComponent<Zenith_TransformComponent>())
		xComponentTypes.push_back("TransformComponent");
	if (const_cast<Zenith_Entity*>(this)->HasComponent<Zenith_ModelComponent>())
		xComponentTypes.push_back("ModelComponent");
	if (const_cast<Zenith_Entity*>(this)->HasComponent<Zenith_CameraComponent>())
		xComponentTypes.push_back("CameraComponent");
	if (const_cast<Zenith_Entity*>(this)->HasComponent<Zenith_TextComponent>())
		xComponentTypes.push_back("TextComponent");
	// TerrainComponent MUST come before ColliderComponent
	// (ColliderComponent may depend on TerrainComponent for terrain colliders)
	if (const_cast<Zenith_Entity*>(this)->HasComponent<Zenith_TerrainComponent>())
		xComponentTypes.push_back("TerrainComponent");
	if (const_cast<Zenith_Entity*>(this)->HasComponent<Zenith_ColliderComponent>())
		xComponentTypes.push_back("ColliderComponent");
	// ScriptComponent should come after ColliderComponent as behaviors may reference it
	if (const_cast<Zenith_Entity*>(this)->HasComponent<Zenith_ScriptComponent>())
		xComponentTypes.push_back("ScriptComponent");

	// Write the number of components
	u_int uNumComponents = static_cast<u_int>(xComponentTypes.size());
	xStream << uNumComponents;

	// Write each component's type name and data
	for (const std::string& strTypeName : xComponentTypes)
	{
		xStream << strTypeName;

		// Write component data
		if (strTypeName == "TransformComponent")
		{
			const_cast<Zenith_Entity*>(this)->GetComponent<Zenith_TransformComponent>().WriteToDataStream(xStream);
		}
		else if (strTypeName == "ModelComponent")
		{
			const_cast<Zenith_Entity*>(this)->GetComponent<Zenith_ModelComponent>().WriteToDataStream(xStream);
		}
		else if (strTypeName == "CameraComponent")
		{
			const_cast<Zenith_Entity*>(this)->GetComponent<Zenith_CameraComponent>().WriteToDataStream(xStream);
		}
		else if (strTypeName == "TextComponent")
		{
			const_cast<Zenith_Entity*>(this)->GetComponent<Zenith_TextComponent>().WriteToDataStream(xStream);
		}
		else if (strTypeName == "TerrainComponent")
		{
			const_cast<Zenith_Entity*>(this)->GetComponent<Zenith_TerrainComponent>().WriteToDataStream(xStream);
		}
		else if (strTypeName == "ColliderComponent")
		{
			const_cast<Zenith_Entity*>(this)->GetComponent<Zenith_ColliderComponent>().WriteToDataStream(xStream);
		}
		else if (strTypeName == "ScriptComponent")
		{
			const_cast<Zenith_Entity*>(this)->GetComponent<Zenith_ScriptComponent>().WriteToDataStream(xStream);
		}
	}
}

void Zenith_Entity::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Read entity metadata
	xStream >> m_uEntityID;
	xStream >> m_uParentEntityID;
	xStream >> m_strName;

	// Read the number of components
	u_int uNumComponents;
	xStream >> uNumComponents;

	// Read and deserialize each component
	for (u_int u = 0; u < uNumComponents; u++)
	{
		std::string strComponentType;
		xStream >> strComponentType;

		// Deserialize component based on type
		if (strComponentType == "TransformComponent")
		{
			// TransformComponent is already created by entity constructor
			if (HasComponent<Zenith_TransformComponent>())
			{
				GetComponent<Zenith_TransformComponent>().ReadFromDataStream(xStream);
			}
		}
		else if (strComponentType == "ModelComponent")
		{
			if (!HasComponent<Zenith_ModelComponent>())
			{
				Zenith_ModelComponent& xComponent = AddComponent<Zenith_ModelComponent>();
				xComponent.ReadFromDataStream(xStream);
			}
		}
		else if (strComponentType == "CameraComponent")
		{
			if (!HasComponent<Zenith_CameraComponent>())
			{
				Zenith_CameraComponent& xComponent = AddComponent<Zenith_CameraComponent>();
				xComponent.ReadFromDataStream(xStream);
			}
		}
		else if (strComponentType == "TextComponent")
		{
			if (!HasComponent<Zenith_TextComponent>())
			{
				Zenith_TextComponent& xComponent = AddComponent<Zenith_TextComponent>();
				xComponent.ReadFromDataStream(xStream);
			}
		}
		else if (strComponentType == "TerrainComponent")
		{
			if (!HasComponent<Zenith_TerrainComponent>())
			{
				Zenith_TerrainComponent& xComponent = AddComponent<Zenith_TerrainComponent>();
				xComponent.ReadFromDataStream(xStream);
			}
		}
		else if (strComponentType == "ColliderComponent")
		{
			if (!HasComponent<Zenith_ColliderComponent>())
			{
				Zenith_ColliderComponent& xComponent = AddComponent<Zenith_ColliderComponent>();
				xComponent.ReadFromDataStream(xStream);
			}
		}
		else if (strComponentType == "ScriptComponent")
		{
			if (!HasComponent<Zenith_ScriptComponent>())
			{
				Zenith_ScriptComponent& xComponent = AddComponent<Zenith_ScriptComponent>();
				xComponent.ReadFromDataStream(xStream);
			}
		}
	}
}