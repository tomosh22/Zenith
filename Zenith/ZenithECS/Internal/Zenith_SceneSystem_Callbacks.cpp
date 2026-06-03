#include "Zenith.h"

#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"

//=============================================================================
// Zenith_SceneSystem — the composite unload helper.
//
// Phase 7b-2 retired the old callback bus (4 callback lists, handle allocator,
// deferred-removal queue, dispatch-depth counter, Register/Unregister surface).
// The scene-lifecycle event-dispatch Fire* methods that briefly replaced it
// (routing through Zenith_EventDispatcher) were themselves removed once the
// Zenith_Event_Scene* events proved to have zero subscribers — so this file no
// longer emits any scene-lifecycle notification.
//
// What survives here: FireUnloadCallbacksAndSelectNewActive (the composite
// unload + new-active selection helper). The active-scene suppression flags +
// ActiveSceneChangeSuppressionScope that once bracketed a SINGLE-mode teardown
// were removed too — the only consumer of the deferred-old-active value was the
// ActiveSceneChanged dispatch, which is gone, so the whole mechanism was dead.
//=============================================================================

//=============================================================================
// Composite unload-dispatch helper.
//=============================================================================

void Zenith_SceneSystem::FireUnloadCallbacksAndSelectNewActive(int iHandle, Zenith_Scene xScene)
{
	// Track if we're unloading the active scene before destruction
	// (capture before the slot is freed below).
	bool bWasActiveScene = (iHandle == m_iActiveSceneHandle);

	// Shared delete → null slot → FreeSceneHandle sequence.
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
	}
}
