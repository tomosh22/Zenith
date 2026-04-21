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
#include "FileAccess/Zenith_FileAccess.h"
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
	// Iterative tree walk using explicit stack (avoids stack overflow on deep hierarchies)
	Zenith_Vector<Zenith_EntityID> axStack;
	axStack.PushBack(xID);

	while (axStack.GetSize() > 0)
	{
		Zenith_EntityID xCurrentID = axStack.GetBack();
		axStack.PopBack();

		if (xCurrentID.m_uIndex >= s_axEntitySlots.GetSize()) continue;
		Zenith_EntitySlot& xSlot = s_axEntitySlots.Get(xCurrentID.m_uIndex);
		if (!xSlot.IsOccupied() || xSlot.m_uGeneration != xCurrentID.m_uGeneration) continue;

		xSlot.m_bActiveInHierarchyDirty = true;

		// Queue children for invalidation
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneDataByHandle(xSlot.m_iSceneHandle);
		if (!pxSceneData) continue;
		if (!pxSceneData->EntityHasComponent<Zenith_TransformComponent>(xCurrentID)) continue;

		const Zenith_TransformComponent& xTransform = pxSceneData->GetComponentFromEntity<Zenith_TransformComponent>(xCurrentID);
		const Zenith_Vector<Zenith_EntityID>& axChildIDs = xTransform.GetChildEntityIDs();
		for (u_int i = 0; i < axChildIDs.GetSize(); ++i)
		{
			axStack.PushBack(axChildIDs.Get(i));
		}
	}
}

Zenith_SceneData::Zenith_SceneData()
{
	// Scene is ready for use immediately after construction
	m_bIsLoaded = true;
}

Zenith_SceneData::~Zenith_SceneData()
{
	// Destructor uses ResetAll so any lingering metadata is scrubbed — matters in
	// tests that inspect scene state after shutdown, and defensively guards against
	// a later refactor that recycles SceneData memory.
	ResetAll();
}

void Zenith_SceneData::DisableEntity(Zenith_EntityID xID)
{
	if (!EntityExists(xID)) return;
	Zenith_Entity xEntity(this, xID);
	Zenith_EntitySlot& xSlot = s_axEntitySlots.Get(xID.m_uIndex);
	if (xEntity.IsEnabled() && xSlot.m_bOnEnableDispatched)
	{
		Zenith_ComponentMetaRegistry::Get().DispatchOnDisable(xEntity);
		xSlot.m_bOnEnableDispatched = false;
	}
}

void Zenith_SceneData::DestroyEntityComponents(Zenith_EntityID xID)
{
	if (!EntityExists(xID)) return;
	Zenith_Entity xEntity(this, xID);
	Zenith_ComponentMetaRegistry::Get().RemoveAllComponents(xEntity);
}

void Zenith_SceneData::CollectResetHierarchy(Zenith_Vector<Zenith_EntityID>& axHierarchyOut)
{
	// Collect roots directly from m_xActiveEntities rather than relying on the cached
	// list, because Reset() may run with entities whose TransformComponent has already
	// been destroyed (e.g. TestSceneDisableDestroyHelpers) — and RebuildRootEntityCache
	// asserts on IsRoot() which needs that component.
	Zenith_Vector<Zenith_EntityID> axRoots;
	for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xID = m_xActiveEntities.Get(u);
		if (!EntityExists(xID)) continue;
		// No transform → no hierarchy → picked up by the sweep below.
		if (!EntityHasComponent<Zenith_TransformComponent>(xID)) continue;
		Zenith_Entity xEntity(this, xID);
		if (xEntity.IsRoot()) axRoots.PushBack(xID);
	}

	for (u_int u = 0; u < axRoots.GetSize(); ++u)
	{
		CollectHierarchyDepthFirst(axRoots.Get(u), axHierarchyOut);
	}

	// Sweep: pick up active entities not reached by the root traversal. This covers
	// (a) entities without a TransformComponent (treated as standalone), and (b) any
	// transient mid-test state where an entity exists but is not attached to a root.
	for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xID = m_xActiveEntities.Get(u);
		if (!EntityExists(xID)) continue;
		bool bAlreadyCollected = false;
		for (u_int v = 0; v < axHierarchyOut.GetSize(); ++v)
		{
			if (axHierarchyOut.Get(v) == xID) { bAlreadyCollected = true; break; }
		}
		if (!bAlreadyCollected) axHierarchyOut.PushBack(xID);
	}
}

