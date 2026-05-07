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

#include "EntityComponent/Zenith_SceneManager.h"

class Zenith_SceneCallbackBus
{
public:
	using CallbackHandle = Zenith_SceneManager::CallbackHandle;
	static constexpr CallbackHandle INVALID_CALLBACK_HANDLE = Zenith_SceneManager::INVALID_CALLBACK_HANDLE;

	//==========================================================================
	// Lifecycle
	//==========================================================================

	// Clear all callback lists, suppression state, deferred-removal queue and
	// reset the dispatch-depth counter. Called from Zenith_SceneManager::Shutdown().
	static void Shutdown();

	//==========================================================================
	// Register / Unregister
	//==========================================================================

	static CallbackHandle RegisterActiveSceneChanged(Zenith_SceneManager::SceneChangedCallback pfn);
	static bool UnregisterActiveSceneChanged(CallbackHandle ulHandle);

	static CallbackHandle RegisterSceneLoaded(Zenith_SceneManager::SceneLoadedCallback pfn);
	static bool UnregisterSceneLoaded(CallbackHandle ulHandle);

	static CallbackHandle RegisterSceneUnloading(Zenith_SceneManager::SceneUnloadingCallback pfn);
	static bool UnregisterSceneUnloading(CallbackHandle ulHandle);

	static CallbackHandle RegisterSceneUnloaded(Zenith_SceneManager::SceneUnloadedCallback pfn);
	static bool UnregisterSceneUnloaded(CallbackHandle ulHandle);

	static CallbackHandle RegisterSceneLoadStarted(Zenith_SceneManager::SceneLoadStartedCallback pfn);
	static bool UnregisterSceneLoadStarted(CallbackHandle ulHandle);

	static CallbackHandle RegisterEntityPersistent(Zenith_SceneManager::EntityPersistentCallback pfn);
	static bool UnregisterEntityPersistent(CallbackHandle ulHandle);

	//==========================================================================
	// Fire
	//==========================================================================

	static void FireActiveSceneChanged(Zenith_Scene xOld, Zenith_Scene xNew);
	static void FireSceneLoaded(Zenith_Scene xScene, Zenith_SceneLoadMode eMode);
	static void FireSceneUnloading(Zenith_Scene xScene);
	static void FireSceneUnloaded(Zenith_Scene xScene);
	static void FireSceneLoadStarted(const std::string& strPath);
	static void FireEntityPersistent(const Zenith_Entity& xEntity);

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

	static bool IsActiveSceneSuppressed();
	static bool HasDeferredOldActive();
	static Zenith_Scene GetDeferredOldActive();
	static void SetDeferredOldActive(Zenith_Scene xScene);

private:
	friend class ActiveSceneChangeSuppressionScope;
	static void SetActiveSceneSuppressed(bool b);
	static void ClearDeferredOldActive();
public:

#ifdef ZENITH_TESTING
	//==========================================================================
	// Test introspection — read-only accessors for tests that previously
	// reached into the now-private statics.
	//==========================================================================

	static u_int GetActiveSceneChangedCallbackCount();
	static u_int GetSceneLoadedCallbackCount();
	static u_int GetSceneUnloadingCallbackCount();
	static u_int GetSceneUnloadedCallbackCount();
	static u_int GetSceneLoadStartedCallbackCount();
	static u_int GetEntityPersistentCallbackCount();
	static u_int GetCallbackDispatchDepth();
	static u_int GetPendingRemovalCount();

	// Lets TestCallbackHandleWrapNoCollision seed the handle counter to UINT64_MAX
	// to force the wrap-around branch deterministically.
	static void SetNextCallbackHandleForTest(CallbackHandle ulValue);

	// Lets TestShutdownClearsAllStatics seed suppression state to verify
	// Shutdown forcibly clears even corrupt flag combinations.
	static void SetActiveSceneSuppressedForTest(bool b);
#endif
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
