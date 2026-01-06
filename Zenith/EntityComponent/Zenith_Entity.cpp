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
	Zenith_Assert(pxScene != nullptr, "Entity created with null scene");

	// CreateEntity now allocates a slot and returns a generation-aware ID
	m_xEntityID = m_pxParentScene->CreateEntity();
	Zenith_Assert(pxScene->m_xEntityComponents.Get(m_xEntityID.m_uIndex).empty(),
		"Entity slot %u already has components - registry not cleared or ID collision", m_xEntityID.m_uIndex);

	// Store this entity in the slot
	pxScene->m_xEntitySlots.Get(m_xEntityID.m_uIndex).m_xEntity = *this;

	AddComponent<Zenith_TransformComponent>();
	// m_bTransient defaults to true (transient). Call SetTransient(false) for persistent entities.

	// Track entities created during Update() - they won't receive callbacks until next frame
	pxScene->RegisterCreatedDuringUpdate(m_xEntityID);
}

Zenith_Entity::Zenith_Entity(Zenith_Scene* pxScene, Zenith_EntityID xID, const std::string& strName)
	: m_pxParentScene(pxScene)
	, m_xEntityID(xID)
	, m_strName(strName)
	, m_bTransient(false)  // Loaded entities are persistent (will be saved back to disk)
{
	Zenith_Assert(pxScene != nullptr, "Entity created with null scene");
	Zenith_Assert(pxScene->m_xEntityComponents.Get(m_xEntityID.m_uIndex).empty(),
		"Entity slot %u already has components - registry not cleared or ID collision", m_xEntityID.m_uIndex);

	// Store this entity in the slot
	pxScene->m_xEntitySlots.Get(m_xEntityID.m_uIndex).m_xEntity = *this;

	AddComponent<Zenith_TransformComponent>();
}

void Zenith_Entity::Initialise(Zenith_Scene* pxScene, const std::string& strName)
{
	Zenith_Assert(pxScene != nullptr, "Entity initialised with null scene");

	m_pxParentScene = pxScene;
	m_strName = strName;
	m_xEntityID = m_pxParentScene->CreateEntity();
	Zenith_Assert(pxScene->m_xEntityComponents.Get(m_xEntityID.m_uIndex).empty(),
		"Entity slot %u already has components - registry not cleared or ID collision", m_xEntityID.m_uIndex);

	// Store this entity in the slot
	pxScene->m_xEntitySlots.Get(m_xEntityID.m_uIndex).m_xEntity = *this;

	AddComponent<Zenith_TransformComponent>();
	// m_bTransient defaults to true (transient). Call SetTransient(false) for persistent entities.
}

void Zenith_Entity::Initialise(Zenith_Scene* pxScene, Zenith_EntityID xID, const std::string& strName)
{
	Zenith_Assert(pxScene != nullptr, "Entity initialised with null scene");

	m_pxParentScene = pxScene;
	m_xEntityID = xID;
	m_strName = strName;
	m_bTransient = false;  // Loaded entities are persistent (will be saved back to disk)
	Zenith_Assert(pxScene->m_xEntityComponents.Get(m_xEntityID.m_uIndex).empty(),
		"Entity slot %u already has components - registry not cleared or ID collision", m_xEntityID.m_uIndex);

	AddComponent<Zenith_TransformComponent>();
}

void Zenith_Entity::SetName(const std::string& strName)
{
	Zenith_Assert(m_pxParentScene != nullptr, "SetName: Entity has no parent scene");
	Zenith_Assert(m_pxParentScene->EntityExists(m_xEntityID), "SetName: Entity (idx=%u, gen=%u) not found in scene",
		m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);

	m_strName = strName;
	// Also update the slot's copy of this entity
	m_pxParentScene->m_xEntitySlots.Get(m_xEntityID.m_uIndex).m_xEntity.m_strName = strName;
}

