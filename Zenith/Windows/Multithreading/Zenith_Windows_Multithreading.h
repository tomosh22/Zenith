#pragma once
#include <Windows.h>
#include <winnt.h>

class Zenith_Windows_Mutex
{
public:

	Zenith_Windows_Mutex();
	~Zenith_Windows_Mutex();

	void Lock();
	bool TryLock();
	void Unlock();

private:

	CRITICAL_SECTION m_xCriticalSection;
};

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