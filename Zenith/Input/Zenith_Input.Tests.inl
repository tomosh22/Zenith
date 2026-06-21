#include "UnitTests/Zenith_UnitTests.h"
#include "Input/Zenith_Input.h"
#include "Maths/Zenith_Maths.h"

// Coverage for the one-shot mouse-discontinuity skip added by the non-tools mouse-look
// fix. Drives the window-free UpdateMouseDeltaFromPosition (BeginFrame's core) with
// synthetic cursor positions so the one-shot semantics are testable headless:
//   - a flagged frame zeroes the delta (suppresses the capture/release teleport spike)
//     and resyncs the baseline to the jumped-to position;
//   - the flag self-clears, so the NEXT frame resumes computing real deltas.

ZENITH_TEST(Input, MouseDiscontinuityOneShot)
{
	Zenith_Input xInput;

	// First frame establishes the baseline (delta forced to zero).
	xInput.UpdateMouseDeltaFromPosition(Zenith_Maths::Vector2_64(100.0, 100.0), false);
	ZENITH_ASSERT_EQ_FLOAT(xInput.m_xMouseDelta.x, 0.0, 1e-6, "first-frame delta zeroed");
	ZENITH_ASSERT_EQ_FLOAT(xInput.m_xMouseDelta.y, 0.0, 1e-6, "first-frame delta zeroed");

	// Normal frame: delta = current - last.
	xInput.UpdateMouseDeltaFromPosition(Zenith_Maths::Vector2_64(150.0, 120.0), false);
	ZENITH_ASSERT_EQ_FLOAT(xInput.m_xMouseDelta.x, 50.0, 1e-6, "normal delta x = current-last");
	ZENITH_ASSERT_EQ_FLOAT(xInput.m_xMouseDelta.y, 20.0, 1e-6, "normal delta y = current-last");

	// Discontinuity raised -> the next frame must suppress the spike despite a big
	// cursor jump (capture/release teleport) AND resync the baseline to it.
	xInput.NotifyMouseDiscontinuity();
	xInput.UpdateMouseDeltaFromPosition(Zenith_Maths::Vector2_64(900.0, 700.0), false);
	ZENITH_ASSERT_EQ_FLOAT(xInput.m_xMouseDelta.x, 0.0, 1e-6, "discontinuity frame suppresses the spike (x)");
	ZENITH_ASSERT_EQ_FLOAT(xInput.m_xMouseDelta.y, 0.0, 1e-6, "discontinuity frame suppresses the spike (y)");

	// One-shot: the FOLLOWING frame resumes real deltas from the resynced baseline (900,700).
	xInput.UpdateMouseDeltaFromPosition(Zenith_Maths::Vector2_64(910.0, 695.0), false);
	ZENITH_ASSERT_EQ_FLOAT(xInput.m_xMouseDelta.x, 10.0, 1e-6, "post-discontinuity delta resumes (x)");
	ZENITH_ASSERT_EQ_FLOAT(xInput.m_xMouseDelta.y, -5.0, 1e-6, "post-discontinuity delta resumes (y)");
}

ZENITH_TEST(Input, LeftSimModeSkipsOneFrame)
{
	Zenith_Input xInput;

	xInput.UpdateMouseDeltaFromPosition(Zenith_Maths::Vector2_64(0.0, 0.0), false);   // baseline
	// bJustLeftSimMode true -> that frame's delta is zeroed + baseline resynced.
	xInput.UpdateMouseDeltaFromPosition(Zenith_Maths::Vector2_64(300.0, 300.0), true);
	ZENITH_ASSERT_EQ_FLOAT(xInput.m_xMouseDelta.x, 0.0, 1e-6, "left-sim frame zeroes the delta");
	// Next normal frame computes from the resynced baseline (300).
	xInput.UpdateMouseDeltaFromPosition(Zenith_Maths::Vector2_64(305.0, 300.0), false);
	ZENITH_ASSERT_EQ_FLOAT(xInput.m_xMouseDelta.x, 5.0, 1e-6, "resumes from resynced baseline after left-sim");
}
