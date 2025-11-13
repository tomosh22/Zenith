#pragma once

#include "Collections/Zenith_CircularQueue.h"
#include "Multithreading/Zenith_Multithreading.h"
#include "Profiling/Zenith_Profiling.h"

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
		, m_uCompletedThreadID(-1)
	{

	}

	virtual ~Zenith_Task() = default;

	virtual void DoTask()
	{
		Zenith_Profiling::BeginProfile(m_eProfileIndex);
		m_pfnFunc(m_pData);
		Zenith_Profiling::EndProfile(m_eProfileIndex);
		m_uCompletedThreadID = Zenith_Multithreading::GetCurrentThreadID();
		m_xSemaphore.Signal();
	}

	void WaitUntilComplete()
	{
		Zenith_Profiling::BeginProfile(ZENITH_PROFILE_INDEX__WAIT_FOR_TASK_SYSTEM);
		m_xSemaphore.Wait();
		Zenith_Profiling::EndProfile(ZENITH_PROFILE_INDEX__WAIT_FOR_TASK_SYSTEM);
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
};

class Zenith_TaskArray : public Zenith_Task
{
public:
	Zenith_TaskArray() = delete;
	Zenith_TaskArray(Zenith_ProfileIndex eProfileIndex, Zenith_TaskArrayFunction pfnFunc, void* pData, u_int uNumInvocations)
		: Zenith_Task(eProfileIndex, nullptr, pData)
		, m_pfnArrayFunc(pfnFunc)
		, m_uNumInvocations(uNumInvocations)
		, m_uInvocationCounter(0)
		, m_uCompletionCounter(0)
	{

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
			m_uCompletedThreadID = Zenith_Multithreading::GetCurrentThreadID();
			m_xSemaphore.Signal();
		}
	}

	void Reset()
	{
		m_uInvocationCounter.store(0);
		m_uCompletionCounter.store(0);
	}

	const u_int GetNumInvocations() const
	{
		return m_uNumInvocations;
	}

private:

	Zenith_TaskArrayFunction m_pfnArrayFunc;
	u_int m_uNumInvocations;
	std::atomic<u_int> m_uInvocationCounter;
	std::atomic<u_int> m_uCompletionCounter;
};

class Zenith_TaskSystem
{
public:
	static void Inititalise();

	static void SubmitTask(Zenith_Task* const pxTask);
	static void SubmitTaskArray(Zenith_TaskArray* const pxTaskArray);

private:
};

