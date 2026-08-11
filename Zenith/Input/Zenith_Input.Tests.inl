#include "UnitTests/Zenith_UnitTests.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputSimulator.h"
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

// ============================================================================
// WP0 device foundations: the platform event FIFO, the drain, release edges,
// the lifecycle barrier, the B3 projection and the gamepad completion.
//
// Everything below drives Zenith_Input through DrainPendingPlatformEvents()
// rather than BeginFrame(), so no window, swapchain or engine is required: the
// drain is the whole of the frame contract's step 3 that owns device state.
// ============================================================================

namespace
{
	Zenith_InputEvent Zenith_MakeTestKeyEvent(Zenith_InputEventType eType, int32_t iCode)
	{
		Zenith_InputEvent xEvent;
		xEvent.m_eType = eType;
		xEvent.m_iCode = iCode;
		return xEvent;
	}

	Zenith_InputEvent Zenith_MakeTestTouchEvent(Zenith_InputEventType eType, int32_t iPointerId, float fX, float fY)
	{
		Zenith_InputEvent xEvent;
		xEvent.m_eType = eType;
		xEvent.m_iCode = iPointerId;
		xEvent.m_fX = fX;
		xEvent.m_fY = fY;
		xEvent.m_fAnchorX = fX;
		xEvent.m_fAnchorY = fY;
		return xEvent;
	}

	Zenith_InputEvent Zenith_MakeTestLifecycleEvent(Zenith_InputEventType eType)
	{
		Zenith_InputEvent xEvent;
		xEvent.m_eType = eType;
		return xEvent;
	}

	// Live-device probe for the RESYNC fail-safe: everything is up.
	bool Zenith_TestProbeAllKeysUp(void*, Zenith_KeyCode) { return false; }

	// The device-path assertions below read through the public query API, which
	// is simulator-intercepted. Unit tests run before the automated-test harness
	// enables the simulator, and every unit that enables it pairs a Disable --
	// assert that rather than assume it, so a leaked Enable fails loudly instead
	// of silently redirecting these reads to the simulator's tables.
	void Zenith_RequireSimulatorOff()
	{
		ZENITH_ASSERT_FALSE(Zenith_InputSimulator::IsEnabled(),
			"device-path input test requires the simulator to be OFF");
	}
}

// The drain is what turns queued platform events into this frame's edges, and it
// resets the previous frame's edges first. Both halves matter: a callback that
// wrote state directly (the pre-WP0 shape) could not survive a skipped frame,
// and an edge that outlived its frame would re-fire every consumer.
ZENITH_TEST(Input, PendingQueueDrainSetsEdgesAfterReset)
{
	Zenith_RequireSimulatorOff();
	Zenith_Input xInput;

	xInput.KeyPressedCallback(ZENITH_KEY_W);
	// Callbacks ONLY enqueue: nothing is visible before the drain.
	ZENITH_ASSERT_FALSE(xInput.IsKeyDown(ZENITH_KEY_W), "callback must not write state directly");
	ZENITH_ASSERT_FALSE(xInput.WasKeyPressedThisFrame(ZENITH_KEY_W), "no edge before the drain");
	ZENITH_ASSERT_EQ(xInput.GetPendingEvents().GetCount(), 1u, "event is queued");

	xInput.DrainPendingPlatformEvents();
	ZENITH_ASSERT_TRUE(xInput.IsKeyDown(ZENITH_KEY_W), "drain applies the press to the held table");
	ZENITH_ASSERT_TRUE(xInput.WasKeyPressedThisFrame(ZENITH_KEY_W), "drain raises the press edge");
	ZENITH_ASSERT_EQ(xInput.GetPendingEvents().GetCount(), 0u, "drain consumes the queue");

	// Next frame with nothing queued: held persists, the edge does not.
	xInput.DrainPendingPlatformEvents();
	ZENITH_ASSERT_TRUE(xInput.IsKeyDown(ZENITH_KEY_W), "held state persists across frames");
	ZENITH_ASSERT_FALSE(xInput.WasKeyPressedThisFrame(ZENITH_KEY_W), "press edge lives exactly one frame");
}

ZENITH_TEST(Input, ReleasedEdgeLivesOneFrame)
{
	Zenith_RequireSimulatorOff();
	Zenith_Input xInput;

	xInput.KeyPressedCallback(ZENITH_KEY_E);
	xInput.DrainPendingPlatformEvents();
	ZENITH_ASSERT_FALSE(xInput.WasKeyReleasedThisFrame(ZENITH_KEY_E), "no release edge on the press frame");

	xInput.KeyReleasedCallback(ZENITH_KEY_E);
	xInput.DrainPendingPlatformEvents();
	ZENITH_ASSERT_FALSE(xInput.IsKeyDown(ZENITH_KEY_E), "release clears the held table");
	ZENITH_ASSERT_TRUE(xInput.WasKeyReleasedThisFrame(ZENITH_KEY_E), "release edge raised on the release frame");

	xInput.DrainPendingPlatformEvents();
	ZENITH_ASSERT_FALSE(xInput.WasKeyReleasedThisFrame(ZENITH_KEY_E), "release edge lives exactly one frame");
}

