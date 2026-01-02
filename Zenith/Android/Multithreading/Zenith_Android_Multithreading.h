#pragma once

#include <pthread.h>
#include <semaphore.h>

class Zenith_Android_Mutex
{
public:
	Zenith_Android_Mutex();
	~Zenith_Android_Mutex();

	void Lock();
	bool TryLock();
	void Unlock();

private:
	pthread_mutex_t m_xMutex;
};

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
