#include "Zenith.h"

#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Zenith_SceneData.h"

//=============================================================================
// Zenith_SceneSystem — callback bus: 4 callback lists, handle allocator,
// deferred-removal queue, dispatch-depth counter, active-scene-suppression
// flags. State lives on Zenith_SceneSystem; helpers are anon-namespace.
//=============================================================================

namespace
{
	using CallbackHandle = Zenith_SceneSystem::CallbackHandle;

	template<typename T>
	using CallbackEntry = Zenith_SceneSystem::CallbackEntry<T>;

	template<typename TCallback>
	using CallbackList = Zenith_SceneSystem::CallbackList<TCallback>;

	bool IsCallbackHandleInUse(CallbackHandle ulHandle)
	{
		Zenith_SceneSystem& xB = g_xEngine.Scenes();
		for (u_int i = 0; i < xB.m_xActiveSceneChangedCallbacks.m_axEntries.GetSize(); ++i)
			if (xB.m_xActiveSceneChangedCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
		for (u_int i = 0; i < xB.m_xSceneLoadedCallbacks.m_axEntries.GetSize(); ++i)
			if (xB.m_xSceneLoadedCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
		for (u_int i = 0; i < xB.m_xSceneUnloadingCallbacks.m_axEntries.GetSize(); ++i)
			if (xB.m_xSceneUnloadingCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
		for (u_int i = 0; i < xB.m_xSceneUnloadedCallbacks.m_axEntries.GetSize(); ++i)
			if (xB.m_xSceneUnloadedCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
		return false;
	}

	CallbackHandle AllocateCallbackHandle()
	{
		if (g_xEngine.Scenes().m_ulNextCallbackHandle == UINT64_MAX)
		{
			Zenith_Warning(LOG_CATEGORY_SCENE,
				"Callback handle counter wrapped around after %llu registrations",
				static_cast<unsigned long long>(g_xEngine.Scenes().m_ulNextCallbackHandle));
			g_xEngine.Scenes().m_ulNextCallbackHandle = 1;

			constexpr uint64_t ulScanLimit = 1ull << 20;
			uint64_t ulScanned = 0;
			while (ulScanned < ulScanLimit && IsCallbackHandleInUse(g_xEngine.Scenes().m_ulNextCallbackHandle))
			{
				++g_xEngine.Scenes().m_ulNextCallbackHandle;
				++ulScanned;
				if (g_xEngine.Scenes().m_ulNextCallbackHandle == 0) g_xEngine.Scenes().m_ulNextCallbackHandle = 1;
			}
			Zenith_Assert(!IsCallbackHandleInUse(g_xEngine.Scenes().m_ulNextCallbackHandle),
				"AllocateCallbackHandle: wrap scan exhausted after %llu iterations",
				static_cast<unsigned long long>(ulScanned));
		}
		return g_xEngine.Scenes().m_ulNextCallbackHandle++;
	}

	bool IsCallbackPendingRemoval(CallbackHandle ulHandle)
	{
		for (u_int i = 0; i < g_xEngine.Scenes().m_axCallbacksPendingRemoval.GetSize(); ++i)
		{
			if (g_xEngine.Scenes().m_axCallbacksPendingRemoval.Get(i) == ulHandle) return true;
		}
		return false;
	}

	template<typename TCallback>
	CallbackHandle Register(CallbackList<TCallback>& xList, TCallback pfn)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(), "Callback registration must be on main thread");

		// B7: warn when Register runs during an active Fire dispatch. The Fire loop
		// captures the entry count at entry and does NOT pick up entries pushed mid-loop.
		if (g_xEngine.Scenes().m_uFiringCallbacksDepth > 0)
		{
			Zenith_Warning(LOG_CATEGORY_SCENE,
				"Zenith_SceneSystem::Register: called during callback dispatch (depth=%u). "
				"The new callback will NOT fire for the currently-dispatching event; it fires on the next one.",
				g_xEngine.Scenes().m_uFiringCallbacksDepth);
		}

		CallbackHandle ulHandle = AllocateCallbackHandle();
		if (ulHandle == Zenith_SceneSystem::INVALID_CALLBACK_HANDLE) return ulHandle;
		xList.m_axEntries.PushBack({ ulHandle, pfn });
		return ulHandle;
	}

	// C7 / F30: Unregister-during-Fire is safe and legal; the handle is queued
	// in g_xEngine.Scenes().m_axCallbacksPendingRemoval. The actual erase happens in
	// ProcessPendingCallbackRemovals after the outermost Fire returns.
	template<typename TCallback>
	bool Unregister(CallbackList<TCallback>& xList, CallbackHandle ulHandle)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(), "Callback unregistration must be on main thread");

		if (g_xEngine.Scenes().m_uFiringCallbacksDepth > 0)
		{
			g_xEngine.Scenes().m_axCallbacksPendingRemoval.PushBack(ulHandle);
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
		for (u_int i = 0; i < g_xEngine.Scenes().m_axCallbacksPendingRemoval.GetSize(); ++i)
		{
			CallbackHandle ulHandle = g_xEngine.Scenes().m_axCallbacksPendingRemoval.Get(i);
			if (Unregister(g_xEngine.Scenes().m_xActiveSceneChangedCallbacks, ulHandle)) continue;
			if (Unregister(g_xEngine.Scenes().m_xSceneLoadedCallbacks, ulHandle)) continue;
			if (Unregister(g_xEngine.Scenes().m_xSceneUnloadingCallbacks, ulHandle)) continue;
			Unregister(g_xEngine.Scenes().m_xSceneUnloadedCallbacks, ulHandle);
		}
		g_xEngine.Scenes().m_axCallbacksPendingRemoval.Clear();
	}

	template<typename TCallback, typename... Args>
	void Fire(CallbackList<TCallback>& xList, Args&&... args)
	{
		g_xEngine.Scenes().m_uFiringCallbacksDepth++;

		// D1: bound callback-dispatch re-entry — a handler that fires the same
		// event type recursively builds the depth counter; cap at 16 to surface
		// runaway recursion before stack overflow.
		constexpr u_int uMAX_CALLBACK_DEPTH = 16;
		Zenith_Assert(g_xEngine.Scenes().m_uFiringCallbacksDepth <= uMAX_CALLBACK_DEPTH,
			"Zenith_SceneSystem::Fire: callback dispatch depth %u exceeded safe limit (%u). "
			"A scene-callback handler is recursively triggering the same event type.",
			g_xEngine.Scenes().m_uFiringCallbacksDepth, uMAX_CALLBACK_DEPTH);

		const u_int uCount = xList.m_axEntries.GetSize();
		for (u_int i = 0; i < uCount; ++i)
		{
			if (!IsCallbackPendingRemoval(xList.m_axEntries.Get(i).m_ulHandle))
			{
				xList.m_axEntries.Get(i).m_pfnCallback(std::forward<Args>(args)...);
			}
		}

		g_xEngine.Scenes().m_uFiringCallbacksDepth--;
		if (g_xEngine.Scenes().m_uFiringCallbacksDepth == 0)
		{
			ProcessPendingCallbackRemovals();
		}
	}
}

//=============================================================================
// Public API
//=============================================================================

// Shutdown body merged into Internal/Zenith_SceneSystem_Lifecycle.cpp.

Zenith_SceneSystem::CallbackHandle Zenith_SceneSystem::RegisterActiveSceneChanged(Zenith_SceneChangedCallback pfn)
{
	return Register(g_xEngine.Scenes().m_xActiveSceneChangedCallbacks, pfn);
}
bool Zenith_SceneSystem::UnregisterActiveSceneChanged(CallbackHandle ulHandle)
{
	return Unregister(g_xEngine.Scenes().m_xActiveSceneChangedCallbacks, ulHandle);
}

Zenith_SceneSystem::CallbackHandle Zenith_SceneSystem::RegisterSceneLoaded(Zenith_SceneLoadedCallback pfn)
{
	return Register(g_xEngine.Scenes().m_xSceneLoadedCallbacks, pfn);
}
bool Zenith_SceneSystem::UnregisterSceneLoaded(CallbackHandle ulHandle)
{
	return Unregister(g_xEngine.Scenes().m_xSceneLoadedCallbacks, ulHandle);
}

Zenith_SceneSystem::CallbackHandle Zenith_SceneSystem::RegisterSceneUnloading(Zenith_SceneUnloadingCallback pfn)
{
	return Register(g_xEngine.Scenes().m_xSceneUnloadingCallbacks, pfn);
}
bool Zenith_SceneSystem::UnregisterSceneUnloading(CallbackHandle ulHandle)
{
	return Unregister(g_xEngine.Scenes().m_xSceneUnloadingCallbacks, ulHandle);
}

Zenith_SceneSystem::CallbackHandle Zenith_SceneSystem::RegisterSceneUnloaded(Zenith_SceneUnloadedCallback pfn)
{
	return Register(g_xEngine.Scenes().m_xSceneUnloadedCallbacks, pfn);
}
bool Zenith_SceneSystem::UnregisterSceneUnloaded(CallbackHandle ulHandle)
{
	return Unregister(g_xEngine.Scenes().m_xSceneUnloadedCallbacks, ulHandle);
}

void Zenith_SceneSystem::FireActiveSceneChanged(Zenith_Scene xOld, Zenith_Scene xNew)
{
	Fire(g_xEngine.Scenes().m_xActiveSceneChangedCallbacks, xOld, xNew);
}
void Zenith_SceneSystem::FireSceneLoaded(Zenith_Scene xScene, Zenith_SceneLoadMode eMode)
{
	Fire(g_xEngine.Scenes().m_xSceneLoadedCallbacks, xScene, eMode);
}
void Zenith_SceneSystem::FireSceneUnloading(Zenith_Scene xScene)
{
	Fire(g_xEngine.Scenes().m_xSceneUnloadingCallbacks, xScene);
}
void Zenith_SceneSystem::FireSceneUnloaded(Zenith_Scene xScene)
{
	Fire(g_xEngine.Scenes().m_xSceneUnloadedCallbacks, xScene);
}

bool Zenith_SceneSystem::IsActiveSceneSuppressed()
{
	return g_xEngine.Scenes().m_bSuppressActiveSceneChanged;
}
void Zenith_SceneSystem::SetActiveSceneSuppressed(bool b)
{
	g_xEngine.Scenes().m_bSuppressActiveSceneChanged = b;
}

bool Zenith_SceneSystem::HasDeferredOldActive()
{
	return g_xEngine.Scenes().m_bHaveDeferredOldActive;
}
Zenith_Scene Zenith_SceneSystem::GetDeferredOldActive()
{
	return g_xEngine.Scenes().m_xDeferredOldActive;
}
void Zenith_SceneSystem::SetDeferredOldActive(Zenith_Scene xScene)
{
	g_xEngine.Scenes().m_xDeferredOldActive = xScene;
	g_xEngine.Scenes().m_bHaveDeferredOldActive = true;
}
void Zenith_SceneSystem::ClearDeferredOldActive()
{
	g_xEngine.Scenes().m_bHaveDeferredOldActive = false;
}

//==========================================================================
// ActiveSceneChangeSuppressionScope
//==========================================================================

ActiveSceneChangeSuppressionScope::ActiveSceneChangeSuppressionScope(Zenith_Scene xInitialOldActive)
	: m_xOldActive(xInitialOldActive)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "ActiveSceneChangeSuppressionScope must be constructed on main thread");
	Zenith_Assert(!g_xEngine.Scenes().IsActiveSceneSuppressed(),
		"ActiveSceneChangeSuppressionScope: nested suppression scopes are not supported");

	g_xEngine.Scenes().SetActiveSceneSuppressed(true);
	g_xEngine.Scenes().ClearDeferredOldActive();
}

