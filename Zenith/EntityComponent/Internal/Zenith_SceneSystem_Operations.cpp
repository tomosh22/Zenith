#include "Zenith.h"
#include "EntityComponent/Zenith_SceneSystem.h"

#include <algorithm>
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Physics/Zenith_Physics.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Flux/Terrain/Flux_TerrainImpl.h"
#include "Flux/Text/Flux_TextImpl.h"
#include "Flux/Particles/Flux_ParticlesImpl.h"
#include "Flux/Skybox/Flux_SkyboxImpl.h"
#include "Flux/Fog/Flux_FogImpl.h"
#ifdef ZENITH_TOOLS
#include "Flux/Gizmos/Flux_GizmosImpl.h"
#endif

//=============================================================================
// File-scope helpers used by LoadScene only.
//=============================================================================

namespace
{
	// RAII helper that restores m_iPendingBuildIndex on scope exit, so
	// LoadScene aborting mid-call doesn't leak the pending value.
	struct PendingBuildIndexGuard
	{
		int m_iPrev;
		explicit PendingBuildIndexGuard(int iValue) : m_iPrev(g_xEngine.Scenes().m_iPendingBuildIndex)
		{
			g_xEngine.Scenes().m_iPendingBuildIndex = iValue;
		}
		~PendingBuildIndexGuard() { g_xEngine.Scenes().m_iPendingBuildIndex = m_iPrev; }

		PendingBuildIndexGuard(const PendingBuildIndexGuard&) = delete;
		PendingBuildIndexGuard& operator=(const PendingBuildIndexGuard&) = delete;
	};

	bool ValidateLoadRequestInternal(const std::string& strPath)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(), "LoadScene must be called from main thread");
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
// inline, deserialises, dispatches Awake / OnEnable, fires SceneLoaded, and
// returns the scene handle — all before returning. Shutdown body merged
// into Internal/Zenith_SceneSystem_Lifecycle.cpp.
//==========================================================================

