#include "Zenith.h"

#include "EntityComponent/Internal/Zenith_SceneCallbackBus.h"

//=============================================================================
// Zenith_SceneCallbackBus — implementation.
//
// Owns the six callback lists, handle allocator, deferred-removal queue,
// dispatch-depth counter and active-scene-suppression flags. Template
// machinery is anonymous-namespace-scoped so the only TU that instantiates
// Zenith_CallbackList<T> is this one.
//=============================================================================

namespace
{
	using CallbackHandle = Zenith_SceneCallbackBus::CallbackHandle;

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
	CallbackList<Zenith_SceneManager::SceneChangedCallback>      g_xActiveSceneChangedCallbacks;
	CallbackList<Zenith_SceneManager::SceneLoadedCallback>       g_xSceneLoadedCallbacks;
	CallbackList<Zenith_SceneManager::SceneUnloadingCallback>    g_xSceneUnloadingCallbacks;
	CallbackList<Zenith_SceneManager::SceneUnloadedCallback>     g_xSceneUnloadedCallbacks;
	CallbackList<Zenith_SceneManager::SceneLoadStartedCallback>  g_xSceneLoadStartedCallbacks;
	CallbackList<Zenith_SceneManager::EntityPersistentCallback>  g_xEntityPersistentCallbacks;

	// Handle allocator & deferred-removal machinery.
	CallbackHandle g_ulNextCallbackHandle = 1;
	Zenith_Vector<CallbackHandle> g_axCallbacksPendingRemoval;
	uint32_t g_uFiringCallbacksDepth = 0;

	// Active-scene-changed suppression state. Phase A: exposed via raw
	// IsActiveSceneSuppressed/SetActiveSceneSuppressed so the existing inline
	// call sites continue to work. A1.5 replaces these with the
	// ActiveSceneChangeSuppressionScope RAII.
	bool g_bSuppressActiveSceneChanged = false;
	bool g_bHaveDeferredOldActive = false;
	Zenith_Scene g_xDeferredOldActive;

