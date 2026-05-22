#pragma once

#include <atomic>

using Zenith_ThreadFunction = void(*)(const void* pUserData);

// Phase 9: state + behaviour for Multithreading subsystem.
class Zenith_MultithreadingImpl
{
public:
	Zenith_MultithreadingImpl() = default;
	~Zenith_MultithreadingImpl() = default;

	Zenith_MultithreadingImpl(const Zenith_MultithreadingImpl&) = delete;
	Zenith_MultithreadingImpl& operator=(const Zenith_MultithreadingImpl&) = delete;

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
