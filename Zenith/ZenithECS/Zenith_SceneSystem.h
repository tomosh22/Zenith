#pragma once

// =============================================================================
// Zenith_SceneSystem — the single scene API.
//
// Accessed via g_xEngine.Scenes(). Every game, every editor panel, every
// engine subsystem talks to scenes through this one class.
//
// There is no Zenith_SceneSystem.cpp — the implementation is split across the
// Internal/ TUs purely for file size (every method is a member of this one
// class; state lives as private members):
//   Internal/Zenith_SceneSystem_Registry.cpp        — slot table + queries + path canonicalise
//   Internal/Zenith_SceneSystem_Operations.cpp      — LoadScene / Unload + render reset
//   Internal/Zenith_SceneSystem_Lifecycle.cpp       — bootstrap, Update pipeline, RAII guard bodies
//   Internal/Zenith_SceneSystem_Callbacks.cpp       — active-scene suppression + unload-reselection helper
//   Internal/Zenith_SceneSystem_EntityOwnership.cpp — cross-scene entity moves
// =============================================================================

#include "Collections/Zenith_Vector.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Internal/Zenith_ECSRuntimeHooks.h"
// Zenith_SceneSystem owns the process-wide Zenith_EntityStore. Include the full
// type here so GetEntityStore() can return a reference and the ctor/dtor see the
// complete type.
#include "ZenithECS/Internal/Zenith_EntityStore.h"
#include <atomic>
#include <cstdint>
#include <string>

struct Zenith_Scene;
struct Zenith_EntityID;
class Zenith_Entity;
class Zenith_SceneData;
class Zenith_DataStream;

//==============================================================================
// Scene-load mode
//==============================================================================

// Scene loading modes (mirrors Unity's LoadSceneMode).
enum Zenith_SceneLoadMode : uint8_t
{
	SCENE_LOAD_SINGLE = 0,                    // unload existing non-persistent, load new
	SCENE_LOAD_ADDITIVE = 1,                  // keep existing, add new
	SCENE_LOAD_ADDITIVE_WITHOUT_LOADING = 2,  // create empty scene with no file load
};

//==============================================================================
// Zenith_SceneInfo — POD snapshot of a scene's metadata.
//
// Replaces the former per-field Zenith_Scene metadata getters (GetName /
// GetPath / GetBuildIndex / IsLoaded / HasUnsavedChanges). Fetched in one call
// via Zenith_SceneSystem::GetSceneInfo(xScene). For an invalid / stale /
// unloaded scene handle, GetSceneInfo returns a default-constructed
// Zenith_SceneInfo (empty strings, m_iBuildIndex == -1, m_bLoaded == false) —
// matching the old getters' null-scene fallbacks.
//==============================================================================
struct Zenith_SceneInfo
{
	std::string m_strName;
	std::string m_strPath;
	int         m_iBuildIndex       = -1;
	bool        m_bLoaded           = false;
#ifdef ZENITH_TOOLS
	// Editor-only dirty flag.
	bool        m_bHasUnsavedChanges = false;
#endif
};

class Zenith_SceneSystem
{
public:
	// The ctor allocates the owned Zenith_EntityStore and publishes the
	// process-wide instance pointer; the dtor frees the store and clears it.
	// Bodies live in Internal/Zenith_SceneSystem_Registry.cpp alongside the
	// s_pxInstance definition.
	Zenith_SceneSystem();
	~Zenith_SceneSystem();

	Zenith_SceneSystem(const Zenith_SceneSystem&) = delete;
	Zenith_SceneSystem& operator=(const Zenith_SceneSystem&) = delete;
	Zenith_SceneSystem(Zenith_SceneSystem&&) = delete;
	Zenith_SceneSystem& operator=(Zenith_SceneSystem&&) = delete;

	//==========================================================================
	// Process-wide entity-store ownership.
	//
	// Zenith_SceneSystem owns the single Zenith_EntityStore (the three global
	// entity arrays). The store's lifetime is exactly this object's lifetime
	// (allocated in the ctor, freed in the dtor).
	//
	// Get(): ECS-internal process-wide accessor returning the single live
	// instance, set in the ctor. Lets the ECS core reach the store (via the
	// PRIVATE GetEntityStore() below) without naming g_xEngine.
	//==========================================================================
	static Zenith_SceneSystem& Get() { return *s_pxInstance; }

	//==========================================================================
	// Bootstrap (called from Zenith_Engine::Initialise/Shutdown)
	//==========================================================================

