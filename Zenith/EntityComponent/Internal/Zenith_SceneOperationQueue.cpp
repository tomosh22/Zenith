#include "Zenith.h"

#include <algorithm>
#include "EntityComponent/Internal/Zenith_SceneOperationQueue.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneManager_Internal.h"
#include "EntityComponent/Internal/Zenith_SceneCallbackBus.h"
#include "EntityComponent/Internal/Zenith_SceneLifecycleContext.h"
#include "EntityComponent/Internal/Zenith_SceneRegistryImpl.h"
#include "EntityComponent/Internal/Zenith_SceneOperationQueueImpl.h"
#include "EntityComponent/Zenith_SceneOperation.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Physics/Zenith_Physics.h"

// Pull in the detail symbols so existing body code can use unqualified names
// (progress milestone constants).
using namespace Zenith_SceneManagerDetail;

//==========================================================================
// Zenith_SceneOperationQueue — async load / unload subsystem.
//
// Owns the operation map, async load + unload job queues, phase machines,
// and async config knobs. Manager-internal lifecycle code and Zenith_SceneManager's
// public async APIs forward into this class. Progress-milestone constants live
// in Zenith_SceneManager_Internal.h's Zenith_SceneManagerDetail namespace.
//==========================================================================

// Phase 5d: queue state lives on Zenith_SceneOperationQueueImpl
// (held by Zenith_Engine as m_pxSceneOperations). Method bodies read
// and write through g_xEngine.SceneOperations().m_xXxx.

void Zenith_SceneOperationQueue::NotifyAsyncJobPriorityChanged()
{
	g_xEngine.SceneOperations().m_bAsyncJobsNeedSort = true;
}

Zenith_SceneOperationQueue::AsyncLoadJob::~AsyncLoadJob()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "AsyncLoadJob must be deleted from main thread");
	delete m_pxLoadedData;
	delete m_pxTask;
}

Zenith_SceneOperationQueue::AsyncUnloadJob::~AsyncUnloadJob()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "AsyncUnloadJob must be deleted from main thread");
}

//==========================================================================
// Lifecycle
//==========================================================================

void Zenith_SceneOperationQueue::Shutdown()
{
	// Wait for in-flight worker tasks before deleting jobs.
	for (u_int i = 0; i < g_xEngine.SceneOperations().m_axAsyncJobs.GetSize(); ++i)
	{
		AsyncLoadJob* pxJob = g_xEngine.SceneOperations().m_axAsyncJobs.Get(i);
		if (pxJob && pxJob->m_pxTask)
		{
			pxJob->m_pxTask->WaitUntilComplete();
		}
		delete pxJob;
	}
	g_xEngine.SceneOperations().m_axAsyncJobs.Clear();

	for (u_int i = 0; i < g_xEngine.SceneOperations().m_axAsyncUnloadJobs.GetSize(); ++i)
	{
		delete g_xEngine.SceneOperations().m_axAsyncUnloadJobs.Get(i);
	}
	g_xEngine.SceneOperations().m_axAsyncUnloadJobs.Clear();

	for (u_int i = 0; i < g_xEngine.SceneOperations().m_axActiveOperations.GetSize(); ++i)
	{
		delete g_xEngine.SceneOperations().m_axActiveOperations.Get(i);
	}
	g_xEngine.SceneOperations().m_axActiveOperations.Clear();
	g_xEngine.SceneOperations().m_axOperationMap.Clear();

	g_xEngine.SceneOperations().m_bAsyncJobsNeedSort = false;
	g_xEngine.SceneOperations().m_ulNextOperationID = 1;
	g_xEngine.SceneOperations().m_uProcessingAsyncLoadsDepth = 0;
	g_xEngine.SceneOperations().m_uProcessingAsyncUnloadsDepth = 0;
}

void Zenith_SceneOperationQueue::ResetForNextTest()
{
	// ResetForNextTest is the same teardown as Shutdown for the queue —
	// callers (Zenith_SceneManager::ResetForNextTest) drive the rest of
	// the engine separately.
	Shutdown();
}

//==========================================================================
// Operation ID allocation
//==========================================================================

Zenith_SceneOperationID Zenith_SceneOperationQueue::AllocateOperationID()
{
	if (g_xEngine.SceneOperations().m_ulNextOperationID == UINT64_MAX)
	{
		Zenith_Warning(LOG_CATEGORY_SCENE,
			"Operation ID counter wrapped around after %llu allocations",
			static_cast<unsigned long long>(g_xEngine.SceneOperations().m_ulNextOperationID));
		g_xEngine.SceneOperations().m_ulNextOperationID = 1;
	}
	return g_xEngine.SceneOperations().m_ulNextOperationID++;
}

//==========================================================================
// Async unload count (used by SceneCount-style queries)
//==========================================================================

uint32_t Zenith_SceneOperationQueue::CountScenesBeingAsyncUnloaded()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "CountScenesBeingAsyncUnloaded must be called from main thread");
	return static_cast<uint32_t>(g_xEngine.SceneOperations().m_axAsyncUnloadJobs.GetSize());
}

void Zenith_SceneOperationQueue::AsyncSceneLoadTask(void* pData)
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

