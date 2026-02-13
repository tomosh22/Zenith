#include "Zenith.h"

#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_SceneOperation.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "DataStream/Zenith_DataStream.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Flux/MeshAnimation/Flux_MeshAnimation.h"
#include "TaskSystem/Zenith_TaskSystem.h"

// Flux reset includes (for ResetAllRenderSystems)
#include "Flux/Terrain/Flux_Terrain.h"
#include "Flux/StaticMeshes/Flux_StaticMeshes.h"
#include "Flux/AnimatedMeshes/Flux_AnimatedMeshes.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "Flux/Primitives/Flux_Primitives.h"
#include "Flux/Text/Flux_Text.h"
#include "Flux/Particles/Flux_Particles.h"
#include "Flux/Skybox/Flux_Skybox.h"
#include "Flux/DeferredShading/Flux_DeferredShading.h"
#include "Flux/HiZ/Flux_HiZ.h"
#include "Flux/SSR/Flux_SSR.h"
#include "Flux/SSAO/Flux_SSAO.h"
#include "Flux/Fog/Flux_Fog.h"
#include "Flux/SDFs/Flux_SDFs.h"
#include "Flux/Quads/Flux_Quads.h"
#ifdef ZENITH_TOOLS
#include "Flux/Gizmos/Flux_Gizmos.h"
#endif
#include "Physics/Zenith_Physics.h"

// Static member definitions
Zenith_Vector<Zenith_SceneData*> Zenith_SceneManager::s_axScenes;
Zenith_Vector<uint32_t> Zenith_SceneManager::s_axSceneGenerations;
Zenith_Vector<int> Zenith_SceneManager::s_axFreeHandles;
int Zenith_SceneManager::s_iActiveSceneHandle = -1;
#ifdef ZENITH_ASSERT
bool Zenith_SceneManager::s_bRenderTasksActive = false;
bool Zenith_SceneManager::s_bAnimTasksActive = false;
#endif
int Zenith_SceneManager::s_iPersistentSceneHandle = -1;
Zenith_Vector<Zenith_SceneOperation*> Zenith_SceneManager::s_axActiveOperations;
float Zenith_SceneManager::s_fFixedTimeAccumulator = 0.0f;
float Zenith_SceneManager::s_fFixedTimestep = 0.02f;  // Default 50Hz (Unity default)
uint32_t Zenith_SceneManager::s_uAsyncUnloadBatchSize = 50;  // Default: 50 entities per frame
uint32_t Zenith_SceneManager::s_uMaxConcurrentAsyncLoads = 8;  // Default: 8 concurrent loads
bool Zenith_SceneManager::s_bIsLoadingScene = false;
bool Zenith_SceneManager::s_bIsPrefabInstantiating = false;
Zenith_SceneManager::InitialSceneLoadFn Zenith_SceneManager::s_pfnInitialSceneLoad = nullptr;
Zenith_Vector<Zenith_SceneManager::SceneNameEntry> Zenith_SceneManager::s_axLoadedSceneNames;
uint64_t Zenith_SceneManager::s_ulNextLoadTimestamp = 1;
Zenith_SceneManager::Zenith_CallbackList<Zenith_SceneManager::SceneChangedCallback> Zenith_SceneManager::s_xActiveSceneChangedCallbacks;
Zenith_SceneManager::Zenith_CallbackList<Zenith_SceneManager::SceneLoadedCallback> Zenith_SceneManager::s_xSceneLoadedCallbacks;
Zenith_SceneManager::Zenith_CallbackList<Zenith_SceneManager::SceneUnloadingCallback> Zenith_SceneManager::s_xSceneUnloadingCallbacks;
Zenith_SceneManager::Zenith_CallbackList<Zenith_SceneManager::SceneUnloadedCallback> Zenith_SceneManager::s_xSceneUnloadedCallbacks;
Zenith_SceneManager::Zenith_CallbackList<Zenith_SceneManager::SceneLoadStartedCallback> Zenith_SceneManager::s_xSceneLoadStartedCallbacks;
Zenith_SceneManager::Zenith_CallbackList<Zenith_SceneManager::EntityPersistentCallback> Zenith_SceneManager::s_xEntityPersistentCallbacks;
Zenith_SceneManager::CallbackHandle Zenith_SceneManager::s_ulNextCallbackHandle = 1;
Zenith_Vector<Zenith_SceneManager::CallbackHandle> Zenith_SceneManager::s_axCallbacksPendingRemoval;
uint32_t Zenith_SceneManager::s_uFiringCallbacksDepth = 0;
Zenith_Vector<std::string> Zenith_SceneManager::s_axBuildIndexToPath;
Zenith_Vector<std::string> Zenith_SceneManager::s_axCurrentlyLoadingPaths;
Zenith_Vector<std::string> Zenith_SceneManager::s_axLifecycleLoadStack;
Zenith_Vector<Zenith_SceneManager::OperationMapEntry> Zenith_SceneManager::s_axOperationMap;
uint64_t Zenith_SceneManager::s_ulNextOperationID = 1;  // Start at 1 so 0 is invalid (0 == ZENITH_INVALID_OPERATION_ID)
Zenith_Vector<Zenith_SceneManager::AsyncLoadJob*> Zenith_SceneManager::s_axAsyncJobs;
Zenith_Vector<Zenith_SceneManager::AsyncUnloadJob*> Zenith_SceneManager::s_axAsyncUnloadJobs;
bool Zenith_SceneManager::s_bAsyncJobsNeedSort = false;
bool Zenith_SceneManager::s_bIsUpdating = false;

// Helper: check if a Zenith_Vector<std::string> contains a given string
static bool VectorContainsString(const Zenith_Vector<std::string>& axVec, const std::string& str)
{
	for (u_int i = 0; i < axVec.GetSize(); ++i)
	{
		if (axVec.Get(i) == str) return true;
	}
	return false;
}

void Zenith_SceneManager::AddToSceneNameCache(int iHandle, const std::string& strName)
{
	SceneNameEntry xEntry;
	xEntry.m_strName = strName;
	xEntry.m_iHandle = iHandle;
	s_axLoadedSceneNames.PushBack(std::move(xEntry));
}

void Zenith_SceneManager::RemoveFromSceneNameCache(int iHandle)
{
	for (u_int i = 0; i < s_axLoadedSceneNames.GetSize(); ++i)
	{
		if (s_axLoadedSceneNames.Get(i).m_iHandle == iHandle)
		{
			s_axLoadedSceneNames.RemoveSwap(i);
			return;
		}
	}
}

// Async load progress milestones (Unity-compatible)
// Progress pauses at ACTIVATION_PAUSED when allowSceneActivation is false
static constexpr float fPROGRESS_FILE_READ_START = 0.1f;
static constexpr float fPROGRESS_FILE_READ_COMPLETE = 0.7f;
static constexpr float fPROGRESS_SCENE_CREATED = 0.75f;
static constexpr float fPROGRESS_DESERIALIZE_START = 0.8f;
static constexpr float fPROGRESS_DESERIALIZE_COMPLETE = 0.85f;
static constexpr float fPROGRESS_ACTIVATION_PAUSED = 0.9f;
static constexpr float fPROGRESS_COMPLETE = 1.0f;

// Async unload progress weights
// Entity destruction accounts for 90% of progress, cleanup (memory deallocation, callbacks) is 10%
static constexpr float fPROGRESS_DESTRUCTION_WEIGHT = 0.9f;

// Operation cleanup delay - operations are kept alive this many frames after completion
// to allow users to call GetResultScene() after checking IsComplete()
// Note: 60 frames (~1 second at 60fps) provides sufficient time for callers to access
// operation results without needing to re-fetch via GetOperation() every frame
static constexpr uint32_t uOPERATION_CLEANUP_DELAY_FRAMES = 60;

// Animation update task system
static Zenith_TaskArray* g_pxAnimUpdateTask = nullptr;
static Zenith_Vector<Flux_MeshAnimation*> g_xAnimationsToUpdate;

static void AnimUpdateTask(void*, u_int uInvocationIndex, u_int uNumInvocations)
{
	const float fDt = Zenith_Core::GetDt();
	const u_int uTotalAnimations = g_xAnimationsToUpdate.GetSize();

	if (uInvocationIndex >= uTotalAnimations)
	{
		return;
	}

	const u_int uAnimsPerInvocation = (uTotalAnimations + uNumInvocations - 1) / uNumInvocations;
	const u_int uStartIndex = uInvocationIndex * uAnimsPerInvocation;
	const u_int uEndIndex = (uStartIndex + uAnimsPerInvocation < uTotalAnimations) ?
		uStartIndex + uAnimsPerInvocation : uTotalAnimations;

	for (u_int u = uStartIndex; u < uEndIndex; u++)
	{
		Flux_MeshAnimation* pxAnim = g_xAnimationsToUpdate.Get(u);
		Zenith_Assert(pxAnim != nullptr, "Null animation");
		pxAnim->Update(fDt);
	}
}

// Helper to extract scene name from file path (e.g., "Levels/MyScene.zscen" -> "MyScene")
static std::string ExtractSceneNameFromPath(const std::string& strPath)
{
	size_t uLastSlash = strPath.find_last_of("/\\");
	size_t uStart = (uLastSlash == std::string::npos) ? 0 : uLastSlash + 1;
	size_t uLastDot = strPath.find_last_of('.');
	size_t uEnd = (uLastDot == std::string::npos || uLastDot < uStart) ?
		strPath.size() : uLastDot;
	return strPath.substr(uStart, uEnd - uStart);
}

// Normalize scene paths for consistent comparison
// Handles: backslashes, ./ prefix, ../ sequences, trailing slashes
// Canonicalizes a relative scene path for consistent path comparisons.
// Handles: backslash->forward slash, double slashes, ./ prefix, ../ resolution, trailing slashes.
// Note: designed for relative scene paths (e.g., "Levels/Scene.zscen"). Absolute paths are not expected.
static std::string CanonicalizeScenePath(const std::string& strPath)
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

//==========================================================================
// Initialization / Shutdown
//==========================================================================

void Zenith_SceneManager::Initialise()
{
	// Assert single initialization - calling twice without Shutdown leaks the animation task
	Zenith_Assert(g_pxAnimUpdateTask == nullptr, "SceneManager::Initialise called twice without Shutdown");
	g_pxAnimUpdateTask = new Zenith_TaskArray(ZENITH_PROFILE_INDEX__ANIMATION, AnimUpdateTask, nullptr, 4, true);

	// Create the persistent scene (always loaded, never unloaded)
	Zenith_Scene xPersistent = CreateEmptyScene("DontDestroyOnLoad");
	s_iPersistentSceneHandle = xPersistent.m_iHandle;

	Zenith_Log(LOG_CATEGORY_SCENE, "SceneManager initialized with persistent scene (handle=%d)", s_iPersistentSceneHandle);
}

