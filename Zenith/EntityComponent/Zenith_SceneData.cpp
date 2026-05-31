#include "Zenith.h"

#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "EntityComponent/Zenith_AccessSet.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
// WS12 parallel-sim chokepoint: the collider guard (the hidden Tween->Jolt
// physics-write edge) needs the concrete ColliderComponent type for
// HasComponent<>, and ParallelDispatchHook fans the eligible wave through the
// task system. Both pull in only when the WS12 gate is ON at runtime.
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#ifdef ZENITH_TOOLS
#include "Editor/Zenith_Editor.h"
#endif

// Phase 5a: global entity storage moved off Zenith_SceneData onto
// Zenith_EntityStore (held by Zenith_Engine). ResetGlobalEntityStorage was
// replaced by Zenith_EntityStore::Reset(); call sites updated.

#ifdef ZENITH_TOOLS
// Defined out-of-line so the inline private MarkEntityStarted doesn't have
// to be reordered; the public Editor_-prefixed entry point simply forwards.
// Used by g_xEngine.Editor().EnterStopMode (namespace, no longer a friend).
void Zenith_SceneData::Editor_MarkEntityStarted(Zenith_EntityID xID)
{
	MarkEntityStarted(xID);
}
#endif

void Zenith_SceneData::InvalidateActiveInHierarchyCache(Zenith_EntityID xID)
{
	// Iterative tree walk using explicit stack (avoids stack overflow on deep hierarchies)
	Zenith_Vector<Zenith_EntityID> axStack;
	axStack.PushBack(xID);

	while (axStack.GetSize() > 0)
	{
		Zenith_EntityID xCurrentID = axStack.GetBack();
		axStack.PopBack();

		if (xCurrentID.m_uIndex >= g_xEngine.EntityStore().m_axEntitySlots.GetSize()) continue;
		Zenith_EntitySlot& xSlot = g_xEngine.EntityStore().m_axEntitySlots.Get(xCurrentID.m_uIndex);
		if (!xSlot.IsOccupied() || xSlot.m_uGeneration != xCurrentID.m_uGeneration) continue;

		xSlot.m_bActiveInHierarchyDirty = true;

		// Queue children for invalidation
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneDataByHandle(xSlot.m_iSceneHandle);
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
	// Scene is ready for use immediately after construction.
	m_eLoadState = SCENE_STATE_LOADED;
}

void Zenith_SceneData::TransitionTo(Zenith_SceneData::LoadState eNew)
{
	// Legal edges (everything else asserts):
	//   DESTROYED → LOADING   (re-init from a scrubbed slot)
	//   LOADING   → LOADED    (lifecycle dispatch complete)
	//   LOADED    → LOADING   (a re-load of an already-active scene)
	//   any state → DESTROYED (ScrubAndReset)
	const bool bLegal =
		(eNew == SCENE_STATE_DESTROYED) ||
		(m_eLoadState == SCENE_STATE_DESTROYED && eNew == SCENE_STATE_LOADING) ||
		(m_eLoadState == SCENE_STATE_LOADING   && eNew == SCENE_STATE_LOADED)   ||
		(m_eLoadState == SCENE_STATE_LOADED    && eNew == SCENE_STATE_LOADING)  ||
		(m_eLoadState == eNew);
	Zenith_Assert(bLegal,
		"Zenith_SceneData::TransitionTo: illegal transition from %d to %d (scene '%s' handle=%d)",
		static_cast<int>(m_eLoadState), static_cast<int>(eNew),
		m_strName.c_str(), m_iHandle);
	m_eLoadState = eNew;
}

Zenith_SceneData::~Zenith_SceneData()
{
	// Tear down entities only — the metadata fields go away with the object
	// when it's freed, so there's nothing to scrub. ScrubAndReset() would also
	// assert we are NOT the persistent scene, but the persistent scene's
	// destructor IS a legitimate shutdown path (SceneRegistry::Shutdown deletes
	// every slot).
	Reset();
}

void Zenith_SceneData::DisableEntity(Zenith_EntityID xID)
{
	if (!EntityExists(xID)) return;
	Zenith_Entity xEntity(this, xID);
	Zenith_EntitySlot& xSlot = g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex);
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
		if (xID.m_uIndex >= g_xEngine.EntityStore().m_axEntitySlots.GetSize()) continue;
		Zenith_EntitySlot& xSlot = g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex);
		if (!xSlot.IsOccupied() || xSlot.m_uGeneration != xID.m_uGeneration) continue;
		g_xEngine.EntityStore().m_axEntityComponents.Get(xID.m_uIndex).clear();
		xSlot.ReleaseSlot();
		g_xEngine.EntityStore().m_axFreeEntityIndices.PushBack(xID.m_uIndex);
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