void Zenith_SceneData::DestroyComponentPools()
{
	for (Zenith_Vector<Zenith_ComponentPoolBase*>::Iterator xIt(m_xComponents); !xIt.Done(); xIt.Next())
	{
		Zenith_ComponentPoolBase* pxPool = xIt.GetData();
		if (pxPool) delete pxPool;
	}
	m_xComponents.Clear();
}

void Zenith_SceneData::FreeGlobalSlotsForActiveEntities()
{
	for (u_int i = 0; i < m_xActiveEntities.GetSize(); ++i)
	{
		Zenith_EntityID xID = m_xActiveEntities.Get(i);
		if (xID.m_uIndex >= s_axEntitySlots.GetSize()) continue;
		Zenith_EntitySlot& xSlot = s_axEntitySlots.Get(xID.m_uIndex);
		if (!xSlot.IsOccupied() || xSlot.m_uGeneration != xID.m_uGeneration) continue;
		s_axEntityComponents.Get(xID.m_uIndex).clear();
		xSlot.ReleaseSlot();
		s_axFreeEntityIndices.PushBack(xID.m_uIndex);
	}
}

void Zenith_SceneData::ClearSceneStateAfterReset()
{
	m_xActiveEntities.Clear();
	m_axNewlyCreatedEntities.Clear();
	m_axPendingStartEntities.Clear();
	m_uPendingStartCount = 0;
	m_xPendingDestruction.Clear();
	m_axTimedDestructions.Clear();
	m_bIsUpdating = false;
	m_xMainCameraEntity = INVALID_ENTITY_ID;
	m_axCachedRootEntities.Clear();
	m_bRootEntitiesDirty = true;
}

void Zenith_SceneData::ResetEntitiesOnly()
{
	// C8: Entity reset destroys every entity in the scene — doing that mid-Update
	// corrupts iteration of m_xActiveEntities. Same-scene self-unload is not
	// supported; use LoadScene(SINGLE) or LoadSceneAsync to transition between
	// scenes and let the scene manager tear the old one down.
	Zenith_Assert(!m_bIsUpdating,
		"ResetEntitiesOnly() called during Update — unload your scene via LoadScene(SINGLE) or "
		"LoadSceneAsync, not from within OnUpdate/OnFixedUpdate. Same-scene self-"
		"unload is not supported. "
		"#TODO: auto-promote same-scene self-unload to async.");
	// C9: see CreateEntity for rationale — render-task-ordering invariant.
	Zenith_Assert(!Zenith_SceneManager::AreRenderTasksActive(),
		"Reset(): scene mutation while render tasks are reading — render-task invariant violated");
	m_bIsBeingDestroyed = true;

	// Unity parity: two-pass destruction in hierarchy-depth-first order. The same
	// traversal as RemoveEntity keeps single-entity destruction and whole-scene
	// teardown consistent and guarantees every child is disabled/destroyed before
	// its parent.
	Zenith_Vector<Zenith_EntityID> axHierarchy;
	CollectResetHierarchy(axHierarchy);

	// Pass 1: OnDisable for every entity while all data is still intact.
	for (u_int u = 0; u < axHierarchy.GetSize(); ++u)
	{
		DisableEntity(axHierarchy.Get(u));
	}

	// Pass 2: OnDestroy + component removal in hierarchy-depth order. Components
	// are removed in dependency-safe serialization order inside RemoveAllComponents.
	for (u_int u = 0; u < axHierarchy.GetSize(); ++u)
	{
		DestroyEntityComponents(axHierarchy.Get(u));
	}

	DestroyComponentPools();
	FreeGlobalSlotsForActiveEntities();
	ClearSceneStateAfterReset();

	m_bIsBeingDestroyed = false;
}