ActiveSceneChangeSuppressionScope::~ActiveSceneChangeSuppressionScope()
{
	Zenith_Assert(m_bResolved,
		"ActiveSceneChangeSuppressionScope destroyed without Complete or Cancel — suppression flag would leak");
	if (!m_bResolved)
	{
		// Defensive in non-assert builds: never let the suppression flag leak.
		g_xEngine.Scenes().SetActiveSceneSuppressed(false);
		g_xEngine.Scenes().ClearDeferredOldActive();
	}
}

//=============================================================================
// Composite unload-dispatch helper.
//=============================================================================

void Zenith_SceneSystem::FireUnloadCallbacksAndSelectNewActive(int iHandle, Zenith_Scene xScene)
{
	// Track if we're unloading the active scene BEFORE callbacks
	// (a callback could call SetActiveScene, so capture this first).
	bool bWasActiveScene = (iHandle == g_xEngine.Scenes().m_iActiveSceneHandle);

	// Fire unloading callback BEFORE destruction (allows access to scene data).
	FireSceneUnloading(xScene);

	// Free the scene data.
	if (iHandle >= 0 && iHandle < static_cast<int>(g_xEngine.Scenes().m_axScenes.GetSize()))
	{
		delete g_xEngine.Scenes().m_axScenes.Get(iHandle);
		g_xEngine.Scenes().m_axScenes.Get(iHandle) = nullptr;

		// Fire unloaded callback BEFORE incrementing generation so the handle
		// is still valid for identification in callbacks (Unity parity).
		FireSceneUnloaded(xScene);

		g_xEngine.Scenes().FreeSceneHandle(iHandle);
	}

	// If active scene was unloaded, select a new active scene.
	if (bWasActiveScene)
	{
		Zenith_Assert(!g_xEngine.Scenes().m_bRenderTasksActive, "Cannot change active scene while render tasks are in flight");
		g_xEngine.Scenes().m_iActiveSceneHandle = g_xEngine.Scenes().SelectNewActiveScene();
		Zenith_Scene xNewActive = g_xEngine.Scenes().GetActiveScene();

		// During a LoadScene(SINGLE) teardown we suppress the intermediate
		// "old → fallback" dispatch and remember the very first oldActive we saw.
		// LoadScene fires a single "original → new" callback once the new scene is
		// active. Mirrors Unity's single-activeSceneChanged-per-load guarantee.
		if (IsActiveSceneSuppressed())
		{
			if (!HasDeferredOldActive())
			{
				SetDeferredOldActive(xScene);
			}
		}
		else
		{
			FireActiveSceneChanged(xScene, xNewActive);
		}
	}
}

