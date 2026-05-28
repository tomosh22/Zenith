#include "Zenith.h"

#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"

//=============================================================================
// Path canonicaliser. Used by GetSceneByPath internally and exposed as a
// public static helper via Zenith_SceneSystem::CanonicalisePath() so the
// LoadScene paths can share the same logic without re-implementing it.
//=============================================================================
std::string Zenith_SceneSystem::CanonicalisePath(const std::string& strPath)
{
	std::string strResult = strPath;

	// Replace backslashes with forward slashes
	for (char& c : strResult)
	{
		if (c == '\\') c = '/';
	}

	// Collapse double slashes (e.g., "Levels//Scene.zscen" -> "Levels/Scene.zscen")
	size_t uDoubleSlash;
	while ((uDoubleSlash = strResult.find("//")) != std::string::npos)
	{
		strResult.erase(uDoubleSlash, 1);
	}

	// Remove ./ prefix
	while (strResult.size() >= 2 && strResult[0] == '.' && strResult[1] == '/')
	{
		strResult = strResult.substr(2);
	}

	// Resolve ../ sequences
	size_t uPos;
	while ((uPos = strResult.find("/../")) != std::string::npos && uPos > 0)
	{
		size_t uPrevSlash = strResult.rfind('/', uPos - 1);
		if (uPrevSlash == std::string::npos)
		{
			strResult = strResult.substr(uPos + 4);
		}
		else
		{
			strResult = strResult.substr(0, uPrevSlash) + strResult.substr(uPos + 3);
		}
	}

	// Remove trailing slashes
	while (!strResult.empty() && strResult.back() == '/')
	{
		strResult.pop_back();
	}

	return strResult;
}

//=============================================================================
// Zenith_SceneSystem — slot table, scene queries, name cache, build-index
// registry. The state lives as private members of the single Zenith_SceneSystem
// instance (held by Zenith_Engine as m_pxScenes); these instance methods reach
// it directly.
//
// Shutdown body lives in Internal/Zenith_SceneSystem_Lifecycle.cpp's combined
// Shutdown — it tears down lifecycle / callbacks / registry slots in the right
// order in one place.
//=============================================================================

//=============================================================================
// Slot management
//=============================================================================

int Zenith_SceneSystem::AllocateSceneHandle()
{
	if (m_axFreeHandles.GetSize() > 0)
	{
		int iHandle = m_axFreeHandles.GetBack();
		m_axFreeHandles.PopBack();
		return iHandle;
	}
	int iNewHandle = static_cast<int>(m_axScenes.GetSize());
	m_axSceneGenerations.PushBack(1);
	return iNewHandle;
}

void Zenith_SceneSystem::FreeSceneHandle(int iHandle)
{
	if (iHandle < 0 || iHandle >= static_cast<int>(m_axSceneGenerations.GetSize()))
	{
		return;
	}

	// D1: double-free guard. Callers must `delete pxSceneData` and null the slot
	// BEFORE calling FreeSceneHandle — otherwise the generation bump here releases
	// the handle while the live scene data is still reachable via m_axScenes,
	// corrupting subsequent loads into the same slot.
	Zenith_Assert(iHandle >= static_cast<int>(m_axScenes.GetSize()) ||
	              m_axScenes.Get(iHandle) == nullptr,
		"FreeSceneHandle(%d): scene data is still non-null — caller forgot to delete + null before releasing the handle",
		iHandle);

	RemoveFromSceneNameCache(iHandle);

	uint32_t& uGen = m_axSceneGenerations.Get(iHandle);
	if (uGen < UINT32_MAX - 1)
	{
		uGen++;
		m_axFreeHandles.PushBack(iHandle);
	}
	else
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "Scene handle %d retired due to generation overflow", iHandle);
	}
}

uint32_t Zenith_SceneSystem::GetSceneSlotCount()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread() || AreRenderTasksActive(),
		"GetSceneSlotCount must be called from main thread or during render task execution");
	return m_axScenes.GetSize();
}

