#include "Zenith.h"

#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "DataStream/Zenith_DataStream.h"
#ifdef ZENITH_TOOLS
#include "Editor/Zenith_Editor.h"
#endif

// Static member definitions - global entity storage shared across all scenes
Zenith_Vector<Zenith_SceneData::Zenith_EntitySlot> Zenith_SceneData::s_axEntitySlots;
Zenith_Vector<uint32_t> Zenith_SceneData::s_axFreeEntityIndices;
Zenith_Vector<std::unordered_map<Zenith_SceneData::TypeID, u_int>> Zenith_SceneData::s_axEntityComponents; // #TODO: Replace with engine hash map

void Zenith_SceneData::ResetGlobalEntityStorage()
{
	s_axEntitySlots.Clear();
	s_axFreeEntityIndices.Clear();
	s_axEntityComponents.Clear();
}

void Zenith_SceneData::InvalidateActiveInHierarchyCache(Zenith_EntityID xID)
{
	if (xID.m_uIndex >= s_axEntitySlots.GetSize()) return;
	Zenith_EntitySlot& xSlot = s_axEntitySlots.Get(xID.m_uIndex);
	if (!xSlot.m_bOccupied || xSlot.m_uGeneration != xID.m_uGeneration) return;

	xSlot.m_bActiveInHierarchyDirty = true;

	// Recursively invalidate children via TransformComponent
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneDataByHandle(xSlot.m_iSceneHandle);
	if (!pxSceneData) return;
	if (!pxSceneData->EntityHasComponent<Zenith_TransformComponent>(xID)) return;

	const Zenith_TransformComponent& xTransform = pxSceneData->GetComponentFromEntity<Zenith_TransformComponent>(xID);
	const Zenith_Vector<Zenith_EntityID>& axChildIDs = xTransform.GetChildEntityIDs();
	for (u_int i = 0; i < axChildIDs.GetSize(); ++i)
	{
		InvalidateActiveInHierarchyCache(axChildIDs.Get(i));
	}
}

Zenith_SceneData::Zenith_SceneData()
{
	// Scene is ready for use immediately after construction
	m_bIsLoaded = true;
}

Zenith_SceneData::~Zenith_SceneData()
{
	Reset();
}

void Zenith_SceneData::Reset()
{
	Zenith_Assert(!m_bIsUpdating, "Reset() called during Update - this would corrupt iteration state");
	m_bIsBeingDestroyed = true;

	// Unity parity: Two-pass destruction for all active entities
	// Pass 1: OnDisable for all entities (all entities still alive during this pass)
	// Reverse iteration so later-created entities are disabled first
	for (int i = static_cast<int>(m_xActiveEntities.GetSize()) - 1; i >= 0; --i)
	{
		Zenith_EntityID xID = m_xActiveEntities.Get(i);
		if (EntityExists(xID))
		{
			Zenith_Entity xEntity(this, xID);
			Zenith_EntitySlot& xSlot = s_axEntitySlots.Get(xID.m_uIndex);
			if (xEntity.IsEnabled() && xSlot.m_bOnEnableDispatched)
			{
				Zenith_ComponentMetaRegistry::Get().DispatchOnDisable(xEntity);
				xSlot.m_bOnEnableDispatched = false;
			}
		}
	}

	// Pass 2: OnDestroy + component removal in correct reverse serialization order
	// Using RemoveAllComponents ensures components are destroyed in dependency-safe order
	// (e.g. ScriptComponent before ColliderComponent before TransformComponent)
	for (int i = static_cast<int>(m_xActiveEntities.GetSize()) - 1; i >= 0; --i)
	{
		Zenith_EntityID xID = m_xActiveEntities.Get(i);
		if (EntityExists(xID))
		{
			Zenith_Entity xEntity(this, xID);
			Zenith_ComponentMetaRegistry::Get().RemoveAllComponents(xEntity);
		}
	}

	// Delete now-empty component pools
	for (Zenith_Vector<Zenith_ComponentPoolBase*>::Iterator xIt(m_xComponents); !xIt.Done(); xIt.Next())
	{
		Zenith_ComponentPoolBase* pxPool = xIt.GetData();
		if (pxPool)
		{
			delete pxPool;
		}
	}
	m_xComponents.Clear();

	// Free global slots for entities that belonged to this scene
	for (u_int i = 0; i < m_xActiveEntities.GetSize(); ++i)
	{
		Zenith_EntityID xID = m_xActiveEntities.Get(i);
		if (xID.m_uIndex < s_axEntitySlots.GetSize())
		{
			Zenith_EntitySlot& xSlot = s_axEntitySlots.Get(xID.m_uIndex);
			if (xSlot.m_bOccupied && xSlot.m_uGeneration == xID.m_uGeneration)
			{
				s_axEntityComponents.Get(xID.m_uIndex).clear();
				xSlot.m_bOccupied = false;
				xSlot.m_bMarkedForDestruction = false;
				xSlot.m_bAwoken = false;
				xSlot.m_bStarted = false;
				xSlot.m_bPendingStart = false;
				xSlot.m_bCreatedDuringUpdate = false;
				xSlot.m_bOnEnableDispatched = false;
				xSlot.m_bActiveInHierarchy = true;
				xSlot.m_bActiveInHierarchyDirty = true;
				xSlot.m_iSceneHandle = -1;
				s_axFreeEntityIndices.PushBack(xID.m_uIndex);
			}
		}
	}

	m_xActiveEntities.Clear();
	m_axNewlyCreatedEntities.Clear();
	m_axPendingStartEntities.Clear();
	m_uPendingStartCount = 0;
	m_xPendingDestruction.Clear();
	m_axTimedDestructions.Clear();
	m_bIsUpdating = false;
	m_xMainCameraEntity = INVALID_ENTITY_ID;

	// Clear root entity cache
	m_axCachedRootEntities.Clear();
	m_bRootEntitiesDirty = true;

	m_bIsBeingDestroyed = false;
}