void ActiveSceneChangeSuppressionScope::Complete(Zenith_Scene xNewActive)
{
	Zenith_Assert(!m_bResolved, "ActiveSceneChangeSuppressionScope::Complete called after Complete/Cancel");
	m_bResolved = true;

	// Resolve the effective old-active scene before clearing bus state.
	const Zenith_Scene xOldActive = m_xOldActive.IsValid()
		? m_xOldActive
		: (g_xEngine.Scenes().HasDeferredOldActive()
			? g_xEngine.Scenes().GetDeferredOldActive()
			: Zenith_Scene::INVALID_SCENE);

	g_xEngine.Scenes().SetActiveSceneSuppressed(false);
	g_xEngine.Scenes().ClearDeferredOldActive();

	if (xOldActive != xNewActive)
	{
		g_xEngine.Scenes().FireActiveSceneChanged(xOldActive, xNewActive);
	}
}

void ActiveSceneChangeSuppressionScope::Cancel()
{
	Zenith_Assert(!m_bResolved, "ActiveSceneChangeSuppressionScope::Cancel called after Complete/Cancel");
	m_bResolved = true;

	g_xEngine.Scenes().SetActiveSceneSuppressed(false);
	g_xEngine.Scenes().ClearDeferredOldActive();
}

#ifdef ZENITH_TESTING
u_int Zenith_SceneSystem::GetActiveSceneChangedCallbackCount() { return Zenith_SceneSystem::m_xActiveSceneChangedCallbacks.m_axEntries.GetSize(); }
u_int Zenith_SceneSystem::GetSceneLoadedCallbackCount()        { return Zenith_SceneSystem::m_xSceneLoadedCallbacks.m_axEntries.GetSize(); }
u_int Zenith_SceneSystem::GetSceneUnloadingCallbackCount()     { return Zenith_SceneSystem::m_xSceneUnloadingCallbacks.m_axEntries.GetSize(); }
u_int Zenith_SceneSystem::GetSceneUnloadedCallbackCount()      { return Zenith_SceneSystem::m_xSceneUnloadedCallbacks.m_axEntries.GetSize(); }
u_int Zenith_SceneSystem::GetCallbackDispatchDepth()           { return Zenith_SceneSystem::m_uFiringCallbacksDepth; }
u_int Zenith_SceneSystem::GetPendingRemovalCount()             { return Zenith_SceneSystem::m_axCallbacksPendingRemoval.GetSize(); }
void Zenith_SceneSystem::SetNextCallbackHandleForTest(CallbackHandle ulValue) { Zenith_SceneSystem::m_ulNextCallbackHandle = ulValue; }
void Zenith_SceneSystem::SetActiveSceneSuppressedForTest(bool b)              { Zenith_SceneSystem::m_bSuppressActiveSceneChanged = b; }
#endif
