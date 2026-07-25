#include "Zenith.h"

#include <cstdio>    // setvbuf / stdout / stderr
#include <cstdlib>   // _set_abort_behavior
#include <crtdbg.h>  // _CrtSetReportMode / _CrtSetReportFile

// <Windows.h> for SetErrorMode, via the shared Win32 include (GLFW APIENTRY guard).
#include "Core/Zenith_Win32.h"

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
	// hanging. Applied unconditionally in Null builds -- those ARE the CI/headless
	// builds, where a modal dialog is an infinite hang. Windowed dev runs keep the
	// dialogs (they are useful there).
	void HardenHeadlessFatalErrorHandling()
	{
		// Unbuffer stdout/stderr. Redirected to a pipe (CI captures both), the CRT
		// defaults to FULL buffering, so on a crash the in-flight buffer is lost and
		// the captured log stops at the last flush -- which is exactly why the
		// engine-gate boot log ended at "AssetRegistry initialized" with the real
		// crash (in the tool asset export phase that runs right after) invisible.
		// Unbuffered => every line reaches the pipe immediately, so a crash is
		// diagnosable from the captured log.
		setvbuf(stdout, nullptr, _IONBF, 0);
		setvbuf(stderr, nullptr, _IONBF, 0);
#ifdef _DEBUG
		// The debug-heap corrupted-block check + failed asserts (the modal-dialog
		// sources) only exist in the debug CRT. In a release CRT _CrtSetReportMode /
		// _CrtSetReportFile are no-op macros that ignore their arguments, so this
		// loop would leave eReport unused (C4189 -> C2220 under /WX). Guard it.
		for (int eReport : { _CRT_WARN, _CRT_ERROR, _CRT_ASSERT })
		{
			_CrtSetReportMode(eReport, _CRTDBG_MODE_FILE);
			_CrtSetReportFile(eReport, _CRTDBG_FILE_STDERR);
		}
#endif
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
	// Headless is build-time now, so this is a compile-time decision -- no argv
	// scan, and no way for a CI invocation to forget the flag and hang on a modal
	// dialog instead of failing.
#ifdef ZENITH_NULL_RENDERER
	HardenHeadlessFatalErrorHandling();
#endif

	Zenith_Core::Zenith_Main();
#ifdef ZENITH_INPUT_SIMULATOR
	// EXT-3a: propagate the test runner's pending exit code so CI / Claude Code
	// can branch on the process exit (0 pass, 1 fail, 2 not found, 3 setup err).
	return Zenith_AutomatedTestRunner::GetPendingExitCode();
#else
	return 0;
#endif
}