	static void InitialiseSubsystems();
	static void ShutdownSubsystems();
#ifdef ZENITH_TESTING
	static void ResetForNextTest();
#endif

	//==========================================================================
	// Scene handle / data resolution
	//
	// LIFETIME: these return a RAW Zenith_SceneData* into recyclable scene storage,
	// valid only until that scene unloads (LoadScene SINGLE / UnloadScene recycle the
	// slot). Do NOT cache the pointer across frames or across a load/unload — store the
	// generation-checked Zenith_Scene handle and re-resolve via GetSceneData() at each
	// use. Returns nullptr for an invalid / stale / unloaded handle.
	//==========================================================================

	Zenith_SceneData* GetSceneData(Zenith_Scene xScene);
	Zenith_SceneData* GetSceneDataForEntity(Zenith_EntityID xID);

	// Convenience: resolve a stored Zenith_EntityID straight to a live
	// Zenith_Entity handle, collapsing the pervasive
	// GetSceneDataForEntity(id) -> null-check -> TryGetEntity(id) chain. NON-
	// asserting: returns an invalid handle (IsValid() == false) when the entity's
	// owning scene is gone or the slot generation is stale, so it is a drop-in for
	// prune / validity loops. Resolves the entity's OWN owning scene (correct
	// across additive scenes + DontDestroyOnLoad), exactly like GetSceneDataForEntity.
	Zenith_Entity ResolveEntity(Zenith_EntityID xID);

	//==========================================================================
	// Scene queries
	//==========================================================================

	Zenith_Scene GetActiveScene();
	Zenith_Scene GetSceneAt(uint32_t uIndex);

	// Convenience forwarder for the pervasive GetSceneData(GetActiveScene())
	// two-step: returns the active scene's SceneData, or nullptr when there is no
	// active scene / it is unloaded (the same null contract GetSceneData callers
	// already handle). Same LIFETIME caveat as GetSceneData — the returned pointer
	// is raw + recyclable; never cache it across a load/unload.
	Zenith_SceneData* GetActiveSceneData() { return GetSceneData(GetActiveScene()); }

	//==========================================================================
	// Scene metadata snapshot + root-entity iteration
	//
	// GetSceneInfo replaces the deleted per-field Zenith_Scene metadata getters:
	// reads the SceneData/slot metadata (name / path / build index / loaded /
	// loaded-additively / cached root-entity count) for the given handle and
	// returns it as a Zenith_SceneInfo POD. An invalid / stale / unloaded handle
	// yields a default-constructed Zenith_SceneInfo (same null-scene fallbacks the
	// old getters had).
	//==========================================================================
	Zenith_SceneInfo GetSceneInfo(Zenith_Scene xScene) const;

	//==========================================================================
	// Active / persistent
	//==========================================================================

	Zenith_Scene GetPersistentScene();
	bool SetActiveScene(Zenith_Scene xScene);
	void SetScenePaused(Zenith_Scene xScene, bool bPaused);
	bool IsScenePaused(Zenith_Scene xScene);

	//==========================================================================
	// Name cache + build-index registry
	//==========================================================================

	void RegisterSceneBuildIndex(int iBuildIndex, const std::string& strPath);
	const std::string& GetRegisteredScenePath(int iBuildIndex);
	uint32_t GetBuildIndexRegistrySize();

	//==========================================================================
	// Scene creation + cross-scene main camera
	//==========================================================================

	// (There is no public "new named scene" entry point — to create a bare empty
	// scene use LoadScene(name, SCENE_LOAD_ADDITIVE_WITHOUT_LOADING).)

	// Leaf form: returns the main-camera EntityID found by scanning loaded
	// scenes (active scene first, then all loaded scenes in slot order), or
	// INVALID_ENTITY_ID if none. The ECS core deliberately does NOT resolve this
	// to the concrete Zenith_CameraComponent -- the engine-side resolver
	// Zenith_GetMainCameraAcrossScenes() (EntityComponent/Zenith_CameraResolve.h)
	// does that, with the same scan order + null semantics as before.
	Zenith_EntityID FindMainCameraEntityAcrossScenes();

	// QueryAllScenes — run a Zenith_Query<Ts...> across every loaded scene. The
	// returned object iterates each loaded scene's per-scene Query<Ts...> in slot
	// order, mirroring the per-scene Query API (ForEach / Count / First / Any).
	// AllScenesQuery is defined in Internal/Zenith_AllScenesQuery.inl.
	template<typename... Ts>
	class AllScenesQuery;