void Zenith_SceneManager::Shutdown()
{
	// Clean up animation task first (before clearing scenes)
	if (g_pxAnimUpdateTask)
	{
		g_pxAnimUpdateTask->WaitUntilComplete();
		delete g_pxAnimUpdateTask;
		g_pxAnimUpdateTask = nullptr;
	}

	// Clean up any pending async load jobs - wait for worker threads before deleting
	for (u_int i = 0; i < s_axAsyncJobs.GetSize(); ++i)
	{
		AsyncLoadJob* pxJob = s_axAsyncJobs.Get(i);
		if (pxJob && pxJob->m_pxTask)
		{
			pxJob->m_pxTask->WaitUntilComplete();
		}
		delete pxJob;
	}
	s_axAsyncJobs.Clear();

	// Clean up any pending async unload jobs
	// Skip callbacks during shutdown - scene data is about to be bulk-deleted
	// and callback handlers could access invalidated data
	for (u_int i = 0; i < s_axAsyncUnloadJobs.GetSize(); ++i)
	{
		delete s_axAsyncUnloadJobs.Get(i);
	}
	s_axAsyncUnloadJobs.Clear();

	// Clean up active operations
	for (u_int i = 0; i < s_axActiveOperations.GetSize(); ++i)
	{
		delete s_axActiveOperations.Get(i);
	}
	s_axActiveOperations.Clear();
	s_axOperationMap.Clear();

	// Unload all scenes
	for (u_int i = 0; i < s_axScenes.GetSize(); ++i)
	{
		delete s_axScenes.Get(i);
	}
	s_axScenes.Clear();
	s_axSceneGenerations.Clear();
	s_axFreeHandles.Clear();
	s_xActiveSceneChangedCallbacks.m_axEntries.Clear();
	s_xSceneLoadedCallbacks.m_axEntries.Clear();
	s_xSceneUnloadingCallbacks.m_axEntries.Clear();
	s_xSceneUnloadedCallbacks.m_axEntries.Clear();
	s_xSceneLoadStartedCallbacks.m_axEntries.Clear();
	s_xEntityPersistentCallbacks.m_axEntries.Clear();
	s_axBuildIndexToPath.Clear();
	s_axLoadedSceneNames.Clear();
	s_iActiveSceneHandle = -1;
	s_iPersistentSceneHandle = -1;

	// Reset state flags (prevents stale state on re-initialization, e.g. unit test cycles)
	s_bIsLoadingScene = false;
	s_bIsPrefabInstantiating = false;
	s_uFiringCallbacksDepth = 0;
	s_fFixedTimeAccumulator = 0.0f;
	s_axCallbacksPendingRemoval.Clear();
	s_axCurrentlyLoadingPaths.Clear();
	s_axLifecycleLoadStack.Clear();
	s_bAsyncJobsNeedSort = false;

	// Reset ID counters for deterministic behavior across Shutdown/Initialise cycles
	s_ulNextLoadTimestamp = 1;
	s_ulNextOperationID = 1;
	s_ulNextCallbackHandle = 1;

	// Reset global entity storage (shared across all scenes)
	Zenith_SceneData::ResetGlobalEntityStorage();
}

//==========================================================================
// Scene Count Queries
//==========================================================================

bool Zenith_SceneManager::IsSceneVisibleToUser(u_int uSlotIndex, const Zenith_SceneData* pxData)
{
	if (!pxData || !pxData->m_bIsLoaded || !pxData->m_bIsActivated || pxData->m_bIsUnloading)
		return false;

	// Unity: sceneCount includes DontDestroyOnLoad once it has entities
	if (static_cast<int>(uSlotIndex) == s_iPersistentSceneHandle && pxData->GetEntityCount() == 0)
		return false;

	return true;
}

bool Zenith_SceneManager::IsSceneUpdatable(const Zenith_SceneData* pxData)
{
	return pxData && pxData->m_bIsLoaded && pxData->m_bIsActivated && !pxData->m_bIsUnloading && !pxData->IsPaused();
}

uint32_t Zenith_SceneManager::GetLoadedSceneCount()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetLoadedSceneCount must be called from main thread");
	uint32_t uCount = 0;
	for (u_int i = 0; i < s_axScenes.GetSize(); ++i)
	{
		if (IsSceneVisibleToUser(i, s_axScenes.Get(i)))
			uCount++;
	}
	// Unity: sceneCount is always >= 1 (there is always at least one scene loaded)
	return uCount > 0 ? uCount : 1;
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
// Scene Creation
//==========================================================================

Zenith_Scene Zenith_SceneManager::CreateEmptyScene(const std::string& strName)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "CreateEmptyScene must be called from main thread");

	int iHandle = AllocateSceneHandle();

	Zenith_SceneData* pxSceneData = new Zenith_SceneData();
	pxSceneData->m_strName = strName;
	pxSceneData->m_iHandle = iHandle;
	pxSceneData->m_uGeneration = s_axSceneGenerations.Get(iHandle);
	pxSceneData->m_bIsLoaded = true;
	pxSceneData->m_ulLoadTimestamp = s_ulNextLoadTimestamp++;

	// Ensure vector is large enough
	while (s_axScenes.GetSize() <= static_cast<u_int>(iHandle))
	{
		s_axScenes.PushBack(nullptr);
	}
	s_axScenes.Get(iHandle) = pxSceneData;
	AddToSceneNameCache(iHandle, strName);

	// Set as active if no active scene
	if (s_iActiveSceneHandle < 0)
	{
		Zenith_Assert(!s_bRenderTasksActive, "Cannot change active scene while render tasks are in flight");
		s_iActiveSceneHandle = iHandle;
	}

	Zenith_Scene xScene;
	xScene.m_iHandle = iHandle;
	xScene.m_uGeneration = s_axSceneGenerations.Get(iHandle);
	return xScene;
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
	Zenith_Scene xScene;
	xScene.m_iHandle = -1;
	xScene.m_uGeneration = 0;

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
	Zenith_Scene xScene;
	xScene.m_iHandle = -1;
	xScene.m_uGeneration = 0;

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
	Zenith_Scene xScene;
	xScene.m_iHandle = -1;
	xScene.m_uGeneration = 0;

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

		// Unity behavior: silently return first match when multiple scenes have the same name
		if (bMatched)
		{
			xScene.m_iHandle = iHandle;
			xScene.m_uGeneration = s_axSceneGenerations.Get(iHandle);
			return xScene;
		}
	}

	return xScene;
}

Zenith_Scene Zenith_SceneManager::GetSceneByPath(const std::string& strPath)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetSceneByPath must be called from main thread");
	Zenith_Scene xScene;
	xScene.m_iHandle = -1;
	xScene.m_uGeneration = 0;

	// Canonicalize input path for consistent comparison
	std::string strCanonical = CanonicalizeScenePath(strPath);

	for (u_int i = 0; i < s_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = s_axScenes.Get(i);
		if (pxData && pxData->m_bIsLoaded && !pxData->m_bIsUnloading && pxData->m_strPath == strCanonical)
		{
			xScene.m_iHandle = static_cast<int>(i);
			xScene.m_uGeneration = s_axSceneGenerations.Get(i);
			return xScene;
		}
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
	Zenith_Log(LOG_CATEGORY_SCENE, "Cleared scene build index registry");
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

//==========================================================================
// Scene Loading (Synchronous)
//==========================================================================

Zenith_Scene Zenith_SceneManager::LoadScene(const std::string& strPath, Zenith_SceneLoadMode eMode)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "LoadScene must be called from main thread");

	// Unity parity: LoadScene called during script execution (Update/FixedUpdate/callbacks)
	// is deferred to next frame's ProcessPendingAsyncLoads, matching Unity's
	// EarlyUpdate.UpdatePreloading behavior. This prevents use-after-free when the
	// calling entity's scene is destroyed by SCENE_LOAD_SINGLE.
	if (s_bIsUpdating)
	{
		LoadSceneAsync(strPath, eMode);
		return Zenith_Scene::INVALID_SCENE;
	}

	// Canonicalize path for consistent detection and storage
	std::string strCanonicalPath = CanonicalizeScenePath(strPath);

	// Handle ADDITIVE_WITHOUT_LOADING - create empty scene without file load
	// Unity behavior: Creates scene entry for later population (e.g., procedural content)
	//
	// NOTE: This mode intentionally bypasses:
	//   - FireSceneLoadStartedCallbacks() - no file is being loaded
	//   - FireSceneLoadedCallbacks() - scene is created but not "loaded" from file
	//   - s_bIsLoadingScene flag - no async loading in progress
	//   - Circular load detection - no file operations involved
	//   - File existence check - no file required
	//
	// This is correct behavior matching Unity's CreateScene() semantics.
	// Use this mode for procedurally generated scenes or runtime-created content.
	if (eMode == SCENE_LOAD_ADDITIVE_WITHOUT_LOADING)
	{
		std::string strName = ExtractSceneNameFromPath(strCanonicalPath);
		Zenith_Scene xScene = CreateEmptyScene(strName);
		Zenith_SceneData* pxSceneData = GetSceneData(xScene);
		pxSceneData->m_strPath = strCanonicalPath;
		return xScene;
	}

	// Check if file exists before proceeding (use original path for file access)
	if (!Zenith_FileAccess::FileExists(strPath.c_str()))
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "LoadScene: File not found: %s", strPath.c_str());
		Zenith_Scene xInvalid;
		xInvalid.m_iHandle = -1;
		return xInvalid;
	}

	// Circular load detection - prevent a scene from loading itself during OnAwake/OnStart
	// Check both active loads and lifecycle dispatch (scenes currently in OnAwake/OnEnable)
	if (VectorContainsString(s_axCurrentlyLoadingPaths, strCanonicalPath) ||
		VectorContainsString(s_axLifecycleLoadStack, strCanonicalPath))
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "Circular scene load detected: %s", strCanonicalPath.c_str());
		Zenith_Scene xInvalid;
		xInvalid.m_iHandle = -1;
		return xInvalid;
	}
	s_axCurrentlyLoadingPaths.PushBack(strCanonicalPath);

	// Fire scene load started callbacks before any loading work
	FireSceneLoadStartedCallbacks(strCanonicalPath);

	s_bIsLoadingScene = true;

	// Unload existing scenes and reset render systems if single mode
	// Order is critical:
	// 1. ResetAllRenderSystems() - clears Flux render state
	// 2. UnloadAllNonPersistent() - destroys scene entities (colliders need physics world)
	// 3. Zenith_Physics::Reset() - resets physics AFTER collider destructors run
	// 4. Reset fixed timestep accumulator (Unity behavior: prevents multiple FixedUpdates after load)
	if (eMode == SCENE_LOAD_SINGLE)
	{
		ResetAllRenderSystems();
		CancelAllPendingAsyncLoads();
		UnloadAllNonPersistent();
		Zenith_Physics::Reset();
		s_fFixedTimeAccumulator = 0.0f;
	}

	// Create new scene with name extracted from path (Unity: scene.name is just filename without path/extension)
	std::string strName = ExtractSceneNameFromPath(strCanonicalPath);
	Zenith_Scene xScene = CreateEmptyScene(strName);
	Zenith_SceneData* pxSceneData = GetSceneData(xScene);
	pxSceneData->m_strPath = strCanonicalPath;  // Canonicalized path for consistent lookups

	// Unity parity: Scenes loaded additively are marked as subscenes
	if (eMode == SCENE_LOAD_ADDITIVE)
	{
		pxSceneData->m_bWasLoadedAdditively = true;
	}

	// Load scene data from file (use original path for file access)
	if (!pxSceneData->LoadFromFile(strPath))
	{
		// Loading failed - clean up the created scene
		Zenith_Error(LOG_CATEGORY_SCENE, "LoadScene: Failed to load '%s'", strPath.c_str());
		UnloadScene(xScene);
		s_bIsLoadingScene = false;
		s_axCurrentlyLoadingPaths.EraseValue(strCanonicalPath);
		return Zenith_Scene::INVALID_SCENE;
	}

	// Set active scene for SINGLE mode
	if (eMode == SCENE_LOAD_SINGLE)
	{
		Zenith_Scene xOldActive = GetActiveScene();
		Zenith_Assert(!s_bRenderTasksActive, "Cannot change active scene while render tasks are in flight");
		s_iActiveSceneHandle = xScene.m_iHandle;
		if (xOldActive != xScene)
		{
			FireActiveSceneChangedCallbacks(xOldActive, xScene);
		}
	}

	// Unity behavior: Awake -> OnEnable -> sceneLoaded -> Start(next frame)
	pxSceneData->DispatchAwakeForNewScene();
	pxSceneData->DispatchEnableAndPendingStartsForNewScene();
	FireSceneLoadedCallbacks(xScene, eMode);

	s_bIsLoadingScene = false;
	s_axCurrentlyLoadingPaths.EraseValue(strCanonicalPath);
	return xScene;
}

