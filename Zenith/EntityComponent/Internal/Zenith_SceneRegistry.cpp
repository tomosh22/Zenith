#include "Zenith.h"

#include "EntityComponent/Internal/Zenith_SceneRegistry.h"
#include "EntityComponent/Internal/Zenith_SceneRegistryImpl.h"
#include "EntityComponent/Zenith_SceneManager.h"

//=============================================================================
// Zenith_SceneRegistry — implementations and static-storage definitions.
//
// State is owned by this class post-A2b. Manager TUs reference
// Zenith_SceneRegistry::s_* directly. The friend-class name-injection issue
// that blocked the earlier A2b attempt is sidestepped by:
//   1. Registry.h has no #include of manager.h (one-way include).
//   2. manager.h includes registry.h BEFORE SceneData.h, so SceneData's in-class
//      `friend class Zenith_SceneRegistry` refers to the already-complete class
//      rather than injecting an incomplete forward declaration.
//   3. The manager's GetAllOfComponentTypeFromAllScenes template uses public
//      registry accessors (GetSceneSlotCount / GetLoadedSceneDataAtSlot) so it
//      doesn't depend on storage layout.
//=============================================================================

// Phase 5b: registry state lives on Zenith_SceneRegistryImpl
// (held by Zenith_Engine as m_pxSceneRegistry). Every method body below
// reads/writes through g_xEngine.SceneRegistry().m_xXxx.

void Zenith_SceneRegistry::Shutdown()
{
	Zenith_SceneRegistryImpl& xR = g_xEngine.SceneRegistry();
	for (u_int i = 0; i < xR.m_axScenes.GetSize(); ++i)
	{
		delete xR.m_axScenes.Get(i);
	}
	xR.m_axScenes.Clear();
	xR.m_axSceneGenerations.Clear();
	xR.m_axFreeHandles.Clear();
	xR.m_axBuildIndexToPath.Clear();
	xR.m_axLoadedSceneNames.Clear();
	xR.m_iActiveSceneHandle = -1;
	xR.m_iPersistentSceneHandle = -1;
	xR.m_ulNextLoadTimestamp = 1;
}

//=============================================================================
// Slot management
//=============================================================================

int Zenith_SceneRegistry::AllocateSceneHandle()
{
	if (g_xEngine.SceneRegistry().m_axFreeHandles.GetSize() > 0)
	{
		int iHandle = g_xEngine.SceneRegistry().m_axFreeHandles.GetBack();
		g_xEngine.SceneRegistry().m_axFreeHandles.PopBack();
		return iHandle;
	}
	int iNewHandle = static_cast<int>(g_xEngine.SceneRegistry().m_axScenes.GetSize());
	g_xEngine.SceneRegistry().m_axSceneGenerations.PushBack(1);
	return iNewHandle;
}

void Zenith_SceneRegistry::FreeSceneHandle(int iHandle)
{
	if (iHandle < 0 || iHandle >= static_cast<int>(g_xEngine.SceneRegistry().m_axSceneGenerations.GetSize()))
	{
		return;
	}

	// D1: double-free guard. Callers must `delete pxSceneData` and null the slot
	// BEFORE calling FreeSceneHandle — otherwise the generation bump here releases
	// the handle while the live scene data is still reachable via g_xEngine.SceneRegistry().m_axScenes,
	// corrupting subsequent loads into the same slot.
	Zenith_Assert(iHandle >= static_cast<int>(g_xEngine.SceneRegistry().m_axScenes.GetSize()) ||
	              g_xEngine.SceneRegistry().m_axScenes.Get(iHandle) == nullptr,
		"FreeSceneHandle(%d): scene data is still non-null — caller forgot to delete + null before releasing the handle",
		iHandle);

	RemoveFromSceneNameCache(iHandle);

	uint32_t& uGen = g_xEngine.SceneRegistry().m_axSceneGenerations.Get(iHandle);
	if (uGen < UINT32_MAX - 1)
	{
		uGen++;
		g_xEngine.SceneRegistry().m_axFreeHandles.PushBack(iHandle);
	}
	else
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "Scene handle %d retired due to generation overflow", iHandle);
	}
}