Zenith_SceneOperation* Zenith_SceneOperationQueue::GetOperation(Zenith_SceneOperationID ulID)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetOperation must be called from main thread");

	if (ulID == ZENITH_INVALID_OPERATION_ID)
	{
		return nullptr;
	}
	for (u_int i = 0; i < g_xEngine.SceneOperations().m_axOperationMap.GetSize(); ++i)
	{
		if (g_xEngine.SceneOperations().m_axOperationMap.Get(i).m_ulOperationID == ulID)
			return g_xEngine.SceneOperations().m_axOperationMap.Get(i).m_pxOperation;
	}
	return nullptr;
}

bool Zenith_SceneOperationQueue::IsOperationValid(Zenith_SceneOperationID ulID)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "IsOperationValid must be called from main thread");

	if (ulID == ZENITH_INVALID_OPERATION_ID)
	{
		return false;
	}
	for (u_int i = 0; i < g_xEngine.SceneOperations().m_axOperationMap.GetSize(); ++i)
	{
		if (g_xEngine.SceneOperations().m_axOperationMap.Get(i).m_ulOperationID == ulID) return true;
	}
	return false;
}

void Zenith_SceneOperationQueue::CleanupCompletedOperations()
{
	// Remove completed operations after a delay to allow result access
	// Users need time to call GetResultScene() after IsComplete() returns true
	for (int i = static_cast<int>(g_xEngine.SceneOperations().m_axActiveOperations.GetSize()) - 1; i >= 0; --i)
	{
		Zenith_SceneOperation* pxOp = g_xEngine.SceneOperations().m_axActiveOperations.Get(static_cast<u_int>(i));
		if (pxOp && pxOp->IsComplete())
		{
			pxOp->m_uFramesSinceComplete++;
			if (pxOp->m_uFramesSinceComplete > uOPERATION_CLEANUP_DELAY_FRAMES)
			{
				// Remove from operation map before deleting
				for (u_int j = 0; j < g_xEngine.SceneOperations().m_axOperationMap.GetSize(); ++j)
				{
					if (g_xEngine.SceneOperations().m_axOperationMap.Get(j).m_ulOperationID == pxOp->m_ulOperationID)
					{
						g_xEngine.SceneOperations().m_axOperationMap.RemoveSwap(j);
						break;
					}
				}
				delete pxOp;
				g_xEngine.SceneOperations().m_axActiveOperations.Remove(static_cast<u_int>(i));
			}
		}
	}
}

