#include "Zenith.h"

#include "Zenith_Windows_Multithreading.h"

#include "Multithreading/Zenith_Multithreading.h"

#include <process.h>
#include <processthreadsapi.h>

thread_local static char tl_g_acThreadName[Zenith_Multithreading::uMAX_THREAD_NAME_LENGTH]{ 0 };
thread_local static u_int tl_g_uThreadID = -1;

Zenith_Windows_Mutex::Zenith_Windows_Mutex()
{
	InitializeCriticalSection(&m_xCriticalSection);
}

Zenith_Windows_Mutex::~Zenith_Windows_Mutex()
{
	DeleteCriticalSection(&m_xCriticalSection);
}

void Zenith_Windows_Mutex::Lock()
{
	EnterCriticalSection(&m_xCriticalSection);
}

bool Zenith_Windows_Mutex::TryLock()
{
	return TryEnterCriticalSection(&m_xCriticalSection) != 0;
}

void Zenith_Windows_Mutex::Unlock()
{
	LeaveCriticalSection(&m_xCriticalSection);
}

Zenith_Windows_Semaphore::Zenith_Windows_Semaphore(u_int uInitialValue, u_int uMaxValue)
{
	m_xHandle = CreateSemaphore(NULL, uInitialValue, uMaxValue, NULL);
}

Zenith_Windows_Semaphore::~Zenith_Windows_Semaphore()
{
	CloseHandle(m_xHandle);
}

void Zenith_Windows_Semaphore::Wait()
{
	WaitForSingleObject(m_xHandle, INFINITE);
}

bool Zenith_Windows_Semaphore::TryWait()
{
	return WaitForSingleObject(m_xHandle, 0) == WAIT_OBJECT_0;
}

bool Zenith_Windows_Semaphore::Signal()
{
	return ReleaseSemaphore(m_xHandle, 1, 0) != 0;
}

struct ThreadParams
{
	Zenith_Windows_Semaphore* m_pxSemaphore;
	Zenith_ThreadFunction m_pfnFunc;
	const void* m_pUserData;
	const char* m_szName;
};

unsigned long ThreadInit(void* pParams)
{
	Zenith_Multithreading::RegisterThread();
	const ThreadParams* pxParams = static_cast<const ThreadParams*>(pParams);
	memcpy(tl_g_acThreadName, pxParams->m_szName, strnlen(pxParams->m_szName, Zenith_Multithreading::uMAX_THREAD_NAME_LENGTH));
	pxParams->m_pxSemaphore->Signal();
	pxParams->m_pfnFunc(pxParams->m_pUserData);
	return 0;
}

void Zenith_Multithreading::Platform_CreateThread(const char* szName, Zenith_ThreadFunction pfnFunc, const void* pUserData)
{
	Zenith_Windows_Semaphore xSemaphore(0, 1);

	ThreadParams xParams;
	xParams.m_pxSemaphore = &xSemaphore;
	xParams.m_pfnFunc = pfnFunc;
	xParams.m_pUserData = pUserData;
	xParams.m_szName = szName;

	HANDLE pHandle = ::CreateThread(NULL, 128 * 1024, ThreadInit, &xParams, 0, NULL);

	xSemaphore.Wait();

	CloseHandle(pHandle);
}

void Zenith_Multithreading::Platform_RegisterThread()
{
	static u_int ls_uThreadID = 0;
	tl_g_uThreadID = ls_uThreadID++;
}

u_int Zenith_Multithreading::Platform_GetCurrentThreadID()
{
	Zenith_Assert(tl_g_uThreadID != -1, "This thread hasn't been registered with RegisterThread");
	return tl_g_uThreadID;
}