uint32_t Zenith_SceneRegistry::GetSceneSlotCount()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread() || Zenith_SceneManager::AreRenderTasksActive(),
		"GetSceneSlotCount must be called from main thread or during render task execution");
	return g_xEngine.SceneRegistry().m_axScenes.GetSize();
}

Zenith_SceneData* Zenith_SceneRegistry::GetSceneDataAtSlot(uint32_t uIndex)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread() || Zenith_SceneManager::AreRenderTasksActive(),
		"GetSceneDataAtSlot must be called from main thread or during render task execution");
	if (uIndex >= g_xEngine.SceneRegistry().m_axScenes.GetSize())
		return nullptr;
	return g_xEngine.SceneRegistry().m_axScenes.Get(uIndex);
}

Zenith_SceneData* Zenith_SceneRegistry::GetLoadedSceneDataAtSlot(uint32_t uIndex)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread() || Zenith_SceneManager::AreRenderTasksActive(),
		"GetLoadedSceneDataAtSlot must be called from main thread or during render task execution");
	if (uIndex >= g_xEngine.SceneRegistry().m_axScenes.GetSize())
		return nullptr;
	Zenith_SceneData* pxData = g_xEngine.SceneRegistry().m_axScenes.Get(uIndex);
	if (!pxData || !pxData->IsLoaded() || pxData->IsUnloading())
		return nullptr;
	return pxData;
}

//=============================================================================
// Scene handle/data resolution
//=============================================================================

Zenith_Scene Zenith_SceneRegistry::MakeInvalidScene()
{
	Zenith_Scene xScene;
	xScene.m_iHandle = -1;
	xScene.m_uGeneration = 0;
	return xScene;
}

Zenith_SceneData* Zenith_SceneRegistry::GetSceneData(Zenith_Scene xScene)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread() || Zenith_SceneManager::AreRenderTasksActive(),
		"GetSceneData must be called from the main thread or during render task execution");
	if (xScene.m_iHandle < 0 || xScene.m_iHandle >= static_cast<int>(g_xEngine.SceneRegistry().m_axScenes.GetSize()))
	{
		return nullptr;
	}
	if (xScene.m_uGeneration != g_xEngine.SceneRegistry().m_axSceneGenerations.Get(xScene.m_iHandle))
	{
		return nullptr;
	}
	return g_xEngine.SceneRegistry().m_axScenes.Get(xScene.m_iHandle);
}

Zenith_SceneData* Zenith_SceneRegistry::GetSceneDataByHandle(int iHandle)
{
	if (iHandle < 0 || iHandle >= static_cast<int>(g_xEngine.SceneRegistry().m_axScenes.GetSize()))
	{
		return nullptr;
	}
	return g_xEngine.SceneRegistry().m_axScenes.Get(iHandle);
}

Zenith_SceneData* Zenith_SceneRegistry::GetSceneDataForEntity(Zenith_EntityID xID)
{
	if (!xID.IsValid()) return nullptr;
	if (xID.m_uIndex >= g_xEngine.EntityStore().m_axEntitySlots.GetSize()) return nullptr;
	const Zenith_SceneData::Zenith_EntitySlot& xSlot = g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex);
	if (!xSlot.IsOccupied() || xSlot.m_uGeneration != xID.m_uGeneration) return nullptr;
	return GetSceneDataByHandle(xSlot.m_iSceneHandle);
}

Zenith_Scene Zenith_SceneRegistry::GetSceneFromHandle(int iHandle)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetSceneFromHandle must be called from main thread");
	Zenith_Scene xScene;
	xScene.m_iHandle = iHandle;
	xScene.m_uGeneration = 0;
	if (iHandle >= 0 && iHandle < static_cast<int>(g_xEngine.SceneRegistry().m_axSceneGenerations.GetSize()))
	{
		xScene.m_uGeneration = g_xEngine.SceneRegistry().m_axSceneGenerations.Get(iHandle);
	}
	return xScene;
}

