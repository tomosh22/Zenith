#include "Zenith.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "DataStream/Zenith_DataStream.h"

//------------------------------------------------------------------------------
// Scene Data Access
//------------------------------------------------------------------------------

Zenith_SceneData* Zenith_Entity::GetSceneData() const
{
	if (!m_xEntityID.IsValid()) return nullptr;
	if (m_xEntityID.m_uIndex >= Zenith_SceneData::s_axEntitySlots.GetSize()) return nullptr;

	const Zenith_SceneData::Zenith_EntitySlot& xSlot = Zenith_SceneData::s_axEntitySlots.Get(m_xEntityID.m_uIndex);
	if (!xSlot.m_bOccupied || xSlot.m_uGeneration != m_xEntityID.m_uGeneration)
		return nullptr;

	// Fast path: compare cached int handle (never dereference stale pointer)
	if (m_pxCachedSceneData && m_iCachedSceneHandle == xSlot.m_iSceneHandle)
		return m_pxCachedSceneData;

	// Slow path: look up SceneData from scene handle and cache it
	m_pxCachedSceneData = Zenith_SceneManager::GetSceneDataByHandle(xSlot.m_iSceneHandle);
	m_iCachedSceneHandle = xSlot.m_iSceneHandle;
	return m_pxCachedSceneData;
}

//------------------------------------------------------------------------------
// Validity Check
//------------------------------------------------------------------------------

bool Zenith_Entity::IsValid() const
{
	Zenith_SceneData* pxSceneData = GetSceneData();
	return pxSceneData != nullptr && pxSceneData->EntityExists(m_xEntityID);
}

Zenith_Scene Zenith_Entity::GetScene() const
{
	if (!m_xEntityID.IsValid()) return Zenith_Scene::INVALID_SCENE;
	if (m_xEntityID.m_uIndex >= Zenith_SceneData::s_axEntitySlots.GetSize()) return Zenith_Scene::INVALID_SCENE;

	const Zenith_SceneData::Zenith_EntitySlot& xSlot = Zenith_SceneData::s_axEntitySlots.Get(m_xEntityID.m_uIndex);
	if (!xSlot.m_bOccupied || xSlot.m_uGeneration != m_xEntityID.m_uGeneration)
		return Zenith_Scene::INVALID_SCENE;

	// Use the global slot's current scene handle (survives cross-scene moves)
	return Zenith_SceneManager::GetSceneFromHandle(xSlot.m_iSceneHandle);
}

//------------------------------------------------------------------------------
// Constructors - Create NEW entities in the scene
//------------------------------------------------------------------------------

// Constructor from SceneData pointer and EntityID (used by SceneData::GetEntity)
Zenith_Entity::Zenith_Entity(Zenith_SceneData* pxSceneData, Zenith_EntityID xID)
	: m_xEntityID(xID)
	, m_pxCachedSceneData(pxSceneData)
	, m_iCachedSceneHandle(pxSceneData->m_iHandle)
{
	Zenith_Assert(pxSceneData != nullptr, "Entity created with null scene data");
}

Zenith_Entity::Zenith_Entity(Zenith_SceneData* pxSceneData, const std::string& strName)
	: m_pxCachedSceneData(pxSceneData)
	, m_iCachedSceneHandle(pxSceneData->m_iHandle)
{
	Zenith_Assert(pxSceneData != nullptr, "Entity created with null scene data");

	// CreateEntity allocates a slot and returns a generation-aware ID
	m_xEntityID = pxSceneData->CreateEntity();
	Zenith_Assert(Zenith_SceneData::s_axEntityComponents.Get(m_xEntityID.m_uIndex).empty(),
		"Entity slot %u already has components - registry not cleared or ID collision", m_xEntityID.m_uIndex);

	// Set initial state directly in the slot (single source of truth)
	Zenith_SceneData::Zenith_EntitySlot& xSlot = Zenith_SceneData::s_axEntitySlots.Get(m_xEntityID.m_uIndex);
	xSlot.m_strName = strName;
	xSlot.m_bEnabled = true;
	xSlot.m_bTransient = true;  // Default: transient (not saved)

	AddComponent<Zenith_TransformComponent>();

	// Track entities created during Update() - they won't receive callbacks until next frame
	pxSceneData->RegisterCreatedDuringUpdate(m_xEntityID);

	// Unity parity: dispatch Awake/OnEnable immediately for runtime-created entities
	pxSceneData->DispatchImmediateLifecycleForRuntime(m_xEntityID);
}

