#include "Zenith.h"

#include "Core/Multithreading/Zenith_MultithreadingImpl.h"
#include "Profiling/Zenith_Profiling.h"

void Zenith_MultithreadingImpl::CreateThread(const char* szName, Zenith_ThreadFunction pfnFunc, const void* pUserData)
{
	return Platform_CreateThread(szName, pfnFunc, pUserData);
}

void Zenith_MultithreadingImpl::RegisterThread(const bool bMainThread /*= false*/)
{
	Platform_RegisterThread(bMainThread);
	Zenith_Profiling::RegisterThread();
}

u_int Zenith_MultithreadingImpl::GetCurrentThreadID()
{
	return Platform_GetCurrentThreadID();
}

bool Zenith_MultithreadingImpl::IsMainThread()
{
	return Platform_IsMainThread();
}
