#include "Zenith.h"

#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_Entity.h"

//=============================================================================
// Scene Manager Callbacks
//
// Extracted from Zenith_SceneManager.cpp: the callback registration/dispatch
// machinery — lists, handle allocation, Register/Unregister/Fire template
// implementations, and the one-liner Register<X>Callback/Fire<X>Callbacks
// wrappers. Static members below are the single definitions for the whole
// program. All other callers (Shutdown's m_axEntries.Clear() directly, and
// the various FireXxxCallbacks invocation sites sprinkled across the scene
// lifecycle code) live in the main .cpp and reach this TU by link.
//=============================================================================

// Static member definitions for the callback subsystem. Moved here alongside
// the template methods so every instantiation site lives in the same TU.
Zenith_SceneManager::Zenith_CallbackList<Zenith_SceneManager::SceneChangedCallback> Zenith_SceneManager::s_xActiveSceneChangedCallbacks;
Zenith_SceneManager::Zenith_CallbackList<Zenith_SceneManager::SceneLoadedCallback> Zenith_SceneManager::s_xSceneLoadedCallbacks;
Zenith_SceneManager::Zenith_CallbackList<Zenith_SceneManager::SceneUnloadingCallback> Zenith_SceneManager::s_xSceneUnloadingCallbacks;
Zenith_SceneManager::Zenith_CallbackList<Zenith_SceneManager::SceneUnloadedCallback> Zenith_SceneManager::s_xSceneUnloadedCallbacks;
Zenith_SceneManager::Zenith_CallbackList<Zenith_SceneManager::SceneLoadStartedCallback> Zenith_SceneManager::s_xSceneLoadStartedCallbacks;
Zenith_SceneManager::Zenith_CallbackList<Zenith_SceneManager::EntityPersistentCallback> Zenith_SceneManager::s_xEntityPersistentCallbacks;
Zenith_SceneManager::CallbackHandle Zenith_SceneManager::s_ulNextCallbackHandle = 1;
Zenith_Vector<Zenith_SceneManager::CallbackHandle> Zenith_SceneManager::s_axCallbacksPendingRemoval;
uint32_t Zenith_SceneManager::s_uFiringCallbacksDepth = 0;

// Check whether a handle is currently live in any callback list. Used by
// AllocateCallbackHandle on wrap to avoid collisions with long-lived callbacks.
bool Zenith_SceneManager::IsCallbackHandleInUse(CallbackHandle ulHandle)
{
	for (u_int i = 0; i < s_xActiveSceneChangedCallbacks.m_axEntries.GetSize(); ++i)
		if (s_xActiveSceneChangedCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
	for (u_int i = 0; i < s_xSceneLoadedCallbacks.m_axEntries.GetSize(); ++i)
		if (s_xSceneLoadedCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
	for (u_int i = 0; i < s_xSceneUnloadingCallbacks.m_axEntries.GetSize(); ++i)
		if (s_xSceneUnloadingCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
	for (u_int i = 0; i < s_xSceneUnloadedCallbacks.m_axEntries.GetSize(); ++i)
		if (s_xSceneUnloadedCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
	for (u_int i = 0; i < s_xSceneLoadStartedCallbacks.m_axEntries.GetSize(); ++i)
		if (s_xSceneLoadStartedCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
	for (u_int i = 0; i < s_xEntityPersistentCallbacks.m_axEntries.GetSize(); ++i)
		if (s_xEntityPersistentCallbacks.m_axEntries.Get(i).m_ulHandle == ulHandle) return true;
	return false;
}

// Helper to allocate callback handles with overflow protection
Zenith_SceneManager::CallbackHandle Zenith_SceneManager::AllocateCallbackHandle()
{
	// Wrap around on overflow (skip 0 which is INVALID_CALLBACK_HANDLE).
	// Collision with a long-lived callback after wrap is astronomically unlikely
	// but not impossible: scan forward from 1 until a free handle is found.
	if (s_ulNextCallbackHandle == UINT64_MAX)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE,
			"Callback handle counter wrapped around after %llu registrations",
			static_cast<unsigned long long>(s_ulNextCallbackHandle));
		s_ulNextCallbackHandle = 1;  // Skip 0 (INVALID_CALLBACK_HANDLE)

		constexpr uint64_t ulScanLimit = 1ull << 20;
		uint64_t ulScanned = 0;
		while (ulScanned < ulScanLimit && IsCallbackHandleInUse(s_ulNextCallbackHandle))
		{
			++s_ulNextCallbackHandle;
			++ulScanned;
			if (s_ulNextCallbackHandle == 0) s_ulNextCallbackHandle = 1;
		}
		Zenith_Assert(!IsCallbackHandleInUse(s_ulNextCallbackHandle),
			"AllocateCallbackHandle: wrap scan exhausted after %llu iterations",
			static_cast<unsigned long long>(ulScanned));
	}
	return s_ulNextCallbackHandle++;
}

// Templatized callback list implementation
template<typename TCallback>
Zenith_SceneManager::CallbackHandle Zenith_SceneManager::Zenith_CallbackList<TCallback>::Register(TCallback pfn)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "Callback registration must be on main thread");

	// B7: warn when Register runs during an active Fire dispatch. The Fire loop
	// captures the entry count at entry and does NOT pick up entries pushed mid-loop,
	// so callers expecting their new callback to run THIS cycle will be surprised.
	// Registering during dispatch is legal but the callback won't fire until next event.
	if (s_uFiringCallbacksDepth > 0)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE,
			"Zenith_CallbackList::Register: called during callback dispatch (depth=%u). "
			"The new callback will NOT fire for the currently-dispatching event; it fires on the next one.",
			s_uFiringCallbacksDepth);
	}

	// Unity parity: duplicate registrations are allowed. Each Register call allocates a
	// fresh handle; Unregister(handle) removes exactly the registration that handle
	// identifies (handle-based, not pfn-based), so duplicates can be torn down cleanly
	// one handle at a time. This matches Unity's `sceneLoaded += Handler` firing the
	// handler twice when subscribed twice.
	CallbackHandle ulHandle = AllocateCallbackHandle();
	if (ulHandle == INVALID_CALLBACK_HANDLE) return ulHandle;
	m_axEntries.PushBack({ ulHandle, pfn });
	return ulHandle;
}

