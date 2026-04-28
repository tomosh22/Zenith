#pragma once

// Zenith_SceneRegistry — owns the API surface for scene-slot management,
// scene-by-{handle,index,name,path,build-index} queries, scene counts,
// active/persistent scene resolution, name cache, build-index registry,
// and RenameScene.
//
// Phase A note: the underlying static state (s_axScenes, generations, free-
// handle list, etc.) currently lives as private members of Zenith_SceneManager
// for migration safety. The registry is friend-of-manager so it can read those
// members directly without churning the 163 internal lifecycle access sites.
// A later polish step can migrate the storage into this class once each Phase A
// extraction has settled.
//
// Game and editor code never includes this header. Public Zenith_SceneManager
// query methods are forwarders that delegate here.

// Permanently independent of Zenith_SceneManager.h. The registry is included
// FROM manager.h (after the manager class declaration), not the other way round.
// Including manager.h here would form a #pragma-once cycle that, when registry.h
// is included first, results in the manager's template body referencing a
// friend-injected forward declaration of Zenith_SceneRegistry rather than the
// real class.
#include "EntityComponent/Zenith_Scene.h"
#include "Collections/Zenith_Vector.h"
#include <string>

class Zenith_SceneData;
struct Zenith_EntityID;

class Zenith_SceneRegistry
{
public:
	//==========================================================================
	// Storage (A2b: state migrated from Zenith_SceneManager).
	//
	// Public statics on a class declared in an internal header. Game and editor
	// code never sees them. Manager-side TUs (Zenith_SceneManager.cpp,
	// Zenith_SceneOperationQueue.cpp, Zenith_SceneEntityOwnership.cpp,
	// Zenith_SceneLifecycleScheduler.cpp) read and mutate them via
	// Zenith_SceneRegistry::s_* qualification.
	//==========================================================================

	struct SceneNameEntry
	{
		std::string m_strName;
		int m_iHandle;
	};

	static Zenith_Vector<Zenith_SceneData*> s_axScenes;
	static Zenith_Vector<uint32_t>          s_axSceneGenerations;
	static Zenith_Vector<int>               s_axFreeHandles;
	static int                              s_iActiveSceneHandle;
	static int                              s_iPersistentSceneHandle;
	static uint64_t                         s_ulNextLoadTimestamp;
	static Zenith_Vector<SceneNameEntry>    s_axLoadedSceneNames;
	static Zenith_Vector<std::string>       s_axBuildIndexToPath;

	//==========================================================================
	// Lifecycle
	//==========================================================================

	// Reset registry state to initial values: clears all storage, resets
	// active/persistent handles to -1, resets load-timestamp counter to 1.
	// Called from Zenith_SceneManager::Shutdown().
	static void Shutdown();

	//==========================================================================
	// Slot management
	//==========================================================================

	// Allocate a scene handle. Reuses a free slot if one exists (no generation
	// bump on reuse — that happens in FreeSceneHandle), otherwise grows the
	// slot vectors. Returns the handle index.
	static int AllocateSceneHandle();

	// Release a scene handle back to the freelist. Bumps the slot's generation
	// counter so any cached Zenith_Scene with the old generation becomes invalid.
	static void FreeSceneHandle(int iHandle);

	// Total slot count (includes empty slots). Used by render systems iterating
	// all slots; always pairs with GetSceneDataAtSlot / GetLoadedSceneDataAtSlot.
	static uint32_t GetSceneSlotCount();

	// Raw access to the scene data at a slot. Returns nullptr for empty slots.
	// Used by render systems during the render-task window — safe from worker
	// threads while AreRenderTasksActive() is true.
	static Zenith_SceneData* GetSceneDataAtSlot(uint32_t uIndex);

	// As GetSceneDataAtSlot but also nullptr if the scene is unloading or not
	// yet activated. Render code prefers this filtered variant.
	static Zenith_SceneData* GetLoadedSceneDataAtSlot(uint32_t uIndex);

	//==========================================================================
	// Scene handle/data resolution
	//==========================================================================

	static Zenith_Scene MakeInvalidScene();
	static Zenith_SceneData* GetSceneData(Zenith_Scene xScene);
	static Zenith_SceneData* GetSceneDataByHandle(int iHandle);
	static Zenith_SceneData* GetSceneDataForEntity(Zenith_EntityID xID);
	static Zenith_Scene GetSceneFromHandle(int iHandle);

	//==========================================================================
	// Scene queries (game/editor visible via Zenith_SceneManager forwarders)
	//==========================================================================

	static Zenith_Scene GetActiveScene();
	static Zenith_Scene GetSceneAt(uint32_t uIndex);
	static Zenith_Scene GetSceneByBuildIndex(int iBuildIndex);
	static Zenith_Scene GetSceneByName(const std::string& strName);
	static Zenith_Scene GetSceneByPath(const std::string& strPath);

	static uint32_t GetLoadedSceneCount();
	static uint32_t GetTotalSceneCount();
	static uint32_t GetBuildSceneCount();

	//==========================================================================
	// Active / persistent
	//==========================================================================

	// Pick a fallback active scene when the current active is unloaded.
	// iExcludeHandle skips a specific slot during selection (used during
	// teardown of a SCENE_LOAD_SINGLE staging swap).
	static int SelectNewActiveScene(int iExcludeHandle = -1);

	static Zenith_Scene GetPersistentScene();

	//==========================================================================
	// Visibility / update predicates
	//==========================================================================

	static bool IsSceneVisibleToUser(u_int uSlotIndex, const Zenith_SceneData* pxData);
	static bool IsSceneUpdatable(const Zenith_SceneData* pxData);

	//==========================================================================
	// Name cache
	//==========================================================================

	static void AddToSceneNameCache(int iHandle, const std::string& strName);
	static void RemoveFromSceneNameCache(int iHandle);

	//==========================================================================
	// Build-index registry
	//==========================================================================

	static void RegisterSceneBuildIndex(int iBuildIndex, const std::string& strPath);
	static void ClearBuildIndexRegistry();
	static const std::string& GetRegisteredScenePath(int iBuildIndex);
	static uint32_t GetBuildIndexRegistrySize();

	//==========================================================================
	// Scene rename (atomic with name-cache update)
	//==========================================================================

	static bool RenameScene(Zenith_Scene xScene, const std::string& strNewName);
};