//------------------------------------------------------------------------------
// Hierarchy Enable/Disable Propagation
//------------------------------------------------------------------------------

void Zenith_Entity::PropagateHierarchyEnabled(Zenith_SceneData* pxSceneData, Zenith_EntityID xParentID, bool bBecomingActive)
{
	Zenith_Entity xParent = pxSceneData->GetEntity(xParentID);
	Zenith_Vector<Zenith_EntityID> axChildIDs = xParent.GetChildEntityIDs();
	for (u_int i = 0; i < axChildIDs.GetSize(); ++i)
	{
		if (!pxSceneData->EntityExists(axChildIDs.Get(i))) continue;

		Zenith_Entity xChild = pxSceneData->GetEntity(axChildIDs.Get(i));
		if (!xChild.IsEnabled()) continue;  // Only propagate to children whose activeSelf is true

		if (bBecomingActive)
		{
			if (!pxSceneData->IsOnEnableDispatched(axChildIDs.Get(i)))
			{
				Zenith_ComponentMetaRegistry::Get().DispatchOnEnable(xChild);
				pxSceneData->SetOnEnableDispatched(axChildIDs.Get(i), true);
			}
		}
		else
		{
			if (pxSceneData->IsOnEnableDispatched(axChildIDs.Get(i)))
			{
				Zenith_ComponentMetaRegistry::Get().DispatchOnDisable(xChild);
				pxSceneData->SetOnEnableDispatched(axChildIDs.Get(i), false);
			}
		}

		// Recurse to grandchildren
		PropagateHierarchyEnabled(pxSceneData, axChildIDs.Get(i), bBecomingActive);
	}
}

//------------------------------------------------------------------------------
// Entity State Accessors (delegate to EntitySlot)
//------------------------------------------------------------------------------

const std::string& Zenith_Entity::GetName() const
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetName must be called from main thread");
	Zenith_SceneData* pxSceneData = GetSceneData();
	Zenith_Assert(pxSceneData != nullptr && pxSceneData->EntityExists(m_xEntityID),
		"GetName: Entity handle is invalid (idx=%u, gen=%u)", m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	return Zenith_SceneData::s_axEntitySlots.Get(m_xEntityID.m_uIndex).m_strName;
}

void Zenith_Entity::SetName(const std::string& strName)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "SetName must be called from main thread");
	Zenith_SceneData* pxSceneData = GetSceneData();
	Zenith_Assert(pxSceneData != nullptr && pxSceneData->EntityExists(m_xEntityID),
		"SetName: Entity handle is invalid (idx=%u, gen=%u)", m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	Zenith_SceneData::s_axEntitySlots.Get(m_xEntityID.m_uIndex).m_strName = strName;
	pxSceneData->MarkDirty();
}

bool Zenith_Entity::IsEnabled() const
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "IsEnabled must be called from main thread");
	Zenith_SceneData* pxSceneData = GetSceneData();
	Zenith_Assert(pxSceneData != nullptr && pxSceneData->EntityExists(m_xEntityID),
		"IsEnabled: Entity handle is invalid (idx=%u, gen=%u)", m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	return Zenith_SceneData::s_axEntitySlots.Get(m_xEntityID.m_uIndex).m_bEnabled;
}

bool Zenith_Entity::IsActiveInHierarchy() const
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "IsActiveInHierarchy must be called from main thread");
	Zenith_SceneData* pxSceneData = GetSceneData();
	if (!pxSceneData || !pxSceneData->EntityExists(m_xEntityID)) return false;
	if (pxSceneData->IsBeingDestroyed()) return false;

	Zenith_SceneData::Zenith_EntitySlot& xSlot = Zenith_SceneData::s_axEntitySlots.Get(m_xEntityID.m_uIndex);

	// Check own enabled flag first (fast path)
	if (!xSlot.m_bEnabled) return false;

	// Use cached value if clean
	if (!xSlot.m_bActiveInHierarchyDirty)
	{
		return xSlot.m_bActiveInHierarchy;
	}

	// Rebuild: walk up the parent chain checking each ancestor
	bool bActive = true;
	Zenith_TransformComponent* pxTransform = TryGetComponent<Zenith_TransformComponent>();
	if (!pxTransform) return false;
	Zenith_EntityID xCurrentParent = pxTransform->GetParentEntityID();
	while (xCurrentParent.IsValid())
	{
		if (xCurrentParent.m_uIndex >= Zenith_SceneData::s_axEntitySlots.GetSize()) { bActive = false; break; }
		const Zenith_SceneData::Zenith_EntitySlot& xParentSlot = Zenith_SceneData::s_axEntitySlots.Get(xCurrentParent.m_uIndex);
		if (!xParentSlot.m_bOccupied || xParentSlot.m_uGeneration != xCurrentParent.m_uGeneration) { bActive = false; break; }
		if (!xParentSlot.m_bEnabled) { bActive = false; break; }

		// Get parent's parent via transform component
		if (!pxSceneData->EntityHasComponent<Zenith_TransformComponent>(xCurrentParent)) break;
		xCurrentParent = pxSceneData->GetComponentFromEntity<Zenith_TransformComponent>(xCurrentParent).GetParentEntityID();
	}

	xSlot.m_bActiveInHierarchy = bActive;
	xSlot.m_bActiveInHierarchyDirty = false;
	return bActive;
}

