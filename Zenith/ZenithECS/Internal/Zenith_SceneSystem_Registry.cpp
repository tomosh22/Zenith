#include "Zenith.h"

#include "ZenithECS/Zenith_SceneSystem.h"

//=============================================================================
// Phase 2.1 (ECS leaf-extraction): process-wide singleton pointer + lifetime.
//
// The single Zenith_SceneSystem publishes itself here so the ECS core can reach
// the entity store via Zenith_SceneSystem::Get().GetEntityStore() without naming
// g_xEngine (Phase 2.2 repoints the call sites). The owned Zenith_EntityStore
// (the three global entity arrays, relocated from Zenith_Engine) is allocated in
// the ctor and freed in the dtor, so its lifetime == this object's lifetime.
//
// Ordering note: Zenith_Engine::Initialise allocates the SceneSystem (m_pxScenes)
// before anything can touch entity slots, so the store exists by the time the
// first Zenith_ECS_EntityStore() (now forwarding to GetEntityStore()) runs.
//=============================================================================
Zenith_SceneSystem* Zenith_SceneSystem::s_pxInstance = nullptr;

Zenith_SceneSystem::Zenith_SceneSystem()
{
	m_pxEntityStore = new Zenith_EntityStore();
	s_pxInstance = this;
}

Zenith_SceneSystem::~Zenith_SceneSystem()
{
	delete m_pxEntityStore;
	m_pxEntityStore = nullptr;
	if (s_pxInstance == this)
	{
		s_pxInstance = nullptr;
	}
}

//=============================================================================
// Path canonicaliser. Exposed as a public static helper via
// Zenith_SceneSystem::CanonicalisePath() so the LoadScene paths can share the
// same logic without re-implementing it.
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
	Zenith_Assert(Zenith_ECS_IsMainThread() || AreRenderTasksActive(),
		"GetSceneSlotCount must be called from main thread or during render task execution");
	return m_axScenes.GetSize();
}

Zenith_SceneData* Zenith_SceneSystem::GetSceneDataAtSlot(uint32_t uIndex)
{
	Zenith_Assert(Zenith_ECS_IsMainThread() || AreRenderTasksActive(),
		"GetSceneDataAtSlot must be called from main thread or during render task execution");
	if (uIndex >= m_axScenes.GetSize())
		return nullptr;
	return m_axScenes.Get(uIndex);
}

Zenith_SceneData* Zenith_SceneSystem::GetLoadedSceneDataAtSlot(uint32_t uIndex)
{
	Zenith_Assert(Zenith_ECS_IsMainThread() || AreRenderTasksActive(),
		"GetLoadedSceneDataAtSlot must be called from main thread or during render task execution");
	if (uIndex >= m_axScenes.GetSize())
		return nullptr;
	Zenith_SceneData* pxData = m_axScenes.Get(uIndex);
	if (!pxData || !pxData->IsLoaded())
		return nullptr;
	return pxData;
}

//=============================================================================
// Scene handle/data resolution
//=============================================================================