u_int Zenith_SceneOperationQueue::CancelAllPendingAsyncLoads(AsyncLoadJob* pxExclude)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "CancelAllPendingAsyncLoads must be called from main thread");

	for (int i = static_cast<int>(g_xEngine.SceneOperations().m_axAsyncJobs.GetSize()) - 1; i >= 0; --i)
	{
		AsyncLoadJob* pxJob = g_xEngine.SceneOperations().m_axAsyncJobs.Get(static_cast<u_int>(i));
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
				iHandle < static_cast<int>(g_xEngine.SceneRegistry().m_axSceneGenerations.GetSize()) &&
				g_xEngine.SceneRegistry().m_axSceneGenerations.Get(iHandle) == pxJob->m_uCreatedSceneGeneration;

			if (bGenerationStillValid)
			{
				Zenith_SceneData* pxSceneData = Zenith_SceneRegistry::GetSceneDataByHandle(iHandle);
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

					Zenith_SceneCallbackBus::FireSceneUnloading(xCreatedScene);
					delete pxSceneData;
					g_xEngine.SceneRegistry().m_axScenes.Get(iHandle) = nullptr;
					Zenith_SceneCallbackBus::FireSceneUnloaded(xCreatedScene);
					Zenith_SceneRegistry::FreeSceneHandle(iHandle);
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

	// Locate pxExclude's surviving slot. Linear scan is fine — g_xEngine.SceneOperations().m_axAsyncJobs is
	// tiny (typically 1 entry after cancel) and this runs once per SINGLE-mode load.
	if (pxExclude == nullptr) return UINT32_MAX;
	for (u_int u = 0; u < g_xEngine.SceneOperations().m_axAsyncJobs.GetSize(); ++u)
	{
		if (g_xEngine.SceneOperations().m_axAsyncJobs.Get(u) == pxExclude) return u;
	}
	return UINT32_MAX;
}

void Zenith_SceneOperationQueue::FailAsyncLoadOperation(Zenith_SceneOperation* pxOp)
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

void Zenith_SceneOperationQueue::CleanupAndRemoveAsyncJob(u_int uIndex)
{
	AsyncLoadJob* pxJob = g_xEngine.SceneOperations().m_axAsyncJobs.Get(uIndex);
	if (pxJob->m_pxTask)
	{
		pxJob->m_pxTask->WaitUntilComplete();
	}
	Zenith_SceneLifecycleScheduler::s_axCurrentlyLoadingPaths.EraseValue(pxJob->m_strCanonicalPath);
	delete pxJob;
	g_xEngine.SceneOperations().m_axAsyncJobs.Remove(uIndex);
}

void Zenith_SceneOperationQueue::ProcessPendingAsyncLoads()
{
	// B4.B: bump the re-entrancy depth so a LoadScene fired from inside one
	// of this pass's SceneLoaded callbacks (Phase 2) does NOT recursively
	// flush the queue via CompletePriorOperationsForBlockingLoad — the firing
	// op is still in g_xEngine.SceneOperations().m_axAsyncJobs at that point, and re-running its phase
	// machine would dispatch the callback again. The local RAII releases
	// even on early return.
	struct ProcessingDepthGuard
	{
		ProcessingDepthGuard()  { ++g_xEngine.SceneOperations().m_uProcessingAsyncLoadsDepth; }
		~ProcessingDepthGuard() { --g_xEngine.SceneOperations().m_uProcessingAsyncLoadsDepth; }
	} xDepthGuard;

	// B2: snapshot the queue-stall predicate up front. The call also runs the
	// priority sort, so the head is at index 0 for the iteration below.
	//
	// Mutable, not const: an ADDITIVE head can transition into the
	// activation-paused state mid-pass — Phase 1 deserializes (file I/O
	// already complete on the worker thread), falls through to Phase 2,
	// and Phase 2 sees IsActivationAllowed()==false → sets progress to
	// fPROGRESS_ACTIVATION_PAUSED and returns Waiting. Without recomputing
	// the predicate after the head finishes its step, the captured `false`
	// would let later jobs advance under the stale value, breaking the
	// queue-stall contract for ADDITIVE heads (the SINGLE-mode case is
	// already covered because Phase 1 SINGLE pauses BEFORE deserialize, so
	// the predicate is true on entry to ProcessPendingAsyncLoads).
	bool bBlockedByPausedHead = IsAsyncQueueBlockedByActivationPausedHead();

	// Process pending async loads, highest priority first. The per-step helpers
	// return an AsyncJobStepResult telling us how to advance: Removed keeps i,
	// Waiting advances i, FallThrough retries the same i under the next phase.
	for (u_int i = 0; i < g_xEngine.SceneOperations().m_axAsyncJobs.GetSize(); )
	{
		AsyncLoadJob* pxJob = g_xEngine.SceneOperations().m_axAsyncJobs.Get(i);
		Zenith_SceneOperation* pxOp = pxJob->m_pxOperation;

		if (HandleAsyncJobCancellation(pxJob, pxOp, i))
			continue; // job removed, i already points to next

		// B2: while the head is activation-paused, behind-the-head jobs do not
		// advance their phase machine (Unity AsyncOperation queue-stall parity).
		// Cancellation above still processes so callers can drop queued ops
		// without resuming the head first. Worker-thread file I/O scheduled
		// before the pause continues; we just skip main-thread phase advancement.
		if (bBlockedByPausedHead && i > 0)
		{
			++i;
			continue;
		}

		if (pxJob->m_ePhase == AsyncLoadJob::LoadPhase::WAITING_FOR_FILE)
		{
			const u_int uIndexBefore = i;
			const AsyncJobStepResult eResult = RunAsyncJobPhase1(pxJob, pxOp, i);
			if (eResult == AsyncJobStepResult::Removed) continue;
			if (eResult == AsyncJobStepResult::Waiting)
			{
				// If the head is what just returned Waiting, recompute the
				// stall predicate before advancing — Phase 1's SINGLE
				// activation-pause gate or any state mutation could have
				// turned the head into a stalling head.
				if (uIndexBefore == 0)
					bBlockedByPausedHead = IsAsyncQueueBlockedByActivationPausedHead();
				++i;
				continue;
			}
			// FallThrough — Phase 1 finished this frame; fall into Phase 2 below.
		}

		if (pxJob->m_ePhase == AsyncLoadJob::LoadPhase::DESERIALIZED)
		{
			const u_int uIndexBefore = i;
			const AsyncJobStepResult eResult = RunAsyncJobPhase2(pxJob, pxOp, i);
			if (eResult == AsyncJobStepResult::Removed) continue;
			if (eResult == AsyncJobStepResult::Waiting)
			{
				// Critical for ADDITIVE: Phase 2's activation-paused exit is
				// the path that turns the head into a stalling head. Recompute
				// before letting later jobs advance.
				if (uIndexBefore == 0)
					bBlockedByPausedHead = IsAsyncQueueBlockedByActivationPausedHead();
				++i;
				continue;
			}
			// Phase 2 never returns FallThrough — it either finishes the job or waits.
		}

		++i;
	}
}

void Zenith_SceneOperationQueue::SortAsyncJobsByPriority()
{
	// Insertion sort: O(n^2) but chosen intentionally —
	// 1. Typical N is very small (1-4 concurrent loads in most games)
	// 2. Stable (preserves FIFO order for equal priorities)
	// 3. In-place, no allocation
	// 4. Adaptive: O(n) when already sorted, which is the common case
	// For N > ~50, switch to std::stable_sort.
	if (!g_xEngine.SceneOperations().m_bAsyncJobsNeedSort || g_xEngine.SceneOperations().m_axAsyncJobs.GetSize() <= 1)
		return;

	for (u_int i = 1; i < g_xEngine.SceneOperations().m_axAsyncJobs.GetSize(); ++i)
	{
		AsyncLoadJob* pxJob = g_xEngine.SceneOperations().m_axAsyncJobs.Get(i);
		const int iPriority = pxJob->m_pxOperation->GetPriority();
		u_int j = i;
		while (j > 0 && g_xEngine.SceneOperations().m_axAsyncJobs.Get(j - 1)->m_pxOperation->GetPriority() < iPriority)
		{
			g_xEngine.SceneOperations().m_axAsyncJobs.Get(j) = g_xEngine.SceneOperations().m_axAsyncJobs.Get(j - 1);
			j--;
		}
		g_xEngine.SceneOperations().m_axAsyncJobs.Get(j) = pxJob;
	}
	g_xEngine.SceneOperations().m_bAsyncJobsNeedSort = false;
}

void Zenith_SceneOperationQueue::CompletePriorOperationsForBlockingLoad()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(),
		"CompletePriorOperationsForBlockingLoad must be called from main thread");

	// B4.B re-entrancy guard. The Unity flush-prior-async semantic only
	// applies to TOP-LEVEL blocking LoadScene calls. Three paths must skip:
	//
	//   * A SceneLoaded handler firing inside Phase 2 calls LoadScene. The
	//     firing op is still in g_xEngine.SceneOperations().m_axAsyncJobs (Cleanup runs after the
	//     callback), so re-pumping would re-enter the same Phase 2 and
	//     re-fire the callback — past the bus's depth-16 safety limit.
	//   * A SceneUnloading / SceneUnloaded handler firing inside the unload
	//     pass calls LoadScene. The firing job is still in g_xEngine.SceneOperations().m_axAsyncUnloadJobs
	//     (and for SceneUnloaded its scene data has already been deleted),
	//     so re-pumping would either fire the unloading callback twice or
	//     trip a use-after-free / double-delete on the unload job.
	//   * Gameplay code calls LoadScene during Update (script handlers, UI
	//     button callbacks). Pre-B4 the engine silently auto-deferred these;
	//     B4 keeps the queue-and-defer behaviour but must not pump the
	//     queue mid-frame either.
	//
	// In all three cases the LoadScene caller still gets its op queued and
	// recoverable via GetLastDeferredLoadOp(); the new op simply completes
	// on the next outer Update tick instead of synchronously.
	if (g_xEngine.SceneOperations().m_uProcessingAsyncLoadsDepth > 0)
		return;
	if (g_xEngine.SceneOperations().m_uProcessingAsyncUnloadsDepth > 0)
		return;
	if (Zenith_SceneLifecycleContext::IsUpdating())
		return;

	// Release any activation-paused ops first; an explicit blocking LoadScene
	// is a stronger signal than the caller's earlier SetActivationAllowed(false).
	for (u_int i = 0; i < g_xEngine.SceneOperations().m_axAsyncJobs.GetSize(); ++i)
	{
		Zenith_SceneOperation* pxOp = g_xEngine.SceneOperations().m_axAsyncJobs.Get(i)->m_pxOperation;
		if (!pxOp->IsActivationAllowed())
			pxOp->SetActivationAllowed(true);
	}

	// Pump the queue until all in-flight load + unload ops drain. Bounded so
	// a deadlocked job can't spin forever — the cap is purely a deadlock
	// sentinel; the actual wait for worker-thread file I/O happens explicitly
	// inside WaitForPendingFileReadsForBlockingPump() below, called once per
	// iteration before the main-thread phase machine runs.
	constexpr int iMAX_ITERS = 100000;
	int iIter = 0;
	while ((g_xEngine.SceneOperations().m_axAsyncJobs.GetSize() > 0 || g_xEngine.SceneOperations().m_axAsyncUnloadJobs.GetSize() > 0) && iIter < iMAX_ITERS)
	{
		WaitForPendingFileReadsForBlockingPump();
		ProcessPendingAsyncLoads();
		ProcessPendingAsyncUnloads();
		++iIter;
	}
	Zenith_Assert(iIter < iMAX_ITERS,
		"CompletePriorOperationsForBlockingLoad: queue did not drain after %d iterations — "
		"in-flight job may be deadlocked (loads=%u, unloads=%u)",
		iIter, g_xEngine.SceneOperations().m_axAsyncJobs.GetSize(), g_xEngine.SceneOperations().m_axAsyncUnloadJobs.GetSize());
}

