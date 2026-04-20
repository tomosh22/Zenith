#include "Zenith.h"

#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneData.h"

//==========================================================================
// Zenith_SceneManager — scene queries and build-index registry
//
// Carved out of Zenith_SceneManager.cpp to keep the main TU focused on
// lifecycle (load/unload/update). The functions here are all static queries
// over SceneManager state — no scene mutation, no lifecycle callbacks.
// Everything operates on the statics declared in the main TU, which are
// accessible across translation units because they're class members.
//==========================================================================

//==========================================================================
// Scene Count Queries
//==========================================================================

uint32_t Zenith_SceneManager::GetLoadedSceneCount()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetLoadedSceneCount must be called from main thread");
	uint32_t uCount = 0;
	for (u_int i = 0; i < s_axScenes.GetSize(); ++i)
	{
		if (IsSceneVisibleToUser(i, s_axScenes.Get(i)))
			uCount++;
	}
	// B6: return the real count. Previously this clamped to `max(1, count)` to imitate
	// Unity's "always at least one scene", but Zenith permits a transient zero state
	// (e.g. after UnloadAllNonPersistent, before the next LoadScene) — lying to callers
	// caused them to iterate GetSceneAt(0) on an invalid index.
	return uCount;
}

uint32_t Zenith_SceneManager::GetTotalSceneCount()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetTotalSceneCount must be called from main thread");
	uint32_t uCount = 0;
	for (u_int i = 0; i < s_axScenes.GetSize(); ++i)
	{
		if (s_axScenes.Get(i))
		{
			uCount++;
		}
	}
	return uCount;
}

uint32_t Zenith_SceneManager::GetBuildSceneCount()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetBuildSceneCount must be called from main thread");
	uint32_t uCount = 0;
	for (u_int i = 0; i < s_axBuildIndexToPath.GetSize(); ++i)
	{
		if (!s_axBuildIndexToPath.Get(i).empty()) uCount++;
	}
	return uCount;
}

//==========================================================================
// Scene Queries
//==========================================================================

Zenith_Scene Zenith_SceneManager::GetActiveScene()
{
	// Safe to call from worker threads during render task execution.
	// All active scene changes complete before render tasks are submitted,
	// and the task system's queue mutex provides the happens-before
	// relationship for memory visibility on worker threads.
	Zenith_Assert(Zenith_Multithreading::IsMainThread() || s_bRenderTasksActive, "GetActiveScene must be called from main thread or during render task execution");
	const int iHandle = s_iActiveSceneHandle;
	Zenith_Scene xScene;
	xScene.m_iHandle = iHandle;
	if (iHandle >= 0 && iHandle < static_cast<int>(s_axSceneGenerations.GetSize()))
	{
		xScene.m_uGeneration = s_axSceneGenerations.Get(iHandle);
	}
	return xScene;
}

Zenith_Scene Zenith_SceneManager::GetSceneAt(uint32_t uIndex)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetSceneAt must be called from main thread");
	Zenith_Scene xScene = MakeInvalidScene();

	uint32_t uCurrent = 0;
	for (u_int i = 0; i < s_axScenes.GetSize(); ++i)
	{
		if (!IsSceneVisibleToUser(i, s_axScenes.Get(i)))
			continue;

		if (uCurrent == uIndex)
		{
			xScene.m_iHandle = static_cast<int>(i);
			xScene.m_uGeneration = s_axSceneGenerations.Get(i);
			return xScene;
		}
		uCurrent++;
	}
	return xScene;
}

Zenith_Scene Zenith_SceneManager::GetSceneByBuildIndex(int iBuildIndex)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetSceneByBuildIndex must be called from main thread");
	Zenith_Scene xScene = MakeInvalidScene();

	for (u_int i = 0; i < s_axScenes.GetSize(); ++i)
	{
		if (s_axScenes.Get(i) && s_axScenes.Get(i)->m_iBuildIndex == iBuildIndex
			&& s_axScenes.Get(i)->m_bIsLoaded && !s_axScenes.Get(i)->m_bIsUnloading)
		{
			xScene.m_iHandle = static_cast<int>(i);
			xScene.m_uGeneration = s_axSceneGenerations.Get(i);
			return xScene;
		}
	}
	return xScene;
}

