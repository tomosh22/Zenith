#include "Zenith.h"

#include "TaskSystem/Zenith_TaskSystem.h"

#include "DebugVariables/Zenith_DebugVariables.h"
#include "Multithreading/Zenith_Multithreading.h"
#include <thread>
#include <atomic>

static constexpr u_int uMAX_TASKS = 128;

static Zenith_CircularQueue<Zenith_Task*, uMAX_TASKS> g_xTaskQueue;
static Zenith_Semaphore* g_pxWorkAvailableSem = nullptr;
static Zenith_Semaphore* g_pxThreadsTerminatedSem = nullptr;
static Zenith_Mutex g_xQueueMutex;
static std::atomic<bool> g_bTerminateThreads{false};
static std::atomic<bool> g_bInitialized{false};
static u_int g_uNumWorkerThreads = 0;

DEBUGVAR bool dbg_bMultithreaded = true;

static void ThreadFunc(const void*)
{
	do
	{
		g_pxWorkAvailableSem->Wait();

		// Use atomic load with acquire ordering to ensure visibility of terminate flag
		if (g_bTerminateThreads.load(std::memory_order_acquire)) break;

		Zenith_Task* pxTask = nullptr;
		g_xQueueMutex.Lock();
		bool bDequeued = g_xTaskQueue.Dequeue(pxTask);
		g_xQueueMutex.Unlock();

		// Semaphore was signaled, so there should be work available
		// If dequeue fails, it indicates a synchronization bug
		Zenith_Assert(bDequeued && pxTask != nullptr,
			"ThreadFunc: Semaphore signaled but dequeue failed - synchronization bug");

		if (!pxTask) continue;  // Safety fallback for release builds

		pxTask->DoTask();

	} while (!g_bTerminateThreads.load(std::memory_order_acquire));

	g_pxThreadsTerminatedSem->Signal();
}

void Zenith_TaskSystem::Inititalise()
{
	Zenith_Assert(!g_bInitialized.load(std::memory_order_acquire),
		"Zenith_TaskSystem::Inititalise: Already initialized - call Shutdown first");

	// Use hardware thread count minus 1 (reserve main thread)
	// Minimum of 1 worker thread, maximum of 16
	constexpr u_int uMAX_TASK_THREADS = 16;
	const u_int uHardwareThreads = std::thread::hardware_concurrency();
	u_int uNumThreads = (uHardwareThreads > 1) ? (uHardwareThreads - 1) : 1;
	if (uNumThreads > uMAX_TASK_THREADS) uNumThreads = uMAX_TASK_THREADS;

	g_uNumWorkerThreads = uNumThreads;  // Store for Shutdown

	Zenith_Log(LOG_CATEGORY_TASKSYSTEM, "Creating %u worker threads (hardware reports %u threads)", uNumThreads, uHardwareThreads);

	g_pxWorkAvailableSem = new Zenith_Semaphore(0, uMAX_TASKS);
	g_pxThreadsTerminatedSem = new Zenith_Semaphore(0, uNumThreads);

	for (u_int u = 0; u < uNumThreads; u++)
	{
		char acName[Zenith_Multithreading::uMAX_THREAD_NAME_LENGTH];
		snprintf(acName, Zenith_Multithreading::uMAX_THREAD_NAME_LENGTH, "Zenith_TaskSystem %u", u);
		Zenith_Multithreading::CreateThread(acName, ThreadFunc, nullptr);
	}

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Task System", "Multithreaded" }, dbg_bMultithreaded);
#endif

	g_bInitialized.store(true, std::memory_order_release);
}

void Zenith_TaskSystem::Shutdown()
{
	if (!g_bInitialized.load(std::memory_order_acquire))
	{
		return;  // Not initialized
	}

	Zenith_Log(LOG_CATEGORY_TASKSYSTEM, "Shutting down task system...");

	// Set terminate flag with release ordering - visible to all workers
	g_bTerminateThreads.store(true, std::memory_order_release);

	// Memory fence ensures flag is visible before signaling workers
	std::atomic_thread_fence(std::memory_order_seq_cst);

	// Wake up all waiting workers so they can check the terminate flag
	Zenith_Assert(g_pxWorkAvailableSem != nullptr, "Shutdown: Semaphore is null");
	for (u_int u = 0; u < g_uNumWorkerThreads; u++)
	{
		g_pxWorkAvailableSem->Signal();
	}

	// Wait for all workers to terminate
	Zenith_Assert(g_pxThreadsTerminatedSem != nullptr, "Shutdown: Termination semaphore is null");
	for (u_int u = 0; u < g_uNumWorkerThreads; u++)
	{
		g_pxThreadsTerminatedSem->Wait();
	}

	// Verify all tasks were processed
	{
		Zenith_ScopedMutexLock xLock(g_xQueueMutex);
		Zenith_Assert(g_xTaskQueue.IsEmpty(),
			"Shutdown: Task queue not empty - %u tasks will be dropped!", g_xTaskQueue.GetSize());
	}

	// Clean up resources
	delete g_pxWorkAvailableSem;
	delete g_pxThreadsTerminatedSem;
	g_pxWorkAvailableSem = nullptr;
	g_pxThreadsTerminatedSem = nullptr;
	g_uNumWorkerThreads = 0;
	g_bTerminateThreads.store(false, std::memory_order_release);  // Reset for potential re-initialization
	g_bInitialized.store(false, std::memory_order_release);

	Zenith_Log(LOG_CATEGORY_TASKSYSTEM, "Task system shutdown complete");
}

