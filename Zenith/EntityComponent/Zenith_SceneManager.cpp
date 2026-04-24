#include "Zenith.h"

#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneManager_Internal.h"
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
#include "Flux/Text/Flux_Text.h"
#include "Flux/Particles/Flux_Particles.h"
#include "Flux/Skybox/Flux_Skybox.h"
#include "Flux/Fog/Flux_Fog.h"
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
// Callback-list statics (s_x*Callbacks, s_ulNextCallbackHandle,
// s_axCallbacksPendingRemoval, s_uFiringCallbacksDepth) are defined in
// Zenith_SceneManager_Callbacks.cpp alongside the template methods that
// use them.
Zenith_Vector<std::string> Zenith_SceneManager::s_axBuildIndexToPath;
Zenith_Vector<std::string> Zenith_SceneManager::s_axCurrentlyLoadingPaths;
Zenith_Vector<std::string> Zenith_SceneManager::s_axLifecycleLoadStack;
Zenith_Vector<Zenith_SceneManager::OperationMapEntry> Zenith_SceneManager::s_axOperationMap;
uint64_t Zenith_SceneManager::s_ulNextOperationID = 1;  // Start at 1 so 0 is invalid (0 == ZENITH_INVALID_OPERATION_ID)
Zenith_Vector<Zenith_SceneManager::AsyncLoadJob*> Zenith_SceneManager::s_axAsyncJobs;
Zenith_Vector<Zenith_SceneManager::AsyncUnloadJob*> Zenith_SceneManager::s_axAsyncUnloadJobs;
bool Zenith_SceneManager::s_bAsyncJobsNeedSort = false;
bool Zenith_SceneManager::s_bIsUpdating = false;
// Audit §3.8: tracks the op-id created by the most recent HandleDeferredLoad
// auto-promotion so sync-LoadScene-during-Update callers can recover it.
// Main-thread-only by contract (HandleDeferredLoad asserts IsMainThread).
Zenith_SceneOperationID Zenith_SceneManager::s_ulLastDeferredLoadOp = ZENITH_INVALID_OPERATION_ID;

// Pending build index plumb lives in Zenith_SceneManagerDetail (see
// Zenith_SceneManager_Internal.h) so the async-load TU can consume it too —
// LoadSceneAsyncByIndex + SCENE_LOAD_ADDITIVE_WITHOUT_LOADING previously dropped
// the build index because the async TU couldn't see a file-static. The RAII guard
// stays here because it's only referenced from this TU.
namespace
{
	// RAII guard that restores s_iPendingBuildIndex on scope exit. Protects against
	// leaking the pending value if LoadScene aborts mid-call (e.g. the render-task-active
	// or recursion-depth asserts trip in a debug test run). LoadScene's own reset-on-consume
	// is preserved and redundant with this guard — the guard simply restores to whatever
	// prior value was live when the scope was entered.
	struct PendingBuildIndexGuard
	{
		int m_iPrev;
		explicit PendingBuildIndexGuard(int iValue) : m_iPrev(Zenith_SceneManagerDetail::s_iPendingBuildIndex)
		{
			Zenith_SceneManagerDetail::s_iPendingBuildIndex = iValue;
		}
		~PendingBuildIndexGuard() { Zenith_SceneManagerDetail::s_iPendingBuildIndex = m_iPrev; }

		PendingBuildIndexGuard(const PendingBuildIndexGuard&) = delete;
		PendingBuildIndexGuard& operator=(const PendingBuildIndexGuard&) = delete;
	};
}

// A5: Suppress transient ActiveSceneChanged dispatches during a SINGLE load's teardown
// so subscribers see exactly one old→new transition, not two (old→fallback, then
// fallback→new). While suppression is active, the first observed oldActive is remembered
// and used once when LoadScene fires the final callback manually.
// Definitions for symbols declared in Zenith_SceneManager_Internal.h. Namespace
// qualification gives them external linkage so Zenith_SceneManager_AsyncLoad.cpp can
// see them. The `using namespace` below lets existing code in this file keep the
// unqualified names.
namespace Zenith_SceneManagerDetail
{
	bool s_bSuppressActiveSceneChanged = false;
	bool s_bHaveDeferredOldActive = false;
	Zenith_Scene s_xDeferredOldActive;
	int s_iPendingBuildIndex = -1;
}
using namespace Zenith_SceneManagerDetail;

// Helper: check if a Zenith_Vector<std::string> contains a given string
static bool VectorContainsString(const Zenith_Vector<std::string>& axVec, const std::string& str)
{
	for (u_int i = 0; i < axVec.GetSize(); ++i)
	{
		if (axVec.Get(i) == str) return true;
	}
	return false;
}

Zenith_Scene Zenith_SceneManager::MakeInvalidScene()
{
	Zenith_Scene xScene;
	xScene.m_iHandle = -1;
	xScene.m_uGeneration = 0;
	return xScene;
}

bool Zenith_SceneManager::CheckCircularLoadDependency(const std::string& strCanonicalPath)
{
	return VectorContainsString(s_axCurrentlyLoadingPaths, strCanonicalPath) ||
		VectorContainsString(s_axLifecycleLoadStack, strCanonicalPath);
}

