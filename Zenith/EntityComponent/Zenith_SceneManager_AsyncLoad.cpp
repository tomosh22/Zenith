#include "Zenith.h"

#include <algorithm>
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneManager_Internal.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_SceneOperation.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "DataStream/Zenith_DataStream.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "Physics/Zenith_Physics.h"

// Pull in the detail symbols so existing body code can use unqualified names.
using namespace Zenith_SceneManagerDetail;

//==========================================================================
// Zenith_SceneManager — async load / unload subsystem
//
// Carved out of Zenith_SceneManager.cpp so the file-I/O-and-activation state
// machine lives next to its helpers. Everything here operates on the static
// SceneManager state (s_axAsyncJobs, s_axAsyncUnloadJobs, s_axOperationMap,
// s_axActiveOperations, etc.) declared in the main translation unit.
//==========================================================================

// Progress-milestone constants live in Zenith_SceneManager_Internal.h (namespace
// Zenith_SceneManagerDetail). The `using namespace Zenith_SceneManagerDetail;` above
// makes them unqualified here, so the main TU and this one share a single source of
// truth — no more drift risk from the previous duplicated copy.

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

		// If the scene was already created during Phase 1, delete it.
		// A5: validate the stored generation still matches the current slot generation
		// before deleting. If they differ, the slot was recycled (scene was unloaded
		// elsewhere and another scene was allocated into the same handle) — deleting
		// would corrupt the *replacement* scene. Silently skip the delete; the
		// replacement's own lifecycle will manage it.
		if (pxJob->m_iCreatedSceneHandle >= 0)
		{
			const int iHandle = pxJob->m_iCreatedSceneHandle;
			const bool bGenerationStillValid =
				iHandle < static_cast<int>(s_axSceneGenerations.GetSize()) &&
				s_axSceneGenerations.Get(iHandle) == pxJob->m_uCreatedSceneGeneration;

			if (bGenerationStillValid)
			{
				Zenith_SceneData* pxSceneData = GetSceneDataByHandle(iHandle);
				if (pxSceneData)
				{
					// E.15 (finding 3.9): Phase 1 already deserialized entities into this
					// scene. Subscribers who registered for SceneUnloading/SceneUnloaded
					// expect lifecycle symmetry — every scene that exists at some point
					// must fire these callbacks when it goes away, even on cancel paths.
					// SceneLoaded was NOT fired (Phase 2 never ran), so firing only
					// Unloading+Unloaded here preserves the "every loaded scene is
					// eventually unloaded" invariant.
					Zenith_Scene xCreatedScene;
					xCreatedScene.m_iHandle = iHandle;
					xCreatedScene.m_uGeneration = pxJob->m_uCreatedSceneGeneration;

					FireSceneUnloadingCallbacks(xCreatedScene);
					delete pxSceneData;
					s_axScenes.Get(iHandle) = nullptr;
					FireSceneUnloadedCallbacks(xCreatedScene);
					FreeSceneHandle(iHandle);
				}
			}
			else
			{
				Zenith_Warning(LOG_CATEGORY_SCENE,
					"CancelAllPendingAsyncLoads: job's created scene slot was recycled "
					"(handle=%d, stored gen=%u) — skipping cleanup to avoid corrupting the replacement.",
					iHandle, pxJob->m_uCreatedSceneGeneration);
			}
		}

		FailAsyncLoadOperation(pxOp);
		CleanupAndRemoveAsyncJob(static_cast<u_int>(i));
	}
}