// C7 (F30): Deferred-unregister contract — what happens when Unregister is called
// during callback dispatch.
//
// Contract:
//   1. Unregister-during-Fire is SAFE and legal. The handle is queued in
//      s_axCallbacksPendingRemoval; the entries vector is not mutated mid-loop.
//   2. The callback matching the unregistered handle will NOT fire again for
//      the CURRENT dispatch iteration (Fire() consults IsCallbackPendingRemoval
//      before calling each entry).
//   3. After the outermost Fire() finishes, ProcessPendingCallbackRemovals drains
//      the queue and actually erases entries from m_axEntries, releasing the slot.
//   4. A handle that has been unregistered-pending still OCCUPIES its slot until
//      drain — so a re-register of the same pfn during dispatch observes the
//      dedupe check and returns the soon-to-be-freed handle. This is the
//      documented behaviour; callers who want to re-register cleanly should
//      wait until after the dispatch completes.
//   5. Re-entrant dispatch (Fire inside Fire) only drains pending removals when
//      s_uFiringCallbacksDepth returns to 0. Nested dispatches see the pending
//      entries as still occupying their slots; the skip-in-Fire behaviour inside
//      Fire() handles the visibility correctly.
template<typename TCallback>
bool Zenith_SceneManager::Zenith_CallbackList<TCallback>::Unregister(CallbackHandle ulHandle)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "Callback unregistration must be on main thread");

	if (s_uFiringCallbacksDepth > 0)
	{
		s_axCallbacksPendingRemoval.PushBack(ulHandle);
		return true;
	}

	for (u_int i = 0; i < m_axEntries.GetSize(); ++i)
	{
		if (m_axEntries.Get(i).m_ulHandle == ulHandle)
		{
			m_axEntries.Remove(i);
			return true;
		}
	}
	return false;
}

template<typename TCallback>
template<typename... Args>
void Zenith_SceneManager::Zenith_CallbackList<TCallback>::Fire(Args&&... args)
{
	s_uFiringCallbacksDepth++;

	// D1: bound callback-dispatch re-entry. A handler that fires a scene event
	// that re-enters the same callback list builds the depth counter; beyond
	// a sane limit (16) something has gone into recursion and stack-overflow is
	// imminent. Catch loudly instead of crashing.
	constexpr u_int uMAX_CALLBACK_DEPTH = 16;
	Zenith_Assert(s_uFiringCallbacksDepth <= uMAX_CALLBACK_DEPTH,
		"Zenith_CallbackList::Fire: callback dispatch depth %u exceeded safe limit (%u). "
		"A scene-callback handler is recursively triggering the same event type.",
		s_uFiringCallbacksDepth, uMAX_CALLBACK_DEPTH);

	const u_int uCount = m_axEntries.GetSize();
	for (u_int i = 0; i < uCount; ++i)
	{
		if (!IsCallbackPendingRemoval(m_axEntries.Get(i).m_ulHandle))
		{
			m_axEntries.Get(i).m_pfnCallback(std::forward<Args>(args)...);
		}
	}

	s_uFiringCallbacksDepth--;
	if (s_uFiringCallbacksDepth == 0)
	{
		ProcessPendingCallbackRemovals();
	}
}

