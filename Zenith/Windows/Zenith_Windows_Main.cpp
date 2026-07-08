#include "Zenith.h"

#include <cstring>
#include <cstdlib>   // _set_abort_behavior
#include <crtdbg.h>  // _CrtSetReportMode / _CrtSetReportFile

// <Windows.h> for SetErrorMode. GLFW (pulled via the PCH) leaves APIENTRY
// defined; undef before <windows.h> redefines it (mirrors Zenith_TestFramework.cpp).
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <Windows.h>

#ifdef ZENITH_INPUT_SIMULATOR
#include "Core/Zenith_AutomatedTest.h"
#endif

namespace
{
	// Headless (CI / automated) hardening. In a Debug build a fatal CRT diagnostic
	// -- a failed assert, the debug-heap's corrupted-block check on free, or abort()
	// from an unhandled exception -- defaults to a MODAL MessageBox. With no
	// interactive user to dismiss it the process blocks on the dialog until the CI
	// watchdog kills it: that is exactly the intermittent engine-gate "hang" (a
	// heap-corruption check firing mid-boot, then waiting forever on the dialog).
	// Route CRT diagnostics to stderr and kill the OS error boxes so a fatal error
	// crashes FAST + diagnosably (non-zero exit + a logged message) instead of
	// hanging. Interactive dev runs keep the dialogs (they are useful there).
	void HardenHeadlessFatalErrorHandling()
	{
		for (int eReport : { _CRT_WARN, _CRT_ERROR, _CRT_ASSERT })
		{
			_CrtSetReportMode(eReport, _CRTDBG_MODE_FILE);
			_CrtSetReportFile(eReport, _CRTDBG_FILE_STDERR);
		}
		// abort() must not pop the "abnormal termination" dialog either.
		_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
		// Suppress the OS-level WER / GPF / critical-error message boxes (e.g. an
		// unhandled access violation) so those fail fast too.
		SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
	}
}

int main()
{
	// Do this BEFORE any engine work: the corruption check can fire during boot.
	// Zenith_CommandLine is not parsed yet, so scan the raw argv for --headless.
	for (int i = 1; i < __argc; ++i)
	{
		if (std::strcmp(__argv[i], "--headless") == 0)
		{
			HardenHeadlessFatalErrorHandling();
			break;
		}
	}

	Zenith_Core::Zenith_Main();
#ifdef ZENITH_INPUT_SIMULATOR
	// EXT-3a: propagate the test runner's pending exit code so CI / Claude Code
	// can branch on the process exit (0 pass, 1 fail, 2 not found, 3 setup err).
	return Zenith_AutomatedTestRunner::GetPendingExitCode();
#else
	return 0;
#endif
}