void Zenith_Entity::SetEnabled(bool bEnabled)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "SetEnabled must be called from main thread");
	Zenith_SceneData* pxSceneData = GetSceneData();
	Zenith_Assert(pxSceneData != nullptr && pxSceneData->EntityExists(m_xEntityID),
		"SetEnabled: Entity handle is invalid (idx=%u, gen=%u)", m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	Zenith_SceneData::Zenith_EntitySlot& xSlot = Zenith_SceneData::s_axEntitySlots.Get(m_xEntityID.m_uIndex);
	if (xSlot.m_bEnabled == bEnabled)
	{
		return;
	}

	xSlot.m_bEnabled = bEnabled;
	pxSceneData->MarkDirty();

	// Invalidate cached activeInHierarchy for this entity and all descendants
	Zenith_SceneData::InvalidateActiveInHierarchyCache(m_xEntityID);

	// Dispatch OnEnable or OnDisable to all components
	// Unity behavior: OnEnable only fires if the entity is actually active in the hierarchy
	// (i.e. all ancestors are also enabled). Setting activeSelf=true on a child under a
	// disabled parent should NOT dispatch OnEnable.
	if (bEnabled)
	{
		bool bActiveInHierarchy = IsActiveInHierarchy();
		if (bActiveInHierarchy)
		{
			Zenith_ComponentMetaRegistry::Get().DispatchOnEnable(*this);
			xSlot.m_bOnEnableDispatched = true;

			// Unity behavior: Start() is called on the first frame AFTER the entity becomes active,
			// not in the same call stack as SetActive(true). Defer to DispatchPendingStarts.
			if (!pxSceneData->IsEntityStarted(m_xEntityID))
			{
				pxSceneData->MarkEntityPendingStart(m_xEntityID);
			}
		}

		// Propagate to children whose activeSelf is true (Unity's activeInHierarchy behavior)
		PropagateHierarchyEnabled(pxSceneData, m_xEntityID, bActiveInHierarchy);
	}
	else
	{
		if (xSlot.m_bOnEnableDispatched)
		{
			Zenith_ComponentMetaRegistry::Get().DispatchOnDisable(*this);
			xSlot.m_bOnEnableDispatched = false;
		}

		// When a parent is disabled, children that were activeInHierarchy receive OnDisable.
		PropagateHierarchyEnabled(pxSceneData, m_xEntityID, false);
	}
}

bool Zenith_Entity::IsTransient() const
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "IsTransient must be called from main thread");
	Zenith_SceneData* pxSceneData = GetSceneData();
	Zenith_Assert(pxSceneData != nullptr && pxSceneData->EntityExists(m_xEntityID),
		"IsTransient: Entity handle is invalid (idx=%u, gen=%u)", m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	return Zenith_SceneData::s_axEntitySlots.Get(m_xEntityID.m_uIndex).m_bTransient;
}

void Zenith_Entity::SetTransient(bool bTransient)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "SetTransient must be called from main thread");
	Zenith_SceneData* pxSceneData = GetSceneData();
	Zenith_Assert(pxSceneData != nullptr && pxSceneData->EntityExists(m_xEntityID),
		"SetTransient: Entity handle is invalid (idx=%u, gen=%u)", m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	Zenith_SceneData::s_axEntitySlots.Get(m_xEntityID.m_uIndex).m_bTransient = bTransient;
}

//------------------------------------------------------------------------------
// Persistence Across Scene Loads
//------------------------------------------------------------------------------

void Zenith_Entity::DontDestroyOnLoad()
{
	Zenith_SceneManager::MarkEntityPersistent(*this);
}