void Zenith_SceneManager::FireUnloadCallbacksAndSelectNewActive(int iHandle, Zenith_Scene xScene)
{
	// Track if we're unloading the active scene BEFORE callbacks
	// (a callback could call SetActiveScene, so capture this first)
	bool bWasActiveScene = (iHandle == s_iActiveSceneHandle);

	// Fire unloading callback BEFORE destruction (allows access to scene data)
	FireSceneUnloadingCallbacks(xScene);

	// Free the scene data
	if (iHandle >= 0 && iHandle < static_cast<int>(s_axScenes.GetSize()))
	{
		delete s_axScenes.Get(iHandle);
		s_axScenes.Get(iHandle) = nullptr;

		// Fire unloaded callback BEFORE incrementing generation so the handle
		// is still valid for identification in callbacks (Unity parity)
		FireSceneUnloadedCallbacks(xScene);

		FreeSceneHandle(iHandle);
	}

	// If active scene was unloaded, select a new active scene
	if (bWasActiveScene)
	{
		Zenith_Assert(!s_bRenderTasksActive, "Cannot change active scene while render tasks are in flight");
		s_iActiveSceneHandle = SelectNewActiveScene();
		Zenith_Scene xNewActive = GetActiveScene();

		// During a LoadScene(SINGLE) teardown we suppress the intermediate
		// "old → fallback" dispatch and remember the very first oldActive we saw.
		// LoadScene fires a single "original → new" callback once the new scene is
		// active. This mirrors Unity's single-activeSceneChanged-per-load guarantee.
		if (s_bSuppressActiveSceneChanged)
		{
			if (!s_bHaveDeferredOldActive)
			{
				s_xDeferredOldActive = xScene;
				s_bHaveDeferredOldActive = true;
			}
		}
		else
		{
			FireActiveSceneChangedCallbacks(xScene, xNewActive);
		}
	}
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

// Async load/unload progress milestones live in Zenith_SceneManager_Internal.h as
// `inline constexpr`s in Zenith_SceneManagerDetail, so both TUs (this one and
// Zenith_SceneManager_AsyncLoad.cpp) share a single source of truth. The
// `using namespace Zenith_SceneManagerDetail;` above makes them unqualified here.

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

// Helper to extract scene name from file path (e.g., "Levels/MyScene.zscen" -> "MyScene").
// In namespace Zenith_SceneManagerDetail so Zenith_SceneManager_AsyncLoad.cpp can call it;
// the using-directive above keeps existing call sites in this file unqualified.
namespace Zenith_SceneManagerDetail
{
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

	// A6: Create the persistent scene WITHOUT auto-activating. It's a container for
	// DontDestroyOnLoad entities, not a user-visible scene, and must never be "active"
	// in Unity terminology.
	Zenith_Scene xPersistent = CreateEmptyScene("DontDestroyOnLoad", /*bAllowSetActive=*/false);
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
	s_bIsUpdating = false;
	s_ulLastDeferredLoadOp = ZENITH_INVALID_OPERATION_ID;
	s_pfnInitialSceneLoad = nullptr;
	Zenith_SceneManagerDetail::s_bSuppressActiveSceneChanged = false;
	Zenith_SceneManagerDetail::s_bHaveDeferredOldActive = false;
	Zenith_SceneManagerDetail::s_xDeferredOldActive = Zenith_Scene{};
	Zenith_SceneManagerDetail::s_iPendingBuildIndex = -1;

	// Reset ID counters for deterministic behavior across Shutdown/Initialise cycles
	s_ulNextLoadTimestamp = 1;
	s_ulNextOperationID = 1;
	s_ulNextCallbackHandle = 1;

	// Reset global entity storage (shared across all scenes)
	Zenith_SceneData::ResetGlobalEntityStorage();
}

#ifdef ZENITH_TESTING
void Zenith_SceneManager::ResetForNextTest()
{
	// Clear transient flags that might have been left true by a crashed or
	// early-returning test.
	s_bIsLoadingScene   = false;
	s_bIsUpdating       = false;
	s_bAsyncJobsNeedSort = false;
	s_ulLastDeferredLoadOp = ZENITH_INVALID_OPERATION_ID;

	// Wait for any in-flight async jobs from the previous test before we
	// tear them down. Skipping this would leak worker threads.
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
	for (u_int i = 0; i < s_axAsyncUnloadJobs.GetSize(); ++i)
	{
		delete s_axAsyncUnloadJobs.Get(i);
	}
	s_axAsyncUnloadJobs.Clear();
	for (u_int i = 0; i < s_axActiveOperations.GetSize(); ++i)
	{
		delete s_axActiveOperations.Get(i);
	}
	s_axActiveOperations.Clear();
	s_axOperationMap.Clear();

	// Full Shutdown() + Initialise() cycle. This is the same operation
	// TestShutdownClearsAllStatics performs and it's the most reliable way
	// to guarantee a pristine baseline — every scene is deleted, every
	// cached handle is invalidated, the animation task is re-created, and
	// the persistent scene is re-spawned. Subsequent tests then call
	// Zenith_SceneData::ResetGlobalEntityStorage via Shutdown so no stale
	// entity slot can dangle into the next test.
	Shutdown();
	Initialise();

	// Many editor / AI / physics tests assume an active scene exists to
	// create entities in. Provide one up-front so individual tests don't
	// need to repeat the boilerplate. Tests that want their own scene
	// simply overwrite the active handle via CreateEmptyScene or LoadScene.
	Zenith_Scene xActive = CreateEmptyScene("TestHarnessDefaultScene");
	s_iActiveSceneHandle = xActive.m_iHandle;
}
#endif // ZENITH_TESTING

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

// Scene-count queries (GetLoadedSceneCount / GetTotalSceneCount /
// GetBuildSceneCount) live in Zenith_SceneManager_Queries.cpp.

//==========================================================================
// Scene Creation
//==========================================================================

Zenith_Scene Zenith_SceneManager::CreateEmptyScene(const std::string& strName, bool bAllowSetActive)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "CreateEmptyScene must be called from main thread");
	Zenith_Assert(!s_bRenderTasksActive, "CreateEmptyScene: scene mutation while render tasks are reading — render-task invariant violated");

	int iHandle = AllocateSceneHandle();

	Zenith_SceneData* pxSceneData = new Zenith_SceneData();
	pxSceneData->m_strName = strName;
	pxSceneData->m_iHandle = iHandle;
	pxSceneData->m_uGeneration = s_axSceneGenerations.Get(iHandle);
	pxSceneData->m_bIsLoaded = true;
	// B.9: mirror the sync/async load progression — scene is created as
	// "loaded-but-not-activated" so any probe attached before the final flip
	// (e.g. via SceneLoadStarted or the soon-to-fire ActiveSceneChanged) observes
	// the same transient state as the disk-load paths. The flip to true happens
	// below, after the slot is wired up and ready for entity creation, but BEFORE
	// the optional auto-activate and BEFORE returning to the caller. Callers of
	// LoadScene(ADDITIVE_WITHOUT_LOADING) + the sync/async load paths all append
	// their own lifecycle/callback work on top.
	pxSceneData->m_bIsActivated = false;
	pxSceneData->m_ulLoadTimestamp = s_ulNextLoadTimestamp++;

	// Ensure vector is large enough
	while (s_axScenes.GetSize() <= static_cast<u_int>(iHandle))
	{
		s_axScenes.PushBack(nullptr);
	}
	s_axScenes.Get(iHandle) = pxSceneData;
	AddToSceneNameCache(iHandle, strName);

	// B.9: flip to activated now that the slot is fully published — empty scenes have
	// no entity lifecycle to dispatch, so there's nothing to wait for. SceneManager's
	// disk-load paths keep m_bIsActivated=false across the Awake/OnEnable window and
	// flip it just before firing SceneLoaded; they reach in and reset this to false
	// immediately after CreateEmptyScene returns.
	pxSceneData->m_bIsActivated = true;

	// A6: Auto-activate only if caller opts in AND no active scene already exists.
	// Initialise() passes false when creating the persistent DontDestroyOnLoad scene
	// so it never becomes the fallback active — preventing the long-standing bug
	// where UnloadAllNonPersistent would leave the persistent scene as the active one.
	if (bAllowSetActive && s_iActiveSceneHandle < 0)
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
// GetActiveScene, GetSceneAt, GetSceneByBuildIndex, GetSceneByName all live
// in Zenith_SceneManager_Queries.cpp. GetSceneByPath stays here because it
// depends on the file-local CanonicalizeScenePath helper above.

