#include "Zenith.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "DataStream/Zenith_DataStream.h"

//------------------------------------------------------------------------------
// Validity Check
//------------------------------------------------------------------------------

bool Zenith_Entity::IsValid() const
{
	return m_pxParentScene != nullptr && m_pxParentScene->EntityExists(m_xEntityID);
}

//------------------------------------------------------------------------------
// Constructors - Create NEW entities in the scene
//------------------------------------------------------------------------------

Zenith_Entity::Zenith_Entity(Zenith_Scene* pxScene, const std::string& strName)
	: m_pxParentScene(pxScene)
{
	Zenith_Assert(pxScene != nullptr, "Entity created with null scene");

	// CreateEntity allocates a slot and returns a generation-aware ID
	m_xEntityID = m_pxParentScene->CreateEntity();
	Zenith_Assert(pxScene->m_xEntityComponents.Get(m_xEntityID.m_uIndex).empty(),
		"Entity slot %u already has components - registry not cleared or ID collision", m_xEntityID.m_uIndex);

	// Set initial state directly in the slot (single source of truth)
	Zenith_Scene::Zenith_EntitySlot& xSlot = pxScene->m_xEntitySlots.Get(m_xEntityID.m_uIndex);
	xSlot.m_strName = strName;
	xSlot.m_bEnabled = true;
	xSlot.m_bTransient = true;  // Default: transient (not saved)

	AddComponent<Zenith_TransformComponent>();

	// Track entities created during Update() - they won't receive callbacks until next frame
	pxScene->RegisterCreatedDuringUpdate(m_xEntityID);
}

Zenith_Entity::Zenith_Entity(Zenith_Scene* pxScene, Zenith_EntityID xID, const std::string& strName)
	: m_pxParentScene(pxScene)
	, m_xEntityID(xID)
{
	Zenith_Assert(pxScene != nullptr, "Entity created with null scene");
	Zenith_Assert(pxScene->m_xEntityComponents.Get(m_xEntityID.m_uIndex).empty(),
		"Entity slot %u already has components - registry not cleared or ID collision", m_xEntityID.m_uIndex);

	// Set initial state directly in the slot (single source of truth)
	Zenith_Scene::Zenith_EntitySlot& xSlot = pxScene->m_xEntitySlots.Get(m_xEntityID.m_uIndex);
	xSlot.m_strName = strName;
	xSlot.m_bEnabled = true;
	xSlot.m_bTransient = false;  // Loaded entities are persistent (will be saved back to disk)

	AddComponent<Zenith_TransformComponent>();
}

void Zenith_Entity::Initialise(Zenith_Scene* pxScene, const std::string& strName)
{
	Zenith_Assert(pxScene != nullptr, "Entity initialised with null scene");

	m_pxParentScene = pxScene;
	m_xEntityID = m_pxParentScene->CreateEntity();
	Zenith_Assert(pxScene->m_xEntityComponents.Get(m_xEntityID.m_uIndex).empty(),
		"Entity slot %u already has components - registry not cleared or ID collision", m_xEntityID.m_uIndex);

	// Set initial state directly in the slot
	Zenith_Scene::Zenith_EntitySlot& xSlot = pxScene->m_xEntitySlots.Get(m_xEntityID.m_uIndex);
	xSlot.m_strName = strName;
	xSlot.m_bEnabled = true;
	xSlot.m_bTransient = true;  // Default: transient

	AddComponent<Zenith_TransformComponent>();
}

void Zenith_Entity::Initialise(Zenith_Scene* pxScene, Zenith_EntityID xID, const std::string& strName)
{
	Zenith_Assert(pxScene != nullptr, "Entity initialised with null scene");

	m_pxParentScene = pxScene;
	m_xEntityID = xID;
	Zenith_Assert(pxScene->m_xEntityComponents.Get(m_xEntityID.m_uIndex).empty(),
		"Entity slot %u already has components - registry not cleared or ID collision", m_xEntityID.m_uIndex);

	// Set initial state directly in the slot
	Zenith_Scene::Zenith_EntitySlot& xSlot = pxScene->m_xEntitySlots.Get(m_xEntityID.m_uIndex);
	xSlot.m_strName = strName;
	xSlot.m_bEnabled = true;
	xSlot.m_bTransient = false;  // Loaded entities are persistent

	AddComponent<Zenith_TransformComponent>();
}