Zenith_Scene Zenith_SceneSystem::LoadScene(const std::string& strPath, Zenith_SceneLoadMode eMode)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "LoadScene must be called from main thread");
	Zenith_Assert(!g_xEngine.Scenes().m_bRenderTasksActive,
		"LoadScene: scene mutation while render tasks are reading — render-task invariant violated");

	if (!ValidateLoadRequestInternal(strPath))
		return g_xEngine.Scenes().MakeInvalidScene();

	// Defer if we're inside a frame-update pass (UI iteration, script OnUpdate)
	// OR inside another LoadScene's lifecycle dispatch (Awake/OnEnable/SceneLoaded
	// subscriber). Either case would tear down state the outer pass is still
	// reading. The pending slot drains as the outer pass returns — either via
	// Zenith_SceneUpdateDeferralGuard's destructor, the scheduler's Update
	// post-amble, or LoadScene's own post-dispatch drain below.
	if (g_xEngine.Scenes().m_bIsLoadingScene
		|| g_xEngine.Scenes().m_bIsUpdating)
	{
		auto& xLC = g_xEngine.Scenes();
		xLC.m_xPendingLoad.m_bSet        = true;
		xLC.m_xPendingLoad.m_bByIndex    = false;
		xLC.m_xPendingLoad.m_strPath     = strPath;
		xLC.m_xPendingLoad.m_uMode       = static_cast<uint8_t>(eMode);
		xLC.m_xPendingLoad.m_iBuildIndex = -1;
		return g_xEngine.Scenes().MakeInvalidScene();
	}

	const std::string strCanonicalPath = Zenith_SceneSystem::CanonicalisePath(strPath);
	const int iPendingBuildIndex = g_xEngine.Scenes().m_iPendingBuildIndex;
	g_xEngine.Scenes().m_iPendingBuildIndex = -1;

	// ADDITIVE_WITHOUT_LOADING: create an empty scene with no file I/O.
	if (eMode == SCENE_LOAD_ADDITIVE_WITHOUT_LOADING)
	{
		const std::string strName = ExtractSceneNameFromPath(strCanonicalPath);
		Zenith_Scene xScene = g_xEngine.Scenes().CreateEmptyScene(strName);
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
		pxSceneData->m_strPath = strCanonicalPath;
		if (iPendingBuildIndex >= 0)
			pxSceneData->m_iBuildIndex = iPendingBuildIndex;
		g_xEngine.Scenes().FireSceneLoaded(xScene, SCENE_LOAD_ADDITIVE);
		return xScene;
	}

	// File existence
	if (!Zenith_FileAccess::FileExists(strPath.c_str()))
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "LoadScene: File not found: %s", strPath.c_str());
		return g_xEngine.Scenes().MakeInvalidScene();
	}

	// Circular load guard
	if (g_xEngine.Scenes().IsCircularLoadDependency(strCanonicalPath))
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "Circular scene load detected: %s", strCanonicalPath.c_str());
		return g_xEngine.Scenes().MakeInvalidScene();
	}
	g_xEngine.Scenes().m_axCurrentlyLoadingPaths.PushBack(strCanonicalPath);

	// Read file synchronously on the main thread. Scene files are assumed
	// well-formed (they only come from the editor's known-good save path),
	// so no header or body validation here.
	Zenith_DataStream xLoadedData;
	xLoadedData.ReadFromFile(strPath.c_str());

	g_xEngine.Scenes().m_bIsLoadingScene = true;

	// SINGLE: tear down the old world BEFORE creating the new scene.
	// Components register with their owning subsystem in their ctor / from
	// ReadFromDataStream (ColliderComponent → Jolt body, TerrainComponent
	// → Flux terrain entry, etc.), so a staging-scene pattern (deserialise
	// new before reset old) would wipe the freshly-registered entries.
	// Reset → unload → deserialise keeps the new scene's registrations
	// intact.
	Zenith_Scene xOldActiveSnapshot = Zenith_Scene::INVALID_SCENE;
	if (eMode == SCENE_LOAD_SINGLE)
	{
		xOldActiveSnapshot = g_xEngine.Scenes().GetActiveScene();
		ActiveSceneChangeSuppressionScope xSuppress(xOldActiveSnapshot);
		g_xEngine.Scenes().ResetAllRenderSystems();
		g_xEngine.Scenes().UnloadAllNonPersistent();
		g_xEngine.Scenes().UnloadUnusedAssets();
		g_xEngine.Physics().Reset();
		g_xEngine.Scenes().m_fFixedTimeAccumulator = 0.0f;  // Unity behavior
		xSuppress.Cancel();
	}

	// Create the new scene + deserialise into the post-reset subsystems.
	const std::string strName = ExtractSceneNameFromPath(strCanonicalPath);
	Zenith_Scene xScene = g_xEngine.Scenes().CreateEmptyScene(strName);
	{
		Zenith_SceneCreationTargetScope xCreationTargetScope(xScene);

		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
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
			g_xEngine.Scenes().UnloadSceneForced(xScene);
			g_xEngine.Scenes().m_bIsLoadingScene = false;
			g_xEngine.Scenes().m_axCurrentlyLoadingPaths.EraseValue(strCanonicalPath);
			return g_xEngine.Scenes().MakeInvalidScene();
		}

		// SINGLE: activate the new scene + fire consolidated ActiveSceneChanged.
		if (eMode == SCENE_LOAD_SINGLE)
		{
			Zenith_Assert(!g_xEngine.Scenes().AreRenderTasksActive(),
				"Cannot change active scene while render tasks are in flight");
			g_xEngine.Scenes().m_iActiveSceneHandle = xScene.m_iHandle;
			if (xOldActiveSnapshot != xScene)
				g_xEngine.Scenes().FireActiveSceneChanged(xOldActiveSnapshot, xScene);
		}

		// Unity: Awake → OnEnable → sceneLoaded → Start (next frame).
		// m_bIsLoadingScene is still TRUE through dispatch AND through
		// FireSceneLoaded — a LoadScene call from a SceneLoaded subscriber
		// would otherwise tear down the very scene whose callback list is
		// still being walked. Deferred + drained after FireSceneLoaded
		// returns.
		pxSceneData->DispatchAwakeForNewScene();
		pxSceneData->DispatchEnableAndPendingStartsForNewScene();
		// Transition to LOADED BEFORE SceneLoaded fires so IsActivated() is
		// true for subscribers, but keep m_bIsLoadingScene set so a
		// subscriber's LoadScene call defers rather than recursing.
		pxSceneData->TransitionTo(Zenith_SceneData::SCENE_STATE_LOADED);

		g_xEngine.Scenes().FireSceneLoaded(xScene, eMode);
	}

	// Clear loading-scene flag AFTER SceneLoaded dispatch (kept set above so
	// subscribers couldn't recursively LoadScene mid-dispatch). Drain any
	// stashed request now that the gate is open.
	g_xEngine.Scenes().m_bIsLoadingScene = false;
	g_xEngine.Scenes().m_axCurrentlyLoadingPaths.EraseValue(strCanonicalPath);

	g_xEngine.Scenes().DrainPendingLoadIfAny();

	return xScene;
}

