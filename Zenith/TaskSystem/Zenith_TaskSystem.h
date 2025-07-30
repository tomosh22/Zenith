#pragma once

#include "Collections/Zenith_CircularQueue.h"
#include "Multithreading/Zenith_Multithreading.h"
#include "Profiling/Zenith_Profiling.h"

using Zenith_TaskFunction = void(*)(void* pData);

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

	void DoTask()
	{
		Zenith_Profiling::BeginProfile(m_eProfileIndex);
		m_pfnFunc(m_pData);
		Zenith_Profiling::EndProfile(m_eProfileIndex);
		m_uCompletedThreadID = Zenith_Multithreading::GetCurrentThreadID();
		m_xSemaphore.Signal();
	}

	void WaitUntilComplete()
	{
		m_xSemaphore.Wait();
	}

	const Zenith_ProfileIndex GetProfileIndex() const
	{
		return m_eProfileIndex;
	}

	const u_int GetCompletedThreadID() const
	{
		return m_uCompletedThreadID;
	}

private:

	Zenith_ProfileIndex m_eProfileIndex;
	Zenith_TaskFunction m_pfnFunc;
	Zenith_Semaphore m_xSemaphore;
	void* m_pData;
	u_int m_uCompletedThreadID;
};

class Zenith_TaskSystem
{
public:
	static void Inititalise();

	static void SubmitTask(Zenith_Task* const pxTask);

private:
};