//=============================================================================
// Scene queries
//=============================================================================

Zenith_Scene Zenith_SceneRegistry::GetActiveScene()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread() || Zenith_SceneManager::AreRenderTasksActive(),
		"GetActiveScene must be called from main thread or during render task execution");
	const int iHandle = g_xEngine.SceneRegistry().m_iActiveSceneHandle;
	Zenith_Scene xScene;
	xScene.m_iHandle = iHandle;
	if (iHandle >= 0 && iHandle < static_cast<int>(g_xEngine.SceneRegistry().m_axSceneGenerations.GetSize()))
	{
		xScene.m_uGeneration = g_xEngine.SceneRegistry().m_axSceneGenerations.Get(iHandle);
	}
	return xScene;
}

Zenith_Scene Zenith_SceneRegistry::GetSceneAt(uint32_t uIndex)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetSceneAt must be called from main thread");
	Zenith_Scene xScene = MakeInvalidScene();

	uint32_t uCurrent = 0;
	for (u_int i = 0; i < g_xEngine.SceneRegistry().m_axScenes.GetSize(); ++i)
	{
		if (!IsSceneVisibleToUser(i, g_xEngine.SceneRegistry().m_axScenes.Get(i)))
			continue;

		if (uCurrent == uIndex)
		{
			xScene.m_iHandle = static_cast<int>(i);
			xScene.m_uGeneration = g_xEngine.SceneRegistry().m_axSceneGenerations.Get(i);
			return xScene;
		}
		uCurrent++;
	}
	return xScene;
}

Zenith_Scene Zenith_SceneRegistry::GetSceneByBuildIndex(int iBuildIndex)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetSceneByBuildIndex must be called from main thread");
	Zenith_Scene xScene = MakeInvalidScene();
	for (u_int i = 0; i < g_xEngine.SceneRegistry().m_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = g_xEngine.SceneRegistry().m_axScenes.Get(i);
		if (pxData && pxData->m_iBuildIndex == iBuildIndex && pxData->m_bIsLoaded && !pxData->m_bIsUnloading)
		{
			xScene.m_iHandle = static_cast<int>(i);
			xScene.m_uGeneration = g_xEngine.SceneRegistry().m_axSceneGenerations.Get(i);
			return xScene;
		}
	}
	return xScene;
}

Zenith_Scene Zenith_SceneRegistry::GetSceneByName(const std::string& strName)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetSceneByName must be called from main thread");
	Zenith_Scene xScene = MakeInvalidScene();
	int iFirstMatchHandle = -1;
	bool bAmbiguous = false;

	for (u_int i = 0; i < g_xEngine.SceneRegistry().m_axLoadedSceneNames.GetSize(); ++i)
	{
		const SceneNameEntry& xEntry = g_xEngine.SceneRegistry().m_axLoadedSceneNames.Get(i);
		const int iHandle = xEntry.m_iHandle;

		Zenith_SceneData* pxData = (iHandle >= 0 && iHandle < static_cast<int>(g_xEngine.SceneRegistry().m_axScenes.GetSize()))
			? g_xEngine.SceneRegistry().m_axScenes.Get(iHandle) : nullptr;
		if (!pxData || !pxData->m_bIsLoaded || pxData->m_bIsUnloading) continue;

		const std::string& strSceneName = xEntry.m_strName;
		bool bMatched = false;

		if (strSceneName == strName)
		{
			bMatched = true;
		}
		else
		{
			size_t uLastSlash = strSceneName.find_last_of("/\\");
			size_t uStart = (uLastSlash == std::string::npos) ? 0 : uLastSlash + 1;
			size_t uLastDot = strSceneName.find_last_of('.');
			size_t uEnd = (uLastDot == std::string::npos || uLastDot < uStart) ?
				strSceneName.size() : uLastDot;
			std::string strBaseName = strSceneName.substr(uStart, uEnd - uStart);
			if (strBaseName == strName) bMatched = true;
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
		xScene.m_uGeneration = g_xEngine.SceneRegistry().m_axSceneGenerations.Get(iFirstMatchHandle);
	}
	return xScene;
}