void Zenith_SceneOperationQueue::WaitForPendingFileReadsForBlockingPump()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(),
		"WaitForPendingFileReadsForBlockingPump must be called from main thread");

	// Match the re-entrancy semantics of CompletePriorOperationsForBlockingLoad
	// and PumpDeferredLoadUntilComplete — blocking the main thread on worker
	// IO from inside a callback is exactly what those guards exist to prevent.
	if (g_xEngine.SceneOperations().m_uProcessingAsyncLoadsDepth > 0)
		return;
	if (g_xEngine.SceneOperations().m_uProcessingAsyncUnloadsDepth > 0)
		return;
	if (Zenith_SceneLifecycleContext::IsUpdating())
		return;

	for (u_int i = 0; i < g_xEngine.SceneOperations().m_axAsyncJobs.GetSize(); ++i)
	{
		AsyncLoadJob* pxJob = g_xEngine.SceneOperations().m_axAsyncJobs.Get(i);
		if (!pxJob || !pxJob->m_pxTask)
			continue;
		if (pxJob->m_bFileLoadComplete.load(std::memory_order_acquire))
			continue;
		// Worker tasks run on the task system; WaitUntilComplete blocks the
		// main thread until the task's lambda returns, after which the worker
		// has stored its result + flipped m_bFileLoadComplete with release
		// semantics. The next ProcessPendingAsyncLoads pass will read it
		// with acquire semantics and advance the phase machine.
		pxJob->m_pxTask->WaitUntilComplete();
	}
}

