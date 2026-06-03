#include "Zenith.h"
#include "ZenithECS/Zenith_SceneSystem.h"

#include "ZenithECS/Zenith_ComponentMeta.h"
// Phase 5b: this TU no longer names any concrete component. DestroyImmediate's
// former component-side detach shim is now the slot-backed
// Zenith_Entity::SetParent(INVALID_ENTITY_ID), so the concrete-component
// Components/ include is gone (the matching ecs_leaf_allowlist.txt entry goes
// stale). CreateEntity below still adds the engine's default component(s) exclusively via the
// engine-installed m_pfnAddDefaultComponents hook, never by naming a component here.
//==========================================================================
// Zenith_SceneSystem — entity creation, cross-scene entity moves,
// DontDestroyOnLoad promotion, and entity destruction.
//
// Phase 3 (ECS leaf-extraction): the creation API (CreateEntity / CreateEntityBare)
// is now the single entity-construction entry point — the former creating ctor
// Zenith_Entity(Zenith_SceneData*, const std::string&) was removed. The
// destruction / persistence / move ops are PRIVATE instance methods; the public
// lifecycle surface is on Zenith_Entity (Destroy / DestroyImmediate /
// DontDestroyOnLoad / MoveToScene), which forwards here as a friend.
//
// LEAF INVARIANT: the NEW creation code (CreateEntityInScene) must NOT name a
// concrete component type. The engine's default component(s) are
// added exclusively by the engine-installed m_pfnAddDefaultComponents hook.
//
// Entity-state mutation reads/writes Zenith_SceneData privates via
// `friend class Zenith_SceneSystem`.
//==========================================================================

Zenith_Entity Zenith_SceneSystem::CreateEntityInScene(Zenith_SceneData* pxSceneData, const std::string& strName, bool bRunDefaultComponentsHook)
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "CreateEntity must be called from main thread");
	Zenith_Assert(pxSceneData != nullptr, "CreateEntityInScene: null scene data");

	// Allocate a slot and construct a generation-aware handle. This is the exact
	// slot-level path the old creating ctor used.
	Zenith_EntityID xID = pxSceneData->CreateEntity();
	Zenith_Assert(Zenith_ECS_EntityStore().m_axEntityComponents.Get(xID.m_uIndex).empty(),
		"Entity slot %u already has components - registry not cleared or ID collision", xID.m_uIndex);

	Zenith_Entity xEntity(pxSceneData, xID);

	// Set initial state directly in the slot (single source of truth).
	Zenith_SceneData::Zenith_EntitySlot& xSlot = Zenith_ECS_EntityStore().m_axEntitySlots.Get(xID.m_uIndex);
	xSlot.m_strName = strName;
	xSlot.m_bEnabled = true;
	xSlot.m_bTransient = true;  // Default: transient (not saved)

	// Default components are added ONLY through the engine-installed
	// hook — keeps the ECS leaf free of any concrete component type. CreateEntityBare
	// passes bRunDefaultComponentsHook=false to skip it entirely.
	if (bRunDefaultComponentsHook && m_xRuntimeHooks.m_pfnAddDefaultComponents)
	{
		m_xRuntimeHooks.m_pfnAddDefaultComponents(xEntity);
	}

	// Track entities created during Update() - they won't receive callbacks until next frame.
	pxSceneData->RegisterCreatedDuringUpdate(xID);

	// Unity parity: dispatch Awake/OnEnable immediately for runtime-created entities.
	pxSceneData->DispatchImmediateLifecycleForRuntime(xID);

	return xEntity;
}

Zenith_Entity Zenith_SceneSystem::CreateEntity(Zenith_SceneData* pxSceneData, const std::string& strName)
{
	return CreateEntityInScene(pxSceneData, strName, /*bRunDefaultComponentsHook*/ true);
}

Zenith_Entity Zenith_SceneSystem::CreateEntityBare(Zenith_SceneData* pxSceneData, const std::string& strName)
{
	return CreateEntityInScene(pxSceneData, strName, /*bRunDefaultComponentsHook*/ false);
}

Zenith_Entity Zenith_SceneSystem::CreateEntity(Zenith_Scene xScene, const std::string& strName)
{
	Zenith_SceneData* pxData = GetSceneData(xScene);
	if (!pxData)
	{
		Zenith_Error(LOG_CATEGORY_SCENE,
			"CreateEntity: scene (handle=%d gen=%u) is not loaded; entity '%s' was not created",
			xScene.m_iHandle, xScene.m_uGeneration, strName.c_str());
		return Zenith_Entity();
	}
	return CreateEntity(pxData, strName);
}

Zenith_Entity Zenith_SceneSystem::CreateEntityBare(Zenith_Scene xScene, const std::string& strName)
{
	Zenith_SceneData* pxData = GetSceneData(xScene);
	if (!pxData)
	{
		Zenith_Error(LOG_CATEGORY_SCENE,
			"CreateEntityBare: scene (handle=%d gen=%u) is not loaded; entity '%s' was not created",
			xScene.m_iHandle, xScene.m_uGeneration, strName.c_str());
		return Zenith_Entity();
	}
	return CreateEntityBare(pxData, strName);
}

