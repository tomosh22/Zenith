#pragma once

#include "Collections/Zenith_CircularQueue.h"
#include "Core/Multithreading/Zenith_Multithreading.h"
#include "Profiling/Zenith_Profiling.h"

#include <atomic>

// TaskSystem is a FLAT task pool, NOT a dependency graph (no DependsOn API). To
// order work, block on a task's WaitUntilComplete() before submitting its
// successor. Use Zenith_DataParallelTask for independent parallel invocations.

using Zenith_TaskFunction = void(*)(void* pData);
using Zenith_DataParallelTaskFunction = void(*)(void* pData, u_int uInvocationIndex, u_int uNumInvocations);

class Zenith_Task
{
public:
	Zenith_Task() = delete;
	Zenith_Task(Zenith_ProfileIndex eProfileIndex, Zenith_TaskFunction pfnFunc, void* pData)
		: m_eProfileIndex(eProfileIndex)
		, m_pfnFunc(pfnFunc)
		, m_xSemaphore(0, 1)
		, m_pData(pData)
		, m_uCompletedThreadID(UINT32_MAX)
		, m_bSubmitted(false)
	{
	}

	virtual ~Zenith_Task() = default;

	virtual void DoTask()
	{
		Zenith_Assert(m_pfnFunc != nullptr, "DoTask: Task function pointer is null");
		Zenith_Profiling_Detail::BeginProfile(m_eProfileIndex, nullptr);
		m_pfnFunc(m_pData);
		Zenith_Profiling_Detail::EndProfile(m_eProfileIndex);
		m_uCompletedThreadID = Zenith_Multithreading_Detail::GetCurrentThreadID();
		m_xSemaphore.Signal();
	}

	void WaitUntilComplete()
	{
		if (!m_bSubmitted.load(std::memory_order_acquire)) return;
		Zenith_Profiling_Detail::BeginProfile(ZENITH_PROFILE_INDEX__WAIT_FOR_TASK_SYSTEM, nullptr);
		m_xSemaphore.Wait();
		Zenith_Profiling_Detail::EndProfile(ZENITH_PROFILE_INDEX__WAIT_FOR_TASK_SYSTEM);
		MarkRecycled();
	}

	const Zenith_ProfileIndex GetProfileIndex() const
	{
		return m_eProfileIndex;
	}

	const u_int GetCompletedThreadID() const
	{
		return m_uCompletedThreadID;
	}

protected:
	// For derived tasks that dispatch through their own function pointer.
	Zenith_Task(Zenith_ProfileIndex eProfileIndex, void* pData)
		: m_eProfileIndex(eProfileIndex)
		, m_pfnFunc(nullptr)
		, m_xSemaphore(0, 1)
		, m_pData(pData)
		, m_uCompletedThreadID(UINT32_MAX)
		, m_bSubmitted(false)
	{
	}

	Zenith_ProfileIndex m_eProfileIndex;
	Zenith_TaskFunction m_pfnFunc;
	Zenith_Semaphore m_xSemaphore;
	void* m_pData;
	u_int m_uCompletedThreadID;

private:
	// m_bSubmitted lifecycle: a submit claims the task exactly once via
	// TryMarkSubmitted; it becomes resubmittable again via MarkRecycled —
	// either by WaitUntilComplete observing completion, or by a failed
	// submit handing the task back to the caller.
	bool TryMarkSubmitted()
	{
		bool bExpected = false;
		return m_bSubmitted.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel);
	}

	void MarkRecycled()
	{
		m_bSubmitted.store(false, std::memory_order_release);
	}

	std::atomic<bool> m_bSubmitted;

	friend class Zenith_TaskSystem;
};

// ONE task executed uNumInvocations times: worker threads (and optionally the
// submitting thread) claim invocation indices via atomic fetch-add.
class Zenith_DataParallelTask : public Zenith_Task
{
public:
	Zenith_DataParallelTask() = delete;
	Zenith_DataParallelTask(Zenith_ProfileIndex eProfileIndex, Zenith_DataParallelTaskFunction pfnFunc, void* pData, u_int uNumInvocations, bool bCallingThreadParticipates = false)
		: Zenith_Task(eProfileIndex, pData)
		, m_pfnInvocationFunc(pfnFunc)
		, m_uNumInvocations(uNumInvocations)
		, m_bCallingThreadParticipates(bCallingThreadParticipates)
		, m_uInvocationCounter(0)
		, m_uCompletionCounter(0)
	{
		Zenith_Assert(pfnFunc != nullptr, "DataParallelTask function pointer cannot be null");
		Zenith_Assert(uNumInvocations > 0, "DataParallelTask must have at least 1 invocation");
	}

