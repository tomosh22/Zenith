#include "Zenith.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "DataStream/Zenith_DataStream.h"

Zenith_Entity::Zenith_Entity(Zenith_Scene* pxScene, const std::string& strName)
	: m_pxParentScene(pxScene)
{
	// Entities can only be created via:
	// 1. Scene loading (deserialization)
	// 2. Prefab instantiation (Zenith_Scene::Instantiate)
	// 3. Prefab creation mode (BeginPrefabCreation/EndPrefabCreation)
	Zenith_Assert(Zenith_Scene::IsEntityCreationAllowed(),
		"Entity creation not allowed! Use Zenith_Scene::Instantiate(prefab) to create entities at runtime. "
		"Direct entity construction is only allowed during scene loading or prefab creation mode.");

	m_uEntityID = m_pxParentScene->CreateEntity();
	Zenith_Assert(pxScene->m_xEntityComponents.Get(m_uEntityID).empty(), "Entity ID %u already has components - registry not cleared or ID collision", m_uEntityID);
	pxScene->SetEntityName(m_uEntityID, strName);
	AddComponent<Zenith_TransformComponent>();

	// Runtime-created entities are automatically transient.
	// EXCEPT: Entities created during prefab creation mode (initial scene setup) should be persistent.
	// Prefab creation mode is used for Project_LoadInitialScene and similar setup code.
	// Prefab instantiation (runtime spawning) still creates transient entities.
	m_bTransient = !Zenith_Scene::IsPrefabCreationMode();

	pxScene->m_xEntityMap.insert({ m_uEntityID, *this });
}

Zenith_Entity::Zenith_Entity(Zenith_Scene* pxScene, Zenith_EntityID uID, Zenith_EntityID uParentID, const std::string& strName)
	: m_pxParentScene(pxScene)
	, m_uEntityID(uID)
	, m_uParentEntityID(uParentID)
{
	Zenith_Assert(pxScene->m_xEntityComponents.Get(m_uEntityID).empty(), "Entity ID %u already has components - registry not cleared or ID collision", m_uEntityID);
	pxScene->SetEntityName(m_uEntityID, strName);
	AddComponent<Zenith_TransformComponent>();
	pxScene->m_xEntityMap.insert({ m_uEntityID, *this });
}

void Zenith_Entity::Initialise(Zenith_Scene* pxScene, const std::string& strName)
{
	// Entities can only be created via:
	// 1. Scene loading (deserialization)
	// 2. Prefab instantiation (Zenith_Scene::Instantiate)
	// 3. Prefab creation mode (BeginPrefabCreation/EndPrefabCreation)
	Zenith_Assert(Zenith_Scene::IsEntityCreationAllowed(),
		"Entity creation not allowed! Use Zenith_Scene::Instantiate(prefab) to create entities at runtime. "
		"Direct entity construction is only allowed during scene loading or prefab creation mode.");

	m_pxParentScene = pxScene;
	m_uEntityID = m_pxParentScene->CreateEntity();
	Zenith_Assert(pxScene->m_xEntityComponents.Get(m_uEntityID).empty(), "Entity ID %u already has components - registry not cleared or ID collision", m_uEntityID);
	pxScene->SetEntityName(m_uEntityID, strName);
	AddComponent<Zenith_TransformComponent>();

	// Runtime-created entities are automatically transient.
	// EXCEPT: Entities created during prefab creation mode (initial scene setup) should be persistent.
	m_bTransient = !Zenith_Scene::IsPrefabCreationMode();

	pxScene->m_xEntityMap.insert({ m_uEntityID, *this });
}

void Zenith_Entity::Initialise(Zenith_Scene* pxScene, Zenith_EntityID uID, Zenith_EntityID uParentID, const std::string& strName)
{
	m_pxParentScene = pxScene;
	m_uParentEntityID = uParentID;
	m_uEntityID = uID;
	Zenith_Assert(pxScene->m_xEntityComponents.Get(m_uEntityID).empty(), "Entity ID %u already has components - registry not cleared or ID collision", m_uEntityID);
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

//------------------------------------------------------------------------------
// Parent/Child Hierarchy
//------------------------------------------------------------------------------

void Zenith_Entity::SetParent(Zenith_EntityID uParentID)
{
	// No change needed
	if (m_uParentEntityID == uParentID)
	{
		return;
	}

	// Remove from old parent's child list
	if (HasParent())
	{
		auto xIt = m_pxParentScene->m_xEntityMap.find(m_uParentEntityID);
		if (xIt != m_pxParentScene->m_xEntityMap.end())
		{
			xIt->second.RemoveChild(m_uEntityID);
		}
	}

	// Update parent reference
	m_uParentEntityID = uParentID;

	// Add to new parent's child list
	if (HasParent())
	{
		auto xIt = m_pxParentScene->m_xEntityMap.find(m_uParentEntityID);
		if (xIt != m_pxParentScene->m_xEntityMap.end())
		{
			xIt->second.AddChild(m_uEntityID);
		}
	}
}

void Zenith_Entity::AddChild(Zenith_EntityID uChildID)
{
	// Check if already a child
	for (u_int u = 0; u < m_xChildEntityIDs.GetSize(); ++u)
	{
		if (m_xChildEntityIDs.Get(u) == uChildID)
		{
			return;  // Already a child
		}
	}

	m_xChildEntityIDs.PushBack(uChildID);
}

void Zenith_Entity::RemoveChild(Zenith_EntityID uChildID)
{
	for (u_int u = 0; u < m_xChildEntityIDs.GetSize(); ++u)
	{
		if (m_xChildEntityIDs.Get(u) == uChildID)
		{
			// Swap and pop for O(1) removal
			m_xChildEntityIDs.Get(u) = m_xChildEntityIDs.GetBack();
			m_xChildEntityIDs.PopBack();
			return;
		}
	}
}

//------------------------------------------------------------------------------
// Serialization
//------------------------------------------------------------------------------

void Zenith_Entity::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Write entity metadata
	xStream << m_uEntityID;
	xStream << m_uParentEntityID;
	xStream << GetName();

	// Write child entity IDs
	uint32_t uChildCount = static_cast<uint32_t>(m_xChildEntityIDs.GetSize());
	xStream << uChildCount;
	for (u_int u = 0; u < m_xChildEntityIDs.GetSize(); ++u)
	{
		xStream << m_xChildEntityIDs.Get(u);
	}

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

	// Read child entity IDs
	uint32_t uChildCount = 0;
	xStream >> uChildCount;
	m_xChildEntityIDs.Clear();
	m_xChildEntityIDs.Reserve(uChildCount);
	for (uint32_t i = 0; i < uChildCount; ++i)
	{
		Zenith_EntityID uChildID;
		xStream >> uChildID;
		m_xChildEntityIDs.PushBack(uChildID);
	}

	// Deserialize all components using the ComponentMeta registry
	Zenith_ComponentMetaRegistry::Get().DeserializeEntityComponents(*this, xStream);
}