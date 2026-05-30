#pragma once

// =============================================================================
// Zenith_SceneSystem — the single scene API.
//
// Accessed via g_xEngine.Scenes(). Every game, every editor panel, every
// engine subsystem talks to scenes through this one class. There are no
// internal subsystem classes any more — Registry / OperationQueue / Lifecycle-
// Scheduler / CallbackBus / EntityOwnership all merged in.
//
// Implementation is split across the Internal/ TUs (there is no
// Zenith_SceneSystem.cpp — bootstrap and LoadScene live in the Lifecycle and
// Operations files respectively):
//   Internal/Zenith_SceneSystem_Registry.cpp        — slot table + queries + path canonicalise
//   Internal/Zenith_SceneSystem_Operations.cpp      — LoadScene / Unload + render reset
//   Internal/Zenith_SceneSystem_Lifecycle.cpp       — bootstrap, Update pipeline, RAII guard bodies
//   Internal/Zenith_SceneSystem_Callbacks.cpp       — callback bus
//   Internal/Zenith_SceneSystem_EntityOwnership.cpp — cross-scene entity moves
// =============================================================================

#include "Collections/Zenith_Vector.h"
#include "Collections/Zenith_HashSet.h"
#include "EntityComponent/Zenith_Scene.h"
#include <atomic>
#include <cstdint>
#include <string>
// Was Zenith_SceneCallbackTypes.h — typedefs + enum inlined below.

struct Zenith_Scene;
struct Zenith_EntityID;
class Zenith_Entity;
class Zenith_SceneData;
class Zenith_CameraComponent;
class Zenith_DataStream;

//==============================================================================
// Scene-load mode + callback typedefs (was Zenith_SceneCallbackTypes.h)
//==============================================================================

// Scene loading modes (mirrors Unity's LoadSceneMode).
enum Zenith_SceneLoadMode : uint8_t
{
	SCENE_LOAD_SINGLE = 0,                    // unload existing non-persistent, load new
	SCENE_LOAD_ADDITIVE = 1,                  // keep existing, add new
	SCENE_LOAD_ADDITIVE_WITHOUT_LOADING = 2,  // create empty scene with no file load
};

using Zenith_SceneCallbackHandle = uint64_t;
inline constexpr Zenith_SceneCallbackHandle Zenith_INVALID_SCENE_CALLBACK_HANDLE = 0;

using Zenith_SceneChangedCallback   = void(*)(Zenith_Scene, Zenith_Scene);
using Zenith_SceneLoadedCallback    = void(*)(Zenith_Scene, Zenith_SceneLoadMode);
using Zenith_SceneUnloadingCallback = void(*)(Zenith_Scene);  // BEFORE destruction
using Zenith_SceneUnloadedCallback  = void(*)(Zenith_Scene);  // AFTER destruction

class Zenith_SceneSystem
{
public:
	using CallbackHandle = Zenith_SceneCallbackHandle;
	using InitialSceneLoadFn = void(*)();
	static constexpr CallbackHandle INVALID_CALLBACK_HANDLE = Zenith_INVALID_SCENE_CALLBACK_HANDLE;

	Zenith_SceneSystem() = default;
	~Zenith_SceneSystem() = default;

	Zenith_SceneSystem(const Zenith_SceneSystem&) = delete;
	Zenith_SceneSystem& operator=(const Zenith_SceneSystem&) = delete;
	Zenith_SceneSystem(Zenith_SceneSystem&&) = delete;
	Zenith_SceneSystem& operator=(Zenith_SceneSystem&&) = delete;

	//==========================================================================
	// Bootstrap (called from Zenith_Engine::Initialise/Shutdown)
	//==========================================================================

	static void InitialiseSubsystems();
	static void ShutdownSubsystems();
#ifdef ZENITH_TESTING
	static void ResetForNextTest();
#endif

	//==========================================================================
	// Slot table + lifecycle
	//==========================================================================

	void Shutdown();

	int  AllocateSceneHandle();
	void FreeSceneHandle(int iHandle);

	uint32_t          GetSceneSlotCount();
	Zenith_SceneData* GetSceneDataAtSlot(uint32_t uIndex);
	Zenith_SceneData* GetLoadedSceneDataAtSlot(uint32_t uIndex);

	//==========================================================================
	// Scene handle / data resolution
	//==========================================================================

	Zenith_Scene      MakeInvalidScene();
	Zenith_SceneData* GetSceneData(Zenith_Scene xScene);
	Zenith_SceneData* GetSceneDataByHandle(int iHandle);
	Zenith_SceneData* GetSceneDataForEntity(Zenith_EntityID xID);
	Zenith_Scene      GetSceneFromHandle(int iHandle);