	template<typename... Ts>
	AllScenesQuery<Ts...> QueryAllScenes();

	// QueryActiveScene — the active-scene counterpart to QueryAllScenes (and the
	// engine-level form of the GetActiveSceneData()->Query<Ts...>() preamble every
	// game hand-rolls). Mirrors the per-scene Query API (ForEach / Count / First /
	// Any); when there is no active scene it is a safe no-op (zero iterations /
	// Count 0 / First == INVALID_ENTITY_ID) rather than a crash, so it is usable at
	// boot before a scene loads. Defined in Internal/Zenith_AllScenesQuery.inl.
	template<typename... Ts>
	class ActiveSceneQuery;

	template<typename... Ts>
	ActiveSceneQuery<Ts...> QueryActiveScene();

	//==========================================================================
	// Load / Unload
	//==========================================================================

	Zenith_Scene LoadScene(const std::string& strPath, Zenith_SceneLoadMode eMode);
	Zenith_Scene LoadSceneByIndex(int iBuildIndex, Zenith_SceneLoadMode eMode);

	void UnloadScene(Zenith_Scene xScene);
	void UnloadSceneForced(Zenith_Scene xScene);
	bool HasPendingDestructions();
	void ResetAllRenderSystems();

	//==========================================================================
	// Update pipeline + lifecycle-deferral state
	//==========================================================================

	// Per-frame entry point. PUBLIC because the caller is Zenith_Core (a
	// namespace of free functions, via ZENITH_PROFILING_FUNCTION_WRAPPER in
	// Zenith_Core.cpp) — a namespace cannot be befriended, so this cannot be
	// privatised without breaking the main loop.
	void Update(float fDt);

	void SetMainLoopRunning(bool bRunning);

#ifdef ZENITH_TESTING
	void DispatchFullLifecycleInit();
#endif

	bool AreRenderTasksActive() const
	{
		return m_bRenderTasksActive.load(std::memory_order_acquire);
	}
	void SetRenderTasksActive(bool b) { m_bRenderTasksActive.store(b, std::memory_order_release); }

	// Scene-graph render-mutation epoch (Phase 2). NotifyRenderMutation() is called
	// immediately before any renderable Flux_ModelInstance is freed (Zenith_ModelComponent::
	// ClearModel + scene/entity teardown) and after one is created/procedurally mutated,
	// so a bump strictly precedes any stale-pointer read. GetRenderMutationEpoch() reads
	// the live value; Flux_RenderSceneSnapshot::IsCurrent compares against the epoch it
	// was built for.
	void NotifyRenderMutation() { m_uRenderMutationEpoch.fetch_add(1, std::memory_order_acq_rel); }
	uint64_t GetRenderMutationEpoch() const { return m_uRenderMutationEpoch.load(std::memory_order_acquire); }

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
	// Entity creation — the ONLY public entity-construction entry points.
	// CreateEntity runs the engine-installed default-components hook after the
	// slot is set up (the everyday creation API); CreateEntityBare never runs
	// the hook (loaders/prefabs add their own components). Overloads target a
	// scene handle or a SceneData*. CreateEntityBare has a single (handle) form,
	// kept public only because the out-of-lib SentinelECS link-proof exercises it.
	// LEAF INVARIANT: none of these name a concrete component type.
	//==========================================================================

	Zenith_Entity CreateEntity(Zenith_Scene xScene, const std::string& strName);
	Zenith_Entity CreateEntity(Zenith_SceneData* pxSceneData, const std::string& strName);

	Zenith_Entity CreateEntityBare(Zenith_Scene xScene, const std::string& strName);

	// Lifecycle-deferral guard accessor (engine bootstrap + Zenith_LifecycleDeferralGuard).
	bool& MutableLifecycleLoadingFlagForGuard();

private:
	// Zenith_Entity is the PUBLIC entity-lifecycle API (Destroy / DestroyImmediate /
	// DontDestroyOnLoad / MoveToScene). Its methods forward to the now-PRIVATE
	// instance ops below (Destroy/DestroyImmediate/MarkEntityPersistent/
	// MoveEntityToScene) via Zenith_SceneSystem::Get(), so it must reach them.
	friend class Zenith_Entity;
	// Zenith_SceneData reaches the PRIVATE handle-resolution + lifecycle-context
	// helpers (GetSceneDataByHandle / PushLifecycleContext / PopLifecycleContext)
	// through Zenith_SceneSystem::Get() from its TUs.
	friend class Zenith_SceneData;
	// Zenith_Engine::Initialise installs the runtime hooks (SetRuntimeHooks) and
	// drains the deferred initial load (DrainPendingLoadIfAny); both are PRIVATE.
	friend class Zenith_Engine;
	// RAII guards (declared below the class) mutate private lifecycle flags
	// directly in their ctor/dtor bodies.
	friend struct Zenith_PrefabInstantiationGuard;
	friend struct Zenith_SceneUpdateDeferralGuard;
	// Leaf-internal free-function forwarders (declared in
	// Internal/Zenith_RenderTaskState.h) reach the SceneSystem-owned entity store +
	// runtime hooks via Get() without naming the engine singleton. Befriend them so
	// they can call the now-private GetEntityStore() / GetRuntimeHooks() accessors.
	friend Zenith_EntityStore& Zenith_ECS_EntityStore();
	friend bool Zenith_ECS_IsMainThread();