Zenith_Scene Zenith_SceneManager::GetSceneByPath(const std::string& strPath)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetSceneByPath must be called from main thread");
	Zenith_Scene xScene = MakeInvalidScene();

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

// Build index registry (RegisterSceneBuildIndex / ClearBuildIndexRegistry /
// GetRegisteredScenePath / GetBuildIndexRegistrySize) lives in
// Zenith_SceneManager_Queries.cpp.

//==========================================================================
// Scene Loading (Synchronous)
//==========================================================================

bool Zenith_SceneManager::ValidateLoadRequest(const std::string& strPath)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "LoadScene must be called from main thread");

	if (strPath.empty())
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "LoadScene: Path is empty");
		return false;
	}
	return true;
}

// Unity parity: LoadScene called during script execution (Update/FixedUpdate/callbacks)
// is deferred to next frame's ProcessPendingAsyncLoads, matching Unity's
// EarlyUpdate.UpdatePreloading behavior. This prevents use-after-free when the
// calling entity's scene is destroyed by SCENE_LOAD_SINGLE.
//
// D.14 (finding 3.6): Return a "future" scene handle when auto-deferring so Unity-
// ported scripts that stash the result don't silently get INVALID_SCENE. For modes
// that complete synchronously inside LoadSceneAsync (SCENE_LOAD_ADDITIVE_WITHOUT_LOADING),
// we can retrieve the result scene immediately. For file-I/O modes, we return
// INVALID because the scene handle isn't known until Phase 1 runs next frame —
// document this in the warning so callers that need tracking switch to LoadSceneAsync.
bool Zenith_SceneManager::HandleDeferredLoad(const std::string& strPath, Zenith_SceneLoadMode eMode, Zenith_Scene& xOutScene)
{
	if (!s_bIsUpdating)
		return false;

	if (eMode == SCENE_LOAD_ADDITIVE_WITHOUT_LOADING)
	{
		Zenith_SceneOperationID ulOpID = LoadSceneAsync(strPath, eMode);
		// Audit §3.8: also surface this op-id via GetLastDeferredLoadOp for callers
		// who only have the sync API in scope.
		s_ulLastDeferredLoadOp = ulOpID;
		Zenith_SceneOperation* pxOp = GetOperation(ulOpID);
		xOutScene = (pxOp && pxOp->IsComplete() && !pxOp->HasFailed())
			? pxOp->GetResultScene()
			: Zenith_Scene::INVALID_SCENE;
		return true;
	}

	Zenith_Warning(LOG_CATEGORY_SCENE,
		"LoadScene('%s') called during Update — auto-deferring to LoadSceneAsync. "
		"Sync return is INVALID_SCENE because the scene slot is not allocated until "
		"the next frame's Phase 1 runs. Call GetLastDeferredLoadOp() to retrieve "
		"the op-id for tracking, or use LoadSceneAsync directly.", strPath.c_str());
	// Audit §3.8: capture the op-id that was previously silently discarded.
	s_ulLastDeferredLoadOp = LoadSceneAsync(strPath, eMode);
	xOutScene = Zenith_Scene::INVALID_SCENE;
	return true;
}

Zenith_SceneOperationID Zenith_SceneManager::GetLastDeferredLoadOp()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(),
		"GetLastDeferredLoadOp must be called from main thread");
	return s_ulLastDeferredLoadOp;
}

bool Zenith_SceneManager::ValidateFileAndDetectCircular(const std::string& strPath, const std::string& strCanonicalPath, Zenith_SceneLoadMode eMode)
{
	// Check if file exists before proceeding (use original path for file access)
	if (!Zenith_FileAccess::FileExists(strPath.c_str()))
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "LoadScene: File not found: %s", strPath.c_str());
		return false;
	}

	// Circular load detection - prevent a scene from loading itself during OnAwake/OnStart
	if (CheckCircularLoadDependency(strCanonicalPath))
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "Circular scene load detected: %s", strCanonicalPath.c_str());
		return false;
	}

	// C10 (N9): guard against accidental deep recursion that isn't circular —
	// e.g. a cascade of sceneLoaded handlers each triggering another load. The
	// circular-detection vectors grow one entry per in-flight load; an unbounded
	// cascade walks those vectors linearly and wastes cycles while obscuring
	// the root cause in logs. 32 levels is far beyond any realistic load chain.
	constexpr u_int uMAX_LOAD_DEPTH = 32;
	Zenith_Assert(s_axCurrentlyLoadingPaths.GetSize() + s_axLifecycleLoadStack.GetSize() < uMAX_LOAD_DEPTH,
		"LoadScene: scene-load recursion depth exceeded safe limit (%u). Check that "
		"sceneLoaded / OnAwake handlers are not cascading scene loads without bound.",
		uMAX_LOAD_DEPTH);

	// CRITICAL: validate the new scene's file header BEFORE destroying the current world.
	// SCENE_LOAD_SINGLE tears down all non-persistent scenes; if LoadFromFile later fails,
	// the engine is left scene-less and unrecoverable. Peeking the header here makes the
	// load rollback-safe: a corrupt or unsupported file is rejected before any teardown.
	if (eMode == SCENE_LOAD_SINGLE && !Zenith_SceneData::ValidateFileHeader(strPath))
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "LoadScene(SINGLE): Invalid or unsupported scene file, aborting before teardown: '%s'", strPath.c_str());
		return false;
	}

	return true;
}

