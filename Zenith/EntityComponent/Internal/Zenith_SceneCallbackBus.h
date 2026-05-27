#pragma once

// Zenith_SceneCallbackBus — owns the six scene-event callback lists, the
// handle allocator, the deferred-removal queue, the dispatch-depth counter,
// and the active-scene-changed suppression flags.
//
// Public surface = Register/Unregister/Fire forwarders + suppression
// accessors + shutdown + test introspection. Template machinery
// (Zenith_CallbackList<T>, CallbackEntry<T>, AllocateCallbackHandle,
// ProcessPendingCallbackRemovals) is bus-private (anonymous namespace in
// the .cpp) and not exposed via this header.
//
// Game and editor code never includes this header — they continue to use
// Zenith_SceneManager::Register*/Unregister*Callback. Those public methods
// become two-line forwarders to this bus.

#include "Collections/Zenith_Vector.h"
#include "EntityComponent/Zenith_SceneManager.h"

class Zenith_SceneCallbackBus
{
public:
	using CallbackHandle = Zenith_SceneCallbackHandle;
	static constexpr CallbackHandle INVALID_CALLBACK_HANDLE = Zenith_INVALID_SCENE_CALLBACK_HANDLE;

	//==========================================================================
	// Lifecycle
	//==========================================================================

	// Clear all callback lists, suppression state, deferred-removal queue and
	// reset the dispatch-depth counter. Called from Zenith_SceneManager::Shutdown().
	void Shutdown();

	//==========================================================================
	// Register / Unregister
	//==========================================================================

	CallbackHandle RegisterActiveSceneChanged(Zenith_SceneChangedCallback pfn);
	bool UnregisterActiveSceneChanged(CallbackHandle ulHandle);

	CallbackHandle RegisterSceneLoaded(Zenith_SceneLoadedCallback pfn);
	bool UnregisterSceneLoaded(CallbackHandle ulHandle);

	CallbackHandle RegisterSceneUnloading(Zenith_SceneUnloadingCallback pfn);
	bool UnregisterSceneUnloading(CallbackHandle ulHandle);

	CallbackHandle RegisterSceneUnloaded(Zenith_SceneUnloadedCallback pfn);
	bool UnregisterSceneUnloaded(CallbackHandle ulHandle);

	CallbackHandle RegisterSceneLoadStarted(Zenith_SceneLoadStartedCallback pfn);
	bool UnregisterSceneLoadStarted(CallbackHandle ulHandle);

	CallbackHandle RegisterEntityPersistent(Zenith_EntityPersistentCallback pfn);
	bool UnregisterEntityPersistent(CallbackHandle ulHandle);

	//==========================================================================
	// Composite unload-dispatch helper (Phase 5e)
	//==========================================================================

	// Fires SceneUnloading, frees the scene data slot, fires SceneUnloaded,
	// and (if the unloaded scene was active) selects + announces a new active
	// scene. Migrated from Zenith_SceneManager during Phase 5e.
	void FireUnloadCallbacksAndSelectNewActive(int iHandle, Zenith_Scene xScene);

	//==========================================================================
	// Fire
	//==========================================================================

	void FireActiveSceneChanged(Zenith_Scene xOld, Zenith_Scene xNew);
	void FireSceneLoaded(Zenith_Scene xScene, Zenith_SceneLoadMode eMode);
	void FireSceneUnloading(Zenith_Scene xScene);
	void FireSceneUnloaded(Zenith_Scene xScene);
	void FireSceneLoadStarted(const std::string& strPath);
	void FireEntityPersistent(const Zenith_Entity& xEntity);

	//==========================================================================
	// Active-scene suppression scope state (read accessors).
	//
	// FireUnloadCallbacksAndSelectNewActive (manager) and the unload path inside
	// UnloadAllNonPersistent observe whether suppression is active and capture the
	// first-observed old active into the deferred slot. Those sites read these
	// accessors and call SetDeferredOldActive when the suppression window is open.
	//
	// SetActiveSceneSuppressed is package-private to ActiveSceneChangeSuppressionScope
	// (see below) — only the scope's ctor/dtor flips the flag. This guarantees there
	// is no scattered begin/end pair to leak.
	//==========================================================================

