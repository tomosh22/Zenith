#include "Zenith.h"

#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_RenderTaskState.h"
#include "EntityComponent/Zenith_SceneManager_Internal.h"
#include "EntityComponent/Internal/Zenith_SceneCallbackBus.h"
#include "EntityComponent/Internal/Zenith_SceneRegistry.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_SceneOperation.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "Collections/Zenith_HashSet.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "DataStream/Zenith_DataStream.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
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

// Static member definitions.
// A1: callback-list statics moved to Zenith_SceneCallbackBus.
// A2b: scene-storage statics (Zenith_SceneRegistry::s_axScenes, Zenith_SceneRegistry::s_axSceneGenerations, s_axFreeHandles,
// Zenith_SceneRegistry::s_iActiveSceneHandle, Zenith_SceneRegistry::s_iPersistentSceneHandle, Zenith_SceneRegistry::s_ulNextLoadTimestamp,
// s_axLoadedSceneNames, Zenith_SceneRegistry::s_axBuildIndexToPath) moved to Zenith_SceneRegistry.
#ifdef ZENITH_ASSERT
bool Zenith_SceneManager::s_bRenderTasksActive = false;
bool Zenith_SceneManager::s_bAnimTasksActive = false;
#endif
#ifdef ZENITH_TESTING
// B3: Unity-parity instrumentation. Tests assert this counter to verify that
// SCENE_LOAD_SINGLE auto-fires UnloadUnusedAssets and ADDITIVE does not.
static uint32_t s_uUnloadUnusedAssetsCallCount = 0;
#endif
// A3: async-operation statics moved to Zenith_SceneOperationQueue.
// A4: lifecycle/update statics (Zenith_SceneLifecycleScheduler::s_bIsLoadingScene, Zenith_SceneLifecycleScheduler::s_bIsPrefabInstantiating,
// Zenith_SceneLifecycleScheduler::s_bIsUpdating, Zenith_SceneLifecycleScheduler::s_ulLastDeferredLoadOp, Zenith_SceneLifecycleScheduler::s_fFixedTimeAccumulator, Zenith_SceneLifecycleScheduler::s_fFixedTimestep,
// Zenith_SceneLifecycleScheduler::s_axCurrentlyLoadingPaths, Zenith_SceneLifecycleScheduler::s_axLifecycleLoadStack, Zenith_SceneLifecycleScheduler::s_pfnInitialSceneLoad)
// moved to Zenith_SceneLifecycleScheduler.

// PendingBuildIndexGuard: RAII helper that restores Zenith_SceneLifecycleScheduler::s_iPendingBuildIndex on scope
// exit. Protects against leaking the pending value if LoadScene aborts mid-call.
// Storage now lives on Zenith_SceneLifecycleScheduler (A4); the guard wraps the
// scheduler's static.
namespace
{
	struct PendingBuildIndexGuard
	{
		int m_iPrev;
		explicit PendingBuildIndexGuard(int iValue) : m_iPrev(Zenith_SceneLifecycleScheduler::s_iPendingBuildIndex)
		{
			Zenith_SceneLifecycleScheduler::s_iPendingBuildIndex = iValue;
		}
		~PendingBuildIndexGuard() { Zenith_SceneLifecycleScheduler::s_iPendingBuildIndex = m_iPrev; }

		PendingBuildIndexGuard(const PendingBuildIndexGuard&) = delete;
		PendingBuildIndexGuard& operator=(const PendingBuildIndexGuard&) = delete;
	};
}

// Pull in the detail symbols (ExtractSceneNameFromPath, progress milestones)
// so existing body code can use unqualified names.
using namespace Zenith_SceneManagerDetail;

Zenith_Scene Zenith_SceneManager::MakeInvalidScene()
{
	return Zenith_SceneRegistry::MakeInvalidScene();
}

// Zenith_SceneLifecycleScheduler::IsCircularLoadDependency body migrated to Zenith_SceneLifecycleScheduler::IsCircularLoadDependency.

void Zenith_SceneManager::FireUnloadCallbacksAndSelectNewActive(int iHandle, Zenith_Scene xScene)
{
	// Track if we're unloading the active scene BEFORE callbacks
	// (a callback could call SetActiveScene, so capture this first)
	bool bWasActiveScene = (iHandle == Zenith_SceneRegistry::s_iActiveSceneHandle);

	// Fire unloading callback BEFORE destruction (allows access to scene data)
	FireSceneUnloadingCallbacks(xScene);

	// Free the scene data
	if (iHandle >= 0 && iHandle < static_cast<int>(Zenith_SceneRegistry::s_axScenes.GetSize()))
	{
		delete Zenith_SceneRegistry::s_axScenes.Get(iHandle);
		Zenith_SceneRegistry::s_axScenes.Get(iHandle) = nullptr;

		// Fire unloaded callback BEFORE incrementing generation so the handle
		// is still valid for identification in callbacks (Unity parity)
		FireSceneUnloadedCallbacks(xScene);

		FreeSceneHandle(iHandle);
	}

	// If active scene was unloaded, select a new active scene
	if (bWasActiveScene)
	{
		Zenith_Assert(!s_bRenderTasksActive, "Cannot change active scene while render tasks are in flight");
		Zenith_SceneRegistry::s_iActiveSceneHandle = SelectNewActiveScene();
		Zenith_Scene xNewActive = GetActiveScene();

		// During a LoadScene(SINGLE) teardown we suppress the intermediate
		// "old → fallback" dispatch and remember the very first oldActive we saw.
		// LoadScene fires a single "original → new" callback once the new scene is
		// active. This mirrors Unity's single-activeSceneChanged-per-load guarantee.
		if (Zenith_SceneCallbackBus::IsActiveSceneSuppressed())
		{
			if (!Zenith_SceneCallbackBus::HasDeferredOldActive())
			{
				Zenith_SceneCallbackBus::SetDeferredOldActive(xScene);
			}
		}
		else
		{
			FireActiveSceneChangedCallbacks(xScene, xNewActive);
		}
	}
}

void Zenith_SceneManager::AddToSceneNameCache(int iHandle, const std::string& strName) { Zenith_SceneRegistry::AddToSceneNameCache(iHandle, strName); }
void Zenith_SceneManager::RemoveFromSceneNameCache(int iHandle) { Zenith_SceneRegistry::RemoveFromSceneNameCache(iHandle); }

// Async load/unload progress milestones live in Zenith_SceneManager_Internal.h as
// `inline constexpr`s in Zenith_SceneManagerDetail; both this TU and
// Zenith_SceneOperationQueue.cpp share that single source of truth. The
// `using namespace Zenith_SceneManagerDetail;` above makes them unqualified here.

// Animation update task system moved to Zenith_SceneLifecycleScheduler.cpp (A4).

// Helper to extract scene name from file path (e.g., "Levels/MyScene.zscen" -> "MyScene").
// In namespace Zenith_SceneManagerDetail so Zenith_SceneOperationQueue.cpp can call it;
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
	// Scheduler creates the animation task.
	Zenith_SceneLifecycleScheduler::Initialise();

	// A6: Create the persistent scene WITHOUT auto-activating. It's a container for
	// DontDestroyOnLoad entities, not a user-visible scene, and must never be "active"
	// in Unity terminology.
	Zenith_Scene xPersistent = CreateEmptyScene("DontDestroyOnLoad", /*bAllowSetActive=*/false);
	Zenith_SceneRegistry::s_iPersistentSceneHandle = xPersistent.m_iHandle;

	Zenith_Log(LOG_CATEGORY_SCENE, "SceneManager initialized with persistent scene (handle=%d)", Zenith_SceneRegistry::s_iPersistentSceneHandle);
}