void Zenith_TaskSystem::SubmitTask(Zenith_Task* const pxTask)
{
	Zenith_Assert(pxTask != nullptr, "SubmitTask: Task is null");
	Zenith_Assert(g_bInitialized.load(std::memory_order_acquire), "SubmitTask: TaskSystem not initialized");
	Zenith_Assert(g_pxWorkAvailableSem != nullptr, "SubmitTask: Semaphore is null");

	// Atomic check-and-set for double-submit prevention (fixes TOCTOU race)
	bool bExpected = false;
	if (!pxTask->m_bSubmitted.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel))
	{
		Zenith_Assert(false, "SubmitTask: Task already submitted - call WaitUntilComplete before resubmitting");
		return;
	}

	if (!dbg_bMultithreaded)
	{
		pxTask->DoTask();
		return;
	}

	bool bEnqueued = false;
	{
		Zenith_ScopedMutexLock xLock(g_xQueueMutex);
		bEnqueued = g_xTaskQueue.Enqueue(pxTask);
	}

	Zenith_Assert(bEnqueued, "SubmitTask: Queue full (capacity=%u) - task dropped!", uMAX_TASKS);

	if (bEnqueued)
	{
		g_pxWorkAvailableSem->Signal();
	}
	else
	{
		// Reset submitted flag if enqueue failed so task can be retried
		pxTask->m_bSubmitted.store(false, std::memory_order_release);
	}
}

void Zenith_TaskSystem::SubmitTaskArray(Zenith_TaskArray* const pxTaskArray)
{
	Zenith_Assert(pxTaskArray != nullptr, "SubmitTaskArray: TaskArray is null");
	Zenith_Assert(g_bInitialized.load(std::memory_order_acquire), "SubmitTaskArray: TaskSystem not initialized");
	Zenith_Assert(g_pxWorkAvailableSem != nullptr, "SubmitTaskArray: Semaphore is null");

	// ATOMIC check-and-set for double-submit prevention
	// Reset counters AFTER successfully claiming the task to prevent TOCTOU race
	// where another thread could reset counters while workers are executing
	bool bExpected = false;
	if (!pxTaskArray->m_bSubmitted.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel))
	{
		Zenith_Assert(false, "SubmitTaskArray: TaskArray already submitted - call WaitUntilComplete before resubmitting");
		return;
	}

	// Reset counters AFTER successfully claiming the submitted flag
	// This is now safe because no other thread can submit until WaitUntilComplete resets m_bSubmitted
	pxTaskArray->Reset();

	if (!dbg_bMultithreaded)
	{
		// Execute all invocations sequentially in single-threaded mode
		for (u_int u = 0; u < pxTaskArray->GetNumInvocations(); u++)
		{
			pxTaskArray->DoTask();
		}
		return;
	}

	const u_int uNumInvocations = pxTaskArray->GetNumInvocations();
	const bool bSubmittingThreadJoins = pxTaskArray->GetSubmittingThreadJoins();

	if (bSubmittingThreadJoins)
	{
		// Submit (N-1) tasks to worker threads, submitting thread will do the last one
		const u_int uTasksForWorkers = uNumInvocations > 0 ? uNumInvocations - 1 : 0;

		u_int uEnqueuedCount = 0;
		{
			Zenith_ScopedMutexLock xLock(g_xQueueMutex);
			for (u_int u = 0; u < uTasksForWorkers; u++)
			{
				if (g_xTaskQueue.Enqueue(pxTaskArray))
				{
					uEnqueuedCount++;
				}
				else
				{
					break;  // Queue full
				}
			}
		}

		Zenith_Assert(uEnqueuedCount == uTasksForWorkers,
			"SubmitTaskArray: Only enqueued %u/%u tasks - queue full!", uEnqueuedCount, uTasksForWorkers);

		// Signal worker threads only for successfully enqueued tasks
		for (u_int u = 0; u < uEnqueuedCount; u++)
		{
			g_pxWorkAvailableSem->Signal();
		}

		// Submitting thread executes one task
		if (uNumInvocations > 0)
		{
			pxTaskArray->DoTask();
		}
	}
	else
	{
		// Submit all tasks to worker threads
		u_int uEnqueuedCount = 0;
		{
			Zenith_ScopedMutexLock xLock(g_xQueueMutex);
			for (u_int u = 0; u < uNumInvocations; u++)
			{
				if (g_xTaskQueue.Enqueue(pxTaskArray))
				{
					uEnqueuedCount++;
				}
				else
				{
					break;  // Queue full
				}
			}
		}

		Zenith_Assert(uEnqueuedCount == uNumInvocations,
			"SubmitTaskArray: Only enqueued %u/%u tasks - queue full!", uEnqueuedCount, uNumInvocations);

		// Signal worker threads only for successfully enqueued tasks
		for (u_int u = 0; u < uEnqueuedCount; u++)
		{
			g_pxWorkAvailableSem->Signal();
		}
	}
}