// A sim run replaces the device sources wholesale, so real window events must not
// leak into it -- otherwise a test's input would depend on what the desktop was
// doing while it ran.
ZENITH_TEST(Input, PendingQueueDiscardedWhileSimActive)
{
	Zenith_Input xInput;

	xInput.KeyPressedCallback(ZENITH_KEY_R);
	ZENITH_ASSERT_EQ(xInput.GetPendingEvents().GetCount(), 1u, "event queued from the real device");

	Zenith_InputSimulator::Enable();
	xInput.BeginFrame();
	ZENITH_ASSERT_EQ(xInput.GetPendingEvents().GetCount(), 0u, "sim BeginFrame discards the FIFO");
	ZENITH_ASSERT_FALSE(xInput.IsKeyDown(ZENITH_KEY_R), "discarded event never reached the held table");
	Zenith_InputSimulator::Disable();
}

// The wheel used to be accumulated straight onto the input object by the GLFW
// callback, which meant it was reset and re-filled around the pump rather than
// riding the same ordered queue as everything else.
ZENITH_TEST(Input, WheelRidesPendingQueue)
{
	Zenith_RequireSimulatorOff();
	Zenith_Input xInput;

	xInput.MouseWheelCallback(0.0, 1.0);
	xInput.MouseWheelCallback(0.0, 2.5);
	ZENITH_ASSERT_EQ_FLOAT(xInput.GetMouseWheelDelta(), 0.0f, 1e-6, "wheel is not applied before the drain");

	xInput.DrainPendingPlatformEvents();
	ZENITH_ASSERT_EQ_FLOAT(xInput.GetMouseWheelDelta(), 3.5f, 1e-6, "drain accumulates every queued tick");

	xInput.DrainPendingPlatformEvents();
	ZENITH_ASSERT_EQ_FLOAT(xInput.GetMouseWheelDelta(), 0.0f, 1e-6, "wheel delta lives exactly one frame");
}

// Coalescing keeps the queue bounded under a fast finger, but it must not lose
// the fact that the finger MOVED: a drag out and back would otherwise look
// identical to a stationary press, and become a tap.
ZENITH_TEST(Input, CoalescedMoveRetainsMaxExcursion)
{
	Zenith_InputEventQueue xQueue;

	xQueue.Push(Zenith_MakeTestTouchEvent(INPUT_EVENT_TOUCH_DOWN, 0, 100.0f, 100.0f));
	xQueue.Push(Zenith_MakeTestTouchEvent(INPUT_EVENT_TOUCH_MOVE, 0, 100.0f, 140.0f));
	xQueue.Push(Zenith_MakeTestTouchEvent(INPUT_EVENT_TOUCH_MOVE, 0, 100.0f, 180.0f));
	xQueue.Push(Zenith_MakeTestTouchEvent(INPUT_EVENT_TOUCH_MOVE, 0, 100.0f, 140.0f));

	ZENITH_ASSERT_EQ(xQueue.GetCount(), 2u, "three moves for one pointer coalesce into one entry");
	const Zenith_InputEvent& xMove = xQueue.Get(1);
	ZENITH_ASSERT_EQ(static_cast<int>(xMove.m_eType), static_cast<int>(INPUT_EVENT_TOUCH_MOVE), "second entry is the move");
	ZENITH_ASSERT_EQ_FLOAT(xMove.m_fY, 140.0f, 1e-4, "coalesced move carries the LATEST position");
	ZENITH_ASSERT_EQ_FLOAT(xMove.m_fExcursion, 40.0f, 1e-4, "coalesced move retains the MAX excursion, not the last one");

	// A second pointer coalesces independently.
	xQueue.Push(Zenith_MakeTestTouchEvent(INPUT_EVENT_TOUCH_MOVE, 1, 10.0f, 10.0f));
	ZENITH_ASSERT_EQ(xQueue.GetCount(), 3u, "a different pointer does not merge into pointer 0's move");
}

// A gesture boundary must not be crossed by coalescing: merging a new gesture's
// move into the previous gesture's would reorder it before the UP.
ZENITH_TEST(Input, CoalescingStopsAtAGestureBoundary)
{
	Zenith_InputEventQueue xQueue;

	xQueue.Push(Zenith_MakeTestTouchEvent(INPUT_EVENT_TOUCH_MOVE, 0, 10.0f, 10.0f));
	xQueue.Push(Zenith_MakeTestTouchEvent(INPUT_EVENT_TOUCH_UP, 0, 10.0f, 10.0f));
	xQueue.Push(Zenith_MakeTestTouchEvent(INPUT_EVENT_TOUCH_DOWN, 0, 50.0f, 50.0f));
	xQueue.Push(Zenith_MakeTestTouchEvent(INPUT_EVENT_TOUCH_MOVE, 0, 60.0f, 50.0f));

	ZENITH_ASSERT_EQ(xQueue.GetCount(), 4u, "the new gesture's move must not merge past the UP");
	ZENITH_ASSERT_EQ_FLOAT(xQueue.Get(0).m_fX, 10.0f, 1e-4, "the first gesture's move is untouched");
}

