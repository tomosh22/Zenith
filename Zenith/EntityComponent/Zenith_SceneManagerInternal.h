// =============================================================================
// Zenith_SceneManagerInternal.h
// -----------------------------------------------------------------------------
// Engine-internal section of Zenith_SceneManager. This header is meant to be
// included *inside* the body of `class Zenith_SceneManager` from
// Zenith_SceneManager.h — it has no #pragma once because the declarations
// below are intentionally part of that class so existing call sites resolve
// unchanged.
//
// What lives here:
//   - The "Internal (Engine Use Only)" public API (Initialise / Shutdown /
//     Update / GetSceneData / lifecycle-state read accessors / etc.)
//   - The full private implementation block (helpers, callback Fire*,
//     unload helpers, ZENITH_ASSERT-only render-task-active tracking)
//
// What stays in Zenith_SceneManager.h:
//   - The public game-facing API (Load/Unload/Get*/MoveEntityToScene/...).
//   - Forward declarations and the navigation guide.
//
// Newcomers reading Zenith_SceneManager.h should not need to read this file.
// Engine-internal callers (Zenith_Core, the editor, Internal/ subsystems,
// Prefab) get full visibility transparently because Zenith_SceneManager.h
// includes this header inside the class body.
// =============================================================================

	//==========================================================================
	// Internal (Engine Use Only)
	//==========================================================================

	static void Initialise();
	static void Shutdown();
	static void NotifyAsyncJobPriorityChanged();  // forwarder to Zenith_SceneOperationQueue
	static void Update(float fDt);
	static void WaitForUpdateComplete();

	/**
	 * Access internal scene data (for engine systems)
	 *
	 * GetSceneData(Zenith_Scene) resolves a scene HANDLE to its backing SceneData. Do not
	 * pair it with an EntityID to route a per-entity query — that is the active-scene-as-filter
	 * anti-pattern described on GetActiveScene(). Use GetSceneDataForEntity(EntityID) when you
	 * have an EntityID and want the owning scene; it walks the global entity slot table and
	 * is immune to cross-scene moves.
	 */
	static Zenith_SceneData* GetSceneData(Zenith_Scene xScene);
	static Zenith_SceneData* GetSceneDataByHandle(int iHandle);
	/**
	 * Resolve an EntityID to its owning scene's data. Prefer this over
	 * GetSceneData(GetActiveScene()) for any EntityID-indexed lookup. Returns nullptr for
	 * invalid / stale IDs (generation check) or when the owning scene has been unloaded.
	 */
	static Zenith_SceneData* GetSceneDataForEntity(Zenith_EntityID xID);

	/**
	 * Build a full scene handle (with generation) from an internal handle index
	 */
	static Zenith_Scene GetSceneFromHandle(int iHandle);

	/**
	 * Check if scene is loading (for asset management)
	 */
	static bool IsLoadingScene();

	/**
	 * True while a PrefabInstantiationGuard is on the stack (post-A6.3 the
	 * guard is the only writer of Zenith_SceneLifecycleScheduler::
	 * s_bIsPrefabInstantiating). Read accessor backing
	 * Zenith_SceneLifecycleContext::IsPrefabInstantiating(); subsystems must
	 * read through the context, not this method directly.
	 */
	static bool IsPrefabInstantiating();

	/**
	 * True during Zenith_SceneManager::Update(). Read accessor backing
	 * Zenith_SceneLifecycleContext::IsUpdating(); subsystems must read through
	 * the context, not this method directly.
	 */
	static bool IsUpdating();

	/**
	 * True between the start of a SCENE_LOAD_SINGLE teardown and the consolidated
	 * old->new ActiveSceneChanged dispatch. Read accessor backing
	 * Zenith_SceneLifecycleContext::IsActiveSceneSuppressed().
	 */
	static bool IsActiveSceneSuppressed();

	/**
	 * Pending build-index parked by LoadSceneByIndex / LoadSceneAsyncByIndex.
	 * -1 when no index is currently being staged. Read accessor backing
	 * Zenith_SceneLifecycleContext::GetPendingBuildIndex().
	 */
	static int GetPendingBuildIndex();

	/**
	 * True when the canonical path is already being loaded (file-I/O stack or
	 * lifecycle dispatch stack). Public accessor backing
	 * Zenith_SceneLifecycleContext::IsCircularLoadDependency(); forwards to the
	 * private CheckCircularLoadDependency helper.
	 */
	static bool IsCircularLoadDependency(const std::string& strCanonicalPath);

	// Nested RAII scope-guard types (LifecycleDeferralGuard,
	// PrefabInstantiationGuard, SceneUpdateDeferralGuard,
	// SceneCreationTargetScope). Definitions in a sibling header so this file
	// stays focused on the public API; existing call sites that name them as
	// `Zenith_SceneManager::PrefabInstantiationGuard` continue to compile
	// unchanged because the types remain nested members of this class.
	#include "EntityComponent/Zenith_SceneManagerGuards.h"

	/**
	 * Set by Zenith_Core::Zenith_MainLoop driver — true just before the main
	 * loop's while(...) starts iterating, false once it exits. Read by
	 * LoadSceneBlockingForBootstrap to assert bootstrap-only invocation.
	 * Production code other than the engine bootstrap should never call this.
	 */
	static void SetMainLoopRunning(bool bRunning);

	/**
	 * Returns the scene that GetDefaultCreationScene-aware APIs should target.
	 * Top of the SceneCreationTargetScope stack if any scope is active;
	 * otherwise the active scene. May return INVALID_SCENE when no creation
	 * scope is open and no scene is active (e.g. during Initialise before the
	 * persistent scene is created).
	 */
	static Zenith_Scene GetDefaultCreationScene();