Zenith_SceneData* Zenith_SceneSystem::GetSceneData(Zenith_Scene xScene)
{
	Zenith_Assert(Zenith_ECS_IsMainThread() || AreRenderTasksActive(),
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
	if (xID.m_uIndex >= Zenith_ECS_EntityStore().m_axEntitySlots.GetSize()) return nullptr;
	const Zenith_SceneData::Zenith_EntitySlot& xSlot = Zenith_ECS_EntityStore().m_axEntitySlots.Get(xID.m_uIndex);
	if (!xSlot.IsOccupied() || xSlot.m_uGeneration != xID.m_uGeneration) return nullptr;
	return GetSceneDataByHandle(xSlot.m_iSceneHandle);
}

Zenith_Scene Zenith_SceneSystem::GetSceneFromHandle(int iHandle)
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "GetSceneFromHandle must be called from main thread");
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
	Zenith_Assert(Zenith_ECS_IsMainThread() || AreRenderTasksActive(),
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
	Zenith_Assert(Zenith_ECS_IsMainThread(), "GetSceneAt must be called from main thread");
	Zenith_Scene xScene = Zenith_Scene::INVALID_SCENE;

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

Zenith_Scene Zenith_SceneSystem::GetSceneByName(const std::string& strName)
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "GetSceneByName must be called from main thread");
	Zenith_Scene xScene = Zenith_Scene::INVALID_SCENE;
	int iFirstMatchHandle = -1;
	bool bAmbiguous = false;

	for (u_int i = 0; i < m_axLoadedSceneNames.GetSize(); ++i)
	{
		const SceneNameEntry& xEntry = m_axLoadedSceneNames.Get(i);
		const int iHandle = xEntry.m_iHandle;

		Zenith_SceneData* pxData = (iHandle >= 0 && iHandle < static_cast<int>(m_axScenes.GetSize()))
			? m_axScenes.Get(iHandle) : nullptr;
		if (!pxData || !pxData->IsLoaded()) continue;

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
			"GetSceneByName('%s'): more than one loaded scene matches; returning first.",
			strName.c_str());
	}

	if (iFirstMatchHandle >= 0)
	{
		xScene.m_iHandle = iFirstMatchHandle;
		xScene.m_uGeneration = m_axSceneGenerations.Get(iFirstMatchHandle);
	}
	return xScene;
}

//=============================================================================
// Scene metadata snapshot
//
// GetSceneInfo replaces the deleted per-field Zenith_Scene metadata getters
// (GetName / GetPath / GetBuildIndex / IsLoaded / WasLoadedAdditively /
// HasUnsavedChanges / GetRootEntityCount). It reads exactly the same SceneData
// sources, with the same null-scene fallbacks. Declared const; GetSceneData is
// logically const (slot-table read) but returns a mutable pointer, so the
// const_cast here is benign — we only READ through pxData (all SceneData getters
// used are const).
//=============================================================================

Zenith_SceneInfo Zenith_SceneSystem::GetSceneInfo(Zenith_Scene xScene) const
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "GetSceneInfo must be called from main thread");
	Zenith_SceneInfo xInfo;

	Zenith_SceneData* pxData = const_cast<Zenith_SceneSystem*>(this)->GetSceneData(xScene);
	if (pxData == nullptr)
	{
		// Invalid / stale / unloaded handle: defaults match the old getters'
		// null-scene fallbacks (empty name+path, build index -1, all flags false,
		// root count 0).
		return xInfo;
	}

	xInfo.m_strName           = pxData->GetName();
	xInfo.m_strPath           = pxData->GetPath();
	xInfo.m_iBuildIndex       = pxData->GetBuildIndex();
	// Old Zenith_Scene::IsLoaded(): loaded AND activated.
	xInfo.m_bLoaded           = pxData->IsLoaded() && pxData->IsActivated();
	xInfo.m_bLoadedAdditively = pxData->WasLoadedAdditively();
	// Old Zenith_Scene::GetRootEntityCount(): 0 unless loaded, else cached count.
	xInfo.m_uRootEntityCount  = pxData->IsLoaded() ? pxData->GetCachedRootEntityCount() : 0u;
#ifdef ZENITH_TOOLS
	xInfo.m_bHasUnsavedChanges = pxData->HasUnsavedChanges();
#endif
	return xInfo;
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
		if (!pxCandidate || !pxCandidate->IsLoaded()) continue;

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
	Zenith_Assert(Zenith_ECS_IsMainThread(), "GetPersistentScene must be called from main thread");
	return GetSceneFromHandle(m_iPersistentSceneHandle);
}

//=============================================================================
// Visibility / update predicates
//=============================================================================

bool Zenith_SceneSystem::IsSceneVisibleToUser(u_int uSlotIndex, const Zenith_SceneData* pxData)
{
	if (!pxData || !pxData->IsLoaded() || !pxData->IsActivated())
		return false;
	if (static_cast<int>(uSlotIndex) == m_iPersistentSceneHandle && pxData->GetEntityCount() == 0)
		return false;
	return true;
}