// Overflow on a POLLABLE device: the held table is resynced against the live
// device, synthesizing the release the FIFO lost. Driven through the probe seam
// so the assertion does not depend on the physical keyboard's state.
ZENITH_TEST(Input, OverflowFailSafeWindowsResync)
{
	Zenith_RequireSimulatorOff();
	Zenith_Input xInput;
	xInput.m_eReconcilePolicy = INPUT_RECONCILE_RESYNC_POLLABLE;

	// A press whose release was lost leaves a key stuck down.
	xInput.KeyPressedCallback(ZENITH_KEY_W);
	xInput.DrainPendingPlatformEvents();
	ZENITH_ASSERT_TRUE(xInput.IsKeyDown(ZENITH_KEY_W), "key held before the resync");

	xInput.ReconcileHeldKeysAgainstDevice(&Zenith_TestProbeAllKeysUp, nullptr);
	ZENITH_ASSERT_FALSE(xInput.IsKeyDown(ZENITH_KEY_W), "resync clears a key the live device says is up");
	ZENITH_ASSERT_TRUE(xInput.WasKeyReleasedThisFrame(ZENITH_KEY_W), "resync SYNTHESIZES the missing release edge");
	// The drain logged the press; the resync appends the release after it.
	ZENITH_ASSERT_EQ(xInput.GetTransitionCount(), 2u, "the synthetic release is logged as a transition");
	ZENITH_ASSERT_EQ(static_cast<int>(xInput.GetTransition(1).m_eType), static_cast<int>(INPUT_EVENT_KEY_RELEASE),
		"and it is a release");

	// Evicting a state-bearing event is what raises the request in the first place.
	for (u_int32 u = 0; u < Zenith_InputEventQueue::uCAPACITY; u++)
	{
		xInput.EnqueuePlatformEvent(Zenith_MakeTestKeyEvent(INPUT_EVENT_KEY_PRESS, ZENITH_KEY_A));
	}
	ZENITH_ASSERT_FALSE(xInput.GetPendingEvents().IsReconcileRequested(), "a full-but-not-overflowed queue is fine");
	xInput.EnqueuePlatformEvent(Zenith_MakeTestKeyEvent(INPUT_EVENT_KEY_PRESS, ZENITH_KEY_A));
	ZENITH_ASSERT_TRUE(xInput.GetPendingEvents().IsReconcileRequested(), "evicting a PRESS requests a reconcile");
	ZENITH_ASSERT_EQ(xInput.GetPendingEvents().GetCount(), Zenith_InputEventQueue::uCAPACITY, "queue stays at capacity");
}

// Overflow on an EVENT-FED device: there is no live state to resync against, so
// the only safe answer is to cancel -- a phantom finger is worse than a dropped one.
ZENITH_TEST(Input, OverflowFailSafeAndroidCancels)
{
	Zenith_RequireSimulatorOff();
	Zenith_Input xInput;
	xInput.m_eReconcilePolicy = INPUT_RECONCILE_CANCEL_EVENT_FED;

	xInput.EnqueuePlatformEvent(Zenith_MakeTestKeyEvent(INPUT_EVENT_KEY_PRESS, ZENITH_KEY_W));
	xInput.EnqueuePlatformEvent(Zenith_MakeTestTouchEvent(INPUT_EVENT_TOUCH_DOWN, 0, 20.0f, 30.0f));
	xInput.DrainPendingPlatformEvents();
	ZENITH_ASSERT_TRUE(xInput.IsKeyDown(ZENITH_KEY_W), "key held before the overflow");
	ZENITH_ASSERT_TRUE(xInput.IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT), "projection holds LMB before the overflow");

	// Flood with events that are neither coalescable nor evictable, so the queue
	// reaches the LAST-RESORT branch: drop the oldest and raise the fail-safe.
	for (u_int32 u = 0; u <= Zenith_InputEventQueue::uCAPACITY; u++)
	{
		xInput.EnqueuePlatformEvent(Zenith_MakeTestKeyEvent(INPUT_EVENT_WHEEL, 0));
	}
	ZENITH_ASSERT_TRUE(xInput.GetPendingEvents().IsReconcileRequested(), "overflow requested the fail-safe");

	xInput.DrainPendingPlatformEvents();
	ZENITH_ASSERT_FALSE(xInput.IsKeyDown(ZENITH_KEY_W), "cancel releases every held key");
	ZENITH_ASSERT_FALSE(xInput.IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT), "cancel drops the projected button");
	ZENITH_ASSERT_TRUE(xInput.WasKeyReleasedThisFrame(ZENITH_KEY_W), "cancel emits the release edge");
	ZENITH_ASSERT_EQ(xInput.GetPrimaryPointerId(), -1, "cancel drops the primary pointer");
}

// The action layer resolves same-frame conflicts by ARRIVAL ORDER, so the log has
// to preserve it -- a set of per-key booleans could not answer "which came first".
ZENITH_TEST(Input, TransitionLogPreservesOrder)
{
	Zenith_Input xInput;

	xInput.KeyPressedCallback(ZENITH_KEY_A);
	xInput.KeyPressedCallback(ZENITH_KEY_B);
	xInput.KeyReleasedCallback(ZENITH_KEY_A);
	xInput.DrainPendingPlatformEvents();

	ZENITH_ASSERT_EQ(xInput.GetTransitionCount(), 3u, "every transition is logged");
	ZENITH_ASSERT_EQ(xInput.GetTransition(0).m_iCode, static_cast<int32_t>(ZENITH_KEY_A), "first is A");
	ZENITH_ASSERT_EQ(static_cast<int>(xInput.GetTransition(0).m_eType), static_cast<int>(INPUT_EVENT_KEY_PRESS), "first is a press");
	ZENITH_ASSERT_EQ(xInput.GetTransition(1).m_iCode, static_cast<int32_t>(ZENITH_KEY_B), "second is B");
	ZENITH_ASSERT_EQ(xInput.GetTransition(2).m_iCode, static_cast<int32_t>(ZENITH_KEY_A), "third is A again");
	ZENITH_ASSERT_EQ(static_cast<int>(xInput.GetTransition(2).m_eType), static_cast<int>(INPUT_EVENT_KEY_RELEASE), "third is the release");
	ZENITH_ASSERT_LT(xInput.GetTransition(0).m_uSequence, xInput.GetTransition(2).m_uSequence, "FIFO sequence is monotonic");

	xInput.DrainPendingPlatformEvents();
	ZENITH_ASSERT_EQ(xInput.GetTransitionCount(), 0u, "the log is per-frame");
}

