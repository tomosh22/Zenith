#pragma once

// Zenith_SceneEntityOwnership — owns the cross-scene entity lifecycle:
//   * MoveEntityToScene / MoveEntityInternal (entity transfer between scenes)
//   * MergeScenes (move all roots from source to target, then unload source)
//   * MarkEntityPersistent (DontDestroyOnLoad — moves root subtree to the
//     persistent scene)
//   * Destroy / Destroy(delay) / DestroyImmediate
//
// The public Zenith_SceneManager methods forward into this class.
//
// Independent of Zenith_SceneManager.h to keep the same one-way include shape
// the other internal subsystems use.

#include "EntityComponent/Zenith_Scene.h"
#include <string>

class Zenith_Entity;
class Zenith_SceneData;

class Zenith_SceneEntityOwnership
{
public:
	//==========================================================================
	// Entity creation (Phase 5c: migrated from Zenith_SceneEntityOwnership::CreateEntity)
	//==========================================================================

	// Creates an entity in the current creation target (active scene unless a
	// SceneCreationTargetScope is in scope). Returns an invalid Zenith_Entity
	// if no creation target is available or the target scene is not loaded.
	static Zenith_Entity CreateEntity(const std::string& strName);

	//==========================================================================
	// Cross-scene entity transfer
	//==========================================================================

	static bool MoveEntityToScene(Zenith_Entity& xEntity, Zenith_Scene xTarget);
	static bool MergeScenes(Zenith_Scene xSource, Zenith_Scene xTarget);

	//==========================================================================
	// DontDestroyOnLoad
	//==========================================================================

	// Walks to the entity's root and moves the whole subtree to the persistent
	// scene. Fires the Zenith-extension EntityPersistent callback after transfer.
	static void MarkEntityPersistent(Zenith_Entity& xEntity);

	//==========================================================================
	// Entity destruction
	//==========================================================================

	static void Destroy(Zenith_Entity& xEntity);
	static void Destroy(Zenith_Entity& xEntity, float fDelay);
	static void DestroyImmediate(Zenith_Entity& xEntity);

private:
	// Recursive zero-copy entity transfer. EntityID is preserved; components
	// are move-constructed into the target scene's pools. No lifecycle events
	// fire (Unity parity: MoveGameObjectToScene is seamless).
	static bool MoveEntityInternal(Zenith_Entity& xEntity, Zenith_SceneData* pxTargetData);
};
