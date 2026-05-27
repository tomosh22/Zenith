#include "Zenith.h"

#include "Core/Multithreading/Zenith_Multithreading.h"
#include "Profiling/Zenith_Profiling.h"

void Zenith_Multithreading::CreateThread(const char* szName, Zenith_ThreadFunction pfnFunc, const void* pUserData)
{
	return Platform_CreateThread(szName, pfnFunc, pUserData);
}

void Zenith_Multithreading::RegisterThread(const bool bMainThread /*= false*/)
{
	Platform_RegisterThread(bMainThread);
	g_xEngine.Profiling().RegisterThread();
}

u_int Zenith_Multithreading::GetCurrentThreadID()
{
	return Platform_GetCurrentThreadID();
}

bool Zenith_Multithreading::IsMainThread()
{
	return Platform_IsMainThread();
}

u_int Zenith_Multithreading::AllocateThreadID(bool bMainThread)
{
	const u_int uID = m_uNextThreadID.fetch_add(1);
	if (bMainThread)
	{
		m_uMainThreadID = uID;
	}
	return uID;
}