//------------------------------------------------------------------------------
// Entity State Accessors (delegate to EntitySlot)
//------------------------------------------------------------------------------

const std::string& Zenith_Entity::GetName() const
{
	Zenith_Assert(IsValid(), "GetName: Entity handle is invalid (idx=%u, gen=%u)",
		m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	return m_pxParentScene->m_xEntitySlots.Get(m_xEntityID.m_uIndex).m_strName;
}

void Zenith_Entity::SetName(const std::string& strName)
{
	Zenith_Assert(IsValid(), "SetName: Entity handle is invalid (idx=%u, gen=%u)",
		m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	m_pxParentScene->m_xEntitySlots.Get(m_xEntityID.m_uIndex).m_strName = strName;
	// No sync needed - slot is the single source of truth!
}

bool Zenith_Entity::IsEnabled() const
{
	Zenith_Assert(IsValid(), "IsEnabled: Entity handle is invalid (idx=%u, gen=%u)",
		m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	return m_pxParentScene->m_xEntitySlots.Get(m_xEntityID.m_uIndex).m_bEnabled;
}

void Zenith_Entity::SetEnabled(bool bEnabled)
{
	Zenith_Assert(IsValid(), "SetEnabled: Entity handle is invalid (idx=%u, gen=%u)",
		m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);

	Zenith_Scene::Zenith_EntitySlot& xSlot = m_pxParentScene->m_xEntitySlots.Get(m_xEntityID.m_uIndex);
	if (xSlot.m_bEnabled == bEnabled)
	{
		return;
	}

	xSlot.m_bEnabled = bEnabled;

	// Dispatch OnEnable or OnDisable to all components
	if (bEnabled)
	{
		Zenith_ComponentMetaRegistry::Get().DispatchOnEnable(*this);
	}
	else
	{
		Zenith_ComponentMetaRegistry::Get().DispatchOnDisable(*this);
	}
}

bool Zenith_Entity::IsTransient() const
{
	Zenith_Assert(IsValid(), "IsTransient: Entity handle is invalid (idx=%u, gen=%u)",
		m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	return m_pxParentScene->m_xEntitySlots.Get(m_xEntityID.m_uIndex).m_bTransient;
}

void Zenith_Entity::SetTransient(bool bTransient)
{
	Zenith_Assert(IsValid(), "SetTransient: Entity handle is invalid (idx=%u, gen=%u)",
		m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	m_pxParentScene->m_xEntitySlots.Get(m_xEntityID.m_uIndex).m_bTransient = bTransient;
	// No sync needed - slot is the single source of truth!
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
		Zenith_Entity xParent = m_pxParentScene->GetEntity(xParentID);
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
	Zenith_Assert(IsValid(), "WriteToDataStream: Entity handle is invalid");

	// Write entity index only (generation is runtime-only for stale detection)
	xStream << m_xEntityID.m_uIndex;
	xStream << GetName();  // Get name from slot

	// Serialize all components using the ComponentMeta registry
	Zenith_ComponentMetaRegistry::Get().SerializeEntityComponents(
		*const_cast<Zenith_Entity*>(this), xStream);
}

void Zenith_Entity::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Read entity index - generation will be assigned fresh on load
	uint32_t uFileIndex;
	xStream >> uFileIndex;

	std::string strName;
	xStream >> strName;

	// Set the name in the slot if we have a valid entity
	if (IsValid())
	{
		m_pxParentScene->m_xEntitySlots.Get(m_xEntityID.m_uIndex).m_strName = strName;
	}

	// Note: m_xEntityID is set by the scene during loading, not here
	// (old format support - scene handles ID assignment now)

	// Deserialize all components using the ComponentMeta registry
	// (TransformComponent reads pending parent ID - hierarchy rebuilt after all entities loaded)
	Zenith_ComponentMetaRegistry::Get().DeserializeEntityComponents(*this, xStream);
}