Zenith_Scene Zenith_SceneManager::LoadSceneByIndex(int iBuildIndex, Zenith_SceneLoadMode eMode)
{
	// Unity parity: defer to async when called during script execution
	if (s_bIsUpdating)
	{
		LoadSceneAsyncByIndex(iBuildIndex, eMode);
		Zenith_Scene xInvalid;
		xInvalid.m_iHandle = -1;
		return xInvalid;
	}

	const u_int uBuildIndex = static_cast<u_int>(iBuildIndex);
	if (iBuildIndex >= 0 && uBuildIndex < s_axBuildIndexToPath.GetSize() && !s_axBuildIndexToPath.Get(uBuildIndex).empty())
	{
		Zenith_Scene xScene = LoadScene(s_axBuildIndexToPath.Get(uBuildIndex), eMode);
		if (xScene.IsValid())
		{
			GetSceneData(xScene)->m_iBuildIndex = iBuildIndex;
		}
		return xScene;
	}

	Zenith_Warning(LOG_CATEGORY_SCENE, "LoadSceneByIndex: No scene registered for build index %d", iBuildIndex);
	Zenith_Scene xInvalid;
	xInvalid.m_iHandle = -1;
	return xInvalid;
}

//==========================================================================
// Scene Loading (Asynchronous)
//==========================================================================

Zenith_SceneOperationID Zenith_SceneManager::LoadSceneAsync(const std::string& strPath, Zenith_SceneLoadMode eMode)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "LoadSceneAsync must be called from main thread");

	// Handle ADDITIVE_WITHOUT_LOADING immediately (no async work needed, no file required)
	// This creates an empty scene for procedural content, completing synchronously.
	if (eMode == SCENE_LOAD_ADDITIVE_WITHOUT_LOADING)
	{
		Zenith_SceneOperation* pxOp = new Zenith_SceneOperation();
		uint64_t ulOpID = AllocateOperationID();
		pxOp->m_ulOperationID = ulOpID;
		s_axActiveOperations.PushBack(pxOp);
		s_axOperationMap.PushBack({ ulOpID, pxOp });

		std::string strCanonicalPath = CanonicalizeScenePath(strPath);
		std::string strName = ExtractSceneNameFromPath(strCanonicalPath);
		Zenith_Scene xScene = CreateEmptyScene(strName);
		GetSceneData(xScene)->m_strPath = strCanonicalPath;

		pxOp->SetResultSceneHandle(xScene.m_iHandle);
		pxOp->SetProgress(1.0f);
		pxOp->SetComplete(true);
		pxOp->FireCompletionCallback();
		return ulOpID;
	}

	Zenith_SceneOperation* pxOp = new Zenith_SceneOperation();
	uint64_t ulOpID = AllocateOperationID();
	pxOp->m_ulOperationID = ulOpID;
	s_axActiveOperations.PushBack(pxOp);
	s_axOperationMap.PushBack({ ulOpID, pxOp });

	// Check if file exists (use original path for file access)
	if (!Zenith_FileAccess::FileExists(strPath.c_str()))
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "LoadSceneAsync: File not found: %s", strPath.c_str());
		pxOp->SetResultSceneHandle(-1);
		pxOp->SetFailed(true);
		pxOp->SetProgress(1.0f);
		pxOp->SetComplete(true);
		pxOp->FireCompletionCallback();
		return ulOpID;
	}

	// Canonicalize path for consistent storage and lookups
	std::string strCanonicalPath = CanonicalizeScenePath(strPath);

	// Circular load detection - prevent loading a scene that's already being loaded
	// Check both active loads and lifecycle dispatch (scenes currently in OnAwake/OnEnable)
	// THREAD SAFETY NOTE: Both sets are only modified on the main thread:
	//   - insert() called here in LoadSceneAsync() (main thread, asserted at function start)
	//   - erase() called in ProcessPendingAsyncLoads() (main thread, called from Update)
	// The worker thread (AsyncSceneLoadTask) never accesses these sets, so no synchronization needed.
	if (VectorContainsString(s_axCurrentlyLoadingPaths, strCanonicalPath) ||
		VectorContainsString(s_axLifecycleLoadStack, strCanonicalPath))
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "Circular async scene load detected: %s", strCanonicalPath.c_str());
		pxOp->SetResultSceneHandle(-1);
		pxOp->SetFailed(true);
		pxOp->SetProgress(1.0f);
		pxOp->SetComplete(true);
		pxOp->FireCompletionCallback();
		return ulOpID;
	}
	s_axCurrentlyLoadingPaths.PushBack(strCanonicalPath);

	// Warn if exceeding max concurrent async loads
	if (s_axAsyncJobs.GetSize() >= s_uMaxConcurrentAsyncLoads)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "LoadSceneAsync: Maximum concurrent loads (%u) reached, load will proceed: %s",
			s_uMaxConcurrentAsyncLoads, strCanonicalPath.c_str());
	}

	FireSceneLoadStartedCallbacks(strCanonicalPath);

	// Create async job - store both original path (for file I/O) and canonical path
	AsyncLoadJob* pxJob = new AsyncLoadJob();
	pxJob->m_strPath = strPath;              // Keep original for file loading
	pxJob->m_strCanonicalPath = strCanonicalPath;  // Store canonical for cleanup/tracking
	pxJob->m_eMode = eMode;
	pxJob->m_pxOperation = pxOp;
	pxJob->m_pxLoadedData = new Zenith_DataStream();

	// Create and submit task for file loading on worker thread
	pxJob->m_pxTask = new Zenith_Task(ZENITH_PROFILE_INDEX__ASSET_LOAD, AsyncSceneLoadTask, pxJob);
	Zenith_TaskSystem::SubmitTask(pxJob->m_pxTask);

	s_axAsyncJobs.PushBack(pxJob);
	s_bAsyncJobsNeedSort = true;
	return ulOpID;
}

Zenith_SceneOperationID Zenith_SceneManager::LoadSceneAsyncByIndex(int iBuildIndex, Zenith_SceneLoadMode eMode)
{
	const u_int uBuildIndex = static_cast<u_int>(iBuildIndex);
	if (iBuildIndex < 0 || uBuildIndex >= s_axBuildIndexToPath.GetSize() || s_axBuildIndexToPath.Get(uBuildIndex).empty())
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "LoadSceneAsyncByIndex: No scene registered for build index %d", iBuildIndex);

		// Create failed operation
		Zenith_SceneOperation* pxOp = new Zenith_SceneOperation();
		uint64_t ulOpID = AllocateOperationID();
		pxOp->m_ulOperationID = ulOpID;
		s_axActiveOperations.PushBack(pxOp);
		s_axOperationMap.PushBack({ ulOpID, pxOp });
		pxOp->SetResultSceneHandle(-1);
		pxOp->SetFailed(true);
		pxOp->SetProgress(1.0f);
		pxOp->SetComplete(true);
		pxOp->FireCompletionCallback();
		return ulOpID;
	}

	// Delegate to LoadSceneAsync for the actual work
	Zenith_SceneOperationID ulOpID = LoadSceneAsync(s_axBuildIndexToPath.Get(uBuildIndex), eMode);

	// Find the newly created job and set its build index
	// The job was just added, so it's at the end of the vector
	if (s_axAsyncJobs.GetSize() > 0)
	{
		AsyncLoadJob* pxJob = s_axAsyncJobs.GetBack();
		if (pxJob && pxJob->m_pxOperation && pxJob->m_pxOperation->m_ulOperationID == ulOpID)
		{
			pxJob->m_iBuildIndex = iBuildIndex;
		}
	}

	return ulOpID;
}

//==========================================================================
// Scene Unloading
//==========================================================================

bool Zenith_SceneManager::CanUnloadScene(Zenith_Scene xScene)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "CanUnloadScene must be called from main thread");

	if (!xScene.IsValid())
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "CanUnloadScene: Invalid scene");
		return false;
	}

	// Cannot unload persistent scene
	if (xScene.m_iHandle == s_iPersistentSceneHandle)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "Cannot unload persistent scene");
		return false;
	}

	// Cannot unload a scene that's already being async unloaded
	Zenith_SceneData* pxSceneData = GetSceneData(xScene);
	if (pxSceneData && pxSceneData->m_bIsUnloading)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "Cannot unload scene that is already being async unloaded");
		return false;
	}

	// Cannot unload the last loaded scene (Unity behavior)
	// Count non-persistent scenes that are fully loaded and usable, excluding scenes
	// that are still being async-loaded (not yet activated) or async-unloaded
	uint32_t uNonPersistentCount = 0;
	for (u_int i = 0; i < s_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = s_axScenes.Get(i);
		if (pxData != nullptr && static_cast<int>(i) != s_iPersistentSceneHandle
			&& pxData->m_bIsLoaded && pxData->m_bIsActivated && !pxData->m_bIsUnloading)
		{
			uNonPersistentCount++;
		}
	}
	if (uNonPersistentCount <= 1)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "Cannot unload the last loaded scene");
		return false;
	}

	return true;
}

void Zenith_SceneManager::UnloadSceneInternal(Zenith_Scene xScene)
{
	// Fire unloading callback BEFORE destruction (allows access to scene data)
	FireSceneUnloadingCallbacks(xScene);

	// Track if we're unloading the active scene
	bool bWasActiveScene = (xScene.m_iHandle == s_iActiveSceneHandle);

	// Free the scene data
	if (xScene.m_iHandle >= 0 && xScene.m_iHandle < static_cast<int>(s_axScenes.GetSize()))
	{
		delete s_axScenes.Get(xScene.m_iHandle);
		s_axScenes.Get(xScene.m_iHandle) = nullptr;

		// Fire unloaded callback BEFORE incrementing generation so the handle
		// is still valid for identification in callbacks (Unity parity)
		FireSceneUnloadedCallbacks(xScene);

		FreeSceneHandle(xScene.m_iHandle);
	}

	// If active scene was unloaded, select a new active scene
	if (bWasActiveScene)
	{
		Zenith_Assert(!s_bRenderTasksActive, "Cannot change active scene while render tasks are in flight");
		s_iActiveSceneHandle = SelectNewActiveScene();
		Zenith_Scene xNewActive = GetActiveScene();
		FireActiveSceneChangedCallbacks(xScene, xNewActive);
	}
}