void Zenith_SceneManager::Shutdown()
{
	// Tear down per-subsystem in reverse-startup order.
	Zenith_SceneLifecycleScheduler::Shutdown();   // waits for anim task
	Zenith_SceneOperationQueue::Shutdown();        // waits for in-flight async loads/unloads
	Zenith_SceneRegistry::Shutdown();              // deletes scene data + clears slot tables
	Zenith_SceneCallbackBus::Shutdown();           // clears callback lists + suppression flags

	// Reset global entity storage (shared across all scenes).
	Zenith_SceneData::ResetGlobalEntityStorage();
}

#ifdef ZENITH_TESTING
uint32_t Zenith_SceneManager::GetUnloadUnusedAssetsCallCount()
{
	return s_uUnloadUnusedAssetsCallCount;
}

void Zenith_SceneManager::ResetForNextTest()
{
	// Clear transient flags that might have been left true by a crashed or
	// early-returning test.
	Zenith_SceneLifecycleScheduler::s_bIsLoadingScene   = false;
	Zenith_SceneLifecycleScheduler::s_bIsUpdating       = false;
	Zenith_SceneLifecycleScheduler::s_ulLastDeferredLoadOp = ZENITH_INVALID_OPERATION_ID;
	s_uUnloadUnusedAssetsCallCount = 0;

	// Tear down the queue (waits for in-flight workers, deletes jobs and operations).
	Zenith_SceneOperationQueue::ResetForNextTest();

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
	Zenith_SceneRegistry::s_iActiveSceneHandle = xActive.m_iHandle;
}
#endif // ZENITH_TESTING

//==========================================================================
// Scene Count Queries
//==========================================================================

bool Zenith_SceneManager::IsSceneVisibleToUser(u_int uSlotIndex, const Zenith_SceneData* pxData)
{
	return Zenith_SceneRegistry::IsSceneVisibleToUser(uSlotIndex, pxData);
}

bool Zenith_SceneManager::IsSceneUpdatable(const Zenith_SceneData* pxData)
{
	return Zenith_SceneRegistry::IsSceneUpdatable(pxData);
}

// Scene-count queries (GetLoadedSceneCount / GetTotalSceneCount /
// GetBuildSceneCount) are forwarders defined near the bottom of this file.

//==========================================================================
// Scene Creation
//==========================================================================

Zenith_Scene Zenith_SceneManager::CreateScene(const std::string& strName)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "CreateScene must be called from main thread");

	if (strName.empty())
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "CreateScene: empty scene name rejected");
		return Zenith_Scene::INVALID_SCENE;
	}

	if (Zenith_SceneRegistry::GetSceneByName(strName).IsValid())
	{
		Zenith_Error(LOG_CATEGORY_SCENE,
			"CreateScene: a scene named '%s' is already loaded; duplicate names are not allowed",
			strName.c_str());
		return Zenith_Scene::INVALID_SCENE;
	}

	return CreateEmptyScene(strName, true);
}

Zenith_Entity Zenith_SceneManager::CreateEntity(const std::string& strName)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "CreateEntity must be called from main thread");

	const Zenith_Scene xTarget = GetDefaultCreationScene();
	if (!xTarget.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_SCENE,
			"CreateEntity: no creation target available (no active scene and no SceneCreationTargetScope); "
			"entity '%s' was not created",
			strName.c_str());
		return Zenith_Entity();
	}

	Zenith_SceneData* pxData = GetSceneData(xTarget);
	if (!pxData)
	{
		Zenith_Error(LOG_CATEGORY_SCENE,
			"CreateEntity: creation target scene (handle=%d gen=%u) is not loaded; "
			"entity '%s' was not created",
			xTarget.m_iHandle, xTarget.m_uGeneration, strName.c_str());
		return Zenith_Entity();
	}

	return Zenith_Entity(pxData, strName);
}

Zenith_Scene Zenith_SceneManager::CreateEmptyScene(const std::string& strName, bool bAllowSetActive)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "CreateEmptyScene must be called from main thread");
	Zenith_Assert(!s_bRenderTasksActive, "CreateEmptyScene: scene mutation while render tasks are reading — render-task invariant violated");

	int iHandle = AllocateSceneHandle();

	Zenith_SceneData* pxSceneData = new Zenith_SceneData();
	pxSceneData->m_strName = strName;
	pxSceneData->m_iHandle = iHandle;
	pxSceneData->m_uGeneration = Zenith_SceneRegistry::s_axSceneGenerations.Get(iHandle);
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
	pxSceneData->m_ulLoadTimestamp = Zenith_SceneRegistry::s_ulNextLoadTimestamp++;

	// Ensure vector is large enough
	while (Zenith_SceneRegistry::s_axScenes.GetSize() <= static_cast<u_int>(iHandle))
	{
		Zenith_SceneRegistry::s_axScenes.PushBack(nullptr);
	}
	Zenith_SceneRegistry::s_axScenes.Get(iHandle) = pxSceneData;
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
	if (bAllowSetActive && Zenith_SceneRegistry::s_iActiveSceneHandle < 0)
	{
		Zenith_Assert(!s_bRenderTasksActive, "Cannot change active scene while render tasks are in flight");
		Zenith_SceneRegistry::s_iActiveSceneHandle = iHandle;
	}

	Zenith_Scene xScene;
	xScene.m_iHandle = iHandle;
	xScene.m_uGeneration = Zenith_SceneRegistry::s_axSceneGenerations.Get(iHandle);
	return xScene;
}

//==========================================================================
// Scene Queries
//==========================================================================
// GetActiveScene, GetSceneAt, GetSceneByBuildIndex, GetSceneByName all live
// on Zenith_SceneRegistry (post-A2). GetSceneByPath stays here because it
// depends on the file-local CanonicalizeScenePath helper above.

Zenith_Scene Zenith_SceneManager::GetSceneByPath(const std::string& strPath)
{
	// Canonicalize first; the registry's GetSceneByPath expects a canonical path.
	return Zenith_SceneRegistry::GetSceneByPath(CanonicalizeScenePath(strPath));
}

// Build index registry (RegisterSceneBuildIndex / ClearBuildIndexRegistry /
// GetRegisteredScenePath / GetBuildIndexRegistrySize) lives on
// Zenith_SceneRegistry.

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

Zenith_SceneOperationID Zenith_SceneManager::GetLastDeferredLoadOp()
{
	return Zenith_SceneLifecycleScheduler::s_ulLastDeferredLoadOp;
}

// HandleDeferredLoad / ValidateFileAndDetectCircular /
// PerformSingleModeTeardownAndSwap / DispatchLifecycleAndFire were the
// pre-B4 multi-phase helpers backing the synchronous LoadScene body. Post-B4
// LoadScene is queue-and-defer; the multi-phase work moved to
// Zenith_SceneOperationQueue::RunAsyncJobPhase1/2 (file I/O, header
// validation, teardown, lifecycle dispatch). The helpers here have been
// retired with the rest of the sync body.