//==========================================================================
// Root Entity Cache (O(1) count access for Unity parity)
//==========================================================================

void Zenith_SceneData::RebuildRootEntityCache()
{
	m_axCachedRootEntities.Clear();
	for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xID = m_xActiveEntities.Get(u);
		if (EntityExists(xID))
		{
			Zenith_Entity xEntity = GetEntity(xID);
			if (xEntity.IsRoot())
			{
				m_axCachedRootEntities.PushBack(xID);
			}
		}
	}
	m_bRootEntitiesDirty = false;
}

uint32_t Zenith_SceneData::GetCachedRootEntityCount()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetCachedRootEntityCount must be called from main thread");
	if (m_bRootEntitiesDirty)
	{
		RebuildRootEntityCache();
	}
	return m_axCachedRootEntities.GetSize();
}

void Zenith_SceneData::GetCachedRootEntities(Zenith_Vector<Zenith_EntityID>& axOut)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetCachedRootEntities must be called from main thread");
	if (m_bRootEntitiesDirty)
	{
		RebuildRootEntityCache();
	}
	for (u_int u = 0; u < m_axCachedRootEntities.GetSize(); ++u)
	{
		axOut.PushBack(m_axCachedRootEntities.Get(u));
	}
}

//==========================================================================
// Entity Management
//==========================================================================

Zenith_EntityID Zenith_SceneData::CreateEntity()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "CreateEntity must be called from main thread");
	uint32_t uIndex;
	uint32_t uGeneration;

	// Try to reuse a free slot. Skip any slots with generation overflow (retired permanently).
	bool bFoundFreeSlot = false;
	while (s_axFreeEntityIndices.GetSize() > 0)
	{
		uIndex = s_axFreeEntityIndices.GetBack();
		s_axFreeEntityIndices.PopBack();

		Zenith_EntitySlot& xSlot = s_axEntitySlots.Get(uIndex);

		// Check for generation overflow (very unlikely but catastrophic if it happens)
		// After UINT32_MAX reuses of the same slot, generation wraps to 0 causing
		// stale handles to incorrectly appear valid.
		if (xSlot.m_uGeneration == UINT32_MAX)
		{
			Zenith_Warning(LOG_CATEGORY_ECS,
				"Entity slot %u generation overflow - retiring slot to prevent stale access bugs", uIndex);
			// Don't reuse this slot - try the next free index
			continue;
		}

		xSlot.m_uGeneration++;
		uGeneration = xSlot.m_uGeneration;
		xSlot.m_bOccupied = true;
		xSlot.m_bMarkedForDestruction = false;
		xSlot.m_iSceneHandle = m_iHandle;
		xSlot.m_bAwoken = false;
		xSlot.m_bStarted = false;
		xSlot.m_bPendingStart = false;
		xSlot.m_bCreatedDuringUpdate = false;
		xSlot.m_bOnEnableDispatched = false;
		xSlot.m_bActiveInHierarchy = true;
		xSlot.m_bActiveInHierarchyDirty = true;
		bFoundFreeSlot = true;
		break;
	}

	// No free slots available (or all were retired) - allocate fresh
	if (!bFoundFreeSlot)
	{
		uIndex = s_axEntitySlots.GetSize();
		uGeneration = 1;

		Zenith_EntitySlot xNewSlot;
		xNewSlot.m_uGeneration = uGeneration;
		xNewSlot.m_bOccupied = true;
		xNewSlot.m_bMarkedForDestruction = false;
		xNewSlot.m_iSceneHandle = m_iHandle;
		s_axEntitySlots.PushBack(std::move(xNewSlot));
	}

	while (s_axEntityComponents.GetSize() <= uIndex)
	{
		s_axEntityComponents.PushBack({});
	}

	Zenith_EntityID xNewID = { uIndex, uGeneration };
	m_xActiveEntities.PushBack(xNewID);
	m_axNewlyCreatedEntities.PushBack(xNewID);
	MarkDirty();
	InvalidateRootEntityCache();  // New entity might be a root
	return xNewID;
}

void Zenith_SceneData::CollectHierarchyDepthFirst(Zenith_EntityID xID, Zenith_Vector<Zenith_EntityID>& axOut)
{
	Zenith_Entity xEntity(this, xID);
	Zenith_Vector<Zenith_EntityID> axChildIDs = xEntity.GetChildEntityIDs();
	for (u_int i = 0; i < axChildIDs.GetSize(); ++i)
	{
		Zenith_EntityID xChildID = axChildIDs.Get(i);
		if (EntityExists(xChildID))
		{
			CollectHierarchyDepthFirst(xChildID, axOut);
		}
	}
	axOut.PushBack(xID);
}