	//==========================================================================
	// Scene queries
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

	int SelectNewActiveScene(int iExcludeHandle = -1);
	Zenith_Scene GetPersistentScene();
	// Raw handle of the persistent ("DontDestroyOnLoad") scene, for internal
	// collaborators (e.g. Zenith_SceneData asserts) that only need to compare handles.
	int GetPersistentSceneHandle() const { return m_iPersistentSceneHandle; }
	bool SetActiveScene(Zenith_Scene xScene);
	void SetScenePaused(Zenith_Scene xScene, bool bPaused);
	bool IsScenePaused(Zenith_Scene xScene);

	//==========================================================================
	// Visibility / update predicates
	//==========================================================================

	bool IsSceneVisibleToUser(u_int uSlotIndex, const Zenith_SceneData* pxData);
	bool IsSceneUpdatable(const Zenith_SceneData* pxData);

	//==========================================================================
	// Name cache + build-index registry
	//==========================================================================

	void AddToSceneNameCache(int iHandle, const std::string& strName);
	void RemoveFromSceneNameCache(int iHandle);

	void RegisterSceneBuildIndex(int iBuildIndex, const std::string& strPath);
	void ClearBuildIndexRegistry();
	const std::string& GetRegisteredScenePath(int iBuildIndex);
	uint32_t GetBuildIndexRegistrySize();

	static std::string CanonicalisePath(const std::string& strPath);

	bool RenameScene(Zenith_Scene xScene, const std::string& strNewName);

	//==========================================================================
	// Scene creation + cross-scene main camera
	//==========================================================================

	Zenith_Scene CreateScene(const std::string& strName);
	Zenith_Scene CreateEmptyScene(const std::string& strName, bool bAllowSetActive = true);

	Zenith_CameraComponent* FindMainCameraAcrossScenes();

	template<typename T>
	void GetAllOfComponentTypeFromAllScenes(Zenith_Vector<T*>& xOut);

	//==========================================================================
	// Load / Unload
	//==========================================================================

	Zenith_Scene LoadScene(const std::string& strPath, Zenith_SceneLoadMode eMode);
	Zenith_Scene LoadSceneByIndex(int iBuildIndex, Zenith_SceneLoadMode eMode);

	void UnloadScene(Zenith_Scene xScene);
	void UnloadSceneForced(Zenith_Scene xScene);
	void UnloadUnusedAssets();
	bool CanUnloadScene(Zenith_Scene xScene);
	bool HasPendingDestructions();
	void ResetAllRenderSystems();

	void UnloadAllNonPersistent(int iExcludeHandle = -1);
	Zenith_Vector<Zenith_Scene> CollectNonPersistentScenes(int iExcludeHandle);
	bool DestroyScenesAndFireUnloaded(const Zenith_Vector<Zenith_Scene>& axScenes,
	                                   const Zenith_HashSet<int>& xAlreadyFired);
	void UpdateActiveSceneAfterUnload(Zenith_Scene xOldActive);
	void UnloadOneScene(Zenith_Scene xScene, bool& bActiveSceneUnloadedInOut);

	//==========================================================================
	// Update pipeline + lifecycle-deferral state
	//==========================================================================

	void Update(float fDt);

	void PushLifecycleContext(const std::string& strCanonicalPath);
	void PopLifecycleContext(const std::string& strCanonicalPath);
	bool IsCircularLoadDependency(const std::string& strCanonicalPath);

	void SetInitialSceneLoadCallback(InitialSceneLoadFn pfn);
	InitialSceneLoadFn GetInitialSceneLoadCallback();

	void SetFixedTimestep(float fTimestep);
	float GetFixedTimestep();

	Zenith_Scene GetDefaultCreationScene();

	void SetMainLoopRunning(bool bRunning);
	bool IsLoadingScene() const        { return m_bIsLoadingScene; }
	bool IsPrefabInstantiating() const { return m_bIsPrefabInstantiating; }
	bool IsUpdating() const            { return m_bIsUpdating; }
	int  GetPendingBuildIndex() const  { return m_iPendingBuildIndex; }

	void DrainPendingLoadIfAny();

#ifdef ZENITH_TESTING
	void DispatchFullLifecycleInit();
#endif

	bool AreRenderTasksActive() const
	{
		return m_bRenderTasksActive.load(std::memory_order_acquire);
	}
	void SetRenderTasksActive(bool b) { m_bRenderTasksActive.store(b, std::memory_order_release); }