Zenith_Scene Zenith_SceneManager::LoadScene(const std::string& strPath, Zenith_SceneLoadMode eMode)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "LoadScene must be called from main thread");
	Zenith_Assert(!s_bRenderTasksActive, "LoadScene: scene mutation while render tasks are reading — render-task invariant violated");

	if (!ValidateLoadRequest(strPath))
		return MakeInvalidScene();

	// B4: queue-and-defer. LoadScene no longer completes synchronously. It
	// flushes prior in-flight async ops (Unity's documented contract) and
	// queues this load as a new async operation. The scene handle is not
	// known until Phase 1 runs during the next Update tick, so the return
	// value is INVALID_SCENE. Callers retrieve the operation via
	// GetLastDeferredLoadOp() and pump Update until completion, or use
	// LoadSceneBlockingForBootstrap / LoadSceneBlocking_ToolsOnly for the
	// authorized synchronous variants.
	Zenith_SceneOperationQueue::CompletePriorOperationsForBlockingLoad();
	const Zenith_SceneOperationID ulOpID = LoadSceneAsync(strPath, eMode);
	Zenith_SceneLifecycleScheduler::s_ulLastDeferredLoadOp = ulOpID;
	return MakeInvalidScene();
}

Zenith_Scene Zenith_SceneManager::LoadSceneByIndex(int iBuildIndex, Zenith_SceneLoadMode eMode)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "LoadSceneByIndex must be called from main thread");

	// B4: queue-and-defer, mirroring LoadScene. LoadSceneAsyncByIndex already
	// owns the build-index handoff (PendingBuildIndexGuard + AsyncLoadJob::
	// m_iBuildIndex), missing-index warning, and failure-op synthesis, so the
	// sync facade is just flush-priors + queue + stash + return INVALID.
	Zenith_SceneOperationQueue::CompletePriorOperationsForBlockingLoad();
	const Zenith_SceneOperationID ulOpID = LoadSceneAsyncByIndex(iBuildIndex, eMode);
	Zenith_SceneLifecycleScheduler::s_ulLastDeferredLoadOp = ulOpID;
	return MakeInvalidScene();
}

//==========================================================================
// Scene Loading (Blocking — bootstrap / tools only)  (B4)
// B4.A delegates straight to LoadScene/LoadSceneByIndex — no behavioural
// change yet. B4.B will swap the bodies to pump Update until completion
// once LoadScene becomes queue-and-defer.
//==========================================================================

// Bootstrap-only context predicate: pre-main-loop OR inside a
// LifecycleDeferralGuard. The guard wraps both the non-tools bootstrap
// (Zenith_Main.cpp) and the tools editor-automation LOAD_INITIAL_SCENE step
// (Zenith_EditorAutomation.cpp), so a single check covers both paths a
// per-game Project_LoadInitialScene can legitimately fire from. Gameplay
// callers from script Update have neither — IsMainLoopRunning is true and
// no guard is on the stack — and remain rejected.
static bool IsBootstrapLoadContext()
{
	if (!Zenith_SceneLifecycleScheduler::s_bIsMainLoopRunning)
		return true;
	if (Zenith_SceneLifecycleScheduler::s_bIsLoadingScene)
		return true;
	return false;
}

// B4.B: pump Zenith_SceneManager::Update until the most recently queued
// deferred load op completes. Used by all four blocking helpers so they
// return a real Zenith_Scene to bootstrap and editor-tool callers, even
// though LoadScene/LoadSceneByIndex are now queue-and-defer.
//
// Re-entrancy guard: same condition as CompletePriorOperationsForBlockingLoad.
// Pumping Update from inside ProcessPendingAsyncLoads (e.g., a SceneLoaded
// handler that calls LoadSceneBlockingForBootstrap) or ProcessPendingAsyncUnloads
// (a SceneUnloading / SceneUnloaded handler that does the same) would re-enter
// the firing op mid-execution. Skip the pump in that case; the inner caller's
// op is still queued and recoverable via GetLastDeferredLoadOp(), and will
// complete on the outer Update tick.
//
// P2: each iteration explicitly waits for in-flight worker file reads via
// WaitForPendingFileReadsForBlockingPump() before pumping Update — the
// iteration cap below is purely a deadlock sentinel, not the IO-wait
// mechanism.
static Zenith_Scene PumpDeferredLoadUntilComplete()
{
	if (Zenith_SceneOperationQueue::s_uProcessingAsyncLoadsDepth > 0)
		return Zenith_Scene::INVALID_SCENE;
	if (Zenith_SceneOperationQueue::s_uProcessingAsyncUnloadsDepth > 0)
		return Zenith_Scene::INVALID_SCENE;
	if (Zenith_SceneLifecycleScheduler::s_bIsUpdating)
		return Zenith_Scene::INVALID_SCENE;

	const Zenith_SceneOperationID ulOpID = Zenith_SceneLifecycleScheduler::s_ulLastDeferredLoadOp;
	if (ulOpID == ZENITH_INVALID_OPERATION_ID)
		return Zenith_Scene::INVALID_SCENE;

	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);
	if (!pxOp)
		return Zenith_Scene::INVALID_SCENE;

	constexpr int iMAX_ITERS = 100000;
	int iIter = 0;
	while (!pxOp->IsComplete() && iIter < iMAX_ITERS)
	{
		Zenith_SceneOperationQueue::WaitForPendingFileReadsForBlockingPump();
		Zenith_SceneManager::Update(0.0f);
		Zenith_SceneManager::WaitForUpdateComplete();
		++iIter;
	}
	Zenith_Assert(iIter < iMAX_ITERS,
		"PumpDeferredLoadUntilComplete: deferred load did not complete after %d iterations", iIter);

	if (pxOp->HasFailed())
		return Zenith_Scene::INVALID_SCENE;

	return pxOp->GetResultScene();
}

Zenith_Scene Zenith_SceneManager::LoadSceneBlockingForBootstrap(const std::string& strPath, Zenith_SceneLoadMode eMode)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "LoadSceneBlockingForBootstrap must be called from main thread");
	Zenith_Assert(IsBootstrapLoadContext(),
		"LoadSceneBlockingForBootstrap called outside a bootstrap context — main loop is running and no "
		"LifecycleDeferralGuard is active. Use LoadSceneAsync from gameplay code, or LoadSceneBlocking_ToolsOnly "
		"from editor commands.");
	LoadScene(strPath, eMode);
	return PumpDeferredLoadUntilComplete();
}

Zenith_Scene Zenith_SceneManager::LoadSceneByIndexBlockingForBootstrap(int iBuildIndex, Zenith_SceneLoadMode eMode)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "LoadSceneByIndexBlockingForBootstrap must be called from main thread");
	Zenith_Assert(IsBootstrapLoadContext(),
		"LoadSceneByIndexBlockingForBootstrap called outside a bootstrap context — main loop is running and no "
		"LifecycleDeferralGuard is active. Use LoadSceneAsyncByIndex from gameplay code, or "
		"LoadSceneByIndexBlocking_ToolsOnly from editor commands.");
	LoadSceneByIndex(iBuildIndex, eMode);
	return PumpDeferredLoadUntilComplete();
}

#ifdef ZENITH_TOOLS
Zenith_Scene Zenith_SceneManager::LoadSceneBlocking_ToolsOnly(const std::string& strPath, Zenith_SceneLoadMode eMode)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "LoadSceneBlocking_ToolsOnly must be called from main thread");
	LoadScene(strPath, eMode);
	return PumpDeferredLoadUntilComplete();
}

