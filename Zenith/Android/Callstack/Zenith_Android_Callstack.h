#pragma once

#include <pthread.h>

// Android-specific callstack capture
// Provides:
// - _Unwind_Backtrace for frame capture
// - dladdr for symbol resolution (function names only, no line numbers on Android)
// - __cxa_demangle for C++ symbol demangling

class Zenith_Android_Callstack
{
public:
	static void Initialise();
	static void Shutdown();

	static u_int Capture(void** apFrames, u_int uMaxFrames, u_int uSkipFrames);
	static bool Symbolicate(void* pAddress, struct Zenith_CallstackFrame& xFrameOut);

private:
	static bool s_bInitialised;
	static pthread_mutex_t s_xMutex;
};
