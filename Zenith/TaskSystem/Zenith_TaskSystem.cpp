#include "Zenith.h"

#include "TaskSystem/Zenith_TaskSystem.h"

#include "Multithreading/Zenith_Multithreading.h"

static constexpr u_int uMAX_TASKS = 128;

static Zenith_CircularQueue<Zenith_Task*, uMAX_TASKS> g_xTaskQueue;
static Zenith_Semaphore* g_pxWorkAvailableSem = nullptr;
static Zenith_Semaphore* g_pxThreadsTerminatedSem = nullptr;
static Zenith_Mutex g_xQueueMutex;
static bool g_bTerminateThreads = false;

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
	//#TO_TODO: work this out properly
	const u_int uNumThreads = 4;

	g_pxWorkAvailableSem = new Zenith_Semaphore(0, uMAX_TASKS);
	g_pxThreadsTerminatedSem = new Zenith_Semaphore(0, uNumThreads);

	for (u_int u = 0; u < uNumThreads; u++)
	{
		char acName[Zenith_Multithreading::uMAX_THREAD_NAME_LENGTH];
		snprintf(acName, Zenith_Multithreading::uMAX_THREAD_NAME_LENGTH, "Zenith_TaskSystem %u", u);
		Zenith_Multithreading::CreateThread(acName, ThreadFunc, nullptr);
	}
}

void Zenith_TaskSystem::SubmitTask(Zenith_Task* const pxTask)
{
	g_xQueueMutex.Lock();
	g_xTaskQueue.Enqueue(pxTask);
	g_xQueueMutex.Unlock();
	g_pxWorkAvailableSem->Signal();
}