Zenith_Scene Zenith_SceneManager::LoadSceneByIndexBlocking_ToolsOnly(int iBuildIndex, Zenith_SceneLoadMode eMode)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "LoadSceneByIndexBlocking_ToolsOnly must be called from main thread");
	LoadSceneByIndex(iBuildIndex, eMode);
	return PumpDeferredLoadUntilComplete();
}
#endif

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
		uint64_t ulOpID = Zenith_SceneOperationQueue::AllocateOperationID();
		pxOp->m_ulOperationID = ulOpID;
		Zenith_SceneOperationQueue::s_axActiveOperations.PushBack(pxOp);
		Zenith_SceneOperationQueue::s_axOperationMap.PushBack({ ulOpID, pxOp });

		std::string strCanonicalPath = CanonicalizeScenePath(strPath);
		std::string strName = ExtractSceneNameFromPath(strCanonicalPath);
		Zenith_Scene xScene = CreateEmptyScene(strName);
		Zenith_SceneData* pxSceneData = GetSceneData(xScene);
		pxSceneData->m_strPath = strCanonicalPath;

		// B.4: apply pending build index from LoadSceneAsyncByIndex before firing
		// SceneLoaded. Previously this branch returned before the patch-up site in
		// LoadSceneAsyncByIndex ran, so LoadSceneAsyncByIndex(idx, ADDITIVE_WITHOUT_LOADING)
		// silently produced a scene with m_iBuildIndex == -1.
		if (Zenith_SceneLifecycleScheduler::s_iPendingBuildIndex >= 0)
		{
			pxSceneData->m_iBuildIndex = Zenith_SceneLifecycleScheduler::s_iPendingBuildIndex;
			Zenith_SceneLifecycleScheduler::s_iPendingBuildIndex = -1;
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
	uint64_t ulOpID = Zenith_SceneOperationQueue::AllocateOperationID();
	pxOp->m_ulOperationID = ulOpID;
	Zenith_SceneOperationQueue::s_axActiveOperations.PushBack(pxOp);
	Zenith_SceneOperationQueue::s_axOperationMap.PushBack({ ulOpID, pxOp });

	// Check if file exists (use original path for file access)
	if (!Zenith_FileAccess::FileExists(strPath.c_str()))
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "LoadSceneAsync: File not found: %s", strPath.c_str());
		// B.5: single-source-of-truth failure path — shared helper keeps the state
		// transitions (SetResultSceneHandle/SetFailed/SetProgress/SetComplete/
		// FireCompletionCallback) in one spot for all synthetic failures.
		Zenith_SceneOperationQueue::FailAsyncLoadOperation(pxOp);
		return ulOpID;
	}

	// Canonicalize path for consistent storage and lookups
	std::string strCanonicalPath = CanonicalizeScenePath(strPath);

	// Circular load detection - prevent loading a scene that's already being loaded
	// THREAD SAFETY NOTE: Both sets are only modified on the main thread:
	//   - insert() called here in LoadSceneAsync() (main thread, asserted at function start)
	//   - erase() called in ProcessPendingAsyncLoads() (main thread, called from Update)
	// The worker thread (AsyncSceneLoadTask) never accesses these sets, so no synchronization needed.
	if (Zenith_SceneLifecycleScheduler::IsCircularLoadDependency(strCanonicalPath))
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "Circular async scene load detected: %s", strCanonicalPath.c_str());
		// B.5: route through the same helper as the file-not-found branch above.
		Zenith_SceneOperationQueue::FailAsyncLoadOperation(pxOp);
		return ulOpID;
	}
	Zenith_SceneLifecycleScheduler::s_axCurrentlyLoadingPaths.PushBack(strCanonicalPath);

	// Warn if exceeding max concurrent async loads
	if (Zenith_SceneOperationQueue::s_axAsyncJobs.GetSize() >= Zenith_SceneOperationQueue::s_uMaxConcurrentAsyncLoads)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "LoadSceneAsync: Maximum concurrent loads (%u) reached, load will proceed: %s",
			Zenith_SceneOperationQueue::s_uMaxConcurrentAsyncLoads, strCanonicalPath.c_str());
	}

	FireSceneLoadStartedCallbacks(strCanonicalPath);

	// Create async job - store both original path (for file I/O) and canonical path
	Zenith_SceneOperationQueue::AsyncLoadJob* pxJob = new Zenith_SceneOperationQueue::AsyncLoadJob();
	pxJob->m_strPath = strPath;              // Keep original for file loading
	pxJob->m_strCanonicalPath = strCanonicalPath;  // Store canonical for cleanup/tracking
	pxJob->m_eMode = eMode;
	pxJob->m_pxOperation = pxOp;
	pxJob->m_pxLoadedData = new Zenith_DataStream();

	// Create and submit task for file loading on worker thread
	pxJob->m_pxTask = new Zenith_Task(ZENITH_PROFILE_INDEX__ASSET_LOAD, Zenith_SceneOperationQueue::AsyncSceneLoadTask, pxJob);
	Zenith_TaskSystem::SubmitTask(pxJob->m_pxTask);

	Zenith_SceneOperationQueue::s_axAsyncJobs.PushBack(pxJob);
	Zenith_SceneOperationQueue::s_bAsyncJobsNeedSort = true;
	return ulOpID;
}

