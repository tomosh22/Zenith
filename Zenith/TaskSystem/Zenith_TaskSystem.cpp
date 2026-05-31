#include "Zenith.h"

#include "TaskSystem/Zenith_TaskSystem.h"

#include "Core/Zenith_ErrorCode.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include <thread>

DEBUGVAR bool dbg_bMultithreaded = true;

namespace
{
	void ThreadFunc(const void*)
	{
		// All worker threads share the single Zenith_TaskSystem
		// instance held by g_xEngine. Cache the reference once.
		g_xEngine.Tasks().RunWorkerLoop();
	}
}

void Zenith_TaskSystem::RunWorkerLoop()
{
	do
	{
		m_pxWorkAvailableSem->Wait();

		// Use atomic load with acquire ordering to ensure visibility of terminate flag
		if (m_bTerminateThreads.load(std::memory_order_acquire)) break;

		Zenith_Task* pxTask = nullptr;
		m_xQueueMutex.Lock();
		bool bDequeued = m_xTaskQueue.Dequeue(pxTask);
		m_xQueueMutex.Unlock();

		// Semaphore was signaled, so there should be work available
		// If dequeue fails, it indicates a synchronization bug
		Zenith_Assert(bDequeued && pxTask != nullptr,
			"ThreadFunc: Semaphore signaled but dequeue failed - synchronization bug");

		if (!pxTask) continue;  // Safety fallback for release builds

		pxTask->DoTask();

	} while (!m_bTerminateThreads.load(std::memory_order_acquire));

	m_pxThreadsTerminatedSem->Signal();
}

void Zenith_TaskSystem::Initialise()
{
	Zenith_Assert(!m_bInitialized.load(std::memory_order_acquire),
		"Zenith_TaskSystem::Initialise: Already initialized - call Shutdown first");

	// Use hardware thread count minus 1 (reserve main thread)
	// Minimum of 1 worker thread, maximum of 16
	constexpr u_int uMAX_TASK_THREADS = 16;
	const u_int uHardwareThreads = std::thread::hardware_concurrency();
	u_int uNumThreads = (uHardwareThreads > 1) ? (uHardwareThreads - 1) : 1;
	if (uNumThreads > uMAX_TASK_THREADS) uNumThreads = uMAX_TASK_THREADS;

	m_uNumWorkerThreads = uNumThreads;  // Store for Shutdown

	Zenith_Log(LOG_CATEGORY_TASKSYSTEM, "Creating %u worker threads (hardware reports %u threads)", uNumThreads, uHardwareThreads);

	m_pxWorkAvailableSem = new Zenith_Semaphore(0, uMAX_TASKS);
	m_pxThreadsTerminatedSem = new Zenith_Semaphore(0, uNumThreads);

	for (u_int u = 0; u < uNumThreads; u++)
	{
		char acName[Zenith_Multithreading::uMAX_THREAD_NAME_LENGTH];
		snprintf(acName, Zenith_Multithreading::uMAX_THREAD_NAME_LENGTH, "Zenith_TaskSystem %u", u);
		g_xEngine.Threading().CreateThread(acName, ThreadFunc, nullptr);
	}

#ifdef ZENITH_DEBUG_VARIABLES
	g_xEngine.DebugVariables().AddBoolean({ "Task System", "Multithreaded" }, dbg_bMultithreaded);
#endif

	m_bInitialized.store(true, std::memory_order_release);
}

void Zenith_TaskSystem::Shutdown()
{
	if (!m_bInitialized.load(std::memory_order_acquire))
	{
		return;  // Not initialized
	}

	Zenith_Log(LOG_CATEGORY_TASKSYSTEM, "Shutting down task system...");

	// Set terminate flag with release ordering - visible to all workers
	m_bTerminateThreads.store(true, std::memory_order_release);

	// Memory fence ensures flag is visible before signaling workers
	std::atomic_thread_fence(std::memory_order_seq_cst);

	// Wake up all waiting workers so they can check the terminate flag
	Zenith_Assert(m_pxWorkAvailableSem != nullptr, "Shutdown: Semaphore is null");
	for (u_int u = 0; u < m_uNumWorkerThreads; u++)
	{
		m_pxWorkAvailableSem->Signal();
	}

	// Wait for all workers to terminate
	Zenith_Assert(m_pxThreadsTerminatedSem != nullptr, "Shutdown: Termination semaphore is null");
	for (u_int u = 0; u < m_uNumWorkerThreads; u++)
	{
		m_pxThreadsTerminatedSem->Wait();
	}

	// Verify all tasks were processed
	{
		Zenith_ScopedMutexLock xLock(m_xQueueMutex);
		Zenith_Assert(m_xTaskQueue.IsEmpty(),
			"Shutdown: Task queue not empty - %u tasks will be dropped!", m_xTaskQueue.GetSize());
	}

	// Clean up resources
	delete m_pxWorkAvailableSem;
	delete m_pxThreadsTerminatedSem;
	m_pxWorkAvailableSem = nullptr;
	m_pxThreadsTerminatedSem = nullptr;
	m_uNumWorkerThreads = 0;
	m_bTerminateThreads.store(false, std::memory_order_release);  // Reset for potential re-initialization
	m_bInitialized.store(false, std::memory_order_release);

	Zenith_Log(LOG_CATEGORY_TASKSYSTEM, "Task system shutdown complete");
}