Zenith_Entity Zenith_SceneSystem::CreateEntity(const std::string& strName)
{
	const Zenith_Scene xTarget = GetDefaultCreationScene();
	if (!xTarget.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_SCENE,
			"CreateEntity: no creation target available (no active scene and no SceneCreationTargetScope); "
			"entity '%s' was not created",
			strName.c_str());
		return Zenith_Entity();
	}

	Zenith_SceneData* pxData = GetSceneData(xTarget);
	if (!pxData)
	{
		Zenith_Error(LOG_CATEGORY_SCENE,
			"CreateEntity: creation target scene (handle=%d gen=%u) is not loaded; "
			"entity '%s' was not created",
			xTarget.m_iHandle, xTarget.m_uGeneration, strName.c_str());
		return Zenith_Entity();
	}

	return CreateEntity(pxData, strName);
}

Zenith_Entity Zenith_SceneSystem::CreateEntityBare(const std::string& strName)
{
	const Zenith_Scene xTarget = GetDefaultCreationScene();
	if (!xTarget.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_SCENE,
			"CreateEntityBare: no creation target available (no active scene and no SceneCreationTargetScope); "
			"entity '%s' was not created",
			strName.c_str());
		return Zenith_Entity();
	}

	Zenith_SceneData* pxData = GetSceneData(xTarget);
	if (!pxData)
	{
		Zenith_Error(LOG_CATEGORY_SCENE,
			"CreateEntityBare: creation target scene (handle=%d gen=%u) is not loaded; "
			"entity '%s' was not created",
			xTarget.m_iHandle, xTarget.m_uGeneration, strName.c_str());
		return Zenith_Entity();
	}

	return CreateEntityBare(pxData, strName);
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
		const Zenith_SceneData::Zenith_EntitySlot& xPreMoveSlot = Zenith_ECS_EntityStore().m_axEntitySlots.Get(xEntityID.m_uIndex);
		Zenith_Assert(xPreMoveSlot.IsOccupied(),
			"MoveEntityInternal: source slot %u is not occupied at mutation point", xEntityID.m_uIndex);
		Zenith_Assert(xPreMoveSlot.m_uGeneration == xEntityID.m_uGeneration,
			"MoveEntityInternal: stale entity handle (slot gen=%u, entity gen=%u)",
			xPreMoveSlot.m_uGeneration, xEntityID.m_uGeneration);
	}

	// Update global slot to point to target scene
	Zenith_ECS_EntityStore().m_axEntitySlots.Get(xEntityID.m_uIndex).m_iSceneHandle = pxTargetData->m_iHandle;

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
	Zenith_SceneData::Zenith_EntitySlot& xSlot = Zenith_ECS_EntityStore().m_axEntitySlots.Get(xEntityID.m_uIndex);
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
	Zenith_Assert(Zenith_ECS_IsMainThread(), "MoveEntityToScene must be called from main thread");
	Zenith_Assert(!Zenith_AreRenderTasksActive(), "MoveEntityToScene: scene mutation while render tasks are reading — render-task invariant violated");

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

	Zenith_SceneData* pxTargetData = Zenith_SceneSystem::Get().GetSceneData(xTarget);
	if (!pxTargetData)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "MoveEntityToScene: Invalid target scene data");
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

//==========================================================================
// Entity Persistence
//==========================================================================

void Zenith_SceneSystem::MarkEntityPersistent(Zenith_Entity& xEntity)
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "MarkEntityPersistent must be called from main thread");

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

	Zenith_Scene xPersistent = Zenith_SceneSystem::Get().GetPersistentScene();

	// Precondition: the destination scene must be loaded. Without this guard,
	// a previously-Reset persistent scene (not loaded but still in m_axScenes)
	// would silently accept the move and defer the failure to the next
	// GetComponent call on the moved entity — typically in a different frame
	// and unrelated stack trace.
	Zenith_SceneData* pxPersistentData = Zenith_SceneSystem::Get().GetSceneData(xPersistent);
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
	Zenith_Assert(Zenith_ECS_IsMainThread(), "Destroy must be called from main thread");

	if (!xEntity.IsValid()) return;

	Zenith_SceneData* pxSceneData = xEntity.GetSceneData();
	if (pxSceneData)
	{
		pxSceneData->MarkForDestruction(xEntity.GetEntityID());
	}
}

void Zenith_SceneSystem::Destroy(Zenith_Entity& xEntity, float fDelay)
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "Destroy must be called from main thread");

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
	Zenith_Assert(Zenith_ECS_IsMainThread(), "DestroyImmediate must be called from main thread");

	if (!xEntity.IsValid()) return;

	Zenith_SceneData* pxSceneData = xEntity.GetSceneData();
	if (pxSceneData)
	{
		Zenith_EntityID xEntityID = xEntity.GetEntityID();

		// Clear from pending destruction if present (use global slot flag).
		if (xEntityID.m_uIndex < Zenith_ECS_EntityStore().m_axEntitySlots.GetSize())
		{
			Zenith_ECS_EntityStore().m_axEntitySlots.Get(xEntityID.m_uIndex).m_bMarkedForDestruction = false;
		}

		// Remove from pending destruction list to prevent stale processing.
		pxSceneData->m_xPendingDestruction.EraseValue(xEntityID);

		// Detach from parent (Unity parity: DestroyImmediate(child) decreases
		// parent.transform.childCount immediately).
		// Phase 5b: detach via the slot-backed Zenith_Entity API (the hierarchy is
		// owned by the slot). Behaviour-identical to the former component-side
		// detach shim, which already forwarded here.
		Zenith_Entity xLocalEntity(pxSceneData, xEntityID);
		xLocalEntity.SetParent(INVALID_ENTITY_ID);

		// RemoveEntity handles child destruction recursively and dispatches
		// OnDisable/OnDestroy for every entity (including children).
		pxSceneData->RemoveEntity(xEntityID);
	}
}
