#pragma once

#include "Windows/Multithreading/Zenith_Windows_Multithreading.h"

// Windows-specific callstack capture using the DbgHelp API.
//
// W5.1/W5.2 (PCH slimming): this header is pulled into the precompiled header via
// Zenith_OS_Include.h, so <Windows.h>/<DbgHelp.h> moved to the .cpp. The process
// HANDLE is stored opaquely as a void* (HANDLE is itself a void* typedef); the
// method signatures already use only neutral types (void*/void**). No Win32 here.
//
// Provides: Capture (CaptureStackBackTrace) + Symbolicate (SymFromAddr +
// SymGetLineFromAddr64 for symbol / file / line).

class Zenith_Windows_Callstack
{
public:
	static void Initialise();
	static void Shutdown();

	static u_int Capture(void** apFrames, u_int uMaxFrames, u_int uSkipFrames);
	static bool Symbolicate(void* pAddress, struct Zenith_CallstackFrame& xFrameOut);

private:
	static bool s_bInitialised;
	static void* s_hProcess; // Win32 HANDLE (void*); reinterpreted in the .cpp.
	static Zenith_Windows_Mutex_T<false> s_xSymMutex;  // DbgHelp is not thread-safe
};