#ifdef ZENITH_TESTING
	/**
	 * Dispatch lifecycle init for all loaded scenes. Test-only — used by unit
	 * tests that build a scene via deferred entity creation and need to drive
	 * the Awake/OnEnable phase explicitly (mirrors what the per-frame Update
	 * does after a load completes). Not callable from production code.
	 */
	static void DispatchFullLifecycleInit();
#endif

	/**
	 * Register the initial scene load callback (called by platform main to set Project_LoadInitialScene)
	 * Used by the editor to re-run initial scene setup after Play/Stop cycle
	 */
	typedef void(*InitialSceneLoadFn)();
	static void SetInitialSceneLoadCallback(InitialSceneLoadFn pfnCallback);
	static InitialSceneLoadFn GetInitialSceneLoadCallback();

	/**
	 * Reset all Flux render systems (called before scene teardown)
	 * Must be called before UnloadScene/UnloadAllNonPersistent when doing
	 * a full scene swap, to clear Flux system state before entity destruction.
	 */
	static void ResetAllRenderSystems();

	/**
	 * Editor / test recovery only. Unloads a scene bypassing the "last loaded
	 * scene" guard that normally prevents the manager from being left with no
	 * active scene. Used by:
	 *   * Editor backup restore (Play -> Stop), where the new scene is loaded
	 *     immediately after this call so the no-active-scene window is brief.
	 *   * Test fixture teardown that needs a clean slate even when only one
	 *     scene remains.
	 * Production gameplay code MUST use UnloadScene / UnloadSceneAsync, which
	 * preserves the at-least-one-scene invariant the lifecycle systems rely on.
	 */
	static void UnloadSceneForced(Zenith_Scene xScene);