//------------------------------------------------------------------------------
// Parent/Child Hierarchy (delegates to TransformComponent)
//------------------------------------------------------------------------------

Zenith_EntityID Zenith_Entity::GetParentEntityID() const
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetParentEntityID must be called from main thread");
	const Zenith_TransformComponent& xTransform = GetComponent<Zenith_TransformComponent>();
	return xTransform.GetParentEntityID();
}

bool Zenith_Entity::HasParent() const
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "HasParent must be called from main thread");
	return GetComponent<Zenith_TransformComponent>().HasParent();
}

void Zenith_Entity::SetParent(Zenith_EntityID xParentID)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "SetParent must be called from main thread");
	Zenith_Assert(IsValid(), "SetParent: Entity handle is invalid (idx=%u, gen=%u)",
		m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);

	Zenith_SceneData* pxSceneData = GetSceneData();
	Zenith_TransformComponent& xTransform = GetComponent<Zenith_TransformComponent>();
	if (!xParentID.IsValid())
	{
		xTransform.SetParent(nullptr);
	}
	else
	{
		Zenith_Assert(pxSceneData->EntityExists(xParentID), "SetParent: Parent entity (idx=%u, gen=%u) does not exist",
			xParentID.m_uIndex, xParentID.m_uGeneration);
		Zenith_Entity xParent = pxSceneData->GetEntity(xParentID);
		xTransform.SetParent(&xParent.GetComponent<Zenith_TransformComponent>());
	}

	// Invalidate cached activeInHierarchy (new parent may have different enabled state)
	Zenith_SceneData::InvalidateActiveInHierarchyCache(m_xEntityID);
}

const Zenith_Vector<Zenith_EntityID>& Zenith_Entity::GetChildEntityIDs() const
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetChildEntityIDs must be called from main thread");
	const Zenith_TransformComponent& xTransform = GetComponent<Zenith_TransformComponent>();
	return xTransform.GetChildEntityIDs();
}

bool Zenith_Entity::HasChildren() const
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "HasChildren must be called from main thread");
	return GetComponent<Zenith_TransformComponent>().GetChildCount() > 0;
}

uint32_t Zenith_Entity::GetChildCount() const
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetChildCount must be called from main thread");
	return GetComponent<Zenith_TransformComponent>().GetChildCount();
}

bool Zenith_Entity::IsRoot() const
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "IsRoot must be called from main thread");
	return GetComponent<Zenith_TransformComponent>().IsRoot();
}

Zenith_TransformComponent& Zenith_Entity::GetTransform()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetTransform must be called from main thread");
	return GetComponent<Zenith_TransformComponent>();
}

//------------------------------------------------------------------------------
// Serialization
//------------------------------------------------------------------------------

void Zenith_Entity::WriteToDataStream(Zenith_DataStream& xStream)
{
	Zenith_Assert(IsValid(), "WriteToDataStream: Entity handle is invalid");

	// Write entity index only (generation is runtime-only for stale detection)
	xStream << m_xEntityID.m_uIndex;
	xStream << GetName();  // Get name from slot

	// Serialize all components using the ComponentMeta registry
	Zenith_ComponentMetaRegistry::Get().SerializeEntityComponents(*this, xStream);
}

void Zenith_Entity::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Read entity index - generation will be assigned fresh on load
	uint32_t uFileIndex;
	xStream >> uFileIndex;

	std::string strName;
	xStream >> strName;

	// Set the name in the slot if we have a valid entity
	Zenith_SceneData* pxSceneData = GetSceneData();
	if (pxSceneData != nullptr && IsValid())
	{
		Zenith_SceneData::s_axEntitySlots.Get(m_xEntityID.m_uIndex).m_strName = strName;
	}

	// Note: m_xEntityID is set by the scene during loading, not here
	// (old format support - scene handles ID assignment now)

	// Deserialize all components using the ComponentMeta registry
	// (TransformComponent reads pending parent ID - hierarchy rebuilt after all entities loaded)
	Zenith_ComponentMetaRegistry::Get().DeserializeEntityComponents(*this, xStream);
}

//------------------------------------------------------------------------------
// Destruction (Unity-style API)
//------------------------------------------------------------------------------

void Zenith_Entity::Destroy()
{
	Zenith_SceneManager::Destroy(*this);
}

void Zenith_Entity::DestroyImmediate()
{
	Zenith_SceneManager::DestroyImmediate(*this);
}