Zenith_SceneOperationID Zenith_SceneManager::LoadSceneAsyncByIndex(int iBuildIndex, Zenith_SceneLoadMode eMode)
{
	const u_int uBuildIndex = static_cast<u_int>(iBuildIndex);
	if (iBuildIndex < 0 || uBuildIndex >= Zenith_SceneRegistry::s_axBuildIndexToPath.GetSize() || Zenith_SceneRegistry::s_axBuildIndexToPath.Get(uBuildIndex).empty())
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "LoadSceneAsyncByIndex: No scene registered for build index %d", iBuildIndex);

		// Create failed operation — B.5: route through FailAsyncLoadOperation for
		// consistency with LoadSceneAsync's synthetic-failure paths.
		Zenith_SceneOperation* pxOp = new Zenith_SceneOperation();
		uint64_t ulOpID = Zenith_SceneOperationQueue::AllocateOperationID();
		pxOp->m_ulOperationID = ulOpID;
		Zenith_SceneOperationQueue::s_axActiveOperations.PushBack(pxOp);
		Zenith_SceneOperationQueue::s_axOperationMap.PushBack({ ulOpID, pxOp });
		Zenith_SceneOperationQueue::FailAsyncLoadOperation(pxOp);
		return ulOpID;
	}

	// B.4: plumb the build index through the same Zenith_SceneLifecycleScheduler::s_iPendingBuildIndex guard the sync
	// path uses. ADDITIVE_WITHOUT_LOADING completes inside LoadSceneAsync without pushing
	// to Zenith_SceneOperationQueue::s_axAsyncJobs, so the old job-patch-up code (retained below as a belt-and-braces
	// safety net for the file-load async path, which uses Zenith_SceneOperationQueue::AsyncLoadJob::m_iBuildIndex
	// directly during Phase 1 scene creation) silently dropped the build index for that
	// mode. The guard fires early so the branch inside LoadSceneAsync can consume it.
	Zenith_SceneOperationID ulOpID;
	{
		PendingBuildIndexGuard xGuard(iBuildIndex);
		ulOpID = LoadSceneAsync(Zenith_SceneRegistry::s_axBuildIndexToPath.Get(uBuildIndex), eMode);
	}

	// For the file-load path, Phase 1 reads Zenith_SceneOperationQueue::AsyncLoadJob::m_iBuildIndex directly. Set
	// it on the newly queued job so Phase 1 assigns the right build index to the scene
	// it creates (Zenith_SceneLifecycleScheduler::s_iPendingBuildIndex is already consumed by the time Phase 1 runs).
	if (Zenith_SceneOperationQueue::s_axAsyncJobs.GetSize() > 0)
	{
		Zenith_SceneOperationQueue::AsyncLoadJob* pxJob = Zenith_SceneOperationQueue::s_axAsyncJobs.GetBack();
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
	if (xScene.m_iHandle == Zenith_SceneRegistry::s_iPersistentSceneHandle)
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
	for (u_int i = 0; i < Zenith_SceneOperationQueue::s_axAsyncUnloadJobs.GetSize(); ++i)
	{
		Zenith_SceneOperationQueue::AsyncUnloadJob* pxJob = Zenith_SceneOperationQueue::s_axAsyncUnloadJobs.Get(i);
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
	for (u_int i = 0; i < Zenith_SceneRegistry::s_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = Zenith_SceneRegistry::s_axScenes.Get(i);
		if (pxData != nullptr && static_cast<int>(i) != Zenith_SceneRegistry::s_iPersistentSceneHandle
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
	if (xScene.m_iHandle == Zenith_SceneRegistry::s_iPersistentSceneHandle)
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
	uint64_t ulOpID = Zenith_SceneOperationQueue::AllocateOperationID();
	pxOp->m_ulOperationID = ulOpID;
	Zenith_SceneOperationQueue::s_axActiveOperations.PushBack(pxOp);
	Zenith_SceneOperationQueue::s_axOperationMap.PushBack({ ulOpID, pxOp });
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
	Zenith_SceneOperationQueue::AsyncUnloadJob* pxJob = new Zenith_SceneOperationQueue::AsyncUnloadJob();
	pxJob->m_iSceneHandle = xScene.m_iHandle;
	pxJob->m_uSceneGeneration = xScene.m_uGeneration;
	pxJob->m_pxOperation = pxOp;
	pxJob->m_uTotalEntities = pxSceneData->GetEntityCount();
	pxJob->m_uDestroyedEntities = 0;
	pxJob->m_bUnloadingCallbackFired = false;

	Zenith_SceneOperationQueue::s_axAsyncUnloadJobs.PushBack(pxJob);
	return ulOpID;
}

//==========================================================================
// Entity Destruction
//==========================================================================

// A5: Destroy / DestroyImmediate / MoveEntityToScene / MergeScenes / MarkEntityPersistent
// bodies live in Zenith_SceneEntityOwnership.
bool Zenith_SceneManager::RenameScene(Zenith_Scene xScene, const std::string& strNewName) { return Zenith_SceneRegistry::RenameScene(xScene, strNewName); }
Zenith_Scene Zenith_SceneManager::GetPersistentScene() { return Zenith_SceneRegistry::GetPersistentScene(); }
void Zenith_SceneManager::Destroy(Zenith_Entity& xEntity) { Zenith_SceneEntityOwnership::Destroy(xEntity); }
void Zenith_SceneManager::Destroy(Zenith_Entity& xEntity, float fDelay) { Zenith_SceneEntityOwnership::Destroy(xEntity, fDelay); }
void Zenith_SceneManager::DestroyImmediate(Zenith_Entity& xEntity) { Zenith_SceneEntityOwnership::DestroyImmediate(xEntity); }
bool Zenith_SceneManager::MoveEntityToScene(Zenith_Entity& xEntity, Zenith_Scene xTarget) { return Zenith_SceneEntityOwnership::MoveEntityToScene(xEntity, xTarget); }
bool Zenith_SceneManager::MergeScenes(Zenith_Scene xSource, Zenith_Scene xTarget) { return Zenith_SceneEntityOwnership::MergeScenes(xSource, xTarget); }
void Zenith_SceneManager::MarkEntityPersistent(Zenith_Entity& xEntity) { Zenith_SceneEntityOwnership::MarkEntityPersistent(xEntity); }

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
	if (xScene.m_iHandle == Zenith_SceneRegistry::s_iPersistentSceneHandle)
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
	Zenith_SceneRegistry::s_iActiveSceneHandle = xScene.m_iHandle;
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

#ifdef ZENITH_TESTING
	++s_uUnloadUnusedAssetsCallCount;
#endif

	// Unity-parity: SCENE_LOAD_SINGLE auto-fires this between teardown and the
	// new scene's load (see Zenith_SceneOperationQueue.cpp:695). Frees every asset
	// whose refcount has dropped to zero — anything still referenced by a handle
	// (engine fallbacks, live scene materials, etc.) survives untouched.
	//
	// Asset registry contract: Get<>/Create<> populate m_xAssetsByPath without
	// AddRef'ing; the asset's refcount is managed entirely by Zenith_AssetHandle.
	// UnloadUnused() walks the cache and deletes every entry with refcount == 0.
	Zenith_AssetRegistry::UnloadUnused();
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
	for (u_int i = 0; i < Zenith_SceneRegistry::s_axScenes.GetSize(); ++i)
	{
		Zenith_SceneData* pxData = Zenith_SceneRegistry::s_axScenes.Get(i);
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
	return Zenith_SceneRegistry::GetSceneSlotCount();
}

Zenith_SceneData* Zenith_SceneManager::GetSceneDataAtSlot(uint32_t uIndex)
{
	return Zenith_SceneRegistry::GetSceneDataAtSlot(uIndex);
}

Zenith_SceneData* Zenith_SceneManager::GetLoadedSceneDataAtSlot(uint32_t uIndex)
{
	return Zenith_SceneRegistry::GetLoadedSceneDataAtSlot(uIndex);
}

// Implementation lives in Zenith_SceneRegistry.cpp post-A2. Body here is a forwarder
// so internal lifecycle code (FireUnloadCallbacksAndSelectNewActive etc.) can keep
// calling SelectNewActiveScene() unqualified.
int Zenith_SceneManager::SelectNewActiveScene(int iExcludeHandle)
{
	return Zenith_SceneRegistry::SelectNewActiveScene(iExcludeHandle);
}

// Internal helper that moves entity between scenes using zero-copy transfer.
// EntityID is preserved (globally unique) - no serialize/deserialize.
// No lifecycle events fire (Unity parity: MoveGameObjectToScene is seamless).
// Returns true on success, false on failure.
// Cross-scene entity operations (MoveEntityInternal, MoveEntityToScene,
// MergeScenes, MarkEntityPersistent, Destroy*) live on
// Zenith_SceneEntityOwnership (post-A5). RenameScene + GetPersistentScene
// live on Zenith_SceneRegistry (post-A2).

//==========================================================================
// Event Callbacks
//==========================================================================

// AllocateOperationID body migrated to Zenith_SceneOperationQueue::AllocateOperationID.

// Callback subsystem (Zenith_CallbackList<T>, AllocateCallbackHandle,
// IsCallbackHandleInUse, IsCallbackPendingRemoval, ProcessPendingCallbackRemovals)
// is defined in Zenith_SceneCallbackBus.cpp post-A1. The public Register/Unregister/
// Fire methods on Zenith_SceneManager are forwarders defined near the end of this file.

//==========================================================================
// Internal
//==========================================================================

//==========================================================================
// Lifecycle-flag RAII guards (A6.1)
// After A6.3, these are the only writers of s_bIsLoadingScene /
// s_bIsPrefabInstantiating / s_bIsUpdating in production code (the public
// SetLoadingScene/SetPrefabInstantiating/SetIsUpdating methods are removed).
//==========================================================================

Zenith_SceneManager::PrefabInstantiationGuard::PrefabInstantiationGuard()
	: m_bPrevValue(Zenith_SceneLifecycleScheduler::s_bIsPrefabInstantiating)
{
	Zenith_SceneLifecycleScheduler::s_bIsPrefabInstantiating = true;
}

Zenith_SceneManager::PrefabInstantiationGuard::~PrefabInstantiationGuard()
{
	Zenith_SceneLifecycleScheduler::s_bIsPrefabInstantiating = m_bPrevValue;
}

Zenith_SceneManager::SceneUpdateDeferralGuard::SceneUpdateDeferralGuard()
	: m_bPrevValue(Zenith_SceneLifecycleScheduler::s_bIsUpdating)
{
	Zenith_SceneLifecycleScheduler::s_bIsUpdating = true;
}

Zenith_SceneManager::SceneUpdateDeferralGuard::~SceneUpdateDeferralGuard()
{
	Zenith_SceneLifecycleScheduler::s_bIsUpdating = m_bPrevValue;
}

//==========================================================================
// SceneCreationTargetScope (B1) — push/pop the scheduler's creation-target
// stack. Read via Zenith_SceneLifecycleContext::GetCurrentCreationTarget().
//==========================================================================

Zenith_SceneManager::SceneCreationTargetScope::SceneCreationTargetScope(Zenith_Scene xScene)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "SceneCreationTargetScope must be constructed on the main thread");
	Zenith_SceneLifecycleScheduler::s_axCreationTargetStack.PushBack(xScene);
}

Zenith_SceneManager::SceneCreationTargetScope::~SceneCreationTargetScope()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "SceneCreationTargetScope must be destroyed on the main thread");
	Zenith_Assert(Zenith_SceneLifecycleScheduler::s_axCreationTargetStack.GetSize() > 0,
		"SceneCreationTargetScope: creation-target stack underflow on destruction");
	Zenith_SceneLifecycleScheduler::s_axCreationTargetStack.PopBack();
}