private:
	//==========================================================================
	// Internal Helpers
	//==========================================================================

	// A5: MoveEntityInternal moved to Zenith_SceneEntityOwnership (private).
	static bool IsSceneVisibleToUser(u_int uSlotIndex, const Zenith_SceneData* pxData);
	static bool IsSceneUpdatable(const Zenith_SceneData* pxData);
	static int SelectNewActiveScene(int iExcludeHandle = -1);
	static int AllocateSceneHandle();
	static void FreeSceneHandle(int iHandle);

	// Phase-2 destruction body for one scene during a non-persistent unload
	// (UnloadAllNonPersistent). Deletes the SceneData, fires SceneUnloaded
	// callback, and releases the handle (which increments the generation
	// counter). bActiveSceneUnloadedInOut is OR'd with whether this scene
	// was the active one, so the caller can detect after the loop whether
	// the active scene needs the post-loop fallback orchestration. The
	// fallback (clearing s_iActiveSceneHandle, firing ActiveSceneChanged)
	// stays in the parent because it depends on cumulative state across
	// all scenes, not just this one.
	static void UnloadOneScene(Zenith_Scene xScene, bool& bActiveSceneUnloadedInOut);

	// Per-frame helpers called from Update(). Split out to flatten Update()'s
	// nesting — the animation collection walk was five levels deep inline.
	// A4: CollectUpdatableScenes / CollectAnimationsFromScene moved to
	// Zenith_SceneLifecycleScheduler.cpp's anonymous namespace.

	/**
	 * Create and return an invalid scene handle (handle=-1, generation=0).
	 * Used by query and load functions to return a sentinel when a scene cannot be found or loaded.
	 */
	static Zenith_Scene MakeInvalidScene();

	/**
	 * Check if the given canonical path is already in the pending load list or lifecycle stack.
	 * Used by LoadScene and LoadSceneAsync to prevent circular scene loads.
	 * @return true if the path is already being loaded (circular dependency detected)
	 */
	// A4: CheckCircularLoadDependency body migrated to
	// Zenith_SceneLifecycleScheduler::IsCircularLoadDependency.

	/**
	 * Fire unloading/unloaded callbacks and select a new active scene if needed.
	 * Used by UnloadSceneInternal to consolidate the callback+active-scene-selection logic.
	 * @param iHandle The scene handle being unloaded
	 * @param xScene The scene being unloaded (with valid generation for callbacks)
	 */
	static void FireUnloadCallbacksAndSelectNewActive(int iHandle, Zenith_Scene xScene);

	//==========================================================================
	// LoadScene helpers. Pre-B4 LoadScene was a multi-phase synchronous body
	// using HandleDeferredLoad / ValidateFileAndDetectCircular /
	// PerformSingleModeTeardownAndSwap / DispatchLifecycleAndFire as Phase
	// helpers. B4 collapsed LoadScene into a queue-and-defer thin facade —
	// the multi-phase work now lives in
	// Zenith_SceneOperationQueue::RunAsyncJobPhase1/2. ValidateLoadRequest
	// stayed as the only top-of-LoadScene precondition check.
	//==========================================================================

	static bool ValidateLoadRequest(const std::string& strPath);

	//==========================================================================
	// Scene storage / name cache / build-index registry — moved to
	// Zenith_SceneRegistry post-A2b. Manager-internal lifecycle code references
	// Zenith_SceneRegistry::s_* directly. The two helpers below stay as public
	// forwarders for one-line historical call sites in CreateEmptyScene etc.
	//==========================================================================

	static void AddToSceneNameCache(int iHandle, const std::string& strName);
	static void RemoveFromSceneNameCache(int iHandle);

	// Phase 5b: g_xEngine.SceneLifecycle().m_bRenderTasksActive / g_xEngine.SceneLifecycle().m_bAnimTasksActive flags + their
	// setters / accessor moved to Zenith_SceneLifecycleScheduler. Callers
	// route through g_xEngine.SceneLifecycle().AreRenderTasksActive() /
	// SetRenderTasksActive() / SetAnimTasksActive(). The global free
	// function Zenith_AreRenderTasksActive() (declared in
	// Zenith_RenderTaskState.h) remains as a header-cycle-break for
	// template bodies in Zenith_SceneData.h that can't pull in the engine
	// header directly.