void Zenith_SceneManager::FailAsyncLoadOperation(Zenith_SceneOperation* pxOp)
{
	// B.5: double-complete guard. The only way this asserts is if a caller manually
	// stamped the op complete and then routed through here — code paths that land here
	// from LoadSceneAsync / LoadSceneAsyncByIndex / phase handlers all fire exactly
	// once, and tests T21/T22 verify the single-fire invariant.
	Zenith_Assert(!pxOp->IsComplete(),
		"FailAsyncLoadOperation: operation already complete (double-complete). "
		"Every synthetic failure path must call this at most once per op.");
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
	SortAsyncJobsByPriority();

	// Process pending async loads, highest priority first. The per-step helpers
	// return an AsyncJobStepResult telling us how to advance: Removed keeps i,
	// Waiting advances i, FallThrough retries the same i under the next phase.
	for (u_int i = 0; i < s_axAsyncJobs.GetSize(); )
	{
		AsyncLoadJob* pxJob = s_axAsyncJobs.Get(i);
		Zenith_SceneOperation* pxOp = pxJob->m_pxOperation;

		if (HandleAsyncJobCancellation(pxJob, pxOp, i))
			continue; // job removed, i already points to next

		if (pxJob->m_ePhase == AsyncLoadJob::LoadPhase::WAITING_FOR_FILE)
		{
			const AsyncJobStepResult eResult = RunAsyncJobPhase1(pxJob, pxOp, i);
			if (eResult == AsyncJobStepResult::Removed) continue;
			if (eResult == AsyncJobStepResult::Waiting) { ++i; continue; }
			// FallThrough — Phase 1 finished this frame; fall into Phase 2 below.
		}

		if (pxJob->m_ePhase == AsyncLoadJob::LoadPhase::DESERIALIZED)
		{
			const AsyncJobStepResult eResult = RunAsyncJobPhase2(pxJob, pxOp, i);
			if (eResult == AsyncJobStepResult::Removed) continue;
			if (eResult == AsyncJobStepResult::Waiting) { ++i; continue; }
			// Phase 2 never returns FallThrough — it either finishes the job or waits.
		}

		++i;
	}
}

