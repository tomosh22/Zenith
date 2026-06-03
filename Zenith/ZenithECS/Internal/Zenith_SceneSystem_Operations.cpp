#include "Zenith.h"
#include "ZenithECS/Zenith_SceneSystem.h"
// Relocated RAII scope type Zenith_SceneCreationTargetScope, used to bracket the
// new scene's deserialisation below.
#include "ZenithECS/Internal/Zenith_SceneSystem_InternalScopes.h"

#include <algorithm>
#include "FileAccess/Zenith_FileAccess.h"
// Phase 10 (ECS leaf-extraction): the scene-teardown reset of the Flux render
// systems, the physics world, and unused assets is routed through the
// Zenith_ECSRuntimeHooks function pointers (m_pfnResetRenderSystems /
// m_pfnResetPhysics / m_pfnUnloadUnusedAssets, installed engine-side in
// Zenith_Engine::Initialise). The ECS core therefore no longer names any
// Flux *Impl type, Zenith_Physics, or Zenith_AssetRegistry, and the includes
// for those headers were dropped. This TU names no concrete engine component.

//=============================================================================
// File-scope helpers used by LoadScene only.
//=============================================================================

namespace
{
	bool ValidateLoadRequestInternal(const std::string& strPath)
	{
		Zenith_Assert(Zenith_ECS_IsMainThread(), "LoadScene must be called from main thread");
		if (strPath.empty())
		{
			Zenith_Error(LOG_CATEGORY_SCENE, "LoadScene: Path is empty");
			return false;
		}
		return true;
	}

	// "Levels/MyScene.zscen" → "MyScene".
	std::string ExtractSceneNameFromPath(const std::string& strPath)
	{
		size_t uLastSlash = strPath.find_last_of("/\\");
		size_t uStart = (uLastSlash == std::string::npos) ? 0 : uLastSlash + 1;
		size_t uLastDot = strPath.find_last_of('.');
		size_t uEnd = (uLastDot == std::string::npos || uLastDot < uStart) ?
			strPath.size() : uLastDot;
		return strPath.substr(uStart, uEnd - uStart);
	}
}

//==========================================================================
// Zenith_SceneSystem — sync scene load / unload. LoadScene reads the file
// inline, deserialises, dispatches Awake / OnEnable, flips the scene to LOADED,
// and returns the scene handle — all before returning. Shutdown body merged
// into Internal/Zenith_SceneSystem_Lifecycle.cpp.
//==========================================================================

