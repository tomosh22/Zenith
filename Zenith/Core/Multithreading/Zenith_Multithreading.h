#pragma once

using Zenith_ThreadFunction = void(*)(const void* pUserData);

class Zenith_Multithreading
{
public:
	static void CreateThread(const char* szName, Zenith_ThreadFunction pfnFunc, const void* pUserData);
	static void RegisterThread();
	static u_int GetCurrentThreadID();

	static constexpr u_int uMAX_THREAD_NAME_LENGTH = 128;

private:
	static void Platform_CreateThread(const char* szName, Zenith_ThreadFunction pfnFunc, const void* pUserData);
	static void Platform_RegisterThread();
	static u_int Platform_GetCurrentThreadID();

};