bool Zenith_SceneOperationQueue::IsAsyncQueueBlockedByActivationPausedHead()
{
	// Self-contained: ensure the head is at index 0. SortAsyncJobsByPriority
	// is idempotent (early-out when no sort is needed), so callers that have
	// already sorted pay a cheap branch.
	SortAsyncJobsByPriority();

	if (g_xEngine.SceneOperations().m_axAsyncJobs.GetSize() == 0)
		return false;

	AsyncLoadJob* pxJob = g_xEngine.SceneOperations().m_axAsyncJobs.Get(0);
	Zenith_SceneOperation* pxOp = pxJob->m_pxOperation;

	// Cancelled / failed / complete heads do NOT block the queue — they will
	// be cleaned up on this same tick (HandleAsyncJobCancellation) or have
	// already finished. A blocking head is one actively waiting on a caller
	// to set IsActivationAllowed(true).
	if (pxOp->IsCancellationRequested())
		return false;
	if (pxOp->IsComplete())
		return false;
	if (pxOp->IsActivationAllowed())
		return false;

	// Activation is paused. Verify the head has actually reached the gated
	// state — otherwise it's still in pre-pause territory (e.g. the file
	// hasn't finished loading yet) and behind-the-head jobs are free to run.
	//   * Phase-2 gate (DESERIALIZED): both SINGLE and ADDITIVE pause here
	//     before activation/Awake.
	//   * Phase-1 SINGLE gate (WAITING_FOR_FILE + file complete): SINGLE
	//     pauses before teardown so the old world stays live until the
	//     caller commits.
	if (pxJob->m_ePhase == AsyncLoadJob::LoadPhase::DESERIALIZED)
		return true;
	if (pxJob->m_ePhase == AsyncLoadJob::LoadPhase::WAITING_FOR_FILE
		&& pxJob->m_eMode == SCENE_LOAD_SINGLE
		&& pxJob->m_bFileLoadComplete.load(std::memory_order_acquire))
		return true;

	return false;
}

bool Zenith_SceneOperationQueue::HandleAsyncJobCancellation(AsyncLoadJob* pxJob, Zenith_SceneOperation* pxOp, u_int uIndex)
{
	if (!pxOp->IsCancellationRequested())
		return false;

	Zenith_Log(LOG_CATEGORY_SCENE, "Async scene load cancelled: %s", pxJob->m_strCanonicalPath.c_str());

	// If the scene was already created in Phase 1, unload it.
	// A5: use the generation captured at creation time (not the current slot
	// generation) so Zenith_SceneManager::UnloadSceneForced can detect a recycled slot via IsValid()
	// and no-op instead of unloading the wrong scene.
	//
	// E.15 (finding 3.9): Zenith_SceneManager::UnloadSceneForced fires SceneUnloading and SceneUnloaded
	// callbacks, preserving lifecycle symmetry — subscribers always see the paired
	// Unloading/Unloaded for any scene that reached Phase 1.
	if (pxJob->m_ePhase == AsyncLoadJob::LoadPhase::DESERIALIZED && pxJob->m_iCreatedSceneHandle >= 0)
	{
		Zenith_Scene xCreatedScene;
		xCreatedScene.m_iHandle = pxJob->m_iCreatedSceneHandle;
		xCreatedScene.m_uGeneration = pxJob->m_uCreatedSceneGeneration;
		Zenith_SceneManager::UnloadSceneForced(xCreatedScene);
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
// D.11 (finding 3.16): teardown of the old world (Zenith_SceneManager::ResetAllRenderSystems +
// Zenith_SceneManager::UnloadAllNonPersistent + Physics::Reset) is deferred inside Phase 1 until the caller
// has granted activation. Callers that set SetActivationAllowed(false) before the file
// completes will see: file reaches 0.7, progress holds at fPROGRESS_ACTIVATION_PAUSED,
// old scene stays fully live and rendering. The instant SetActivationAllowed(true) is
// called, Phase 1 proceeds with teardown+create+deserialize in a single tick and
// hands off to Phase 2. Matches Unity's AsyncOperation.allowSceneActivation semantics
// for LoadSceneMode.Single — the old scene is the user-facing scene until the swap.
//
// uIndex is mutable because SCENE_LOAD_SINGLE cancels all peer jobs and leaves this
// job at index 0, so we must update the caller's index accordingly.
Zenith_SceneOperationQueue::AsyncJobStepResult
Zenith_SceneOperationQueue::RunAsyncJobPhase1(AsyncLoadJob* pxJob, Zenith_SceneOperation* pxOp, u_int& uIndex)
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
	Zenith_SceneLifecycleScheduler::s_bIsLoadingScene = true;

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
			Zenith_SceneLifecycleScheduler::s_bIsLoadingScene = false;
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
		const Zenith_Scene xOldActiveSnapshot = Zenith_SceneRegistry::GetActiveScene();
		pxJob->m_iSingleModeOldActiveHandle = xOldActiveSnapshot.m_iHandle;
		pxJob->m_uSingleModeOldActiveGeneration = xOldActiveSnapshot.m_uGeneration;

		ActiveSceneChangeSuppressionScope xSuppress(xOldActiveSnapshot);

		Zenith_SceneManager::ResetAllRenderSystems();
		// CancelAllPendingAsyncLoads returns pxJob's post-cancel index. Sync the
		// caller's loop index so subsequent CleanupAndRemoveAsyncJob(uIndex) calls
		// hit the correct slot, without assuming the surviving job is at index 0.
		const u_int uPostCancelIndex = CancelAllPendingAsyncLoads(pxJob);
		Zenith_Assert(uPostCancelIndex != UINT32_MAX,
			"RunAsyncJobPhase1: surviving SINGLE-mode job vanished from g_xEngine.SceneOperations().m_axAsyncJobs during cancel");
		uIndex = uPostCancelIndex;
		Zenith_SceneManager::UnloadAllNonPersistent();
		// B3: Unity-parity. Mirror the sync SINGLE teardown — auto-fire
		// UnloadUnusedAssets after the old non-persistent scenes are torn down,
		// before Physics::Reset (matching PerformSingleModeTeardownAndSwap order).
		// Phase 1 already validated the new file's header by this point, so we
		// won't reach here on a corrupt SINGLE load.
		Zenith_SceneManager::UnloadUnusedAssets();
		Zenith_Physics::Reset();
		Zenith_SceneLifecycleScheduler::s_fFixedTimeAccumulator = 0.0f;  // Unity behavior: reset on scene load

		// Cancel the scope: Phase 2 fires the consolidated ActiveSceneChanged
		// using pxJob's stored snapshot, possibly across frames if activation
		// pauses. We do not want the scope to fire its own callback here.
		xSuppress.Cancel();
	}

	// Create the new scene and deserialize.
	const std::string& strCanonicalPath = pxJob->m_strCanonicalPath;
	const std::string strName = ExtractSceneNameFromPath(strCanonicalPath);

	pxOp->SetProgress(fPROGRESS_SCENE_CREATED);

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene(strName);
	{
		// B1: route GetDefaultCreationScene-aware APIs at the loading scene
		// during deserialization. Phase 2 opens its own scope when activation
		// resumes (the two scopes don't overlap because Phase 1 ends before
		// Phase 2 begins, possibly across frames).
		Zenith_SceneManager::SceneCreationTargetScope xCreationTargetScope(xScene);

		Zenith_SceneData* pxSceneData = Zenith_SceneRegistry::GetSceneData(xScene);
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
			// Must use Zenith_SceneManager::UnloadSceneForced: in SCENE_LOAD_SINGLE the just-created scene
			// may be the only non-persistent scene after teardown, and UnloadScene would
			// be rejected by CanUnloadScene's last-scene guard, leaking a ghost slot.
			Zenith_Error(LOG_CATEGORY_SCENE, "LoadSceneAsync: Failed to deserialize '%s'", pxJob->m_strPath.c_str());
			Zenith_SceneManager::UnloadSceneForced(xScene);
			FailAsyncLoadOperation(pxOp);
			Zenith_SceneLifecycleScheduler::s_bIsLoadingScene = false;
			CleanupAndRemoveAsyncJob(uIndex);
			return AsyncJobStepResult::Removed;
		}

		pxOp->SetProgress(fPROGRESS_DESERIALIZE_COMPLETE);
		pxOp->SetResultScene(xScene.m_iHandle, xScene.m_uGeneration);

		// Scene deserialized but dormant; transition to Phase 2.
		pxJob->m_iCreatedSceneHandle = xScene.m_iHandle;
		pxJob->m_uCreatedSceneGeneration = xScene.m_uGeneration;
		pxJob->m_ePhase = AsyncLoadJob::LoadPhase::DESERIALIZED;
		Zenith_SceneLifecycleScheduler::s_bIsLoadingScene = false;
	}

	return AsyncJobStepResult::FallThrough;
}

