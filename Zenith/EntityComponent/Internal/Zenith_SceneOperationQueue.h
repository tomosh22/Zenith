#pragma once

// Zenith_SceneOperationQueue — owns the async scene-operation pipeline:
//   * Operation map (ID -> Zenith_SceneOperation*) with delayed cleanup.
//   * Async load job queue (file I/O on workers, scene creation/activation
//     on the main thread).
//   * Async unload job queue (entity destruction spread across frames).
//   * Phase machines (RunAsyncJobPhase1/2, ProcessPendingAsyncLoads/Unloads).
//   * Async config knobs (batch size, concurrent-load warning threshold).
//
// Public Zenith_SceneManager async APIs (LoadSceneAsync*, UnloadSceneAsync,
// GetOperation, IsOperationValid, async-config setters/getters) forward into
// this queue. Manager-internal lifecycle code (sync LoadScene's auto-promote,
// SCENE_LOAD_SINGLE staging swap, Shutdown teardown) calls the queue's Cancel*,
// Cleanup*, Run* methods directly.
//
// This header is independent of Zenith_SceneManager.h to avoid the header-cycle
// pitfall that bit A2b: SceneData.h's `friend class Zenith_SceneOperationQueue`
// (added below) would otherwise inject an incomplete forward declaration that
// shadows the real class.

#include "EntityComponent/Zenith_Scene.h"  // for Zenith_SceneOperationID alias
#include <atomic>
#include <cstdint>
#include <string>

class Zenith_SceneOperation;
class Zenith_SceneData;
class Zenith_DataStream;
class Zenith_Task;
enum Zenith_SceneLoadMode : uint8_t;
using Zenith_SceneOperationID = uint64_t;

class Zenith_SceneOperationQueue
{
public:
	//==========================================================================
	// Nested types (migrated from Zenith_SceneManager)
	//==========================================================================

	struct OperationMapEntry
	{
		uint64_t m_ulOperationID = 0;
		Zenith_SceneOperation* m_pxOperation = nullptr;
	};

	// File load milestones (used instead of atomic float for better ordering guarantees)
	enum class FileLoadMilestone : uint8_t
	{
		IDLE = 0,
		FILE_READ_STARTED = 10,     // Maps to 0.1 progress
		FILE_READ_COMPLETE = 70     // Maps to 0.7 progress
	};

	// Async loading job — file I/O on worker thread, scene creation on main thread.
	struct AsyncLoadJob
	{
		enum class LoadPhase : uint8_t
		{
			WAITING_FOR_FILE,
			DESERIALIZED,
		};

		std::string m_strPath;
		std::string m_strCanonicalPath;
		Zenith_SceneLoadMode m_eMode;
		int m_iBuildIndex;
		Zenith_SceneOperation* m_pxOperation;
		std::atomic<bool> m_bFileLoadComplete;
		std::atomic<FileLoadMilestone> m_eMilestone;
		Zenith_DataStream* m_pxLoadedData;
		Zenith_Task* m_pxTask;
		LoadPhase m_ePhase;
		int m_iCreatedSceneHandle;
		uint32_t m_uCreatedSceneGeneration;
		int m_iSingleModeOldActiveHandle;
		uint32_t m_uSingleModeOldActiveGeneration;

		AsyncLoadJob()
			: m_iBuildIndex(-1), m_pxOperation(nullptr), m_bFileLoadComplete(false)
			, m_eMilestone(FileLoadMilestone::IDLE), m_pxLoadedData(nullptr), m_pxTask(nullptr)
			, m_ePhase(LoadPhase::WAITING_FOR_FILE), m_iCreatedSceneHandle(-1)
			, m_uCreatedSceneGeneration(0), m_iSingleModeOldActiveHandle(-1)
			, m_uSingleModeOldActiveGeneration(0) {}
		~AsyncLoadJob();
	};

	enum class AsyncJobStepResult
	{
		Removed,     // job removed this iteration; do NOT advance index
		Waiting,     // job still pending; advance to next job
		FallThrough, // step complete; try the next phase on the SAME job this iteration
	};

	// Async unloading job — destruction spread across frames.
	struct AsyncUnloadJob
	{
		int m_iSceneHandle;
		uint32_t m_uSceneGeneration;
		Zenith_SceneOperation* m_pxOperation;
		uint32_t m_uTotalEntities;
		uint32_t m_uDestroyedEntities;
		bool m_bUnloadingCallbackFired;

		// MEDIUM-1: when we unload the active scene, the active-pointer swap
		// happens early but the ActiveSceneChanged callback fire is deferred
		// until after SceneUnloaded.
		bool m_bActiveSceneChangePending;
		int m_iOldActiveHandle;
		uint32_t m_uOldActiveGeneration;
		int m_iNewActiveHandle;
		uint32_t m_uNewActiveGeneration;

		AsyncUnloadJob()
			: m_iSceneHandle(-1), m_uSceneGeneration(0), m_pxOperation(nullptr)
			, m_uTotalEntities(0), m_uDestroyedEntities(0), m_bUnloadingCallbackFired(false)
			, m_bActiveSceneChangePending(false)
			, m_iOldActiveHandle(-1), m_uOldActiveGeneration(0)
			, m_iNewActiveHandle(-1), m_uNewActiveGeneration(0) {}
		~AsyncUnloadJob();
	};

