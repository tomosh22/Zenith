#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR
#include "Core/Zenith_AutomatedTest.h"
#endif

int main()
{
	Zenith_Core::Zenith_Main();
#ifdef ZENITH_INPUT_SIMULATOR
	// EXT-3a: propagate the test runner's pending exit code so CI / Claude Code
	// can branch on the process exit (0 pass, 1 fail, 2 not found, 3 setup err).
	return Zenith_AutomatedTestRunner::GetPendingExitCode();
#else
	return 0;
#endif
}