// Phase 2: activation (Awake + OnEnable + scene-loaded callbacks). Runs only when the
// operation has been granted activation (`IsActivationAllowed`). Either finishes the
// job (Removed), or waits for activation (Waiting).
Zenith_SceneOperationQueue::AsyncJobStepResult
Zenith_SceneOperationQueue::RunAsyncJobPhase2(AsyncLoadJob* pxJob, Zenith_SceneOperation* pxOp, u_int uIndex)
{
	if (!pxOp->IsActivationAllowed())
	{
		pxOp->SetProgress(fPROGRESS_ACTIVATION_PAUSED);
		return AsyncJobStepResult::Waiting;
	}

	Zenith_SceneLifecycleScheduler::s_bIsLoadingScene = true;

	// A5: use the generation captured at Phase 1 end, not the current slot generation.
	// If the slot was recycled between deserialization and activation, Zenith_SceneRegistry::GetSceneData
	// returns nullptr and we abort cleanly instead of activating the wrong scene.
	Zenith_Scene xScene;
	xScene.m_iHandle = pxJob->m_iCreatedSceneHandle;
	xScene.m_uGeneration = pxJob->m_uCreatedSceneGeneration;
	Zenith_SceneData* pxSceneData = Zenith_SceneRegistry::GetSceneData(xScene);

	if (!pxSceneData)
	{
		// Slot was recycled underneath us — the job's created scene was unloaded
		// and replaced. Fail the op so callers can detect the abort; do not touch
		// g_xEngine.SceneRegistry().m_iActiveSceneHandle or call lifecycle dispatch.
		Zenith_Warning(LOG_CATEGORY_SCENE,
			"ProcessPendingAsyncLoads: Phase 2 aborted — scene slot %d was recycled "
			"(stored gen=%u, current gen=%u).",
			pxJob->m_iCreatedSceneHandle,
			pxJob->m_uCreatedSceneGeneration,
			pxJob->m_iCreatedSceneHandle < static_cast<int>(g_xEngine.SceneRegistry().m_axSceneGenerations.GetSize())
				? g_xEngine.SceneRegistry().m_axSceneGenerations.Get(pxJob->m_iCreatedSceneHandle) : 0);
		Zenith_SceneLifecycleScheduler::s_bIsLoadingScene = false;
		FailAsyncLoadOperation(pxOp);
		CleanupAndRemoveAsyncJob(uIndex);
		return AsyncJobStepResult::Removed;
	}

	{
		// B1: route GetDefaultCreationScene-aware APIs at the loading scene
		// during activation. Awake/OnEnable handlers and SceneLoaded subscribers
		// that spawn entities should target the new scene rather than whatever
		// was active before the swap.
		Zenith_SceneManager::SceneCreationTargetScope xCreationTargetScope(xScene);

		// Set active scene for SINGLE mode.
		if (pxJob->m_eMode == SCENE_LOAD_SINGLE)
		{
			Zenith_Assert(!Zenith_SceneManager::AreRenderTasksActive(), "Cannot change active scene while render tasks are in flight");
			g_xEngine.SceneRegistry().m_iActiveSceneHandle = xScene.m_iHandle;

			// A5: reconstruct the pre-teardown active scene from the snapshot stored on
			// the job so subscribers see a single consolidated old → new transition.
			Zenith_Scene xOldActive;
			xOldActive.m_iHandle = pxJob->m_iSingleModeOldActiveHandle;
			xOldActive.m_uGeneration = pxJob->m_uSingleModeOldActiveGeneration;
			if (xOldActive != xScene)
			{
				Zenith_SceneCallbackBus::FireActiveSceneChanged(xOldActive, xScene);
			}
		}

		// Unity behavior: Awake -> OnEnable -> sceneLoaded -> Start(next frame)
		pxSceneData->DispatchAwakeForNewScene();
		pxSceneData->DispatchEnableAndPendingStartsForNewScene();
		pxSceneData->m_bIsActivated = true;

		// D.13 (finding 3.12): clear the loading-scene flag BEFORE SceneLoaded callback
		// dispatch so subscribers see IsLoadingScene() == false. Mirrors the sync path.
		// CleanupAndRemoveAsyncJob below will also clear Zenith_SceneLifecycleScheduler::s_axCurrentlyLoadingPaths; the
		// circular-load guard stays active until the per-path bookkeeping is released.
		Zenith_SceneLifecycleScheduler::s_bIsLoadingScene = false;

		Zenith_SceneCallbackBus::FireSceneLoaded(xScene, pxJob->m_eMode);
	}

	pxOp->SetProgress(1.0f);
	pxOp->SetComplete(true);
	pxOp->FireCompletionCallback();

	CleanupAndRemoveAsyncJob(uIndex);
	return AsyncJobStepResult::Removed;
}