	// WS10 sparse-set query toggle. When true (the shipped default), Zenith_Query
	// uses the O(matches) sparse-set fast path; when false it uses the legacy
	// O(entities x types) scan. Both paths are kept (reversibility), and the
	// pool always dual-writes the sparse index regardless of this flag, so the
	// toggle can be flipped live (tests/bench pin a path and restore it). Atomic
	// because ForEach may run on render-task worker threads, mirroring
	// m_bRenderTasksActive. Compiled in ALL configs (not #ifdef-d).
	bool AreSparseQueryReadsEnabled() const
	{
		return m_bUseSparseQueryReads.load(std::memory_order_acquire);
	}
	void SetSparseQueryReads(bool b) { m_bUseSparseQueryReads.store(b, std::memory_order_release); }

	//==========================================================================
	// Callback bus
	//==========================================================================

	CallbackHandle RegisterActiveSceneChanged(Zenith_SceneChangedCallback pfn);
	bool UnregisterActiveSceneChanged(CallbackHandle ulHandle);

	CallbackHandle RegisterSceneLoaded(Zenith_SceneLoadedCallback pfn);
	bool UnregisterSceneLoaded(CallbackHandle ulHandle);

	CallbackHandle RegisterSceneUnloading(Zenith_SceneUnloadingCallback pfn);
	bool UnregisterSceneUnloading(CallbackHandle ulHandle);

	CallbackHandle RegisterSceneUnloaded(Zenith_SceneUnloadedCallback pfn);
	bool UnregisterSceneUnloaded(CallbackHandle ulHandle);

	void FireUnloadCallbacksAndSelectNewActive(int iHandle, Zenith_Scene xScene);

	void FireActiveSceneChanged(Zenith_Scene xOld, Zenith_Scene xNew);
	void FireSceneLoaded(Zenith_Scene xScene, Zenith_SceneLoadMode eMode);
	void FireSceneUnloading(Zenith_Scene xScene);
	void FireSceneUnloaded(Zenith_Scene xScene);

	bool IsActiveSceneSuppressed();
	bool HasDeferredOldActive();
	Zenith_Scene GetDeferredOldActive();
	void SetDeferredOldActive(Zenith_Scene xScene);

private:
	friend class ActiveSceneChangeSuppressionScope;
	// RAII guards (declared below the class) mutate private lifecycle flags
	// directly in their ctor/dtor bodies.
	friend struct Zenith_PrefabInstantiationGuard;
	friend struct Zenith_SceneUpdateDeferralGuard;
	friend struct Zenith_SceneCreationTargetScope;
	void SetActiveSceneSuppressed(bool b);
	void ClearDeferredOldActive();
public:

#ifdef ZENITH_TESTING
	u_int GetActiveSceneChangedCallbackCount();
	u_int GetSceneLoadedCallbackCount();
	u_int GetSceneUnloadingCallbackCount();
	u_int GetSceneUnloadedCallbackCount();
	u_int GetCallbackDispatchDepth();
	u_int GetPendingRemovalCount();
	void  SetNextCallbackHandleForTest(CallbackHandle ulValue);
	void  SetActiveSceneSuppressedForTest(bool b);
	uint32_t GetUnloadUnusedAssetsCallCount() const { return m_uUnloadUnusedAssetsCallCount; }
#endif

	//==========================================================================
	// Cross-scene entity ownership (was Zenith_SceneEntityOwnership)
	//==========================================================================

	static Zenith_Entity CreateEntity(const std::string& strName);
	static bool MoveEntityToScene(Zenith_Entity& xEntity, Zenith_Scene xTarget);
	static bool MergeScenes(Zenith_Scene xSource, Zenith_Scene xTarget);
	static void MarkEntityPersistent(Zenith_Entity& xEntity);
	static void Destroy(Zenith_Entity& xEntity);
	static void Destroy(Zenith_Entity& xEntity, float fDelay);
	static void DestroyImmediate(Zenith_Entity& xEntity);

	//==========================================================================
	// Lifecycle-deferral guard accessor (used by the engine bootstrap).
	//==========================================================================

	bool& MutableLifecycleLoadingFlagForGuard();

private:
	static bool MoveEntityInternal(Zenith_Entity& xEntity, Zenith_SceneData* pxTargetData);

	//==========================================================================
	// Nested types
	//==========================================================================

public:
	struct SceneNameEntry
	{
		std::string m_strName;
		int m_iHandle;
	};

	template<typename T>
	struct CallbackEntry
	{
		CallbackHandle m_ulHandle;
		T m_pfnCallback;
	};