void Zenith_SceneManager::PerformSingleModeTeardownAndSwap(Zenith_Scene xScene, Zenith_Scene xOldActiveBeforeTeardown)
{
	// A5: suppress intermediate ActiveSceneChanged dispatches during teardown so
	// subscribers observe a single old → new transition at the end.
	s_bSuppressActiveSceneChanged = true;
	s_bHaveDeferredOldActive = false;
	s_xDeferredOldActive = Zenith_Scene::INVALID_SCENE;

	// Teardown order is critical:
	// 1. ResetAllRenderSystems() - clears Flux render state from old scenes
	// 2. UnloadAllNonPersistent(excluding staging) - destroys old scene entities
	//    (colliders remove themselves from the physics world in their dtors)
	// 3. Zenith_Physics::Reset() - catch-all: removes any residual bodies that
	//    weren't owned by a ColliderComponent. Safe here because the staging
	//    scene's colliders have NOT yet been promoted into the physics world
	//    (deserialization happened in the same tick, but colliders register on
	//    OnAwake which hasn't fired yet).
	//
	// NOTE: if a future change makes colliders register on AddComponent instead
	// of OnAwake, Physics::Reset will wipe the staging scene's bodies too —
	// revisit this ordering then.
	ResetAllRenderSystems();
	CancelAllPendingAsyncLoads();
	UnloadAllNonPersistent(xScene.m_iHandle);
	Zenith_Physics::Reset();
	s_fFixedTimeAccumulator = 0.0f;

	Zenith_Assert(!s_bRenderTasksActive, "Cannot change active scene while render tasks are in flight");
	s_iActiveSceneHandle = xScene.m_iHandle;

	// Stop suppressing and emit the single consolidated ActiveSceneChanged.
	s_bSuppressActiveSceneChanged = false;
	Zenith_Scene xOldActive = xOldActiveBeforeTeardown.IsValid()
		? xOldActiveBeforeTeardown
		: (s_bHaveDeferredOldActive ? s_xDeferredOldActive : Zenith_Scene::INVALID_SCENE);
	s_bHaveDeferredOldActive = false;
	s_xDeferredOldActive = Zenith_Scene::INVALID_SCENE;

	if (xOldActive != xScene)
	{
		FireActiveSceneChangedCallbacks(xOldActive, xScene);
	}
}

void Zenith_SceneManager::DispatchLifecycleAndFire(Zenith_Scene xScene, Zenith_SceneData* pxSceneData, const std::string& strCanonicalPath, Zenith_SceneLoadMode eMode)
{
	// Unity behavior: Awake -> OnEnable -> sceneLoaded -> Start(next frame)
	pxSceneData->DispatchAwakeForNewScene();
	pxSceneData->DispatchEnableAndPendingStartsForNewScene();

	// A10: Flip IsActivated after lifecycle completes but BEFORE SceneLoaded callbacks.
	// Subscribers of SceneLoaded see a fully-activated scene; Awake/OnEnable handlers
	// see IsActivated()==false (matches the async path at lines ~2009/~2078).
	pxSceneData->m_bIsActivated = true;

	// D.13 (finding 3.12): clear s_bIsLoadingScene BEFORE FireSceneLoadedCallbacks so
	// subscribers see IsLoadingScene() == false during dispatch — the original B4 fix
	// reverted because clearing BOTH this flag AND s_axCurrentlyLoadingPaths before
	// the callback broke same-path re-entrant loads (the circular-load guard relies
	// on the path vector, not the bool). The key insight: the two pieces of state have
	// different contracts. s_bIsLoadingScene is "engine mid-load (for entity-lifecycle
	// deferral purposes)"; s_axCurrentlyLoadingPaths is "paths already in flight (for
	// circular-load detection)". They must be cleared at different times.
	//
	// Entities created from inside a SceneLoaded callback now correctly behave as
	// runtime-created entities (immediate Awake/OnEnable via
	// Zenith_SceneData::DispatchImmediateLifecycleForRuntime at the call site),
	// not as "part of the scene being loaded" (batched Awake during the load pass).
	s_bIsLoadingScene = false;

	FireSceneLoadedCallbacks(xScene, eMode);

	// Clear the path marker only AFTER callbacks so a handler that attempts to load
	// the same scene hits the circular-load guard.
	s_axCurrentlyLoadingPaths.EraseValue(strCanonicalPath);
}