	bool IsActiveSceneSuppressed();
	bool HasDeferredOldActive();
	Zenith_Scene GetDeferredOldActive();
	void SetDeferredOldActive(Zenith_Scene xScene);

private:
	friend class ActiveSceneChangeSuppressionScope;
	void SetActiveSceneSuppressed(bool b);
	void ClearDeferredOldActive();
public:

#ifdef ZENITH_TESTING
	//==========================================================================
	// Test introspection — read-only accessors for tests that previously
	// reached into the now-private statics.
	//==========================================================================

	u_int GetActiveSceneChangedCallbackCount();
	u_int GetSceneLoadedCallbackCount();
	u_int GetSceneUnloadingCallbackCount();
	u_int GetSceneUnloadedCallbackCount();
	u_int GetSceneLoadStartedCallbackCount();
	u_int GetEntityPersistentCallbackCount();
	u_int GetCallbackDispatchDepth();
	u_int GetPendingRemovalCount();

	// Lets TestCallbackHandleWrapNoCollision seed the handle counter to UINT64_MAX
	// to force the wrap-around branch deterministically.
	void SetNextCallbackHandleForTest(CallbackHandle ulValue);

	// Lets TestShutdownClearsAllStatics seed suppression state to verify
	// Shutdown forcibly clears even corrupt flag combinations.
	void SetActiveSceneSuppressedForTest(bool b);
#endif

	//==========================================================================
	// Data members (was Zenith_SceneCallbackBus)
	//==========================================================================

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

	// Six callback lists — one per event type.
	CallbackList<Zenith_SceneChangedCallback>      m_xActiveSceneChangedCallbacks;
	CallbackList<Zenith_SceneLoadedCallback>       m_xSceneLoadedCallbacks;
	CallbackList<Zenith_SceneUnloadingCallback>    m_xSceneUnloadingCallbacks;
	CallbackList<Zenith_SceneUnloadedCallback>     m_xSceneUnloadedCallbacks;
	CallbackList<Zenith_SceneLoadStartedCallback>  m_xSceneLoadStartedCallbacks;
	CallbackList<Zenith_EntityPersistentCallback>  m_xEntityPersistentCallbacks;

	// Handle allocator & deferred-removal machinery.
	CallbackHandle                m_ulNextCallbackHandle = 1;
	Zenith_Vector<CallbackHandle> m_axCallbacksPendingRemoval;
	uint32_t                      m_uFiringCallbacksDepth = 0;

	// Active-scene-changed suppression state.
	bool         m_bSuppressActiveSceneChanged = false;
	bool         m_bHaveDeferredOldActive      = false;
	Zenith_Scene m_xDeferredOldActive;
};

//==========================================================================
// ActiveSceneChangeSuppressionScope — RAII over the suppression window.
//
// Construct around a SINGLE-mode teardown to suppress intermediate
// ActiveSceneChanged dispatches; subscribers observe a single old → new
// transition at Complete (or no transition at Cancel).
//
// Destructor REQUIRES that exactly one of Complete or Cancel was called.
// Forgetting either fires an assert — the safety the manual Begin/End
// pattern lacked.
//
// "Old active" resolution at Complete:
//   - Constructor argument if it was a valid scene handle.
//   - Otherwise, falls back to whatever FireUnloadCallbacksAndSelectNewActive
//     captured into the bus's deferred slot during the suppression window
//     (relevant when the caller didn't have a snapshot, e.g. first-ever load).
//   - Otherwise INVALID_SCENE — Complete fires nothing if old equals new.
//
// Two call patterns:
//   Sync teardown (PerformSingleModeTeardownAndSwap):
//       ActiveSceneChangeSuppressionScope xScope(xOldActive);
//       /* teardown */
//       xScope.Complete(xNewActive);
//
//   Async Phase 1 (RunAsyncJobPhase1) — Phase 2 fires the callback later
//   from a per-job snapshot, possibly across frames if activation pauses:
//       ActiveSceneChangeSuppressionScope xScope(xOldActive);
//       /* teardown */
//       xScope.Cancel();
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