void Zenith_SceneData::RemoveEntity(Zenith_EntityID xID)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "RemoveEntity must be called from main thread");
	if (!EntityExists(xID))
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "Attempted to remove non-existent entity (idx=%u, gen=%u)", xID.m_uIndex, xID.m_uGeneration);
		return;
	}

	// Collect entire hierarchy depth-first (children before parent)
	Zenith_Vector<Zenith_EntityID> axHierarchy;
	CollectHierarchyDepthFirst(xID, axHierarchy);

	// Clear main camera reference if any entity in hierarchy is the camera
	for (u_int i = 0; i < axHierarchy.GetSize(); ++i)
	{
		if (m_xMainCameraEntity.IsValid() && m_xMainCameraEntity == axHierarchy.Get(i))
		{
			m_xMainCameraEntity = INVALID_ENTITY_ID;
			break;
		}
	}

	// Unity parity: Two-pass destruction
	// Pass 1: OnDisable for entire hierarchy (all entities still alive during this pass)
	for (u_int i = 0; i < axHierarchy.GetSize(); ++i)
	{
		Zenith_EntityID xEntityID = axHierarchy.Get(i);
		if (!EntityExists(xEntityID)) continue;
		Zenith_EntitySlot& xSlot = s_axEntitySlots.Get(xEntityID.m_uIndex);
		Zenith_Entity xEntity(this, xEntityID);
		if (xEntity.IsEnabled() && xSlot.m_bOnEnableDispatched)
		{
			Zenith_ComponentMetaRegistry::Get().DispatchOnDisable(xEntity);
			xSlot.m_bOnEnableDispatched = false;
		}
	}

	// Pass 2: OnDestroy + component removal + slot cleanup
	for (u_int i = 0; i < axHierarchy.GetSize(); ++i)
	{
		Zenith_EntityID xEntityID = axHierarchy.Get(i);
		if (!EntityExists(xEntityID)) continue;
		Zenith_Entity xEntity(this, xEntityID);
		Zenith_EntitySlot& xSlot = s_axEntitySlots.Get(xEntityID.m_uIndex);

		Zenith_ComponentMetaRegistry::Get().RemoveAllComponents(xEntity);

		s_axEntityComponents.Get(xEntityID.m_uIndex).clear();

		// Reset lifecycle flags
		xSlot.m_bAwoken = false;
		xSlot.m_bStarted = false;
		if (xSlot.m_bPendingStart)
		{
			Zenith_Assert(m_uPendingStartCount > 0, "PendingStartCount underflow in RemoveEntity");
			xSlot.m_bPendingStart = false;
			m_uPendingStartCount--;
		}
		xSlot.m_bCreatedDuringUpdate = false;
		xSlot.m_bOnEnableDispatched = false;

		xSlot.m_bActiveInHierarchy = true;
		xSlot.m_bActiveInHierarchyDirty = true;
		xSlot.m_bOccupied = false;
		xSlot.m_bMarkedForDestruction = false;
		xSlot.m_iSceneHandle = -1;
		s_axFreeEntityIndices.PushBack(xEntityID.m_uIndex);

		m_xActiveEntities.EraseValue(xEntityID);
	}

	MarkDirty();
	InvalidateRootEntityCache();

	Zenith_Log(LOG_CATEGORY_SCENE, "Entity (idx=%u, gen=%u) and hierarchy removed from scene", xID.m_uIndex, xID.m_uGeneration);
}

Zenith_Entity Zenith_SceneData::GetEntity(Zenith_EntityID xID)
{
	Zenith_Assert(EntityExists(xID), "GetEntity: Entity (idx=%u, gen=%u) is invalid", xID.m_uIndex, xID.m_uGeneration);
	return Zenith_Entity(this, xID);
}

Zenith_Entity Zenith_SceneData::TryGetEntity(Zenith_EntityID xID)
{
	if (!EntityExists(xID))
	{
		return Zenith_Entity();
	}
	return Zenith_Entity(this, xID);
}

Zenith_Entity Zenith_SceneData::FindEntityByName(const std::string& strName)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "FindEntityByName must be called from main thread");
	for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xID = m_xActiveEntities.Get(u);
		if (EntityExists(xID))
		{
			const Zenith_EntitySlot& xSlot = s_axEntitySlots.Get(xID.m_uIndex);
			if (xSlot.m_strName == strName)
			{
				return Zenith_Entity(this, xID);
			}
		}
	}
	return Zenith_Entity();
}

const Zenith_SceneData::Zenith_EntitySlot& Zenith_SceneData::GetSlot(Zenith_EntityID xID) const
{
	Zenith_Assert(EntityExists(xID), "GetSlot: Entity (idx=%u, gen=%u) is invalid", xID.m_uIndex, xID.m_uGeneration);
	return s_axEntitySlots.Get(xID.m_uIndex);
}

//==========================================================================
// Camera
//==========================================================================

void Zenith_SceneData::SetMainCameraEntity(Zenith_EntityID xEntity)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "SetMainCameraEntity must be called from main thread");
	m_xMainCameraEntity = xEntity;
}

Zenith_EntityID Zenith_SceneData::GetMainCameraEntity() const
{
	// Read-only: m_xMainCameraEntity is stable during render/animation tasks
	// (main thread does not modify it while worker threads are running)
	Zenith_Assert(Zenith_Multithreading::IsMainThread() || Zenith_SceneManager::s_bRenderTasksActive,
		"GetMainCameraEntity must be called from main thread or during render task execution");
	return m_xMainCameraEntity;
}

