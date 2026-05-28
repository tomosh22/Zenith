#include "Zenith.h"
#include "EntityComponent/Zenith_SceneSystem.h"

#include "EntityComponent/Zenith_ComponentMeta.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
//==========================================================================
// Zenith_SceneSystem — cross-scene entity moves, scene merges,
// DontDestroyOnLoad promotion, and entity destruction.
//
// These are static members (Unity-style Object.Destroy / DontDestroyOnLoad
// surface), so they reach the singleton through g_xEngine.Scenes(). Entity-state
// mutation reads/writes Zenith_SceneData privates via `friend class Zenith_SceneSystem`.
//==========================================================================

Zenith_Entity Zenith_SceneSystem::CreateEntity(const std::string& strName)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "CreateEntity must be called from main thread");

	const Zenith_Scene xTarget = g_xEngine.Scenes().GetDefaultCreationScene();
	if (!xTarget.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_SCENE,
			"CreateEntity: no creation target available (no active scene and no SceneCreationTargetScope); "
			"entity '%s' was not created",
			strName.c_str());
		return Zenith_Entity();
	}

	Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(xTarget);
	if (!pxData)
	{
		Zenith_Error(LOG_CATEGORY_SCENE,
			"CreateEntity: creation target scene (handle=%d gen=%u) is not loaded; "
			"entity '%s' was not created",
			xTarget.m_iHandle, xTarget.m_uGeneration, strName.c_str());
		return Zenith_Entity();
	}

	return Zenith_Entity(pxData, strName);
}

bool Zenith_SceneSystem::MoveEntityInternal(Zenith_Entity& xEntity, Zenith_SceneData* pxTargetData)
{
	Zenith_SceneData* pxSourceData = xEntity.GetSceneData();
	if (!pxSourceData || pxSourceData == pxTargetData)
	{
		return false;
	}

	Zenith_EntityID xEntityID = xEntity.GetEntityID();
	Zenith_Vector<Zenith_EntityID> axChildIDs = xEntity.GetChildEntityIDs();

	// Move children recursively FIRST (depth-first)
	for (u_int i = 0; i < axChildIDs.GetSize(); ++i)
	{
		Zenith_Entity xChild = pxSourceData->TryGetEntity(axChildIDs.Get(i));
		if (xChild.IsValid())
		{
			if (!MoveEntityInternal(xChild, pxTargetData))
			{
				Zenith_Warning(LOG_CATEGORY_SCENE, "MoveEntityInternal: Failed to move child entity (idx=%u)", axChildIDs.Get(i).m_uIndex);
				return false;
			}
		}
	}

	// Transfer all components from source pools to target pools (move-construct, zero-copy)
	Zenith_ComponentMetaRegistry::Get().TransferAllComponents(xEntityID, pxSourceData, pxTargetData);

	// C2 (N3): lock in the assumption that the slot is still occupied and the entity
	// handle's generation still matches — this guards a future refactor that allows
	// slot destroy to interleave with move from corrupting the global entity table.
	{
		const Zenith_SceneData::Zenith_EntitySlot& xPreMoveSlot = g_xEngine.EntityStore().m_axEntitySlots.Get(xEntityID.m_uIndex);
		Zenith_Assert(xPreMoveSlot.IsOccupied(),
			"MoveEntityInternal: source slot %u is not occupied at mutation point", xEntityID.m_uIndex);
		Zenith_Assert(xPreMoveSlot.m_uGeneration == xEntityID.m_uGeneration,
			"MoveEntityInternal: stale entity handle (slot gen=%u, entity gen=%u)",
			xPreMoveSlot.m_uGeneration, xEntityID.m_uGeneration);
	}

	// Update global slot to point to target scene
	g_xEngine.EntityStore().m_axEntitySlots.Get(xEntityID.m_uIndex).m_iSceneHandle = pxTargetData->m_iHandle;

	// Move entity from source's active list to target's active list
	pxSourceData->m_xActiveEntities.EraseValue(xEntityID);
	pxTargetData->m_xActiveEntities.PushBack(xEntityID);

	// Transfer timed destructions for this entity from source to target scene
	for (int i = static_cast<int>(pxSourceData->m_axTimedDestructions.GetSize()) - 1; i >= 0; --i)
	{
		if (pxSourceData->m_axTimedDestructions.Get(i).m_xEntityID == xEntityID)
		{
			pxTargetData->m_axTimedDestructions.PushBack(pxSourceData->m_axTimedDestructions.Get(i));
			pxSourceData->m_axTimedDestructions.Remove(i);
		}
	}

	// Adjust pending start count and list if this entity hasn't had Start() called yet
	Zenith_SceneData::Zenith_EntitySlot& xSlot = g_xEngine.EntityStore().m_axEntitySlots.Get(xEntityID.m_uIndex);
	if (xSlot.IsPendingStart())
	{
		Zenith_Assert(pxSourceData->m_uPendingStartCount > 0, "PendingStartCount underflow in MoveEntityInternal");
		pxSourceData->m_uPendingStartCount--;
		pxTargetData->m_uPendingStartCount++;
		pxSourceData->m_axPendingStartEntities.EraseValue(xEntityID);
		pxTargetData->m_axPendingStartEntities.PushBack(xEntityID);
	}

	// Transfer newly created entity tracking so OnAwake is dispatched by the correct scene.
	// Without this, entities moved before their first Update (e.g. DontDestroyOnLoad in
	// Project_LoadInitialScene) would have OnAwake dispatched by the source scene with the
	// wrong scene data, causing HasComponent to fail and OnAwake to never fire.
	pxSourceData->m_axNewlyCreatedEntities.EraseValue(xEntityID);
	pxTargetData->m_axNewlyCreatedEntities.PushBack(xEntityID);

	// Handle main camera reference transfer
	bool bWasSourceMainCamera = (pxSourceData->GetMainCameraEntity() == xEntityID);
	if (bWasSourceMainCamera)
	{
		pxSourceData->SetMainCameraEntity(INVALID_ENTITY_ID);
		if (!pxTargetData->GetMainCameraEntity().IsValid())
		{
			pxTargetData->SetMainCameraEntity(xEntityID);
		}
	}

	// Invalidate root caches on both scenes
	pxSourceData->InvalidateRootEntityCache();
	pxTargetData->InvalidateRootEntityCache();
	pxSourceData->MarkDirty();
	pxTargetData->MarkDirty();

	// Entity handle stays valid - EntityID is unchanged, global slot points to new scene
	return true;
}