Zenith_Scene Zenith_SceneManager::LoadScene(const std::string& strPath, Zenith_SceneLoadMode eMode)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "LoadScene must be called from main thread");
	Zenith_Assert(!s_bRenderTasksActive, "LoadScene: scene mutation while render tasks are reading — render-task invariant violated");

	if (!ValidateLoadRequest(strPath))
		return MakeInvalidScene();

	Zenith_Scene xDeferredResult;
	if (HandleDeferredLoad(strPath, eMode, xDeferredResult))
		return xDeferredResult;

	// Canonicalize path for consistent detection and storage
	std::string strCanonicalPath = CanonicalizeScenePath(strPath);

	// Handle ADDITIVE_WITHOUT_LOADING - create empty scene without file load
	// Unity behavior: Creates scene entry for later population (e.g., procedural content)
	//
	// NOTE: This mode intentionally bypasses:
	//   - FireSceneLoadStartedCallbacks() - no file is being loaded
	//   - s_bIsLoadingScene flag - no async loading in progress
	//   - Circular load detection - no file operations involved
	//   - File existence check - no file required
	//
	// Use this mode for procedurally generated scenes or runtime-created content.
	if (eMode == SCENE_LOAD_ADDITIVE_WITHOUT_LOADING)
	{
		std::string strName = ExtractSceneNameFromPath(strCanonicalPath);
		Zenith_Scene xScene = CreateEmptyScene(strName);
		Zenith_SceneData* pxSceneData = GetSceneData(xScene);
		pxSceneData->m_strPath = strCanonicalPath;

		// B.4: apply pending build index from LoadSceneByIndex before firing SceneLoaded
		// so subscribers observe the correct m_iBuildIndex. Previously this branch ignored
		// s_iPendingBuildIndex, dropping the build-index caller contract silently.
		if (s_iPendingBuildIndex >= 0)
		{
			pxSceneData->m_iBuildIndex = s_iPendingBuildIndex;
			s_iPendingBuildIndex = -1;
		}

		// F3 Unity parity: CreateScene fires sceneLoaded with LoadSceneMode.Additive.
		// Game code that registers for sceneLoaded (e.g. editor panels, asset scanners,
		// scene-registry systems) expects to be notified uniformly regardless of
		// whether the scene was loaded from disk or created empty.
		FireSceneLoadedCallbacks(xScene, SCENE_LOAD_ADDITIVE);
		return xScene;
	}

	if (!ValidateFileAndDetectCircular(strPath, strCanonicalPath, eMode))
		return MakeInvalidScene();

	s_axCurrentlyLoadingPaths.PushBack(strCanonicalPath);

	// Fire scene load started callbacks before any loading work
	FireSceneLoadStartedCallbacks(strCanonicalPath);

	s_bIsLoadingScene = true;

	// D.12 (finding 3.1): atomic-swap sync path. Deserialize the new scene into a
	// STAGING scene alongside the old world; only if deserialization succeeds do we
	// tear down the old world and swap. This retires the long-standing "engine is
	// scene-less if the body is corrupt after a valid header" hazard — a failed
	// load leaves the original scene intact and untouched.
	//
	// Capture pre-teardown active scene in case we end up firing the consolidated
	// ActiveSceneChanged after the swap (SINGLE only).
	Zenith_Scene xOldActiveBeforeTeardown = GetActiveScene();

	// Create new scene with name extracted from path (Unity: scene.name is just filename without path/extension)
	std::string strName = ExtractSceneNameFromPath(strCanonicalPath);
	Zenith_Scene xScene = CreateEmptyScene(strName);
	Zenith_SceneData* pxSceneData = GetSceneData(xScene);
	pxSceneData->m_strPath = strCanonicalPath;  // Canonicalized path for consistent lookups

	// A10: Mark the scene as not-yet-activated so Awake/OnEnable handlers observe
	// IsActivated() == false, matching the async path's behaviour at line ~2009.
	// Without this, sync and async loads disagree: async dispatches Awake with
	// m_bIsActivated=false, sync dispatched it with the header default of true.
	pxSceneData->m_bIsActivated = false;

	// Apply the build index BEFORE lifecycle/callbacks fire so handlers see the correct
	// value. LoadSceneByIndex deposits the index in s_iPendingBuildIndex (main thread only);
	// previously the assignment happened after LoadScene returned, meaning SceneLoaded
	// handlers observed m_iBuildIndex == -1 during the sync path. Async already sets the
	// index before firing callbacks (see ProcessPendingAsyncLoads line ~1893).
	if (s_iPendingBuildIndex >= 0)
	{
		pxSceneData->m_iBuildIndex = s_iPendingBuildIndex;
		s_iPendingBuildIndex = -1;
	}

	// Unity parity: Scenes loaded additively are marked as subscenes
	if (eMode == SCENE_LOAD_ADDITIVE)
	{
		pxSceneData->m_bWasLoadedAdditively = true;
	}

	// Load scene data from file (use original path for file access). Old world is
	// STILL LIVE at this point — both scenes coexist in s_axScenes for the duration
	// of the deserialization window.
	if (!pxSceneData->LoadFromFile(strPath))
	{
		// D.12: deserialization failed WITHOUT teardown having run. Old world is
		// intact. Just unload the staging scene and return INVALID — no callback
		// synthesis needed because the active scene never actually changed.
		Zenith_Error(LOG_CATEGORY_SCENE, "LoadScene: Failed to load '%s' — old scene preserved", strPath.c_str());
		UnloadSceneForced(xScene);
		s_bIsLoadingScene = false;
		s_axCurrentlyLoadingPaths.EraseValue(strCanonicalPath);
		return Zenith_Scene::INVALID_SCENE;
	}

	// Deserialization succeeded. SINGLE mode: NOW tear down the old world and swap.
	if (eMode == SCENE_LOAD_SINGLE)
	{
		PerformSingleModeTeardownAndSwap(xScene, xOldActiveBeforeTeardown);
	}

	DispatchLifecycleAndFire(xScene, pxSceneData, strCanonicalPath, eMode);
	return xScene;
}