Zenith_Scene Zenith_SceneSystem::LoadScene(const std::string& strPath, Zenith_SceneLoadMode eMode)
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "LoadScene must be called from main thread");
	Zenith_Assert(!m_bRenderTasksActive,
		"LoadScene: scene mutation while render tasks are reading — render-task invariant violated");

	if (!ValidateLoadRequestInternal(strPath))
		return Zenith_Scene::INVALID_SCENE;

	// Defer if we're inside a frame-update pass (UI iteration, script OnUpdate)
	// OR inside another LoadScene's lifecycle dispatch (a script's Awake/OnEnable
	// hook). Either case would tear down state the outer pass is still
	// reading. The pending slot drains as the outer pass returns — either via
	// Zenith_SceneUpdateDeferralGuard's destructor, the scheduler's Update
	// post-amble, or LoadScene's own post-dispatch drain below.
	if (m_bIsLoadingScene
		|| m_bIsUpdating)
	{
		m_xPendingLoad.m_bSet        = true;
		m_xPendingLoad.m_bByIndex    = false;
		m_xPendingLoad.m_strPath     = strPath;
		m_xPendingLoad.m_uMode       = static_cast<uint8_t>(eMode);
		m_xPendingLoad.m_iBuildIndex = -1;
		return Zenith_Scene::INVALID_SCENE;
	}

	const std::string strCanonicalPath = Zenith_SceneSystem::CanonicalisePath(strPath);
	const int iPendingBuildIndex = m_iPendingBuildIndex;
	m_iPendingBuildIndex = -1;

	// ADDITIVE_WITHOUT_LOADING: create an empty scene with no file I/O.
	if (eMode == SCENE_LOAD_ADDITIVE_WITHOUT_LOADING)
	{
		const std::string strName = ExtractSceneNameFromPath(strCanonicalPath);
		Zenith_Scene xScene = AllocateEmptyScene(strName);
		Zenith_SceneData* pxSceneData = GetSceneData(xScene);
		pxSceneData->m_strPath = strCanonicalPath;
		if (iPendingBuildIndex >= 0)
			pxSceneData->m_iBuildIndex = iPendingBuildIndex;
		return xScene;
	}

	// File existence. This is the first of two pre-teardown checks: existence
	// here, then ValidateSceneStream's non-destructive header pre-pass right
	// after the read below. Together they reject a missing / corrupt / unsupported
	// file before the SINGLE teardown, so a bad file fails safely with the live
	// world intact. The post-teardown deserialise rollback further down remains
	// as a backstop for body-level corruption that passes the header.
	if (!Zenith_FileAccess::FileExists(strPath.c_str()))
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "LoadScene: File not found: %s", strPath.c_str());
		return Zenith_Scene::INVALID_SCENE;
	}

	// Circular load guard
	if (IsCircularLoadDependency(strCanonicalPath))
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "Circular scene load detected: %s", strCanonicalPath.c_str());
		return Zenith_Scene::INVALID_SCENE;
	}
	m_axCurrentlyLoadingPaths.PushBack(strCanonicalPath);

	// Read file synchronously on the main thread, then run ValidateSceneStream
	// as a non-destructive header pre-pass (see below) so a corrupt/unsupported
	// file is rejected before the SINGLE teardown. The deserialise-failure
	// rollback further down stays as a backstop for body-level corruption.
	Zenith_DataStream xLoadedData;
	xLoadedData.ReadFromFile(strPath.c_str());

	// Non-destructive header pre-pass: reject a corrupt/empty/old/future-version
	// file HERE, before the SINGLE teardown below, so the live world (render
	// systems, entities, physics) stays intact on failure. ValidateSceneStream
	// restores the cursor, so the SetCursor(0) before LoadFromDataStream still
	// works unchanged. Cleanup is EraseValue only — m_bIsLoadingScene has NOT
	// been set yet (it is set just below), so we must not reset it here.
	if (!Zenith_SceneData::ValidateSceneStream(xLoadedData))
	{
		m_axCurrentlyLoadingPaths.EraseValue(strCanonicalPath);
		return Zenith_Scene::INVALID_SCENE;
	}

	m_bIsLoadingScene = true;

	// SINGLE: tear down the old world BEFORE creating the new scene.
	// Components register with their owning subsystem in their ctor / from
	// ReadFromDataStream (ColliderComponent → Jolt body, TerrainComponent
	// → Flux terrain entry, etc.), so a staging-scene pattern (deserialise
	// new before reset old) would wipe the freshly-registered entries.
	// Reset → unload → deserialise keeps the new scene's registrations
	// intact.
	if (eMode == SCENE_LOAD_SINGLE)
	{
		ResetAllRenderSystems();
		UnloadAllNonPersistent();
		UnloadUnusedAssets();
		if (m_xRuntimeHooks.m_pfnResetPhysics) { m_xRuntimeHooks.m_pfnResetPhysics(); }
		m_fFixedTimeAccumulator = 0.0f;  // Unity behavior
	}

	// Create the new scene + deserialise into the post-reset subsystems.
	const std::string strName = ExtractSceneNameFromPath(strCanonicalPath);
	Zenith_Scene xScene = AllocateEmptyScene(strName);
	{
		Zenith_SceneCreationTargetScope xCreationTargetScope(xScene);

		Zenith_SceneData* pxSceneData = GetSceneData(xScene);
		pxSceneData->m_strPath     = strCanonicalPath;
		pxSceneData->m_iBuildIndex = iPendingBuildIndex;
		pxSceneData->TransitionTo(Zenith_SceneData::SCENE_STATE_LOADING);
		if (eMode == SCENE_LOAD_ADDITIVE)
			pxSceneData->m_bWasLoadedAdditively = true;

		xLoadedData.SetCursor(0);
		// Scene files are assumed well-formed in practice, but a defensive
		// rollback is cheap and saves us from a half-torn-down engine if the
		// assumption ever breaks (corrupt file on disk, partial download,
		// version mismatch). Asserts in debug; in release we unwind cleanly
		// and return INVALID_SCENE.
		const bool bDeserialised = pxSceneData->LoadFromDataStream(xLoadedData);
		Zenith_Assert(bDeserialised,
			"LoadScene: failed to deserialise '%s' — scene files must be well-formed",
			strPath.c_str());
		if (!bDeserialised)
		{
			Zenith_Error(LOG_CATEGORY_SCENE, "LoadScene: deserialisation failed for '%s'", strPath.c_str());
			UnloadSceneForced(xScene);
			m_bIsLoadingScene = false;
			m_axCurrentlyLoadingPaths.EraseValue(strCanonicalPath);
			return Zenith_Scene::INVALID_SCENE;
		}

		// SINGLE: activate the new scene.
		if (eMode == SCENE_LOAD_SINGLE)
		{
			Zenith_Assert(!AreRenderTasksActive(),
				"Cannot change active scene while render tasks are in flight");
			m_iActiveSceneHandle = xScene.m_iHandle;
		}

		// Unity: Awake → OnEnable → Start (next frame).
		// m_bIsLoadingScene is kept TRUE through lifecycle dispatch — a re-entrant
		// LoadScene call from an Awake/OnEnable hook would otherwise tear down the
		// very scene being brought up. Such a call is deferred + drained after
		// dispatch returns.
		pxSceneData->DispatchAwakeForNewScene();
		pxSceneData->DispatchEnableAndPendingStartsForNewScene();
		// Transition to LOADED so IsActivated() is true once dispatch completes,
		// but keep m_bIsLoadingScene set so a re-entrant LoadScene call defers
		// rather than recursing.
		pxSceneData->TransitionTo(Zenith_SceneData::SCENE_STATE_LOADED);
	}

	// Clear loading-scene flag AFTER lifecycle dispatch (kept set above so a
	// hook couldn't recursively LoadScene mid-dispatch). Drain any stashed
	// request now that the gate is open.
	m_bIsLoadingScene = false;
	m_axCurrentlyLoadingPaths.EraseValue(strCanonicalPath);

	DrainPendingLoadIfAny();

	return xScene;
}