void Zenith_SceneManager::UnloadSceneForced(Zenith_Scene xScene)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "UnloadSceneForced must be called from main thread");

	if (!xScene.IsValid())
	{
		return;
	}

	// Cannot unload persistent scene even when forced
	if (xScene.m_iHandle == s_iPersistentSceneHandle)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "Cannot unload persistent scene");
		return;
	}

	UnloadSceneInternal(xScene);
}

void Zenith_SceneManager::UnloadScene(Zenith_Scene xScene)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "UnloadScene must be called from main thread");

	if (!CanUnloadScene(xScene))
	{
		return;
	}

	UnloadSceneInternal(xScene);
}


Zenith_SceneOperationID Zenith_SceneManager::UnloadSceneAsync(Zenith_Scene xScene)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "UnloadSceneAsync must be called from main thread");

	Zenith_SceneOperation* pxOp = new Zenith_SceneOperation();
	uint64_t ulOpID = AllocateOperationID();
	pxOp->m_ulOperationID = ulOpID;
	s_axActiveOperations.PushBack(pxOp);
	s_axOperationMap.PushBack({ ulOpID, pxOp });

	if (!CanUnloadScene(xScene))
	{
		pxOp->SetProgress(1.0f);
		pxOp->SetFailed(true);
		pxOp->SetComplete(true);
		pxOp->FireCompletionCallback();
		return ulOpID;
	}

	Zenith_SceneData* pxSceneData = GetSceneData(xScene);
	if (!pxSceneData)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "UnloadSceneAsync: Invalid scene data");
		pxOp->SetProgress(1.0f);
		pxOp->SetComplete(true);
		pxOp->FireCompletionCallback();
		return ulOpID;
	}

	// Mark scene as unloading immediately so IsLoaded() returns false,
	// SetActiveScene rejects it, and duplicate UnloadSceneAsync calls are prevented
	pxSceneData->m_bIsUnloading = true;

	// Create async unload job
	AsyncUnloadJob* pxJob = new AsyncUnloadJob();
	pxJob->m_iSceneHandle = xScene.m_iHandle;
	pxJob->m_uSceneGeneration = xScene.m_uGeneration;
	pxJob->m_pxOperation = pxOp;
	pxJob->m_uTotalEntities = pxSceneData->GetEntityCount();
	pxJob->m_uDestroyedEntities = 0;
	pxJob->m_bUnloadingCallbackFired = false;

	s_axAsyncUnloadJobs.PushBack(pxJob);
	return ulOpID;
}

//==========================================================================
// Entity Destruction
//==========================================================================

void Zenith_SceneManager::Destroy(Zenith_Entity& xEntity)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "Destroy must be called from main thread");

	if (!xEntity.IsValid()) return;

	Zenith_SceneData* pxSceneData = xEntity.GetSceneData();
	if (pxSceneData)
	{
		pxSceneData->MarkForDestruction(xEntity.GetEntityID());
	}
}

void Zenith_SceneManager::Destroy(Zenith_Entity& xEntity, float fDelay)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "Destroy must be called from main thread");

	if (!xEntity.IsValid()) return;

	if (fDelay <= 0.0f)
	{
		Destroy(xEntity);
		return;
	}

	Zenith_SceneData* pxSceneData = xEntity.GetSceneData();
	if (pxSceneData)
	{
		pxSceneData->MarkForTimedDestruction(xEntity.GetEntityID(), fDelay);
	}
}

void Zenith_SceneManager::DestroyImmediate(Zenith_Entity& xEntity)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "DestroyImmediate must be called from main thread");

	if (!xEntity.IsValid()) return;

	Zenith_SceneData* pxSceneData = xEntity.GetSceneData();
	if (pxSceneData)
	{
		Zenith_EntityID xEntityID = xEntity.GetEntityID();

		// Clear from pending destruction if present (use global slot flag)
		if (xEntityID.m_uIndex < Zenith_SceneData::s_axEntitySlots.GetSize())
		{
			Zenith_SceneData::s_axEntitySlots.Get(xEntityID.m_uIndex).m_bMarkedForDestruction = false;
		}

		// Remove from pending destruction list to prevent stale processing in ProcessPendingDestructions
		pxSceneData->m_xPendingDestruction.EraseValue(xEntityID);

		// Detach from parent to clean up parent's child list (Unity parity:
		// DestroyImmediate(child) decreases parent.transform.childCount immediately)
		Zenith_Entity xLocalEntity(pxSceneData, xEntityID);
		xLocalEntity.GetComponent<Zenith_TransformComponent>().DetachFromParent();

		// RemoveEntity handles child destruction recursively and dispatches
		// OnDisable/OnDestroy for every entity (including children)
		pxSceneData->RemoveEntity(xEntityID);
	}
}

//==========================================================================
// Scene Management
//==========================================================================

bool Zenith_SceneManager::SetActiveScene(Zenith_Scene xScene)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "SetActiveScene must be called from main thread");

	if (!xScene.IsValid())
	{
		return false;
	}

	Zenith_SceneData* pxSceneData = GetSceneData(xScene);
	if (!pxSceneData || !pxSceneData->m_bIsLoaded)
	{
		return false;
	}

	// Cannot set a scene as active if it's being unloaded
	if (pxSceneData->m_bIsUnloading)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "Cannot set unloading scene as active");
		return false;
	}

	// Unity behavior: Don't fire callback if same scene
	// Use operator== to validate both handle AND generation counter
	Zenith_Scene xCurrent = GetActiveScene();
	if (xCurrent == xScene)
	{
		return true;
	}

	Zenith_Assert(!s_bRenderTasksActive, "Cannot change active scene while render tasks are in flight");
	s_iActiveSceneHandle = xScene.m_iHandle;
	FireActiveSceneChangedCallbacks(xCurrent, xScene);
	return true;
}

void Zenith_SceneManager::SetScenePaused(Zenith_Scene xScene, bool bPaused)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "SetScenePaused must be called from main thread");

	Zenith_SceneData* pxSceneData = GetSceneData(xScene);
	if (pxSceneData)
	{
		pxSceneData->SetPaused(bPaused);
	}
}

bool Zenith_SceneManager::IsScenePaused(Zenith_Scene xScene)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "IsScenePaused must be called from main thread");

	Zenith_SceneData* pxSceneData = GetSceneData(xScene);
	return pxSceneData ? pxSceneData->IsPaused() : false;
}

void Zenith_SceneManager::UnloadUnusedAssets()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "UnloadUnusedAssets must be called from main thread");

	// TODO(flux-refcount): Implement once Flux asset managers support reference counting.
	// This method should delegate to:
	// - Flux_TextureManager::UnloadUnused()
	// - Flux_ModelManager::UnloadUnused()
	// - Flux_MaterialManager::UnloadUnused()
	// - Flux_ShaderManager::UnloadUnused()
	// etc.
	//
	// For now, log a warning so developers know this is not yet implemented.
	static bool ls_bWarnedOnce = false;
	if (!ls_bWarnedOnce)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE,
			"UnloadUnusedAssets: Not yet implemented - Flux asset managers need reference counting support. "
			"Assets will remain in memory after scene unloads.");
		ls_bWarnedOnce = true;
	}
}

// ============================================================================
// Multi-Scene Rendering Helpers
// ============================================================================

Zenith_CameraComponent* Zenith_SceneManager::FindMainCameraAcrossScenes()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread() || s_bRenderTasksActive, "FindMainCameraAcrossScenes must be called from main thread or during render task execution");
	// Try active scene first (common case)
	Zenith_SceneData* pxActiveData = GetSceneData(GetActiveScene());
	if (pxActiveData)
	{
		Zenith_CameraComponent* pxCamera = pxActiveData->TryGetMainCamera();
		if (pxCamera)
			return pxCamera;
	}

	// Search all loaded scenes (finds camera in persistent scene, etc.)
	for (u_int i = 0; i < s_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = s_axScenes.Get(i);
		if (pxData && pxData->IsLoaded() && !pxData->IsUnloading())
		{
			Zenith_CameraComponent* pxCamera = pxData->TryGetMainCamera();
			if (pxCamera)
				return pxCamera;
		}
	}

	return nullptr;
}

uint32_t Zenith_SceneManager::GetSceneSlotCount()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread() || s_bRenderTasksActive, "GetSceneSlotCount must be called from main thread or during render task execution");
	return s_axScenes.GetSize();
}

Zenith_SceneData* Zenith_SceneManager::GetSceneDataAtSlot(uint32_t uIndex)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread() || s_bRenderTasksActive, "GetSceneDataAtSlot must be called from main thread or during render task execution");
	if (uIndex >= s_axScenes.GetSize())
		return nullptr;
	return s_axScenes.Get(uIndex);
}

// Select best scene to become active when the current active scene is unloaded.
// Unity behaviour: prefers the loaded scene with the lowest build index.
// Falls back to the most recently loaded scene if none have build indices.
// iExcludeHandle: scene handle to exclude (e.g. the scene being unloaded)
int Zenith_SceneManager::SelectNewActiveScene(int iExcludeHandle)
{
	int iBestHandle = -1;
	int iBestBuildIndex = -1;
	uint64_t ulBestTimestamp = 0;

	for (u_int i = 0; i < s_axScenes.GetSize(); ++i)
	{
		if (static_cast<int>(i) == s_iPersistentSceneHandle) continue;
		if (static_cast<int>(i) == iExcludeHandle) continue;

		Zenith_SceneData* pxCandidate = s_axScenes.Get(i);
		if (!pxCandidate || !pxCandidate->m_bIsLoaded || pxCandidate->m_bIsUnloading)
			continue;

		// Prefer scene with lowest build index (Unity parity)
		if (pxCandidate->m_iBuildIndex >= 0)
		{
			if (iBestBuildIndex < 0 || pxCandidate->m_iBuildIndex < iBestBuildIndex)
			{
				iBestBuildIndex = pxCandidate->m_iBuildIndex;
				iBestHandle = static_cast<int>(i);
			}
		}
		// Fall back to most recently loaded if no build index match yet
		else if (iBestBuildIndex < 0 && pxCandidate->m_ulLoadTimestamp > ulBestTimestamp)
		{
			ulBestTimestamp = pxCandidate->m_ulLoadTimestamp;
			iBestHandle = static_cast<int>(i);
		}
	}

	return (iBestHandle >= 0) ? iBestHandle : s_iPersistentSceneHandle;
}