// Register/Unregister/Fire one-liner wrappers
Zenith_SceneManager::CallbackHandle Zenith_SceneManager::RegisterActiveSceneChangedCallback(SceneChangedCallback pfn) { return s_xActiveSceneChangedCallbacks.Register(pfn); }
void Zenith_SceneManager::UnregisterActiveSceneChangedCallback(CallbackHandle ulHandle) { s_xActiveSceneChangedCallbacks.Unregister(ulHandle); }

Zenith_SceneManager::CallbackHandle Zenith_SceneManager::RegisterSceneLoadedCallback(SceneLoadedCallback pfn) { return s_xSceneLoadedCallbacks.Register(pfn); }
void Zenith_SceneManager::UnregisterSceneLoadedCallback(CallbackHandle ulHandle) { s_xSceneLoadedCallbacks.Unregister(ulHandle); }

Zenith_SceneManager::CallbackHandle Zenith_SceneManager::RegisterSceneUnloadingCallback(SceneUnloadingCallback pfn) { return s_xSceneUnloadingCallbacks.Register(pfn); }
void Zenith_SceneManager::UnregisterSceneUnloadingCallback(CallbackHandle ulHandle) { s_xSceneUnloadingCallbacks.Unregister(ulHandle); }

Zenith_SceneManager::CallbackHandle Zenith_SceneManager::RegisterSceneUnloadedCallback(SceneUnloadedCallback pfn) { return s_xSceneUnloadedCallbacks.Register(pfn); }
void Zenith_SceneManager::UnregisterSceneUnloadedCallback(CallbackHandle ulHandle) { s_xSceneUnloadedCallbacks.Unregister(ulHandle); }

Zenith_SceneManager::CallbackHandle Zenith_SceneManager::RegisterSceneLoadStartedCallback(SceneLoadStartedCallback pfn) { return s_xSceneLoadStartedCallbacks.Register(pfn); }
void Zenith_SceneManager::UnregisterSceneLoadStartedCallback(CallbackHandle ulHandle) { s_xSceneLoadStartedCallbacks.Unregister(ulHandle); }

Zenith_SceneManager::CallbackHandle Zenith_SceneManager::RegisterEntityPersistentCallback(EntityPersistentCallback pfn) { return s_xEntityPersistentCallbacks.Register(pfn); }
void Zenith_SceneManager::UnregisterEntityPersistentCallback(CallbackHandle ulHandle) { s_xEntityPersistentCallbacks.Unregister(ulHandle); }

bool Zenith_SceneManager::IsCallbackPendingRemoval(CallbackHandle ulHandle)
{
	for (u_int i = 0; i < s_axCallbacksPendingRemoval.GetSize(); ++i)
	{
		if (s_axCallbacksPendingRemoval.Get(i) == ulHandle) return true;
	}
	return false;
}

void Zenith_SceneManager::ProcessPendingCallbackRemovals()
{
	for (u_int i = 0; i < s_axCallbacksPendingRemoval.GetSize(); ++i)
	{
		CallbackHandle ulHandle = s_axCallbacksPendingRemoval.Get(i);
		// Callback handles are unique across all lists - stop on first match
		if (s_xActiveSceneChangedCallbacks.Unregister(ulHandle)) continue;
		if (s_xSceneLoadedCallbacks.Unregister(ulHandle)) continue;
		if (s_xSceneUnloadingCallbacks.Unregister(ulHandle)) continue;
		if (s_xSceneUnloadedCallbacks.Unregister(ulHandle)) continue;
		if (s_xSceneLoadStartedCallbacks.Unregister(ulHandle)) continue;
		s_xEntityPersistentCallbacks.Unregister(ulHandle);
	}
	s_axCallbacksPendingRemoval.Clear();
}

void Zenith_SceneManager::FireSceneLoadedCallbacks(Zenith_Scene xScene, Zenith_SceneLoadMode eMode) { s_xSceneLoadedCallbacks.Fire(xScene, eMode); }
void Zenith_SceneManager::FireSceneUnloadingCallbacks(Zenith_Scene xScene) { s_xSceneUnloadingCallbacks.Fire(xScene); }
void Zenith_SceneManager::FireSceneUnloadedCallbacks(Zenith_Scene xScene) { s_xSceneUnloadedCallbacks.Fire(xScene); }
void Zenith_SceneManager::FireActiveSceneChangedCallbacks(Zenith_Scene xOld, Zenith_Scene xNew) { s_xActiveSceneChangedCallbacks.Fire(xOld, xNew); }
void Zenith_SceneManager::FireSceneLoadStartedCallbacks(const std::string& strPath) { s_xSceneLoadStartedCallbacks.Fire(strPath); }
void Zenith_SceneManager::FireEntityPersistentCallbacks(const Zenith_Entity& xEntity) { s_xEntityPersistentCallbacks.Fire(xEntity); }