Zenith_Scene Zenith_SceneSystem::LoadSceneByIndex(int iBuildIndex, Zenith_SceneLoadMode eMode)
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "LoadSceneByIndex must be called from main thread");

	const u_int uBuildIndex = static_cast<u_int>(iBuildIndex);
	if (iBuildIndex < 0
		|| uBuildIndex >= m_axBuildIndexToPath.GetSize()
		|| m_axBuildIndexToPath.Get(uBuildIndex).empty())
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "LoadSceneByIndex: No scene registered for build index %d", iBuildIndex);
		return Zenith_Scene::INVALID_SCENE;
	}

	// Same re-entrancy guard as LoadScene — defer if mid-update OR mid-load
	// (inside another LoadScene's Awake/OnEnable dispatch). We stash the
	// build-index form so the build index is preserved through the drain (a
	// canonical-path lookup would lose it).
	if (m_bIsLoadingScene
		|| m_bIsUpdating)
	{
		m_xPendingLoad.m_bSet        = true;
		m_xPendingLoad.m_bByIndex    = true;
		m_xPendingLoad.m_iBuildIndex = iBuildIndex;
		m_xPendingLoad.m_strPath.clear();
		m_xPendingLoad.m_uMode       = static_cast<uint8_t>(eMode);
		return Zenith_Scene::INVALID_SCENE;
	}

	// Stash the build index so LoadScene picks it up and stamps the new scene,
	// restoring the previous value once the (possibly nested) load returns —
	// so a mid-call abort doesn't leak the pending value.
	const int iPrevPendingBuildIndex = m_iPendingBuildIndex;
	m_iPendingBuildIndex = iBuildIndex;
	Zenith_Scene xResult = LoadScene(m_axBuildIndexToPath.Get(uBuildIndex), eMode);
	m_iPendingBuildIndex = iPrevPendingBuildIndex;
	return xResult;
}