void Zenith_SceneData::Reset()
{
	// C8: Entity reset destroys every entity in the scene — doing that mid-Update
	// corrupts iteration of m_xActiveEntities. Same-scene self-unload is not
	// supported; use LoadScene(SINGLE) or LoadSceneAsync to transition between
	// scenes and let the scene manager tear the old one down.
	Zenith_Assert(!m_bIsUpdating,
		"Reset() called during Update — unload your scene via LoadScene(SINGLE) or "
		"LoadSceneAsync, not from within OnUpdate/OnFixedUpdate. Same-scene self-"
		"unload is not supported. "
		"#TODO: auto-promote same-scene self-unload to async.");
	// C9: see CreateEntity for rationale — render-task-ordering invariant.
	Zenith_Assert(!g_xEngine.Scenes().AreRenderTasksActive(),
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

void Zenith_SceneData::ScrubAndReset()
{
	// Refuse to scrub the persistent scene's metadata. ScrubAndReset() clears
	// m_bIsLoaded + name + path + build index — leaving the persistent scene
	// reachable via m_axScenes but flagged unloaded, which crashes the next
	// MarkEntityPersistent caller in GetComponent. The destructor is the only
	// legitimate caller for the persistent scene (engine shutdown), and the
	// destructor never runs while the persistent scene is alive in m_axScenes —
	// FreeSceneHandle nulls the slot first.
	Zenith_Assert(m_iHandle != g_xEngine.Scenes().GetPersistentSceneHandle(),
		"Zenith_SceneData::ScrubAndReset: refusing to wipe persistent scene metadata. "
		"Use Reset() instead — ScrubAndReset() transitions to DESTROYED and clears "
		"name/path/buildIndex, leaving the persistent scene reachable but flagged "
		"unloaded. The next MarkEntityPersistent caller would land an entity in a "
		"half-dead scene and the following GetComponent would assert.");

	// Entity teardown first — keeps the existing invariants around lifecycle dispatch,
	// render-task safety, and cache invalidation intact.
	Reset();

	// Then scrub scene-level metadata so the SceneData looks brand-new. Matters
	// specifically when a non-scene-system caller (or a test) reuses the same
	// SceneData* across loads; without this, name/path/buildIndex would leak from
	// the previous scene. LoadFromFile deliberately calls Reset (the safe variant)
	// because it will overwrite these fields itself.
	// Leave m_iHandle / m_uGeneration alone: the SceneRegistry owns those and a
	// reset is not a handle-release event. FreeSceneHandle is the authoritative
	// handle-invalidation path.
	m_strName.clear();
	m_strPath.clear();
	m_iBuildIndex = -1;
	TransitionTo(SCENE_STATE_DESTROYED);
	m_bWasLoadedAdditively = false;
	m_bIsPaused = false;
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
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "GetCachedRootEntityCount must be called from main thread");
	if (m_bRootEntitiesDirty)
	{
		RebuildRootEntityCache();
	}
	return m_axCachedRootEntities.GetSize();
}

void Zenith_SceneData::GetCachedRootEntities(Zenith_Vector<Zenith_EntityID>& axOut)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "GetCachedRootEntities must be called from main thread");
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
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "CreateEntity must be called from main thread");
	// C9: render-task-ordering invariant — several read APIs (EntityExists,
	// GetComponentFromEntity, GetAllOfComponentType) widen their main-thread-only
	// assertion to "main thread OR AreRenderTasksActive()". That relaxation is
	// sound only if the main thread never mutates scene storage while render
	// tasks are in flight. Enforce that invariant here.
	Zenith_Assert(!g_xEngine.Scenes().AreRenderTasksActive(),
		"CreateEntity: scene mutation while render tasks are reading — render-task invariant violated");
	u_int uIndex = 0;
	u_int uGeneration = 0;

	// Try to reuse a free slot. Skip any slots with generation overflow (retired permanently).
	bool bFoundFreeSlot = false;
	while (g_xEngine.EntityStore().m_axFreeEntityIndices.GetSize() > 0)
	{
		uIndex = g_xEngine.EntityStore().m_axFreeEntityIndices.GetBack();
		g_xEngine.EntityStore().m_axFreeEntityIndices.PopBack();

		Zenith_EntitySlot& xSlot = g_xEngine.EntityStore().m_axEntitySlots.Get(uIndex);

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
		uIndex = g_xEngine.EntityStore().m_axEntitySlots.GetSize();
		uGeneration = 1;

		Zenith_EntitySlot xNewSlot;
		xNewSlot.m_uGeneration = uGeneration;
		xNewSlot.OccupySlot(m_iHandle);
		g_xEngine.EntityStore().m_axEntitySlots.PushBack(std::move(xNewSlot));
	}

	while (g_xEngine.EntityStore().m_axEntityComponents.GetSize() <= uIndex)
	{
		g_xEngine.EntityStore().m_axEntityComponents.PushBack({});
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
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "RemoveEntity must be called from main thread");
	// C9: see CreateEntity for rationale — render-task-ordering invariant.
	Zenith_Assert(!g_xEngine.Scenes().AreRenderTasksActive(),
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
		Zenith_EntitySlot& xSlot = g_xEngine.EntityStore().m_axEntitySlots.Get(xEntityID.m_uIndex);

		g_xEngine.EntityStore().m_axEntityComponents.Get(xEntityID.m_uIndex).clear();

		// Cancel any pending start before releasing the slot
		if (xSlot.IsPendingStart())
		{
			CancelPendingStart(xEntityID);
		}
		xSlot.ReleaseSlot();
		g_xEngine.EntityStore().m_axFreeEntityIndices.PushBack(xEntityID.m_uIndex);

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
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "FindEntityByName must be called from main thread");
	for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xID = m_xActiveEntities.Get(u);
		if (EntityExists(xID))
		{
			const Zenith_EntitySlot& xSlot = g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex);
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
	return g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex);
}