private:

	//==========================================================================
	// Lifecycle / Update State — moved to Zenith_SceneLifecycleScheduler post-A4.
	// Public manager methods forward into the scheduler.
	//==========================================================================

	// Push/Pop lifecycle context — Zenith_SceneData.cpp calls these directly via
	// the manager's public API. Body forwards to the scheduler.
	static void PushLifecycleContext(const std::string& strCanonicalPath);
	static void PopLifecycleContext(const std::string& strCanonicalPath);

	//==========================================================================
	// Callback System
	//
	// Lists, handle allocator, deferred-removal queue, dispatch-depth counter
	// and active-scene-changed suppression flags now live in
	// Zenith_SceneCallbackBus (Internal/Zenith_SceneCallbackBus.{h,cpp}).
	// The Fire* methods below remain as thin forwarders so internal call
	// sites in this header's TU and sibling SceneManager TUs don't need to
	// change.
	//==========================================================================

	static void FireSceneLoadedCallbacks(Zenith_Scene xScene, Zenith_SceneLoadMode eMode);
	static void FireSceneUnloadingCallbacks(Zenith_Scene xScene);
	static void FireSceneUnloadedCallbacks(Zenith_Scene xScene);
	static void FireActiveSceneChangedCallbacks(Zenith_Scene xOld, Zenith_Scene xNew);
	static void FireSceneLoadStartedCallbacks(const std::string& strPath);
	static void FireEntityPersistentCallbacks(const Zenith_Entity& xEntity);

	//==========================================================================
	// Async Operations — moved to Zenith_SceneOperationQueue post-A3.
	//
	// State (operation map, async load + unload jobs, async config knobs) and
	// internal pipeline (phase machines, cancellation, cleanup, sort) live in
	// Internal/Zenith_SceneOperationQueue.{h,cpp}. The public manager methods
	// below are forwarders or keep their bodies on the manager but reference
	// Zenith_SceneOperationQueue::s_*.
	//==========================================================================

	static uint32_t CountScenesBeingAsyncUnloaded();

	//==========================================================================
	// Unload Helpers
	//==========================================================================

	static bool CanUnloadScene(Zenith_Scene xScene);
	static void UnloadSceneInternal(Zenith_Scene xScene);
	static void ProcessPendingUnloads();
	// iExcludeHandle: optional scene-slot index to skip (in addition to the persistent
	// scene). D.12 uses this to keep the staging scene alive while the old world is
	// torn down during an atomic-swap sync LoadScene(SINGLE). Pass -1 (the default)
	// for the original "unload every non-persistent scene" behaviour.
	static void UnloadAllNonPersistent(int iExcludeHandle = -1);

	// UnloadAllNonPersistent helpers — extracted to keep the orchestrator small
	// and to give the threading/dedup invariants explicit names. All four are
	// main-thread-only by construction (UnloadAllNonPersistent asserts at top).

	// Cancel + complete every in-flight async unload job, marking each as
	// SUCCEEDED (the sync path completes the unload via a different code path).
	// Records into xAlreadyFiredOut the handles whose SceneUnloading callback
	// fired during the async job — Phase-1 in DestroyScenesAndFireUnloaded
	// must skip those to honour Unity's exactly-one Unloading guarantee.
	static void CompleteAsyncUnloadJobs(Zenith_HashSet<int>& xAlreadyFiredOut);

	// Walk Zenith_SceneRegistry for loaded non-persistent scenes that aren't
	// the explicit excluded handle. Returns scene handles + generations so
	// callbacks see correct generation counters even after destruction.
	static Zenith_Vector<Zenith_Scene> CollectNonPersistentScenes(int iExcludeHandle);

	// Two-phase: fire SceneUnloading for every scene not in xAlreadyFired,
	// then destroy each scene + fire SceneUnloaded. Returns true if the
	// active scene was among the destroyed set (caller dispatches the
	// post-loop active-scene fallback).
	static bool DestroyScenesAndFireUnloaded(const Zenith_Vector<Zenith_Scene>& axScenes,
		const Zenith_HashSet<int>& xAlreadyFired);

	// Post-destroy active-scene bookkeeping: clear the active handle and fire
	// ActiveSceneChanged (or defer it via the suppression flag during a
	// SINGLE-load teardown). Caller only invokes this if DestroyScenesAndFireUnloaded
	// returned true.
	static void UpdateActiveSceneAfterUnload(Zenith_Scene xOldActive);