// Internal helper that moves entity between scenes using zero-copy transfer.
// EntityID is preserved (globally unique) - no serialize/deserialize.
// No lifecycle events fire (Unity parity: MoveGameObjectToScene is seamless).
// Returns true on success, false on failure.
bool Zenith_SceneManager::MoveEntityInternal(Zenith_Entity& xEntity, Zenith_SceneData* pxTargetData)
{
	Zenith_SceneData* pxSourceData = xEntity.GetSceneData();
	if (!pxSourceData || pxSourceData == pxTargetData)
	{
		return false;
	}

	Zenith_EntityID xEntityID = xEntity.GetEntityID();
	Zenith_Vector<Zenith_EntityID> axChildIDs = xEntity.GetChildEntityIDs();

	// Move children recursively FIRST (depth-first)
	for (u_int i = 0; i < axChildIDs.GetSize(); ++i)
	{
		Zenith_Entity xChild = pxSourceData->TryGetEntity(axChildIDs.Get(i));
		if (xChild.IsValid())
		{
			if (!MoveEntityInternal(xChild, pxTargetData))
			{
				Zenith_Warning(LOG_CATEGORY_SCENE, "MoveEntityInternal: Failed to move child entity (idx=%u)", axChildIDs.Get(i).m_uIndex);
				return false;
			}
		}
	}

	// Transfer all components from source pools to target pools (move-construct, zero-copy)
	Zenith_ComponentMetaRegistry::Get().TransferAllComponents(xEntityID, pxSourceData, pxTargetData);

	// Update global slot to point to target scene
	Zenith_SceneData::s_axEntitySlots.Get(xEntityID.m_uIndex).m_iSceneHandle = pxTargetData->m_iHandle;

	// Move entity from source's active list to target's active list
	pxSourceData->m_xActiveEntities.EraseValue(xEntityID);
	pxTargetData->m_xActiveEntities.PushBack(xEntityID);

	// Transfer timed destructions for this entity from source to target scene
	for (int i = static_cast<int>(pxSourceData->m_axTimedDestructions.GetSize()) - 1; i >= 0; --i)
	{
		if (pxSourceData->m_axTimedDestructions.Get(i).m_xEntityID == xEntityID)
		{
			pxTargetData->m_axTimedDestructions.PushBack(pxSourceData->m_axTimedDestructions.Get(i));
			pxSourceData->m_axTimedDestructions.Remove(i);
		}
	}

	// Adjust pending start count and list if this entity hasn't had Start() called yet
	Zenith_SceneData::Zenith_EntitySlot& xSlot = Zenith_SceneData::s_axEntitySlots.Get(xEntityID.m_uIndex);
	if (xSlot.IsPendingStart())
	{
		Zenith_Assert(pxSourceData->m_uPendingStartCount > 0, "PendingStartCount underflow in MoveEntityInternal");
		pxSourceData->m_uPendingStartCount--;
		pxTargetData->m_uPendingStartCount++;
		pxSourceData->m_axPendingStartEntities.EraseValue(xEntityID);
		pxTargetData->m_axPendingStartEntities.PushBack(xEntityID);
	}

	// Transfer newly created entity tracking so OnAwake is dispatched by the correct scene.
	// Without this, entities moved before their first Update (e.g. DontDestroyOnLoad in
	// Project_LoadInitialScene) would have OnAwake dispatched by the source scene with the
	// wrong scene data, causing HasComponent to fail and OnAwake to never fire.
	pxSourceData->m_axNewlyCreatedEntities.EraseValue(xEntityID);
	pxTargetData->m_axNewlyCreatedEntities.PushBack(xEntityID);

	// Handle main camera reference transfer
	bool bWasSourceMainCamera = (pxSourceData->GetMainCameraEntity() == xEntityID);
	if (bWasSourceMainCamera)
	{
		pxSourceData->SetMainCameraEntity(INVALID_ENTITY_ID);
		if (!pxTargetData->GetMainCameraEntity().IsValid())
		{
			pxTargetData->SetMainCameraEntity(xEntityID);
		}
	}

	// Invalidate root caches on both scenes
	pxSourceData->InvalidateRootEntityCache();
	pxTargetData->InvalidateRootEntityCache();
	pxSourceData->MarkDirty();
	pxTargetData->MarkDirty();

	// Entity handle stays valid - EntityID is unchanged, global slot points to new scene
	return true;
}

bool Zenith_SceneManager::MoveEntityToScene(Zenith_Entity& xEntity, Zenith_Scene xTarget)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "MoveEntityToScene must be called from main thread");

	if (!xEntity.IsValid())
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "MoveEntityToScene: Invalid entity");
		return false;
	}

	if (!xTarget.IsValid())
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "MoveEntityToScene: Invalid target scene");
		return false;
	}

	Zenith_SceneData* pxTargetData = GetSceneData(xTarget);
	if (!pxTargetData)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "MoveEntityToScene: Invalid target scene data");
		return false;
	}

	if (pxTargetData->m_bIsUnloading)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "MoveEntityToScene: Target scene '%s' is being unloaded", pxTargetData->m_strName.c_str());
		return false;
	}

	// Validate source and target are different scenes
	Zenith_SceneData* pxSourceData = xEntity.GetSceneData();
	if (pxSourceData == pxTargetData)
	{
		// Moving to same scene is a no-op (not an error)
		return true;
	}

	// Unity parity: Only root entities can be moved between scenes (Unity throws InvalidOperationException)
	if (xEntity.GetParentEntityID().IsValid())
	{
		Zenith_Error(LOG_CATEGORY_SCENE,
			"MoveEntityToScene: Entity '%s' has a parent. Only root entities can be moved between scenes.",
			xEntity.GetName().c_str());
		return false;
	}

	// MoveEntityInternal updates xEntity in place to point to the new entity
	return MoveEntityInternal(xEntity, pxTargetData);
}

bool Zenith_SceneManager::MergeScenes(Zenith_Scene xSource, Zenith_Scene xTarget)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "MergeScenes must be called from main thread");

	Zenith_SceneData* pxSource = GetSceneData(xSource);
	Zenith_SceneData* pxTarget = GetSceneData(xTarget);

	if (!pxSource || !pxTarget)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "MergeScenes: Invalid source or target scene");
		return false;
	}

	if (pxSource == pxTarget)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "MergeScenes: Source and target are the same scene");
		return false;
	}

	// Cannot merge from the persistent scene
	if (xSource.m_iHandle == s_iPersistentSceneHandle)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "MergeScenes: Cannot merge from persistent scene");
		return false;
	}

	// Unity behavior: If source is active, target becomes active
	if (xSource.m_iHandle == s_iActiveSceneHandle)
	{
		SetActiveScene(xTarget);
	}

	// Get all root entities from source
	Zenith_Vector<Zenith_Entity> axRoots;
	xSource.GetRootEntities(axRoots);

	// Move each root (and children recursively)
	for (u_int i = 0; i < axRoots.GetSize(); ++i)
	{
		Zenith_Entity& xRoot = axRoots.Get(i);
		MoveEntityToScene(xRoot, xTarget);
	}

	// Unload the now-empty source scene (forced - bypasses last-scene guard since source is empty)
	UnloadSceneForced(xSource);
	return true;
}

//==========================================================================
// Entity Persistence
//==========================================================================

void Zenith_SceneManager::MarkEntityPersistent(Zenith_Entity& xEntity)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "MarkEntityPersistent must be called from main thread");

	if (!xEntity.IsValid()) return;

	// Unity parity: DontDestroyOnLoad moves the ROOT of the hierarchy,
	// not just the target entity. Walk up to find the root.
	Zenith_Entity xRoot = xEntity;
	while (xRoot.GetParentEntityID().IsValid())
	{
		Zenith_SceneData* pxSceneData = xRoot.GetSceneData();
		if (!pxSceneData) break;
		Zenith_EntityID xParentID = xRoot.GetParentEntityID();
		if (!pxSceneData->EntityExists(xParentID)) break;
		xRoot = pxSceneData->GetEntity(xParentID);
	}

	Zenith_Scene xPersistent = GetPersistentScene();
	MoveEntityToScene(xRoot, xPersistent);

	// Fire callback AFTER transfer so the entity reference is valid in the new scene
	// Note: Unity's DontDestroyOnLoad doesn't have a callback; this is a Zenith extension
	FireEntityPersistentCallbacks(xRoot);
}

Zenith_Scene Zenith_SceneManager::GetPersistentScene()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetPersistentScene must be called from main thread");
	Zenith_Scene xScene;
	xScene.m_iHandle = s_iPersistentSceneHandle;
	if (s_iPersistentSceneHandle >= 0 && s_iPersistentSceneHandle < static_cast<int>(s_axSceneGenerations.GetSize()))
	{
		xScene.m_uGeneration = s_axSceneGenerations.Get(s_iPersistentSceneHandle);
	}
	return xScene;
}

//==========================================================================
// Event Callbacks
//==========================================================================

// Helper to allocate operation IDs with overflow protection (skip 0 = ZENITH_INVALID_OPERATION_ID)
Zenith_SceneOperationID Zenith_SceneManager::AllocateOperationID()
{
	if (s_ulNextOperationID == UINT64_MAX)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE,
			"Operation ID counter wrapped around after %llu allocations",
			static_cast<unsigned long long>(s_ulNextOperationID));
		s_ulNextOperationID = 1;
	}
	return s_ulNextOperationID++;
}

// Helper to allocate callback handles with overflow protection
Zenith_SceneManager::CallbackHandle Zenith_SceneManager::AllocateCallbackHandle()
{
	// Wrap around on overflow (skip 0 which is INVALID_CALLBACK_HANDLE)
	// Note: Collision with an active callback is astronomically unlikely - it would require
	// a callback registered 18 quintillion registrations ago to still be active.
	if (s_ulNextCallbackHandle == UINT64_MAX)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE,
			"Callback handle counter wrapped around after %llu registrations",
			static_cast<unsigned long long>(s_ulNextCallbackHandle));
		s_ulNextCallbackHandle = 1;  // Skip 0 (INVALID_CALLBACK_HANDLE)
	}
	return s_ulNextCallbackHandle++;
}

// Templatized callback list implementation
template<typename TCallback>
Zenith_SceneManager::CallbackHandle Zenith_SceneManager::Zenith_CallbackList<TCallback>::Register(TCallback pfn)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "Callback registration must be on main thread");
	CallbackHandle ulHandle = AllocateCallbackHandle();
	if (ulHandle == INVALID_CALLBACK_HANDLE) return ulHandle;
	m_axEntries.PushBack({ ulHandle, pfn });
	return ulHandle;
}

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

//==========================================================================
// Internal
//==========================================================================

void Zenith_SceneManager::AsyncSceneLoadTask(void* pData)
{
	AsyncLoadJob* pxJob = static_cast<AsyncLoadJob*>(pData);

	// Set file read started milestone
	// Note: Using memory_order_release to ensure consistent ordering with FILE_READ_COMPLETE
	pxJob->m_eMilestone.store(FileLoadMilestone::FILE_READ_STARTED, std::memory_order_release);

	// Read file data on worker thread
	// NOTE: Progress jumps from 0.1 to 0.7 during file read because DataStream::ReadFromFile
	// is a blocking operation with no progress callback. Unlike Unity's AsyncOperation which
	// can report smooth progress during asset loading, Zenith's progress only updates at
	// milestone boundaries. For smoother progress during large file loads, consider:
	// - Implementing chunked reading in DataStream with progress callbacks
	// - Using memory-mapped I/O with incremental processing
	// - Artificial progress interpolation on the main thread (not recommended)
	pxJob->m_pxLoadedData->ReadFromFile(pxJob->m_strPath.c_str());

	// File read complete milestone
	pxJob->m_eMilestone.store(FileLoadMilestone::FILE_READ_COMPLETE, std::memory_order_release);

	pxJob->m_bFileLoadComplete.store(true, std::memory_order_release);
}