	// Engine-only hook install (friend Zenith_Engine).
	void SetRuntimeHooks(const Zenith_ECSRuntimeHooks& xHooks) { m_xRuntimeHooks = xHooks; }
	// Leaf-safe runtime hooks (see Zenith_ECSRuntimeHooks.h). PRIVATE: the only
	// reader is the leaf forwarder Zenith_ECS_IsMainThread() (defined in
	// Internal/Zenith_SceneSystem_Lifecycle.cpp) via Get(); the engine installs
	// them through SetRuntimeHooks above. No external caller names this.
	const Zenith_ECSRuntimeHooks& GetRuntimeHooks() const { return m_xRuntimeHooks; }

	//==========================================================================
	// Internal-only members relocated here from the public surface. Every caller
	// is inside Zenith/ZenithECS/ (the class's own Internal/ TUs) or an existing
	// friend (Zenith_SceneData / Zenith_Engine) reaching them via Get(); no
	// external, non-friend caller names any of these (grep-verified).
	//==========================================================================

	// Hot-path entity-store accessor with NO null assert (the store is allocated
	// in the ctor before any subsystem touches entity slots). PRIVATE: the ECS
	// core reaches it through the leaf forwarder Zenith_ECS_EntityStore() via
	// Get(), and Zenith_Engine::EntityStore() (a friend) forwards to it; no
	// external, non-friend caller names it.
	Zenith_EntityStore& GetEntityStore() { return *m_pxEntityStore; }

	// Instance teardown (Get().Shutdown() from the bootstrap TU) + raw handle
	// allocation primitive (called only by AllocateEmptyScene).
	void Shutdown();
	int  AllocateSceneHandle();

	// Raw handle of the persistent ("DontDestroyOnLoad") scene, for internal
	// collaborators (e.g. Zenith_SceneData asserts, a friend) that only need to
	// compare handles.
	int GetPersistentSceneHandle() const { return m_iPersistentSceneHandle; }

	// Per-scene visibility predicate (used by the slot-scan in Registry.cpp).
	bool IsSceneVisibleToUser(u_int uSlotIndex, const Zenith_SceneData* pxData);

	// Name-cache maintenance (driven by AllocateEmptyScene / FreeSceneHandle).
	void AddToSceneNameCache(int iHandle, const std::string& strName);
	void RemoveFromSceneNameCache(int iHandle);

	// Build-index registry reset + path canonicalisation helper (LoadScene paths).
	void ClearBuildIndexRegistry();
	static std::string CanonicalisePath(const std::string& strPath);

	// Lifecycle-deferral state readers (Zenith_SceneData, a friend, reads these
	// via Get() to gate deferred work).
	bool IsLoadingScene() const        { return m_bIsLoadingScene; }
	bool IsPrefabInstantiating() const { return m_bIsPrefabInstantiating; }

	// Unused-asset unload + can-unload predicate + bulk non-persistent teardown
	// and its step helpers -- internal to the unload/SINGLE-load paths.
	void UnloadUnusedAssets();
	bool CanUnloadScene(Zenith_Scene xScene);
	void UnloadAllNonPersistent(int iExcludeHandle = -1);
	Zenith_Vector<Zenith_Scene> CollectNonPersistentScenes(int iExcludeHandle);
	bool DestroyScenes(const Zenith_Vector<Zenith_Scene>& axScenes);
	void UpdateActiveSceneAfterUnload();

