#include "Zenith.h"

#include "Zenith_Windows_Multithreading.h"

#include "Core/Multithreading/Zenith_MultithreadingImpl.h"
#include "Profiling/Zenith_Profiling.h"
#include "Core/Multithreading/Zenith_MultithreadingImpl.h"

#include <process.h>
#include <processthreadsapi.h>

// TLS state stays per-thread (carve-out per refactor plan -- threads
// don't belong to an Engine, their registration index does). The
// shared state (thread-ID allocator + main-thread ID) moved to
// g_xEngine.Threading() in Phase 3a.
thread_local static char tl_g_acThreadName[Zenith_MultithreadingImpl::uMAX_THREAD_NAME_LENGTH]{ 0 };
thread_local static u_int tl_g_uThreadID = ~0u;

template<>
void Zenith_Windows_Mutex_T<true>::Lock()
{
	Zenith_Profiling::BeginProfile(ZENITH_PROFILE_INDEX__WAIT_FOR_MUTEX);
	EnterCriticalSection(&m_xCriticalSection);
	Zenith_Profiling::EndProfile(ZENITH_PROFILE_INDEX__WAIT_FOR_MUTEX);
}

template<>
void Zenith_Windows_Mutex_T<false>::Lock()
{
	EnterCriticalSection(&m_xCriticalSection);
}

Zenith_Windows_Semaphore::Zenith_Windows_Semaphore(u_int uInitialValue, u_int uMaxValue)
{
	m_xHandle = CreateSemaphore(NULL, uInitialValue, uMaxValue, NULL);
	Zenith_Assert(m_xHandle != NULL, "CreateSemaphore failed with error %lu", GetLastError());
}

Zenith_Windows_Semaphore::~Zenith_Windows_Semaphore()
{
	CloseHandle(m_xHandle);
}

void Zenith_Windows_Semaphore::Wait()
{
	DWORD ulResult = WaitForSingleObject(m_xHandle, INFINITE);
	Zenith_Assert(ulResult == WAIT_OBJECT_0, "Failed to wait for semaphore");
}

bool Zenith_Windows_Semaphore::TryWait()
{
	return WaitForSingleObject(m_xHandle, 0) == WAIT_OBJECT_0;
}

bool Zenith_Windows_Semaphore::Signal()
{
	const bool bRet = ReleaseSemaphore(m_xHandle, 1, 0) != 0;
	#ifdef ZENITH_ASSERT
	if (!bRet)
	{
		DWORD ulError = GetLastError();
		Zenith_Assert(false, "Failed to signal semaphore, error: %lu", ulError);
	}
	#endif
	return bRet;
}

struct ThreadParams
{
	Zenith_ThreadFunction m_pfnFunc;
	const void* m_pUserData;
	char m_acName[Zenith_MultithreadingImpl::uMAX_THREAD_NAME_LENGTH];
};

unsigned long ThreadInit(void* pParams)
{
	g_xEngine.Threading().RegisterThread();

	// Take ownership of heap-allocated params - we are responsible for deleting
	ThreadParams* pxParams = static_cast<ThreadParams*>(pParams);

	// Copy data to local/thread-local storage
	memcpy(tl_g_acThreadName, pxParams->m_acName, Zenith_MultithreadingImpl::uMAX_THREAD_NAME_LENGTH);
	Zenith_ThreadFunction pfnFunc = pxParams->m_pfnFunc;
	const void* pUserData = pxParams->m_pUserData;

	// Delete heap-allocated params now that we've copied everything
	delete pxParams;

	// Call the thread function
	pfnFunc(pUserData);
	return 0;
}

void Zenith_MultithreadingImpl::Platform_CreateThread(const char* szName, Zenith_ThreadFunction pfnFunc, const void* pUserData)
{
	// Allocate params on heap - new thread takes ownership and deletes
	ThreadParams* pxParams = new ThreadParams;
	pxParams->m_pfnFunc = pfnFunc;
	pxParams->m_pUserData = pUserData;
	strncpy_s(pxParams->m_acName, Zenith_MultithreadingImpl::uMAX_THREAD_NAME_LENGTH, szName, _TRUNCATE);

	HANDLE pHandle = ::CreateThread(NULL, 128 * 1024, ThreadInit, pxParams, 0, NULL);
	Zenith_Assert(pHandle != NULL, "CreateThread failed with error %lu", GetLastError());

	CloseHandle(pHandle);
}

void Zenith_MultithreadingImpl::Platform_RegisterThread(const bool bMainThread)
{
	// Thread-ID allocator + main-thread tracking live on g_xEngine.Threading()
	// (Phase 3a). The engine guarantees the impl exists before any
	// thread can reach this code path.
	tl_g_uThreadID = g_xEngine.Threading().AllocateThreadID(bMainThread);
}

u_int Zenith_MultithreadingImpl::Platform_GetCurrentThreadID()
{
	Zenith_Assert(tl_g_uThreadID != ~0u, "This thread hasn't been registered with RegisterThread");
	return tl_g_uThreadID;
}

bool Zenith_MultithreadingImpl::Platform_IsMainThread()
{
	return tl_g_uThreadID == g_xEngine.Threading().GetMainThreadID();
}
