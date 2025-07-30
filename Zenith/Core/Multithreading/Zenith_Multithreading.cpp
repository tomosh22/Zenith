#include "Zenith.h"

#include "Multithreading/Zenith_Multithreading.h"
#include "Profiling/Zenith_Profiling.h"

void Zenith_Multithreading::CreateThread(const char* szName, Zenith_ThreadFunction pfnFunc, const void* pUserData)
{
	return Platform_CreateThread(szName, pfnFunc, pUserData);
}

void Zenith_Multithreading::RegisterThread()
{
	Platform_RegisterThread();
	Zenith_Profiling::RegisterThread();
}

u_int Zenith_Multithreading::GetCurrentThreadID()
{
	return Platform_GetCurrentThreadID();
}
