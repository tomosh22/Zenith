#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"

// ============================================================================
// Hello_Test - minimal harness smoke test (EXT-3a milestone M1).
//
// Verifies:
//   - CLI flag --automated-test resolves the test by name
//   - Setup runs once after scene+OnStart settle
//   - Step is called for the configured frame count
//   - Verify is called once after Step terminates
//   - Pending exit code is 0 on pass
// ============================================================================

static int g_iSetupCalls    = 0;
static int g_iStepCalls     = 0;
static int g_iVerifyCalls   = 0;

static void Setup_Hello()
{
	++g_iSetupCalls;
}

static bool Step_Hello(int iFrame)
{
	++g_iStepCalls;
	return iFrame < 10; // run for 10 frames then stop
}

static bool Verify_Hello()
{
	++g_iVerifyCalls;
	return g_iSetupCalls == 1
	    && g_iStepCalls  >= 10
	    && g_iVerifyCalls == 1;
}

static const Zenith_AutomatedTest g_xHelloTest = {
	"Hello_Test",
	&Setup_Hello,
	&Step_Hello,
	&Verify_Hello,
	120 // max-frames safety net
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xHelloTest);

#endif // ZENITH_INPUT_SIMULATOR