bool Zenith_TaskSystem::TryClaimTask(Zenith_Task* pxTask, const char* szCallerName)
{
	Zenith_Assert(pxTask != nullptr, "%s: Task is null", szCallerName);
	Zenith_Assert(m_bInitialized.load(std::memory_order_acquire), "%s: TaskSystem not initialized", szCallerName);
	Zenith_Assert(m_pxWorkAvailableSem != nullptr, "%s: Semaphore is null", szCallerName);

	// Atomic check-and-set for double-submit prevention (fixes TOCTOU race)
	bool bExpected = false;
	if (!pxTask->m_bSubmitted.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel))
	{
		Zenith_Assert(false, "%s: Task already submitted - call WaitUntilComplete before resubmitting", szCallerName);
		return false;
	}
	return true;
}

u_int Zenith_TaskSystem::EnqueueAndSignal(Zenith_Task* pxTask, u_int uCount)
{
	u_int uEnqueuedCount = 0;
	{
		Zenith_ScopedMutexLock xLock(m_xQueueMutex);
		for (u_int u = 0; u < uCount; u++)
		{
			if (m_xTaskQueue.Enqueue(pxTask))
			{
				uEnqueuedCount++;
			}
			else
			{
				break;  // Queue full
			}
		}
	}

	// Queue overflow is recoverable: SubmitTask resets m_bSubmitted on a short
	// enqueue so the task can be retried, and SubmitTaskArray simply runs fewer
	// worker invocations. A hard assert/break here would crash a shipping build
	// on transient back-pressure, so log it (Zenith_ErrorCode::QUEUE_FULL) and
	// fall through — signalling only the tasks we actually enqueued.
	Zenith_Check(uEnqueuedCount == uCount,
		"EnqueueAndSignal: Only enqueued %u/%u tasks - queue full (Zenith_ErrorCode::QUEUE_FULL)!", uEnqueuedCount, uCount);

	for (u_int u = 0; u < uEnqueuedCount; u++)
	{
		m_pxWorkAvailableSem->Signal();
	}

	return uEnqueuedCount;
}

void Zenith_TaskSystem::SubmitTask(Zenith_Task* pxTask)
{
	if (!TryClaimTask(pxTask, "SubmitTask"))
	{
		return;
	}

	if (!dbg_bMultithreaded)
	{
		pxTask->DoTask();
		return;
	}

	u_int uEnqueued = EnqueueAndSignal(pxTask, 1);
	if (uEnqueued == 0)
	{
		// Reset submitted flag if enqueue failed so task can be retried
		pxTask->m_bSubmitted.store(false, std::memory_order_release);
	}
}

void Zenith_TaskSystem::SubmitTaskArray(Zenith_TaskArray* pxTaskArray)
{
	if (!TryClaimTask(pxTaskArray, "SubmitTaskArray"))
	{
		return;
	}

	// Reset counters AFTER successfully claiming the submitted flag
	// This is now safe because no other thread can submit until WaitUntilComplete resets m_bSubmitted
	pxTaskArray->Reset();

	const u_int uNumInvocations = pxTaskArray->GetNumInvocations();

	if (!dbg_bMultithreaded)
	{
		for (u_int u = 0; u < uNumInvocations; u++)
		{
			pxTaskArray->DoTask();
		}
		return;
	}

	const bool bCallingThreadParticipates = pxTaskArray->GetCallingThreadParticipates();
	const u_int uTasksForWorkers = bCallingThreadParticipates
		? (uNumInvocations > 0 ? uNumInvocations - 1 : 0)
		: uNumInvocations;

	const u_int uEnqueuedForWorkers = EnqueueAndSignal(pxTaskArray, uTasksForWorkers);

	// The completion semaphore fires only when EXACTLY m_uNumInvocations DoTask()
	// calls finish (see Zenith_TaskArray::DoTask). EnqueueAndSignal can short-
	// enqueue on queue overflow (QUEUE_FULL is recoverable, not a crash), so the
	// calling thread must run EVERY invocation that did NOT reach a worker — its
	// own participation slot AND any the full queue dropped — or the completion
	// counter never reaches the target and WaitUntilComplete blocks forever on
	// transient back-pressure. (A single SubmitTask recovers via the m_bSubmitted
	// reset above; a TaskArray's fixed completion target cannot, so the caller
	// absorbs the shortfall here.)
	const u_int uCallerInvocations = uNumInvocations - uEnqueuedForWorkers;
	for (u_int u = 0; u < uCallerInvocations; u++)
	{
		pxTaskArray->DoTask();
	}
}