// The whole point of an event FIFO over a per-frame poll: a press and release
// that both land inside one frame must produce BOTH edges. The old poll-based
// device layer produced neither.
ZENITH_TEST(Input, SameFrameTapProducesBothDeviceEdges)
{
	Zenith_RequireSimulatorOff();
	Zenith_Input xInput;

	xInput.MouseButtonPressedCallback(ZENITH_MOUSE_BUTTON_LEFT);
	xInput.MouseButtonReleasedCallback(ZENITH_MOUSE_BUTTON_LEFT);
	xInput.DrainPendingPlatformEvents();

	ZENITH_ASSERT_TRUE(xInput.WasKeyPressedThisFrame(ZENITH_MOUSE_BUTTON_LEFT), "same-frame tap raises the press edge");
	ZENITH_ASSERT_TRUE(xInput.WasKeyReleasedThisFrame(ZENITH_MOUSE_BUTTON_LEFT), "same-frame tap raises the release edge");
	ZENITH_ASSERT_FALSE(xInput.IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT), "and ends up not held");
	ZENITH_ASSERT_EQ(xInput.GetTransitionCount(), 2u, "both transitions are logged, in order");
}

// The drain sits after the swapchain acquire, so a skipped frame simply does not
// drain. Nothing may be lost: the FIFO and the retained pad snapshot both carry
// over, and a pad tap that started and ended during the skip still produces both
// edges on the next real frame.
ZENITH_TEST(Input, SkippedFrameRetainsFIFOAndPadEdges)
{
	Zenith_RequireSimulatorOff();
	Zenith_Input xInput;

	// Frame 1 pump: a key press and a pad press+release, no drain (frame skipped).
	xInput.KeyPressedCallback(ZENITH_KEY_SPACE);

	Zenith_GamepadSnapshot xPad;
	xPad.m_bConnected = true;
	xInput.SubmitGamepadSnapshot(0, xPad);                                  // connect
	xPad.m_abButtons[ZENITH_GAMEPAD_BUTTON_A] = true;
	xInput.SubmitGamepadSnapshot(0, xPad);                                  // press
	xPad.m_abButtons[ZENITH_GAMEPAD_BUTTON_A] = false;
	xInput.SubmitGamepadSnapshot(0, xPad);                                  // release

	ZENITH_ASSERT_GE(xInput.GetPendingEvents().GetCount(), 4u, "everything survives the skipped frame in the FIFO");

	xInput.DrainPendingPlatformEvents();
	ZENITH_ASSERT_TRUE(xInput.WasKeyPressedThisFrame(ZENITH_KEY_SPACE), "the key press survived the skip");
	ZENITH_ASSERT_TRUE(xInput.WasGamepadButtonPressedThisFrame(ZENITH_GAMEPAD_BUTTON_A, 0), "the pad press survived the skip");
	ZENITH_ASSERT_TRUE(xInput.WasGamepadButtonReleasedThisFrame(ZENITH_GAMEPAD_BUTTON_A, 0), "and so did its release");
	ZENITH_ASSERT_FALSE(xInput.IsGamepadButtonDown(ZENITH_GAMEPAD_BUTTON_A, 0), "net result is not held");
	ZENITH_ASSERT_TRUE(xInput.IsGamepadConnected(0), "the pad connect survived too");
}

// A lifecycle barrier voids everything staged before it. Without this, an app
// backgrounded mid-touch comes back with the finger still down.
ZENITH_TEST(Input, LifecycleBarrierCancelsStagedDown)
{
	Zenith_RequireSimulatorOff();
	Zenith_Input xInput;

	xInput.EnqueuePlatformEvent(Zenith_MakeTestTouchEvent(INPUT_EVENT_TOUCH_DOWN, 0, 40.0f, 60.0f));
	xInput.EnqueuePlatformEvent(Zenith_MakeTestLifecycleEvent(INPUT_EVENT_LIFECYCLE_RESET));
	xInput.DrainPendingPlatformEvents();

	// The DOWN is gone from the staged stream; the barrier itself stays, because
	// the pointer table has to know where the discontinuity was.
	bool bFoundDown = false;
	bool bFoundBarrier = false;
	for (u_int32 u = 0; u < xInput.GetStagedTouchCount(); u++)
	{
		const Zenith_InputEventType eType = xInput.GetStagedTouch(u).m_eType;
		if (eType == INPUT_EVENT_TOUCH_DOWN) bFoundDown = true;
		if (eType == INPUT_EVENT_LIFECYCLE_RESET) bFoundBarrier = true;
	}
	ZENITH_ASSERT_FALSE(bFoundDown, "the barrier discards state staged before it");
	ZENITH_ASSERT_TRUE(bFoundBarrier, "the barrier itself is preserved in the staged stream");

	ZENITH_ASSERT_FALSE(xInput.IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT), "the projected button is released");
	ZENITH_ASSERT_TRUE(xInput.WasKeyReleasedThisFrame(ZENITH_MOUSE_BUTTON_LEFT), "and its release edge is raised");
	ZENITH_ASSERT_FALSE(xInput.IsInputArmed(), "a barrier disarms input");
}