void Zenith_SceneData::ResetAll()
{
	// Entity teardown first — keeps the existing invariants around lifecycle dispatch,
	// render-task safety, and cache invalidation intact.
	ResetEntitiesOnly();

	// Then scrub scene-level metadata so the SceneData looks brand-new. Matters
	// specifically when a non-SceneManager caller (or a test) reuses the same
	// SceneData* across loads; without this, name/path/buildIndex would leak from
	// the previous scene. LoadFromFile deliberately calls ResetEntitiesOnly (not
	// this) because it will overwrite these fields itself.
	m_strName.clear();
	m_strPath.clear();
	m_iBuildIndex = -1;
	// Leave m_iHandle / m_uGeneration alone: the SceneManager owns those and a
	// reset is not a handle-release event. FreeSceneHandle is the authoritative
	// handle-invalidation path.
	m_bIsLoaded = false;
	m_bIsActivated = false;
	m_bWasLoadedAdditively = false;
	m_bIsPaused = false;
	m_bIsUnloading = false;
	m_ulLoadTimestamp = 0;
#ifdef ZENITH_TOOLS
	m_bHasUnsavedChanges = false;
#endif
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
	// C9: render-task-ordering invariant — several read APIs (EntityExists,
	// GetComponentFromEntity, GetAllOfComponentType) widen their main-thread-only
	// assertion to "main thread OR AreRenderTasksActive()". That relaxation is
	// sound only if the main thread never mutates scene storage while render
	// tasks are in flight. Enforce that invariant here.
	Zenith_Assert(!Zenith_SceneManager::AreRenderTasksActive(),
		"CreateEntity: scene mutation while render tasks are reading — render-task invariant violated");
	u_int uIndex = 0;
	u_int uGeneration = 0;

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
		xSlot.OccupySlot(m_iHandle);
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
		xNewSlot.OccupySlot(m_iHandle);
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
	// Hierarchy is encoded on the TransformComponent. If the component has been
	// explicitly destroyed by a caller (Reset() tolerates this — see its hierarchy
	// traversal) we have no children to recurse into; still emit this entity so
	// its slot gets freed.
	if (EntityHasComponent<Zenith_TransformComponent>(xID))
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
	}
	axOut.PushBack(xID);
}

void Zenith_SceneData::RemoveEntity(Zenith_EntityID xID)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "RemoveEntity must be called from main thread");
	// C9: see CreateEntity for rationale — render-task-ordering invariant.
	Zenith_Assert(!Zenith_SceneManager::AreRenderTasksActive(),
		"RemoveEntity: scene mutation while render tasks are reading — render-task invariant violated");
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
		DisableEntity(axHierarchy.Get(i));

	// Pass 2: OnDestroy + component removal + slot cleanup
	for (u_int i = 0; i < axHierarchy.GetSize(); ++i)
	{
		Zenith_EntityID xEntityID = axHierarchy.Get(i);
		if (!EntityExists(xEntityID)) continue;
		DestroyEntityComponents(xEntityID);
		Zenith_EntitySlot& xSlot = s_axEntitySlots.Get(xEntityID.m_uIndex);

		s_axEntityComponents.Get(xEntityID.m_uIndex).clear();

		// Cancel any pending start before releasing the slot
		if (xSlot.IsPendingStart())
		{
			CancelPendingStart(xEntityID);
		}
		xSlot.ReleaseSlot();
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
	s_axEntitySlots.Get(xID.m_uIndex).TransitionToDestroying();
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
			s_axEntitySlots.Get(xChildID.m_uIndex).TransitionToDestroying();
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

	// Snapshot entity IDs before iteration
	Zenith_Vector<Zenith_EntityID> xSnapshotIDs = SnapshotActiveEntities();

	// 1-2. Awake/OnEnable for new entities, then queue Start for next frame
	Zenith_Vector<Zenith_EntityID> axAllNewEntities;
	DispatchAwakeAndEnableForNewEntities(axAllNewEntities);
	QueuePendingStartsForNewEntities(axAllNewEntities);

	// 3. OnFixedUpdate - handled by SceneManager before calling Update()

	// 4-5. OnUpdate and OnLateUpdate
	DispatchOnUpdateForEntities(xSnapshotIDs, fDt);
	DispatchOnLateUpdateForEntities(xSnapshotIDs, fDt);

	// 6-7. Timed destructions and deferred destruction processing
	TickTimedDestructions(fDt);
	ProcessPendingDestructions();

	m_bIsUpdating = false;
	ClearCreatedDuringUpdateFlags();
}