// Caller passes an already-canonicalized scene path. The public
// Zenith_SceneManager::GetSceneByPath forwarder runs the canonicalization
// (it lives in Zenith_SceneManager.cpp's file-static CanonicalizeScenePath
// helper alongside the LoadScene path validation that uses the same helper).
Zenith_Scene Zenith_SceneRegistry::GetSceneByPath(const std::string& strCanonicalPath)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetSceneByPath must be called from main thread");
	Zenith_Scene xScene = MakeInvalidScene();

	for (u_int i = 0; i < g_xEngine.SceneRegistry().m_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = g_xEngine.SceneRegistry().m_axScenes.Get(i);
		if (pxData && pxData->m_bIsLoaded && !pxData->m_bIsUnloading && pxData->m_strPath == strCanonicalPath)
		{
			xScene.m_iHandle = static_cast<int>(i);
			xScene.m_uGeneration = g_xEngine.SceneRegistry().m_axSceneGenerations.Get(i);
			return xScene;
		}
	}
	return xScene;
}

uint32_t Zenith_SceneRegistry::GetLoadedSceneCount()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetLoadedSceneCount must be called from main thread");
	uint32_t uCount = 0;
	for (u_int i = 0; i < g_xEngine.SceneRegistry().m_axScenes.GetSize(); ++i)
	{
		if (IsSceneVisibleToUser(i, g_xEngine.SceneRegistry().m_axScenes.Get(i))) uCount++;
	}
	return uCount;
}

uint32_t Zenith_SceneRegistry::GetTotalSceneCount()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetTotalSceneCount must be called from main thread");
	uint32_t uCount = 0;
	for (u_int i = 0; i < g_xEngine.SceneRegistry().m_axScenes.GetSize(); ++i)
	{
		if (g_xEngine.SceneRegistry().m_axScenes.Get(i)) uCount++;
	}
	return uCount;
}

uint32_t Zenith_SceneRegistry::GetBuildSceneCount()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetBuildSceneCount must be called from main thread");
	uint32_t uCount = 0;
	for (u_int i = 0; i < g_xEngine.SceneRegistry().m_axBuildIndexToPath.GetSize(); ++i)
	{
		if (!g_xEngine.SceneRegistry().m_axBuildIndexToPath.Get(i).empty()) uCount++;
	}
	return uCount;
}

//=============================================================================
// Active / persistent
//=============================================================================

int Zenith_SceneRegistry::SelectNewActiveScene(int iExcludeHandle)
{
	int iBestHandle = -1;
	int iBestBuildIndex = -1;
	uint64_t ulBestTimestamp = 0;

	for (u_int i = 0; i < g_xEngine.SceneRegistry().m_axScenes.GetSize(); ++i)
	{
		if (static_cast<int>(i) == g_xEngine.SceneRegistry().m_iPersistentSceneHandle) continue;
		if (static_cast<int>(i) == iExcludeHandle) continue;

		Zenith_SceneData* pxCandidate = g_xEngine.SceneRegistry().m_axScenes.Get(i);
		if (!pxCandidate || !pxCandidate->m_bIsLoaded || pxCandidate->m_bIsUnloading) continue;

		if (pxCandidate->m_iBuildIndex >= 0)
		{
			if (iBestBuildIndex < 0 || pxCandidate->m_iBuildIndex < iBestBuildIndex)
			{
				iBestBuildIndex = pxCandidate->m_iBuildIndex;
				iBestHandle = static_cast<int>(i);
			}
		}
		else if (iBestBuildIndex < 0 && pxCandidate->m_ulLoadTimestamp > ulBestTimestamp)
		{
			ulBestTimestamp = pxCandidate->m_ulLoadTimestamp;
			iBestHandle = static_cast<int>(i);
		}
	}

	if (iBestHandle < 0)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE,
			"SelectNewActiveScene: no eligible non-persistent scene — active scene is now invalid. "
			"GetActiveScene() will return INVALID_SCENE until a new scene is loaded.");
	}
	return iBestHandle;
}