	//==========================================================================
	// Composite unload helper: unloads a scene's slot and selects a new active
	// scene if the unloaded one was active. Internal callers only.
	//
	// NOTE: the former scene-lifecycle event-dispatch Fire* methods
	// (FireActiveSceneChanged / FireSceneLoaded / FireSceneUnloading /
	// FireSceneUnloaded) were removed — the Zenith_Event_Scene* events they
	// Dispatched had zero subscribers. The dispatcher (Zenith_EventDispatcher)
	// remains for game-defined events.
	//==========================================================================
	void FireUnloadCallbacksAndSelectNewActive(int iHandle, Zenith_Scene xScene);

	// Slot-table walk primitives. PRIVATE: the only caller is the nested
	// AllScenesQuery<Ts...> (Internal/Zenith_AllScenesQuery.inl), which iterates
	// loaded scenes in slot order. External multi-scene gather goes through
	// QueryAllScenes<Ts...> / QueryActiveScene<Ts...> instead of a raw slot loop;
	// the unfiltered GetSceneDataAtSlot was removed (zero callers).
	uint32_t          GetSceneSlotCount();
	Zenith_SceneData* GetLoadedSceneDataAtSlot(uint32_t uIndex);

	// Engine-internal scene resolution + lifecycle-context + active-selection +
	// per-scene-update predicate + handle release + bulk-unload step. Internal
	// callers only (same class / Zenith_Entity / Zenith_SceneData friends).
	void              FreeSceneHandle(int iHandle);
	Zenith_SceneData* GetSceneDataByHandle(int iHandle);
	Zenith_Scene      GetSceneFromHandle(int iHandle);
	int               SelectNewActiveScene(int iExcludeHandle = -1);
	bool              IsSceneUpdatable(const Zenith_SceneData* pxData);
	void              UnloadOneScene(Zenith_Scene xScene, bool& bActiveSceneUnloadedInOut);

	void PushLifecycleContext(const std::string& strCanonicalPath);
	void PopLifecycleContext(const std::string& strCanonicalPath);
	bool IsCircularLoadDependency(const std::string& strCanonicalPath);

	// Engine-only: drained by Zenith_Engine::Initialise + internal update paths.
	void DrainPendingLoadIfAny();

	//==========================================================================
	// Cross-scene entity ownership. These are
	// the implementation bodies behind the PUBLIC Zenith_Entity lifecycle API
	// (Destroy / DestroyImmediate / DontDestroyOnLoad / MoveToScene). They are
	// PRIVATE instance methods — Zenith_Entity (a friend) calls them through
	// Zenith_SceneSystem::Get(). Bodies live in
	// Internal/Zenith_SceneSystem_EntityOwnership.cpp.
	//==========================================================================

	bool MoveEntityToScene(Zenith_Entity& xEntity, Zenith_Scene xTarget);
	void MarkEntityPersistent(Zenith_Entity& xEntity);
	void Destroy(Zenith_Entity& xEntity);
	void Destroy(Zenith_Entity& xEntity, float fDelay);
	void DestroyImmediate(Zenith_Entity& xEntity);

	// Raw empty-scene allocation primitive (was the public CreateEmptyScene).
	// PRIVATE: allocates a handle + Zenith_SceneData, wires up the slot / name
	// cache, flips it to LOADED, and optionally auto-activates when there is no
	// active scene yet. Used by the LoadScene paths
	// (SINGLE / ADDITIVE / ADDITIVE_WITHOUT_LOADING) and the bootstrap. External
	// callers that want a bare empty scene use
	// LoadScene(name, SCENE_LOAD_ADDITIVE_WITHOUT_LOADING).
	Zenith_Scene AllocateEmptyScene(const std::string& strName, bool bAllowSetActive = true);

	// Shared allocation helper for the CreateEntity / CreateEntityBare overloads:
	// allocates a slot in pxSceneData, sets name/enabled/transient, registers the
	// new entity for deferred lifecycle, and dispatches the immediate runtime
	// Awake/OnEnable. When bRunDefaultComponentsHook is true and the engine has
	// installed m_pfnAddDefaultComponents, the hook runs (adds the engine's default
	// component(s)) AFTER the slot state is set and BEFORE the lifecycle dispatch —
	// matching the old creating ctor ordering exactly. Returns an invalid handle on failure.
	Zenith_Entity CreateEntityInScene(Zenith_SceneData* pxSceneData, const std::string& strName, bool bRunDefaultComponentsHook);

	bool MoveEntityInternal(Zenith_Entity& xEntity, Zenith_SceneData* pxTargetData);

	//==========================================================================
	// Nested types -- PRIVATE: SceneNameEntry only backs the private
	// m_axLoadedSceneNames cache; no external code names it.
	//==========================================================================

	struct SceneNameEntry
	{
		std::string m_strName;
		int m_iHandle;
	};