Zenith_CameraComponent& Zenith_SceneData::GetMainCamera() const
{
	Zenith_Assert(m_xMainCameraEntity.IsValid() && EntityExists(m_xMainCameraEntity), "GetMainCamera: No valid main camera set");
	return GetComponentFromEntity<Zenith_CameraComponent>(m_xMainCameraEntity);
}

Zenith_CameraComponent* Zenith_SceneData::TryGetMainCamera() const
{
	if (!m_xMainCameraEntity.IsValid() || !EntityExists(m_xMainCameraEntity))
	{
		return nullptr;
	}
	if (!EntityHasComponent<Zenith_CameraComponent>(m_xMainCameraEntity))
	{
		return nullptr;
	}
	return &GetComponentFromEntity<Zenith_CameraComponent>(m_xMainCameraEntity);
}

//==========================================================================
// Deferred Destruction
//==========================================================================

void Zenith_SceneData::MarkForDestruction(Zenith_EntityID xID)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "MarkForDestruction must be called from main thread");
	if (!xID.IsValid()) return;
	if (!EntityExists(xID)) return;

	// Already marked - prevent double-marking and infinite recursion (use global slot flag)
	if (s_axEntitySlots.Get(xID.m_uIndex).m_bMarkedForDestruction) return;

	// Mark children's flags (so scripts can't interact with them), but only push the root
	// entity to m_xPendingDestruction. RemoveEntity handles children recursively.
	MarkChildrenForDestructionRecursive(xID);

	// Mark this entity and add to pending list (only root entities are in the pending list)
	s_axEntitySlots.Get(xID.m_uIndex).m_bMarkedForDestruction = true;
	m_xPendingDestruction.PushBack(xID);
}

void Zenith_SceneData::MarkChildrenForDestructionRecursive(Zenith_EntityID xID)
{
	Zenith_Entity xEntity = GetEntity(xID);
	if (!xEntity.HasComponent<Zenith_TransformComponent>()) return;

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	Zenith_Vector<Zenith_EntityID> axChildIDs = xTransform.GetChildEntityIDs();
	for (u_int u = 0; u < axChildIDs.GetSize(); ++u)
	{
		Zenith_EntityID xChildID = axChildIDs.Get(u);
		if (xChildID.IsValid() && EntityExists(xChildID) && !s_axEntitySlots.Get(xChildID.m_uIndex).m_bMarkedForDestruction)
		{
			s_axEntitySlots.Get(xChildID.m_uIndex).m_bMarkedForDestruction = true;
			MarkChildrenForDestructionRecursive(xChildID);
		}
	}
}

void Zenith_SceneData::MarkForTimedDestruction(Zenith_EntityID xID, float fDelay)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "MarkForTimedDestruction must be called from main thread");
	if (!xID.IsValid() || !EntityExists(xID)) return;

	TimedDestruction xEntry;
	xEntry.m_xEntityID = xID;
	xEntry.m_fTimeRemaining = fDelay;
	m_axTimedDestructions.PushBack(xEntry);
}

bool Zenith_SceneData::IsMarkedForDestruction(Zenith_EntityID xID) const
{
	if (!xID.IsValid()) return false;
	if (xID.m_uIndex >= s_axEntitySlots.GetSize()) return false;
	return s_axEntitySlots.Get(xID.m_uIndex).m_bMarkedForDestruction;
}

void Zenith_SceneData::ProcessPendingDestructions()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "ProcessPendingDestructions must be called from main thread");
	for (int i = static_cast<int>(m_xPendingDestruction.GetSize()) - 1; i >= 0; --i)
	{
		Zenith_EntityID xEntityID = m_xPendingDestruction.Get(i);
		if (EntityExists(xEntityID))
		{
			Zenith_Entity xEntity = GetEntity(xEntityID);
			xEntity.GetComponent<Zenith_TransformComponent>().DetachFromParent();
			RemoveEntity(xEntityID);
		}
	}

	m_xPendingDestruction.Clear();
}

//==========================================================================
// Update
//==========================================================================