ZENITH_TEST(Input, DisarmedInputDiscardedUntilRearm)
{
	Zenith_RequireSimulatorOff();
	Zenith_Input xInput;

	xInput.EnqueuePlatformEvent(Zenith_MakeTestLifecycleEvent(INPUT_EVENT_LIFECYCLE_RESET));
	xInput.KeyPressedCallback(ZENITH_KEY_W);
	xInput.DrainPendingPlatformEvents();
	ZENITH_ASSERT_FALSE(xInput.IsInputArmed(), "disarmed by the barrier");
	ZENITH_ASSERT_FALSE(xInput.IsKeyDown(ZENITH_KEY_W), "an event after the barrier is DISCARDED");

	// Still disarmed on a later frame.
	xInput.KeyPressedCallback(ZENITH_KEY_W);
	xInput.DrainPendingPlatformEvents();
	ZENITH_ASSERT_FALSE(xInput.IsKeyDown(ZENITH_KEY_W), "still discarding while disarmed");

	// Re-arm, then the same event is honoured.
	xInput.EnqueuePlatformEvent(Zenith_MakeTestLifecycleEvent(INPUT_EVENT_LIFECYCLE_ARM));
	xInput.KeyPressedCallback(ZENITH_KEY_W);
	xInput.DrainPendingPlatformEvents();
	ZENITH_ASSERT_TRUE(xInput.IsInputArmed(), "ARM re-arms input");
	ZENITH_ASSERT_TRUE(xInput.IsKeyDown(ZENITH_KEY_W), "events after the re-arm are honoured again");
}

// Android BACK is system NAVIGATION, not a key: it reaches the transition log
// flagged as such and never touches the key domain.
ZENITH_TEST(Input, SystemBackEventCarried)
{
	Zenith_RequireSimulatorOff();
	Zenith_Input xInput;

	xInput.SystemBackCallback();
	xInput.DrainPendingPlatformEvents();

	ZENITH_ASSERT_EQ(xInput.GetTransitionCount(), 1u, "the back gesture reaches the transition log");
	ZENITH_ASSERT_EQ(static_cast<int>(xInput.GetTransition(0).m_eType), static_cast<int>(INPUT_EVENT_SYSTEM_BACK), "as its own event type");
	ZENITH_ASSERT_TRUE(xInput.GetTransition(0).m_bSystemNav, "flagged as system navigation");
	ZENITH_ASSERT_FALSE(xInput.WasKeyPressedThisFrame(0), "it is not a key press");
}

// B3 (PERMANENT): the first touch drives the mouse view, which is what keeps every
// unmigrated IsMouseButtonHeld consumer working on a touch device.
ZENITH_TEST(Input, PrimaryTouchProjectsOntoMouseView)
{
	Zenith_RequireSimulatorOff();
	Zenith_Input xInput;

	xInput.EnqueuePlatformEvent(Zenith_MakeTestTouchEvent(INPUT_EVENT_TOUCH_DOWN, 7, 120.0f, 240.0f));
	xInput.DrainPendingPlatformEvents();

	Zenith_Maths::Vector2_64 xPos;
	xInput.GetProjectedPointerPosition(xPos);
	ZENITH_ASSERT_EQ_FLOAT(xPos.x, 120.0, 1e-4, "the projection follows the primary touch (x)");
	ZENITH_ASSERT_EQ_FLOAT(xPos.y, 240.0, 1e-4, "the projection follows the primary touch (y)");
	ZENITH_ASSERT_TRUE(xInput.IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT), "a touch down holds the left button");
	ZENITH_ASSERT_TRUE(xInput.WasKeyPressedThisFrame(ZENITH_MOUSE_BUTTON_LEFT), "and raises its press edge");
	ZENITH_ASSERT_EQ(xInput.GetPrimaryPointerId(), 7, "the first touch becomes the primary pointer");

	// A SECOND finger must not steal the projection.
	xInput.EnqueuePlatformEvent(Zenith_MakeTestTouchEvent(INPUT_EVENT_TOUCH_DOWN, 9, 500.0f, 500.0f));
	xInput.EnqueuePlatformEvent(Zenith_MakeTestTouchEvent(INPUT_EVENT_TOUCH_MOVE, 7, 130.0f, 250.0f));
	xInput.DrainPendingPlatformEvents();
	xInput.GetProjectedPointerPosition(xPos);
	ZENITH_ASSERT_EQ_FLOAT(xPos.x, 130.0, 1e-4, "a secondary pointer does not move the mouse view");
	ZENITH_ASSERT_EQ(xInput.GetPrimaryPointerId(), 7, "the primary pointer is unchanged");

	xInput.EnqueuePlatformEvent(Zenith_MakeTestTouchEvent(INPUT_EVENT_TOUCH_UP, 7, 130.0f, 250.0f));
	xInput.DrainPendingPlatformEvents();
	ZENITH_ASSERT_FALSE(xInput.IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT), "lifting the primary releases the button");
	ZENITH_ASSERT_TRUE(xInput.WasKeyReleasedThisFrame(ZENITH_MOUSE_BUTTON_LEFT), "and raises its release edge");
}

