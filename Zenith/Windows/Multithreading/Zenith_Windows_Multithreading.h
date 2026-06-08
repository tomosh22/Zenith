#pragma once

// W5.1 (PCH slimming): this header is pulled into the precompiled header via
// Zenith_OS_Include.h, so any <Windows.h> here lands in EVERY translation unit.
// To let <Windows.h> leave the PCH, the Win32 handles below are stored as raw,
// correctly-aligned bytes (CRITICAL_SECTION) / an opaque void* (HANDLE) and the
// actual Win32 calls live in the .cpp (which includes <Windows.h>). This is the
// no-pimpl-respecting "inline opaque storage" pattern: the storage is BY VALUE
// (no heap pointer, no hidden anonymous type) — only the *implementation* moves
// out of line. The small cost is that Lock/TryLock/Unlock are no longer inlined;
// accepted in exchange for dropping the engine-wide <Windows.h> dependency.

template<bool bEnableProfiling = true>
class Zenith_Windows_Mutex_T
{
public:

	Zenith_Windows_Mutex_T();
	~Zenith_Windows_Mutex_T();

	void Lock();
	bool TryLock();
	void Unlock();

private:

	// Opaque storage for a Win32 CRITICAL_SECTION. sizeof(CRITICAL_SECTION) is 40
	// on x64 / 24 on x86 and alignof is 8 / 4; sized generously here and
	// static_assert'd against the real type in the .cpp where <Windows.h> is visible.
	alignas(8) unsigned char m_axCriticalSectionStorage[64];
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

	// Win32 HANDLE is a typedef for void*; stored opaquely so the header needs no
	// <Windows.h>. The .cpp reinterprets it back to HANDLE for the Win32 calls.
	void* m_pHandle = nullptr;
};