void Zenith_SceneData::Update(float fDt)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "Update must be called from main thread");

	m_bIsUpdating = true;

	Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();

	// Snapshot entity IDs before iteration
	Zenith_Vector<Zenith_EntityID> xEntityIDs;
	xEntityIDs.Reserve(m_xActiveEntities.GetSize());
	for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
	{
		xEntityIDs.PushBack(m_xActiveEntities.Get(u));
	}

	// 1. OnAwake/OnEnable for new entities
	// Unity parity: Awake/OnEnable are called immediately when an entity is instantiated,
	// even during Update(). Only Update/LateUpdate are deferred to the next frame.
	// Note: Runtime path calls Awake+OnEnable per-entity (unlike scene load which batches).
	// PERF: Only iterate newly created entities rather than scanning all entities each frame.
	// Loop until stable: entities created during Awake get Awake called immediately (Unity parity).
	Zenith_Vector<Zenith_EntityID> axAllNewEntities;
	constexpr u_int uMAX_AWAKE_ITERATIONS = 100;
	u_int uIteration = 0;
	while (m_axNewlyCreatedEntities.GetSize() > 0 && uIteration < uMAX_AWAKE_ITERATIONS)
	{
		Zenith_Vector<Zenith_EntityID> axNewEntities;
		axNewEntities.Reserve(m_axNewlyCreatedEntities.GetSize());
		for (u_int u = 0; u < m_axNewlyCreatedEntities.GetSize(); ++u)
		{
			axNewEntities.PushBack(m_axNewlyCreatedEntities.Get(u));
			axAllNewEntities.PushBack(m_axNewlyCreatedEntities.Get(u));
		}
		m_axNewlyCreatedEntities.Clear();

		for (u_int u = 0; u < axNewEntities.GetSize(); ++u)
		{
			Zenith_EntityID uID = axNewEntities.Get(u);
			if (!EntityExists(uID)) continue;
			if (IsEntityAwoken(uID)) continue;

			DispatchAwakeForEntity(uID);

			Zenith_EntitySlot& xSlot = s_axEntitySlots.Get(uID.m_uIndex);
			Zenith_Entity xEntity = GetEntity(uID);
			if (xEntity.IsActiveInHierarchy() && !xSlot.m_bOnEnableDispatched)
			{
				xRegistry.DispatchOnEnable(xEntity);
				xSlot.m_bOnEnableDispatched = true;
			}
		}
		uIteration++;
	}
	Zenith_Assert(uIteration < uMAX_AWAKE_ITERATIONS || m_axNewlyCreatedEntities.GetSize() == 0,
		"Awake iteration limit reached (%u) - infinite entity creation in Awake callbacks", uMAX_AWAKE_ITERATIONS);

	// 2. Queue OnStart for new entities (deferred to next frame via DispatchPendingStarts)
	// Unity defers Start() to the frame after Awake/OnEnable, not the same frame
	for (u_int u = 0; u < axAllNewEntities.GetSize(); ++u)
	{
		Zenith_EntityID uID = axAllNewEntities.Get(u);
		if (!EntityExists(uID)) continue;

		if (!IsEntityStarted(uID))
		{
			MarkEntityPendingStart(uID);
		}
	}

	// Note: DispatchPendingStarts is handled by SceneManager::Update() before calling
	// SceneData::Update(), ensuring Start() runs before the first Update() for all entities.

	// 3. OnFixedUpdate (30Hz physics timestep - see SceneManager::fFIXED_TIMESTEP)
	// Note: Fixed time accumulator is managed by SceneManager for all scenes
	// Individual scenes just run fixed update when called

	// 4. OnUpdate (every frame)
	for (u_int u = 0; u < xEntityIDs.GetSize(); ++u)
	{
		Zenith_EntityID uID = xEntityIDs.Get(u);
		if (!EntityExists(uID)) continue;
		if (WasCreatedDuringUpdate(uID)) continue;

		Zenith_Entity xEntity = GetEntity(uID);
		if (!xEntity.IsActiveInHierarchy()) continue;
		xRegistry.DispatchOnUpdate(xEntity, fDt);
	}

	// 5. OnLateUpdate
	for (u_int u = 0; u < xEntityIDs.GetSize(); ++u)
	{
		Zenith_EntityID uID = xEntityIDs.Get(u);
		if (!EntityExists(uID)) continue;
		if (WasCreatedDuringUpdate(uID)) continue;

		Zenith_Entity xEntity = GetEntity(uID);
		if (!xEntity.IsActiveInHierarchy()) continue;
		xRegistry.DispatchOnLateUpdate(xEntity, fDt);
	}

	// 6. Tick timed destructions (Unity Destroy(obj, delay) parity)
	// Note: Unity uses scaled Time.time for Destroy(obj, delay), which is what fDt provides.
	for (int i = static_cast<int>(m_axTimedDestructions.GetSize()) - 1; i >= 0; --i)
	{
		TimedDestruction& xEntry = m_axTimedDestructions.Get(i);

		// Clean up entries for entities that were already destroyed (e.g., via DestroyImmediate or scene unload)
		if (!EntityExists(xEntry.m_xEntityID))
		{
			m_axTimedDestructions.Remove(i);
			continue;
		}

		xEntry.m_fTimeRemaining -= fDt;
		if (xEntry.m_fTimeRemaining <= 0.0f)
		{
			MarkForDestruction(xEntry.m_xEntityID);
			m_axTimedDestructions.Remove(i);
		}
	}

	// 7. Process deferred destructions
	ProcessPendingDestructions();

	m_bIsUpdating = false;

	// Clear created-during-update flags on entity slots
	for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xID = m_xActiveEntities.Get(u);
		if (xID.m_uIndex < s_axEntitySlots.GetSize())
		{
			s_axEntitySlots.Get(xID.m_uIndex).m_bCreatedDuringUpdate = false;
		}
	}
}

void Zenith_SceneData::FixedUpdate(float fFixedDt)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "FixedUpdate must be called from main thread");
	Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();

	// Snapshot entity IDs before iteration - OnFixedUpdate may create/destroy entities
	Zenith_Vector<Zenith_EntityID> xEntityIDs;
	xEntityIDs.Reserve(m_xActiveEntities.GetSize());
	for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
	{
		xEntityIDs.PushBack(m_xActiveEntities.Get(u));
	}

	for (u_int u = 0; u < xEntityIDs.GetSize(); ++u)
	{
		Zenith_EntityID xID = xEntityIDs.Get(u);
		if (!EntityExists(xID)) continue;

		Zenith_Entity xEntity = GetEntity(xID);
		if (!xEntity.IsActiveInHierarchy()) continue;
		xRegistry.DispatchOnFixedUpdate(xEntity, fFixedDt);
	}
}