//==========================================================================
// Camera
//==========================================================================

void Zenith_SceneData::SetMainCameraEntity(Zenith_EntityID xEntity)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "SetMainCameraEntity must be called from main thread");
	m_xMainCameraEntity = xEntity;
}

Zenith_EntityID Zenith_SceneData::GetMainCameraEntity() const
{
	// Read-only: m_xMainCameraEntity is stable during render/animation tasks
	// (main thread does not modify it while worker threads are running)
	Zenith_Assert(g_xEngine.Threading().IsMainThread() || g_xEngine.Scenes().AreRenderTasksActive(),
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
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "MarkForDestruction must be called from main thread");
	if (!xID.IsValid()) return;
	if (!EntityExists(xID)) return;

	// Already marked - prevent double-marking and infinite recursion (use global slot flag)
	if (g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex).m_bMarkedForDestruction) return;

	// Phase 8: if this entity is still PENDING_START, cancel that first. Otherwise
	// the destruction marker transitions the slot out of PENDING_START while
	// leaving its EntityID in m_axPendingStartEntities — DispatchPendingStarts
	// then iterates the vector, looks up the now-FREE/DESTROYING slot, and
	// silently skips. The bookkeeping silently diverges; cancelling here keeps
	// vector + slot state coherent.
	if (g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex).IsPendingStart())
	{
		CancelPendingStart(xID);
	}

	// Mark children's flags (so scripts can't interact with them), but only push the root
	// entity to m_xPendingDestruction. RemoveEntity handles children recursively.
	MarkChildrenForDestructionRecursive(xID);

	// Mark this entity and add to pending list (only root entities are in the pending list)
	g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex).TransitionToDestroying();
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
		if (xChildID.IsValid() && EntityExists(xChildID) && !g_xEngine.EntityStore().m_axEntitySlots.Get(xChildID.m_uIndex).m_bMarkedForDestruction)
		{
			g_xEngine.EntityStore().m_axEntitySlots.Get(xChildID.m_uIndex).TransitionToDestroying();
			MarkChildrenForDestructionRecursive(xChildID);
		}
	}
}