Zenith_Scene Zenith_SceneManager::GetDefaultCreationScene()
{
	const u_int uDepth = Zenith_SceneLifecycleScheduler::s_axCreationTargetStack.GetSize();
	if (uDepth > 0)
	{
		return Zenith_SceneLifecycleScheduler::s_axCreationTargetStack.Get(uDepth - 1);
	}
	return GetActiveScene();
}

void Zenith_SceneManager::SetMainLoopRunning(bool bRunning)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "SetMainLoopRunning must be called from main thread");
	Zenith_SceneLifecycleScheduler::s_bIsMainLoopRunning = bRunning;
}

void Zenith_SceneManager::SetFixedTimestep(float fTimestep) { Zenith_SceneLifecycleScheduler::SetFixedTimestep(fTimestep); }
float Zenith_SceneManager::GetFixedTimestep() { return Zenith_SceneLifecycleScheduler::GetFixedTimestep(); }

// A4: Update / WaitForUpdateComplete bodies live in Zenith_SceneLifecycleScheduler.cpp.
void Zenith_SceneManager::Update(float fDt) { Zenith_SceneLifecycleScheduler::Update(fDt); }
void Zenith_SceneManager::WaitForUpdateComplete() { Zenith_SceneLifecycleScheduler::WaitForUpdateComplete(); }

Zenith_SceneData* Zenith_SceneManager::GetSceneData(Zenith_Scene xScene) { return Zenith_SceneRegistry::GetSceneData(xScene); }
Zenith_SceneData* Zenith_SceneManager::GetSceneDataByHandle(int iHandle) { return Zenith_SceneRegistry::GetSceneDataByHandle(iHandle); }
Zenith_SceneData* Zenith_SceneManager::GetSceneDataForEntity(Zenith_EntityID xID) { return Zenith_SceneRegistry::GetSceneDataForEntity(xID); }
Zenith_Scene Zenith_SceneManager::GetSceneFromHandle(int iHandle) { return Zenith_SceneRegistry::GetSceneFromHandle(iHandle); }

// A4: lifecycle accessors / setters / dispatch hook all forward to scheduler.
bool Zenith_SceneManager::IsLoadingScene() { return Zenith_SceneLifecycleScheduler::s_bIsLoadingScene; }
bool Zenith_SceneManager::IsPrefabInstantiating() { return Zenith_SceneLifecycleScheduler::s_bIsPrefabInstantiating; }
bool Zenith_SceneManager::IsUpdating() { return Zenith_SceneLifecycleScheduler::s_bIsUpdating; }
bool Zenith_SceneManager::IsActiveSceneSuppressed() { return Zenith_SceneCallbackBus::IsActiveSceneSuppressed(); }
int Zenith_SceneManager::GetPendingBuildIndex() { return Zenith_SceneLifecycleScheduler::s_iPendingBuildIndex; }
bool Zenith_SceneManager::IsCircularLoadDependency(const std::string& strCanonicalPath) { return Zenith_SceneLifecycleScheduler::IsCircularLoadDependency(strCanonicalPath); }
void Zenith_SceneManager::SetInitialSceneLoadCallback(InitialSceneLoadFn pfnCallback) { Zenith_SceneLifecycleScheduler::SetInitialSceneLoadCallback(pfnCallback); }
Zenith_SceneManager::InitialSceneLoadFn Zenith_SceneManager::GetInitialSceneLoadCallback() { return Zenith_SceneLifecycleScheduler::GetInitialSceneLoadCallback(); }
#ifdef ZENITH_TESTING
void Zenith_SceneManager::DispatchFullLifecycleInit() { Zenith_SceneLifecycleScheduler::DispatchFullLifecycleInit(); }
#endif

int Zenith_SceneManager::AllocateSceneHandle() { return Zenith_SceneRegistry::AllocateSceneHandle(); }
void Zenith_SceneManager::FreeSceneHandle(int iHandle) { Zenith_SceneRegistry::FreeSceneHandle(iHandle); }

// Free-function forwarder for use from Zenith_SceneData.h template bodies
// (declared in Zenith_RenderTaskState.h). The static class accessor and
// underlying flag storage stay in Zenith_SceneManagerInternal.h — this file
// is just the definition site, intentionally minimal.
bool Zenith_AreRenderTasksActive() { return Zenith_SceneManager::AreRenderTasksActive(); }

void Zenith_SceneManager::UnloadOneScene(Zenith_Scene xScene, bool& bActiveSceneUnloadedInOut)
{
	if (xScene.m_iHandle == Zenith_SceneRegistry::s_iActiveSceneHandle)
	{
		bActiveSceneUnloadedInOut = true;
	}

	delete Zenith_SceneRegistry::s_axScenes.Get(xScene.m_iHandle);
	Zenith_SceneRegistry::s_axScenes.Get(xScene.m_iHandle) = nullptr;

	// Fire unloaded callback BEFORE incrementing generation so the handle
	// is still valid for identification in callbacks (Unity parity).
	FireSceneUnloadedCallbacks(xScene);

	FreeSceneHandle(xScene.m_iHandle);
}

void Zenith_SceneManager::PushLifecycleContext(const std::string& strCanonicalPath) { Zenith_SceneLifecycleScheduler::PushLifecycleContext(strCanonicalPath); }
void Zenith_SceneManager::PopLifecycleContext(const std::string& strCanonicalPath) { Zenith_SceneLifecycleScheduler::PopLifecycleContext(strCanonicalPath); }

void Zenith_SceneManager::ProcessPendingUnloads()
{
	// Synchronous unloading - no pending operations to process
	// Reserved for future async unload implementation if needed
}