//=============================================================================
// Unload entry points + helpers.
//=============================================================================

bool Zenith_SceneSystem::CanUnloadScene(Zenith_Scene xScene)
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "CanUnloadScene must be called from main thread");

	if (!xScene.IsValid())
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "CanUnloadScene: Invalid scene");
		return false;
	}

	if (xScene.m_iHandle == m_iPersistentSceneHandle)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "Cannot unload persistent scene");
		return false;
	}

	// Cannot unload the last loaded scene (Unity behavior).
	uint32_t uNonPersistentCount = 0;
	for (u_int i = 0; i < m_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = m_axScenes.Get(i);
		if (pxData != nullptr && static_cast<int>(i) != m_iPersistentSceneHandle
			&& pxData->IsLoaded() && pxData->IsActivated())
		{
			uNonPersistentCount++;
		}
	}
	if (uNonPersistentCount <= 1)
	{
		Zenith_Error(LOG_CATEGORY_SCENE,
			"Cannot unload the last loaded scene (handle=%d). Engine requires at least "
			"one non-persistent scene to remain active. To replace it, use "
			"LoadScene(SINGLE) instead of unloading first.",
			xScene.m_iHandle);
		return false;
	}

	return true;
}

void Zenith_SceneSystem::UnloadScene(Zenith_Scene xScene)
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "UnloadScene must be called from main thread");
	Zenith_Assert(!m_bRenderTasksActive, "UnloadScene: scene mutation while render tasks are reading — render-task invariant violated");

	if (!CanUnloadScene(xScene))
		return;

	// UnloadSceneInternal: fire callbacks and select new active scene.
	FireUnloadCallbacksAndSelectNewActive(xScene.m_iHandle, xScene);
}

void Zenith_SceneSystem::UnloadSceneForced(Zenith_Scene xScene)
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "UnloadSceneForced must be called from main thread");

	if (!xScene.IsValid())
		return;

	if (xScene.m_iHandle == m_iPersistentSceneHandle)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "Cannot unload persistent scene");
		return;
	}

	FireUnloadCallbacksAndSelectNewActive(xScene.m_iHandle, xScene);
}

void Zenith_SceneSystem::UnloadUnusedAssets()
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "UnloadUnusedAssets must be called from main thread");

	// Unity-parity: SCENE_LOAD_SINGLE auto-fires this between teardown and the new scene's load.
	// Frees every asset whose refcount has dropped to zero. Leaf-unsafe
	// (AssetHandling): routed through the engine-installed runtime hook so the ECS
	// core never names Zenith_AssetRegistry directly. The concrete UnloadUnused()
	// call lives in the hook lambda set up in Zenith_Engine::Initialise.
	if (m_xRuntimeHooks.m_pfnUnloadUnusedAssets) { m_xRuntimeHooks.m_pfnUnloadUnusedAssets(); }
}

bool Zenith_SceneSystem::HasPendingDestructions()
{
	Zenith_Assert(Zenith_ECS_IsMainThread(),
		"HasPendingDestructions must be called from main thread");
	// True while any loaded scene still has entities marked via Destroy() that
	// the next Update tick has not yet drained. Timed destructions from
	// Destroy(entity, delay) are intentionally excluded — those are scheduled
	// future deletes, not work waiting to drain, and would otherwise pin this
	// true for the whole delay window.
	for (u_int i = 0; i < m_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = m_axScenes.Get(i);
		if (pxData && pxData->IsLoaded() && pxData->m_xPendingDestruction.GetSize() > 0)
		{
			return true;
		}
	}
	return false;
}