void Zenith_SceneData::MarkForTimedDestruction(Zenith_EntityID xID, float fDelay)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "MarkForTimedDestruction must be called from main thread");
	if (!xID.IsValid() || !EntityExists(xID)) return;

	TimedDestruction xEntry;
	xEntry.m_xEntityID = xID;
	xEntry.m_fTimeRemaining = fDelay;
	m_axTimedDestructions.PushBack(xEntry);
}

bool Zenith_SceneData::IsMarkedForDestruction(Zenith_EntityID xID) const
{
	if (!xID.IsValid()) return false;
	if (xID.m_uIndex >= g_xEngine.EntityStore().m_axEntitySlots.GetSize()) return false;
	return g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex).m_bMarkedForDestruction;
}

void Zenith_SceneData::ProcessPendingDestructions()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "ProcessPendingDestructions must be called from main thread");
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
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "Update must be called from main thread");

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

			Zenith_EntitySlot& xSlot = g_xEngine.EntityStore().m_axEntitySlots.Get(uID.m_uIndex);
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

void Zenith_SceneData::DispatchLifecycleHookForEntities(const Zenith_Vector<Zenith_EntityID>& xSnapshotIDs, float fDt, LifecycleHook eHook)
{
	// WS12 gate. DEFAULT FALSE -> the existing serial loop below runs verbatim:
	// identical iteration order, single thread, zero behaviour change. Only when
	// g_xEngine.Scenes().AreParallelSimEnabled() is true do we take the parallel
	// branch (proven byte-identical by the determinism cross-check). Reversibility
	// = flip the flag / delete this else-branch.
	if (!g_xEngine.Scenes().AreParallelSimEnabled())
	{
		Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();

		for (u_int u = 0; u < xSnapshotIDs.GetSize(); ++u)
		{
			Zenith_EntityID uID = xSnapshotIDs.Get(u);
			if (!EntityExists(uID)) continue;
			if (WasCreatedDuringUpdate(uID)) continue;

			Zenith_Entity xEntity = GetEntity(uID);
			if (!xEntity.IsActiveInHierarchy()) continue;

			switch (eHook)
			{
			case LifecycleHook::UPDATE:      xRegistry.DispatchOnUpdate(xEntity, fDt);     break;
			case LifecycleHook::LATE_UPDATE: xRegistry.DispatchOnLateUpdate(xEntity, fDt); break;
			}
		}
		return;
	}

	ParallelDispatchHook(xSnapshotIDs, fDt, eHook);
}

//-----------------------------------------------------------------------------
// WS12 parallel-sim eligibility + dispatch
//-----------------------------------------------------------------------------