	template<typename TCallback>
	struct CallbackList
	{
		Zenith_Vector<CallbackEntry<TCallback>> m_axEntries;
	};

private:
	//==========================================================================
	// Callback-bus internal helpers (were anonymous-namespace free functions in
	// Zenith_SceneSystem_Callbacks.cpp; now members so the state they touch can
	// stay private). Templated bodies are instantiated only within that TU.
	//==========================================================================

	bool           IsCallbackHandleInUse(CallbackHandle ulHandle) const;
	CallbackHandle AllocateCallbackHandle();
	bool           IsCallbackPendingRemoval(CallbackHandle ulHandle) const;
	void           ProcessPendingCallbackRemovals();

	template<typename TCallback>
	CallbackHandle Register(CallbackList<TCallback>& xList, TCallback pfn);
	template<typename TCallback>
	bool Unregister(CallbackList<TCallback>& xList, CallbackHandle ulHandle);
	template<typename TCallback, typename... Args>
	void Fire(CallbackList<TCallback>& xList, Args&&... args);

	//==========================================================================
	// Data members — engine-internal state. Private; the implementation TUs all
	// define members of this one class, so they reach these directly. External
	// code goes through the public accessors (e.g. MutableLifecycleLoadingFlagForGuard).
	//==========================================================================

	Zenith_Vector<Zenith_SceneData*>     m_axScenes;
	Zenith_Vector<uint32_t>              m_axSceneGenerations;
	Zenith_Vector<int>                   m_axFreeHandles;
	int                                  m_iActiveSceneHandle     = -1;
	int                                  m_iPersistentSceneHandle = -1;
	uint64_t                             m_ulNextLoadTimestamp    = 1;
	Zenith_Vector<SceneNameEntry>        m_axLoadedSceneNames;
	Zenith_Vector<std::string>           m_axBuildIndexToPath;

	//==========================================================================
	// Data members (was Zenith_SceneLifecycleScheduler)
	//==========================================================================

	bool                          m_bIsLoadingScene          = false;
	bool                          m_bIsPrefabInstantiating   = false;
	bool                          m_bIsUpdating              = false;

	float                         m_fFixedTimeAccumulator    = 0.0f;
	float                         m_fFixedTimestep           = 0.02f;  // 50Hz default

	Zenith_Vector<std::string>    m_axCurrentlyLoadingPaths;
	Zenith_Vector<std::string>    m_axLifecycleLoadStack;

	int                           m_iPendingBuildIndex       = -1;

	Zenith_Vector<Zenith_Scene>   m_axCreationTargetStack;

	bool                          m_bIsMainLoopRunning       = false;
	InitialSceneLoadFn            m_pfnInitialSceneLoad      = nullptr;

	// Deferred LoadScene slot — populated by LoadScene/LoadSceneByIndex when
	// called while m_bIsUpdating or m_bIsLoadingScene is true. Drained by
	// Zenith_SceneUpdateDeferralGuard's destructor (or the scheduler Update's
	// post-amble) once the outer pass returns. Only the most recent request
	// is honoured — chained loads in the same frame collapse to the last one.
	struct PendingLoad
	{
		bool        m_bSet         = false;
		bool        m_bByIndex     = false;
		int         m_iBuildIndex  = -1;
		std::string m_strPath;
		uint8_t     m_uMode        = 0;  // Zenith_SceneLoadMode
	};
	PendingLoad                   m_xPendingLoad;

	// Render-phase boundary signal. Compiled in ALL configs (not just assert
	// builds): parallel-sim work needs an authoritative, race-free value of
	// whether render tasks are reading scene state, even in shipping builds.
	// Set true around ExecuteRenderGraph (see Zenith_Core.cpp); the scene-
	// mutation asserts read it to permit concurrent reads during that window.
	std::atomic<bool>             m_bRenderTasksActive { false };

	// WS10 sparse-set query read path. Default true = sparse fast path shipped on.
	// Plain std::atomic (NOT #ifdef-d) so it compiles + works in *_False configs.
	std::atomic<bool>             m_bUseSparseQueryReads { true };

	//==========================================================================
	// Data members (was Zenith_SceneCallbackBus)
	//==========================================================================

	CallbackList<Zenith_SceneChangedCallback>      m_xActiveSceneChangedCallbacks;
	CallbackList<Zenith_SceneLoadedCallback>       m_xSceneLoadedCallbacks;
	CallbackList<Zenith_SceneUnloadingCallback>    m_xSceneUnloadingCallbacks;
	CallbackList<Zenith_SceneUnloadedCallback>     m_xSceneUnloadedCallbacks;

