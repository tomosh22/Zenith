#include "Zenith.h"

#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_ComponentMeta.h"

//==========================================================================
// Zenith_SceneManager — cross-scene entity operations
//
// Carved out of Zenith_SceneManager.cpp to keep the main TU focused on
// scene lifecycle (load/unload/active). The functions here move entities
// between scenes, rename scenes, merge scenes, and manage persistent-scene
// membership. All are static members of Zenith_SceneManager and access the
// statics declared in the main TU.
//==========================================================================

bool Zenith_SceneManager::MoveEntityInternal(Zenith_Entity& xEntity, Zenith_SceneData* pxTargetData)
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
		const Zenith_SceneData::Zenith_EntitySlot& xPreMoveSlot = Zenith_SceneData::s_axEntitySlots.Get(xEntityID.m_uIndex);
		Zenith_Assert(xPreMoveSlot.IsOccupied(),
			"MoveEntityInternal: source slot %u is not occupied at mutation point", xEntityID.m_uIndex);
		Zenith_Assert(xPreMoveSlot.m_uGeneration == xEntityID.m_uGeneration,
			"MoveEntityInternal: stale entity handle (slot gen=%u, entity gen=%u)",
			xPreMoveSlot.m_uGeneration, xEntityID.m_uGeneration);
	}

	// Update global slot to point to target scene
	Zenith_SceneData::s_axEntitySlots.Get(xEntityID.m_uIndex).m_iSceneHandle = pxTargetData->m_iHandle;

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
	Zenith_SceneData::Zenith_EntitySlot& xSlot = Zenith_SceneData::s_axEntitySlots.Get(xEntityID.m_uIndex);
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

bool Zenith_SceneManager::MoveEntityToScene(Zenith_Entity& xEntity, Zenith_Scene xTarget)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "MoveEntityToScene must be called from main thread");
	Zenith_Assert(!s_bRenderTasksActive, "MoveEntityToScene: scene mutation while render tasks are reading — render-task invariant violated");

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

	Zenith_SceneData* pxTargetData = GetSceneData(xTarget);
	if (!pxTargetData)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "MoveEntityToScene: Invalid target scene data");
		return false;
	}

	if (pxTargetData->m_bIsUnloading)
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

bool Zenith_SceneManager::RenameScene(Zenith_Scene xScene, const std::string& strNewName)
{
	// E.18 / finding 3.15: the scene-name cache (s_axLoadedSceneNames) is an O(1) lookup
	// shortcut for GetSceneByName. Before this API, renaming via friend access to
	// pxSceneData->m_strName left the cache stale — lookups by old name still hit, by
	// new name missed. Now cache and data move together.
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "RenameScene must be called from main thread");
	Zenith_Assert(!s_bRenderTasksActive, "RenameScene: scene mutation while render tasks are reading — render-task invariant violated");

	if (!xScene.IsValid())
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "RenameScene: invalid scene handle");
		return false;
	}

	if (xScene.m_iHandle == s_iPersistentSceneHandle)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "RenameScene: cannot rename the persistent scene");
		return false;
	}

	Zenith_SceneData* pxSceneData = GetSceneData(xScene);
	if (!pxSceneData)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "RenameScene: scene data not found");
		return false;
	}

	pxSceneData->m_strName = strNewName;

	// Update cache entry in place. Matches how AddToSceneNameCache writes — the list
	// is short (one entry per loaded scene), so linear scan is cheap.
	for (u_int i = 0; i < s_axLoadedSceneNames.GetSize(); ++i)
	{
		if (s_axLoadedSceneNames.Get(i).m_iHandle == xScene.m_iHandle)
		{
			s_axLoadedSceneNames.Get(i).m_strName = strNewName;
			pxSceneData->MarkDirty();
			return true;
		}
	}

	// Cache entry missing — shouldn't happen for a valid loaded scene, but be defensive.
	Zenith_Warning(LOG_CATEGORY_SCENE,
		"RenameScene: scene handle %d is loaded but has no cache entry — appending one",
		xScene.m_iHandle);
	AddToSceneNameCache(xScene.m_iHandle, strNewName);
	pxSceneData->MarkDirty();
	return true;
}

bool Zenith_SceneManager::MergeScenes(Zenith_Scene xSource, Zenith_Scene xTarget)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "MergeScenes must be called from main thread");
	Zenith_Assert(!s_bRenderTasksActive, "MergeScenes: scene mutation while render tasks are reading — render-task invariant violated");

	Zenith_SceneData* pxSource = GetSceneData(xSource);
	Zenith_SceneData* pxTarget = GetSceneData(xTarget);

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
	if (xSource.m_iHandle == s_iPersistentSceneHandle)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "MergeScenes: Cannot merge from persistent scene");
		return false;
	}

	// Unity behavior: If source is active, target becomes active
	if (xSource.m_iHandle == s_iActiveSceneHandle)
	{
		SetActiveScene(xTarget);
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
	UnloadSceneForced(xSource);
	return true;
}

//==========================================================================
// Entity Persistence
//==========================================================================

void Zenith_SceneManager::MarkEntityPersistent(Zenith_Entity& xEntity)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "MarkEntityPersistent must be called from main thread");

	if (!xEntity.IsValid()) return;

	// Unity parity: DontDestroyOnLoad moves the ROOT of the hierarchy,
	// not just the target entity. Walk up to find the root.
	Zenith_Entity xRoot = xEntity;
	while (xRoot.GetParentEntityID().IsValid())
	{
		Zenith_SceneData* pxSceneData = xRoot.GetSceneData();
		if (!pxSceneData) break;
		Zenith_EntityID xParentID = xRoot.GetParentEntityID();
		if (!pxSceneData->EntityExists(xParentID)) break;
		xRoot = pxSceneData->GetEntity(xParentID);
	}

	Zenith_Scene xPersistent = GetPersistentScene();
	MoveEntityToScene(xRoot, xPersistent);

	// Fire callback AFTER transfer so the entity reference is valid in the new scene
	// Note: Unity's DontDestroyOnLoad doesn't have a callback; this is a Zenith extension
	FireEntityPersistentCallbacks(xRoot);
}

Zenith_Scene Zenith_SceneManager::GetPersistentScene()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetPersistentScene must be called from main thread");
	Zenith_Scene xScene;
	xScene.m_iHandle = s_iPersistentSceneHandle;
	if (s_iPersistentSceneHandle >= 0 && s_iPersistentSceneHandle < static_cast<int>(s_axSceneGenerations.GetSize()))
	{
		xScene.m_uGeneration = s_axSceneGenerations.Get(s_iPersistentSceneHandle);
	}
	return xScene;
}