	//==========================================================================
	// Data members — engine-internal state. Private; the implementation TUs all
	// define members of this one class, so they reach these directly. External
	// code goes through the public accessors (e.g. MutableLifecycleLoadingFlagForGuard).
	//==========================================================================

	// Owned process-wide entity store. Heap-allocated in the ctor, freed in the
	// dtor. Reached via GetEntityStore().
	Zenith_EntityStore*                  m_pxEntityStore = nullptr;

	// The single live Zenith_SceneSystem, published in the ctor and cleared in
	// the dtor. Backs the static Get() accessor. Defined in
	// Internal/Zenith_SceneSystem_Registry.cpp.
	static Zenith_SceneSystem*           s_pxInstance;

	Zenith_Vector<Zenith_SceneData*>     m_axScenes;
	Zenith_Vector<uint32_t>              m_axSceneGenerations;
	Zenith_Vector<int>                   m_axFreeHandles;
	int                                  m_iActiveSceneHandle     = -1;
	int                                  m_iPersistentSceneHandle = -1;
	uint64_t                             m_ulNextLoadTimestamp    = 1;
	Zenith_Vector<SceneNameEntry>        m_axLoadedSceneNames;
	Zenith_Vector<std::string>           m_axBuildIndexToPath;

	//==========================================================================
	// Lifecycle / update-pipeline state
	//==========================================================================

	bool                          m_bIsLoadingScene          = false;
	bool                          m_bIsPrefabInstantiating   = false;
	bool                          m_bIsUpdating              = false;

	float                         m_fFixedTimeAccumulator    = 0.0f;
	float                         m_fFixedTimestep           = 0.02f;  // 50Hz default

	Zenith_Vector<std::string>    m_axCurrentlyLoadingPaths;
	Zenith_Vector<std::string>    m_axLifecycleLoadStack;

	int                           m_iPendingBuildIndex       = -1;

	bool                          m_bIsMainLoopRunning       = false;

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
	// builds) so AreRenderTasksActive() returns an authoritative, race-free
	// value in every build. Set true around ExecuteRenderGraph (see
	// Zenith_Core.cpp); the scene-mutation asserts read it to permit concurrent
	// reads during that window.
	std::atomic<bool>             m_bRenderTasksActive { false };

	// WS10 sparse-set query read path. Default true = sparse fast path shipped on.
	// Plain std::atomic (NOT #ifdef-d) so it compiles + works in *_False configs.
	std::atomic<bool>             m_bUseSparseQueryReads { true };

	// Scene-graph render-mutation epoch (Phase 2). Bumped by NotifyRenderMutation()
	// whenever a renderable Flux_ModelInstance is created or destroyed (incl. scene
	// load/unload + entity destroy). The Flux_RenderSceneSnapshot stamps the epoch it
	// was built for; the tools panel compares against the live value to detect a stale
	// snapshot before dereferencing a possibly-freed instance. Starts at 1 so a
	// never-built snapshot (epoch 0) is never "current". Atomic because frees can be
	// queued from teardown paths; mirrors the other atomics here.
	std::atomic<uint64_t>         m_uRenderMutationEpoch { 1 };

	// Leaf-safe runtime hooks installed by the engine bootstrap (all nullptr
	// until SetRuntimeHooks is called; the documented null-semantics make every
	// hook a safe no-op before installation). See Zenith_ECSRuntimeHooks.h.
	Zenith_ECSRuntimeHooks        m_xRuntimeHooks;
};

//==========================================================================
// RAII scope guards
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

// Pull in the complete Zenith_SceneData (for GetLoadedSceneDataAtSlot's result)
// AND Zenith_Query: the AllScenesQuery member-template bodies below name
// pxData->Query<Ts...>(), whose returned Zenith_Query<Ts...> must be COMPLETE
// where AllScenesQuery is instantiated. Providing Query.h HERE means every
// QueryAllScenes caller gets it transitively via SceneSystem.h -- no caller
// (incl. the Flux gather-callers) needs an explicit Zenith_Query.h include, which
// also keeps the EC<->Flux include ratchet from gaining new cross-layer edges.
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_Query.h"

// AllScenesQuery<Ts...> member-template bodies + QueryAllScenes — must come after
// Zenith_SceneData.h + Zenith_Query.h are complete (the bodies name
// pxData->Query<Ts...>()). Kept out of the header body so the scene API reads as
// a method list.
#include "ZenithECS/Internal/Zenith_AllScenesQuery.inl"