Zenith_Scene Zenith_SceneRegistry::GetPersistentScene()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetPersistentScene must be called from main thread");
	return GetSceneFromHandle(g_xEngine.SceneRegistry().m_iPersistentSceneHandle);
}

//=============================================================================
// Visibility / update predicates
//=============================================================================

bool Zenith_SceneRegistry::IsSceneVisibleToUser(u_int uSlotIndex, const Zenith_SceneData* pxData)
{
	if (!pxData || !pxData->m_bIsLoaded || !pxData->m_bIsActivated || pxData->m_bIsUnloading)
		return false;
	if (static_cast<int>(uSlotIndex) == g_xEngine.SceneRegistry().m_iPersistentSceneHandle && pxData->GetEntityCount() == 0)
		return false;
	return true;
}

bool Zenith_SceneRegistry::IsSceneUpdatable(const Zenith_SceneData* pxData)
{
	return pxData && pxData->m_bIsLoaded && pxData->m_bIsActivated && !pxData->m_bIsUnloading && !pxData->IsPaused();
}

//=============================================================================
// Name cache
//=============================================================================

void Zenith_SceneRegistry::AddToSceneNameCache(int iHandle, const std::string& strName)
{
	SceneNameEntry xEntry;
	xEntry.m_strName = strName;
	xEntry.m_iHandle = iHandle;
	g_xEngine.SceneRegistry().m_axLoadedSceneNames.PushBack(std::move(xEntry));
}

void Zenith_SceneRegistry::RemoveFromSceneNameCache(int iHandle)
{
	for (u_int i = 0; i < g_xEngine.SceneRegistry().m_axLoadedSceneNames.GetSize(); ++i)
	{
		if (g_xEngine.SceneRegistry().m_axLoadedSceneNames.Get(i).m_iHandle == iHandle)
		{
			g_xEngine.SceneRegistry().m_axLoadedSceneNames.RemoveSwap(i);
			return;
		}
	}
}

//=============================================================================
// Build-index registry
//=============================================================================

void Zenith_SceneRegistry::RegisterSceneBuildIndex(int iBuildIndex, const std::string& strPath)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "RegisterSceneBuildIndex must be called from main thread");
	Zenith_Assert(iBuildIndex >= 0, "RegisterSceneBuildIndex: Build index must be non-negative");

	const u_int uBuildIndex = static_cast<u_int>(iBuildIndex);
	if (uBuildIndex < g_xEngine.SceneRegistry().m_axBuildIndexToPath.GetSize()
		&& !g_xEngine.SceneRegistry().m_axBuildIndexToPath.Get(uBuildIndex).empty()
		&& g_xEngine.SceneRegistry().m_axBuildIndexToPath.Get(uBuildIndex) != strPath)
	{
		Zenith_Assert(false,
			"RegisterSceneBuildIndex: Build index %d already registered for '%s', cannot register for '%s'",
			iBuildIndex, g_xEngine.SceneRegistry().m_axBuildIndexToPath.Get(uBuildIndex).c_str(), strPath.c_str());
		Zenith_Error(LOG_CATEGORY_SCENE,
			"RegisterSceneBuildIndex: Build index %d already registered - ignoring duplicate", iBuildIndex);
		return;
	}
	while (g_xEngine.SceneRegistry().m_axBuildIndexToPath.GetSize() <= uBuildIndex)
		g_xEngine.SceneRegistry().m_axBuildIndexToPath.PushBack(std::string());
	g_xEngine.SceneRegistry().m_axBuildIndexToPath.Get(uBuildIndex) = strPath;
	Zenith_Log(LOG_CATEGORY_SCENE, "Registered scene build index %d -> %s", iBuildIndex, strPath.c_str());
}

