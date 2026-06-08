#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Zenith_Windows_Multithreading.h"

#include "Core/Multithreading/Zenith_Multithreading.h"
#include "Profiling/Zenith_Profiling.h"
#include "Core/Multithreading/Zenith_Multithreading.h"

// W5.1: <Windows.h> now lives in the .cpp (not the PCH-reachable header). The
// APIENTRY guard mirrors Zenith.h's (GLFW, pulled via the PCH, also defines it).
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <Windows.h>
#include <process.h>
#include <processthreadsapi.h>

// TLS state stays per-thread (carve-out per refactor plan -- threads
// don't belong to an Engine, their registration index does). The
// shared state (thread-ID allocator + main-thread ID) moved to
// g_xEngine.Threading() in Phase 3a.
thread_local static char tl_g_acThreadName[Zenith_Multithreading::uMAX_THREAD_NAME_LENGTH]{ 0 };
thread_local static u_int tl_g_uThreadID = ~0u;

// W5.1: the CRITICAL_SECTION lives in the header's opaque byte storage; recover it
// here (where <Windows.h> is visible). sizeof/alignof are validated below.
static_assert(sizeof(CRITICAL_SECTION) <= 64, "m_axCriticalSectionStorage is too small for CRITICAL_SECTION");
static_assert(alignof(CRITICAL_SECTION) <= 8, "m_axCriticalSectionStorage alignment is too weak for CRITICAL_SECTION");

#define ZENITH_MUTEX_CS(self) (reinterpret_cast<CRITICAL_SECTION*>((self).m_axCriticalSectionStorage))

template<bool bEnableProfiling>
Zenith_Windows_Mutex_T<bEnableProfiling>::Zenith_Windows_Mutex_T()
{
	InitializeCriticalSection(ZENITH_MUTEX_CS(*this));
}

template<bool bEnableProfiling>
Zenith_Windows_Mutex_T<bEnableProfiling>::~Zenith_Windows_Mutex_T()
{
	DeleteCriticalSection(ZENITH_MUTEX_CS(*this));
}

template<bool bEnableProfiling>
bool Zenith_Windows_Mutex_T<bEnableProfiling>::TryLock()
{
	return TryEnterCriticalSection(ZENITH_MUTEX_CS(*this)) != 0;
}

template<bool bEnableProfiling>
void Zenith_Windows_Mutex_T<bEnableProfiling>::Unlock()
{
	LeaveCriticalSection(ZENITH_MUTEX_CS(*this));
}

template<>
void Zenith_Windows_Mutex_T<true>::Lock()
{
	g_xEngine.Profiling().BeginProfile(ZENITH_PROFILE_INDEX__WAIT_FOR_MUTEX);
	EnterCriticalSection(ZENITH_MUTEX_CS(*this));
	g_xEngine.Profiling().EndProfile(ZENITH_PROFILE_INDEX__WAIT_FOR_MUTEX);
}

template<>
void Zenith_Windows_Mutex_T<false>::Lock()
{
	EnterCriticalSection(ZENITH_MUTEX_CS(*this));
}

// Explicit instantiation: this TU emits both mutex variants used engine-wide
// (Zenith_Windows_Mutex = <true>, Zenith_Mutex_NoProfiling = <false>). The two
// Lock() specializations above are picked up by these instantiations.
template class Zenith_Windows_Mutex_T<true>;
template class Zenith_Windows_Mutex_T<false>;

Zenith_Windows_Semaphore::Zenith_Windows_Semaphore(u_int uInitialValue, u_int uMaxValue)
{
	m_pHandle = CreateSemaphore(NULL, uInitialValue, uMaxValue, NULL);
	Zenith_Assert(m_pHandle != NULL, "CreateSemaphore failed with error %lu", GetLastError());
}

Zenith_Windows_Semaphore::~Zenith_Windows_Semaphore()
{
	CloseHandle(m_pHandle);
}

void Zenith_Windows_Semaphore::Wait()
{
	DWORD ulResult = WaitForSingleObject(m_pHandle, INFINITE);
	Zenith_Assert(ulResult == WAIT_OBJECT_0, "Failed to wait for semaphore");
}

bool Zenith_Windows_Semaphore::TryWait()
{
	return WaitForSingleObject(m_pHandle, 0) == WAIT_OBJECT_0;
}

bool Zenith_Windows_Semaphore::Signal()
{
	const bool bRet = ReleaseSemaphore(m_pHandle, 1, 0) != 0;
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
	char m_acName[Zenith_Multithreading::uMAX_THREAD_NAME_LENGTH];
};

unsigned long ThreadInit(void* pParams)
{
	g_xEngine.Threading().RegisterThread();

	// Take ownership of heap-allocated params - we are responsible for deleting
	ThreadParams* pxParams = static_cast<ThreadParams*>(pParams);

	// Copy data to local/thread-local storage
	memcpy(tl_g_acThreadName, pxParams->m_acName, Zenith_Multithreading::uMAX_THREAD_NAME_LENGTH);
	Zenith_ThreadFunction pfnFunc = pxParams->m_pfnFunc;
	const void* pUserData = pxParams->m_pUserData;

	// Delete heap-allocated params now that we've copied everything
	delete pxParams;

	// Call the thread function
	pfnFunc(pUserData);
	return 0;
}

void Zenith_Multithreading::Platform_CreateThread(const char* szName, Zenith_ThreadFunction pfnFunc, const void* pUserData)
{
	// Allocate params on heap - new thread takes ownership and deletes
	ThreadParams* pxParams = new ThreadParams;
	pxParams->m_pfnFunc = pfnFunc;
	pxParams->m_pUserData = pUserData;
	strncpy_s(pxParams->m_acName, Zenith_Multithreading::uMAX_THREAD_NAME_LENGTH, szName, _TRUNCATE);

	HANDLE pHandle = ::CreateThread(NULL, 128 * 1024, ThreadInit, pxParams, 0, NULL);
	Zenith_Assert(pHandle != NULL, "CreateThread failed with error %lu", GetLastError());

	CloseHandle(pHandle);
}

void Zenith_Multithreading::Platform_RegisterThread(const bool bMainThread)
{
	// Thread-ID allocator + main-thread tracking live on g_xEngine.Threading()
	// (Phase 3a). The engine guarantees the impl exists before any
	// thread can reach this code path.
	tl_g_uThreadID = g_xEngine.Threading().AllocateThreadID(bMainThread);
}

u_int Zenith_Multithreading::Platform_GetCurrentThreadID()
{
	Zenith_Assert(tl_g_uThreadID != ~0u, "This thread hasn't been registered with RegisterThread");
	return tl_g_uThreadID;
}

bool Zenith_Multithreading::Platform_IsMainThread()
{
	return tl_g_uThreadID == g_xEngine.Threading().GetMainThreadID();
}