void Zenith_Entity::SetEnabled(bool bEnabled)
{
	Zenith_Assert(m_pxParentScene != nullptr, "SetEnabled: Entity has no parent scene");
	Zenith_Assert(m_pxParentScene->EntityExists(m_xEntityID), "SetEnabled: Entity (idx=%u, gen=%u) not found in scene",
		m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);

	if (m_bEnabled == bEnabled)
	{
		return;
	}

	m_bEnabled = bEnabled;
	// Also update the slot's copy of this entity
	m_pxParentScene->m_xEntitySlots.Get(m_xEntityID.m_uIndex).m_xEntity.m_bEnabled = bEnabled;

	// Dispatch OnEnable or OnDisable to all components
	if (m_bEnabled)
	{
		Zenith_ComponentMetaRegistry::Get().DispatchOnEnable(*this);
	}
	else
	{
		Zenith_ComponentMetaRegistry::Get().DispatchOnDisable(*this);
	}
}

void Zenith_Entity::SetTransient(bool bTransient)
{
	m_bTransient = bTransient;
	// Also update the slot's copy of this entity (if valid)
	if (m_pxParentScene != nullptr && m_pxParentScene->EntityExists(m_xEntityID))
	{
		m_pxParentScene->m_xEntitySlots.Get(m_xEntityID.m_uIndex).m_xEntity.m_bTransient = bTransient;
	}
}

//------------------------------------------------------------------------------
// Parent/Child Hierarchy (delegates to TransformComponent)
//------------------------------------------------------------------------------

Zenith_EntityID Zenith_Entity::GetParentEntityID() const
{
	const Zenith_TransformComponent& xTransform = GetComponent<Zenith_TransformComponent>();
	if (xTransform.GetParent() == nullptr) return INVALID_ENTITY_ID;
	return xTransform.GetParent()->GetEntity().GetEntityID();
}

bool Zenith_Entity::HasParent() const
{
	return GetComponent<Zenith_TransformComponent>().HasParent();
}

void Zenith_Entity::SetParent(Zenith_EntityID xParentID)
{
	Zenith_TransformComponent& xTransform = GetComponent<Zenith_TransformComponent>();
	if (!xParentID.IsValid())
	{
		xTransform.SetParent(nullptr);
	}
	else
	{
		Zenith_Assert(m_pxParentScene->EntityExists(xParentID), "SetParent: Parent entity (idx=%u, gen=%u) does not exist",
			xParentID.m_uIndex, xParentID.m_uGeneration);
		Zenith_Entity& xParent = m_pxParentScene->GetEntityRef(xParentID);
		xTransform.SetParent(&xParent.GetComponent<Zenith_TransformComponent>());
	}
}

Zenith_Vector<Zenith_EntityID> Zenith_Entity::GetChildEntityIDs() const
{
	// Return a copy of the child entity IDs from the transform component
	const Zenith_TransformComponent& xTransform = GetComponent<Zenith_TransformComponent>();
	return xTransform.GetChildEntityIDs();
}

bool Zenith_Entity::HasChildren() const
{
	return GetComponent<Zenith_TransformComponent>().GetChildCount() > 0;
}

uint32_t Zenith_Entity::GetChildCount() const
{
	return GetComponent<Zenith_TransformComponent>().GetChildCount();
}

bool Zenith_Entity::IsRoot() const
{
	return GetComponent<Zenith_TransformComponent>().IsRoot();
}

Zenith_TransformComponent& Zenith_Entity::GetTransform()
{
	return GetComponent<Zenith_TransformComponent>();
}

//------------------------------------------------------------------------------
// Serialization
//------------------------------------------------------------------------------

void Zenith_Entity::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Write entity index only (generation is runtime-only for stale detection)
	xStream << m_xEntityID.m_uIndex;
	xStream << m_strName;

	// Serialize all components using the ComponentMeta registry
	Zenith_ComponentMetaRegistry::Get().SerializeEntityComponents(
		*const_cast<Zenith_Entity*>(this), xStream);
}

void Zenith_Entity::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Read entity index - generation will be assigned fresh on load
	uint32_t uFileIndex;
	xStream >> uFileIndex;
	xStream >> m_strName;

	// Note: m_xEntityID is set by the scene during loading, not here
	// (old format support - scene handles ID assignment now)

	// Deserialize all components using the ComponentMeta registry
	// (TransformComponent reads pending parent ID - hierarchy rebuilt after all entities loaded)
	Zenith_ComponentMetaRegistry::Get().DeserializeEntityComponents(*this, xStream);
}