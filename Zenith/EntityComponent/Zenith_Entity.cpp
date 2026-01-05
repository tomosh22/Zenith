#include "Zenith.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "DataStream/Zenith_DataStream.h"

Zenith_Entity::Zenith_Entity(Zenith_Scene* pxScene, const std::string& strName)
	: m_pxParentScene(pxScene)
	, m_strName(strName)
{
	m_uEntityID = m_pxParentScene->CreateEntity();
	Zenith_Assert(pxScene->m_xEntityComponents.Get(m_uEntityID).empty(), "Entity ID %u already has components - registry not cleared or ID collision", m_uEntityID);
	AddComponent<Zenith_TransformComponent>();
	// m_bTransient defaults to true (transient). Call SetTransient(false) for persistent entities.
	pxScene->m_xEntityMap.insert({ m_uEntityID, *this });
}

Zenith_Entity::Zenith_Entity(Zenith_Scene* pxScene, Zenith_EntityID uID, Zenith_EntityID uParentID, const std::string& strName)
	: m_pxParentScene(pxScene)
	, m_uEntityID(uID)
	, m_uParentEntityID(uParentID)
	, m_strName(strName)
	, m_bTransient(false)  // Loaded entities are persistent (will be saved back to disk)
{
	Zenith_Assert(pxScene->m_xEntityComponents.Get(m_uEntityID).empty(), "Entity ID %u already has components - registry not cleared or ID collision", m_uEntityID);
	AddComponent<Zenith_TransformComponent>();
	pxScene->m_xEntityMap.insert({ m_uEntityID, *this });
}

void Zenith_Entity::Initialise(Zenith_Scene* pxScene, const std::string& strName)
{
	m_pxParentScene = pxScene;
	m_strName = strName;
	m_uEntityID = m_pxParentScene->CreateEntity();
	Zenith_Assert(pxScene->m_xEntityComponents.Get(m_uEntityID).empty(), "Entity ID %u already has components - registry not cleared or ID collision", m_uEntityID);
	AddComponent<Zenith_TransformComponent>();
	// m_bTransient defaults to true (transient). Call SetTransient(false) for persistent entities.
	pxScene->m_xEntityMap.insert({ m_uEntityID, *this });
}

void Zenith_Entity::Initialise(Zenith_Scene* pxScene, Zenith_EntityID uID, Zenith_EntityID uParentID, const std::string& strName)
{
	m_pxParentScene = pxScene;
	m_uParentEntityID = uParentID;
	m_uEntityID = uID;
	m_strName = strName;
	m_bTransient = false;  // Loaded entities are persistent (will be saved back to disk)
	Zenith_Assert(pxScene->m_xEntityComponents.Get(m_uEntityID).empty(), "Entity ID %u already has components - registry not cleared or ID collision", m_uEntityID);
	AddComponent<Zenith_TransformComponent>();
	pxScene->m_xEntityMap.insert({ m_uEntityID, *this });
}

void Zenith_Entity::SetName(const std::string& strName)
{
	m_strName = strName;
	// Also update the copy in the entity map
	auto xIt = m_pxParentScene->m_xEntityMap.find(m_uEntityID);
	if (xIt != m_pxParentScene->m_xEntityMap.end() && &xIt->second != this)
	{
		xIt->second.m_strName = strName;
	}
}

void Zenith_Entity::SetEnabled(bool bEnabled)
{
	if (m_bEnabled == bEnabled)
	{
		return;
	}

	m_bEnabled = bEnabled;

	// Dispatch OnEnable or OnDisable to all components
	if (m_bEnabled)
	{
		Zenith_ComponentMetaRegistry::Get().DispatchOnEnable(*this);
	}
	else
	{
		Zenith_ComponentMetaRegistry::Get().DispatchOnDisable(*this);
	}

	// Also update the copy in the entity map
	auto xIt = m_pxParentScene->m_xEntityMap.find(m_uEntityID);
	if (xIt != m_pxParentScene->m_xEntityMap.end() && &xIt->second != this)
	{
		xIt->second.m_bEnabled = bEnabled;
	}
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
	xStream << m_strName;

	// Child hierarchy is rebuilt from parent IDs on load, so we don't serialize children
	// Write placeholder count for backward compatibility with scene format
	uint32_t uChildCount = 0;
	xStream << uChildCount;

	// Serialize all components using the ComponentMeta registry
	Zenith_ComponentMetaRegistry::Get().SerializeEntityComponents(
		*const_cast<Zenith_Entity*>(this), xStream);
}

void Zenith_Entity::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Read entity metadata
	xStream >> m_uEntityID;
	xStream >> m_uParentEntityID;
	xStream >> m_strName;

	// Skip child IDs (backward compatibility) - children are rebuilt from parent IDs
	uint32_t uChildCount = 0;
	xStream >> uChildCount;
	for (uint32_t i = 0; i < uChildCount; ++i)
	{
		Zenith_EntityID uChildID;
		xStream >> uChildID;
		// Discarded - hierarchy rebuilt from parent IDs
	}

	// Deserialize all components using the ComponentMeta registry
	Zenith_ComponentMetaRegistry::Get().DeserializeEntityComponents(*this, xStream);
}