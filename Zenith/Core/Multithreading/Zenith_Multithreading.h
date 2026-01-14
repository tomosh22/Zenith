#pragma once

using Zenith_ThreadFunction = void(*)(const void* pUserData);

class Zenith_Multithreading
{
public:
	static void CreateThread(const char* szName, Zenith_ThreadFunction pfnFunc, const void* pUserData);
	static void RegisterThread(const bool bMainThread = false);
	static u_int GetCurrentThreadID();
	static bool IsMainThread();

	static constexpr u_int uMAX_THREAD_NAME_LENGTH = 128;

private:
	static void Platform_CreateThread(const char* szName, Zenith_ThreadFunction pfnFunc, const void* pUserData);
	static void Platform_RegisterThread(const bool bMainThread);
	static u_int Platform_GetCurrentThreadID();
	static bool Platform_IsMainThread();

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