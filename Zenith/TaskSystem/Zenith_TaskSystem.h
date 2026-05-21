#pragma once

#include "Collections/Zenith_CircularQueue.h"
#include "Core/Multithreading/Zenith_MultithreadingImpl.h"
#include "Profiling/Zenith_Profiling.h"

// ----------------------------------------------------------------------------
// TaskSystem dependency model
//
// This is a flat task pool, NOT a dependency graph. Tasks do not wait on each
// other and there is no DependsOn(...) API. To order work, the caller blocks
// on WaitUntilComplete() of the predecessor task before submitting the next:
//
//     Zenith_TaskSystem::SubmitTask(&xTaskA);
//     xTaskA.WaitUntilComplete();              // serialise A before B
//     Zenith_TaskSystem::SubmitTask(&xTaskB);
//
// Use Zenith_TaskArray for data-parallel work where each invocation is
// independent. Use multiple Zenith_Task submissions when the work items can
// run concurrently but should all complete before a barrier (wait on each).
// ----------------------------------------------------------------------------

using Zenith_TaskFunction = void(*)(void* pData);
using Zenith_TaskArrayFunction = void(*)(void* pData, u_int uInvocationIndex, u_int uNumInvocations);

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
		// Note: pfnFunc can be nullptr for Zenith_TaskArray which uses m_pfnArrayFunc instead
	}

protected:
	// Protected constructor for derived classes that don't use m_pfnFunc
	Zenith_Task(Zenith_ProfileIndex eProfileIndex, void* pData)
		: m_eProfileIndex(eProfileIndex)
		, m_pfnFunc(nullptr)
		, m_xSemaphore(0, 1)
		, m_pData(pData)
		, m_uCompletedThreadID(UINT32_MAX)
		, m_bSubmitted(false)
	{
	}

public:

	virtual ~Zenith_Task() = default;

	virtual void DoTask()
	{
		Zenith_Assert(m_pfnFunc != nullptr, "DoTask: Task function pointer is null");
		Zenith_Profiling::BeginProfile(m_eProfileIndex);
		m_pfnFunc(m_pData);
		Zenith_Profiling::EndProfile(m_eProfileIndex);
		m_uCompletedThreadID = g_xEngine.Threading().GetCurrentThreadID();
		m_xSemaphore.Signal();
	}

	void WaitUntilComplete()
	{
		if (!m_bSubmitted.load(std::memory_order_acquire)) return;
		Zenith_Profiling::BeginProfile(ZENITH_PROFILE_INDEX__WAIT_FOR_TASK_SYSTEM);
		m_xSemaphore.Wait();
		Zenith_Profiling::EndProfile(ZENITH_PROFILE_INDEX__WAIT_FOR_TASK_SYSTEM);
		m_bSubmitted.store(false, std::memory_order_release);
	}

	// Reset for task reuse.
	// Simple tasks have no per-invocation counters, so this base implementation
	// is empty. The m_bSubmitted flag IS cleared, but by WaitUntilComplete()
	// (see line ~57) — not here. Zenith_TaskArray::Reset() overrides this to
	// also clear its invocation/completion counters.
	void Reset()
	{
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

	Zenith_ProfileIndex m_eProfileIndex;
	Zenith_TaskFunction m_pfnFunc;
	Zenith_Semaphore m_xSemaphore;
	void* m_pData;
	u_int m_uCompletedThreadID;

	friend class Zenith_TaskSystem;
	friend class Zenith_TaskSystemImpl;  // Phase 3b: state owner accesses m_bSubmitted
	std::atomic<bool> m_bSubmitted;  // Thread-safe submitted flag
};

class Zenith_TaskArray : public Zenith_Task
{
public:
	Zenith_TaskArray() = delete;
	Zenith_TaskArray(Zenith_ProfileIndex eProfileIndex, Zenith_TaskArrayFunction pfnFunc, void* pData, u_int uNumInvocations, bool bCallingThreadParticipates = false)
		: Zenith_Task(eProfileIndex, pData)  // Use protected constructor (no pfnFunc)
		, m_pfnArrayFunc(pfnFunc)
		, m_uNumInvocations(uNumInvocations)
		, m_bCallingThreadParticipates(bCallingThreadParticipates)
		, m_uInvocationCounter(0)
		, m_uCompletionCounter(0)
	{
		Zenith_Assert(pfnFunc != nullptr, "TaskArray function pointer cannot be null");
		Zenith_Assert(uNumInvocations > 0, "TaskArray must have at least 1 invocation");
	}

	virtual void DoTask() override
	{
		u_int uInvocationIndex = m_uInvocationCounter.fetch_add(1);

		Zenith_Profiling::BeginProfile(m_eProfileIndex);
		m_pfnArrayFunc(m_pData, uInvocationIndex, m_uNumInvocations);
		Zenith_Profiling::EndProfile(m_eProfileIndex);

		// Signal completion when ALL threads have finished their work
		Zenith_Assert(uInvocationIndex < m_uNumInvocations, "We have done this task too many times");
		u_int uCompletedCount = m_uCompletionCounter.fetch_add(1) + 1;
		if (uCompletedCount == m_uNumInvocations)
		{
			m_uCompletedThreadID = g_xEngine.Threading().GetCurrentThreadID();
			m_xSemaphore.Signal();
		}
	}

	// Reset counters for task reuse
	// NOTE: Called by TaskSystem after successfully claiming m_bSubmitted flag
	// The assertion on m_bSubmitted is now handled by TaskSystem's compare_exchange
	void Reset()
	{
		m_uInvocationCounter.store(0, std::memory_order_release);
		m_uCompletionCounter.store(0, std::memory_order_release);
	}

	const u_int GetNumInvocations() const
	{
		return m_uNumInvocations;
	}

	// If true, the thread that calls SubmitTaskArray() participates as a worker
	// on this array (instead of returning immediately). Useful when the calling
	// thread would otherwise idle waiting for completion.
	const bool GetCallingThreadParticipates() const
	{
		return m_bCallingThreadParticipates;
	}

	// Backward-compatible name. Prefer GetCallingThreadParticipates() in new code.
	const bool GetSubmittingThreadJoins() const
	{
		return GetCallingThreadParticipates();
	}

private:

	Zenith_TaskArrayFunction m_pfnArrayFunc;
	u_int m_uNumInvocations;
	bool m_bCallingThreadParticipates;
	std::atomic<u_int> m_uInvocationCounter;
	std::atomic<u_int> m_uCompletionCounter;
};

// Public static facade. Phase 3b moved the actual state + logic onto
// Zenith_TaskSystemImpl (held by Zenith_Engine). These methods remain
// as 1-line forwarders to g_xEngine.Tasks() so the 26 existing call
// sites compile unchanged; Phase 9's static-API removal sweep deletes
// them once the codemod to g_xEngine.Tasks().X() is done.
class Zenith_TaskSystem
{
public:
	static void Inititalise();
	static void Shutdown();

	static void SubmitTask(Zenith_Task* const pxTask);
	static void SubmitTaskArray(Zenith_TaskArray* const pxTaskArray);
};

