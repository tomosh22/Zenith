#pragma once

#include <atomic>
#include "Zenith_OS_Include.h"//for Zenith_Mutex

using Zenith_ThreadFunction = void(*)(const void* pUserData);

// State + behaviour for the Multithreading subsystem. Held on g_xEngine
// and accessed via g_xEngine.Threading().
class Zenith_Multithreading
{
public:
	Zenith_Multithreading() = default;
	~Zenith_Multithreading() = default;

	Zenith_Multithreading(const Zenith_Multithreading&) = delete;
	Zenith_Multithreading& operator=(const Zenith_Multithreading&) = delete;

	void CreateThread(const char* szName, Zenith_ThreadFunction pfnFunc, const void* pUserData);
	void RegisterThread(const bool bMainThread = false);
	u_int GetCurrentThreadID();
	bool IsMainThread();

	static constexpr u_int uMAX_THREAD_NAME_LENGTH = 128;

	// Called from the platform-specific Platform_RegisterThread.
	u_int AllocateThreadID(bool bMainThread);

	u_int GetMainThreadID() const { return m_uMainThreadID; }

	std::atomic<u_int> m_uNextThreadID{0};
	u_int m_uMainThreadID = ~0u;

private:
	void Platform_CreateThread(const char* szName, Zenith_ThreadFunction pfnFunc, const void* pUserData);
	void Platform_RegisterThread(const bool bMainThread);
	u_int Platform_GetCurrentThreadID();
	bool Platform_IsMainThread();
};

// Bridge free function: lets headers that run on worker threads (e.g. the inline
// Zenith_Task::DoTask in Zenith_TaskSystem.h) read the current thread ID without
// including Zenith_Engine.h. Defined in Zenith_Multithreading.cpp; forwards to
// g_xEngine.Threading().GetCurrentThreadID(). Mirrors Zenith_Profiling_Detail.
namespace Zenith_Multithreading_Detail
{
	u_int GetCurrentThreadID();
}

template<typename TMutex = Zenith_Mutex>
class Zenith_ScopedMutexLock_T
{
public:
	Zenith_ScopedMutexLock_T() = delete;
	Zenith_ScopedMutexLock_T(TMutex& xMutex)
		: m_xMutex(xMutex)
	{
		m_xMutex.Lock();
	}
	~Zenith_ScopedMutexLock_T()
	{
		m_xMutex.Unlock();
	}
private:
	TMutex& m_xMutex;
};

using Zenith_ScopedMutexLock = Zenith_ScopedMutexLock_T<Zenith_Mutex>;