void Zenith_SceneData::DispatchLifecycleForNewScene()
{
	// Convenience method that runs both phases together (used by DispatchFullLifecycleInit)
	DispatchAwakeForNewScene();
	DispatchEnableAndPendingStartsForNewScene();
}

void Zenith_SceneData::DispatchAwakeForEntity(Zenith_EntityID xEntityID)
{
	if (IsEntityAwoken(xEntityID)) return;
	Zenith_Entity xEntity = GetEntity(xEntityID);
	Zenith_ComponentMetaRegistry::Get().DispatchOnAwake(xEntity);
	MarkEntityAwoken(xEntityID);
}

void Zenith_SceneData::DispatchImmediateLifecycleForRuntime(Zenith_EntityID xID)
{
	// Unity parity: Awake/OnEnable fire immediately when an entity is created at runtime.
	// During scene loading and prefab instantiation, lifecycle is dispatched in batch.
	if (Zenith_SceneManager::s_bIsLoadingScene || Zenith_SceneManager::s_bIsPrefabInstantiating)
		return;

	DispatchAwakeForEntity(xID);

	Zenith_EntitySlot& xSlot = s_axEntitySlots.Get(xID.m_uIndex);
	Zenith_Entity xEntity = GetEntity(xID);
	if (xEntity.IsActiveInHierarchy() && !xSlot.m_bOnEnableDispatched)
	{
		Zenith_ComponentMetaRegistry::Get().DispatchOnEnable(xEntity);
		xSlot.m_bOnEnableDispatched = true;
	}

	// Queue Start for next frame (Unity defers Start to the frame after Awake/OnEnable)
	if (!IsEntityStarted(xID))
	{
		MarkEntityPendingStart(xID);
	}
}

void Zenith_SceneData::DispatchAwakeForNewScene()
{
	// Track that we're in lifecycle dispatch for this scene (for circular load detection)
	// If OnAwake tries to load this same scene, it will be detected as circular
	if (!m_strPath.empty())
	{
		Zenith_SceneManager::PushLifecycleContext(m_strPath);
	}

	// Phase 1: OnAwake for all entities (Unity fires sceneLoaded after Awake and OnEnable but before Start)
	// Wave-drain pattern: process entities in waves. If OnAwake creates new entities,
	// those form the next wave. Guard against infinite creation chains.
	// This matches the runtime Update() pattern in SceneData::Update().
	constexpr u_int uMAX_AWAKE_ITERATIONS = 100;
	u_int uWaveStart = 0;
	u_int uWaveEnd = m_xActiveEntities.GetSize();
	u_int uIteration = 0;
	while (uWaveStart < uWaveEnd)
	{
		for (u_int u = uWaveStart; u < uWaveEnd; ++u)
		{
			Zenith_EntityID xEntityID = m_xActiveEntities.Get(u);
			if (EntityExists(xEntityID))
			{
				DispatchAwakeForEntity(xEntityID);
			}
		}

		uWaveStart = uWaveEnd;
		uWaveEnd = m_xActiveEntities.GetSize();

		// Only count iterations when new entities appeared (a new wave is needed)
		if (uWaveStart < uWaveEnd)
		{
			uIteration++;
			Zenith_Assert(uIteration < uMAX_AWAKE_ITERATIONS,
				"DispatchAwakeForNewScene: Awake iteration limit reached (%u) - "
				"infinite entity creation in Awake callbacks", uMAX_AWAKE_ITERATIONS);
			if (uIteration >= uMAX_AWAKE_ITERATIONS)
			{
				break;
			}
		}
	}

	// Remove from lifecycle context after awake completes
	if (!m_strPath.empty())
	{
		Zenith_SceneManager::PopLifecycleContext(m_strPath);
	}
}

void Zenith_SceneData::DispatchEnableAndPendingStartsForNewScene()
{
	Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();

	// Phase 2: OnEnable for awakened entities (skip if already dispatched during Awake phase)
	for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xEntityID = m_xActiveEntities.Get(u);
		if (EntityExists(xEntityID))
		{
			Zenith_EntitySlot& xSlot = s_axEntitySlots.Get(xEntityID.m_uIndex);
			if (xSlot.m_bOnEnableDispatched) continue;  // Already dispatched (e.g., via SetEnabled during OnAwake)

			Zenith_Entity xEntity = GetEntity(xEntityID);
			if (xEntity.IsActiveInHierarchy())
			{
				xRegistry.DispatchOnEnable(xEntity);
				xSlot.m_bOnEnableDispatched = true;
			}
		}
	}

	// Phase 3: Mark enabled entities as pending Start (deferred to first Update)
	// Unity behavior: Start() is called on the first frame after scene load, not during load
	for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xEntityID = m_xActiveEntities.Get(u);
		if (EntityExists(xEntityID))
		{
			if (!IsEntityStarted(xEntityID))
			{
				Zenith_Entity xEntity = GetEntity(xEntityID);
				if (xEntity.IsActiveInHierarchy())
				{
					MarkEntityPendingStart(xEntityID);
				}
			}
		}
	}
}