	CallbackHandle                m_ulNextCallbackHandle = 1;
	Zenith_Vector<CallbackHandle> m_axCallbacksPendingRemoval;
	uint32_t                      m_uFiringCallbacksDepth = 0;

	bool         m_bSuppressActiveSceneChanged = false;
	bool         m_bHaveDeferredOldActive      = false;
	Zenith_Scene m_xDeferredOldActive;

#ifdef ZENITH_TESTING
	uint32_t m_uUnloadUnusedAssetsCallCount = 0;
#endif
};

//==========================================================================
// RAII scope guards (was Zenith_SceneSystemGuards.h)
//
// Save-on-construct / restore-on-destruct around the corresponding
// Zenith_SceneSystem flag, so nested guards stack correctly. The bodies for
// the three with non-inline implementations live in
// Internal/Zenith_SceneSystem_Lifecycle.cpp alongside the state they mutate.
//==========================================================================

struct Zenith_LifecycleDeferralGuard
{
	bool& m_bFlag;
	bool  m_bPrevValue;
	Zenith_LifecycleDeferralGuard(bool& bFlag) : m_bFlag(bFlag), m_bPrevValue(bFlag) { m_bFlag = true; }
	~Zenith_LifecycleDeferralGuard() { m_bFlag = m_bPrevValue; }
	Zenith_LifecycleDeferralGuard(const Zenith_LifecycleDeferralGuard&) = delete;
	Zenith_LifecycleDeferralGuard& operator=(const Zenith_LifecycleDeferralGuard&) = delete;
};

struct Zenith_PrefabInstantiationGuard
{
	Zenith_PrefabInstantiationGuard();
	~Zenith_PrefabInstantiationGuard();
	Zenith_PrefabInstantiationGuard(const Zenith_PrefabInstantiationGuard&) = delete;
	Zenith_PrefabInstantiationGuard& operator=(const Zenith_PrefabInstantiationGuard&) = delete;
private:
	bool m_bPrevValue;
};

struct Zenith_SceneUpdateDeferralGuard
{
	Zenith_SceneUpdateDeferralGuard();
	~Zenith_SceneUpdateDeferralGuard();
	Zenith_SceneUpdateDeferralGuard(const Zenith_SceneUpdateDeferralGuard&) = delete;
	Zenith_SceneUpdateDeferralGuard& operator=(const Zenith_SceneUpdateDeferralGuard&) = delete;
private:
	bool m_bPrevValue;
};

struct Zenith_SceneCreationTargetScope
{
	explicit Zenith_SceneCreationTargetScope(Zenith_Scene xScene);
	~Zenith_SceneCreationTargetScope();
	Zenith_SceneCreationTargetScope(const Zenith_SceneCreationTargetScope&) = delete;
	Zenith_SceneCreationTargetScope& operator=(const Zenith_SceneCreationTargetScope&) = delete;
};

//==========================================================================
// ActiveSceneChangeSuppressionScope — RAII over the suppression window.
// Construct around a SINGLE-mode teardown to suppress intermediate
// ActiveSceneChanged dispatches; subscribers observe a single old→new
// transition at Complete (or no transition at Cancel). Forgetting either
// asserts in the destructor.
//==========================================================================

class ActiveSceneChangeSuppressionScope
{
public:
	explicit ActiveSceneChangeSuppressionScope(Zenith_Scene xInitialOldActive);
	~ActiveSceneChangeSuppressionScope();

	ActiveSceneChangeSuppressionScope(const ActiveSceneChangeSuppressionScope&) = delete;
	ActiveSceneChangeSuppressionScope& operator=(const ActiveSceneChangeSuppressionScope&) = delete;

	void Complete(Zenith_Scene xNewActive);
	void Cancel();

private:
	Zenith_Scene m_xOldActive;
	bool m_bResolved = false;
};

// Template body — defined after the class so it can reference Zenith_SceneData
// via the registry's iteration. Body uses inline forwarder to the registry-
// internal AppendAllOfComponentType through SceneData.h (pulled in by callers).
#include "EntityComponent/Zenith_SceneData.h"

template<typename T>
inline void Zenith_SceneSystem::GetAllOfComponentTypeFromAllScenes(Zenith_Vector<T*>& xOut)
{
	xOut.Clear();
	const uint32_t uSlotCount = GetSceneSlotCount();
	for (uint32_t uIndex = 0; uIndex < uSlotCount; ++uIndex)
	{
		Zenith_SceneData* pxData = GetLoadedSceneDataAtSlot(uIndex);
		if (pxData)
		{
			pxData->AppendAllOfComponentType<T>(xOut);
		}
	}
}