ZENITH_TEST(Input, CancelAllInputReleasesEverything)
{
	Zenith_RequireSimulatorOff();
	Zenith_Input xInput;

	Zenith_GamepadSnapshot xPad;
	xPad.m_bConnected = true;
	xInput.SubmitGamepadSnapshot(0, xPad);
	xPad.m_abButtons[ZENITH_GAMEPAD_BUTTON_B] = true;
	xPad.m_afAxes[ZENITH_GAMEPAD_AXIS_LEFT_X] = 0.9f;
	xInput.SubmitGamepadSnapshot(0, xPad);
	xInput.KeyPressedCallback(ZENITH_KEY_LEFT_SHIFT);
	xInput.DrainPendingPlatformEvents();
	ZENITH_ASSERT_TRUE(xInput.IsGamepadButtonDown(ZENITH_GAMEPAD_BUTTON_B, 0), "pad button held before the cancel");

	xInput.CancelAllInput();
	ZENITH_ASSERT_FALSE(xInput.IsKeyDown(ZENITH_KEY_LEFT_SHIFT), "cancel releases held keys");
	ZENITH_ASSERT_FALSE(xInput.IsGamepadButtonDown(ZENITH_GAMEPAD_BUTTON_B, 0), "cancel releases held pad buttons");
	ZENITH_ASSERT_TRUE(xInput.WasGamepadButtonReleasedThisFrame(ZENITH_GAMEPAD_BUTTON_B, 0), "with a release edge");
	ZENITH_ASSERT_EQ_FLOAT(xInput.GetGamepadAxis(ZENITH_GAMEPAD_AXIS_LEFT_X, 0), 0.0f, 1e-6, "cancel zeroes the axes");
}

ZENITH_TEST(Input, ResetTransientForTestClearsDeviceState)
{
	Zenith_RequireSimulatorOff();
	Zenith_Input xInput;

	xInput.EnqueuePlatformEvent(Zenith_MakeTestTouchEvent(INPUT_EVENT_TOUCH_DOWN, 0, 5.0f, 5.0f));
	xInput.KeyPressedCallback(ZENITH_KEY_W);
	xInput.DrainPendingPlatformEvents();
	xInput.EnqueuePlatformEvent(Zenith_MakeTestLifecycleEvent(INPUT_EVENT_LIFECYCLE_RESET));
	xInput.DrainPendingPlatformEvents();
	xInput.KeyPressedCallback(ZENITH_KEY_S);   // queued but never drained

	xInput.ResetTransientForTest();
	ZENITH_ASSERT_FALSE(xInput.IsKeyDown(ZENITH_KEY_W), "held keys do not leak into the next test");
	ZENITH_ASSERT_EQ(xInput.GetPendingEvents().GetCount(), 0u, "a half-filled FIFO does not leak either");
	ZENITH_ASSERT_EQ(xInput.GetTransitionCount(), 0u, "nor the transition log");
	ZENITH_ASSERT_TRUE(xInput.IsInputArmed(), "the next test starts armed");
	ZENITH_ASSERT_EQ(xInput.GetPrimaryPointerId(), -1, "and with no live pointer");
}

// ---------------------------------------------------------------------------
// Gamepad
// ---------------------------------------------------------------------------

// The poll-diff is the whole of the pad's event production: level state in,
// transitions out.
ZENITH_TEST(Input, GamepadPollDiffProducesTransitions)
{
	Zenith_RequireSimulatorOff();
	Zenith_Input xInput;

	Zenith_GamepadSnapshot xPad;
	xPad.m_bConnected = true;
	xPad.m_abButtons[ZENITH_GAMEPAD_BUTTON_A] = true;
	xInput.SubmitGamepadSnapshot(0, xPad);
	xInput.DrainPendingPlatformEvents();
	// A pad that arrives with a button already held has not just been pressed.
	ZENITH_ASSERT_FALSE(xInput.WasGamepadButtonPressedThisFrame(ZENITH_GAMEPAD_BUTTON_A, 0),
		"connect adopts the arriving state without inventing a press");
	ZENITH_ASSERT_TRUE(xInput.IsGamepadConnected(0), "the pad is connected");

	xPad.m_abButtons[ZENITH_GAMEPAD_BUTTON_A] = false;
	xPad.m_abButtons[ZENITH_GAMEPAD_BUTTON_B] = true;
	xInput.SubmitGamepadSnapshot(0, xPad);
	xInput.DrainPendingPlatformEvents();
	ZENITH_ASSERT_TRUE(xInput.WasGamepadButtonPressedThisFrame(ZENITH_GAMEPAD_BUTTON_B, 0), "B's press edge");
	ZENITH_ASSERT_TRUE(xInput.IsGamepadButtonDown(ZENITH_GAMEPAD_BUTTON_B, 0), "B is held");
	// A is diffed against the adopted baseline, so its release IS a transition.
	ZENITH_ASSERT_TRUE(xInput.WasGamepadButtonReleasedThisFrame(ZENITH_GAMEPAD_BUTTON_A, 0), "A's release edge");

	xInput.DrainPendingPlatformEvents();
	ZENITH_ASSERT_FALSE(xInput.WasGamepadButtonPressedThisFrame(ZENITH_GAMEPAD_BUTTON_B, 0), "pad edges live one frame");
	ZENITH_ASSERT_TRUE(xInput.IsGamepadButtonDown(ZENITH_GAMEPAD_BUTTON_B, 0), "held state persists");
}

ZENITH_TEST(Input, GamepadDisconnectSynthesizesReleases)
{
	Zenith_RequireSimulatorOff();
	Zenith_Input xInput;

	Zenith_GamepadSnapshot xPad;
	xPad.m_bConnected = true;
	xInput.SubmitGamepadSnapshot(0, xPad);
	xPad.m_abButtons[ZENITH_GAMEPAD_BUTTON_X] = true;
	xPad.m_afAxes[ZENITH_GAMEPAD_AXIS_RIGHT_TRIGGER] = 0.8f;
	xInput.SubmitGamepadSnapshot(0, xPad);
	xInput.DrainPendingPlatformEvents();
	ZENITH_ASSERT_TRUE(xInput.IsGamepadButtonDown(ZENITH_GAMEPAD_BUTTON_X, 0), "held before the yank");

	// Pad yanked: nothing will ever send its release.
	xInput.SubmitGamepadSnapshot(0, Zenith_GamepadSnapshot());
	xInput.DrainPendingPlatformEvents();

	ZENITH_ASSERT_FALSE(xInput.IsGamepadConnected(0), "the pad reads disconnected");
	ZENITH_ASSERT_FALSE(xInput.IsGamepadButtonDown(ZENITH_GAMEPAD_BUTTON_X, 0), "no button stays held");
	ZENITH_ASSERT_TRUE(xInput.WasGamepadButtonReleasedThisFrame(ZENITH_GAMEPAD_BUTTON_X, 0), "the release is SYNTHESIZED");
	ZENITH_ASSERT_EQ_FLOAT(xInput.GetGamepadRightTrigger(0), 0.0f, 1e-6, "and the axes rest at zero");
}