void Zenith_SceneData::DispatchPendingStarts()
{
	if (m_uPendingStartCount == 0)
	{
		return;
	}

	Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();

	// Snapshot pending start entities for safe iteration (Start might spawn entities
	// which add to m_axPendingStartEntities - those will be processed next frame)
	Zenith_Vector<Zenith_EntityID> axSnapshot;
	axSnapshot.Reserve(m_axPendingStartEntities.GetSize());
	for (u_int u = 0; u < m_axPendingStartEntities.GetSize(); ++u)
	{
		axSnapshot.PushBack(m_axPendingStartEntities.Get(u));
	}
	m_axPendingStartEntities.Clear();

	// Dispatch OnStart for entities marked as pending start
	// Clear individual flags as we process them; new pending starts added during
	// callbacks will have their flags set and counted by MarkEntityPendingStart
	for (u_int u = 0; u < axSnapshot.GetSize(); ++u)
	{
		Zenith_EntityID xEntityID = axSnapshot.Get(u);
		if (xEntityID.m_uIndex >= s_axEntitySlots.GetSize()) continue;
		Zenith_EntitySlot& xSlot = s_axEntitySlots.Get(xEntityID.m_uIndex);
		if (!xSlot.m_bPendingStart) continue;

		// Validate entity still owns this slot before clearing the flag.
		// If the slot was freed and reused by a new entity, the new entity's
		// m_bPendingStart flag must not be cleared by the stale snapshot entry.
		if (!xSlot.m_bOccupied || xSlot.m_uGeneration != xEntityID.m_uGeneration)
			continue;

		if (!IsEntityStarted(xEntityID))
		{
			// Skip entities marked for deferred destruction (Unity parity:
			// Destroy() in Awake prevents Start from ever firing)
			if (IsMarkedForDestruction(xEntityID))
			{
				xSlot.m_bPendingStart = false;
				Zenith_Assert(m_uPendingStartCount > 0, "PendingStartCount underflow in DispatchPendingStarts (destroyed entity)");
				m_uPendingStartCount--;
				continue;
			}

			Zenith_Entity xEntity = GetEntity(xEntityID);
			if (xEntity.IsActiveInHierarchy())
			{
				xRegistry.DispatchOnStart(xEntity);
				MarkEntityStarted(xEntityID);

				// If entity was moved to another scene during Start,
				// MoveEntityInternal already transferred the pending start count.
				// Don't double-decrement. Leave m_bPendingStart true so the target
				// scene clears it via the "already started" path.
				if (xSlot.m_iSceneHandle != m_iHandle)
				{
					continue;
				}

				// Only clear the pending flag when Start actually fires.
				// If the entity is inactive, leave it pending so Start() is
				// dispatched when it (or its parent) is first enabled (Unity parity).
				xSlot.m_bPendingStart = false;
				Zenith_Assert(m_uPendingStartCount > 0, "PendingStartCount underflow in DispatchPendingStarts");
				m_uPendingStartCount--;
			}
			else
			{
				// Entity is inactive - re-add to pending list for next frame
				m_axPendingStartEntities.PushBack(xEntityID);
			}
		}
		else
		{
			// Entity was already started (e.g., by SetEnabled path) - clear the stale pending flag
			xSlot.m_bPendingStart = false;
			Zenith_Assert(m_uPendingStartCount > 0, "PendingStartCount underflow in DispatchPendingStarts");
			m_uPendingStartCount--;
		}
	}
}

//==========================================================================
// Serialization
//==========================================================================

void Zenith_SceneData::SaveToFile(const std::string& strFilename, bool bIncludeTransient)
{
	Zenith_DataStream xStream;

	static constexpr u_int SCENE_MAGIC = 0x5A53434E;
	static constexpr u_int SCENE_VERSION = 5;
	xStream << SCENE_MAGIC;
	xStream << SCENE_VERSION;

	u_int uNumEntities = 0;
	for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xID = m_xActiveEntities.Get(u);
		const Zenith_EntitySlot& xSlot = s_axEntitySlots.Get(xID.m_uIndex);
		if (bIncludeTransient || !xSlot.m_bTransient)
		{
			uNumEntities++;
		}
	}
	xStream << uNumEntities;

	for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xID = m_xActiveEntities.Get(u);
		const Zenith_EntitySlot& xSlot = s_axEntitySlots.Get(xID.m_uIndex);
		if (!bIncludeTransient && xSlot.m_bTransient)
		{
			continue;
		}
		Zenith_Entity xEntity(this, xID);
		xEntity.WriteToDataStream(xStream);
	}

	// Only write valid camera index if the camera entity was actually included in the file
	// (transient entities may be excluded, which would leave a dangling file index)
	uint32_t uMainCameraIndex = Zenith_EntityID::INVALID_INDEX;
	if (m_xMainCameraEntity.IsValid())
	{
		const Zenith_EntitySlot& xCameraSlot = s_axEntitySlots.Get(m_xMainCameraEntity.m_uIndex);
		if (bIncludeTransient || !xCameraSlot.m_bTransient)
		{
			uMainCameraIndex = m_xMainCameraEntity.m_uIndex;
		}
	}
	xStream << uMainCameraIndex;

	xStream.WriteToFile(strFilename.c_str());

	ClearDirty();
}