void Zenith_SceneManager::CompleteAsyncUnloadJobs(Zenith_HashSet<int>& xAlreadyFiredOut)
{
	// C5: in-flight async unloads are pre-empted by the synchronous bulk path,
	// which itself completes the unload — so the operation finishes successfully,
	// just via a different code path. Mark SUCCEEDED rather than FAILED so
	// status-polling callers see the right outcome.
	for (u_int i = 0; i < Zenith_SceneOperationQueue::s_axAsyncUnloadJobs.GetSize(); ++i)
	{
		Zenith_SceneOperationQueue::AsyncUnloadJob* pxJob = Zenith_SceneOperationQueue::s_axAsyncUnloadJobs.Get(i);
		if (pxJob->m_bUnloadingCallbackFired)
		{
			// HIGH-2: this job already fired SceneUnloading on the async path.
			// Phase 1 below must skip these handles or we'd violate Unity's
			// exactly-one-Unloading guarantee.
			xAlreadyFiredOut.Insert(pxJob->m_iSceneHandle);
		}
		Zenith_SceneOperation* pxOp = pxJob->m_pxOperation;
		pxOp->SetFailed(false);                       // explicit "completed without error"
		pxOp->SetResultSceneHandle(-1);               // E.16: unload ops report INVALID_SCENE
		pxOp->SetProgress(1.0f);
		pxOp->SetComplete(true);
		pxOp->FireCompletionCallback();
		delete pxJob;
	}
	Zenith_SceneOperationQueue::s_axAsyncUnloadJobs.Clear();
}

Zenith_Vector<Zenith_Scene> Zenith_SceneManager::CollectNonPersistentScenes(int iExcludeHandle)
{
	Zenith_Vector<Zenith_Scene> axScenes;
	for (u_int i = 0; i < Zenith_SceneRegistry::s_axScenes.GetSize(); ++i)
	{
		// D.12: skip the explicit excluded handle (atomic-swap sync LoadScene
		// keeps the staging scene alive while old scenes are torn down).
		if (static_cast<int>(i) == Zenith_SceneRegistry::s_iPersistentSceneHandle ||
		    static_cast<int>(i) == iExcludeHandle)
		{
			continue;
		}
		Zenith_SceneData* pxData = Zenith_SceneRegistry::s_axScenes.Get(i);
		if (pxData && pxData->m_bIsLoaded)
		{
			Zenith_Scene xScene;
			xScene.m_iHandle = static_cast<int>(i);
			xScene.m_uGeneration = Zenith_SceneRegistry::s_axSceneGenerations.Get(i);
			axScenes.PushBack(xScene);
		}
	}
	return axScenes;
}

bool Zenith_SceneManager::DestroyScenesAndFireUnloaded(const Zenith_Vector<Zenith_Scene>& axScenes,
	const Zenith_HashSet<int>& xAlreadyFired)
{
	// Phase 1: fire SceneUnloading BEFORE any destruction so cleanup code can
	// still touch scene data. HIGH-2 dedup against xAlreadyFired prevents a
	// second Unloading callback for scenes the cancelled async path already
	// notified. SceneUnloaded still fires in Phase 2 (HIGH-3 — pair Unloaded
	// with the earlier Unloading on the cancel path).
	for (u_int i = 0; i < axScenes.GetSize(); ++i)
	{
		const Zenith_Scene& xScene = axScenes.Get(i);
		if (!xAlreadyFired.Contains(xScene.m_iHandle))
		{
			FireSceneUnloadingCallbacks(xScene);
		}
	}

	// Phase 2: destroy + fire SceneUnloaded. UnloadOneScene mutates the bool
	// ref; aggregate the result into bActiveSceneUnloaded for the caller.
	bool bActiveSceneUnloaded = false;
	for (u_int i = 0; i < axScenes.GetSize(); ++i)
	{
		UnloadOneScene(axScenes.Get(i), bActiveSceneUnloaded);
	}
	return bActiveSceneUnloaded;
}

void Zenith_SceneManager::UpdateActiveSceneAfterUnload(Zenith_Scene xOldActive)
{
	// A6: clear the active handle. Do NOT fall back to the persistent scene —
	// it's a DontDestroyOnLoad container, not a real scene. The next LoadScene
	// will set a new active scene; during the gap GetActiveScene returns
	// INVALID (callers handle it via IsValid() paths).
	Zenith_Assert(!s_bRenderTasksActive, "Cannot change active scene while render tasks are in flight");
	Zenith_SceneRegistry::s_iActiveSceneHandle = -1;
	Zenith_Scene xNewActive = GetActiveScene();  // will be INVALID_SCENE
	if (xOldActive == xNewActive)
	{
		return;
	}
	// A5: suppress during SINGLE-load teardown — LoadScene fires the single
	// consolidated old→new callback once the new scene is active.
	if (Zenith_SceneCallbackBus::IsActiveSceneSuppressed())
	{
		if (!Zenith_SceneCallbackBus::HasDeferredOldActive())
		{
			Zenith_SceneCallbackBus::SetDeferredOldActive(xOldActive);
		}
	}
	else
	{
		FireActiveSceneChangedCallbacks(xOldActive, xNewActive);
	}
}

void Zenith_SceneManager::UnloadAllNonPersistent(int iExcludeHandle)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "UnloadAllNonPersistent must be called from main thread");
	Zenith_Assert(!s_bRenderTasksActive, "UnloadAllNonPersistent: scene mutation while render tasks are reading — render-task invariant violated");

	Zenith_HashSet<int> xAlreadyFired;
	CompleteAsyncUnloadJobs(xAlreadyFired);

	// Capture active scene BEFORE destruction (FreeSceneHandle increments generation).
	const Zenith_Scene xOldActive = GetActiveScene();
	const Zenith_Vector<Zenith_Scene> axScenesToUnload = CollectNonPersistentScenes(iExcludeHandle);
	const bool bActiveSceneUnloaded = DestroyScenesAndFireUnloaded(axScenesToUnload, xAlreadyFired);

	if (bActiveSceneUnloaded)
	{
		UpdateActiveSceneAfterUnload(xOldActive);
	}
}

uint32_t Zenith_SceneManager::CountScenesBeingAsyncUnloaded() { return Zenith_SceneOperationQueue::CountScenesBeingAsyncUnloaded(); }

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

//==========================================================================
// Scene-query API forwarders (A2). Bodies live in Zenith_SceneRegistry.cpp.
//==========================================================================

uint32_t Zenith_SceneManager::GetLoadedSceneCount() { return Zenith_SceneRegistry::GetLoadedSceneCount(); }
uint32_t Zenith_SceneManager::GetTotalSceneCount() { return Zenith_SceneRegistry::GetTotalSceneCount(); }
uint32_t Zenith_SceneManager::GetBuildSceneCount() { return Zenith_SceneRegistry::GetBuildSceneCount(); }

Zenith_Scene Zenith_SceneManager::GetActiveScene() { return Zenith_SceneRegistry::GetActiveScene(); }
Zenith_Scene Zenith_SceneManager::GetSceneAt(uint32_t uIndex) { return Zenith_SceneRegistry::GetSceneAt(uIndex); }
Zenith_Scene Zenith_SceneManager::GetSceneByBuildIndex(int iBuildIndex) { return Zenith_SceneRegistry::GetSceneByBuildIndex(iBuildIndex); }
Zenith_Scene Zenith_SceneManager::GetSceneByName(const std::string& strName) { return Zenith_SceneRegistry::GetSceneByName(strName); }

void Zenith_SceneManager::RegisterSceneBuildIndex(int iBuildIndex, const std::string& strPath) { Zenith_SceneRegistry::RegisterSceneBuildIndex(iBuildIndex, strPath); }
void Zenith_SceneManager::ClearBuildIndexRegistry() { Zenith_SceneRegistry::ClearBuildIndexRegistry(); }
const std::string& Zenith_SceneManager::GetRegisteredScenePath(int iBuildIndex) { return Zenith_SceneRegistry::GetRegisteredScenePath(iBuildIndex); }
uint32_t Zenith_SceneManager::GetBuildIndexRegistrySize() { return Zenith_SceneRegistry::GetBuildIndexRegistrySize(); }

