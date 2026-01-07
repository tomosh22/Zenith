#include "Zenith.h"

#include "Zenith_Android_Multithreading.h"

#include "Profiling/Zenith_Profiling.h"
#include "Multithreading/Zenith_Multithreading.h"

#include <pthread.h>
#include <unistd.h>
#include <cstring>

thread_local static char tl_g_acThreadName[Zenith_Multithreading::uMAX_THREAD_NAME_LENGTH]{ 0 };
thread_local static u_int tl_g_uThreadID = -1;
static u_int g_uMainThreadID = -1;

Zenith_Android_Mutex::Zenith_Android_Mutex()
{
	pthread_mutex_init(&m_xMutex, nullptr);
}

Zenith_Android_Mutex::~Zenith_Android_Mutex()
{
	pthread_mutex_destroy(&m_xMutex);
}

void Zenith_Android_Mutex::Lock()
{
	Zenith_Profiling::BeginProfile(ZENITH_PROFILE_INDEX__WAIT_FOR_MUTEX);
	pthread_mutex_lock(&m_xMutex);
	Zenith_Profiling::EndProfile(ZENITH_PROFILE_INDEX__WAIT_FOR_MUTEX);
}

bool Zenith_Android_Mutex::TryLock()
{
	return pthread_mutex_trylock(&m_xMutex) == 0;
}

void Zenith_Android_Mutex::Unlock()
{
	pthread_mutex_unlock(&m_xMutex);
}

Zenith_Android_Semaphore::Zenith_Android_Semaphore(u_int uInitialValue, u_int uMaxValue)
	: m_uMaxValue(uMaxValue)
{
	sem_init(&m_xSemaphore, 0, uInitialValue);
}

Zenith_Android_Semaphore::~Zenith_Android_Semaphore()
{
	sem_destroy(&m_xSemaphore);
}

void Zenith_Android_Semaphore::Wait()
{
	int iResult = sem_wait(&m_xSemaphore);
	Zenith_Assert(iResult == 0, "Failed to wait for semaphore");
}

bool Zenith_Android_Semaphore::TryWait()
{
	return sem_trywait(&m_xSemaphore) == 0;
}

bool Zenith_Android_Semaphore::Signal()
{
	int iValue;
	sem_getvalue(&m_xSemaphore, &iValue);
	if (static_cast<u_int>(iValue) >= m_uMaxValue)
	{
		return false;
	}
	return sem_post(&m_xSemaphore) == 0;
}

struct ThreadParams
{
	Zenith_Android_Semaphore* m_pxSemaphore;
	Zenith_ThreadFunction m_pfnFunc;
	const void* m_pUserData;
	const char* m_szName;
};

static void* ThreadInit(void* pParams)
{
	Zenith_Multithreading::RegisterThread();
	const ThreadParams* pxParams = static_cast<const ThreadParams*>(pParams);
	// Copy thread name with guaranteed null termination
	size_t uNameLen = strnlen(pxParams->m_szName, Zenith_Multithreading::uMAX_THREAD_NAME_LENGTH - 1);
	memcpy(tl_g_acThreadName, pxParams->m_szName, uNameLen);
	tl_g_acThreadName[uNameLen] = '\0';
	pxParams->m_pxSemaphore->Signal();
	pxParams->m_pfnFunc(pxParams->m_pUserData);
	return nullptr;
}

void Zenith_Multithreading::Platform_CreateThread(const char* szName, Zenith_ThreadFunction pfnFunc, const void* pUserData)
{
	Zenith_Android_Semaphore xSemaphore(0, 1);

	ThreadParams xParams;
	xParams.m_pxSemaphore = &xSemaphore;
	xParams.m_pfnFunc = pfnFunc;
	xParams.m_pUserData = pUserData;
	xParams.m_szName = szName;

	pthread_t xThread;
	pthread_attr_t xAttr;
	pthread_attr_init(&xAttr);
	pthread_attr_setstacksize(&xAttr, 128 * 1024);
	pthread_attr_setdetachstate(&xAttr, PTHREAD_CREATE_DETACHED);

	pthread_create(&xThread, &xAttr, ThreadInit, &xParams);
	pthread_attr_destroy(&xAttr);

	xSemaphore.Wait();
}

void Zenith_Multithreading::Platform_RegisterThread(const bool bMainThread)
{
	static std::atomic<u_int> ls_uThreadID{0};
	tl_g_uThreadID = ls_uThreadID.fetch_add(1);
	if (bMainThread)
	{
		g_uMainThreadID = tl_g_uThreadID;
	}
}

u_int Zenith_Multithreading::Platform_GetCurrentThreadID()
{
	Zenith_Assert(tl_g_uThreadID != -1, "This thread hasn't been registered with RegisterThread");
	return tl_g_uThreadID;
}

bool Zenith_Multithreading::Platform_IsMainThread()
{
	return tl_g_uThreadID == g_uMainThreadID;
}