Zenith_SceneOperation* Zenith_SceneManager::GetOperation(Zenith_SceneOperationID ulID)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetOperation must be called from main thread");

	if (ulID == ZENITH_INVALID_OPERATION_ID)
	{
		return nullptr;
	}
	for (u_int i = 0; i < s_axOperationMap.GetSize(); ++i)
	{
		if (s_axOperationMap.Get(i).m_ulOperationID == ulID)
			return s_axOperationMap.Get(i).m_pxOperation;
	}
	return nullptr;
}

bool Zenith_SceneManager::IsOperationValid(Zenith_SceneOperationID ulID)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "IsOperationValid must be called from main thread");

	if (ulID == ZENITH_INVALID_OPERATION_ID)
	{
		return false;
	}
	for (u_int i = 0; i < s_axOperationMap.GetSize(); ++i)
	{
		if (s_axOperationMap.Get(i).m_ulOperationID == ulID) return true;
	}
	return false;
}

void Zenith_SceneManager::CleanupCompletedOperations()
{
	// Remove completed operations after a delay to allow result access
	// Users need time to call GetResultScene() after IsComplete() returns true
	for (int i = static_cast<int>(s_axActiveOperations.GetSize()) - 1; i >= 0; --i)
	{
		Zenith_SceneOperation* pxOp = s_axActiveOperations.Get(static_cast<u_int>(i));
		if (pxOp && pxOp->IsComplete())
		{
			pxOp->m_uFramesSinceComplete++;
			if (pxOp->m_uFramesSinceComplete > uOPERATION_CLEANUP_DELAY_FRAMES)
			{
				// Remove from operation map before deleting
				for (u_int j = 0; j < s_axOperationMap.GetSize(); ++j)
				{
					if (s_axOperationMap.Get(j).m_ulOperationID == pxOp->m_ulOperationID)
					{
						s_axOperationMap.RemoveSwap(j);
						break;
					}
				}
				delete pxOp;
				s_axActiveOperations.Remove(static_cast<u_int>(i));
			}
		}
	}
}

void Zenith_SceneManager::CancelAllPendingAsyncLoads(AsyncLoadJob* pxExclude)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "CancelAllPendingAsyncLoads must be called from main thread");

	for (int i = static_cast<int>(s_axAsyncJobs.GetSize()) - 1; i >= 0; --i)
	{
		AsyncLoadJob* pxJob = s_axAsyncJobs.Get(static_cast<u_int>(i));
		if (pxJob == pxExclude) continue;

		Zenith_SceneOperation* pxOp = pxJob->m_pxOperation;

		// If the scene was already created during Phase 1, delete it
		if (pxJob->m_iCreatedSceneHandle >= 0)
		{
			Zenith_SceneData* pxSceneData = GetSceneDataByHandle(pxJob->m_iCreatedSceneHandle);
			if (pxSceneData)
			{
				delete pxSceneData;
				s_axScenes.Get(pxJob->m_iCreatedSceneHandle) = nullptr;
				FreeSceneHandle(pxJob->m_iCreatedSceneHandle);
			}
		}

		FailAsyncLoadOperation(pxOp);
		CleanupAndRemoveAsyncJob(static_cast<u_int>(i));
	}
}

void Zenith_SceneManager::FailAsyncLoadOperation(Zenith_SceneOperation* pxOp)
{
	pxOp->SetResultSceneHandle(-1);
	pxOp->SetFailed(true);
	pxOp->SetProgress(1.0f);
	pxOp->SetComplete(true);
	pxOp->FireCompletionCallback();
}

void Zenith_SceneManager::CleanupAndRemoveAsyncJob(u_int uIndex)
{
	AsyncLoadJob* pxJob = s_axAsyncJobs.Get(uIndex);
	if (pxJob->m_pxTask)
	{
		pxJob->m_pxTask->WaitUntilComplete();
	}
	s_axCurrentlyLoadingPaths.EraseValue(pxJob->m_strCanonicalPath);
	delete pxJob;
	s_axAsyncJobs.Remove(uIndex);
}

void Zenith_SceneManager::ProcessPendingAsyncLoads()
{
	// Sort pending jobs by priority (higher priority first) using insertion sort.
	// Insertion sort is O(n^2) but chosen intentionally because:
	// 1. Typical N is very small (1-4 concurrent loads in most games)
	// 2. It's stable (preserves FIFO order for jobs with equal priorities)
	// 3. It's in-place with minimal allocation overhead
	// 4. It's adaptive: O(n) when already sorted, which is the common case
	// For large N (>50 concurrent loads), consider std::stable_sort instead.
	// Only re-sort when jobs have been added or priorities may have changed.
	if (s_bAsyncJobsNeedSort && s_axAsyncJobs.GetSize() > 1)
	{
		for (u_int i = 1; i < s_axAsyncJobs.GetSize(); ++i)
		{
			AsyncLoadJob* pxJob = s_axAsyncJobs.Get(i);
			int iPriority = pxJob->m_pxOperation->GetPriority();
			u_int j = i;
			while (j > 0 && s_axAsyncJobs.Get(j - 1)->m_pxOperation->GetPriority() < iPriority)
			{
				s_axAsyncJobs.Get(j) = s_axAsyncJobs.Get(j - 1);
				j--;
			}
			s_axAsyncJobs.Get(j) = pxJob;
		}
		s_bAsyncJobsNeedSort = false;
	}

	// Process pending async loads - iterate forward, highest priority processed first
	for (u_int i = 0; i < s_axAsyncJobs.GetSize(); )
	{
		AsyncLoadJob* pxJob = s_axAsyncJobs.Get(i);
		Zenith_SceneOperation* pxOp = pxJob->m_pxOperation;

		// Check for cancellation request
		if (pxOp->IsCancellationRequested())
		{
			Zenith_Log(LOG_CATEGORY_SCENE, "Async scene load cancelled: %s", pxJob->m_strCanonicalPath.c_str());

			// If the scene was already created in Phase 1, unload it
			// Use UnloadSceneForced to bypass CanUnloadScene - the cancelled scene
			// is not activated, so it wouldn't be counted as a "usable" scene by the
			// last-scene protection check
			if (pxJob->m_ePhase == AsyncLoadJob::LoadPhase::DESERIALIZED && pxJob->m_iCreatedSceneHandle >= 0)
			{
				Zenith_Scene xCreatedScene;
				xCreatedScene.m_iHandle = pxJob->m_iCreatedSceneHandle;
				xCreatedScene.m_uGeneration = s_axSceneGenerations.Get(xCreatedScene.m_iHandle);
				UnloadSceneForced(xCreatedScene);
			}

			FailAsyncLoadOperation(pxOp);
			CleanupAndRemoveAsyncJob(i);
			continue;
		}

		//----------------------------------------------------------------------
		// Phase 1: File I/O → create scene → deserialize
		//----------------------------------------------------------------------
		if (pxJob->m_ePhase == AsyncLoadJob::LoadPhase::WAITING_FOR_FILE)
		{
			// Check if file load is complete (happens on worker thread)
			// Read milestone with acquire ordering to ensure proper synchronization
			FileLoadMilestone eMilestone = pxJob->m_eMilestone.load(std::memory_order_acquire);
			if (!pxJob->m_bFileLoadComplete.load(std::memory_order_acquire))
			{
				// Convert milestone to progress value and report
				float fFileProgress = static_cast<float>(static_cast<uint8_t>(eMilestone)) / 100.0f;
				pxOp->SetProgress(fFileProgress);
				++i;
				continue;
			}

			// File loaded - proceed with scene creation and deserialization immediately
			// (even if activation is paused, so the scene is ready when activation is allowed)
			s_bIsLoadingScene = true;

			// Unload existing scenes if single mode
			if (pxJob->m_eMode == SCENE_LOAD_SINGLE)
			{
				ResetAllRenderSystems();
				CancelAllPendingAsyncLoads(pxJob);
				// pxJob is now the only element in s_axAsyncJobs (at index 0)
				i = 0;
				UnloadAllNonPersistent();
				Zenith_Physics::Reset();
				s_fFixedTimeAccumulator = 0.0f;  // Unity behavior: reset on scene load
			}

			// Create scene with name extracted from path
			const std::string& strCanonicalPath = pxJob->m_strCanonicalPath;
			std::string strName = ExtractSceneNameFromPath(strCanonicalPath);

			pxOp->SetProgress(fPROGRESS_SCENE_CREATED);

			Zenith_Scene xScene = CreateEmptyScene(strName);
			Zenith_SceneData* pxSceneData = GetSceneData(xScene);
			pxSceneData->m_strPath = strCanonicalPath;
			pxSceneData->m_iBuildIndex = pxJob->m_iBuildIndex;
			pxSceneData->m_bIsActivated = false;  // Not activated until Awake/OnEnable complete (Unity parity)

			if (pxJob->m_eMode == SCENE_LOAD_ADDITIVE)
			{
				pxSceneData->m_bWasLoadedAdditively = true;
			}

			pxOp->SetProgress(fPROGRESS_DESERIALIZE_START);

			// Deserialize from the pre-loaded data stream
			pxJob->m_pxLoadedData->SetCursor(0);
			if (!pxSceneData->LoadFromDataStream(*pxJob->m_pxLoadedData))
			{
				Zenith_Error(LOG_CATEGORY_SCENE, "LoadSceneAsync: Failed to deserialize '%s'", pxJob->m_strPath.c_str());
				UnloadScene(xScene);
				FailAsyncLoadOperation(pxOp);
				s_bIsLoadingScene = false;
				CleanupAndRemoveAsyncJob(i);
				continue;
			}

			pxOp->SetProgress(fPROGRESS_DESERIALIZE_COMPLETE);
			pxOp->SetResultSceneHandle(xScene.m_iHandle);

			// Scene is deserialized but dormant - transition to phase 2
			pxJob->m_iCreatedSceneHandle = xScene.m_iHandle;
			pxJob->m_ePhase = AsyncLoadJob::LoadPhase::DESERIALIZED;
			s_bIsLoadingScene = false;

			// Fall through to phase 2 check below (activation may already be allowed)
		}

		//----------------------------------------------------------------------
		// Phase 2: Activation (Awake/OnEnable) - only when allowed
		//----------------------------------------------------------------------
		if (pxJob->m_ePhase == AsyncLoadJob::LoadPhase::DESERIALIZED)
		{
			if (!pxOp->IsActivationAllowed())
			{
				pxOp->SetProgress(fPROGRESS_ACTIVATION_PAUSED);
				++i;
				continue;
			}

			s_bIsLoadingScene = true;

			Zenith_Scene xScene;
			xScene.m_iHandle = pxJob->m_iCreatedSceneHandle;
			xScene.m_uGeneration = s_axSceneGenerations.Get(xScene.m_iHandle);
			Zenith_SceneData* pxSceneData = GetSceneData(xScene);

			// Set active scene for SINGLE mode
			if (pxJob->m_eMode == SCENE_LOAD_SINGLE)
			{
				Zenith_Scene xOldActive = GetActiveScene();
				Zenith_Assert(!s_bRenderTasksActive, "Cannot change active scene while render tasks are in flight");
				s_iActiveSceneHandle = xScene.m_iHandle;
				if (xOldActive != xScene)
				{
					FireActiveSceneChangedCallbacks(xOldActive, xScene);
				}
			}

			// Unity behavior: Awake -> OnEnable -> sceneLoaded -> Start(next frame)
			pxSceneData->DispatchAwakeForNewScene();
			pxSceneData->DispatchEnableAndPendingStartsForNewScene();
			pxSceneData->m_bIsActivated = true;
			FireSceneLoadedCallbacks(xScene, pxJob->m_eMode);

			pxOp->SetProgress(1.0f);
			pxOp->SetComplete(true);
			pxOp->FireCompletionCallback();

			s_bIsLoadingScene = false;

			CleanupAndRemoveAsyncJob(i);
			continue;
		}

		++i;
	}
}