bool Zenith_SceneData::IsEntityParallelEligible(Zenith_Entity& xEntity)
{
	// (c) No ScriptComponent — scripts are an open-ended surface (arbitrary user
	//     OnUpdate that may touch any state); always serial.
	if (xEntity.HasComponent<Zenith_ScriptComponent>()) return false;

	// (b) No ColliderComponent — the hidden Tween->Jolt physics-write guard.
	//     Transform::SetPosition/SetRotation mirror onto the shared Jolt
	//     BodyInterface IFF the entity has a collider, which the determinism
	//     hash (ECS state only) cannot see. Exclude collider-bearing entities.
	if (xEntity.HasComponent<Zenith_ColliderComponent>()) return false;

	// Defense-in-depth (strictly CONSERVATIVE — only ever REMOVES entities from
	// the eligible set, so it cannot affect the default-OFF byte-identity nor the
	// serial-vs-parallel equivalence). A Tween animating SCALE on an entity with
	// a ModelComponent that has a baked physics mesh would, via
	// Transform::SetScale -> ModelComponent::GeneratePhysicsMesh, write shared
	// physics state — the SAME class of cross-system race as the collider edge
	// (and equally invisible to the ECS-state hash). The mask check below does
	// NOT catch this because ModelComponent has no per-frame update hook and so
	// never contributes to the aggregate mask. Exclude it explicitly. Beyond the
	// spec's three named conditions, but demanded by the "correctness must be
	// PROVEN, never assumed" principle for this highest-risk change.
	if (xEntity.HasComponent<Zenith_ModelComponent>())
	{
		if (xEntity.GetComponent<Zenith_ModelComponent>().HasPhysicsMesh()) return false;
	}

	// (a) Aggregate per-frame-update access mask is a strict subset of
	//     {READS_TRANSFORM|WRITES_TRANSFORM}. Any UNKNOWN bit (an un-annotated
	//     update component) or PHYSICS bit forces serial. This keeps ~11/12
	//     component types serial; only the audited Tween passes.
	const u_int uMask = Zenith_AccessSet::ComputeEntityUpdateAccessMask(
		Zenith_ComponentMetaRegistry::Get(), xEntity);
	return Zenith_AccessSet::MaskIsParallelEligible(uMask);
}

// One shard of the WS12 eligible-entity parallel wave. Splits the eligible list
// into uNumInvocations CONTIGUOUS ranges (deterministic partition, independent
// of which worker picks which index) and dispatches its range. Eligible
// entities are mutually disjoint (each writes only its own Transform), so ranges
// never touch shared state and the union of all ranges == the whole eligible
// list exactly once, in order. Static member (not a free function) so it can
// reach the private WasCreatedDuringUpdate guard.
void Zenith_SceneData::ParallelSimShardFunc(void* pData, u_int uInvocationIndex, u_int uNumInvocations)
{
	ParallelSimShardContext* pxCtx = static_cast<ParallelSimShardContext*>(pData);
	const Zenith_Vector<Zenith_EntityID>& xEligible = *pxCtx->m_pxEligibleIDs;
	Zenith_SceneData* pxSceneData = pxCtx->m_pxSceneData;
	Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();

	const u_int uTotal = xEligible.GetSize();

	// Even contiguous partition with the remainder spread over the first
	// (uTotal % uNumInvocations) shards. Pure integer math on the index, so
	// every worker computes the same disjoint ranges regardless of order.
	const u_int uBase      = uTotal / uNumInvocations;
	const u_int uRemainder = uTotal % uNumInvocations;
	const u_int uBegin = uInvocationIndex * uBase + (uInvocationIndex < uRemainder ? uInvocationIndex : uRemainder);
	const u_int uCount = uBase + (uInvocationIndex < uRemainder ? 1u : 0u);
	const u_int uEnd   = uBegin + uCount;

	for (u_int u = uBegin; u < uEnd; ++u)
	{
		Zenith_EntityID uID = xEligible.Get(u);

		// Same per-entity guards as the serial loop, re-checked inside the shard
		// (the EntityExists / WasCreatedDuringUpdate guards are pure reads of
		// stable slot storage — thread-safe during task execution; see
		// EntityExists' contract comment). IsActiveInHierarchy was resolved on
		// the MAIN THREAD during the gather (it asserts main-thread and mutates
		// the per-slot cache, so it must NOT run on a worker); only entities that
		// passed it are in this list. A Tween OnUpdate cannot create/destroy
		// entities, so nothing in this wave can invalidate another eligible
		// entity mid-wave — but we keep the cheap guards anyway for serial parity.
		if (!pxSceneData->EntityExists(uID)) continue;
		if (pxSceneData->WasCreatedDuringUpdate(uID)) continue;

		Zenith_Entity xEntity = pxSceneData->GetEntity(uID);

		if (pxCtx->m_bLateUpdate)
		{
			xRegistry.DispatchOnLateUpdate(xEntity, pxCtx->m_fDt);
		}
		else
		{
			xRegistry.DispatchOnUpdate(xEntity, pxCtx->m_fDt);
		}
	}
}