void Zenith_SceneSystem::ResetAllRenderSystems()
{
	// Reset all Flux render systems - called during SINGLE mode scene loads
	// to clean up all render state before loading a new scene.
	// Note: This must NOT be called during ADDITIVE loads, as it would
	// destroy render data from other loaded scenes.
	// Leaf-unsafe (Flux): routed through the engine-installed runtime hook so the
	// ECS core never names the renderer directly. The concrete reset sequence
	// (Terrain/Text/Particles/Skybox/Fog + the tools-only Gizmos) lives verbatim
	// in the hook lambda set up in Zenith_Engine::Initialise.
	if (m_xRuntimeHooks.m_pfnResetRenderSystems) { m_xRuntimeHooks.m_pfnResetRenderSystems(); }
}

//=============================================================================
// UnloadAllNonPersistent + private helpers — bulk teardown for
// SCENE_LOAD_SINGLE and engine shutdown paths.
//=============================================================================

void Zenith_SceneSystem::UnloadOneScene(Zenith_Scene xScene, bool& bActiveSceneUnloadedInOut)
{
	if (xScene.m_iHandle == m_iActiveSceneHandle)
	{
		bActiveSceneUnloadedInOut = true;
	}

	delete m_axScenes.Get(xScene.m_iHandle);
	m_axScenes.Get(xScene.m_iHandle) = nullptr;

	FreeSceneHandle(xScene.m_iHandle);
}

Zenith_Vector<Zenith_Scene> Zenith_SceneSystem::CollectNonPersistentScenes(int iExcludeHandle)
{
	Zenith_Vector<Zenith_Scene> axScenes;
	for (u_int i = 0; i < m_axScenes.GetSize(); ++i)
	{
		// D.12: skip the explicit excluded handle.
		if (static_cast<int>(i) == m_iPersistentSceneHandle ||
		    static_cast<int>(i) == iExcludeHandle)
		{
			continue;
		}
		Zenith_SceneData* pxData = m_axScenes.Get(i);
		if (pxData && pxData->IsLoaded())
		{
			Zenith_Scene xScene;
			xScene.m_iHandle = static_cast<int>(i);
			xScene.m_uGeneration = m_axSceneGenerations.Get(i);
			axScenes.PushBack(xScene);
		}
	}
	return axScenes;
}

bool Zenith_SceneSystem::DestroyScenes(const Zenith_Vector<Zenith_Scene>& axScenes)
{
	bool bActiveSceneUnloaded = false;
	for (u_int i = 0; i < axScenes.GetSize(); ++i)
	{
		UnloadOneScene(axScenes.Get(i), bActiveSceneUnloaded);
	}
	return bActiveSceneUnloaded;
}

void Zenith_SceneSystem::UpdateActiveSceneAfterUnload()
{
	// A6: clear the active handle. Do NOT fall back to the persistent scene.
	Zenith_Assert(!m_bRenderTasksActive, "Cannot change active scene while render tasks are in flight");
	m_iActiveSceneHandle = -1;
}

void Zenith_SceneSystem::UnloadAllNonPersistent(int iExcludeHandle)
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "UnloadAllNonPersistent must be called from main thread");
	Zenith_Assert(!m_bRenderTasksActive, "UnloadAllNonPersistent: scene mutation while render tasks are reading — render-task invariant violated");

	const Zenith_Vector<Zenith_Scene> axScenesToUnload = CollectNonPersistentScenes(iExcludeHandle);
	const bool bActiveSceneUnloaded = DestroyScenes(axScenesToUnload);

	if (bActiveSceneUnloaded)
	{
		UpdateActiveSceneAfterUnload();
	}
}
