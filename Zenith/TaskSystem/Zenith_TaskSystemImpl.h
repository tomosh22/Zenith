#pragma once

#include "Collections/Zenith_CircularQueue.h"
#include "TaskSystem/Zenith_TaskSystem.h"

#include <atomic>

// Per-Engine task-system state. Owns the worker-thread pool + the
// shared task queue that used to live as module-scope globals in
// Zenith_TaskSystem.cpp:
//   g_xTaskQueue, g_pxWorkAvailableSem, g_pxThreadsTerminatedSem,
//   g_xQueueMutex, g_bTerminateThreads, g_bInitialized,
//   g_uNumWorkerThreads.
//
// Zenith_TaskSystem's existing static facade
// (Inititalise / Shutdown / SubmitTask / SubmitTaskArray) is preserved
// as thin forwarders to g_xEngine.Tasks(). Worker threads access the
// per-engine state via g_xEngine.Tasks() as well -- the single Engine
// instance is the canonical owner.
class Zenith_TaskSystemImpl
{
public:
	Zenith_TaskSystemImpl() = default;
	~Zenith_TaskSystemImpl() = default;

	Zenith_TaskSystemImpl(const Zenith_TaskSystemImpl&) = delete;
	Zenith_TaskSystemImpl& operator=(const Zenith_TaskSystemImpl&) = delete;

	static constexpr u_int uMAX_TASKS = 128;

	void Initialise();
	void Shutdown();

	void SubmitTask(Zenith_Task* pxTask);
	void SubmitTaskArray(Zenith_TaskArray* pxTaskArray);

	// Called by the static worker thread function. Public so the
	// free-function ThreadFunc in the .cpp can reach in.
	void RunWorkerLoop();

private:
	// Atomic CAS to claim a task for submission. Returns false if
	// already submitted.
	bool TryClaimTask(Zenith_Task* pxTask, const char* szCallerName);

	// Enqueue a task pointer uCount times under the queue lock, then
	// signal workers. Returns the number of tasks successfully
	// enqueued.
	u_int EnqueueAndSignal(Zenith_Task* pxTask, u_int uCount);

	Zenith_CircularQueue<Zenith_Task*, uMAX_TASKS> m_xTaskQueue;
	Zenith_Semaphore* m_pxWorkAvailableSem    = nullptr;
	Zenith_Semaphore* m_pxThreadsTerminatedSem = nullptr;
	Zenith_Mutex      m_xQueueMutex;
	std::atomic<bool> m_bTerminateThreads     {false};
	std::atomic<bool> m_bInitialized          {false};
	u_int             m_uNumWorkerThreads     = 0;
};
