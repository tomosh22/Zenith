#include "Zenith.h"

#include "EntityComponent/Internal/Zenith_SceneCallbackBus.h"
#include "EntityComponent/Internal/Zenith_SceneCallbackBusImpl.h"

//=============================================================================
// Zenith_SceneCallbackBus — implementation.
//
// Owns the six callback lists, handle allocator, deferred-removal queue,
// dispatch-depth counter and active-scene-suppression flags. Phase 5c moved
// the state onto Zenith_SceneCallbackBusImpl held by Zenith_Engine; the
// helper templates below remain anon-namespace scoped and now drive that
// Impl rather than file-scope statics.
//=============================================================================

namespace
{
	using CallbackHandle = Zenith_SceneCallbackBus::CallbackHandle;

	// Aliases for the Impl's nested templates so the helpers below read
	// the same way they did before the migration.
	template<typename T>
	using CallbackEntry = Zenith_SceneCallbackBusImpl::CallbackEntry<T>;

	template<typename TCallback>
	using CallbackList = Zenith_SceneCallbackBusImpl::CallbackList<TCallback>;

	bool IsCallbackHandleInUse(CallbackHandle ulHandle)
	{
		Zenith_SceneCallbackBusImpl& xB = g_xEngine.SceneCallbacks();
		for (u_int i = 0; i < xB.m_xActiveSceneChangedCallbacks.m_axEntries.GetSize(); ++i)
			if (xB.m_xActiveSceneChangedCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
		for (u_int i = 0; i < xB.m_xSceneLoadedCallbacks.m_axEntries.GetSize(); ++i)
			if (xB.m_xSceneLoadedCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
		for (u_int i = 0; i < xB.m_xSceneUnloadingCallbacks.m_axEntries.GetSize(); ++i)
			if (xB.m_xSceneUnloadingCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
		for (u_int i = 0; i < xB.m_xSceneUnloadedCallbacks.m_axEntries.GetSize(); ++i)
			if (xB.m_xSceneUnloadedCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
		for (u_int i = 0; i < xB.m_xSceneLoadStartedCallbacks.m_axEntries.GetSize(); ++i)
			if (xB.m_xSceneLoadStartedCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
		for (u_int i = 0; i < xB.m_xEntityPersistentCallbacks.m_axEntries.GetSize(); ++i)
			if (xB.m_xEntityPersistentCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
		return false;
	}

	CallbackHandle AllocateCallbackHandle()
	{
		if (g_xEngine.SceneCallbacks().m_ulNextCallbackHandle == UINT64_MAX)
		{
			Zenith_Warning(LOG_CATEGORY_SCENE,
				"Callback handle counter wrapped around after %llu registrations",
				static_cast<unsigned long long>(g_xEngine.SceneCallbacks().m_ulNextCallbackHandle));
			g_xEngine.SceneCallbacks().m_ulNextCallbackHandle = 1;

			constexpr uint64_t ulScanLimit = 1ull << 20;
			uint64_t ulScanned = 0;
			while (ulScanned < ulScanLimit && IsCallbackHandleInUse(g_xEngine.SceneCallbacks().m_ulNextCallbackHandle))
			{
				++g_xEngine.SceneCallbacks().m_ulNextCallbackHandle;
				++ulScanned;
				if (g_xEngine.SceneCallbacks().m_ulNextCallbackHandle == 0) g_xEngine.SceneCallbacks().m_ulNextCallbackHandle = 1;
			}
			Zenith_Assert(!IsCallbackHandleInUse(g_xEngine.SceneCallbacks().m_ulNextCallbackHandle),
				"AllocateCallbackHandle: wrap scan exhausted after %llu iterations",
				static_cast<unsigned long long>(ulScanned));
		}
		return g_xEngine.SceneCallbacks().m_ulNextCallbackHandle++;
	}

	bool IsCallbackPendingRemoval(CallbackHandle ulHandle)
	{
		for (u_int i = 0; i < g_xEngine.SceneCallbacks().m_axCallbacksPendingRemoval.GetSize(); ++i)
		{
			if (g_xEngine.SceneCallbacks().m_axCallbacksPendingRemoval.Get(i) == ulHandle) return true;
		}
		return false;
	}

	template<typename TCallback>
	CallbackHandle Register(CallbackList<TCallback>& xList, TCallback pfn)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(), "Callback registration must be on main thread");

		// B7: warn when Register runs during an active Fire dispatch. The Fire loop
		// captures the entry count at entry and does NOT pick up entries pushed mid-loop.
		if (g_xEngine.SceneCallbacks().m_uFiringCallbacksDepth > 0)
		{
			Zenith_Warning(LOG_CATEGORY_SCENE,
				"Zenith_SceneCallbackBus::Register: called during callback dispatch (depth=%u). "
				"The new callback will NOT fire for the currently-dispatching event; it fires on the next one.",
				g_xEngine.SceneCallbacks().m_uFiringCallbacksDepth);
		}

		CallbackHandle ulHandle = AllocateCallbackHandle();
		if (ulHandle == Zenith_SceneCallbackBus::INVALID_CALLBACK_HANDLE) return ulHandle;
		xList.m_axEntries.PushBack({ ulHandle, pfn });
		return ulHandle;
	}

	// C7 / F30: Unregister-during-Fire is safe and legal; the handle is queued
	// in g_xEngine.SceneCallbacks().m_axCallbacksPendingRemoval. The actual erase happens in
	// ProcessPendingCallbackRemovals after the outermost Fire returns.
	template<typename TCallback>
	bool Unregister(CallbackList<TCallback>& xList, CallbackHandle ulHandle)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(), "Callback unregistration must be on main thread");

		if (g_xEngine.SceneCallbacks().m_uFiringCallbacksDepth > 0)
		{
			g_xEngine.SceneCallbacks().m_axCallbacksPendingRemoval.PushBack(ulHandle);
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
		for (u_int i = 0; i < g_xEngine.SceneCallbacks().m_axCallbacksPendingRemoval.GetSize(); ++i)
		{
			CallbackHandle ulHandle = g_xEngine.SceneCallbacks().m_axCallbacksPendingRemoval.Get(i);
			if (Unregister(g_xEngine.SceneCallbacks().m_xActiveSceneChangedCallbacks, ulHandle)) continue;
			if (Unregister(g_xEngine.SceneCallbacks().m_xSceneLoadedCallbacks, ulHandle)) continue;
			if (Unregister(g_xEngine.SceneCallbacks().m_xSceneUnloadingCallbacks, ulHandle)) continue;
			if (Unregister(g_xEngine.SceneCallbacks().m_xSceneUnloadedCallbacks, ulHandle)) continue;
			if (Unregister(g_xEngine.SceneCallbacks().m_xSceneLoadStartedCallbacks, ulHandle)) continue;
			Unregister(g_xEngine.SceneCallbacks().m_xEntityPersistentCallbacks, ulHandle);
		}
		g_xEngine.SceneCallbacks().m_axCallbacksPendingRemoval.Clear();
	}

	template<typename TCallback, typename... Args>
	void Fire(CallbackList<TCallback>& xList, Args&&... args)
	{
		g_xEngine.SceneCallbacks().m_uFiringCallbacksDepth++;

		// D1: bound callback-dispatch re-entry — a handler that fires the same
		// event type recursively builds the depth counter; cap at 16 to surface
		// runaway recursion before stack overflow.
		constexpr u_int uMAX_CALLBACK_DEPTH = 16;
		Zenith_Assert(g_xEngine.SceneCallbacks().m_uFiringCallbacksDepth <= uMAX_CALLBACK_DEPTH,
			"Zenith_SceneCallbackBus::Fire: callback dispatch depth %u exceeded safe limit (%u). "
			"A scene-callback handler is recursively triggering the same event type.",
			g_xEngine.SceneCallbacks().m_uFiringCallbacksDepth, uMAX_CALLBACK_DEPTH);

		const u_int uCount = xList.m_axEntries.GetSize();
		for (u_int i = 0; i < uCount; ++i)
		{
			if (!IsCallbackPendingRemoval(xList.m_axEntries.Get(i).m_ulHandle))
			{
				xList.m_axEntries.Get(i).m_pfnCallback(std::forward<Args>(args)...);
			}
		}

		g_xEngine.SceneCallbacks().m_uFiringCallbacksDepth--;
		if (g_xEngine.SceneCallbacks().m_uFiringCallbacksDepth == 0)
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
	g_xEngine.SceneCallbacks().m_xActiveSceneChangedCallbacks.m_axEntries.Clear();
	g_xEngine.SceneCallbacks().m_xSceneLoadedCallbacks.m_axEntries.Clear();
	g_xEngine.SceneCallbacks().m_xSceneUnloadingCallbacks.m_axEntries.Clear();
	g_xEngine.SceneCallbacks().m_xSceneUnloadedCallbacks.m_axEntries.Clear();
	g_xEngine.SceneCallbacks().m_xSceneLoadStartedCallbacks.m_axEntries.Clear();
	g_xEngine.SceneCallbacks().m_xEntityPersistentCallbacks.m_axEntries.Clear();
	g_xEngine.SceneCallbacks().m_axCallbacksPendingRemoval.Clear();
	g_xEngine.SceneCallbacks().m_uFiringCallbacksDepth = 0;
	g_xEngine.SceneCallbacks().m_bSuppressActiveSceneChanged = false;
	g_xEngine.SceneCallbacks().m_bHaveDeferredOldActive = false;
}

Zenith_SceneCallbackBus::CallbackHandle Zenith_SceneCallbackBus::RegisterActiveSceneChanged(Zenith_SceneManager::SceneChangedCallback pfn)
{
	return Register(g_xEngine.SceneCallbacks().m_xActiveSceneChangedCallbacks, pfn);
}
bool Zenith_SceneCallbackBus::UnregisterActiveSceneChanged(CallbackHandle ulHandle)
{
	return Unregister(g_xEngine.SceneCallbacks().m_xActiveSceneChangedCallbacks, ulHandle);
}

Zenith_SceneCallbackBus::CallbackHandle Zenith_SceneCallbackBus::RegisterSceneLoaded(Zenith_SceneManager::SceneLoadedCallback pfn)
{
	return Register(g_xEngine.SceneCallbacks().m_xSceneLoadedCallbacks, pfn);
}
bool Zenith_SceneCallbackBus::UnregisterSceneLoaded(CallbackHandle ulHandle)
{
	return Unregister(g_xEngine.SceneCallbacks().m_xSceneLoadedCallbacks, ulHandle);
}

Zenith_SceneCallbackBus::CallbackHandle Zenith_SceneCallbackBus::RegisterSceneUnloading(Zenith_SceneManager::SceneUnloadingCallback pfn)
{
	return Register(g_xEngine.SceneCallbacks().m_xSceneUnloadingCallbacks, pfn);
}
bool Zenith_SceneCallbackBus::UnregisterSceneUnloading(CallbackHandle ulHandle)
{
	return Unregister(g_xEngine.SceneCallbacks().m_xSceneUnloadingCallbacks, ulHandle);
}

Zenith_SceneCallbackBus::CallbackHandle Zenith_SceneCallbackBus::RegisterSceneUnloaded(Zenith_SceneManager::SceneUnloadedCallback pfn)
{
	return Register(g_xEngine.SceneCallbacks().m_xSceneUnloadedCallbacks, pfn);
}
bool Zenith_SceneCallbackBus::UnregisterSceneUnloaded(CallbackHandle ulHandle)
{
	return Unregister(g_xEngine.SceneCallbacks().m_xSceneUnloadedCallbacks, ulHandle);
}

Zenith_SceneCallbackBus::CallbackHandle Zenith_SceneCallbackBus::RegisterSceneLoadStarted(Zenith_SceneManager::SceneLoadStartedCallback pfn)
{
	return Register(g_xEngine.SceneCallbacks().m_xSceneLoadStartedCallbacks, pfn);
}
bool Zenith_SceneCallbackBus::UnregisterSceneLoadStarted(CallbackHandle ulHandle)
{
	return Unregister(g_xEngine.SceneCallbacks().m_xSceneLoadStartedCallbacks, ulHandle);
}

Zenith_SceneCallbackBus::CallbackHandle Zenith_SceneCallbackBus::RegisterEntityPersistent(Zenith_SceneManager::EntityPersistentCallback pfn)
{
	return Register(g_xEngine.SceneCallbacks().m_xEntityPersistentCallbacks, pfn);
}
bool Zenith_SceneCallbackBus::UnregisterEntityPersistent(CallbackHandle ulHandle)
{
	return Unregister(g_xEngine.SceneCallbacks().m_xEntityPersistentCallbacks, ulHandle);
}

void Zenith_SceneCallbackBus::FireActiveSceneChanged(Zenith_Scene xOld, Zenith_Scene xNew)
{
	Fire(g_xEngine.SceneCallbacks().m_xActiveSceneChangedCallbacks, xOld, xNew);
}
void Zenith_SceneCallbackBus::FireSceneLoaded(Zenith_Scene xScene, Zenith_SceneLoadMode eMode)
{
	Fire(g_xEngine.SceneCallbacks().m_xSceneLoadedCallbacks, xScene, eMode);
}
void Zenith_SceneCallbackBus::FireSceneUnloading(Zenith_Scene xScene)
{
	Fire(g_xEngine.SceneCallbacks().m_xSceneUnloadingCallbacks, xScene);
}
void Zenith_SceneCallbackBus::FireSceneUnloaded(Zenith_Scene xScene)
{
	Fire(g_xEngine.SceneCallbacks().m_xSceneUnloadedCallbacks, xScene);
}
void Zenith_SceneCallbackBus::FireSceneLoadStarted(const std::string& strPath)
{
	Fire(g_xEngine.SceneCallbacks().m_xSceneLoadStartedCallbacks, strPath);
}
void Zenith_SceneCallbackBus::FireEntityPersistent(const Zenith_Entity& xEntity)
{
	Fire(g_xEngine.SceneCallbacks().m_xEntityPersistentCallbacks, xEntity);
}

bool Zenith_SceneCallbackBus::IsActiveSceneSuppressed()
{
	return g_xEngine.SceneCallbacks().m_bSuppressActiveSceneChanged;
}
void Zenith_SceneCallbackBus::SetActiveSceneSuppressed(bool b)
{
	g_xEngine.SceneCallbacks().m_bSuppressActiveSceneChanged = b;
}

bool Zenith_SceneCallbackBus::HasDeferredOldActive()
{
	return g_xEngine.SceneCallbacks().m_bHaveDeferredOldActive;
}
Zenith_Scene Zenith_SceneCallbackBus::GetDeferredOldActive()
{
	return g_xEngine.SceneCallbacks().m_xDeferredOldActive;
}
void Zenith_SceneCallbackBus::SetDeferredOldActive(Zenith_Scene xScene)
{
	g_xEngine.SceneCallbacks().m_xDeferredOldActive = xScene;
	g_xEngine.SceneCallbacks().m_bHaveDeferredOldActive = true;
}
void Zenith_SceneCallbackBus::ClearDeferredOldActive()
{
	g_xEngine.SceneCallbacks().m_bHaveDeferredOldActive = false;
}

//==========================================================================
// ActiveSceneChangeSuppressionScope
//==========================================================================

ActiveSceneChangeSuppressionScope::ActiveSceneChangeSuppressionScope(Zenith_Scene xInitialOldActive)
	: m_xOldActive(xInitialOldActive)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "ActiveSceneChangeSuppressionScope must be constructed on main thread");
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
u_int Zenith_SceneCallbackBus::GetActiveSceneChangedCallbackCount() { return g_xEngine.SceneCallbacks().m_xActiveSceneChangedCallbacks.m_axEntries.GetSize(); }
u_int Zenith_SceneCallbackBus::GetSceneLoadedCallbackCount()        { return g_xEngine.SceneCallbacks().m_xSceneLoadedCallbacks.m_axEntries.GetSize(); }
u_int Zenith_SceneCallbackBus::GetSceneUnloadingCallbackCount()     { return g_xEngine.SceneCallbacks().m_xSceneUnloadingCallbacks.m_axEntries.GetSize(); }
u_int Zenith_SceneCallbackBus::GetSceneUnloadedCallbackCount()      { return g_xEngine.SceneCallbacks().m_xSceneUnloadedCallbacks.m_axEntries.GetSize(); }
u_int Zenith_SceneCallbackBus::GetSceneLoadStartedCallbackCount()   { return g_xEngine.SceneCallbacks().m_xSceneLoadStartedCallbacks.m_axEntries.GetSize(); }
u_int Zenith_SceneCallbackBus::GetEntityPersistentCallbackCount()   { return g_xEngine.SceneCallbacks().m_xEntityPersistentCallbacks.m_axEntries.GetSize(); }
u_int Zenith_SceneCallbackBus::GetCallbackDispatchDepth()           { return g_xEngine.SceneCallbacks().m_uFiringCallbacksDepth; }
u_int Zenith_SceneCallbackBus::GetPendingRemovalCount()             { return g_xEngine.SceneCallbacks().m_axCallbacksPendingRemoval.GetSize(); }
void Zenith_SceneCallbackBus::SetNextCallbackHandleForTest(CallbackHandle ulValue) { g_xEngine.SceneCallbacks().m_ulNextCallbackHandle = ulValue; }
void Zenith_SceneCallbackBus::SetActiveSceneSuppressedForTest(bool b)              { g_xEngine.SceneCallbacks().m_bSuppressActiveSceneChanged = b; }
#endif