Zenith_Scene Zenith_SceneManager::GetSceneByName(const std::string& strName)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetSceneByName must be called from main thread");
	Zenith_Scene xScene = MakeInvalidScene();
	int iFirstMatchHandle = -1;
	bool bAmbiguous = false;

	// Use name cache for O(n) scan over loaded scenes only (smaller than all scene slots)
	for (u_int i = 0; i < s_axLoadedSceneNames.GetSize(); ++i)
	{
		const SceneNameEntry& xEntry = s_axLoadedSceneNames.Get(i);
		const int iHandle = xEntry.m_iHandle;

		// Verify scene is still valid and loaded
		Zenith_SceneData* pxData = (iHandle >= 0 && iHandle < static_cast<int>(s_axScenes.GetSize())) ? s_axScenes.Get(iHandle) : nullptr;
		if (!pxData || !pxData->m_bIsLoaded || pxData->m_bIsUnloading) continue;

		const std::string& strSceneName = xEntry.m_strName;
		bool bMatched = false;

		// Exact match
		if (strSceneName == strName)
		{
			bMatched = true;
		}
		else
		{
			// Unity: Also match by filename without path/extension
			// e.g., "MyScene" matches "Levels/MyScene.zscen"
			size_t uLastSlash = strSceneName.find_last_of("/\\");
			size_t uStart = (uLastSlash == std::string::npos) ? 0 : uLastSlash + 1;
			size_t uLastDot = strSceneName.find_last_of('.');
			size_t uEnd = (uLastDot == std::string::npos || uLastDot < uStart) ?
				strSceneName.size() : uLastDot;

			std::string strBaseName = strSceneName.substr(uStart, uEnd - uStart);
			if (strBaseName == strName)
			{
				bMatched = true;
			}
		}

		if (bMatched)
		{
			if (iFirstMatchHandle < 0)
			{
				iFirstMatchHandle = iHandle;
			}
			else
			{
				bAmbiguous = true;
				break;
			}
		}
	}

	if (bAmbiguous)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE,
			"GetSceneByName('%s'): more than one loaded scene matches; returning first. "
			"Use GetSceneByPath for unique lookup.", strName.c_str());
	}

	if (iFirstMatchHandle >= 0)
	{
		xScene.m_iHandle = iFirstMatchHandle;
		xScene.m_uGeneration = s_axSceneGenerations.Get(iFirstMatchHandle);
	}
	return xScene;
}

//==========================================================================
// Build Settings Registry
//==========================================================================

void Zenith_SceneManager::RegisterSceneBuildIndex(int iBuildIndex, const std::string& strPath)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "RegisterSceneBuildIndex must be called from main thread");

	Zenith_Assert(iBuildIndex >= 0, "RegisterSceneBuildIndex: Build index must be non-negative");
	const u_int uBuildIndex = static_cast<u_int>(iBuildIndex);
	if (uBuildIndex < s_axBuildIndexToPath.GetSize() && !s_axBuildIndexToPath.Get(uBuildIndex).empty()
		&& s_axBuildIndexToPath.Get(uBuildIndex) != strPath)
	{
		Zenith_Assert(false, "RegisterSceneBuildIndex: Build index %d already registered for '%s', cannot register for '%s'",
			iBuildIndex, s_axBuildIndexToPath.Get(uBuildIndex).c_str(), strPath.c_str());
		Zenith_Error(LOG_CATEGORY_SCENE,
			"RegisterSceneBuildIndex: Build index %d already registered - ignoring duplicate",
			iBuildIndex);
		return;  // Don't overwrite existing registration
	}
	while (s_axBuildIndexToPath.GetSize() <= uBuildIndex) s_axBuildIndexToPath.PushBack(std::string());
	s_axBuildIndexToPath.Get(uBuildIndex) = strPath;
	Zenith_Log(LOG_CATEGORY_SCENE, "Registered scene build index %d -> %s", iBuildIndex, strPath.c_str());
}

void Zenith_SceneManager::ClearBuildIndexRegistry()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "ClearBuildIndexRegistry must be called from main thread");

	s_axBuildIndexToPath.Clear();

	// C12: Reset m_iBuildIndex on every loaded scene. Previously ClearBuildIndexRegistry
	// wiped the path→index map but left loaded scenes with stale build-index values,
	// so subsequent lookups via GetRegisteredScenePath(scene.GetBuildIndex()) would
	// return empty strings or nonsense. Now the two are kept in sync.
	uint32_t uScenesCleared = 0;
	for (u_int i = 0; i < s_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = s_axScenes.Get(i);
		if (pxData && pxData->m_iBuildIndex >= 0)
		{
			pxData->m_iBuildIndex = -1;
			++uScenesCleared;
		}
	}

	Zenith_Log(LOG_CATEGORY_SCENE, "Cleared scene build index registry (reset %u loaded-scene indices)", uScenesCleared);
}

static const std::string s_strEmptyBuildPath;

const std::string& Zenith_SceneManager::GetRegisteredScenePath(int iBuildIndex)
{
	const u_int uBuildIndex = static_cast<u_int>(iBuildIndex);
	if (iBuildIndex >= 0 && uBuildIndex < s_axBuildIndexToPath.GetSize())
	{
		return s_axBuildIndexToPath.Get(uBuildIndex);
	}
	return s_strEmptyBuildPath;
}

uint32_t Zenith_SceneManager::GetBuildIndexRegistrySize()
{
	return s_axBuildIndexToPath.GetSize();
}