bool Zenith_SceneSystem::MoveEntityToScene(Zenith_Entity& xEntity, Zenith_Scene xTarget)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "MoveEntityToScene must be called from main thread");
	Zenith_Assert(!g_xEngine.Scenes().AreRenderTasksActive(), "MoveEntityToScene: scene mutation while render tasks are reading — render-task invariant violated");

	if (!xEntity.IsValid())
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "MoveEntityToScene: Invalid entity");
		return false;
	}

	if (!xTarget.IsValid())
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "MoveEntityToScene: Invalid target scene");
		return false;
	}

	Zenith_SceneData* pxTargetData = g_xEngine.Scenes().GetSceneData(xTarget);
	if (!pxTargetData)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "MoveEntityToScene: Invalid target scene data");
		return false;
	}

	if (pxTargetData->IsUnloading())
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "MoveEntityToScene: Target scene '%s' is being unloaded", pxTargetData->m_strName.c_str());
		return false;
	}

	// Validate source and target are different scenes
	Zenith_SceneData* pxSourceData = xEntity.GetSceneData();
	if (pxSourceData == pxTargetData)
	{
		// Moving to same scene is a no-op (not an error)
		return true;
	}

	// Unity parity: Only root entities can be moved between scenes (Unity throws InvalidOperationException)
	if (xEntity.GetParentEntityID().IsValid())
	{
		Zenith_Error(LOG_CATEGORY_SCENE,
			"MoveEntityToScene: Entity '%s' has a parent. Only root entities can be moved between scenes.",
			xEntity.GetName().c_str());
		return false;
	}

	// MoveEntityInternal updates xEntity in place to point to the new entity
	return MoveEntityInternal(xEntity, pxTargetData);
}

bool Zenith_SceneSystem::MergeScenes(Zenith_Scene xSource, Zenith_Scene xTarget)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "MergeScenes must be called from main thread");
	Zenith_Assert(!g_xEngine.Scenes().AreRenderTasksActive(), "MergeScenes: scene mutation while render tasks are reading — render-task invariant violated");

	Zenith_SceneData* pxSource = g_xEngine.Scenes().GetSceneData(xSource);
	Zenith_SceneData* pxTarget = g_xEngine.Scenes().GetSceneData(xTarget);

	if (!pxSource || !pxTarget)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "MergeScenes: Invalid source or target scene");
		return false;
	}

	if (pxSource == pxTarget)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "MergeScenes: Source and target are the same scene");
		return false;
	}

	// Cannot merge from the persistent scene
	if (xSource.m_iHandle == g_xEngine.Scenes().m_iPersistentSceneHandle)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "MergeScenes: Cannot merge from persistent scene");
		return false;
	}

	// Unity behavior: If source is active, target becomes active
	if (xSource.m_iHandle == g_xEngine.Scenes().m_iActiveSceneHandle)
	{
		g_xEngine.Scenes().SetActiveScene(xTarget);
	}

	// Get all root entities from source
	Zenith_Vector<Zenith_Entity> axRoots;
	xSource.GetRootEntities(axRoots);

	// Move each root (and children recursively)
	for (u_int i = 0; i < axRoots.GetSize(); ++i)
	{
		Zenith_Entity& xRoot = axRoots.Get(i);
		MoveEntityToScene(xRoot, xTarget);
	}

	// Unload the now-empty source scene (forced - bypasses last-scene guard since source is empty)
	g_xEngine.Scenes().UnloadSceneForced(xSource);
	return true;
}