void Zenith_SceneManager::ProcessPendingAsyncUnloads()
{
	// Process pending async unloads - iterate in reverse for safe removal
	for (int i = static_cast<int>(s_axAsyncUnloadJobs.GetSize()) - 1; i >= 0; --i)
	{
		AsyncUnloadJob* pxJob = s_axAsyncUnloadJobs.Get(static_cast<u_int>(i));
		Zenith_SceneOperation* pxOp = pxJob->m_pxOperation;

		// Build scene handle for validation
		Zenith_Scene xScene;
		xScene.m_iHandle = pxJob->m_iSceneHandle;
		xScene.m_uGeneration = pxJob->m_uSceneGeneration;

		Zenith_SceneData* pxSceneData = GetSceneData(xScene);
		if (!pxSceneData)
		{
			// Scene already gone - complete the operation
			pxOp->SetProgress(1.0f);
			pxOp->SetComplete(true);
			pxOp->FireCompletionCallback();
			delete pxJob;
			s_axAsyncUnloadJobs.Remove(static_cast<u_int>(i));
			continue;
		}

		// Fire unloading callback once (before any destruction)
		if (!pxJob->m_bUnloadingCallbackFired)
		{
			FireSceneUnloadingCallbacks(xScene);
			pxJob->m_bUnloadingCallbackFired = true;

			// If we're unloading the active scene, select a new one
			if (xScene.m_iHandle == s_iActiveSceneHandle)
			{
				Zenith_Assert(!s_bRenderTasksActive, "Cannot change active scene while render tasks are in flight");
				s_iActiveSceneHandle = SelectNewActiveScene(xScene.m_iHandle);
				Zenith_Scene xNewActive = GetActiveScene();
				FireActiveSceneChangedCallbacks(xScene, xNewActive);
			}
		}

		// Destroy a batch of entities this frame
		const Zenith_Vector<Zenith_EntityID>& axEntities = pxSceneData->GetActiveEntities();
		uint32_t uEntitiesThisFrame = 0;

		// Destroy from the end to avoid index shifting issues
		// RemoveEntity dispatches OnDisable/OnDestroy internally for each entity
		// and recursively for children, ensuring no entity is missed
		while (axEntities.GetSize() > 0 && uEntitiesThisFrame < s_uAsyncUnloadBatchSize)
		{
			uint32_t uBefore = axEntities.GetSize();
			Zenith_EntityID xID = axEntities.Get(uBefore - 1);
			// Detach from parent before removal to keep parent's child list clean
			// during multi-frame async unload (prevents stale references between batches)
			if (pxSceneData->EntityExists(xID))
			{
				Zenith_Entity xEntity(pxSceneData, xID);
				xEntity.GetComponent<Zenith_TransformComponent>().DetachFromParent();
			}
			pxSceneData->RemoveEntity(xID);
			// RemoveEntity recursively removes all descendants, so count the actual
			// number removed to respect the batch limit and track progress correctly
			uint32_t uActualRemoved = uBefore - axEntities.GetSize();
			pxJob->m_uDestroyedEntities += uActualRemoved;
			uEntitiesThisFrame += uActualRemoved;
		}

		// Update progress using the initial entity count captured at job creation
		// This prevents progress from appearing to go backwards if entities spawn during OnDestroy
		// Use max() to handle edge case where more entities destroyed than originally counted
		uint32_t uEffectiveTotal = std::max(pxJob->m_uTotalEntities, pxJob->m_uDestroyedEntities);
		if (uEffectiveTotal > 0)
		{
			float fNewProgress = static_cast<float>(pxJob->m_uDestroyedEntities) / static_cast<float>(uEffectiveTotal);
			// Ensure monotonic progress - never decrease (handles edge cases where OnDestroy spawns entities)
			float fCurrentProgress = pxOp->GetProgress();
			float fClampedProgress = std::max(fCurrentProgress, fNewProgress * fPROGRESS_DESTRUCTION_WEIGHT);
			pxOp->SetProgress(fClampedProgress);
		}

		// Check if all entities destroyed
		if (axEntities.GetSize() == 0)
		{
			// Free scene data
			delete pxSceneData;
			s_axScenes.Get(pxJob->m_iSceneHandle) = nullptr;

			// Fire unloaded callback BEFORE incrementing generation so the handle
			// is still valid for identification in callbacks (Unity parity)
			FireSceneUnloadedCallbacks(xScene);

			FreeSceneHandle(pxJob->m_iSceneHandle);

			// Complete operation
			pxOp->SetProgress(1.0f);
			pxOp->SetComplete(true);
			pxOp->FireCompletionCallback();

			delete pxJob;
			s_axAsyncUnloadJobs.Remove(static_cast<u_int>(i));
		}
	}
}

void Zenith_SceneManager::SetFixedTimestep(float fTimestep)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "SetFixedTimestep must be called from main thread");
	Zenith_Assert(fTimestep > 0.0f, "Fixed timestep must be positive");
	s_fFixedTimestep = fTimestep;
}

float Zenith_SceneManager::GetFixedTimestep()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetFixedTimestep must be called from main thread");
	return s_fFixedTimestep;
}

void Zenith_SceneManager::SetAsyncUnloadBatchSize(uint32_t uEntitiesPerFrame)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "SetAsyncUnloadBatchSize must be called from main thread");

	// Validate batch size to prevent infinite loops (0) and excessive frame hitches (very large)
	constexpr uint32_t uMIN_BATCH = 1;
	constexpr uint32_t uMAX_BATCH = 10000;

	if (uEntitiesPerFrame < uMIN_BATCH)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE,
			"SetAsyncUnloadBatchSize: Clamping value %u to minimum %u (0 would cause infinite loops)",
			uEntitiesPerFrame, uMIN_BATCH);
		uEntitiesPerFrame = uMIN_BATCH;
	}
	else if (uEntitiesPerFrame > uMAX_BATCH)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE,
			"SetAsyncUnloadBatchSize: Clamping value %u to maximum %u (large values defeat async unload purpose)",
			uEntitiesPerFrame, uMAX_BATCH);
		uEntitiesPerFrame = uMAX_BATCH;
	}

	s_uAsyncUnloadBatchSize = uEntitiesPerFrame;
}

uint32_t Zenith_SceneManager::GetAsyncUnloadBatchSize()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetAsyncUnloadBatchSize must be called from main thread");
	return s_uAsyncUnloadBatchSize;
}

void Zenith_SceneManager::SetMaxConcurrentAsyncLoads(uint32_t uMax)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "SetMaxConcurrentAsyncLoads must be called from main thread");
	s_uMaxConcurrentAsyncLoads = (uMax > 0) ? uMax : 1;
}

uint32_t Zenith_SceneManager::GetMaxConcurrentAsyncLoads()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetMaxConcurrentAsyncLoads must be called from main thread");
	return s_uMaxConcurrentAsyncLoads;
}

void Zenith_SceneManager::Update(float fDt)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "Update must be called from main thread");

	// Process pending async loads with activation control
	ProcessPendingAsyncLoads();

	// Process pending async unloads (batch destruction)
	ProcessPendingAsyncUnloads();

	// Clean up completed operations that are no longer being polled
	CleanupCompletedOperations();

	// Mark as updating - any LoadScene/LoadSceneByIndex calls during script execution
	// will route through LoadSceneAsync to defer to next frame (Unity parity)
	s_bIsUpdating = true;

	// Fixed timestep accumulation for FixedUpdate (50Hz by default, configurable via SetFixedTimestep)
	// #TODO: Use scaled time when timeScale system is implemented (Unity's Time.fixedDeltaTime is affected by Time.timeScale)
	// Clamp delta time to prevent runaway FixedUpdate loops after long freezes or breakpoints
	// (Unity parity: Time.maximumDeltaTime defaults to 0.3333s)
	static constexpr float fMAX_FIXED_DT = 0.333f;
	s_fFixedTimeAccumulator += std::min(fDt, fMAX_FIXED_DT);
	while (s_fFixedTimeAccumulator >= s_fFixedTimestep)
	{
		for (u_int i = 0; i < s_axScenes.GetSize(); ++i)
		{
			Zenith_SceneData* pxData = s_axScenes.Get(i);
			if (IsSceneUpdatable(pxData))
			{
				pxData->FixedUpdate(s_fFixedTimestep);
			}
		}
		s_fFixedTimeAccumulator -= s_fFixedTimestep;
	}

	// Unity execution order: FixedUpdate -> Start -> Update -> LateUpdate
	// Dispatch pending Start() calls after FixedUpdate but before Update
	for (u_int i = 0; i < s_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = s_axScenes.Get(i);
		if (IsSceneUpdatable(pxData))
		{
			pxData->DispatchPendingStarts();
		}
	}

	// Update all loaded scenes (skip paused and non-activated scenes)
	for (u_int i = 0; i < s_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = s_axScenes.Get(i);
		if (IsSceneUpdatable(pxData))
		{
			pxData->Update(fDt);
		}
	}

	// Animation Update (parallel task system)
	// Note: This runs after script updates to avoid vector resize issues
	// when scripts add new model components
	g_xAnimationsToUpdate.Clear();

	// Collect animations from all loaded, non-paused scenes.
	// IMPORTANT: pxModel->Update(fDt) must NOT create/destroy entities or add/remove
	// ModelComponents. GetAllOfComponentType returns raw pointers into component pools;
	// pool reallocation from entity/component mutations would invalidate these pointers.
	for (u_int i = 0; i < s_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = s_axScenes.Get(i);
		if (pxData && pxData->m_bIsLoaded && !pxData->m_bIsUnloading && !pxData->IsPaused())
		{
			Zenith_Vector<Zenith_ModelComponent*> xModels;
			pxData->GetAllOfComponentType<Zenith_ModelComponent>(xModels);

			for (Zenith_Vector<Zenith_ModelComponent*>::Iterator xIt(xModels); !xIt.Done(); xIt.Next())
			{
				Zenith_ModelComponent* pxModel = xIt.GetData();

				// Skip inactive entities to avoid unnecessary animation updates
				// IsActiveInHierarchy checks both the entity's own flag and all parents,
				// ensuring animations stop when a parent is disabled (Unity parity)
				Zenith_Entity xEntity = pxModel->GetParentEntity();
				if (!xEntity.IsActiveInHierarchy())
				{
					continue;
				}

				// New model instance system: Update the animation controller and skeleton
				if (pxModel->IsUsingModelInstance())
				{
					pxModel->Update(fDt);
				}

				// Legacy system: Collect animations for parallel update task
				for (u_int uMesh = 0; uMesh < pxModel->GetNumMeshEntries(); uMesh++)
				{
					if (Flux_MeshAnimation* pxAnim = pxModel->GetMeshGeometryAtIndex(uMesh).m_pxAnimation)
					{
						g_xAnimationsToUpdate.PushBack(pxAnim);
					}
				}
			}
		}
	}

	s_bIsUpdating = false;

	if (g_pxAnimUpdateTask)
	{
#ifdef ZENITH_ASSERT
		s_bAnimTasksActive = true;
#endif
		Zenith_TaskSystem::SubmitTaskArray(g_pxAnimUpdateTask);
	}
}

