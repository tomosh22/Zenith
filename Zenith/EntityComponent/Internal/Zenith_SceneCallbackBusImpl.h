#pragma once

#include "Collections/Zenith_Vector.h"
#include "EntityComponent/Internal/Zenith_SceneCallbackBus.h"

// Phase 5c: per-Engine state for the scene callback bus. The
// anonymous-namespace globals in Zenith_SceneCallbackBus.cpp move here
// (callback lists + handle allocator + deferred-removal queue + dispatch
// depth + active-scene suppression flags). The .cpp's helper templates
// (Register / Unregister / Fire / IsCallbackHandleInUse /
// IsCallbackPendingRemoval / ProcessPendingCallbackRemovals) keep their
// anonymous-namespace scope and read/write g_xEngine.SceneCallbacks().
//
// CallbackEntry and CallbackList templates are nested public types so
// they have a single declaration point and don't collide with anything
// in global scope.
class Zenith_SceneCallbackBusImpl
{
public:
	Zenith_SceneCallbackBusImpl() = default;
	~Zenith_SceneCallbackBusImpl() = default;

	Zenith_SceneCallbackBusImpl(const Zenith_SceneCallbackBusImpl&) = delete;
	Zenith_SceneCallbackBusImpl& operator=(const Zenith_SceneCallbackBusImpl&) = delete;

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

	// Six callback lists -- one per event type.
	CallbackList<Zenith_SceneManager::SceneChangedCallback>      m_xActiveSceneChangedCallbacks;
	CallbackList<Zenith_SceneManager::SceneLoadedCallback>       m_xSceneLoadedCallbacks;
	CallbackList<Zenith_SceneManager::SceneUnloadingCallback>    m_xSceneUnloadingCallbacks;
	CallbackList<Zenith_SceneManager::SceneUnloadedCallback>     m_xSceneUnloadedCallbacks;
	CallbackList<Zenith_SceneManager::SceneLoadStartedCallback>  m_xSceneLoadStartedCallbacks;
	CallbackList<Zenith_SceneManager::EntityPersistentCallback>  m_xEntityPersistentCallbacks;

	// Handle allocator & deferred-removal machinery.
	CallbackHandle              m_ulNextCallbackHandle = 1;
	Zenith_Vector<CallbackHandle> m_axCallbacksPendingRemoval;
	uint32_t                    m_uFiringCallbacksDepth = 0;

	// Active-scene-changed suppression state.
	bool         m_bSuppressActiveSceneChanged = false;
	bool         m_bHaveDeferredOldActive      = false;
	Zenith_Scene m_xDeferredOldActive;
};
