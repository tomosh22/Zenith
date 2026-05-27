#include "Zenith.h"

#include "Zenith_Windows_Callstack.h"
#include "Callstack/Zenith_Callstack.h"
#include "Core/Multithreading/Zenith_Multithreading.h"

#include <cstring>

#pragma comment(lib, "dbghelp.lib")

bool Zenith_Windows_Callstack::s_bInitialised = false;
HANDLE Zenith_Windows_Callstack::s_hProcess = NULL;
Zenith_Windows_Mutex_T<false> Zenith_Windows_Callstack::s_xSymMutex;

void Zenith_Windows_Callstack::Initialise()
{
	if (s_bInitialised)
	{
		return;
	}

	s_hProcess = GetCurrentProcess();

	// Initialize symbol handler
	SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
	if (!SymInitialize(s_hProcess, NULL, TRUE))
	{
		DWORD ulError = GetLastError();
		Zenith_Log(LOG_CATEGORY_CORE, "SymInitialize failed with error %lu", ulError);
		return;
	}

	s_bInitialised = true;
	Zenith_Log(LOG_CATEGORY_CORE, "Callstack capture initialised");
}

void Zenith_Windows_Callstack::Shutdown()
{
	if (!s_bInitialised)
	{
		return;
	}

	SymCleanup(s_hProcess);
	s_bInitialised = false;
	s_hProcess = NULL;
}

u_int Zenith_Windows_Callstack::Capture(void** apFrames, u_int uMaxFrames, u_int uSkipFrames)
{
	if (!s_bInitialised || apFrames == nullptr || uMaxFrames == 0)
	{
		return 0;
	}

	// CaptureStackBackTrace already handles skipping internal frames
	// We add extra skip for our own capture function
	USHORT uFrameCount = CaptureStackBackTrace(
		uSkipFrames + 1,  // +1 to skip this function
		static_cast<DWORD>(uMaxFrames),
		apFrames,
		NULL
	);

	return static_cast<u_int>(uFrameCount);
}

bool Zenith_Windows_Callstack::Symbolicate(void* pAddress, Zenith_CallstackFrame& xFrameOut)
{
	if (!s_bInitialised || pAddress == nullptr)
	{
		return false;
	}

	// Initialize output
	xFrameOut.m_pAddress = pAddress;
	xFrameOut.m_acSymbol[0] = '\0';
	xFrameOut.m_acFile[0] = '\0';
	xFrameOut.m_uLine = 0;

	// DbgHelp is not thread-safe, so we need to lock
	Zenith_ScopedMutexLock_T<Zenith_Windows_Mutex_T<false>> xLock(s_xSymMutex);

	DWORD64 ulAddress = reinterpret_cast<DWORD64>(pAddress);

	// Get symbol name
	char acSymbolBuffer[sizeof(SYMBOL_INFO) + uCALLSTACK_SYMBOL_MAX_LENGTH];
	SYMBOL_INFO* pxSymbol = reinterpret_cast<SYMBOL_INFO*>(acSymbolBuffer);
	pxSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	pxSymbol->MaxNameLen = uCALLSTACK_SYMBOL_MAX_LENGTH - 1;

	DWORD64 ulDisplacement = 0;
	if (SymFromAddr(s_hProcess, ulAddress, &ulDisplacement, pxSymbol))
	{
		strncpy_s(xFrameOut.m_acSymbol, uCALLSTACK_SYMBOL_MAX_LENGTH, pxSymbol->Name, _TRUNCATE);
	}
	else
	{
		snprintf(xFrameOut.m_acSymbol, uCALLSTACK_SYMBOL_MAX_LENGTH, "0x%llX", ulAddress);
	}

	// Get file and line info
	IMAGEHLP_LINE64 xLine;
	xLine.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
	DWORD ulLineDisplacement = 0;

	if (SymGetLineFromAddr64(s_hProcess, ulAddress, &ulLineDisplacement, &xLine))
	{
		strncpy_s(xFrameOut.m_acFile, uCALLSTACK_SYMBOL_MAX_LENGTH, xLine.FileName, _TRUNCATE);
		xFrameOut.m_uLine = xLine.LineNumber;
	}

	return true;
}

// Platform implementation functions called by Zenith_Callstack
void Zenith_Callstack::Platform_Initialise()
{
	Zenith_Windows_Callstack::Initialise();
}

void Zenith_Callstack::Platform_Shutdown()
{
	Zenith_Windows_Callstack::Shutdown();
}

u_int Zenith_Callstack::Platform_Capture(void** apFrames, u_int uMaxFrames, u_int uSkipFrames)
{
	return Zenith_Windows_Callstack::Capture(apFrames, uMaxFrames, uSkipFrames);
}

bool Zenith_Callstack::Platform_Symbolicate(void* pAddress, Zenith_CallstackFrame& xFrameOut)
{
	return Zenith_Windows_Callstack::Symbolicate(pAddress, xFrameOut);
}