void Zenith_SceneManager::SortAsyncJobsByPriority()
{
	// Insertion sort: O(n^2) but chosen intentionally —
	// 1. Typical N is very small (1-4 concurrent loads in most games)
	// 2. Stable (preserves FIFO order for equal priorities)
	// 3. In-place, no allocation
	// 4. Adaptive: O(n) when already sorted, which is the common case
	// For N > ~50, switch to std::stable_sort.
	if (!s_bAsyncJobsNeedSort || s_axAsyncJobs.GetSize() <= 1)
		return;

	for (u_int i = 1; i < s_axAsyncJobs.GetSize(); ++i)
	{
		AsyncLoadJob* pxJob = s_axAsyncJobs.Get(i);
		const int iPriority = pxJob->m_pxOperation->GetPriority();
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

bool Zenith_SceneManager::HandleAsyncJobCancellation(AsyncLoadJob* pxJob, Zenith_SceneOperation* pxOp, u_int uIndex)
{
	if (!pxOp->IsCancellationRequested())
		return false;

	Zenith_Log(LOG_CATEGORY_SCENE, "Async scene load cancelled: %s", pxJob->m_strCanonicalPath.c_str());

	// If the scene was already created in Phase 1, unload it.
	// A5: use the generation captured at creation time (not the current slot
	// generation) so UnloadSceneForced can detect a recycled slot via IsValid()
	// and no-op instead of unloading the wrong scene.
	//
	// E.15 (finding 3.9): UnloadSceneForced fires SceneUnloading and SceneUnloaded
	// callbacks, preserving lifecycle symmetry — subscribers always see the paired
	// Unloading/Unloaded for any scene that reached Phase 1.
	if (pxJob->m_ePhase == AsyncLoadJob::LoadPhase::DESERIALIZED && pxJob->m_iCreatedSceneHandle >= 0)
	{
		Zenith_Scene xCreatedScene;
		xCreatedScene.m_iHandle = pxJob->m_iCreatedSceneHandle;
		xCreatedScene.m_uGeneration = pxJob->m_uCreatedSceneGeneration;
		UnloadSceneForced(xCreatedScene);
	}

	FailAsyncLoadOperation(pxOp);
	CleanupAndRemoveAsyncJob(uIndex);
	return true;
}

// Phase 1: file-I/O wait → optional header validation → activation-pause gate (SINGLE) →
// teardown (SINGLE) → create scene → deserialize. On success the job transitions to
// LoadPhase::DESERIALIZED and we return FallThrough so the outer loop can try Phase 2
// on the same job.
//
// D.11 (finding 3.16): teardown of the old world (ResetAllRenderSystems +
// UnloadAllNonPersistent + Physics::Reset) is deferred inside Phase 1 until the caller
// has granted activation. Callers that set SetActivationAllowed(false) before the file
// completes will see: file reaches 0.7, progress holds at fPROGRESS_ACTIVATION_PAUSED,
// old scene stays fully live and rendering. The instant SetActivationAllowed(true) is
// called, Phase 1 proceeds with teardown+create+deserialize in a single tick and
// hands off to Phase 2. Matches Unity's AsyncOperation.allowSceneActivation semantics
// for LoadSceneMode.Single — the old scene is the user-facing scene until the swap.
//
// uIndex is mutable because SCENE_LOAD_SINGLE cancels all peer jobs and leaves this
// job at index 0, so we must update the caller's index accordingly.
Zenith_SceneManager::AsyncJobStepResult
Zenith_SceneManager::RunAsyncJobPhase1(AsyncLoadJob* pxJob, Zenith_SceneOperation* pxOp, u_int& uIndex)
{
	// Check if file load is complete (happens on worker thread) — acquire-ordered read
	// pairs with the worker's release-ordered store on completion.
	const FileLoadMilestone eMilestone = pxJob->m_eMilestone.load(std::memory_order_acquire);
	if (!pxJob->m_bFileLoadComplete.load(std::memory_order_acquire))
	{
		// Still loading; surface progress and move on to the next job.
		const float fFileProgress = static_cast<float>(static_cast<uint8_t>(eMilestone)) / 100.0f;
		pxOp->SetProgress(fFileProgress);
		return AsyncJobStepResult::Waiting;
	}

	// D.11: SINGLE-mode activation gate. If the caller paused activation, the old
	// scene must remain live until they resume — no teardown, no deserialization,
	// no render-system reset. Hold progress at the activation-paused milestone so
	// polling UI gets the same "waiting for activation" signal Unity produces.
	// ADDITIVE doesn't care here because there's no teardown to defer; Phase 2
	// still respects the gate for callback ordering.
	if (pxJob->m_eMode == SCENE_LOAD_SINGLE && !pxOp->IsActivationAllowed())
	{
		pxOp->SetProgress(fPROGRESS_ACTIVATION_PAUSED);
		return AsyncJobStepResult::Waiting;
	}

	// File is loaded AND (ADDITIVE OR activation allowed) — proceed to scene creation.
	s_bIsLoadingScene = true;

	// Validate the pre-loaded stream header BEFORE destroying the current world for
	// SCENE_LOAD_SINGLE. A corrupt / unsupported file must not leave the engine
	// scene-less. Mirrors the sync-path check in LoadScene().
	if (pxJob->m_eMode == SCENE_LOAD_SINGLE)
	{
		static constexpr uint64_t ulMIN_HEADER_SIZE = sizeof(u_int) * 2;

		bool bHeaderValid = false;
		if (pxJob->m_pxLoadedData && pxJob->m_pxLoadedData->GetSize() >= ulMIN_HEADER_SIZE)
		{
			pxJob->m_pxLoadedData->SetCursor(0);
			u_int uMagic = 0;
			u_int uVersion = 0;
			*pxJob->m_pxLoadedData >> uMagic;
			*pxJob->m_pxLoadedData >> uVersion;
			pxJob->m_pxLoadedData->SetCursor(0);  // restore for the real load below
			bHeaderValid = (uMagic == Zenith_SceneData::uSCENE_MAGIC
				&& uVersion >= Zenith_SceneData::uSCENE_VERSION_MIN_SUPPORTED
				&& uVersion <= Zenith_SceneData::uSCENE_VERSION_CURRENT);
		}

		if (!bHeaderValid)
		{
			Zenith_Error(LOG_CATEGORY_SCENE, "LoadSceneAsync(SINGLE): Invalid or unsupported scene file, aborting before teardown: '%s'", pxJob->m_strPath.c_str());
			FailAsyncLoadOperation(pxOp);
			s_bIsLoadingScene = false;
			CleanupAndRemoveAsyncJob(uIndex);
			return AsyncJobStepResult::Removed;
		}
	}

	// SCENE_LOAD_SINGLE: tear down the old world before creating the new scene.
	// By this point activation has been granted (see gate above), so the swap-over
	// is imminent — the teardown window is bounded to this single tick.
	if (pxJob->m_eMode == SCENE_LOAD_SINGLE)
	{
		// A5: capture pre-teardown active scene and suppress intermediate
		// ActiveSceneChanged dispatches; Phase 2 fires the single consolidated
		// old→new callback when the new scene is activated.
		const Zenith_Scene xOldActiveSnapshot = GetActiveScene();
		pxJob->m_iSingleModeOldActiveHandle = xOldActiveSnapshot.m_iHandle;
		pxJob->m_uSingleModeOldActiveGeneration = xOldActiveSnapshot.m_uGeneration;
		s_bSuppressActiveSceneChanged = true;
		s_bHaveDeferredOldActive = false;
		s_xDeferredOldActive = Zenith_Scene::INVALID_SCENE;

		ResetAllRenderSystems();
		CancelAllPendingAsyncLoads(pxJob);
		// pxJob is now the only element in s_axAsyncJobs — at index 0. Sync the
		// caller's loop index so subsequent CleanupAndRemoveAsyncJob(uIndex) calls
		// hit the correct slot.
		uIndex = 0;
		UnloadAllNonPersistent();
		Zenith_Physics::Reset();
		s_fFixedTimeAccumulator = 0.0f;  // Unity behavior: reset on scene load

		// Clear the in-progress suppression state now that teardown is done. The
		// actual consolidated ActiveSceneChanged fires in Phase 2 using
		// pxJob->m_xSingleModeOldActive — this decouples the flag from potential
		// cross-frame gaps if activation is paused.
		s_bSuppressActiveSceneChanged = false;
		s_bHaveDeferredOldActive = false;
		s_xDeferredOldActive = Zenith_Scene::INVALID_SCENE;
	}

	// Create the new scene and deserialize.
	const std::string& strCanonicalPath = pxJob->m_strCanonicalPath;
	const std::string strName = ExtractSceneNameFromPath(strCanonicalPath);

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

	pxJob->m_pxLoadedData->SetCursor(0);
	if (!pxSceneData->LoadFromDataStream(*pxJob->m_pxLoadedData))
	{
		// Must use UnloadSceneForced: in SCENE_LOAD_SINGLE the just-created scene
		// may be the only non-persistent scene after teardown, and UnloadScene would
		// be rejected by CanUnloadScene's last-scene guard, leaking a ghost slot.
		Zenith_Error(LOG_CATEGORY_SCENE, "LoadSceneAsync: Failed to deserialize '%s'", pxJob->m_strPath.c_str());
		UnloadSceneForced(xScene);
		FailAsyncLoadOperation(pxOp);
		s_bIsLoadingScene = false;
		CleanupAndRemoveAsyncJob(uIndex);
		return AsyncJobStepResult::Removed;
	}

	pxOp->SetProgress(fPROGRESS_DESERIALIZE_COMPLETE);
	pxOp->SetResultScene(xScene.m_iHandle, xScene.m_uGeneration);

	// Scene deserialized but dormant; transition to Phase 2.
	pxJob->m_iCreatedSceneHandle = xScene.m_iHandle;
	pxJob->m_uCreatedSceneGeneration = xScene.m_uGeneration;
	pxJob->m_ePhase = AsyncLoadJob::LoadPhase::DESERIALIZED;
	s_bIsLoadingScene = false;

	return AsyncJobStepResult::FallThrough;
}

// Phase 2: activation (Awake + OnEnable + scene-loaded callbacks). Runs only when the
// operation has been granted activation (`IsActivationAllowed`). Either finishes the
// job (Removed), or waits for activation (Waiting).
Zenith_SceneManager::AsyncJobStepResult
Zenith_SceneManager::RunAsyncJobPhase2(AsyncLoadJob* pxJob, Zenith_SceneOperation* pxOp, u_int uIndex)
{
	if (!pxOp->IsActivationAllowed())
	{
		pxOp->SetProgress(fPROGRESS_ACTIVATION_PAUSED);
		return AsyncJobStepResult::Waiting;
	}

	s_bIsLoadingScene = true;

	// A5: use the generation captured at Phase 1 end, not the current slot generation.
	// If the slot was recycled between deserialization and activation, GetSceneData
	// returns nullptr and we abort cleanly instead of activating the wrong scene.
	Zenith_Scene xScene;
	xScene.m_iHandle = pxJob->m_iCreatedSceneHandle;
	xScene.m_uGeneration = pxJob->m_uCreatedSceneGeneration;
	Zenith_SceneData* pxSceneData = GetSceneData(xScene);

	if (!pxSceneData)
	{
		// Slot was recycled underneath us — the job's created scene was unloaded
		// and replaced. Fail the op so callers can detect the abort; do not touch
		// s_iActiveSceneHandle or call lifecycle dispatch.
		Zenith_Warning(LOG_CATEGORY_SCENE,
			"ProcessPendingAsyncLoads: Phase 2 aborted — scene slot %d was recycled "
			"(stored gen=%u, current gen=%u).",
			pxJob->m_iCreatedSceneHandle,
			pxJob->m_uCreatedSceneGeneration,
			pxJob->m_iCreatedSceneHandle < static_cast<int>(s_axSceneGenerations.GetSize())
				? s_axSceneGenerations.Get(pxJob->m_iCreatedSceneHandle) : 0);
		s_bIsLoadingScene = false;
		FailAsyncLoadOperation(pxOp);
		CleanupAndRemoveAsyncJob(uIndex);
		return AsyncJobStepResult::Removed;
	}

	// Set active scene for SINGLE mode.
	if (pxJob->m_eMode == SCENE_LOAD_SINGLE)
	{
		Zenith_Assert(!s_bRenderTasksActive, "Cannot change active scene while render tasks are in flight");
		s_iActiveSceneHandle = xScene.m_iHandle;

		// A5: reconstruct the pre-teardown active scene from the snapshot stored on
		// the job so subscribers see a single consolidated old → new transition.
		Zenith_Scene xOldActive;
		xOldActive.m_iHandle = pxJob->m_iSingleModeOldActiveHandle;
		xOldActive.m_uGeneration = pxJob->m_uSingleModeOldActiveGeneration;
		if (xOldActive != xScene)
		{
			FireActiveSceneChangedCallbacks(xOldActive, xScene);
		}
	}

	// Unity behavior: Awake -> OnEnable -> sceneLoaded -> Start(next frame)
	pxSceneData->DispatchAwakeForNewScene();
	pxSceneData->DispatchEnableAndPendingStartsForNewScene();
	pxSceneData->m_bIsActivated = true;

	// D.13 (finding 3.12): clear the loading-scene flag BEFORE SceneLoaded callback
	// dispatch so subscribers see IsLoadingScene() == false. Mirrors the sync path.
	// CleanupAndRemoveAsyncJob below will also clear s_axCurrentlyLoadingPaths; the
	// circular-load guard stays active until the per-path bookkeeping is released.
	s_bIsLoadingScene = false;

	FireSceneLoadedCallbacks(xScene, pxJob->m_eMode);

	pxOp->SetProgress(1.0f);
	pxOp->SetComplete(true);
	pxOp->FireCompletionCallback();

	CleanupAndRemoveAsyncJob(uIndex);
	return AsyncJobStepResult::Removed;
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
