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

class Zenith_ScopedMutexLock
{
public:
	Zenith_ScopedMutexLock() = delete;
	Zenith_ScopedMutexLock(Zenith_Mutex& xMutex);
	~Zenith_ScopedMutexLock();
private:
	Zenith_Mutex& m_xMutex;
};