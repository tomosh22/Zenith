#include "Zenith.h"

#include "Zenith_Android_Callstack.h"
#include "Callstack/Zenith_Callstack.h"

#include <unwind.h>
#include <dlfcn.h>
#include <cxxabi.h>
#include <cstring>
#include <cstdio>

bool Zenith_Android_Callstack::s_bInitialised = false;
pthread_mutex_t Zenith_Android_Callstack::s_xMutex = PTHREAD_MUTEX_INITIALIZER;

// Context for unwinding
struct UnwindContext
{
	void** m_apFrames;
	u_int m_uMaxFrames;
	u_int m_uSkipFrames;
	u_int m_uCurrentFrame;
};

// Callback for _Unwind_Backtrace
static _Unwind_Reason_Code UnwindCallback(struct _Unwind_Context* pContext, void* pArg)
{
	UnwindContext* pxCtx = static_cast<UnwindContext*>(pArg);

	void* pIP = reinterpret_cast<void*>(_Unwind_GetIP(pContext));
	if (pIP == nullptr)
	{
		return _URC_END_OF_STACK;
	}

	// Skip frames if needed
	if (pxCtx->m_uSkipFrames > 0)
	{
		--pxCtx->m_uSkipFrames;
		return _URC_NO_REASON;
	}

	// Store frame if we have space
	if (pxCtx->m_uCurrentFrame < pxCtx->m_uMaxFrames)
	{
		pxCtx->m_apFrames[pxCtx->m_uCurrentFrame] = pIP;
		++pxCtx->m_uCurrentFrame;
		return _URC_NO_REASON;
	}

	return _URC_END_OF_STACK;
}

void Zenith_Android_Callstack::Initialise()
{
	if (s_bInitialised)
	{
		return;
	}

	s_bInitialised = true;
	Zenith_Log(LOG_CATEGORY_CORE, "Callstack capture initialised (Android)");
}

void Zenith_Android_Callstack::Shutdown()
{
	if (!s_bInitialised)
	{
		return;
	}

	s_bInitialised = false;
}

u_int Zenith_Android_Callstack::Capture(void** apFrames, u_int uMaxFrames, u_int uSkipFrames)
{
	if (!s_bInitialised || apFrames == nullptr || uMaxFrames == 0)
	{
		return 0;
	}

	UnwindContext xCtx;
	xCtx.m_apFrames = apFrames;
	xCtx.m_uMaxFrames = uMaxFrames;
	xCtx.m_uSkipFrames = uSkipFrames + 2;  // +2 to skip Capture and _Unwind_Backtrace
	xCtx.m_uCurrentFrame = 0;

	_Unwind_Backtrace(UnwindCallback, &xCtx);

	return xCtx.m_uCurrentFrame;
}

bool Zenith_Android_Callstack::Symbolicate(void* pAddress, Zenith_CallstackFrame& xFrameOut)
{
	if (!s_bInitialised || pAddress == nullptr)
	{
		return false;
	}

	// Initialize output
	xFrameOut.m_pAddress = pAddress;
	xFrameOut.m_acSymbol[0] = '\0';
	xFrameOut.m_acFile[0] = '\0';
	xFrameOut.m_uLine = 0;  // Line numbers not available on Android without debug info

	pthread_mutex_lock(&s_xMutex);

	Dl_info xInfo;
	if (dladdr(pAddress, &xInfo) && xInfo.dli_sname != nullptr)
	{
		// Try to demangle C++ symbols
		int iStatus = 0;
		char* szDemangled = abi::__cxa_demangle(xInfo.dli_sname, nullptr, nullptr, &iStatus);

		if (iStatus == 0 && szDemangled != nullptr)
		{
			strncpy(xFrameOut.m_acSymbol, szDemangled, uCALLSTACK_SYMBOL_MAX_LENGTH - 1);
			xFrameOut.m_acSymbol[uCALLSTACK_SYMBOL_MAX_LENGTH - 1] = '\0';
			free(szDemangled);
		}
		else
		{
			strncpy(xFrameOut.m_acSymbol, xInfo.dli_sname, uCALLSTACK_SYMBOL_MAX_LENGTH - 1);
			xFrameOut.m_acSymbol[uCALLSTACK_SYMBOL_MAX_LENGTH - 1] = '\0';
		}

		// Store the shared object file name
		if (xInfo.dli_fname != nullptr)
		{
			strncpy(xFrameOut.m_acFile, xInfo.dli_fname, uCALLSTACK_SYMBOL_MAX_LENGTH - 1);
			xFrameOut.m_acFile[uCALLSTACK_SYMBOL_MAX_LENGTH - 1] = '\0';
		}
	}
	else
	{
		// Fallback to address
		snprintf(xFrameOut.m_acSymbol, uCALLSTACK_SYMBOL_MAX_LENGTH, "0x%p", pAddress);
	}

	pthread_mutex_unlock(&s_xMutex);

	return true;
}

// Platform implementation functions called by Zenith_Callstack
void Zenith_Callstack::Platform_Initialise()
{
	Zenith_Android_Callstack::Initialise();
}

void Zenith_Callstack::Platform_Shutdown()
{
	Zenith_Android_Callstack::Shutdown();
}

u_int Zenith_Callstack::Platform_Capture(void** apFrames, u_int uMaxFrames, u_int uSkipFrames)
{
	return Zenith_Android_Callstack::Capture(apFrames, uMaxFrames, uSkipFrames);
}

bool Zenith_Callstack::Platform_Symbolicate(void* pAddress, Zenith_CallstackFrame& xFrameOut)
{
	return Zenith_Android_Callstack::Symbolicate(pAddress, xFrameOut);
}