//==========================================================================
// Callback API forwarders (A1).
//
// Public Register/Unregister/Fire methods on Zenith_SceneManager forward
// directly into Zenith_SceneCallbackBus. The bus owns the lists, handle
// allocator, deferred-removal queue, dispatch-depth counter and
// active-scene-changed suppression scope state.
//==========================================================================

Zenith_SceneManager::CallbackHandle Zenith_SceneManager::RegisterActiveSceneChangedCallback(SceneChangedCallback pfn) { return Zenith_SceneCallbackBus::RegisterActiveSceneChanged(pfn); }
void Zenith_SceneManager::UnregisterActiveSceneChangedCallback(CallbackHandle ulHandle) { Zenith_SceneCallbackBus::UnregisterActiveSceneChanged(ulHandle); }

Zenith_SceneManager::CallbackHandle Zenith_SceneManager::RegisterSceneLoadedCallback(SceneLoadedCallback pfn) { return Zenith_SceneCallbackBus::RegisterSceneLoaded(pfn); }
void Zenith_SceneManager::UnregisterSceneLoadedCallback(CallbackHandle ulHandle) { Zenith_SceneCallbackBus::UnregisterSceneLoaded(ulHandle); }

Zenith_SceneManager::CallbackHandle Zenith_SceneManager::RegisterSceneUnloadingCallback(SceneUnloadingCallback pfn) { return Zenith_SceneCallbackBus::RegisterSceneUnloading(pfn); }
void Zenith_SceneManager::UnregisterSceneUnloadingCallback(CallbackHandle ulHandle) { Zenith_SceneCallbackBus::UnregisterSceneUnloading(ulHandle); }

Zenith_SceneManager::CallbackHandle Zenith_SceneManager::RegisterSceneUnloadedCallback(SceneUnloadedCallback pfn) { return Zenith_SceneCallbackBus::RegisterSceneUnloaded(pfn); }
void Zenith_SceneManager::UnregisterSceneUnloadedCallback(CallbackHandle ulHandle) { Zenith_SceneCallbackBus::UnregisterSceneUnloaded(ulHandle); }

Zenith_SceneManager::CallbackHandle Zenith_SceneManager::RegisterSceneLoadStartedCallback(SceneLoadStartedCallback pfn) { return Zenith_SceneCallbackBus::RegisterSceneLoadStarted(pfn); }
void Zenith_SceneManager::UnregisterSceneLoadStartedCallback(CallbackHandle ulHandle) { Zenith_SceneCallbackBus::UnregisterSceneLoadStarted(ulHandle); }

Zenith_SceneManager::CallbackHandle Zenith_SceneManager::RegisterEntityPersistentCallback(EntityPersistentCallback pfn) { return Zenith_SceneCallbackBus::RegisterEntityPersistent(pfn); }
void Zenith_SceneManager::UnregisterEntityPersistentCallback(CallbackHandle ulHandle) { Zenith_SceneCallbackBus::UnregisterEntityPersistent(ulHandle); }

//==========================================================================
// Async-operation API forwarders (A3). Bodies live in Zenith_SceneOperationQueue.cpp.
//==========================================================================

void Zenith_SceneManager::NotifyAsyncJobPriorityChanged() { Zenith_SceneOperationQueue::NotifyAsyncJobPriorityChanged(); }
Zenith_SceneOperation* Zenith_SceneManager::GetOperation(Zenith_SceneOperationID ulID) { return Zenith_SceneOperationQueue::GetOperation(ulID); }
bool Zenith_SceneManager::IsOperationValid(Zenith_SceneOperationID ulID) { return Zenith_SceneOperationQueue::IsOperationValid(ulID); }
void Zenith_SceneManager::SetAsyncUnloadBatchSize(uint32_t uEntitiesPerFrame) { Zenith_SceneOperationQueue::SetAsyncUnloadBatchSize(uEntitiesPerFrame); }
uint32_t Zenith_SceneManager::GetAsyncUnloadBatchSize() { return Zenith_SceneOperationQueue::GetAsyncUnloadBatchSize(); }
void Zenith_SceneManager::SetMaxConcurrentAsyncLoads(uint32_t uMax) { Zenith_SceneOperationQueue::SetMaxConcurrentAsyncLoads(uMax); }
uint32_t Zenith_SceneManager::GetMaxConcurrentAsyncLoads() { return Zenith_SceneOperationQueue::GetMaxConcurrentAsyncLoads(); }

void Zenith_SceneManager::FireSceneLoadedCallbacks(Zenith_Scene xScene, Zenith_SceneLoadMode eMode) { Zenith_SceneCallbackBus::FireSceneLoaded(xScene, eMode); }
void Zenith_SceneManager::FireSceneUnloadingCallbacks(Zenith_Scene xScene) { Zenith_SceneCallbackBus::FireSceneUnloading(xScene); }
void Zenith_SceneManager::FireSceneUnloadedCallbacks(Zenith_Scene xScene) { Zenith_SceneCallbackBus::FireSceneUnloaded(xScene); }
void Zenith_SceneManager::FireActiveSceneChangedCallbacks(Zenith_Scene xOld, Zenith_Scene xNew) { Zenith_SceneCallbackBus::FireActiveSceneChanged(xOld, xNew); }
void Zenith_SceneManager::FireSceneLoadStartedCallbacks(const std::string& strPath) { Zenith_SceneCallbackBus::FireSceneLoadStarted(strPath); }
void Zenith_SceneManager::FireEntityPersistentCallbacks(const Zenith_Entity& xEntity) { Zenith_SceneCallbackBus::FireEntityPersistent(xEntity); }

//==========================================================================
// Zenith_SceneLifecycleContext (A0): read-only cross-subsystem state surface.
// Implementations forward to the corresponding Zenith_SceneManager accessors
// during Phase A. As subsystems are extracted, each forwarder is repointed
// at the new owner without changing the call surface.
//==========================================================================

#include "EntityComponent/Internal/Zenith_SceneLifecycleContext.h"

namespace Zenith_SceneLifecycleContext
{
	bool IsLoadingScene()
	{
		return Zenith_SceneManager::IsLoadingScene();
	}

	bool IsPrefabInstantiating()
	{
		return Zenith_SceneManager::IsPrefabInstantiating();
	}

	bool IsUpdating()
	{
		return Zenith_SceneManager::IsUpdating();
	}

	bool IsMainLoopRunning()
	{
		return Zenith_SceneLifecycleScheduler::s_bIsMainLoopRunning;
	}

	int GetPendingBuildIndex()
	{
		return Zenith_SceneManager::GetPendingBuildIndex();
	}

	bool IsCircularLoadDependency(const std::string& strCanonicalPath)
	{
		return Zenith_SceneManager::IsCircularLoadDependency(strCanonicalPath);
	}

	bool IsActiveSceneSuppressed()
	{
		return Zenith_SceneManager::IsActiveSceneSuppressed();
	}

	Zenith_Scene GetCurrentCreationTarget()
	{
		const u_int uDepth = Zenith_SceneLifecycleScheduler::s_axCreationTargetStack.GetSize();
		if (uDepth == 0)
		{
			return Zenith_Scene::INVALID_SCENE;
		}
		return Zenith_SceneLifecycleScheduler::s_axCreationTargetStack.Get(uDepth - 1);
	}
}

#ifdef ZENITH_TESTING
#include "EntityComponent/Zenith_SceneManager.Tests.inl"
#endif