	virtual void DoTask() override
	{
		u_int uInvocationIndex = m_uInvocationCounter.fetch_add(1);

		Zenith_Profiling_Detail::BeginProfile(m_eProfileIndex, nullptr);
		m_pfnInvocationFunc(m_pData, uInvocationIndex, m_uNumInvocations);
		Zenith_Profiling_Detail::EndProfile(m_eProfileIndex);

		Zenith_Assert(uInvocationIndex < m_uNumInvocations, "We have done this task too many times");
		u_int uCompletedCount = m_uCompletionCounter.fetch_add(1) + 1;
		if (uCompletedCount == m_uNumInvocations)
		{
			m_uCompletedThreadID = Zenith_Multithreading_Detail::GetCurrentThreadID();
			m_xSemaphore.Signal();
		}
	}

	const u_int GetNumInvocations() const
	{
		return m_uNumInvocations;
	}

	// If true, the thread that calls SubmitDataParallelTask() runs invocations
	// itself instead of returning immediately — useful when it would otherwise
	// idle waiting for completion.
	const bool GetCallingThreadParticipates() const
	{
		return m_bCallingThreadParticipates;
	}

private:
	void ResetCounters()
	{
		m_uInvocationCounter.store(0, std::memory_order_release);
		m_uCompletionCounter.store(0, std::memory_order_release);
	}

	Zenith_DataParallelTaskFunction m_pfnInvocationFunc;
	u_int m_uNumInvocations;
	bool m_bCallingThreadParticipates;
	std::atomic<u_int> m_uInvocationCounter;
	std::atomic<u_int> m_uCompletionCounter;

	friend class Zenith_TaskSystem;
};

// Per-Engine task-system state. Owns the worker-thread pool + the
// shared task queue. Accessed via g_xEngine.Tasks(); worker threads
// also reach in through g_xEngine.Tasks() since Engine is the
// canonical single owner.
class Zenith_TaskSystem
{
public:
	Zenith_TaskSystem() = default;
	~Zenith_TaskSystem() = default;

	Zenith_TaskSystem(const Zenith_TaskSystem&) = delete;
	Zenith_TaskSystem& operator=(const Zenith_TaskSystem&) = delete;

	static constexpr u_int uMAX_TASKS = 128;

	void Initialise();
	void Shutdown();

	void SubmitTask(Zenith_Task* pxTask);
	void SubmitDataParallelTask(Zenith_DataParallelTask* pxTask);

	// Number of worker threads created at Initialise (min(hw_concurrency-1, 16)).
	// Used by data-parallel callers to size a Zenith_DataParallelTask's
	// invocation count. May be 0 before Initialise / on a single-core box;
	// callers must clamp to at least 1.
	u_int GetNumWorkerThreads() const { return m_uNumWorkerThreads; }

	// Called by the static worker thread function. Public so the
	// free-function ThreadFunc in the .cpp can reach in.
	void RunWorkerLoop();

private:
	// CAS-claims the task for submission; false if already submitted.
	bool TryClaimTask(Zenith_Task* pxTask, const char* szCallerName);

	// Enqueue a task pointer uCount times under the queue lock, then signal
	// workers. Returns how many were actually enqueued (the queue may fill).
	u_int EnqueueAndSignal(Zenith_Task* pxTask, u_int uCount);

	void RecoverFromShortEnqueue(Zenith_Task* pxTask);
	void RunInvocationsOnCallingThread(Zenith_DataParallelTask* pxTask, u_int uCount);

	Zenith_CircularQueue<Zenith_Task*, uMAX_TASKS> m_xTaskQueue;
	Zenith_Semaphore* m_pxWorkAvailableSem    = nullptr;
	Zenith_Semaphore* m_pxThreadsTerminatedSem = nullptr;
	Zenith_Mutex      m_xQueueMutex;
	std::atomic<bool> m_bTerminateThreads     {false};
	std::atomic<bool> m_bInitialized          {false};
	u_int             m_uNumWorkerThreads     = 0;
};