Zenith_SceneData* Zenith_SceneSystem::GetSceneDataAtSlot(uint32_t uIndex)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread() || AreRenderTasksActive(),
		"GetSceneDataAtSlot must be called from main thread or during render task execution");
	if (uIndex >= m_axScenes.GetSize())
		return nullptr;
	return m_axScenes.Get(uIndex);
}

Zenith_SceneData* Zenith_SceneSystem::GetLoadedSceneDataAtSlot(uint32_t uIndex)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread() || AreRenderTasksActive(),
		"GetLoadedSceneDataAtSlot must be called from main thread or during render task execution");
	if (uIndex >= m_axScenes.GetSize())
		return nullptr;
	Zenith_SceneData* pxData = m_axScenes.Get(uIndex);
	if (!pxData || !pxData->IsLoaded() || pxData->IsUnloading())
		return nullptr;
	return pxData;
}

//=============================================================================
// Scene handle/data resolution
//=============================================================================

Zenith_Scene Zenith_SceneSystem::MakeInvalidScene()
{
	Zenith_Scene xScene;
	xScene.m_iHandle = -1;
	xScene.m_uGeneration = 0;
	return xScene;
}

Zenith_SceneData* Zenith_SceneSystem::GetSceneData(Zenith_Scene xScene)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread() || AreRenderTasksActive(),
		"GetSceneData must be called from the main thread or during render task execution");
	if (xScene.m_iHandle < 0 || xScene.m_iHandle >= static_cast<int>(m_axScenes.GetSize()))
	{
		return nullptr;
	}
	if (xScene.m_uGeneration != m_axSceneGenerations.Get(xScene.m_iHandle))
	{
		return nullptr;
	}
	return m_axScenes.Get(xScene.m_iHandle);
}

Zenith_SceneData* Zenith_SceneSystem::GetSceneDataByHandle(int iHandle)
{
	if (iHandle < 0 || iHandle >= static_cast<int>(m_axScenes.GetSize()))
	{
		return nullptr;
	}
	return m_axScenes.Get(iHandle);
}

Zenith_SceneData* Zenith_SceneSystem::GetSceneDataForEntity(Zenith_EntityID xID)
{
	if (!xID.IsValid()) return nullptr;
	if (xID.m_uIndex >= g_xEngine.EntityStore().m_axEntitySlots.GetSize()) return nullptr;
	const Zenith_SceneData::Zenith_EntitySlot& xSlot = g_xEngine.EntityStore().m_axEntitySlots.Get(xID.m_uIndex);
	if (!xSlot.IsOccupied() || xSlot.m_uGeneration != xID.m_uGeneration) return nullptr;
	return GetSceneDataByHandle(xSlot.m_iSceneHandle);
}

Zenith_Scene Zenith_SceneSystem::GetSceneFromHandle(int iHandle)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "GetSceneFromHandle must be called from main thread");
	Zenith_Scene xScene;
	xScene.m_iHandle = iHandle;
	xScene.m_uGeneration = 0;
	if (iHandle >= 0 && iHandle < static_cast<int>(m_axSceneGenerations.GetSize()))
	{
		xScene.m_uGeneration = m_axSceneGenerations.Get(iHandle);
	}
	return xScene;
}

//=============================================================================
// Scene queries
//=============================================================================

Zenith_Scene Zenith_SceneSystem::GetActiveScene()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread() || AreRenderTasksActive(),
		"GetActiveScene must be called from main thread or during render task execution");
	const int iHandle = m_iActiveSceneHandle;
	Zenith_Scene xScene;
	xScene.m_iHandle = iHandle;
	if (iHandle >= 0 && iHandle < static_cast<int>(m_axSceneGenerations.GetSize()))
	{
		xScene.m_uGeneration = m_axSceneGenerations.Get(iHandle);
	}
	return xScene;
}