//==========================================================================
// Entity Persistence
//==========================================================================

void Zenith_SceneSystem::MarkEntityPersistent(Zenith_Entity& xEntity)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "MarkEntityPersistent must be called from main thread");

	if (!xEntity.IsValid()) return;

	// B5: strict root-only semantics. Unity's DontDestroyOnLoad operates on
	// root GameObjects only — children survive because their root survives.
	// Pre-B5 Zenith silently walked up to the root, which masked caller bugs
	// (e.g., a script calling DontDestroyOnLoad() on `this` from a child
	// component would unexpectedly promote the entire parent chain). The
	// editor hierarchy panel and other callers that want subtree promotion
	// now walk to the root explicitly at the call site.
	if (xEntity.GetParentEntityID().IsValid())
	{
		Zenith_Error(LOG_CATEGORY_SCENE,
			"MarkEntityPersistent: entity '%s' is not a root (has parent EntityID idx=%u gen=%u). "
			"Walk to the root explicitly before calling, or use SetParent(INVALID_ENTITY_ID) to detach. "
			"Children of a root automatically follow when the root is moved.",
			xEntity.GetName().c_str(),
			xEntity.GetParentEntityID().m_uIndex,
			xEntity.GetParentEntityID().m_uGeneration);
		return;
	}

	Zenith_Scene xPersistent = g_xEngine.Scenes().GetPersistentScene();

	// Precondition: the destination scene must be loaded. Without this guard,
	// a previously-Reset persistent scene (not loaded but still in m_axScenes)
	// would silently accept the move and defer the failure to the next
	// GetComponent call on the moved entity — typically in a different frame
	// and unrelated stack trace.
	Zenith_SceneData* pxPersistentData = g_xEngine.Scenes().GetSceneData(xPersistent);
	Zenith_Assert(pxPersistentData && pxPersistentData->IsLoaded(),
		"MarkEntityPersistent: persistent scene is not loaded (m_bIsLoaded=false). "
		"Something Reset()'d it without restoring the flag — check editor backup-restore "
		"path and any other call site that calls Reset() on a scene in m_axScenes.");

	MoveEntityToScene(xEntity, xPersistent);
}

//==========================================================================
// Entity destruction
//==========================================================================

void Zenith_SceneSystem::Destroy(Zenith_Entity& xEntity)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "Destroy must be called from main thread");

	if (!xEntity.IsValid()) return;

	Zenith_SceneData* pxSceneData = xEntity.GetSceneData();
	if (pxSceneData)
	{
		pxSceneData->MarkForDestruction(xEntity.GetEntityID());
	}
}

void Zenith_SceneSystem::Destroy(Zenith_Entity& xEntity, float fDelay)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "Destroy must be called from main thread");

	if (!xEntity.IsValid()) return;

	if (fDelay <= 0.0f)
	{
		Destroy(xEntity);
		return;
	}

	Zenith_SceneData* pxSceneData = xEntity.GetSceneData();
	if (pxSceneData)
	{
		pxSceneData->MarkForTimedDestruction(xEntity.GetEntityID(), fDelay);
	}
}

void Zenith_SceneSystem::DestroyImmediate(Zenith_Entity& xEntity)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "DestroyImmediate must be called from main thread");

	if (!xEntity.IsValid()) return;

	Zenith_SceneData* pxSceneData = xEntity.GetSceneData();
	if (pxSceneData)
	{
		Zenith_EntityID xEntityID = xEntity.GetEntityID();

		// Clear from pending destruction if present (use global slot flag).
		if (xEntityID.m_uIndex < g_xEngine.EntityStore().m_axEntitySlots.GetSize())
		{
			g_xEngine.EntityStore().m_axEntitySlots.Get(xEntityID.m_uIndex).m_bMarkedForDestruction = false;
		}

		// Remove from pending destruction list to prevent stale processing.
		pxSceneData->m_xPendingDestruction.EraseValue(xEntityID);

		// Detach from parent (Unity parity: DestroyImmediate(child) decreases
		// parent.transform.childCount immediately).
		Zenith_Entity xLocalEntity(pxSceneData, xEntityID);
		xLocalEntity.GetComponent<Zenith_TransformComponent>().DetachFromParent();

		// RemoveEntity handles child destruction recursively and dispatches
		// OnDisable/OnDestroy for every entity (including children).
		pxSceneData->RemoveEntity(xEntityID);
	}
}
