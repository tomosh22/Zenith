#include "Zenith.h"

#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Zenith_SceneData.h"

//=============================================================================
// Zenith_SceneSystem — callback bus: 4 callback lists, handle allocator,
// deferred-removal queue, dispatch-depth counter, active-scene-suppression
// flags.
//
// The dispatch helpers (Register / Unregister / Fire / ...) are private members
// so the state they touch can stay private. Instance methods reach their own
// members directly; ActiveSceneChangeSuppressionScope (a separate type) reaches
// the singleton through g_xEngine.Scenes().
//=============================================================================

//=============================================================================
// Internal helpers (private members).
//=============================================================================

bool Zenith_SceneSystem::IsCallbackHandleInUse(CallbackHandle ulHandle) const
{
	for (u_int i = 0; i < m_xActiveSceneChangedCallbacks.m_axEntries.GetSize(); ++i)
		if (m_xActiveSceneChangedCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
	for (u_int i = 0; i < m_xSceneLoadedCallbacks.m_axEntries.GetSize(); ++i)
		if (m_xSceneLoadedCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
	for (u_int i = 0; i < m_xSceneUnloadingCallbacks.m_axEntries.GetSize(); ++i)
		if (m_xSceneUnloadingCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
	for (u_int i = 0; i < m_xSceneUnloadedCallbacks.m_axEntries.GetSize(); ++i)
		if (m_xSceneUnloadedCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
	return false;
}

Zenith_SceneSystem::CallbackHandle Zenith_SceneSystem::AllocateCallbackHandle()
{
	if (m_ulNextCallbackHandle == UINT64_MAX)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE,
			"Callback handle counter wrapped around after %llu registrations",
			static_cast<unsigned long long>(m_ulNextCallbackHandle));
		m_ulNextCallbackHandle = 1;

		constexpr uint64_t ulScanLimit = 1ull << 20;
		uint64_t ulScanned = 0;
		while (ulScanned < ulScanLimit && IsCallbackHandleInUse(m_ulNextCallbackHandle))
		{
			++m_ulNextCallbackHandle;
			++ulScanned;
			if (m_ulNextCallbackHandle == 0) m_ulNextCallbackHandle = 1;
		}
		Zenith_Assert(!IsCallbackHandleInUse(m_ulNextCallbackHandle),
			"AllocateCallbackHandle: wrap scan exhausted after %llu iterations",
			static_cast<unsigned long long>(ulScanned));
	}
	return m_ulNextCallbackHandle++;
}

bool Zenith_SceneSystem::IsCallbackPendingRemoval(CallbackHandle ulHandle) const
{
	for (u_int i = 0; i < m_axCallbacksPendingRemoval.GetSize(); ++i)
	{
		if (m_axCallbacksPendingRemoval.Get(i) == ulHandle) return true;
	}
	return false;
}

template<typename TCallback>
Zenith_SceneSystem::CallbackHandle Zenith_SceneSystem::Register(CallbackList<TCallback>& xList, TCallback pfn)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "Callback registration must be on main thread");

	// B7: warn when Register runs during an active Fire dispatch. The Fire loop
	// captures the entry count at entry and does NOT pick up entries pushed mid-loop.
	if (m_uFiringCallbacksDepth > 0)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE,
			"Zenith_SceneSystem::Register: called during callback dispatch (depth=%u). "
			"The new callback will NOT fire for the currently-dispatching event; it fires on the next one.",
			m_uFiringCallbacksDepth);
	}

	CallbackHandle ulHandle = AllocateCallbackHandle();
	if (ulHandle == INVALID_CALLBACK_HANDLE) return ulHandle;
	xList.m_axEntries.PushBack({ ulHandle, pfn });
	return ulHandle;
}