Zenith_Scene Zenith_SceneSystem::GetSceneAt(uint32_t uIndex)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "GetSceneAt must be called from main thread");
	Zenith_Scene xScene = MakeInvalidScene();

	uint32_t uCurrent = 0;
	for (u_int i = 0; i < m_axScenes.GetSize(); ++i)
	{
		if (!IsSceneVisibleToUser(i, m_axScenes.Get(i)))
			continue;

		if (uCurrent == uIndex)
		{
			xScene.m_iHandle = static_cast<int>(i);
			xScene.m_uGeneration = m_axSceneGenerations.Get(i);
			return xScene;
		}
		uCurrent++;
	}
	return xScene;
}

Zenith_Scene Zenith_SceneSystem::GetSceneByBuildIndex(int iBuildIndex)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "GetSceneByBuildIndex must be called from main thread");
	Zenith_Scene xScene = MakeInvalidScene();
	for (u_int i = 0; i < m_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = m_axScenes.Get(i);
		if (pxData && pxData->m_iBuildIndex == iBuildIndex && pxData->IsLoaded() && !pxData->IsUnloading())
		{
			xScene.m_iHandle = static_cast<int>(i);
			xScene.m_uGeneration = m_axSceneGenerations.Get(i);
			return xScene;
		}
	}
	return xScene;
}

Zenith_Scene Zenith_SceneSystem::GetSceneByName(const std::string& strName)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "GetSceneByName must be called from main thread");
	Zenith_Scene xScene = MakeInvalidScene();
	int iFirstMatchHandle = -1;
	bool bAmbiguous = false;

	for (u_int i = 0; i < m_axLoadedSceneNames.GetSize(); ++i)
	{
		const SceneNameEntry& xEntry = m_axLoadedSceneNames.Get(i);
		const int iHandle = xEntry.m_iHandle;

		Zenith_SceneData* pxData = (iHandle >= 0 && iHandle < static_cast<int>(m_axScenes.GetSize()))
			? m_axScenes.Get(iHandle) : nullptr;
		if (!pxData || !pxData->IsLoaded() || pxData->IsUnloading()) continue;

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
		xScene.m_uGeneration = m_axSceneGenerations.Get(iFirstMatchHandle);
	}
	return xScene;
}

// Canonicalises the supplied path (via the shared CanonicalisePath helper —
// the same one LoadScene uses) before matching, so callers don't have to.
Zenith_Scene Zenith_SceneSystem::GetSceneByPath(const std::string& strPath)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "GetSceneByPath must be called from main thread");
	// Canonicalise first so callers don't have to (matches Unity's
	// SceneManager.GetSceneByPath which is path-insensitive to slashes).
	const std::string strCanonical = CanonicalisePath(strPath);
	Zenith_Scene xScene = MakeInvalidScene();

	for (u_int i = 0; i < m_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = m_axScenes.Get(i);
		if (pxData && pxData->IsLoaded() && !pxData->IsUnloading() && pxData->m_strPath == strCanonical)
		{
			xScene.m_iHandle = static_cast<int>(i);
			xScene.m_uGeneration = m_axSceneGenerations.Get(i);
			return xScene;
		}
	}
	return xScene;
}

uint32_t Zenith_SceneSystem::GetLoadedSceneCount()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "GetLoadedSceneCount must be called from main thread");
	uint32_t uCount = 0;
	for (u_int i = 0; i < m_axScenes.GetSize(); ++i)
	{
		if (IsSceneVisibleToUser(i, m_axScenes.Get(i))) uCount++;
	}
	return uCount;
}

uint32_t Zenith_SceneSystem::GetTotalSceneCount()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "GetTotalSceneCount must be called from main thread");
	uint32_t uCount = 0;
	for (u_int i = 0; i < m_axScenes.GetSize(); ++i)
	{
		if (m_axScenes.Get(i)) uCount++;
	}
	return uCount;
}

uint32_t Zenith_SceneSystem::GetBuildSceneCount()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "GetBuildSceneCount must be called from main thread");
	uint32_t uCount = 0;
	for (u_int i = 0; i < m_axBuildIndexToPath.GetSize(); ++i)
	{
		if (!m_axBuildIndexToPath.Get(i).empty()) uCount++;
	}
	return uCount;
}

