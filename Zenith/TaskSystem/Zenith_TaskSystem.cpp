#include "Zenith.h"

#include "TaskSystem/Zenith_TaskSystem.h"

#include "DebugVariables/Zenith_DebugVariables.h"
#include "Multithreading/Zenith_Multithreading.h"
#include <thread>

static constexpr u_int uMAX_TASKS = 128;

static Zenith_CircularQueue<Zenith_Task*, uMAX_TASKS> g_xTaskQueue;
static Zenith_Semaphore* g_pxWorkAvailableSem = nullptr;
static Zenith_Semaphore* g_pxThreadsTerminatedSem = nullptr;
static Zenith_Mutex g_xQueueMutex;
static bool g_bTerminateThreads = false;

DEBUGVAR bool dbg_bMultithreaded = true;

static void ThreadFunc(const void* pData)
{
	do
	{
		g_pxWorkAvailableSem->Wait();

		if (g_bTerminateThreads) break;

		Zenith_Task* pxTask = nullptr;
		do
		{
			g_xQueueMutex.Lock();
			g_xTaskQueue.Dequeue(pxTask);
			g_xQueueMutex.Unlock();
		}
		while (!pxTask);

		pxTask->DoTask();

	} while (!g_bTerminateThreads);

	g_pxThreadsTerminatedSem->Signal();
}

void Zenith_TaskSystem::Inititalise()
{
	// Use hardware thread count minus 1 (reserve main thread)
	// Minimum of 1 worker thread, maximum of 16
	constexpr u_int uMAX_TASK_THREADS = 16;
	const u_int uHardwareThreads = std::thread::hardware_concurrency();
	u_int uNumThreads = (uHardwareThreads > 1) ? (uHardwareThreads - 1) : 1;
	if (uNumThreads > uMAX_TASK_THREADS) uNumThreads = uMAX_TASK_THREADS;

	Zenith_Log("TaskSystem: Creating %u worker threads (hardware reports %u threads)", uNumThreads, uHardwareThreads);

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
}

void Zenith_TaskSystem::SubmitTask(Zenith_Task* const pxTask)
{
	if (!dbg_bMultithreaded)
	{
		pxTask->m_bSubmitted = true;
		pxTask->DoTask();
		return;
	}
	g_xQueueMutex.Lock();
	pxTask->m_bSubmitted = true;
	g_xTaskQueue.Enqueue(pxTask);
	g_xQueueMutex.Unlock();
	g_pxWorkAvailableSem->Signal();
}

void Zenith_TaskSystem::SubmitTaskArray(Zenith_TaskArray* const pxTaskArray)
{
	if (!dbg_bMultithreaded)
	{
		pxTaskArray->m_bSubmitted = true;
		// Execute all invocations sequentially in debug mode
		for (u_int u = 0; u < pxTaskArray->GetNumInvocations(); u++)
		{
			pxTaskArray->DoTask();
		}
		pxTaskArray->Reset();
		return;
	}

	pxTaskArray->Reset();

	const u_int uNumInvocations = pxTaskArray->GetNumInvocations();
	const bool bSubmittingThreadJoins = pxTaskArray->GetSubmittingThreadJoins();

	if (bSubmittingThreadJoins)
	{
		// Submit (N-1) tasks to worker threads, submitting thread will do the last one
		const u_int uTasksForWorkers = uNumInvocations > 0 ? uNumInvocations - 1 : 0;
		
		g_xQueueMutex.Lock();
		pxTaskArray->m_bSubmitted = true;
		for (u_int u = 0; u < uTasksForWorkers; u++)
		{
			g_xTaskQueue.Enqueue(pxTaskArray);
		}
		g_xQueueMutex.Unlock();

		// Signal worker threads
		for (u_int u = 0; u < uTasksForWorkers; u++)
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
		// Original behavior: submit all tasks to worker threads
		g_xQueueMutex.Lock();
		pxTaskArray->m_bSubmitted = true;
		for (u_int u = 0; u < uNumInvocations; u++)
		{
			g_xTaskQueue.Enqueue(pxTaskArray);
		}
		g_xQueueMutex.Unlock();

		// Signal worker threads
		for (u_int u = 0; u < uNumInvocations; u++)
		{
			g_pxWorkAvailableSem->Signal();
		}
	}
}