void Zenith_SceneData::ParallelDispatchHook(const Zenith_Vector<Zenith_EntityID>& xSnapshotIDs, float fDt, LifecycleHook eHook)
{
	Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();

	// ORDER-PRESERVING SEGMENTED DISPATCH. We walk the snapshot in order and
	// accumulate MAXIMAL CONTIGUOUS RUNS of eligible entities into a pending
	// wave. The moment we hit an INELIGIBLE entity (or the end of the snapshot),
	// we FLUSH the pending eligible wave (parallel dispatch + barrier) and THEN
	// dispatch the ineligible entity inline. This preserves the EXACT global
	// serial order: every entity — eligible or not — is dispatched in its
	// snapshot position relative to every other, just with the disjoint eligible
	// runs fanned across workers.
	//
	// Why segment rather than one big wave: the spec's one-wave shortcut is
	// byte-identical to serial ONLY if no ineligible entity reads an eligible
	// entity's per-frame output. An ineligible entity is UNKNOWN (we must assume
	// it reads anything — e.g. a follow-camera script reading a tweened entity's
	// position). Deferring ALL eligible entities to a single post-gather wave
	// would let such an ineligible entity observe a STALE transform, diverging
	// from serial — a hazard the determinism cross-check (pure-Tween scenes)
	// cannot see, exactly the "assumed, not proven" trap the spec warns against.
	// Segmenting makes the equivalence hold for ANY scene, not just the test
	// scenes. Eligible entities never read other entities, so order WITHIN a run
	// is irrelevant; the barrier before each ineligible dispatch makes all prior
	// eligible writes visible. In the common cases this is still cheap: an
	// all-eligible scene (the bench/soak) is exactly one wave; an all-ineligible
	// scene (~every real scene today) submits no wave at all.
	//
	// All guards (EntityExists / WasCreatedDuringUpdate / IsActiveInHierarchy)
	// run here on the MAIN THREAD during the walk — IsActiveInHierarchy asserts
	// main-thread and mutates the per-slot cache, so it must NOT run on a worker.

	Zenith_Vector<Zenith_EntityID> xPendingEligible;
	xPendingEligible.Reserve(xSnapshotIDs.GetSize());

	const u_int uNumWorkers = g_xEngine.Tasks().GetNumWorkerThreads();

	// Flush the pending eligible run as one mutually-disjoint parallel wave, then
	// clear it. No-op when the run is empty (also avoids Zenith_TaskArray's
	// "0 invocations" assert).
	auto FlushEligibleWave = [&]()
	{
		const u_int uCount = xPendingEligible.GetSize();
		if (uCount == 0u) return;

		// Shard count = worker threads + 1 (the calling main thread participates
		// via bCallingThreadParticipates). +1 guarantees >= 1 even on a single-
		// core / pre-init box where GetNumWorkerThreads() is 0. Never more shards
		// than items (no empty shard; TaskArray forbids 0 invocations).
		u_int uNumInvocations = uNumWorkers + 1u;
		if (uNumInvocations > uCount)
		{
			uNumInvocations = uCount;
		}

		ParallelSimShardContext xCtx;
		xCtx.m_pxSceneData   = this;
		xCtx.m_pxEligibleIDs = &xPendingEligible;
		xCtx.m_fDt           = fDt;
		xCtx.m_bLateUpdate   = (eHook == LifecycleHook::LATE_UPDATE);

		Zenith_TaskArray xArray(
			ZENITH_PROFILE_INDEX__SCENE_UPDATE,
			&Zenith_SceneData::ParallelSimShardFunc,
			&xCtx,
			uNumInvocations,
			/* bCallingThreadParticipates = */ true);

		// Signal the wave-in-flight window so the SceneData component-read asserts
		// permit worker-thread reads of the (disjoint) eligible entities — exactly
		// the m_bRenderTasksActive pattern. Cleared the instant the barrier
		// returns, before any ineligible entity is dispatched, so the asserts go
		// back to strict main-thread-only outside the wave.
		g_xEngine.Scenes().SetParallelSimWaveActive(true);
		g_xEngine.Tasks().SubmitTaskArray(&xArray);
		xArray.WaitUntilComplete();
		g_xEngine.Scenes().SetParallelSimWaveActive(false);

		xPendingEligible.Clear();
	};

	for (u_int u = 0; u < xSnapshotIDs.GetSize(); ++u)
	{
		Zenith_EntityID uID = xSnapshotIDs.Get(u);
		if (!EntityExists(uID)) continue;
		if (WasCreatedDuringUpdate(uID)) continue;

		Zenith_Entity xEntity = GetEntity(uID);
		if (!xEntity.IsActiveInHierarchy()) continue;

		if (IsEntityParallelEligible(xEntity))
		{
			// Extend the current eligible run.
			xPendingEligible.PushBack(uID);
		}
		else
		{
			// End of an eligible run: flush it (barrier) so its writes are
			// visible, then dispatch this ineligible entity inline, in order —
			// byte-identical to the serial loop.
			FlushEligibleWave();
			switch (eHook)
			{
			case LifecycleHook::UPDATE:      xRegistry.DispatchOnUpdate(xEntity, fDt);     break;
			case LifecycleHook::LATE_UPDATE: xRegistry.DispatchOnLateUpdate(xEntity, fDt); break;
			}
		}
	}

	// Flush any trailing eligible run.
	FlushEligibleWave();
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
		if (xID.m_uIndex < g_xEngine.EntityStore().m_axEntitySlots.GetSize())
		{
			g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex).m_bCreatedDuringUpdate = false;
		}
	}
}