void Zenith_SceneRegistry::ClearBuildIndexRegistry()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "ClearBuildIndexRegistry must be called from main thread");
	g_xEngine.SceneRegistry().m_axBuildIndexToPath.Clear();

	uint32_t uScenesCleared = 0;
	for (u_int i = 0; i < g_xEngine.SceneRegistry().m_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = g_xEngine.SceneRegistry().m_axScenes.Get(i);
		if (pxData && pxData->m_iBuildIndex >= 0)
		{
			pxData->m_iBuildIndex = -1;
			++uScenesCleared;
		}
	}

	Zenith_Log(LOG_CATEGORY_SCENE,
		"Cleared scene build index registry (reset %u loaded-scene indices)", uScenesCleared);
}

namespace
{
	const std::string s_strEmptyBuildPath;
}

const std::string& Zenith_SceneRegistry::GetRegisteredScenePath(int iBuildIndex)
{
	const u_int uBuildIndex = static_cast<u_int>(iBuildIndex);
	if (iBuildIndex >= 0 && uBuildIndex < g_xEngine.SceneRegistry().m_axBuildIndexToPath.GetSize())
	{
		return g_xEngine.SceneRegistry().m_axBuildIndexToPath.Get(uBuildIndex);
	}
	return s_strEmptyBuildPath;
}

uint32_t Zenith_SceneRegistry::GetBuildIndexRegistrySize()
{
	return g_xEngine.SceneRegistry().m_axBuildIndexToPath.GetSize();
}

//=============================================================================
// RenameScene (atomic name-cache update)
//=============================================================================

bool Zenith_SceneRegistry::RenameScene(Zenith_Scene xScene, const std::string& strNewName)
{
	// E.18 / finding 3.15: the scene-name cache (g_xEngine.SceneRegistry().m_axLoadedSceneNames) is an O(1) lookup
	// shortcut for GetSceneByName. Before this API, renaming via friend access to
	// pxSceneData->m_strName left the cache stale — lookups by old name still hit, by
	// new name missed. Now cache and data move together.
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "RenameScene must be called from main thread");
	Zenith_Assert(!Zenith_SceneManager::AreRenderTasksActive(),
		"RenameScene: scene mutation while render tasks are reading — render-task invariant violated");

	if (!xScene.IsValid())
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "RenameScene: invalid scene handle");
		return false;
	}

	if (xScene.m_iHandle == g_xEngine.SceneRegistry().m_iPersistentSceneHandle)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "RenameScene: cannot rename the persistent scene");
		return false;
	}

	Zenith_SceneData* pxSceneData = GetSceneData(xScene);
	if (!pxSceneData)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "RenameScene: scene data not found");
		return false;
	}

	pxSceneData->m_strName = strNewName;

	for (u_int i = 0; i < g_xEngine.SceneRegistry().m_axLoadedSceneNames.GetSize(); ++i)
	{
		if (g_xEngine.SceneRegistry().m_axLoadedSceneNames.Get(i).m_iHandle == xScene.m_iHandle)
		{
			g_xEngine.SceneRegistry().m_axLoadedSceneNames.Get(i).m_strName = strNewName;
			pxSceneData->MarkDirty();
			return true;
		}
	}

	// Cache entry missing — shouldn't happen for a valid loaded scene, but be defensive.
	Zenith_Warning(LOG_CATEGORY_SCENE,
		"RenameScene: scene handle %d is loaded but has no cache entry — appending one",
		xScene.m_iHandle);
	AddToSceneNameCache(xScene.m_iHandle, strNewName);
	pxSceneData->MarkDirty();
	return true;
}