void Zenith_SceneOperationQueue::ProcessPendingAsyncUnloads()
{
	// B4.B P1: bump the unload re-entrancy depth so a SceneUnloading or
	// SceneUnloaded handler that calls a blocking LoadScene does NOT cause
	// the inner CompletePriorOperationsForBlockingLoad to re-enter this
	// pass. Re-entry is genuinely unsafe for unloads:
	//   * SceneUnloading branch: the firing job is still in g_xEngine.SceneOperations().m_axAsyncUnloadJobs
	//     with m_bUnloadingCallbackFired=true (set just below before the
	//     fire), so an inner pass advances entity destruction on a job the
	//     outer pass is holding a pointer to.
	//   * SceneUnloaded branch: the scene data is already deleted but the
	//     job is still in the vector. An inner pass would take the
	//     "scene already gone" branch, delete the job, and remove it —
	//     leaving the outer pass's pxJob pointer dangling
	//     (use-after-free + double-delete on outer's `delete pxJob`).
	struct ProcessingUnloadDepthGuard
	{
		ProcessingUnloadDepthGuard()  { ++g_xEngine.SceneOperations().m_uProcessingAsyncUnloadsDepth; }
		~ProcessingUnloadDepthGuard() { --g_xEngine.SceneOperations().m_uProcessingAsyncUnloadsDepth; }
	} xDepthGuard;

	// B2: Unity AsyncOperation queue-stall — async unloads queued behind a
	// paused-at-0.9 load head must not advance. The unload list is independent
	// of the load list, but the documented Unity contract is that the entire
	// AsyncOperation queue stalls behind the paused head, regardless of op type.
	if (IsAsyncQueueBlockedByActivationPausedHead())
		return;

	// Process pending async unloads - iterate in reverse for safe removal
	for (int i = static_cast<int>(g_xEngine.SceneOperations().m_axAsyncUnloadJobs.GetSize()) - 1; i >= 0; --i)
	{
		AsyncUnloadJob* pxJob = g_xEngine.SceneOperations().m_axAsyncUnloadJobs.Get(static_cast<u_int>(i));
		Zenith_SceneOperation* pxOp = pxJob->m_pxOperation;

		// Build scene handle for validation
		Zenith_Scene xScene;
		xScene.m_iHandle = pxJob->m_iSceneHandle;
		xScene.m_uGeneration = pxJob->m_uSceneGeneration;

		Zenith_SceneData* pxSceneData = Zenith_SceneRegistry::GetSceneData(xScene);
		if (!pxSceneData)
		{
			// Scene already gone - complete the operation
			pxOp->SetProgress(1.0f);
			pxOp->SetComplete(true);
			pxOp->FireCompletionCallback();
			delete pxJob;
			g_xEngine.SceneOperations().m_axAsyncUnloadJobs.Remove(static_cast<u_int>(i));
			continue;
		}

		// Fire unloading callback once (before any destruction)
		if (!pxJob->m_bUnloadingCallbackFired)
		{
			// B4.B P1: flip the "fired" flag BEFORE the dispatch, not after.
			// The callback runs on the main thread synchronously; if it
			// somehow re-enters this pass (defence in depth — the unload
			// depth guard above already prevents the blocking-load path),
			// the second pass over the same job sees the flag set and
			// skips the duplicate fire instead of dispatching again.
			pxJob->m_bUnloadingCallbackFired = true;
			Zenith_SceneCallbackBus::FireSceneUnloading(xScene);

			// If we're unloading the active scene, swap the pointer NOW so observers
			// never see a dying scene as active, but DEFER the ActiveSceneChanged
			// callback fire until after SceneUnloaded (MEDIUM-1: match sync-unload
			// ordering [Unloading, Unloaded, ActiveChanged] instead of the old
			// [Unloading, ActiveChanged, Unloaded]).
			if (xScene.m_iHandle == g_xEngine.SceneRegistry().m_iActiveSceneHandle)
			{
				Zenith_Assert(!Zenith_SceneManager::AreRenderTasksActive(), "Cannot change active scene while render tasks are in flight");
				g_xEngine.SceneRegistry().m_iActiveSceneHandle = Zenith_SceneRegistry::SelectNewActiveScene(xScene.m_iHandle);
				Zenith_Scene xNewActive = Zenith_SceneRegistry::GetActiveScene();
				pxJob->m_iOldActiveHandle = xScene.m_iHandle;
				pxJob->m_uOldActiveGeneration = xScene.m_uGeneration;
				pxJob->m_iNewActiveHandle = xNewActive.m_iHandle;
				pxJob->m_uNewActiveGeneration = xNewActive.m_uGeneration;
				pxJob->m_bActiveSceneChangePending = true;
			}
		}

		// Destroy a batch of entities this frame
		const Zenith_Vector<Zenith_EntityID>& axEntities = pxSceneData->GetActiveEntities();
		uint32_t uEntitiesThisFrame = 0;

		// Destroy from the end to avoid index shifting issues
		// RemoveEntity dispatches OnDisable/OnDestroy internally for each entity
		// and recursively for children, ensuring no entity is missed
		while (axEntities.GetSize() > 0 && uEntitiesThisFrame < g_xEngine.SceneOperations().m_uAsyncUnloadBatchSize)
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

			// Audit §3.5 fix: log a warning when a single RemoveEntity cascade
			// destroys significantly more than the batch size — the frame-budget
			// contract is a soft cap ("at least one subtree per frame") and a
			// cascade that destroys >2x the batch tells QA they should retune
			// g_xEngine.SceneOperations().m_uAsyncUnloadBatchSize or flatten the hierarchy.
			if (uActualRemoved > 2 * g_xEngine.SceneOperations().m_uAsyncUnloadBatchSize)
			{
				Zenith_Warning(LOG_CATEGORY_SCENE,
					"Async unload: single RemoveEntity destroyed %u entities, exceeding 2x "
					"batch size (%u). Frame-budget cap is a soft ceiling; consider raising "
					"SetAsyncUnloadBatchSize() or flattening the hierarchy under the subtree root.",
					uActualRemoved, g_xEngine.SceneOperations().m_uAsyncUnloadBatchSize);
			}
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
			g_xEngine.SceneRegistry().m_axScenes.Get(pxJob->m_iSceneHandle) = nullptr;

			// Fire unloaded callback BEFORE incrementing generation so the handle
			// is still valid for identification in callbacks (Unity parity)
			Zenith_SceneCallbackBus::FireSceneUnloaded(xScene);

			Zenith_SceneRegistry::FreeSceneHandle(pxJob->m_iSceneHandle);

			// MEDIUM-1: fire the deferred ActiveSceneChanged now, AFTER
			// SceneUnloaded. Preserves the sync-unload ordering
			// [Unloading, Unloaded, ActiveChanged] for async unloads too.
			if (pxJob->m_bActiveSceneChangePending)
			{
				Zenith_Scene xOld;
				xOld.m_iHandle = pxJob->m_iOldActiveHandle;
				xOld.m_uGeneration = pxJob->m_uOldActiveGeneration;
				Zenith_Scene xNew;
				xNew.m_iHandle = pxJob->m_iNewActiveHandle;
				xNew.m_uGeneration = pxJob->m_uNewActiveGeneration;
				Zenith_SceneCallbackBus::FireActiveSceneChanged(xOld, xNew);
				pxJob->m_bActiveSceneChangePending = false;
			}

			// Complete operation
			pxOp->SetProgress(1.0f);
			pxOp->SetComplete(true);
			pxOp->FireCompletionCallback();

			delete pxJob;
			g_xEngine.SceneOperations().m_axAsyncUnloadJobs.Remove(static_cast<u_int>(i));
		}
	}
}

void Zenith_SceneOperationQueue::SetAsyncUnloadBatchSize(uint32_t uEntitiesPerFrame)
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

	g_xEngine.SceneOperations().m_uAsyncUnloadBatchSize = uEntitiesPerFrame;
}

uint32_t Zenith_SceneOperationQueue::GetAsyncUnloadBatchSize()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetAsyncUnloadBatchSize must be called from main thread");
	return g_xEngine.SceneOperations().m_uAsyncUnloadBatchSize;
}

void Zenith_SceneOperationQueue::SetMaxConcurrentAsyncLoads(uint32_t uMax)
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "SetMaxConcurrentAsyncLoads must be called from main thread");
	g_xEngine.SceneOperations().m_uMaxConcurrentAsyncLoads = (uMax > 0) ? uMax : 1;
}

uint32_t Zenith_SceneOperationQueue::GetMaxConcurrentAsyncLoads()
{
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "GetMaxConcurrentAsyncLoads must be called from main thread");
	return g_xEngine.SceneOperations().m_uMaxConcurrentAsyncLoads;
}