	bool IsCallbackHandleInUse(CallbackHandle ulHandle)
	{
		for (u_int i = 0; i < g_xActiveSceneChangedCallbacks.m_axEntries.GetSize(); ++i)
			if (g_xActiveSceneChangedCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
		for (u_int i = 0; i < g_xSceneLoadedCallbacks.m_axEntries.GetSize(); ++i)
			if (g_xSceneLoadedCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
		for (u_int i = 0; i < g_xSceneUnloadingCallbacks.m_axEntries.GetSize(); ++i)
			if (g_xSceneUnloadingCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
		for (u_int i = 0; i < g_xSceneUnloadedCallbacks.m_axEntries.GetSize(); ++i)
			if (g_xSceneUnloadedCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
		for (u_int i = 0; i < g_xSceneLoadStartedCallbacks.m_axEntries.GetSize(); ++i)
			if (g_xSceneLoadStartedCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
		for (u_int i = 0; i < g_xEntityPersistentCallbacks.m_axEntries.GetSize(); ++i)
			if (g_xEntityPersistentCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
		return false;
	}

	CallbackHandle AllocateCallbackHandle()
	{
		if (g_ulNextCallbackHandle == UINT64_MAX)
		{
			Zenith_Warning(LOG_CATEGORY_SCENE,
				"Callback handle counter wrapped around after %llu registrations",
				static_cast<unsigned long long>(g_ulNextCallbackHandle));
			g_ulNextCallbackHandle = 1;

			constexpr uint64_t ulScanLimit = 1ull << 20;
			uint64_t ulScanned = 0;
			while (ulScanned < ulScanLimit && IsCallbackHandleInUse(g_ulNextCallbackHandle))
			{
				++g_ulNextCallbackHandle;
				++ulScanned;
				if (g_ulNextCallbackHandle == 0) g_ulNextCallbackHandle = 1;
			}
			Zenith_Assert(!IsCallbackHandleInUse(g_ulNextCallbackHandle),
				"AllocateCallbackHandle: wrap scan exhausted after %llu iterations",
				static_cast<unsigned long long>(ulScanned));
		}
		return g_ulNextCallbackHandle++;
	}

	bool IsCallbackPendingRemoval(CallbackHandle ulHandle)
	{
		for (u_int i = 0; i < g_axCallbacksPendingRemoval.GetSize(); ++i)
		{
			if (g_axCallbacksPendingRemoval.Get(i) == ulHandle) return true;
		}
		return false;
	}

	template<typename TCallback>
	CallbackHandle Register(CallbackList<TCallback>& xList, TCallback pfn)
	{
		Zenith_Assert(Zenith_Multithreading::IsMainThread(), "Callback registration must be on main thread");

		// B7: warn when Register runs during an active Fire dispatch. The Fire loop
		// captures the entry count at entry and does NOT pick up entries pushed mid-loop.
		if (g_uFiringCallbacksDepth > 0)
		{
			Zenith_Warning(LOG_CATEGORY_SCENE,
				"Zenith_SceneCallbackBus::Register: called during callback dispatch (depth=%u). "
				"The new callback will NOT fire for the currently-dispatching event; it fires on the next one.",
				g_uFiringCallbacksDepth);
		}

		CallbackHandle ulHandle = AllocateCallbackHandle();
		if (ulHandle == Zenith_SceneCallbackBus::INVALID_CALLBACK_HANDLE) return ulHandle;
		xList.m_axEntries.PushBack({ ulHandle, pfn });
		return ulHandle;
	}

	// C7 / F30: Unregister-during-Fire is safe and legal; the handle is queued
	// in g_axCallbacksPendingRemoval. The actual erase happens in
	// ProcessPendingCallbackRemovals after the outermost Fire returns.
	template<typename TCallback>
	bool Unregister(CallbackList<TCallback>& xList, CallbackHandle ulHandle)
	{
		Zenith_Assert(Zenith_Multithreading::IsMainThread(), "Callback unregistration must be on main thread");

		if (g_uFiringCallbacksDepth > 0)
		{
			g_axCallbacksPendingRemoval.PushBack(ulHandle);
			return true;
		}

		for (u_int i = 0; i < xList.m_axEntries.GetSize(); ++i)
		{
			if (xList.m_axEntries.Get(i).m_ulHandle == ulHandle)
			{
				xList.m_axEntries.Remove(i);
				return true;
			}
		}
		return false;
	}

	void ProcessPendingCallbackRemovals()
	{
		for (u_int i = 0; i < g_axCallbacksPendingRemoval.GetSize(); ++i)
		{
			CallbackHandle ulHandle = g_axCallbacksPendingRemoval.Get(i);
			if (Unregister(g_xActiveSceneChangedCallbacks, ulHandle)) continue;
			if (Unregister(g_xSceneLoadedCallbacks, ulHandle)) continue;
			if (Unregister(g_xSceneUnloadingCallbacks, ulHandle)) continue;
			if (Unregister(g_xSceneUnloadedCallbacks, ulHandle)) continue;
			if (Unregister(g_xSceneLoadStartedCallbacks, ulHandle)) continue;
			Unregister(g_xEntityPersistentCallbacks, ulHandle);
		}
		g_axCallbacksPendingRemoval.Clear();
	}

	template<typename TCallback, typename... Args>
	void Fire(CallbackList<TCallback>& xList, Args&&... args)
	{
		g_uFiringCallbacksDepth++;

		// D1: bound callback-dispatch re-entry — a handler that fires the same
		// event type recursively builds the depth counter; cap at 16 to surface
		// runaway recursion before stack overflow.
		constexpr u_int uMAX_CALLBACK_DEPTH = 16;
		Zenith_Assert(g_uFiringCallbacksDepth <= uMAX_CALLBACK_DEPTH,
			"Zenith_SceneCallbackBus::Fire: callback dispatch depth %u exceeded safe limit (%u). "
			"A scene-callback handler is recursively triggering the same event type.",
			g_uFiringCallbacksDepth, uMAX_CALLBACK_DEPTH);

		const u_int uCount = xList.m_axEntries.GetSize();
		for (u_int i = 0; i < uCount; ++i)
		{
			if (!IsCallbackPendingRemoval(xList.m_axEntries.Get(i).m_ulHandle))
			{
				xList.m_axEntries.Get(i).m_pfnCallback(std::forward<Args>(args)...);
			}
		}

		g_uFiringCallbacksDepth--;
		if (g_uFiringCallbacksDepth == 0)
		{
			ProcessPendingCallbackRemovals();
		}
	}
}

//=============================================================================
// Public API
//=============================================================================

void Zenith_SceneCallbackBus::Shutdown()
{
	g_xActiveSceneChangedCallbacks.m_axEntries.Clear();
	g_xSceneLoadedCallbacks.m_axEntries.Clear();
	g_xSceneUnloadingCallbacks.m_axEntries.Clear();
	g_xSceneUnloadedCallbacks.m_axEntries.Clear();
	g_xSceneLoadStartedCallbacks.m_axEntries.Clear();
	g_xEntityPersistentCallbacks.m_axEntries.Clear();
	g_axCallbacksPendingRemoval.Clear();
	g_uFiringCallbacksDepth = 0;
	g_bSuppressActiveSceneChanged = false;
	g_bHaveDeferredOldActive = false;
}

Zenith_SceneCallbackBus::CallbackHandle Zenith_SceneCallbackBus::RegisterActiveSceneChanged(Zenith_SceneManager::SceneChangedCallback pfn)
{
	return Register(g_xActiveSceneChangedCallbacks, pfn);
}
bool Zenith_SceneCallbackBus::UnregisterActiveSceneChanged(CallbackHandle ulHandle)
{
	return Unregister(g_xActiveSceneChangedCallbacks, ulHandle);
}

Zenith_SceneCallbackBus::CallbackHandle Zenith_SceneCallbackBus::RegisterSceneLoaded(Zenith_SceneManager::SceneLoadedCallback pfn)
{
	return Register(g_xSceneLoadedCallbacks, pfn);
}
bool Zenith_SceneCallbackBus::UnregisterSceneLoaded(CallbackHandle ulHandle)
{
	return Unregister(g_xSceneLoadedCallbacks, ulHandle);
}

Zenith_SceneCallbackBus::CallbackHandle Zenith_SceneCallbackBus::RegisterSceneUnloading(Zenith_SceneManager::SceneUnloadingCallback pfn)
{
	return Register(g_xSceneUnloadingCallbacks, pfn);
}
bool Zenith_SceneCallbackBus::UnregisterSceneUnloading(CallbackHandle ulHandle)
{
	return Unregister(g_xSceneUnloadingCallbacks, ulHandle);
}

Zenith_SceneCallbackBus::CallbackHandle Zenith_SceneCallbackBus::RegisterSceneUnloaded(Zenith_SceneManager::SceneUnloadedCallback pfn)
{
	return Register(g_xSceneUnloadedCallbacks, pfn);
}
bool Zenith_SceneCallbackBus::UnregisterSceneUnloaded(CallbackHandle ulHandle)
{
	return Unregister(g_xSceneUnloadedCallbacks, ulHandle);
}

Zenith_SceneCallbackBus::CallbackHandle Zenith_SceneCallbackBus::RegisterSceneLoadStarted(Zenith_SceneManager::SceneLoadStartedCallback pfn)
{
	return Register(g_xSceneLoadStartedCallbacks, pfn);
}
bool Zenith_SceneCallbackBus::UnregisterSceneLoadStarted(CallbackHandle ulHandle)
{
	return Unregister(g_xSceneLoadStartedCallbacks, ulHandle);
}

Zenith_SceneCallbackBus::CallbackHandle Zenith_SceneCallbackBus::RegisterEntityPersistent(Zenith_SceneManager::EntityPersistentCallback pfn)
{
	return Register(g_xEntityPersistentCallbacks, pfn);
}
bool Zenith_SceneCallbackBus::UnregisterEntityPersistent(CallbackHandle ulHandle)
{
	return Unregister(g_xEntityPersistentCallbacks, ulHandle);
}

void Zenith_SceneCallbackBus::FireActiveSceneChanged(Zenith_Scene xOld, Zenith_Scene xNew)
{
	Fire(g_xActiveSceneChangedCallbacks, xOld, xNew);
}
void Zenith_SceneCallbackBus::FireSceneLoaded(Zenith_Scene xScene, Zenith_SceneLoadMode eMode)
{
	Fire(g_xSceneLoadedCallbacks, xScene, eMode);
}
void Zenith_SceneCallbackBus::FireSceneUnloading(Zenith_Scene xScene)
{
	Fire(g_xSceneUnloadingCallbacks, xScene);
}
void Zenith_SceneCallbackBus::FireSceneUnloaded(Zenith_Scene xScene)
{
	Fire(g_xSceneUnloadedCallbacks, xScene);
}
void Zenith_SceneCallbackBus::FireSceneLoadStarted(const std::string& strPath)
{
	Fire(g_xSceneLoadStartedCallbacks, strPath);
}
void Zenith_SceneCallbackBus::FireEntityPersistent(const Zenith_Entity& xEntity)
{
	Fire(g_xEntityPersistentCallbacks, xEntity);
}

bool Zenith_SceneCallbackBus::IsActiveSceneSuppressed()
{
	return g_bSuppressActiveSceneChanged;
}
void Zenith_SceneCallbackBus::SetActiveSceneSuppressed(bool b)
{
	g_bSuppressActiveSceneChanged = b;
}

bool Zenith_SceneCallbackBus::HasDeferredOldActive()
{
	return g_bHaveDeferredOldActive;
}
Zenith_Scene Zenith_SceneCallbackBus::GetDeferredOldActive()
{
	return g_xDeferredOldActive;
}
void Zenith_SceneCallbackBus::SetDeferredOldActive(Zenith_Scene xScene)
{
	g_xDeferredOldActive = xScene;
	g_bHaveDeferredOldActive = true;
}
void Zenith_SceneCallbackBus::ClearDeferredOldActive()
{
	g_bHaveDeferredOldActive = false;
}

//==========================================================================
// ActiveSceneChangeSuppressionScope
//==========================================================================

ActiveSceneChangeSuppressionScope::ActiveSceneChangeSuppressionScope(Zenith_Scene xInitialOldActive)
	: m_xOldActive(xInitialOldActive)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "ActiveSceneChangeSuppressionScope must be constructed on main thread");
	Zenith_Assert(!Zenith_SceneCallbackBus::IsActiveSceneSuppressed(),
		"ActiveSceneChangeSuppressionScope: nested suppression scopes are not supported");

	Zenith_SceneCallbackBus::SetActiveSceneSuppressed(true);
	Zenith_SceneCallbackBus::ClearDeferredOldActive();
}

ActiveSceneChangeSuppressionScope::~ActiveSceneChangeSuppressionScope()
{
	Zenith_Assert(m_bResolved,
		"ActiveSceneChangeSuppressionScope destroyed without Complete or Cancel — suppression flag would leak");
	if (!m_bResolved)
	{
		// Defensive in non-assert builds: never let the suppression flag leak.
		Zenith_SceneCallbackBus::SetActiveSceneSuppressed(false);
		Zenith_SceneCallbackBus::ClearDeferredOldActive();
	}
}

void ActiveSceneChangeSuppressionScope::Complete(Zenith_Scene xNewActive)
{
	Zenith_Assert(!m_bResolved, "ActiveSceneChangeSuppressionScope::Complete called after Complete/Cancel");
	m_bResolved = true;

	// Resolve the effective old-active scene before clearing bus state.
	const Zenith_Scene xOldActive = m_xOldActive.IsValid()
		? m_xOldActive
		: (Zenith_SceneCallbackBus::HasDeferredOldActive()
			? Zenith_SceneCallbackBus::GetDeferredOldActive()
			: Zenith_Scene::INVALID_SCENE);

	Zenith_SceneCallbackBus::SetActiveSceneSuppressed(false);
	Zenith_SceneCallbackBus::ClearDeferredOldActive();

	if (xOldActive != xNewActive)
	{
		Zenith_SceneCallbackBus::FireActiveSceneChanged(xOldActive, xNewActive);
	}
}

void ActiveSceneChangeSuppressionScope::Cancel()
{
	Zenith_Assert(!m_bResolved, "ActiveSceneChangeSuppressionScope::Cancel called after Complete/Cancel");
	m_bResolved = true;

	Zenith_SceneCallbackBus::SetActiveSceneSuppressed(false);
	Zenith_SceneCallbackBus::ClearDeferredOldActive();
}

#ifdef ZENITH_TESTING
u_int Zenith_SceneCallbackBus::GetActiveSceneChangedCallbackCount() { return g_xActiveSceneChangedCallbacks.m_axEntries.GetSize(); }
u_int Zenith_SceneCallbackBus::GetSceneLoadedCallbackCount()        { return g_xSceneLoadedCallbacks.m_axEntries.GetSize(); }
u_int Zenith_SceneCallbackBus::GetSceneUnloadingCallbackCount()     { return g_xSceneUnloadingCallbacks.m_axEntries.GetSize(); }
u_int Zenith_SceneCallbackBus::GetSceneUnloadedCallbackCount()      { return g_xSceneUnloadedCallbacks.m_axEntries.GetSize(); }
u_int Zenith_SceneCallbackBus::GetSceneLoadStartedCallbackCount()   { return g_xSceneLoadStartedCallbacks.m_axEntries.GetSize(); }
u_int Zenith_SceneCallbackBus::GetEntityPersistentCallbackCount()   { return g_xEntityPersistentCallbacks.m_axEntries.GetSize(); }
u_int Zenith_SceneCallbackBus::GetCallbackDispatchDepth()           { return g_uFiringCallbacksDepth; }
u_int Zenith_SceneCallbackBus::GetPendingRemovalCount()             { return g_axCallbacksPendingRemoval.GetSize(); }
void Zenith_SceneCallbackBus::SetNextCallbackHandleForTest(CallbackHandle ulValue) { g_ulNextCallbackHandle = ulValue; }
void Zenith_SceneCallbackBus::SetActiveSceneSuppressedForTest(bool b)              { g_bSuppressActiveSceneChanged = b; }
#endif