bool Zenith_SceneData::LoadFromFile(const std::string& strFilename)
{
	// Note: Flux render systems and Physics reset are now handled by
	// Zenith_SceneManager::LoadScene() for SINGLE mode loads only.
	// This allows LoadFromFile to be used for ADDITIVE loads without
	// destroying render data from other loaded scenes.
	Reset();

	Zenith_DataStream xStream;
	xStream.ReadFromFile(strFilename.c_str());

	if (!xStream.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "LoadFromFile: Failed to read file '%s'", strFilename.c_str());
		return false;
	}

	if (!LoadFromDataStream(xStream))
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "LoadFromFile: Failed to parse scene file '%s'", strFilename.c_str());
		return false;
	}

	// Note: Lifecycle dispatch (OnAwake/OnEnable/OnStart) is handled by
	// Zenith_SceneManager::LoadScene() after this method returns.
	// Do NOT dispatch here to avoid double-dispatch.

	// Only set path if not already set by caller (LoadScene sets canonical path before calling this)
	if (m_strPath.empty())
	{
		m_strPath = strFilename;
	}
	m_bIsLoaded = true;
	ClearDirty();
	return true;
}

bool Zenith_SceneData::LoadFromDataStream(Zenith_DataStream& xStream)
{
	// Validate stream has minimum header data (magic + version = 8 bytes)
	static constexpr uint64_t ulMIN_HEADER_SIZE = sizeof(u_int) * 2;
	if (xStream.GetSize() < ulMIN_HEADER_SIZE)
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "Malformed scene file: too small (size=%llu, minimum=%llu)",
			xStream.GetSize(), ulMIN_HEADER_SIZE);
		return false;
	}

	u_int uMagicNumber;
	u_int uVersion;
	xStream >> uMagicNumber;
	xStream >> uVersion;

	static constexpr u_int SCENE_MAGIC = 0x5A53434E;
	static constexpr u_int SCENE_VERSION_CURRENT = 5;
	static constexpr u_int SCENE_VERSION_MIN_SUPPORTED = 3;

	if (uMagicNumber != SCENE_MAGIC)
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "Invalid scene file format: bad magic number 0x%08X (expected 0x%08X)",
			uMagicNumber, SCENE_MAGIC);
		return false;
	}

	if (uVersion > SCENE_VERSION_CURRENT || uVersion < SCENE_VERSION_MIN_SUPPORTED)
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "Unsupported scene file version %u", uVersion);
		return false;
	}

	u_int uNumEntities;
	xStream >> uNumEntities;

	std::unordered_map<uint32_t, Zenith_EntityID> xFileIndexToNewID; // #TODO: Replace with engine hash map

	for (u_int u = 0; u < uNumEntities; u++)
	{
		uint32_t uFileIndex;
		std::string strName;
		uint32_t uFileParentIndex = Zenith_EntityID::INVALID_INDEX;

		if (uVersion == 3)
		{
			xStream >> uFileIndex;
			xStream >> uFileParentIndex;
			xStream >> strName;

			uint32_t uChildCount = 0;
			xStream >> uChildCount;
			for (uint32_t i = 0; i < uChildCount; ++i)
			{
				uint32_t uChildIndex;
				xStream >> uChildIndex;
			}
		}
		else // v4 and v5 share the same entity format (no child list, parent resolved via transform hierarchy)
		{
			xStream >> uFileIndex;
			xStream >> strName;
		}

		Zenith_EntityID xNewID = CreateEntity();
		xFileIndexToNewID[uFileIndex] = xNewID;

		Zenith_EntitySlot& xSlot = s_axEntitySlots.Get(xNewID.m_uIndex);
		xSlot.m_strName = strName;
		xSlot.m_bEnabled = true;
		xSlot.m_bTransient = false;

		Zenith_Entity xEntity(this, xNewID);
		xEntity.AddComponent<Zenith_TransformComponent>();

		Zenith_ComponentMetaRegistry::Get().DeserializeEntityComponents(xEntity, xStream);

		if (uVersion == 3 && uFileParentIndex != Zenith_EntityID::INVALID_INDEX)
		{
			Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
			xTransform.SetPendingParentFileIndex(uFileParentIndex);
		}
	}

	// Rebuild hierarchy
	for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xID = m_xActiveEntities.Get(u);
		Zenith_Entity xEntity = GetEntity(xID);
		Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();

		uint32_t uParentFileIndex = xTransform.GetPendingParentFileIndex();
		xTransform.ClearPendingParentFileIndex();

		if (uParentFileIndex != Zenith_EntityID::INVALID_INDEX)
		{
			auto it = xFileIndexToNewID.find(uParentFileIndex);
			if (it != xFileIndexToNewID.end() && EntityExists(it->second))
			{
				xTransform.SetParentByID(it->second);
			}
		}
	}

	// Read main camera
	uint32_t uMainCameraFileIndex;
	xStream >> uMainCameraFileIndex;

	if (uMainCameraFileIndex != Zenith_EntityID::INVALID_INDEX)
	{
		auto it = xFileIndexToNewID.find(uMainCameraFileIndex);
		if (it != xFileIndexToNewID.end() && EntityExists(it->second))
		{
			m_xMainCameraEntity = it->second;
		}
	}

	m_bIsLoaded = true;
	ClearDirty();
	return true;
}