Zenith_Scene Zenith_SceneManager::LoadSceneByIndex(int iBuildIndex, Zenith_SceneLoadMode eMode)
{
	// Unity parity: defer to async when called during script execution.
	// D.14 (finding 3.6): same contract as LoadScene's s_bIsUpdating branch — for
	// synchronously-completing modes return the actual scene handle; otherwise warn
	// and return INVALID with a pointer to the async API.
	if (s_bIsUpdating)
	{
		if (eMode == SCENE_LOAD_ADDITIVE_WITHOUT_LOADING)
		{
			Zenith_SceneOperationID ulOpID = LoadSceneAsyncByIndex(iBuildIndex, eMode);
			Zenith_SceneOperation* pxOp = GetOperation(ulOpID);
			if (pxOp && pxOp->IsComplete() && !pxOp->HasFailed())
			{
				return pxOp->GetResultScene();
			}
			return MakeInvalidScene();
		}

		Zenith_Warning(LOG_CATEGORY_SCENE,
			"LoadSceneByIndex(%d) called during Update — auto-deferring to "
			"LoadSceneAsyncByIndex. Sync return is INVALID_SCENE because the scene slot "
			"is not allocated until the next frame's Phase 1 runs. Use "
			"LoadSceneAsyncByIndex directly and retain the operation ID if you need to "
			"track this load.", iBuildIndex);
		LoadSceneAsyncByIndex(iBuildIndex, eMode);
		return MakeInvalidScene();
	}

	const u_int uBuildIndex = static_cast<u_int>(iBuildIndex);
	if (iBuildIndex >= 0 && uBuildIndex < s_axBuildIndexToPath.GetSize() && !s_axBuildIndexToPath.Get(uBuildIndex).empty())
	{
		// Stash the build index so LoadScene can assign it to m_iBuildIndex BEFORE firing
		// SceneLoaded callbacks. Prior to this (audit finding HIGH 8) the assignment lived
		// after LoadScene returned, so callbacks observed m_iBuildIndex == -1. The RAII
		// guard restores on scope exit so an early abort inside LoadScene cannot leak the
		// pending index into the next call.
		PendingBuildIndexGuard xGuard(iBuildIndex);
		return LoadScene(s_axBuildIndexToPath.Get(uBuildIndex), eMode);
	}

	Zenith_Warning(LOG_CATEGORY_SCENE, "LoadSceneByIndex: No scene registered for build index %d", iBuildIndex);
	return MakeInvalidScene();
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
		Zenith_SceneData* pxSceneData = GetSceneData(xScene);
		pxSceneData->m_strPath = strCanonicalPath;

		// B.4: apply pending build index from LoadSceneAsyncByIndex before firing
		// SceneLoaded. Previously this branch returned before the patch-up site in
		// LoadSceneAsyncByIndex ran, so LoadSceneAsyncByIndex(idx, ADDITIVE_WITHOUT_LOADING)
		// silently produced a scene with m_iBuildIndex == -1.
		if (s_iPendingBuildIndex >= 0)
		{
			pxSceneData->m_iBuildIndex = s_iPendingBuildIndex;
			s_iPendingBuildIndex = -1;
		}

		pxOp->SetResultScene(xScene.m_iHandle, xScene.m_uGeneration);
		pxOp->SetProgress(1.0f);
		pxOp->SetComplete(true);

		// F3 Unity parity: match the sync ADDITIVE_WITHOUT_LOADING branch so
		// callers get a sceneLoaded notification regardless of sync/async entry.
		FireSceneLoadedCallbacks(xScene, SCENE_LOAD_ADDITIVE);
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
		// B.5: single-source-of-truth failure path — shared helper keeps the state
		// transitions (SetResultSceneHandle/SetFailed/SetProgress/SetComplete/
		// FireCompletionCallback) in one spot for all synthetic failures.
		FailAsyncLoadOperation(pxOp);
		return ulOpID;
	}

	// Canonicalize path for consistent storage and lookups
	std::string strCanonicalPath = CanonicalizeScenePath(strPath);

	// Circular load detection - prevent loading a scene that's already being loaded
	// THREAD SAFETY NOTE: Both sets are only modified on the main thread:
	//   - insert() called here in LoadSceneAsync() (main thread, asserted at function start)
	//   - erase() called in ProcessPendingAsyncLoads() (main thread, called from Update)
	// The worker thread (AsyncSceneLoadTask) never accesses these sets, so no synchronization needed.
	if (CheckCircularLoadDependency(strCanonicalPath))
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "Circular async scene load detected: %s", strCanonicalPath.c_str());
		// B.5: route through the same helper as the file-not-found branch above.
		FailAsyncLoadOperation(pxOp);
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

		// Create failed operation — B.5: route through FailAsyncLoadOperation for
		// consistency with LoadSceneAsync's synthetic-failure paths.
		Zenith_SceneOperation* pxOp = new Zenith_SceneOperation();
		uint64_t ulOpID = AllocateOperationID();
		pxOp->m_ulOperationID = ulOpID;
		s_axActiveOperations.PushBack(pxOp);
		s_axOperationMap.PushBack({ ulOpID, pxOp });
		FailAsyncLoadOperation(pxOp);
		return ulOpID;
	}

	// B.4: plumb the build index through the same s_iPendingBuildIndex guard the sync
	// path uses. ADDITIVE_WITHOUT_LOADING completes inside LoadSceneAsync without pushing
	// to s_axAsyncJobs, so the old job-patch-up code (retained below as a belt-and-braces
	// safety net for the file-load async path, which uses AsyncLoadJob::m_iBuildIndex
	// directly during Phase 1 scene creation) silently dropped the build index for that
	// mode. The guard fires early so the branch inside LoadSceneAsync can consume it.
	Zenith_SceneOperationID ulOpID;
	{
		PendingBuildIndexGuard xGuard(iBuildIndex);
		ulOpID = LoadSceneAsync(s_axBuildIndexToPath.Get(uBuildIndex), eMode);
	}

	// For the file-load path, Phase 1 reads AsyncLoadJob::m_iBuildIndex directly. Set
	// it on the newly queued job so Phase 1 assigns the right build index to the scene
	// it creates (s_iPendingBuildIndex is already consumed by the time Phase 1 runs).
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

	// Cannot unload a scene that's already being async unloaded.
	// Audit §3.7 hardening: check both m_bIsUnloading AND the async-unload-jobs
	// queue. Today the flag is set in UnloadSceneAsync before the job is pushed,
	// so the flag alone is sufficient. The jobs-list walk is belt-and-braces —
	// it catches any future code path where the flag could lag behind the queue
	// (e.g. if unload scheduling is ever refactored to be two-phase), and fails
	// loudly with the same warning rather than allowing a double-teardown.
	Zenith_SceneData* pxSceneData = GetSceneData(xScene);
	if (pxSceneData && pxSceneData->m_bIsUnloading)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "Cannot unload scene that is already being async unloaded");
		return false;
	}
	for (u_int i = 0; i < s_axAsyncUnloadJobs.GetSize(); ++i)
	{
		AsyncUnloadJob* pxJob = s_axAsyncUnloadJobs.Get(i);
		if (pxJob != nullptr
			&& pxJob->m_iSceneHandle == xScene.m_iHandle
			&& pxJob->m_uSceneGeneration == xScene.m_uGeneration)
		{
			Zenith_Warning(LOG_CATEGORY_SCENE,
				"Cannot unload scene (handle=%d): an async unload job is already queued for it", xScene.m_iHandle);
			return false;
		}
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
		// B1: bumped from Warning to Error. Unity's UnloadSceneAsync returns a
		// failed AsyncOperation in this case — callers that subscribe to
		// sceneUnloaded expecting it to fire will stall waiting for a callback
		// that never arrives. Error-level logging makes the silent-no-op hazard
		// visible in QA and production, and UnloadSceneAsync already returns a
		// synthetic failed op (HasFailed()==true) for callers to detect.
		Zenith_Error(LOG_CATEGORY_SCENE,
			"Cannot unload the last loaded scene (handle=%d). Engine requires at least "
			"one non-persistent scene to remain active. To replace it, use "
			"LoadScene(SINGLE) instead of unloading first.",
			xScene.m_iHandle);
		return false;
	}

	return true;
}

void Zenith_SceneManager::UnloadSceneInternal(Zenith_Scene xScene)
{
	FireUnloadCallbacksAndSelectNewActive(xScene.m_iHandle, xScene);
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
	Zenith_Assert(!s_bRenderTasksActive, "UnloadScene: scene mutation while render tasks are reading — render-task invariant violated");

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
	// E.16 (finding 3.17): unload ops don't produce a result scene — the scene
	// they asked about is gone. Pin the op's result to INVALID_SCENE so callers
	// polling GetResultScene() get a handle whose IsValid()==false instead of a
	// stale pointer to a scene that no longer exists. Must happen before any
	// SetComplete() so callers observing IsComplete() see a consistent result.
	pxOp->SetResultSceneHandle(-1);

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

	// A6: the persistent scene is a container for DontDestroyOnLoad entities only;
	// it must never be the "active" scene (matches Unity's DontDestroyOnLoad semantics).
	// Fallbacks in SelectNewActiveScene and CreateEmptyScene also honour this.
	if (xScene.m_iHandle == s_iPersistentSceneHandle)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "SetActiveScene: Cannot set the persistent scene as active (DontDestroyOnLoad is a container, not a real scene)");
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
	// Audit §3.1 (Phase 9 minimal patch): the warning is no longer guarded by a
	// one-shot `ls_bWarnedOnce` flag. Every call prints the warning so QA notices
	// the Unity-parity gap on every SCENE_LOAD_SINGLE. Unity auto-invokes
	// Resources.UnloadUnusedAssets on LoadSceneMode.Single — this auto-invocation
	// is community-documented behaviour rather than explicit ScriptReference docs,
	// but the practical effect is well known. Until Flux asset managers support
	// refcounting, callers must manage their own texture/model/material unloads.
	// Ref: https://docs.unity3d.com/ScriptReference/Resources.UnloadUnusedAssets.html
	Zenith_Warning(LOG_CATEGORY_SCENE,
		"UnloadUnusedAssets: Not yet implemented - Flux asset managers need reference counting support. "
		"Assets will remain in memory after scene unloads. Callers must manage their own unloads.");
}

