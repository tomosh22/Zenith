#pragma once
#pragma warning(push)
#pragma warning(disable: 4005) // APIENTRY macro redefinition (GLFW vs Windows SDK)
#include <Windows.h>
#include <winnt.h>
#pragma warning(pop)

template<bool bEnableProfiling = true>
class Zenith_Windows_Mutex_T
{
public:

	Zenith_Windows_Mutex_T()
	{
		InitializeCriticalSection(&m_xCriticalSection);
	}

	~Zenith_Windows_Mutex_T()
	{
		DeleteCriticalSection(&m_xCriticalSection);
	}

	void Lock();

	bool TryLock()
	{
		return TryEnterCriticalSection(&m_xCriticalSection) != 0;
	}

	void Unlock()
	{
		LeaveCriticalSection(&m_xCriticalSection);
	}

private:

	CRITICAL_SECTION m_xCriticalSection;
};

using Zenith_Windows_Mutex = Zenith_Windows_Mutex_T<true>;

class Zenith_Windows_Semaphore
{
public:

	Zenith_Windows_Semaphore(u_int uInitialValue, u_int uMaxValue);
	~Zenith_Windows_Semaphore();

	void Wait();
	bool TryWait();
	bool Signal();

private:

	HANDLE m_xHandle;
};