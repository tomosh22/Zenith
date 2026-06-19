#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "TaskSystem/Zenith_TaskSystem.h"

#include "Core/Zenith_ErrorCode.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include <thread>

DEBUGVAR bool dbg_bMultithreaded = true;

namespace
{
	void ThreadFunc(const void*)
	{
		g_xEngine.Tasks().RunWorkerLoop();
	}
}

void Zenith_TaskSystem::RunWorkerLoop()
{
	do
	{
		m_pxWorkAvailableSem->Wait();

		if (m_bTerminateThreads.load(std::memory_order_acquire)) break;

		Zenith_Task* pxTask = nullptr;
		m_xQueueMutex.Lock();
		bool bDequeued = m_xTaskQueue.Dequeue(pxTask);
		m_xQueueMutex.Unlock();

		Zenith_Assert(bDequeued && pxTask != nullptr,
			"ThreadFunc: Semaphore signaled but dequeue failed - synchronization bug");

		if (!pxTask) continue;  // Safety fallback for release builds

		pxTask->DoTask();

	} while (!m_bTerminateThreads.load(std::memory_order_acquire));

	// Thread-exit unregister: free this worker's profiling ring + clear its TLS
	// before signalling termination, so Profiling::Shutdown (which runs after the
	// main thread observes all workers terminated) only has the main + any
	// non-joining producer (e.g. FileWatcher) left in the table. Routed through the
	// _Detail bridge so this TU stays off the engine-singleton ratchet.
	Zenith_Profiling_Detail::UnregisterThread();

	m_pxThreadsTerminatedSem->Signal();
}

void Zenith_TaskSystem::Initialise()
{
	Zenith_Assert(!m_bInitialized.load(std::memory_order_acquire),
		"Zenith_TaskSystem::Initialise: Already initialized - call Shutdown first");

	// Hardware thread count minus 1 (the main thread is reserved), clamped to [1, 16]
	constexpr u_int uMAX_TASK_THREADS = 16;
	const u_int uHardwareThreads = std::thread::hardware_concurrency();
	u_int uNumThreads = (uHardwareThreads > 1) ? (uHardwareThreads - 1) : 1;
	if (uNumThreads > uMAX_TASK_THREADS) uNumThreads = uMAX_TASK_THREADS;

	m_uNumWorkerThreads = uNumThreads;

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
		return;
	}

	Zenith_Log(LOG_CATEGORY_TASKSYSTEM, "Shutting down task system...");

	m_bTerminateThreads.store(true, std::memory_order_release);

	// Fence ensures the terminate flag is visible before workers are woken
	std::atomic_thread_fence(std::memory_order_seq_cst);

	Zenith_Assert(m_pxWorkAvailableSem != nullptr, "Shutdown: Semaphore is null");
	for (u_int u = 0; u < m_uNumWorkerThreads; u++)
	{
		m_pxWorkAvailableSem->Signal();
	}

	Zenith_Assert(m_pxThreadsTerminatedSem != nullptr, "Shutdown: Termination semaphore is null");
	for (u_int u = 0; u < m_uNumWorkerThreads; u++)
	{
		m_pxThreadsTerminatedSem->Wait();
	}

	{
		Zenith_ScopedMutexLock xLock(m_xQueueMutex);
		Zenith_Assert(m_xTaskQueue.IsEmpty(),
			"Shutdown: Task queue not empty - %u tasks will be dropped!", m_xTaskQueue.GetSize());
	}

	delete m_pxWorkAvailableSem;
	delete m_pxThreadsTerminatedSem;
	m_pxWorkAvailableSem = nullptr;
	m_pxThreadsTerminatedSem = nullptr;
	m_uNumWorkerThreads = 0;
	m_bTerminateThreads.store(false, std::memory_order_release);
	m_bInitialized.store(false, std::memory_order_release);

	Zenith_Log(LOG_CATEGORY_TASKSYSTEM, "Task system shutdown complete");
}

bool Zenith_TaskSystem::TryClaimTask(Zenith_Task* pxTask, const char* szCallerName)
{
	Zenith_Assert(pxTask != nullptr, "%s: Task is null", szCallerName);
	Zenith_Assert(m_bInitialized.load(std::memory_order_acquire), "%s: TaskSystem not initialized", szCallerName);
	Zenith_Assert(m_pxWorkAvailableSem != nullptr, "%s: Semaphore is null", szCallerName);

	if (!pxTask->TryMarkSubmitted())
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
			if (!m_xTaskQueue.Enqueue(pxTask))
			{
				break;
			}
			uEnqueuedCount++;
		}
	}

	// Overflow is recoverable back-pressure, not a crash: log QUEUE_FULL and
	// let the caller absorb the shortfall (RecoverFromShortEnqueue /
	// RunInvocationsOnCallingThread).
	Zenith_Check(uEnqueuedCount == uCount,
		"EnqueueAndSignal: Only enqueued %u/%u tasks - queue full (Zenith_ErrorCode::QUEUE_FULL)!", uEnqueuedCount, uCount);

	for (u_int u = 0; u < uEnqueuedCount; u++)
	{
		m_pxWorkAvailableSem->Signal();
	}

	return uEnqueuedCount;
}

// A task the queue refused was never handed to a worker: recycle it so the
// caller can resubmit and its WaitUntilComplete() is a no-op.
void Zenith_TaskSystem::RecoverFromShortEnqueue(Zenith_Task* pxTask)
{
	pxTask->MarkRecycled();
}

// The completion semaphore fires only after EXACTLY GetNumInvocations() DoTask()
// calls, so every invocation that did not reach a worker (the calling thread's
// participation slot, plus any a full queue dropped) must run here or
// WaitUntilComplete() blocks forever.
void Zenith_TaskSystem::RunInvocationsOnCallingThread(Zenith_DataParallelTask* pxTask, u_int uCount)
{
	for (u_int u = 0; u < uCount; u++)
	{
		pxTask->DoTask();
	}
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

	if (EnqueueAndSignal(pxTask, 1) == 0)
	{
		RecoverFromShortEnqueue(pxTask);
	}
}

void Zenith_TaskSystem::SubmitDataParallelTask(Zenith_DataParallelTask* pxTask)
{
	if (!TryClaimTask(pxTask, "SubmitDataParallelTask"))
	{
		return;
	}

	// Safe to reset only while claimed: the submitted flag serializes resubmission
	pxTask->ResetCounters();

	const u_int uNumInvocations = pxTask->GetNumInvocations();

	if (!dbg_bMultithreaded)
	{
		RunInvocationsOnCallingThread(pxTask, uNumInvocations);
		return;
	}

	const u_int uTasksForWorkers = pxTask->GetCallingThreadParticipates()
		? (uNumInvocations > 0 ? uNumInvocations - 1 : 0)
		: uNumInvocations;

	const u_int uEnqueuedForWorkers = EnqueueAndSignal(pxTask, uTasksForWorkers);
	RunInvocationsOnCallingThread(pxTask, uNumInvocations - uEnqueuedForWorkers);
}