	//==========================================================================
	// Storage type declarations only — actual state lives on
	// Zenith_SceneOperationQueueImpl owned by Zenith_Engine (Phase 5d).
	// External readers reach it through g_xEngine.SceneOperations().m_xXxx.
	//==========================================================================

	//==========================================================================
	// Lifecycle
	//==========================================================================

	// Tear down all in-flight load tasks (waits for workers), all unload jobs,
	// the operation map, and active-operation list. Called from Zenith_SceneManager::Shutdown.
	static void Shutdown();

	// Same teardown but without waiting for ongoing operations to complete normally —
	// used by Zenith_SceneManager::ResetForNextTest between unit tests.
	static void ResetForNextTest();

	//==========================================================================
	// Operation ID allocation / lookup / cleanup
	//==========================================================================

	static Zenith_SceneOperationID AllocateOperationID();
	static Zenith_SceneOperation* GetOperation(Zenith_SceneOperationID ulID);
	static bool IsOperationValid(Zenith_SceneOperationID ulID);
	static void CleanupCompletedOperations();

	//==========================================================================
	// Async load pipeline (internal — public LoadSceneAsync* lives on
	// Zenith_SceneManager and uses these helpers + state).
	//==========================================================================

	static void AsyncSceneLoadTask(void* pData);
	static u_int CancelAllPendingAsyncLoads(AsyncLoadJob* pxExclude = nullptr);
	static void ProcessPendingAsyncLoads();
	static void FailAsyncLoadOperation(Zenith_SceneOperation* pxOp);
	static void CleanupAndRemoveAsyncJob(u_int uIndex);
	static void SortAsyncJobsByPriority();
	static bool HandleAsyncJobCancellation(AsyncLoadJob* pxJob, Zenith_SceneOperation* pxOp, u_int uIndex);

	// B2: Unity AsyncOperation queue-stall predicate.
	//
	// Returns true when the priority-sorted async-load head has reached the
	// activation-paused state and is waiting on SetActivationAllowed(true).
	// While true, behind-the-head load jobs and ALL async-unload jobs must
	// not advance — matching Unity's documented contract that the
	// AsyncOperation queue stalls behind a paused-at-0.9 head.
	//
	// "Blocked" means all of:
	//   * Head is not cancelled, complete, or already permitted to activate.
	//   * Head has actually reached the gated state — either the Phase-1
	//     SINGLE gate (file complete, pre-teardown) or the Phase-2 gate
	//     (post-deserialize, pre-activation).
	//
	// Self-contained: re-runs SortAsyncJobsByPriority() so callers don't
	// need to pre-sort.
	static bool IsAsyncQueueBlockedByActivationPausedHead();

	// B4.B P2: synchronously wait for every in-flight async-load worker
	// task to finish reading its file (i.e., flip m_bFileLoadComplete).
	// The blocking-load entry points call this so their bounded
	// ProcessPendingAsyncLoads / Update pump never spins waiting on worker
	// progress — the wait is explicit, the iteration cap is a deadlock
	// sentinel only.
	//
	// Skipped under re-entrancy (any of the depth counters non-zero, or
	// IsUpdating()) because blocking the main thread on worker IO from
	// inside a callback is exactly what those guards exist to prevent.
	static void WaitForPendingFileReadsForBlockingPump();
	static AsyncJobStepResult RunAsyncJobPhase1(AsyncLoadJob* pxJob, Zenith_SceneOperation* pxOp, u_int& uIndex);
	static AsyncJobStepResult RunAsyncJobPhase2(AsyncLoadJob* pxJob, Zenith_SceneOperation* pxOp, u_int uIndex);

	// Inline forwarder. Body kept here for hot-path callers; it reaches
	// into the engine-owned Impl (Zenith.h's PCH brings g_xEngine in scope).
	static void NotifyAsyncJobPriorityChanged();

	//==========================================================================
	// Async unload pipeline (internal — public UnloadSceneAsync lives on
	// Zenith_SceneManager).
	//==========================================================================

	static void ProcessPendingAsyncUnloads();
	static uint32_t CountScenesBeingAsyncUnloaded();

	// B4: Unity LoadScene flush-prior-async semantic.
	//
	// Drives all in-flight async load and unload operations to completion
	// before the caller's own blocking load proceeds. Mirrors Unity's
	// documented behaviour that LoadScene forces all prior AsyncOperations
	// to complete first.
	//
	// Rules owned here (so the manager facade doesn't poke job vectors):
	//   * Activation-paused loads are released — for queue-drain purposes
	//     they're treated as "complete now"; their callers' explicit
	//     SetActivationAllowed(false) loses to the explicit blocking load.
	//   * Cancelled ops are left to the normal cancellation processing in
	//     ProcessPendingAsyncLoads — no special-casing.
	//   * Failed ops surface as failures via their op state — the caller's
	//     own LoadScene continues regardless.
	//   * Waits on worker-side file I/O via the existing milestone atomics
	//     (no extra synchronization).
	static void CompletePriorOperationsForBlockingLoad();

	//==========================================================================
	// Async configuration
	//==========================================================================

	static void SetAsyncUnloadBatchSize(uint32_t uEntitiesPerFrame);
	static uint32_t GetAsyncUnloadBatchSize();
	static void SetMaxConcurrentAsyncLoads(uint32_t uMax);
	static uint32_t GetMaxConcurrentAsyncLoads();
};