//=============================================================================
// Active / persistent
//=============================================================================

int Zenith_SceneSystem::SelectNewActiveScene(int iExcludeHandle)
{
	int iBestHandle = -1;
	int iBestBuildIndex = -1;
	uint64_t ulBestTimestamp = 0;

	for (u_int i = 0; i < m_axScenes.GetSize(); ++i)
	{
		if (static_cast<int>(i) == m_iPersistentSceneHandle) continue;
		if (static_cast<int>(i) == iExcludeHandle) continue;

		Zenith_SceneData* pxCandidate = m_axScenes.Get(i);
		if (!pxCandidate || !pxCandidate->IsLoaded() || pxCandidate->IsUnloading()) continue;

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

Zenith_Scene Zenith_SceneSystem::GetPersistentScene()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "GetPersistentScene must be called from main thread");
	return GetSceneFromHandle(m_iPersistentSceneHandle);
}

//=============================================================================
// Visibility / update predicates
//=============================================================================

bool Zenith_SceneSystem::IsSceneVisibleToUser(u_int uSlotIndex, const Zenith_SceneData* pxData)
{
	if (!pxData || !pxData->IsLoaded() || !pxData->IsActivated() || pxData->IsUnloading())
		return false;
	if (static_cast<int>(uSlotIndex) == m_iPersistentSceneHandle && pxData->GetEntityCount() == 0)
		return false;
	return true;
}

bool Zenith_SceneSystem::IsSceneUpdatable(const Zenith_SceneData* pxData)
{
	return pxData && pxData->IsLoaded() && pxData->IsActivated() && !pxData->IsUnloading() && !pxData->IsPaused();
}

//=============================================================================
// Name cache
//=============================================================================

void Zenith_SceneSystem::AddToSceneNameCache(int iHandle, const std::string& strName)
{
	SceneNameEntry xEntry;
	xEntry.m_strName = strName;
	xEntry.m_iHandle = iHandle;
	m_axLoadedSceneNames.PushBack(std::move(xEntry));
}

void Zenith_SceneSystem::RemoveFromSceneNameCache(int iHandle)
{
	for (u_int i = 0; i < m_axLoadedSceneNames.GetSize(); ++i)
	{
		if (m_axLoadedSceneNames.Get(i).m_iHandle == iHandle)
		{
			m_axLoadedSceneNames.RemoveSwap(i);
			return;
		}
	}
}

//=============================================================================
// Build-index registry
//=============================================================================

void Zenith_SceneSystem::RegisterSceneBuildIndex(int iBuildIndex, const std::string& strPath)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "RegisterSceneBuildIndex must be called from main thread");
	Zenith_Assert(iBuildIndex >= 0, "RegisterSceneBuildIndex: Build index must be non-negative");

	const u_int uBuildIndex = static_cast<u_int>(iBuildIndex);
	if (uBuildIndex < m_axBuildIndexToPath.GetSize()
		&& !m_axBuildIndexToPath.Get(uBuildIndex).empty()
		&& m_axBuildIndexToPath.Get(uBuildIndex) != strPath)
	{
		Zenith_Assert(false,
			"RegisterSceneBuildIndex: Build index %d already registered for '%s', cannot register for '%s'",
			iBuildIndex, m_axBuildIndexToPath.Get(uBuildIndex).c_str(), strPath.c_str());
		Zenith_Error(LOG_CATEGORY_SCENE,
			"RegisterSceneBuildIndex: Build index %d already registered - ignoring duplicate", iBuildIndex);
		return;
	}
	while (m_axBuildIndexToPath.GetSize() <= uBuildIndex)
		m_axBuildIndexToPath.PushBack(std::string());
	m_axBuildIndexToPath.Get(uBuildIndex) = strPath;
	Zenith_Log(LOG_CATEGORY_SCENE, "Registered scene build index %d -> %s", iBuildIndex, strPath.c_str());
}