// The pre-WP0 bug in one test: the getter did (axis + 1) * 0.5, so a pad with no
// axis data at all reported both triggers half-pulled.
ZENITH_TEST(Input, DisconnectedTriggerRestsAtZero)
{
	Zenith_RequireSimulatorOff();
	Zenith_Input xInput;

	ZENITH_ASSERT_FALSE(xInput.IsGamepadConnected(0), "no pad in a fresh device layer");
	ZENITH_ASSERT_EQ_FLOAT(xInput.GetGamepadLeftTrigger(0), 0.0f, 1e-6, "a disconnected LEFT trigger rests at 0, not 0.5");
	ZENITH_ASSERT_EQ_FLOAT(xInput.GetGamepadRightTrigger(0), 0.0f, 1e-6, "a disconnected RIGHT trigger rests at 0, not 0.5");

	// The normalisation itself happens ONCE, at the device layer.
	ZENITH_ASSERT_EQ_FLOAT(Zenith_Input::NormaliseTriggerAxis(-1.0f), 0.0f, 1e-6, "GLFW rest (-1) is 0");
	ZENITH_ASSERT_EQ_FLOAT(Zenith_Input::NormaliseTriggerAxis(0.0f), 0.5f, 1e-6, "GLFW half (0) is 0.5");
	ZENITH_ASSERT_EQ_FLOAT(Zenith_Input::NormaliseTriggerAxis(1.0f), 1.0f, 1e-6, "GLFW full (1) is 1");
}

// The deadzone is RADIAL: a per-axis test carves a square hole out of the stick
// and leaks diagonal drift that is well inside the dead radius.
ZENITH_TEST(Input, StickDeadzoneIsRadial)
{
	ZENITH_ASSERT_EQ_FLOAT(Zenith_Input::ApplyRadialDeadzone(0.10f, 0.0f), 0.0f, 1e-6, "inside the radius is dead");
	ZENITH_ASSERT_EQ_FLOAT(Zenith_Input::ApplyRadialDeadzone(0.30f, 0.0f), 0.30f, 1e-6, "outside the radius passes through");
	// 0.12 on each axis is inside the 0.15 radius but outside a 0.15 per-axis box test.
	ZENITH_ASSERT_EQ_FLOAT(Zenith_Input::ApplyRadialDeadzone(0.12f, 0.12f), 0.12f, 1e-6,
		"a diagonal past the radius survives");
	ZENITH_ASSERT_EQ_FLOAT(Zenith_Input::ApplyRadialDeadzone(0.10f, 0.10f), 0.0f, 1e-6,
		"a diagonal inside the radius is dead even though each axis is not");
}

// ---------------------------------------------------------------------------
// Simulator
// ---------------------------------------------------------------------------

ZENITH_TEST(Input, SimulatedReleaseEdge)
{
	Zenith_Input xInput;
	Zenith_InputSimulator::Enable();

	Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_F);
	ZENITH_ASSERT_TRUE(xInput.WasKeyPressedThisFrame(ZENITH_KEY_F), "simulated press edge");
	ZENITH_ASSERT_FALSE(xInput.WasKeyReleasedThisFrame(ZENITH_KEY_F), "no release edge yet");

	Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_F);
	ZENITH_ASSERT_TRUE(xInput.WasKeyReleasedThisFrame(ZENITH_KEY_F), "simulated release edge");
	ZENITH_ASSERT_FALSE(xInput.IsKeyDown(ZENITH_KEY_F), "and the key is no longer held");

	// A SimulateKeyPress auto-releases on the next frame boundary, and that
	// release must carry an edge too -- the UI widgets' click-on-release now
	// depends on it.
	Zenith_InputSimulator::EndTestFrame();
	Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_G);
	ZENITH_ASSERT_TRUE(xInput.WasKeyPressedThisFrame(ZENITH_KEY_G), "press frame");
	Zenith_InputSimulator::ProcessAutoReleases();
	ZENITH_ASSERT_FALSE(xInput.WasKeyPressedThisFrame(ZENITH_KEY_G), "press edge cleared on the next frame");
	ZENITH_ASSERT_TRUE(xInput.WasKeyReleasedThisFrame(ZENITH_KEY_G), "auto-release raises the release edge");

	Zenith_InputSimulator::Disable();
}