// ============================================================================
// Multi-Scene Rendering Helpers
// ============================================================================

Zenith_CameraComponent* Zenith_SceneManager::FindMainCameraAcrossScenes()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread() || s_bRenderTasksActive, "FindMainCameraAcrossScenes must be called from main thread or during render task execution");
	// Try active scene first (common case). Audit §3.18 note: this IS a
	// legitimate use of GetActiveScene because the active scene is the
	// documented source of rendering/camera/lighting context in Unity — see
	// the banner comment on GetActiveScene(). This is not the "EntityID-as-
	// filter" anti-pattern; the subsequent loop falls back to every other
	// loaded scene if the active scene has no main camera.
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

Zenith_SceneData* Zenith_SceneManager::GetLoadedSceneDataAtSlot(uint32_t uIndex)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread() || s_bRenderTasksActive, "GetLoadedSceneDataAtSlot must be called from main thread or during render task execution");
	if (uIndex >= s_axScenes.GetSize())
		return nullptr;
	Zenith_SceneData* pxData = s_axScenes.Get(uIndex);
	if (!pxData || !pxData->IsLoaded() || pxData->IsUnloading())
		return nullptr;
	return pxData;
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

	// A6: never fall back to the persistent scene — it's a DontDestroyOnLoad container,
	// not a user-visible scene. Returning -1 when no user scene is available lets
	// GetActiveScene() return INVALID and makes the "no active scene" state explicit.
	//
	// C4: surface the "no active scene" transition as a warning so game code and QA
	// can correlate a missing GetActiveScene() with a concrete log line. Persistent-only
	// engine state is a legitimate but rare condition (e.g. between SINGLE unload and
	// the next LoadScene) — loud enough to notice but not an error.
	if (iBestHandle < 0)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE,
			"SelectNewActiveScene: no eligible non-persistent scene — active scene is now invalid. "
			"GetActiveScene() will return INVALID_SCENE until a new scene is loaded.");
	}
	return iBestHandle;
}

// Internal helper that moves entity between scenes using zero-copy transfer.
// EntityID is preserved (globally unique) - no serialize/deserialize.
// No lifecycle events fire (Unity parity: MoveGameObjectToScene is seamless).
// Returns true on success, false on failure.
// Cross-scene entity operations (MoveEntityInternal, MoveEntityToScene,
// RenameScene, MergeScenes, MarkEntityPersistent, GetPersistentScene) live
// in Zenith_SceneManager_EntityOps.cpp.

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

// Callback subsystem (AllocateCallbackHandle, Zenith_CallbackList templates,
// Register/Unregister/Fire wrappers, IsCallbackPendingRemoval,
// ProcessPendingCallbackRemovals) is defined in Zenith_SceneManager_Callbacks.cpp.

//==========================================================================
// Internal
//==========================================================================

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

