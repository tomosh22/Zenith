#pragma once

#include <pthread.h>
#include <semaphore.h>

template<bool bEnableProfiling = true>
class Zenith_Android_Mutex_T
{
public:
	Zenith_Android_Mutex_T()
	{
		// Use recursive mutex to match Windows CRITICAL_SECTION behaviour
		pthread_mutexattr_t xAttr;
		pthread_mutexattr_init(&xAttr);
		pthread_mutexattr_settype(&xAttr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&m_xMutex, &xAttr);
		pthread_mutexattr_destroy(&xAttr);
	}

	~Zenith_Android_Mutex_T()
	{
		pthread_mutex_destroy(&m_xMutex);
	}

	void Lock();

	bool TryLock()
	{
		return pthread_mutex_trylock(&m_xMutex) == 0;
	}

	void Unlock()
	{
		pthread_mutex_unlock(&m_xMutex);
	}

private:
	pthread_mutex_t m_xMutex;
};

using Zenith_Android_Mutex = Zenith_Android_Mutex_T<true>;

class Zenith_Android_Semaphore
{
public:
	Zenith_Android_Semaphore(u_int uInitialValue, u_int uMaxValue);
	~Zenith_Android_Semaphore();

	void Wait();
	bool TryWait();
	bool Signal();

private:
	sem_t m_xSemaphore;
	u_int m_uMaxValue;
};