// C7 / F30: Unregister-during-Fire is safe and legal; the handle is queued
// in m_axCallbacksPendingRemoval. The actual erase happens in
// ProcessPendingCallbackRemovals after the outermost Fire returns.
template<typename TCallback>
bool Zenith_SceneSystem::Unregister(CallbackList<TCallback>& xList, CallbackHandle ulHandle)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "Callback unregistration must be on main thread");

	if (m_uFiringCallbacksDepth > 0)
	{
		m_axCallbacksPendingRemoval.PushBack(ulHandle);
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

void Zenith_SceneSystem::ProcessPendingCallbackRemovals()
{
	for (u_int i = 0; i < m_axCallbacksPendingRemoval.GetSize(); ++i)
	{
		CallbackHandle ulHandle = m_axCallbacksPendingRemoval.Get(i);
		if (Unregister(m_xActiveSceneChangedCallbacks, ulHandle)) continue;
		if (Unregister(m_xSceneLoadedCallbacks, ulHandle)) continue;
		if (Unregister(m_xSceneUnloadingCallbacks, ulHandle)) continue;
		Unregister(m_xSceneUnloadedCallbacks, ulHandle);
	}
	m_axCallbacksPendingRemoval.Clear();
}

template<typename TCallback, typename... Args>
void Zenith_SceneSystem::Fire(CallbackList<TCallback>& xList, Args&&... args)
{
	m_uFiringCallbacksDepth++;

	// D1: bound callback-dispatch re-entry — a handler that fires the same
	// event type recursively builds the depth counter; cap at 16 to surface
	// runaway recursion before stack overflow.
	constexpr u_int uMAX_CALLBACK_DEPTH = 16;
	Zenith_Assert(m_uFiringCallbacksDepth <= uMAX_CALLBACK_DEPTH,
		"Zenith_SceneSystem::Fire: callback dispatch depth %u exceeded safe limit (%u). "
		"A scene-callback handler is recursively triggering the same event type.",
		m_uFiringCallbacksDepth, uMAX_CALLBACK_DEPTH);

	const u_int uCount = xList.m_axEntries.GetSize();
	for (u_int i = 0; i < uCount; ++i)
	{
		if (!IsCallbackPendingRemoval(xList.m_axEntries.Get(i).m_ulHandle))
		{
			xList.m_axEntries.Get(i).m_pfnCallback(std::forward<Args>(args)...);
		}
	}

	m_uFiringCallbacksDepth--;
	if (m_uFiringCallbacksDepth == 0)
	{
		ProcessPendingCallbackRemovals();
	}
}

//=============================================================================
// Public API
//=============================================================================

// Shutdown body merged into Internal/Zenith_SceneSystem_Lifecycle.cpp.

Zenith_SceneSystem::CallbackHandle Zenith_SceneSystem::RegisterActiveSceneChanged(Zenith_SceneChangedCallback pfn)
{
	return Register(m_xActiveSceneChangedCallbacks, pfn);
}
bool Zenith_SceneSystem::UnregisterActiveSceneChanged(CallbackHandle ulHandle)
{
	return Unregister(m_xActiveSceneChangedCallbacks, ulHandle);
}

Zenith_SceneSystem::CallbackHandle Zenith_SceneSystem::RegisterSceneLoaded(Zenith_SceneLoadedCallback pfn)
{
	return Register(m_xSceneLoadedCallbacks, pfn);
}
bool Zenith_SceneSystem::UnregisterSceneLoaded(CallbackHandle ulHandle)
{
	return Unregister(m_xSceneLoadedCallbacks, ulHandle);
}

Zenith_SceneSystem::CallbackHandle Zenith_SceneSystem::RegisterSceneUnloading(Zenith_SceneUnloadingCallback pfn)
{
	return Register(m_xSceneUnloadingCallbacks, pfn);
}
bool Zenith_SceneSystem::UnregisterSceneUnloading(CallbackHandle ulHandle)
{
	return Unregister(m_xSceneUnloadingCallbacks, ulHandle);
}

Zenith_SceneSystem::CallbackHandle Zenith_SceneSystem::RegisterSceneUnloaded(Zenith_SceneUnloadedCallback pfn)
{
	return Register(m_xSceneUnloadedCallbacks, pfn);
}
bool Zenith_SceneSystem::UnregisterSceneUnloaded(CallbackHandle ulHandle)
{
	return Unregister(m_xSceneUnloadedCallbacks, ulHandle);
}

void Zenith_SceneSystem::FireActiveSceneChanged(Zenith_Scene xOld, Zenith_Scene xNew)
{
	Fire(m_xActiveSceneChangedCallbacks, xOld, xNew);
}
void Zenith_SceneSystem::FireSceneLoaded(Zenith_Scene xScene, Zenith_SceneLoadMode eMode)
{
	Fire(m_xSceneLoadedCallbacks, xScene, eMode);
}
void Zenith_SceneSystem::FireSceneUnloading(Zenith_Scene xScene)
{
	Fire(m_xSceneUnloadingCallbacks, xScene);
}
void Zenith_SceneSystem::FireSceneUnloaded(Zenith_Scene xScene)
{
	Fire(m_xSceneUnloadedCallbacks, xScene);
}

bool Zenith_SceneSystem::IsActiveSceneSuppressed()
{
	return m_bSuppressActiveSceneChanged;
}
void Zenith_SceneSystem::SetActiveSceneSuppressed(bool b)
{
	m_bSuppressActiveSceneChanged = b;
}

bool Zenith_SceneSystem::HasDeferredOldActive()
{
	return m_bHaveDeferredOldActive;
}
Zenith_Scene Zenith_SceneSystem::GetDeferredOldActive()
{
	return m_xDeferredOldActive;
}
void Zenith_SceneSystem::SetDeferredOldActive(Zenith_Scene xScene)
{
	m_xDeferredOldActive = xScene;
	m_bHaveDeferredOldActive = true;
}
void Zenith_SceneSystem::ClearDeferredOldActive()
{
	m_bHaveDeferredOldActive = false;
}

//=============================================================================
// Composite unload-dispatch helper.
//=============================================================================

void Zenith_SceneSystem::FireUnloadCallbacksAndSelectNewActive(int iHandle, Zenith_Scene xScene)
{
	// Track if we're unloading the active scene BEFORE callbacks
	// (a callback could call SetActiveScene, so capture this first).
	bool bWasActiveScene = (iHandle == m_iActiveSceneHandle);

	// Fire unloading callback BEFORE destruction (allows access to scene data).
	FireSceneUnloading(xScene);

	// Shared delete → null slot → SceneUnloaded → FreeSceneHandle sequence.
	if (iHandle >= 0 && iHandle < static_cast<int>(m_axScenes.GetSize()))
	{
		bool bIgnored = false;
		UnloadOneScene(xScene, bIgnored);
	}

	// If active scene was unloaded, select a new active scene.
	if (bWasActiveScene)
	{
		Zenith_Assert(!m_bRenderTasksActive, "Cannot change active scene while render tasks are in flight");
		m_iActiveSceneHandle = SelectNewActiveScene();
		Zenith_Scene xNewActive = GetActiveScene();

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

//==========================================================================
// ActiveSceneChangeSuppressionScope — a free-standing type, so it reaches the
// singleton through g_xEngine.Scenes().
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
u_int Zenith_SceneSystem::GetActiveSceneChangedCallbackCount() { return m_xActiveSceneChangedCallbacks.m_axEntries.GetSize(); }
u_int Zenith_SceneSystem::GetSceneLoadedCallbackCount()        { return m_xSceneLoadedCallbacks.m_axEntries.GetSize(); }
u_int Zenith_SceneSystem::GetSceneUnloadingCallbackCount()     { return m_xSceneUnloadingCallbacks.m_axEntries.GetSize(); }
u_int Zenith_SceneSystem::GetSceneUnloadedCallbackCount()      { return m_xSceneUnloadedCallbacks.m_axEntries.GetSize(); }
u_int Zenith_SceneSystem::GetCallbackDispatchDepth()           { return m_uFiringCallbacksDepth; }
u_int Zenith_SceneSystem::GetPendingRemovalCount()             { return m_axCallbacksPendingRemoval.GetSize(); }
void Zenith_SceneSystem::SetNextCallbackHandleForTest(CallbackHandle ulValue) { m_ulNextCallbackHandle = ulValue; }
void Zenith_SceneSystem::SetActiveSceneSuppressedForTest(bool b)              { m_bSuppressActiveSceneChanged = b; }
#endif