Zenith_Scene Zenith_SceneSystem::LoadSceneByIndex(int iBuildIndex, Zenith_SceneLoadMode eMode)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "LoadSceneByIndex must be called from main thread");

	const u_int uBuildIndex = static_cast<u_int>(iBuildIndex);
	if (iBuildIndex < 0
		|| uBuildIndex >= g_xEngine.Scenes().m_axBuildIndexToPath.GetSize()
		|| g_xEngine.Scenes().m_axBuildIndexToPath.Get(uBuildIndex).empty())
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "LoadSceneByIndex: No scene registered for build index %d", iBuildIndex);
		return g_xEngine.Scenes().MakeInvalidScene();
	}

	// Same re-entrancy guard as LoadScene — defer if mid-update OR mid-load
	// (inside another LoadScene's Awake/OnEnable/SceneLoaded dispatch). We
	// stash the build-index form so the build index is preserved through
	// the drain (a canonical-path lookup would lose it).
	if (g_xEngine.Scenes().m_bIsLoadingScene
		|| g_xEngine.Scenes().m_bIsUpdating)
	{
		auto& xLC = g_xEngine.Scenes();
		xLC.m_xPendingLoad.m_bSet        = true;
		xLC.m_xPendingLoad.m_bByIndex    = true;
		xLC.m_xPendingLoad.m_iBuildIndex = iBuildIndex;
		xLC.m_xPendingLoad.m_strPath.clear();
		xLC.m_xPendingLoad.m_uMode       = static_cast<uint8_t>(eMode);
		return g_xEngine.Scenes().MakeInvalidScene();
	}

	// Stash the build index so LoadScene picks it up and stamps the new scene.
	PendingBuildIndexGuard xGuard(iBuildIndex);
	return LoadScene(g_xEngine.Scenes().m_axBuildIndexToPath.Get(uBuildIndex), eMode);
}

//=============================================================================
// Unload entry points + helpers.
//=============================================================================

bool Zenith_SceneSystem::CanUnloadScene(Zenith_Scene xScene)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "CanUnloadScene must be called from main thread");

	if (!xScene.IsValid())
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "CanUnloadScene: Invalid scene");
		return false;
	}

	if (xScene.m_iHandle == g_xEngine.Scenes().m_iPersistentSceneHandle)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "Cannot unload persistent scene");
		return false;
	}

	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	if (pxSceneData && pxSceneData->IsUnloading())
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "Cannot unload scene that is already being unloaded");
		return false;
	}

	// Cannot unload the last loaded scene (Unity behavior).
	uint32_t uNonPersistentCount = 0;
	for (u_int i = 0; i < g_xEngine.Scenes().m_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = g_xEngine.Scenes().m_axScenes.Get(i);
		if (pxData != nullptr && static_cast<int>(i) != g_xEngine.Scenes().m_iPersistentSceneHandle
			&& pxData->IsLoaded() && pxData->IsActivated() && !pxData->IsUnloading())
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
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "UnloadScene must be called from main thread");
	Zenith_Assert(!g_xEngine.Scenes().m_bRenderTasksActive, "UnloadScene: scene mutation while render tasks are reading — render-task invariant violated");

	if (!CanUnloadScene(xScene))
		return;

	// UnloadSceneInternal: fire callbacks and select new active scene.
	g_xEngine.Scenes().FireUnloadCallbacksAndSelectNewActive(xScene.m_iHandle, xScene);
}

void Zenith_SceneSystem::UnloadSceneForced(Zenith_Scene xScene)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "UnloadSceneForced must be called from main thread");

	if (!xScene.IsValid())
		return;

	if (xScene.m_iHandle == g_xEngine.Scenes().m_iPersistentSceneHandle)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "Cannot unload persistent scene");
		return;
	}

	g_xEngine.Scenes().FireUnloadCallbacksAndSelectNewActive(xScene.m_iHandle, xScene);
}

void Zenith_SceneSystem::UnloadUnusedAssets()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "UnloadUnusedAssets must be called from main thread");

#ifdef ZENITH_TESTING
	++g_xEngine.Scenes().m_uUnloadUnusedAssetsCallCount;
#endif

	// Unity-parity: SCENE_LOAD_SINGLE auto-fires this between teardown and the new scene's load.
	// Frees every asset whose refcount has dropped to zero.
	Zenith_AssetRegistry::UnloadUnused();
}

bool Zenith_SceneSystem::HasPendingDestructions()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(),
		"HasPendingDestructions must be called from main thread");
	// Sync scene system: all destruction completes within the calling stack
	// frame. No deferred work left to drain.
	return false;
}

void Zenith_SceneSystem::ResetAllRenderSystems()
{
	// Reset all Flux render systems - called during SINGLE mode scene loads
	// to clean up all render state before loading a new scene.
	// Note: This must NOT be called during ADDITIVE loads, as it would
	// destroy render data from other loaded scenes.
	g_xEngine.Terrain().Reset();
	g_xEngine.Text().Reset();
	g_xEngine.Particles().Reset();
	g_xEngine.Skybox().Reset();
	g_xEngine.Fog().Reset();
#ifdef ZENITH_TOOLS
	g_xEngine.Gizmos().Reset();
#endif
}

//=============================================================================
// UnloadAllNonPersistent + private helpers — bulk teardown for
// SCENE_LOAD_SINGLE and engine shutdown paths.
//=============================================================================