void Zenith_SceneSystem::ClearBuildIndexRegistry()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "ClearBuildIndexRegistry must be called from main thread");
	m_axBuildIndexToPath.Clear();

	uint32_t uScenesCleared = 0;
	for (u_int i = 0; i < m_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = m_axScenes.Get(i);
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

//=============================================================================
// Scene creation / activation / pausing
//=============================================================================

Zenith_Scene Zenith_SceneSystem::CreateScene(const std::string& strName)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "CreateScene must be called from main thread");

	if (strName.empty())
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "CreateScene: empty scene name rejected");
		return Zenith_Scene::INVALID_SCENE;
	}

	if (GetSceneByName(strName).IsValid())
	{
		Zenith_Error(LOG_CATEGORY_SCENE,
			"CreateScene: a scene named '%s' is already loaded; duplicate names are not allowed",
			strName.c_str());
		return Zenith_Scene::INVALID_SCENE;
	}

	return CreateEmptyScene(strName, true);
}

Zenith_Scene Zenith_SceneSystem::CreateEmptyScene(const std::string& strName, bool bAllowSetActive)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "CreateEmptyScene must be called from main thread");
	Zenith_Assert(!m_bRenderTasksActive, "CreateEmptyScene: scene mutation while render tasks are reading — render-task invariant violated");

	int iHandle = AllocateSceneHandle();

	Zenith_SceneData* pxSceneData = new Zenith_SceneData();
	pxSceneData->m_strName = strName;
	pxSceneData->m_iHandle = iHandle;
	pxSceneData->m_uGeneration = m_axSceneGenerations.Get(iHandle);
	pxSceneData->TransitionTo(Zenith_SceneData::SCENE_STATE_LOADING);
	// B.9: mirror the sync/async load progression — scene is created as
	// "loaded-but-not-activated" so any probe attached before the final flip
	// (e.g. via SceneLoadStarted or the soon-to-fire ActiveSceneChanged) observes
	// the same transient state as the disk-load paths. The flip to true happens
	// below, after the slot is wired up and ready for entity creation, but BEFORE
	// the optional auto-activate and BEFORE returning to the caller.
	pxSceneData->m_ulLoadTimestamp = m_ulNextLoadTimestamp++;

	// Ensure vector is large enough
	while (m_axScenes.GetSize() <= static_cast<u_int>(iHandle))
	{
		m_axScenes.PushBack(nullptr);
	}
	m_axScenes.Get(iHandle) = pxSceneData;
	AddToSceneNameCache(iHandle, strName);

	// B.9: flip to activated now that the slot is fully published — empty scenes
	// have no entity lifecycle to dispatch, so there's nothing to wait for.
	pxSceneData->TransitionTo(Zenith_SceneData::SCENE_STATE_LOADED);

	// A6: Auto-activate only if caller opts in AND no active scene already exists.
	// Initialise() passes false when creating the persistent DontDestroyOnLoad scene
	// so it never becomes the fallback active.
	if (bAllowSetActive && m_iActiveSceneHandle < 0)
	{
		Zenith_Assert(!m_bRenderTasksActive, "Cannot change active scene while render tasks are in flight");
		m_iActiveSceneHandle = iHandle;
	}

	Zenith_Scene xScene;
	xScene.m_iHandle = iHandle;
	xScene.m_uGeneration = m_axSceneGenerations.Get(iHandle);
	return xScene;
}

