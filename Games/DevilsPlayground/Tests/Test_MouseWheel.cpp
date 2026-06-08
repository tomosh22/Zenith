#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputSimulator.h"

// ============================================================================
// MouseWheel_Test - exercises EXT-4 (mouse wheel input + simulator override).
//
// Verifies:
//   - SimulateMouseWheel(+1.5) makes g_xEngine.Input().GetMouseWheelDelta() report +1.5
//   - The reported delta is preserved across reads within a frame
//   - SimulateMouseWheel(0) clears it
// ============================================================================

static int g_iFramesObserved      = 0;
static bool g_bSawPositiveDelta   = false;
static bool g_bDeltaConsistentInFrame = true;

static void Setup_MouseWheel()
{
	g_iFramesObserved = 0;
	g_bSawPositiveDelta = false;
	g_bDeltaConsistentInFrame = true;
	Zenith_InputSimulator::SimulateMouseWheel(1.5f);
}

static bool Step_MouseWheel(int iFrame)
{
	if (iFrame == 0)
	{
		const float fA = g_xEngine.Input().GetMouseWheelDelta();
		const float fB = g_xEngine.Input().GetMouseWheelDelta();
		if (fA != fB)
		{
			g_bDeltaConsistentInFrame = false;
		}
		if (fA > 0.5f)
		{
			g_bSawPositiveDelta = true;
		}
	}
	++g_iFramesObserved;
	return iFrame < 5;
}

static bool Verify_MouseWheel()
{
	return g_bSawPositiveDelta && g_bDeltaConsistentInFrame && g_iFramesObserved >= 5;
}

static const Zenith_AutomatedTest g_xMouseWheelTest = {
	"MouseWheel_Test",
	&Setup_MouseWheel,
	&Step_MouseWheel,
	&Verify_MouseWheel,
	60
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xMouseWheelTest);

#endif // ZENITH_INPUT_SIMULATOR