ZENITH_TEST(Input, SimulatedGamepadCanonicalDomains)
{
	Zenith_Input xInput;
	Zenith_InputSimulator::Enable();

	Zenith_InputSimulator::SimulateGamepadConnected(true, 0);
	ZENITH_ASSERT_TRUE(xInput.IsGamepadConnected(0), "simulated pad reads connected");

	// Sticks live on [-1,1] with the radial deadzone applied at the GETTER.
	Zenith_InputSimulator::SimulateGamepadStick(ZENITH_GAMEPAD_AXIS_LEFT_X, 0.9f, -0.4f, 0);
	ZENITH_ASSERT_EQ_FLOAT(xInput.GetGamepadAxis(ZENITH_GAMEPAD_AXIS_LEFT_X, 0), 0.9f, 1e-6, "stick x passes through");
	ZENITH_ASSERT_EQ_FLOAT(xInput.GetGamepadAxis(ZENITH_GAMEPAD_AXIS_LEFT_Y, 0), -0.4f, 1e-6, "stick y passes through");

	Zenith_InputSimulator::SimulateGamepadStick(ZENITH_GAMEPAD_AXIS_LEFT_X, 0.05f, 0.05f, 0);
	float fX = 1.0f;
	float fY = 1.0f;
	xInput.GetGamepadLeftStick(fX, fY, 0);
	ZENITH_ASSERT_EQ_FLOAT(fX, 0.0f, 1e-6, "a simulated stick inside the deadzone reads zero (x)");
	ZENITH_ASSERT_EQ_FLOAT(fY, 0.0f, 1e-6, "a simulated stick inside the deadzone reads zero (y)");

	// Injection CLAMPS to the canonical domain rather than trusting the caller.
	Zenith_InputSimulator::SimulateGamepadAxis(ZENITH_GAMEPAD_AXIS_RIGHT_X, 4.0f, 0);
	ZENITH_ASSERT_EQ_FLOAT(xInput.GetGamepadAxis(ZENITH_GAMEPAD_AXIS_RIGHT_X, 0), 1.0f, 1e-6, "stick clamps to 1");

	// Triggers are ALREADY canonical [0,1]: no second normalisation at the getter.
	Zenith_InputSimulator::SimulateGamepadAxis(ZENITH_GAMEPAD_AXIS_LEFT_TRIGGER, 0.42f, 0);
	ZENITH_ASSERT_EQ_FLOAT(xInput.GetGamepadLeftTrigger(0), 0.42f, 1e-6, "trigger reads back exactly as injected");
	Zenith_InputSimulator::SimulateGamepadAxis(ZENITH_GAMEPAD_AXIS_RIGHT_TRIGGER, -3.0f, 0);
	ZENITH_ASSERT_EQ_FLOAT(xInput.GetGamepadRightTrigger(0), 0.0f, 1e-6, "trigger clamps to its 0 rest");

	// Buttons + the disconnect release synthesis, all through the intercepted getters.
	Zenith_InputSimulator::SimulateGamepadButtonDown(ZENITH_GAMEPAD_BUTTON_A, 0);
	ZENITH_ASSERT_TRUE(xInput.IsGamepadButtonDown(ZENITH_GAMEPAD_BUTTON_A, 0), "simulated pad button held");
	ZENITH_ASSERT_TRUE(xInput.WasGamepadButtonPressedThisFrame(ZENITH_GAMEPAD_BUTTON_A, 0), "with a press edge");
	Zenith_InputSimulator::SimulateGamepadConnected(false, 0);
	ZENITH_ASSERT_FALSE(xInput.IsGamepadButtonDown(ZENITH_GAMEPAD_BUTTON_A, 0), "a simulated yank releases it");
	ZENITH_ASSERT_TRUE(xInput.WasGamepadButtonReleasedThisFrame(ZENITH_GAMEPAD_BUTTON_A, 0), "with a release edge");
	ZENITH_ASSERT_EQ_FLOAT(xInput.GetGamepadLeftTrigger(0), 0.0f, 1e-6, "and zeroed axes");

	Zenith_InputSimulator::Disable();
}

// Step 7 of the frame contract: what a test Step injected becomes ordered
// transitions on the device layer, so the action layer has exactly one path to
// read regardless of whether the input was real or simulated.
ZENITH_TEST(Input, SimulatorInjectionBecomesOrderedTransitions)
{
	Zenith_Input xInput;
	Zenith_InputSimulator::Enable();

	Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_1);
	Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_1);
	Zenith_InputSimulator::SimulateTouchDown(3, 11.0f, 22.0f);
	ZENITH_ASSERT_EQ(Zenith_InputSimulator::GetPendingInjectionCount(), 3u, "injections queue up during the Step");

	// ApplyPendingInjection walks g_xEngine; drive the same seam directly so the
	// unit needs no engine.
	xInput.AppendInjectedEvent(Zenith_MakeTestKeyEvent(INPUT_EVENT_KEY_PRESS, ZENITH_KEY_1));
	xInput.AppendInjectedEvent(Zenith_MakeTestKeyEvent(INPUT_EVENT_KEY_RELEASE, ZENITH_KEY_1));
	xInput.AppendInjectedEvent(Zenith_MakeTestTouchEvent(INPUT_EVENT_TOUCH_DOWN, 3, 11.0f, 22.0f));

	ZENITH_ASSERT_EQ(xInput.GetTransitionCount(), 3u, "press, release, and the projected touch press");
	ZENITH_ASSERT_EQ(static_cast<int>(xInput.GetTransition(0).m_eType), static_cast<int>(INPUT_EVENT_KEY_PRESS), "order preserved (press)");
	ZENITH_ASSERT_EQ(static_cast<int>(xInput.GetTransition(1).m_eType), static_cast<int>(INPUT_EVENT_KEY_RELEASE), "order preserved (release)");
	ZENITH_ASSERT_EQ(xInput.GetStagedTouchCount(), 1u, "the injected touch reaches the staged stream");
	ZENITH_ASSERT_EQ(xInput.GetPrimaryPointerId(), 3, "and the B3 projection");

	Zenith_InputSimulator::Disable();
}