void Zenith_SceneSystem::UnloadOneScene(Zenith_Scene xScene, bool& bActiveSceneUnloadedInOut)
{
	if (xScene.m_iHandle == g_xEngine.Scenes().m_iActiveSceneHandle)
	{
		bActiveSceneUnloadedInOut = true;
	}

	delete g_xEngine.Scenes().m_axScenes.Get(xScene.m_iHandle);
	g_xEngine.Scenes().m_axScenes.Get(xScene.m_iHandle) = nullptr;

	// Fire unloaded callback BEFORE incrementing generation so the handle
	// is still valid for identification in callbacks (Unity parity).
	g_xEngine.Scenes().FireSceneUnloaded(xScene);

	g_xEngine.Scenes().FreeSceneHandle(xScene.m_iHandle);
}

Zenith_Vector<Zenith_Scene> Zenith_SceneSystem::CollectNonPersistentScenes(int iExcludeHandle)
{
	Zenith_Vector<Zenith_Scene> axScenes;
	for (u_int i = 0; i < g_xEngine.Scenes().m_axScenes.GetSize(); ++i)
	{
		// D.12: skip the explicit excluded handle.
		if (static_cast<int>(i) == g_xEngine.Scenes().m_iPersistentSceneHandle ||
		    static_cast<int>(i) == iExcludeHandle)
		{
			continue;
		}
		Zenith_SceneData* pxData = g_xEngine.Scenes().m_axScenes.Get(i);
		if (pxData && pxData->IsLoaded())
		{
			Zenith_Scene xScene;
			xScene.m_iHandle = static_cast<int>(i);
			xScene.m_uGeneration = g_xEngine.Scenes().m_axSceneGenerations.Get(i);
			axScenes.PushBack(xScene);
		}
	}
	return axScenes;
}

bool Zenith_SceneSystem::DestroyScenesAndFireUnloaded(const Zenith_Vector<Zenith_Scene>& axScenes,
	const Zenith_HashSet<int>& xAlreadyFired)
{
	// Phase 1: fire SceneUnloading BEFORE any destruction so cleanup code can
	// still touch scene data.
	for (u_int i = 0; i < axScenes.GetSize(); ++i)
	{
		const Zenith_Scene& xScene = axScenes.Get(i);
		if (!xAlreadyFired.Contains(xScene.m_iHandle))
		{
			g_xEngine.Scenes().FireSceneUnloading(xScene);
		}
	}

	// Phase 2: destroy + fire SceneUnloaded.
	bool bActiveSceneUnloaded = false;
	for (u_int i = 0; i < axScenes.GetSize(); ++i)
	{
		UnloadOneScene(axScenes.Get(i), bActiveSceneUnloaded);
	}
	return bActiveSceneUnloaded;
}

void Zenith_SceneSystem::UpdateActiveSceneAfterUnload(Zenith_Scene xOldActive)
{
	// A6: clear the active handle. Do NOT fall back to the persistent scene.
	Zenith_Assert(!g_xEngine.Scenes().m_bRenderTasksActive, "Cannot change active scene while render tasks are in flight");
	g_xEngine.Scenes().m_iActiveSceneHandle = -1;
	Zenith_Scene xNewActive = g_xEngine.Scenes().GetActiveScene();  // will be INVALID_SCENE
	if (xOldActive == xNewActive)
	{
		return;
	}
	// A5: suppress during SINGLE-load teardown.
	if (g_xEngine.Scenes().IsActiveSceneSuppressed())
	{
		if (!g_xEngine.Scenes().HasDeferredOldActive())
		{
			g_xEngine.Scenes().SetDeferredOldActive(xOldActive);
		}
	}
	else
	{
		g_xEngine.Scenes().FireActiveSceneChanged(xOldActive, xNewActive);
	}
}

void Zenith_SceneSystem::UnloadAllNonPersistent(int iExcludeHandle)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "UnloadAllNonPersistent must be called from main thread");
	Zenith_Assert(!g_xEngine.Scenes().m_bRenderTasksActive, "UnloadAllNonPersistent: scene mutation while render tasks are reading — render-task invariant violated");

	// Capture active scene BEFORE destruction (FreeSceneHandle increments generation).
	const Zenith_Scene xOldActive = g_xEngine.Scenes().GetActiveScene();
	const Zenith_Vector<Zenith_Scene> axScenesToUnload = CollectNonPersistentScenes(iExcludeHandle);
	const Zenith_HashSet<int> xAlreadyFired;  // sync path never pre-fires SceneUnloading
	const bool bActiveSceneUnloaded = DestroyScenesAndFireUnloaded(axScenesToUnload, xAlreadyFired);

	if (bActiveSceneUnloaded)
	{
		UpdateActiveSceneAfterUnload(xOldActive);
	}
}