bool Zenith_SceneSystem::IsSceneUpdatable(const Zenith_SceneData* pxData)
{
	return pxData && pxData->IsLoaded() && pxData->IsActivated() && !pxData->IsPaused();
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
	Zenith_Assert(Zenith_ECS_IsMainThread(), "RegisterSceneBuildIndex must be called from main thread");
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
	Zenith_Assert(Zenith_ECS_IsMainThread(), "ClearBuildIndexRegistry must be called from main thread");
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

Zenith_Scene Zenith_SceneSystem::AllocateEmptyScene(const std::string& strName, bool bAllowSetActive)
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "AllocateEmptyScene must be called from main thread");
	Zenith_Assert(!m_bRenderTasksActive, "AllocateEmptyScene: scene mutation while render tasks are reading — render-task invariant violated");

	int iHandle = AllocateSceneHandle();

	Zenith_SceneData* pxSceneData = new Zenith_SceneData();
	pxSceneData->m_strName = strName;
	pxSceneData->m_iHandle = iHandle;
	pxSceneData->m_uGeneration = m_axSceneGenerations.Get(iHandle);
	pxSceneData->TransitionTo(Zenith_SceneData::SCENE_STATE_LOADING);
	// B.9: mirror the sync/async load progression — scene is created as
	// "loaded-but-not-activated" so any code that inspects the scene before the
	// final flip observes the same transient state as the disk-load paths. The flip to true happens
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
	Zenith_Assert(Zenith_ECS_IsMainThread(), "SetActiveScene must be called from main thread");

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

	// Unity behavior: no-op if already the active scene.
	Zenith_Scene xCurrent = GetActiveScene();
	if (xCurrent == xScene)
	{
		return true;
	}

	Zenith_Assert(!m_bRenderTasksActive, "Cannot change active scene while render tasks are in flight");
	m_iActiveSceneHandle = xScene.m_iHandle;
	return true;
}

void Zenith_SceneSystem::SetScenePaused(Zenith_Scene xScene, bool bPaused)
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "SetScenePaused must be called from main thread");

	Zenith_SceneData* pxSceneData = GetSceneData(xScene);
	if (pxSceneData)
	{
		pxSceneData->SetPaused(bPaused);
	}
}

bool Zenith_SceneSystem::IsScenePaused(Zenith_Scene xScene)
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "IsScenePaused must be called from main thread");

	Zenith_SceneData* pxSceneData = GetSceneData(xScene);
	return pxSceneData ? pxSceneData->IsPaused() : false;
}

Zenith_EntityID Zenith_SceneSystem::FindMainCameraEntityAcrossScenes()
{
	// Leaf form of the old FindMainCameraAcrossScenes(): walks the LIVE ECS scene
	// list and returns the main-camera EntityID rather than resolving it to a
	// Zenith_CameraComponent (the engine-side resolver Zenith_GetMainCameraAcrossScenes()
	// does that). Scene-iteration order + the main-thread restriction are unchanged
	// from the typed version: active scene first (common case), then every loaded,
	// non-unloading scene in slot order. A scene's stored camera entity is gated on
	// IsValid()+EntityExists() so a scene whose camera reference is stale is skipped
	// and the scan continues -- matching the old TryGetMainCamera() per-scene
	// validity skip (the HasComponent gate is applied by the resolver).
	Zenith_Assert(Zenith_ECS_IsMainThread(), "FindMainCameraEntityAcrossScenes must be called from the main thread (walks live ECS)");
	// Try active scene first (common case).
	Zenith_SceneData* pxActiveData = GetSceneData(GetActiveScene());
	if (pxActiveData)
	{
		const Zenith_EntityID xCameraEntity = pxActiveData->GetMainCameraEntity();
		if (xCameraEntity.IsValid() && pxActiveData->EntityExists(xCameraEntity))
			return xCameraEntity;
	}

	// Search all loaded scenes (finds camera in persistent scene, etc.)
	for (u_int i = 0; i < m_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = m_axScenes.Get(i);
		if (pxData && pxData->IsLoaded())
		{
			const Zenith_EntityID xCameraEntity = pxData->GetMainCameraEntity();
			if (xCameraEntity.IsValid() && pxData->EntityExists(xCameraEntity))
				return xCameraEntity;
		}
	}

	return INVALID_ENTITY_ID;
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