bool Zenith_SceneSystem::SetActiveScene(Zenith_Scene xScene)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "SetActiveScene must be called from main thread");

	if (!xScene.IsValid())
	{
		return false;
	}

	// A6: the persistent scene is a container for DontDestroyOnLoad entities only;
	// it must never be the "active" scene (matches Unity's DontDestroyOnLoad semantics).
	if (xScene.m_iHandle == m_iPersistentSceneHandle)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "SetActiveScene: Cannot set the persistent scene as active (DontDestroyOnLoad is a container, not a real scene)");
		return false;
	}

	Zenith_SceneData* pxSceneData = GetSceneData(xScene);
	if (!pxSceneData || !pxSceneData->IsLoaded())
	{
		return false;
	}

	// Cannot set a scene as active if it's being unloaded
	if (pxSceneData->IsUnloading())
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "Cannot set unloading scene as active");
		return false;
	}

	// Unity behavior: Don't fire callback if same scene
	Zenith_Scene xCurrent = GetActiveScene();
	if (xCurrent == xScene)
	{
		return true;
	}

	Zenith_Assert(!m_bRenderTasksActive, "Cannot change active scene while render tasks are in flight");
	m_iActiveSceneHandle = xScene.m_iHandle;
	FireActiveSceneChanged(xCurrent, xScene);
	return true;
}

void Zenith_SceneSystem::SetScenePaused(Zenith_Scene xScene, bool bPaused)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "SetScenePaused must be called from main thread");

	Zenith_SceneData* pxSceneData = GetSceneData(xScene);
	if (pxSceneData)
	{
		pxSceneData->SetPaused(bPaused);
	}
}

bool Zenith_SceneSystem::IsScenePaused(Zenith_Scene xScene)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "IsScenePaused must be called from main thread");

	Zenith_SceneData* pxSceneData = GetSceneData(xScene);
	return pxSceneData ? pxSceneData->IsPaused() : false;
}

Zenith_CameraComponent* Zenith_SceneSystem::FindMainCameraAcrossScenes()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread() || m_bRenderTasksActive, "FindMainCameraAcrossScenes must be called from main thread or during render task execution");
	// Try active scene first (common case).
	Zenith_SceneData* pxActiveData = GetSceneData(GetActiveScene());
	if (pxActiveData)
	{
		Zenith_CameraComponent* pxCamera = pxActiveData->TryGetMainCamera();
		if (pxCamera)
			return pxCamera;
	}

	// Search all loaded scenes (finds camera in persistent scene, etc.)
	for (u_int i = 0; i < m_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = m_axScenes.Get(i);
		if (pxData && pxData->IsLoaded() && !pxData->IsUnloading())
		{
			Zenith_CameraComponent* pxCamera = pxData->TryGetMainCamera();
			if (pxCamera)
				return pxCamera;
		}
	}

	return nullptr;
}

const std::string& Zenith_SceneSystem::GetRegisteredScenePath(int iBuildIndex)
{
	const u_int uBuildIndex = static_cast<u_int>(iBuildIndex);
	if (iBuildIndex >= 0 && uBuildIndex < m_axBuildIndexToPath.GetSize())
	{
		return m_axBuildIndexToPath.Get(uBuildIndex);
	}
	return s_strEmptyBuildPath;
}

uint32_t Zenith_SceneSystem::GetBuildIndexRegistrySize()
{
	return m_axBuildIndexToPath.GetSize();
}

//=============================================================================
// RenameScene (atomic name-cache update)
//=============================================================================

bool Zenith_SceneSystem::RenameScene(Zenith_Scene xScene, const std::string& strNewName)
{
	// E.18 / finding 3.15: the scene-name cache (m_axLoadedSceneNames) is the
	// backing store GetSceneByName scans (a linear pass that avoids touching
	// scene data per slot). Before this API, renaming via friend access to
	// pxSceneData->m_strName left the cache stale — lookups by old name still hit,
	// by new name missed. Now cache and data move together.
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "RenameScene must be called from main thread");
	Zenith_Assert(!AreRenderTasksActive(),
		"RenameScene: scene mutation while render tasks are reading — render-task invariant violated");

	if (!xScene.IsValid())
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "RenameScene: invalid scene handle");
		return false;
	}

	if (xScene.m_iHandle == m_iPersistentSceneHandle)
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

	for (u_int i = 0; i < m_axLoadedSceneNames.GetSize(); ++i)
	{
		if (m_axLoadedSceneNames.Get(i).m_iHandle == xScene.m_iHandle)
		{
			m_axLoadedSceneNames.Get(i).m_strName = strNewName;
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