void Zenith_SceneManager::Update(float fDt)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "Update must be called from main thread");

	ProcessPendingAsyncLoads();
	ProcessPendingAsyncUnloads();
	CleanupCompletedOperations();

	// Mark as updating - any LoadScene/LoadSceneByIndex calls during script execution
	// will route through LoadSceneAsync to defer to next frame (Unity parity)
	s_bIsUpdating = true;

	// E.20 (finding 3.21): collect updatable scenes once per frame; iterate the
	// snapshot across the three phases below. A deferred LoadSceneAsync won't
	// mutate s_axScenes mid-Update (all s_bIsUpdating-guarded mutations are
	// routed to the next frame), so the snapshot stays accurate for the tick.
	Zenith_Vector<Zenith_SceneData*> axUpdatable;
	CollectUpdatableScenes(axUpdatable);

	// HIGH-1: Unity execution order is Awake -> OnEnable -> Start -> FixedUpdate
	// -> Update -> LateUpdate. Flush pending Starts BEFORE the FixedUpdate
	// accumulator — otherwise entities that land in PENDING_START via async
	// Phase 2 (or any path that defers Start into Update) receive
	// OnFixedUpdate(dt) on their very first pump without having received
	// OnStart() yet. Must stay inside the s_bIsUpdating = true gate so deferred
	// load/unload routing from user callbacks still works.
	for (u_int i = 0; i < axUpdatable.GetSize(); ++i)
	{
		axUpdatable.Get(i)->DispatchPendingStarts();
	}

	// Fixed timestep accumulation for FixedUpdate (50Hz by default, configurable via SetFixedTimestep)
	// #TODO: Use scaled time when timeScale system is implemented (Unity's Time.fixedDeltaTime is affected by Time.timeScale)
	// Clamp delta time to prevent runaway FixedUpdate loops after long freezes or breakpoints
	// (Unity parity: Time.maximumDeltaTime defaults to 0.3333s)
	static constexpr float fMAX_FIXED_DT = 0.333f;
	s_fFixedTimeAccumulator += std::min(fDt, fMAX_FIXED_DT);
	while (s_fFixedTimeAccumulator >= s_fFixedTimestep)
	{
		for (u_int i = 0; i < axUpdatable.GetSize(); ++i)
		{
			axUpdatable.Get(i)->FixedUpdate(s_fFixedTimestep);
		}
		s_fFixedTimeAccumulator -= s_fFixedTimestep;
	}

	for (u_int i = 0; i < axUpdatable.GetSize(); ++i)
	{
		axUpdatable.Get(i)->Update(fDt);
	}

	// Animation Update (parallel task system). Runs after script updates so
	// scripts that add model components don't invalidate the collected pointers.
	g_xAnimationsToUpdate.Clear();
	for (u_int i = 0; i < s_axScenes.GetSize(); ++i)
	{
		CollectAnimationsFromScene(s_axScenes.Get(i));
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

void Zenith_SceneManager::CollectUpdatableScenes(Zenith_Vector<Zenith_SceneData*>& axOut)
{
	for (u_int i = 0; i < s_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = s_axScenes.Get(i);
		if (IsSceneUpdatable(pxData))
		{
			axOut.PushBack(pxData);
		}
	}
}

void Zenith_SceneManager::CollectAnimationsFromScene(Zenith_SceneData* pxData)
{
	// Skeletal animation is handled by Zenith_AnimatorComponent::OnUpdate; the
	// legacy per-Flux_MeshGeometry animation collection was removed along with
	// Zenith_ModelComponent::m_xMeshEntries. Kept as a no-op for now so the
	// update-loop call site stays structurally familiar.
	(void)pxData;
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
	// Scene slot array mutates only on the main thread, so reads from worker
	// threads are only safe while the render-task window is live (all scene
	// changes complete before render tasks submit; the task-queue mutex provides
	// the happens-before relationship). Mirrors the guard on GetActiveScene and
	// GetSceneDataAtSlot.
	Zenith_Assert(Zenith_Multithreading::IsMainThread() || s_bRenderTasksActive,
		"GetSceneData must be called from the main thread or during render task execution");
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

	// D1: double-free guard. Callers must `delete pxSceneData` and null the slot
	// BEFORE calling FreeSceneHandle — otherwise the generation bump here releases
	// the handle while the live scene data is still reachable via s_axScenes,
	// corrupting subsequent loads into the same slot.
	Zenith_Assert(iHandle >= static_cast<int>(s_axScenes.GetSize()) ||
	              s_axScenes.Get(iHandle) == nullptr,
		"FreeSceneHandle(%d): scene data is still non-null — caller forgot to delete + null before releasing the handle",
		iHandle);

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

void Zenith_SceneManager::UnloadAllNonPersistent(int iExcludeHandle)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "UnloadAllNonPersistent must be called from main thread");
	Zenith_Assert(!s_bRenderTasksActive, "UnloadAllNonPersistent: scene mutation while render tasks are reading — render-task invariant violated");

	// HIGH-2: track scene handles whose SceneUnloading callback has already
	// fired via an in-flight async unload job. Without this, the Phase-1 walk
	// below would fire SceneUnloading a second time for the same scene — Unity
	// guarantees exactly-one Unloading per unload.
	Zenith_Vector<int> axAsyncUnloadingAlreadyFired;

	// Cancel pending async unload jobs.
	// C5: These jobs are being preempted by the synchronous UnloadAllNonPersistent
	// path which itself unloads the scene. The unload IS completing (just via a
	// different code path), so we mark the operation SUCCEEDED rather than FAILED.
	// Previously "cancelled async unloads" showed up as failures in the API,
	// which was misleading for callers polling for operation status.
	for (u_int i = 0; i < s_axAsyncUnloadJobs.GetSize(); ++i)
	{
		AsyncUnloadJob* pxJob = s_axAsyncUnloadJobs.Get(i);
		if (pxJob->m_bUnloadingCallbackFired)
		{
			axAsyncUnloadingAlreadyFired.PushBack(pxJob->m_iSceneHandle);
		}
		Zenith_SceneOperation* pxOp = pxJob->m_pxOperation;
		// SetFailed(false) is the explicit "completed without error" state.
		pxOp->SetFailed(false);
		// E.16 (finding 3.17): unload ops always report INVALID_SCENE as their result.
		pxOp->SetResultSceneHandle(-1);
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
		// D.12: skip the explicit excluded handle too (used by atomic-swap sync
		// LoadScene to keep the staging scene alive while old scenes are unloaded).
		if (static_cast<int>(i) != s_iPersistentSceneHandle && static_cast<int>(i) != iExcludeHandle)
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
	// This allows cleanup code to access scene data before it's destroyed.
	// HIGH-2: skip scenes that already fired SceneUnloading via an in-flight
	// async unload — firing it twice violates Unity's exactly-one guarantee.
	// SceneUnloaded is always fired below in Phase 2 (HIGH-3 — cancel path
	// must still pair Unloaded with the earlier Unloading).
	for (u_int i = 0; i < axScenesToUnload.GetSize(); ++i)
	{
		const Zenith_Scene& xScene = axScenesToUnload.Get(i);
		bool bSkipUnloading = false;
		for (u_int j = 0; j < axAsyncUnloadingAlreadyFired.GetSize(); ++j)
		{
			if (axAsyncUnloadingAlreadyFired.Get(j) == xScene.m_iHandle)
			{
				bSkipUnloading = true;
				break;
			}
		}
		if (!bSkipUnloading)
		{
			FireSceneUnloadingCallbacks(xScene);
		}
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

	// A6: If the active scene was unloaded, clear the active handle. Do NOT fall back
	// to the persistent scene — it's a DontDestroyOnLoad container, not a real scene.
	// The next LoadScene will set a new active scene, and during the gap GetActiveScene
	// returns INVALID (callers must handle it; they already do via IsValid() paths).
	if (bActiveSceneUnloaded)
	{
		Zenith_Assert(!s_bRenderTasksActive, "Cannot change active scene while render tasks are in flight");
		s_iActiveSceneHandle = -1;
		Zenith_Scene xNewActive = GetActiveScene();  // will be INVALID_SCENE
		if (xOldActive != xNewActive)
		{
			// A5: suppress during SINGLE-load teardown — LoadScene fires the
			// single consolidated old→new callback once the new scene is active.
			if (s_bSuppressActiveSceneChanged)
			{
				if (!s_bHaveDeferredOldActive)
				{
					s_xDeferredOldActive = xOldActive;
					s_bHaveDeferredOldActive = true;
				}
			}
			else
			{
				FireActiveSceneChangedCallbacks(xOldActive, xNewActive);
			}
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
	// Only subsystems with real per-scene state need a Reset hook. The empty
	// stubs that the render-graph refactor left behind have been deleted.
	Flux_Terrain::Reset();
	Flux_Text::Reset();
	Flux_Particles::Reset();
	Flux_Skybox::Reset();
	Flux_Fog::Reset();
#ifdef ZENITH_TOOLS
	Flux_Gizmos::Reset();
#endif
}

#ifdef ZENITH_TESTING
#include "EntityComponent/Zenith_SceneManager.Tests.inl"
#endif

