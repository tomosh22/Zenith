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
#include "Collections/Zenith_Vector.h"
#include "EntityComponent/Zenith_Scene.h"
#include <string>

class Zenith_SceneData;
struct Zenith_EntityID;
class Zenith_Entity;
class Zenith_CameraComponent;

class Zenith_SceneRegistry
{
public:
	//==========================================================================
	// Storage type (only — actual state moved to Zenith_SceneRegistry
	// owned by Zenith_Engine in Phase 5b). Sister TUs that used to read
	// Zenith_SceneRegistry::s_axScenes etc. now read
	// g_xEngine.SceneRegistry().m_axScenes through the Impl.
	//==========================================================================

	struct SceneNameEntry
	{
		std::string m_strName;
		int m_iHandle;
	};

	//==========================================================================
	// Lifecycle
	//==========================================================================

	// Reset registry state to initial values: clears all storage, resets
	// active/persistent handles to -1, resets load-timestamp counter to 1.
	// Called from Zenith_SceneManager::Shutdown().
	void Shutdown();

	//==========================================================================
	// Slot management
	//==========================================================================

	// Allocate a scene handle. Reuses a free slot if one exists (no generation
	// bump on reuse — that happens in FreeSceneHandle), otherwise grows the
	// slot vectors. Returns the handle index.
	int AllocateSceneHandle();

	// Release a scene handle back to the freelist. Bumps the slot's generation
	// counter so any cached Zenith_Scene with the old generation becomes invalid.
	void FreeSceneHandle(int iHandle);

	// Total slot count (includes empty slots). Used by render systems iterating
	// all slots; always pairs with GetSceneDataAtSlot / GetLoadedSceneDataAtSlot.
	uint32_t GetSceneSlotCount();

	// Raw access to the scene data at a slot. Returns nullptr for empty slots.
	// Used by render systems during the render-task window — safe from worker
	// threads while AreRenderTasksActive() is true.
	Zenith_SceneData* GetSceneDataAtSlot(uint32_t uIndex);

	// As GetSceneDataAtSlot but also nullptr if the scene is unloading or not
	// yet activated. Render code prefers this filtered variant.
	Zenith_SceneData* GetLoadedSceneDataAtSlot(uint32_t uIndex);

	//==========================================================================
	// Scene handle/data resolution
	//==========================================================================

	Zenith_Scene MakeInvalidScene();
	Zenith_SceneData* GetSceneData(Zenith_Scene xScene);
	Zenith_SceneData* GetSceneDataByHandle(int iHandle);
	Zenith_SceneData* GetSceneDataForEntity(Zenith_EntityID xID);
	Zenith_Scene GetSceneFromHandle(int iHandle);

	//==========================================================================
	// Scene queries (game/editor visible via Zenith_SceneManager forwarders)
	//==========================================================================

	Zenith_Scene GetActiveScene();
	Zenith_Scene GetSceneAt(uint32_t uIndex);
	Zenith_Scene GetSceneByBuildIndex(int iBuildIndex);
	Zenith_Scene GetSceneByName(const std::string& strName);
	Zenith_Scene GetSceneByPath(const std::string& strPath);

	uint32_t GetLoadedSceneCount();
	uint32_t GetTotalSceneCount();
	uint32_t GetBuildSceneCount();

	//==========================================================================
	// Active / persistent
	//==========================================================================

	// Pick a fallback active scene when the current active is unloaded.
	// iExcludeHandle skips a specific slot during selection (used during
	// teardown of a SCENE_LOAD_SINGLE staging swap).
	int SelectNewActiveScene(int iExcludeHandle = -1);

	Zenith_Scene GetPersistentScene();

	//==========================================================================
	// Visibility / update predicates
	//==========================================================================

	bool IsSceneVisibleToUser(u_int uSlotIndex, const Zenith_SceneData* pxData);
	bool IsSceneUpdatable(const Zenith_SceneData* pxData);

	//==========================================================================
	// Name cache
	//==========================================================================

	void AddToSceneNameCache(int iHandle, const std::string& strName);
	void RemoveFromSceneNameCache(int iHandle);

	//==========================================================================
	// Build-index registry
	//==========================================================================

	void RegisterSceneBuildIndex(int iBuildIndex, const std::string& strPath);
	void ClearBuildIndexRegistry();
	const std::string& GetRegisteredScenePath(int iBuildIndex);
	uint32_t GetBuildIndexRegistrySize();

	//==========================================================================
	// Path canonicalisation (used by GetSceneByPath internally; also exposed
	// for the LoadScene paths that compare path keys).
	//==========================================================================
	static std::string CanonicalisePath(const std::string& strPath);

	//==========================================================================
	// Scene rename (atomic with name-cache update)
	//==========================================================================

	bool RenameScene(Zenith_Scene xScene, const std::string& strNewName);

	//==========================================================================
	// Scene creation / activation / pausing (Phase 5c: delegators that forward
	// to Zenith_SceneManager. The actual bodies live there until Phase 5e moves
	// them onto this class).
	//==========================================================================

	Zenith_Scene CreateScene(const std::string& strName);
	Zenith_Scene CreateEmptyScene(const std::string& strName, bool bAllowSetActive = true);

	bool SetActiveScene(Zenith_Scene xScene);
	void SetScenePaused(Zenith_Scene xScene, bool bPaused);
	bool IsScenePaused(Zenith_Scene xScene);

	// Cross-scene main-camera lookup. Returns the first enabled camera marked
	// as main across all loaded scenes.
	Zenith_CameraComponent* FindMainCameraAcrossScenes();

	//==========================================================================
	// Data members (was Zenith_SceneRegistry)
	//==========================================================================

	// Scene-slot table. m_axScenes[i] is the SceneData* for handle i (nullptr
	// when the slot is free). m_axSceneGenerations[i] increments on every
	// free/realloc cycle so cached Zenith_Scene handles can detect staleness.
	Zenith_Vector<Zenith_SceneData*>     m_axScenes;
	Zenith_Vector<uint32_t>              m_axSceneGenerations;
	Zenith_Vector<int>                   m_axFreeHandles;

	// Active / persistent scene tracking.
	int                                  m_iActiveSceneHandle     = -1;
	int                                  m_iPersistentSceneHandle = -1;

	// Monotonic load-order counter (1-based; 0 == not loaded).
	uint64_t                             m_ulNextLoadTimestamp    = 1;

	// Name-lookup cache. Parallel to m_axScenes; rebuilt by AddToSceneNameCache /
	// RemoveFromSceneNameCache as scenes load and unload.
	Zenith_Vector<SceneNameEntry>        m_axLoadedSceneNames;

	// Build-index -> path registry, populated at editor automation time.
	Zenith_Vector<std::string>           m_axBuildIndexToPath;

	//==========================================================================
	// Cross-scene component iteration (template).
	//
	// Defined inline at the bottom of Zenith_SceneManager.h (where SceneData
	// is fully visible). Until that header is deleted in Phase 5e, callers can
	// reach it via g_xEngine.SceneRegistry().GetAllOfComponentTypeFromAllScenes<T>(out).
	//==========================================================================
	template<typename T>
	void GetAllOfComponentTypeFromAllScenes(Zenith_Vector<T*>& xOut);
};