void Zenith_SceneData::DispatchAwakeAndEnableForNewEntities(Zenith_Vector<Zenith_EntityID>& axAllNewEntities)
{
	Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();

	// Unity parity: Awake/OnEnable are called immediately when an entity is instantiated,
	// even during Update(). Only Update/LateUpdate are deferred to the next frame.
	// Loop until stable: entities created during Awake get Awake called immediately (Unity parity).
	constexpr u_int uMAX_AWAKE_ITERATIONS = 100;
	u_int uIteration = 0;
	while (m_axNewlyCreatedEntities.GetSize() > 0 && uIteration < uMAX_AWAKE_ITERATIONS)
	{
		// Snapshot this wave and accumulate into output (single pass instead of two copies)
		Zenith_Vector<Zenith_EntityID> axNewEntities;
		axNewEntities.Reserve(m_axNewlyCreatedEntities.GetSize());
		for (u_int u = 0; u < m_axNewlyCreatedEntities.GetSize(); ++u)
		{
			Zenith_EntityID xID = m_axNewlyCreatedEntities.Get(u);
			axNewEntities.PushBack(xID);
			axAllNewEntities.PushBack(xID);
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
	if (uIteration >= uMAX_AWAKE_ITERATIONS && m_axNewlyCreatedEntities.GetSize() > 0)
	{
		// Runtime-path divergence from scene-load: we do NOT destroy the leftover
		// entities the way DispatchAwakeForNewScene does. Leftover entities stay
		// in m_axNewlyCreatedEntities and will receive OnAwake on the next Update()
		// cycle — the scene is already active so there is no invariant that every
		// entity be awake by a specific tick. That said, reaching this branch means
		// an OnAwake handler is creating more entities than can drain in one frame,
		// which will stall OnStart for those entities indefinitely if sustained.
		Zenith_Assert(false,
			"DispatchAwakeAndEnableForNewEntities: Awake iteration limit (%u) reached; "
			"%u entities remain unawakened this frame. An OnAwake handler is chain-creating "
			"entities faster than one frame can drain.",
			uMAX_AWAKE_ITERATIONS, m_axNewlyCreatedEntities.GetSize());
		Zenith_Error(LOG_CATEGORY_SCENE,
			"DispatchAwakeAndEnableForNewEntities: %u entities deferred to next frame "
			"after Awake wave limit (%u). OnStart for these entities will be delayed.",
			m_axNewlyCreatedEntities.GetSize(), uMAX_AWAKE_ITERATIONS);
	}
}

void Zenith_SceneData::QueuePendingStartsForNewEntities(const Zenith_Vector<Zenith_EntityID>& axAllNewEntities)
{
	// Unity defers Start() to the frame after Awake/OnEnable, not the same frame.
	// DispatchPendingStarts is handled by SceneManager::Update() before calling
	// SceneData::Update(), ensuring Start() runs before the first Update().
	for (u_int u = 0; u < axAllNewEntities.GetSize(); ++u)
	{
		Zenith_EntityID uID = axAllNewEntities.Get(u);
		if (!EntityExists(uID)) continue;

		if (!IsEntityStarted(uID))
		{
			MarkEntityPendingStart(uID);
		}
	}
}

void Zenith_SceneData::DispatchOnUpdateForEntities(const Zenith_Vector<Zenith_EntityID>& xSnapshotIDs, float fDt)
{
	Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();

	for (u_int u = 0; u < xSnapshotIDs.GetSize(); ++u)
	{
		Zenith_EntityID uID = xSnapshotIDs.Get(u);
		if (!EntityExists(uID)) continue;
		if (WasCreatedDuringUpdate(uID)) continue;

		Zenith_Entity xEntity = GetEntity(uID);
		if (!xEntity.IsActiveInHierarchy()) continue;
		xRegistry.DispatchOnUpdate(xEntity, fDt);
	}
}

void Zenith_SceneData::DispatchOnLateUpdateForEntities(const Zenith_Vector<Zenith_EntityID>& xSnapshotIDs, float fDt)
{
	Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();

	for (u_int u = 0; u < xSnapshotIDs.GetSize(); ++u)
	{
		Zenith_EntityID uID = xSnapshotIDs.Get(u);
		if (!EntityExists(uID)) continue;
		if (WasCreatedDuringUpdate(uID)) continue;

		Zenith_Entity xEntity = GetEntity(uID);
		if (!xEntity.IsActiveInHierarchy()) continue;
		xRegistry.DispatchOnLateUpdate(xEntity, fDt);
	}
}

void Zenith_SceneData::TickTimedDestructions(float fDt)
{
	// C3 (F11): this uses the raw fDt the caller passes in. Zenith does not have a
	// global Time.timeScale equivalent today, so the "scaled Time.time" claim in
	// the prior comment was aspirational. If a timescale system is introduced,
	// update the caller (SceneManager::Update) to multiply dt by timescale before
	// forwarding — do NOT multiply here, so OnUpdate and this decay stay aligned.
	for (int i = static_cast<int>(m_axTimedDestructions.GetSize()) - 1; i >= 0; --i)
	{
		TimedDestruction& xEntry = m_axTimedDestructions.Get(i);

		// Clean up entries for entities that were already destroyed
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
}

void Zenith_SceneData::ClearCreatedDuringUpdateFlags()
{
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
	Zenith_Vector<Zenith_EntityID> xEntityIDs = SnapshotActiveEntities();

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
				// Release-build safety net: the Zenith_Assert above compiles away in
				// release builds, so without this explicit Zenith_Error a runaway scene
				// would silently truncate its Awake dispatch. The break is already
				// outside the assert so the loop still terminates; this just makes the
				// failure observable in shipped builds.
				Zenith_Error(LOG_CATEGORY_SCENE,
					"DispatchAwakeForNewScene: Awake wave limit (%u) reached for scene '%s'; "
					"destroying %u unawakened entities. An OnAwake handler is creating "
					"entities without bound.",
					uMAX_AWAKE_ITERATIONS, m_strPath.c_str(), uWaveEnd - uWaveStart);

				// Phase A3 fix: destroy the unawakened entities so the scene-lifecycle
				// invariant holds (every surviving entity in DispatchEnableAndPendingStartsForNewScene
				// must have received OnAwake). Leaving them in m_xActiveEntities would
				// silently fire OnEnable/OnStart on entities that never got OnAwake.
				//
				// Snapshot IDs first because RemoveEntity erases from m_xActiveEntities.
				// A scene that hits this path is malformed, so we're also not worried
				// about recursive OnDestroy creating more entities — they'd just be
				// swept by the same cleanup on the next scene load.
				//
				// Audit §3.6 note (Unity-parity divergence, deliberate): Unity's
				// MonoBehaviour.OnDestroy only fires on objects that became active
				// (i.e. had Awake). Zenith fires OnDestroy here on entities whose
				// Awake never ran. That's a deliberate divergence: this branch only
				// triggers on pathological content (100+ cascading Awake-creates-Awake
				// waves), and routing overflow through RemoveEntity gives component
				// destructors — especially those holding OS resources — a chance to
				// clean up instead of silently leaking. The alternative (skip OnDestroy
				// for strict Unity parity) would trade a predictable cleanup path for
				// an invisible resource leak in an already-malformed scene.
				// Refs: https://docs.unity3d.com/ScriptReference/MonoBehaviour.Awake.html
				//       https://docs.unity3d.com/ScriptReference/MonoBehaviour.OnDestroy.html
				Zenith_Vector<Zenith_EntityID> axUnawokenIDs;
				for (u_int u = uWaveStart; u < uWaveEnd; ++u)
				{
					axUnawokenIDs.PushBack(m_xActiveEntities.Get(u));
				}
				for (u_int i = 0; i < axUnawokenIDs.GetSize(); ++i)
				{
					Zenith_EntityID xID = axUnawokenIDs.Get(i);
					if (EntityExists(xID))
					{
						RemoveEntity(xID);
					}
				}
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

	for (u_int u = 0; u < axSnapshot.GetSize(); ++u)
	{
		PendingStartResult eResult = ProcessSinglePendingStart(axSnapshot.Get(u), xRegistry);
		if (eResult == PendingStartResult::REQUEUE)
		{
			m_axPendingStartEntities.PushBack(axSnapshot.Get(u));
		}
	}
}

Zenith_SceneData::PendingStartResult Zenith_SceneData::ProcessSinglePendingStart(Zenith_EntityID xEntityID, Zenith_ComponentMetaRegistry& xRegistry)
{
	if (xEntityID.m_uIndex >= s_axEntitySlots.GetSize()) return PendingStartResult::SKIP;
	Zenith_EntitySlot& xSlot = s_axEntitySlots.Get(xEntityID.m_uIndex);
	if (!xSlot.IsPendingStart()) return PendingStartResult::SKIP;

	// Validate entity still owns this slot.
	// If the slot was freed and reused by a new entity, the new entity's
	// lifecycle state must not be modified by the stale snapshot entry.
	if (xSlot.m_uGeneration != xEntityID.m_uGeneration)
		return PendingStartResult::SKIP;

	// Skip entities marked for deferred destruction (Unity parity:
	// Destroy() in Awake prevents Start from ever firing)
	if (IsMarkedForDestruction(xEntityID))
	{
		// Revert lifecycle (ReleaseSlot will reset to FREE during destruction)
		xSlot.RevertFromPendingStart();
		Zenith_Assert(m_uPendingStartCount > 0, "PendingStartCount underflow in DispatchPendingStarts (destroyed entity)");
		m_uPendingStartCount--;
		return PendingStartResult::CLEARED;
	}

	Zenith_Entity xEntity = GetEntity(xEntityID);
	if (!xEntity.IsActiveInHierarchy())
	{
		return PendingStartResult::REQUEUE;
	}

	xRegistry.DispatchOnStart(xEntity);

	// If entity was moved to another scene during Start,
	// MoveEntityInternal already decremented our count and transferred
	// the pending start to the target. Clean up the target's bookkeeping
	// since Start was already dispatched by this (source) scene.
	if (xSlot.m_iSceneHandle != m_iHandle)
	{
		Zenith_SceneData* pxTargetData = Zenith_SceneManager::GetSceneDataByHandle(xSlot.m_iSceneHandle);
		if (pxTargetData)
		{
			// Remove from target's pending list and decrement count
			// (don't use CancelPendingStart - lifecycle is still PENDING_START and
			// we want MarkEntityStarted to handle the transition, not revert to AWOKEN)
			Zenith_Assert(pxTargetData->m_uPendingStartCount > 0, "PendingStartCount underflow in target scene after Start-during-move");
			pxTargetData->m_uPendingStartCount--;
			pxTargetData->m_axPendingStartEntities.EraseValue(xEntityID);
		}
		MarkEntityStarted(xEntityID);
		return PendingStartResult::CLEARED;
	}

	MarkEntityStarted(xEntityID);
	Zenith_Assert(m_uPendingStartCount > 0, "PendingStartCount underflow in DispatchPendingStarts");
	m_uPendingStartCount--;
	return PendingStartResult::CLEARED;
}

//==========================================================================
// Serialization
//==========================================================================

void Zenith_SceneData::SaveToFile(const std::string& strFilename, bool bIncludeTransient)
{
	Zenith_DataStream xStream;

	xStream << uSCENE_MAGIC;
	xStream << uSCENE_VERSION_CURRENT;

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

bool Zenith_SceneData::ValidateFileHeader(const std::string& strFilename)
{
	if (!Zenith_FileAccess::FileExists(strFilename.c_str()))
	{
		return false;
	}

	Zenith_DataStream xStream;
	xStream.ReadFromFile(strFilename.c_str());

	if (!xStream.IsValid())
	{
		return false;
	}

	static constexpr uint64_t ulMIN_HEADER_SIZE = sizeof(u_int) * 2;
	if (xStream.GetSize() < ulMIN_HEADER_SIZE)
	{
		return false;
	}

	u_int uMagicNumber;
	u_int uVersion;
	xStream >> uMagicNumber;
	xStream >> uVersion;

	if (uMagicNumber != uSCENE_MAGIC)
	{
		return false;
	}
	if (uVersion > uSCENE_VERSION_CURRENT || uVersion < uSCENE_VERSION_MIN_SUPPORTED)
	{
		return false;
	}
	return true;
}

bool Zenith_SceneData::LoadFromFile(const std::string& strFilename)
{
	// Note: Flux render systems and Physics reset are now handled by
	// Zenith_SceneManager::LoadScene() for SINGLE mode loads only.
	// This allows LoadFromFile to be used for ADDITIVE loads without
	// destroying render data from other loaded scenes.
	//
	// B.8: gatekeep LoadFromFile so external callers can't silently nuke a loaded
	// scene's entities. SceneManager always drives this against a fresh SceneData
	// from CreateEmptyScene (entity count = 0). Any non-SceneManager caller that
	// reaches this with live entities is almost certainly bypassing the lifecycle
	// (no SceneUnloading/SceneUnloaded callbacks, no active-scene handling, no
	// physics reset) and should route via Zenith_SceneManager::LoadScene instead.
	Zenith_Assert(m_xActiveEntities.GetSize() == 0,
		"Zenith_SceneData::LoadFromFile: scene is not empty (entityCount=%u). "
		"Call Zenith_SceneManager::LoadScene / LoadSceneAsync to route through the "
		"full lifecycle, or call ResetEntitiesOnly() first if you intentionally want "
		"to skip the callbacks.", m_xActiveEntities.GetSize());
	// B.7: use ResetEntitiesOnly, not ResetAll. We're about to overwrite name/path/
	// buildIndex via deserialization + the caller's assignments — wiping them first
	// would race with SceneManager::LoadScene which sets m_strPath before calling
	// this (see LoadScene line ~846). B.8 asserts the caller already cleared the
	// entity table, so this call is effectively "defensive cleanup of partial state".
	ResetEntitiesOnly();

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

Zenith_EntityID Zenith_SceneData::ReadEntityFromDataStream(Zenith_DataStream& xStream, u_int uVersion,
	std::unordered_map<uint32_t, Zenith_EntityID>& xFileIndexToNewID) // #TODO: Replace with engine hash map
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
	else // v4 and v5 share the same entity format
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

	return xNewID;
}

bool Zenith_SceneData::LoadFromDataStream(Zenith_DataStream& xStream)
{
	// C9: see CreateEntity for rationale — render-task-ordering invariant.
	// LoadFromDataStream creates entities + components en masse; if render
	// tasks are actively reading, those reads would see half-populated scenes.
	Zenith_Assert(!Zenith_SceneManager::AreRenderTasksActive(),
		"LoadFromDataStream: scene mutation while render tasks are reading — render-task invariant violated");
	// B.8: same gate as LoadFromFile. Deserializing over a populated scene would
	// blindly append new entities without any OnDestroy/SceneUnloading/etc. fires
	// for the old ones. SceneManager guarantees this by always calling this on
	// freshly-created scene data; tests intentionally start from CreateEmptyScene.
	Zenith_Assert(m_xActiveEntities.GetSize() == 0,
		"Zenith_SceneData::LoadFromDataStream: scene is not empty (entityCount=%u). "
		"Load against a fresh scene (CreateEmptyScene) or call ResetEntitiesOnly() first.",
		m_xActiveEntities.GetSize());

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

	if (uMagicNumber != uSCENE_MAGIC)
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "Invalid scene file format: bad magic number 0x%08X (expected 0x%08X)",
			uMagicNumber, uSCENE_MAGIC);
		return false;
	}

	if (uVersion > uSCENE_VERSION_CURRENT || uVersion < uSCENE_VERSION_MIN_SUPPORTED)
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "Unsupported scene file version %u", uVersion);
		return false;
	}

	u_int uNumEntities;
	xStream >> uNumEntities;

	std::unordered_map<uint32_t, Zenith_EntityID> xFileIndexToNewID; // #TODO: Replace with engine hash map

	for (u_int u = 0; u < uNumEntities; u++)
		ReadEntityFromDataStream(xStream, uVersion, xFileIndexToNewID);

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