void Zenith_SceneData::FixedUpdate(float fFixedDt)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "FixedUpdate must be called from main thread");
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
	if (g_xEngine.Scenes().IsLoadingScene() || g_xEngine.Scenes().IsPrefabInstantiating())
		return;

	DispatchAwakeForEntity(xID);

	Zenith_EntitySlot& xSlot = g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex);
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
		g_xEngine.Scenes().PushLifecycleContext(m_strPath);
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
				HandleAwakeOverflow(uWaveStart, uWaveEnd, uMAX_AWAKE_ITERATIONS);
				break;
			}
		}
	}

	// Remove from lifecycle context after awake completes
	if (!m_strPath.empty())
	{
		g_xEngine.Scenes().PopLifecycleContext(m_strPath);
	}
}

// Release-build safety net: the Zenith_Assert in DispatchAwakeForNewScene compiles
// away in release builds, so without this explicit Zenith_Error a runaway scene
// would silently truncate its Awake dispatch.
//
// The helper destroys unawakened entities so the scene-lifecycle invariant holds
// (every surviving entity in DispatchEnableAndPendingStartsForNewScene must have
// received OnAwake). Leaving them in m_xActiveEntities would silently fire
// OnEnable/OnStart on entities that never got OnAwake.
//
// Snapshot IDs first because RemoveEntity erases from m_xActiveEntities. A scene
// that hits this path is malformed, so we're not worried about recursive OnDestroy
// creating more entities — they'd be swept by the same cleanup on the next load.
//
// Audit §3.6 note (Unity-parity divergence, deliberate): Unity's
// MonoBehaviour.OnDestroy only fires on objects that became active (i.e. had
// Awake). Zenith fires OnDestroy here on entities whose Awake never ran. That's
// a deliberate divergence: this branch only triggers on pathological content
// (100+ cascading Awake-creates-Awake waves), and routing overflow through
// RemoveEntity gives component destructors — especially those holding OS
// resources — a chance to clean up instead of silently leaking. The alternative
// (skip OnDestroy for strict Unity parity) would trade a predictable cleanup
// path for an invisible resource leak in an already-malformed scene.
// Refs: https://docs.unity3d.com/ScriptReference/MonoBehaviour.Awake.html
//       https://docs.unity3d.com/ScriptReference/MonoBehaviour.OnDestroy.html
void Zenith_SceneData::HandleAwakeOverflow(u_int uWaveStart, u_int uWaveEnd, u_int uMaxIterations)
{
	Zenith_Error(LOG_CATEGORY_SCENE,
		"DispatchAwakeForNewScene: Awake wave limit (%u) reached for scene '%s'; "
		"destroying %u unawakened entities. An OnAwake handler is creating "
		"entities without bound.",
		uMaxIterations, m_strPath.c_str(), uWaveEnd - uWaveStart);

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
			Zenith_EntitySlot& xSlot = g_xEngine.EntityStore().m_axEntitySlots.Get(xEntityID.m_uIndex);
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
	if (xEntityID.m_uIndex >= g_xEngine.EntityStore().m_axEntitySlots.GetSize()) return PendingStartResult::SKIP;
	Zenith_EntitySlot& xSlot = g_xEngine.EntityStore().m_axEntitySlots.Get(xEntityID.m_uIndex);
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
		// Underflow tolerance: same justification as the
		// MoveEntityInternal-during-Start path below (lines ~1035-1038).
		// Reproduced 2026-05-25 with Release_False + windowed +
		// --automated-test in DP: the harness's scene-load and the
		// first render frame interleave such that m_uPendingStartCount
		// can already be zero by the time this path runs (the entity's
		// PENDING_START bookkeeping was cleared elsewhere). Only
		// decrement when there's something to decrement -- the
		// underflow case means bookkeeping is already consistent.
		if (m_uPendingStartCount > 0)
		{
			m_uPendingStartCount--;
		}
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
		Zenith_SceneData* pxTargetData = g_xEngine.Scenes().GetSceneDataByHandle(xSlot.m_iSceneHandle);
		if (pxTargetData)
		{
			// Remove from target's pending list and decrement count
			// (don't use CancelPendingStart - lifecycle is still PENDING_START and
			// we want MarkEntityStarted to handle the transition, not revert to AWOKEN).
			//
			// Underflow tolerance (2026-05-13, MVP-0.1.3 CI bisect): the assert
			// was firing only on the CI runner during the DP harness's
			// between-tests scene cycling, where the target scene's pending list
			// can have already cleared this entity (e.g. via an earlier
			// MoveEntityInternal-during-Start). The list-erase is idempotent
			// (EraseValue returns false on miss), and we only need to decrement
			// when the count is positive -- the underflow case means
			// bookkeeping is already consistent.
			if (pxTargetData->m_uPendingStartCount > 0)
			{
				pxTargetData->m_uPendingStartCount--;
			}
			pxTargetData->m_axPendingStartEntities.EraseValue(xEntityID);
		}
		MarkEntityStarted(xEntityID);
		return PendingStartResult::CLEARED;
	}

	MarkEntityStarted(xEntityID);
	// Underflow tolerance: same justification as the cross-scene-move
	// branch above (lines ~1035-1038). Reproduced 2026-05-25 with
	// Release_False + windowed + --automated-test in DP: the harness's
	// scene-load and the first render frame interleave such that
	// m_uPendingStartCount can already be zero by the time this path
	// runs (the entity's PENDING_START bookkeeping was cleared
	// elsewhere -- e.g. an earlier MoveEntityInternal-during-Start
	// transferred the count to the target scene which dispatched Start
	// first). Only decrement when there's something to decrement.
	if (m_uPendingStartCount > 0)
	{
		m_uPendingStartCount--;
	}
	return PendingStartResult::CLEARED;
}

//==========================================================================
// Serialization
//
// SaveToFile, LoadFromFile, ReadEntityFromDataStream, and LoadFromDataStream
// now live in Zenith_SceneData_Serialization.cpp.
//==========================================================================

