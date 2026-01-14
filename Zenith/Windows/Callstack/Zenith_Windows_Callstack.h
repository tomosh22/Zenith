#pragma once

#include <Windows.h>
#include <DbgHelp.h>
#include "Windows/Multithreading/Zenith_Windows_Multithreading.h"

// Windows-specific callstack capture using DbgHelp API
// Provides:
// - CaptureStackBackTrace for frame capture
// - SymFromAddr for symbol resolution
// - SymGetLineFromAddr64 for file/line info

class Zenith_Windows_Callstack
{
public:
	static void Initialise();
	static void Shutdown();

	static u_int Capture(void** apFrames, u_int uMaxFrames, u_int uSkipFrames);
	static bool Symbolicate(void* pAddress, struct Zenith_CallstackFrame& xFrameOut);

private:
	static bool s_bInitialised;
	static HANDLE s_hProcess;
	static Zenith_Windows_Mutex_T<false> s_xSymMutex;  // DbgHelp is not thread-safe
};