void Zenith_SceneManager::WaitForUpdateComplete()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "WaitForUpdateComplete must be called from main thread");

	// Wait for animation tasks to complete
	if (g_pxAnimUpdateTask)
	{
		g_pxAnimUpdateTask->WaitUntilComplete();
#ifdef ZENITH_ASSERT
		s_bAnimTasksActive = false;
#endif
	}
}

Zenith_SceneData* Zenith_SceneManager::GetSceneData(Zenith_Scene xScene)
{
	if (xScene.m_iHandle < 0 || xScene.m_iHandle >= static_cast<int>(s_axScenes.GetSize()))
	{
		return nullptr;
	}
	// Validate generation counter to detect stale handles
	if (xScene.m_uGeneration != s_axSceneGenerations.Get(xScene.m_iHandle))
	{
		return nullptr;
	}
	return s_axScenes.Get(xScene.m_iHandle);
}

// Safety invariant: This function does NOT validate scene generation counters.
// It is safe because the only external call path is Entity::GetSceneData(),
// which validates entity generation (slot occupied + generation match) before
// reaching here. If exposing this to new callers, add a generation parameter
// or make callers validate the handle first.
Zenith_SceneData* Zenith_SceneManager::GetSceneDataByHandle(int iHandle)
{
	if (iHandle < 0 || iHandle >= static_cast<int>(s_axScenes.GetSize()))
	{
		return nullptr;
	}
	return s_axScenes.Get(iHandle);
}

Zenith_SceneData* Zenith_SceneManager::GetSceneDataForEntity(Zenith_EntityID xID)
{
	if (!xID.IsValid()) return nullptr;
	if (xID.m_uIndex >= Zenith_SceneData::s_axEntitySlots.GetSize()) return nullptr;
	const Zenith_SceneData::Zenith_EntitySlot& xSlot = Zenith_SceneData::s_axEntitySlots.Get(xID.m_uIndex);
	if (!xSlot.IsOccupied() || xSlot.m_uGeneration != xID.m_uGeneration) return nullptr;
	return GetSceneDataByHandle(xSlot.m_iSceneHandle);
}

Zenith_Scene Zenith_SceneManager::GetSceneFromHandle(int iHandle)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetSceneFromHandle must be called from main thread");
	Zenith_Scene xScene;
	xScene.m_iHandle = iHandle;
	xScene.m_uGeneration = 0;

	if (iHandle >= 0 && iHandle < static_cast<int>(s_axSceneGenerations.GetSize()))
	{
		xScene.m_uGeneration = s_axSceneGenerations.Get(iHandle);
	}
	return xScene;
}

bool Zenith_SceneManager::IsLoadingScene()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "IsLoadingScene must be called from main thread");
	return s_bIsLoadingScene;
}

void Zenith_SceneManager::SetPrefabInstantiating(bool b)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "SetPrefabInstantiating must be called from main thread");
	s_bIsPrefabInstantiating = b;
}

void Zenith_SceneManager::SetLoadingScene(bool b)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "SetLoadingScene must be called from main thread");
	s_bIsLoadingScene = b;
}

void Zenith_SceneManager::SetInitialSceneLoadCallback(InitialSceneLoadFn pfnCallback)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "SetInitialSceneLoadCallback must be called from main thread");
	s_pfnInitialSceneLoad = pfnCallback;
}

Zenith_SceneManager::InitialSceneLoadFn Zenith_SceneManager::GetInitialSceneLoadCallback()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetInitialSceneLoadCallback must be called from main thread");
	return s_pfnInitialSceneLoad;
}

void Zenith_SceneManager::DispatchFullLifecycleInit()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "DispatchFullLifecycleInit must be called from main thread");
	// Dispatch lifecycle for all loaded scenes
	for (u_int i = 0; i < s_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = s_axScenes.Get(i);
		if (pxData && pxData->m_bIsLoaded)
		{
			pxData->DispatchLifecycleForNewScene();
		}
	}
}

int Zenith_SceneManager::AllocateSceneHandle()
{
	// Try to reuse a free handle
	// Note: FreeSceneHandle already incremented the generation, so handles in
	// the free list are ready to use without further increment
	if (s_axFreeHandles.GetSize() > 0)
	{
		int iHandle = s_axFreeHandles.GetBack();
		s_axFreeHandles.PopBack();
		return iHandle;
	}
	// New handle - add initial generation of 1
	int iNewHandle = static_cast<int>(s_axScenes.GetSize());
	s_axSceneGenerations.PushBack(1);
	return iNewHandle;
}

// IMPORTANT: FreeSceneHandle increments the generation counter, which invalidates
// all existing Zenith_Scene handles for this slot. Therefore, this function must
// be called AFTER FireSceneUnloadedCallbacks() so that callbacks can still identify
// the scene via its handle. See UnloadSceneInternal() and ProcessPendingAsyncUnloads().
void Zenith_SceneManager::FreeSceneHandle(int iHandle)
{
	if (iHandle < 0 || iHandle >= static_cast<int>(s_axSceneGenerations.GetSize()))
	{
		return;
	}

	RemoveFromSceneNameCache(iHandle);

	// Increment generation to immediately invalidate any existing handles
	// This ensures old Zenith_Scene references become invalid right away
	uint32_t& uGen = s_axSceneGenerations.Get(iHandle);
	if (uGen < UINT32_MAX - 1)
	{
		uGen++;
		s_axFreeHandles.PushBack(iHandle);  // Only recycle if not saturated
	}
	else
	{
		// Slot is permanently retired - don't add to free list
		// This prevents generation wrap-around bugs
		Zenith_Warning(LOG_CATEGORY_SCENE, "Scene handle %d retired due to generation overflow", iHandle);
	}
}

void Zenith_SceneManager::PushLifecycleContext(const std::string& strCanonicalPath)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "PushLifecycleContext must be called from main thread");
	s_axLifecycleLoadStack.PushBack(strCanonicalPath);
}

void Zenith_SceneManager::PopLifecycleContext(const std::string& strCanonicalPath)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "PopLifecycleContext must be called from main thread");
	s_axLifecycleLoadStack.EraseValue(strCanonicalPath);
}

void Zenith_SceneManager::ProcessPendingUnloads()
{
	// Synchronous unloading - no pending operations to process
	// Reserved for future async unload implementation if needed
}

void Zenith_SceneManager::UnloadAllNonPersistent()
{
	// Cancel pending async unload jobs
	for (u_int i = 0; i < s_axAsyncUnloadJobs.GetSize(); ++i)
	{
		AsyncUnloadJob* pxJob = s_axAsyncUnloadJobs.Get(i);
		Zenith_SceneOperation* pxOp = pxJob->m_pxOperation;
		pxOp->SetFailed(true);
		pxOp->SetProgress(1.0f);
		pxOp->SetComplete(true);
		pxOp->FireCompletionCallback();
		delete pxJob;
	}
	s_axAsyncUnloadJobs.Clear();

	// Track if we're unloading the active scene
	bool bActiveSceneUnloaded = false;
	// Capture active scene BEFORE destruction (FreeSceneHandle increments generation)
	Zenith_Scene xOldActive = GetActiveScene();

	// Collect scenes to unload for two-phase callback/destruction
	Zenith_Vector<Zenith_Scene> axScenesToUnload;
	for (u_int i = 0; i < s_axScenes.GetSize(); ++i)
	{
		if (static_cast<int>(i) != s_iPersistentSceneHandle)
		{
			Zenith_SceneData* pxData = s_axScenes.Get(i);
			if (pxData && pxData->m_bIsLoaded)
			{
				Zenith_Scene xScene;
				xScene.m_iHandle = static_cast<int>(i);
				xScene.m_uGeneration = s_axSceneGenerations.Get(i);
				axScenesToUnload.PushBack(xScene);
			}
		}
	}

	// Phase 1: Fire SceneUnloading callbacks BEFORE destruction
	// This allows cleanup code to access scene data before it's destroyed
	for (u_int i = 0; i < axScenesToUnload.GetSize(); ++i)
	{
		FireSceneUnloadingCallbacks(axScenesToUnload.Get(i));
	}

	// Phase 2: Destroy scenes and fire SceneUnloaded callbacks AFTER destruction
	for (u_int i = 0; i < axScenesToUnload.GetSize(); ++i)
	{
		Zenith_Scene xScene = axScenesToUnload.Get(i);

		if (xScene.m_iHandle == s_iActiveSceneHandle)
		{
			bActiveSceneUnloaded = true;
		}

		delete s_axScenes.Get(xScene.m_iHandle);
		s_axScenes.Get(xScene.m_iHandle) = nullptr;

		// Fire unloaded callback BEFORE incrementing generation so the handle
		// is still valid for identification in callbacks (Unity parity)
		FireSceneUnloadedCallbacks(xScene);

		FreeSceneHandle(xScene.m_iHandle);
	}

	// If active scene was unloaded, fall back to persistent scene
	if (bActiveSceneUnloaded)
	{
		Zenith_Assert(!s_bRenderTasksActive, "Cannot change active scene while render tasks are in flight");
		s_iActiveSceneHandle = s_iPersistentSceneHandle;
		Zenith_Scene xNewActive = GetActiveScene();
		if (xOldActive != xNewActive)
		{
			FireActiveSceneChangedCallbacks(xOldActive, xNewActive);
		}
	}
}

uint32_t Zenith_SceneManager::CountScenesBeingAsyncUnloaded()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "CountScenesBeingAsyncUnloaded must be called from main thread");
	// O(1) - vector only contains active jobs (removed on completion)
	return static_cast<uint32_t>(s_axAsyncUnloadJobs.GetSize());
}

void Zenith_SceneManager::ResetAllRenderSystems()
{
	// Reset all Flux render systems - called during SINGLE mode scene loads
	// to clean up all render state before loading a new scene.
	// Note: This must NOT be called during ADDITIVE loads, as it would
	// destroy render data from other loaded scenes.
	//
	// IMPORTANT: Physics::Reset() is NOT included here because it must be
	// called AFTER scene entity destruction (collider destructors need
	// the physics world to still be valid). Physics reset is called
	// separately after UnloadAllNonPersistent().
	Flux_Terrain::Reset();
	Flux_StaticMeshes::Reset();
	Flux_AnimatedMeshes::Reset();
	Flux_Shadows::Reset();
	Flux_Primitives::Reset();
	Flux_Text::Reset();
	Flux_Particles::Reset();
	Flux_Skybox::Reset();
	Flux_HiZ::Reset();
	Flux_SSR::Reset();
	Flux_DeferredShading::Reset();
	Flux_SSAO::Reset();
	Flux_Fog::Reset();
	Flux_SDFs::Reset();
	Flux_Quads::Reset();
#ifdef ZENITH_TOOLS
	Flux_Gizmos::Reset();
#endif
}

