#include "UnitTests/Zenith_UnitTests.h"
#include "Input/Zenith_InputActions.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_Pointers.h"
#include "Maths/Zenith_Maths.h"

// ============================================================================
// WP2: the action layer (B4 / B5 / B8).
//
// Everything here drives a LOCAL Zenith_InputActions bound to a LOCAL
// Zenith_Input + Zenith_Pointers (B13), so no window, engine or swapchain is
// required and a game's registrations on the engine's own instance are never
// touched.
//
// One test "frame" is the real frame contract, minus the engine:
//   BeginFrame()  -> the device-layer frame boundary (edges + transition log)
//   ...events...  -> the drain / injection (steps 3 and 7)
//   UpdateProfile -> step 8
//   FinalizeReservedUI / FinalizeGameplay -> steps 10b and 10e
// ============================================================================

namespace
{
	// Test action ids. Game ids start at uINPUT_ACTION_FIRST_GAME_ID; anything
	// below that is engine-reserved and asserts on a game registration.
	static constexpr Zenith_InputActionID uTEST_ACTION_A    = 16;
	static constexpr Zenith_InputActionID uTEST_ACTION_B    = 17;
	static constexpr Zenith_InputActionID uTEST_ACTION_AXIS = 18;

	static constexpr u_int8 uTEST_PROFILE_ALL      = 1;
	static constexpr u_int8 uTEST_PROFILE_KEYBOARD = 2;
	static constexpr u_int8 uTEST_PROFILE_GAMEPAD  = 3;
	static constexpr u_int8 uTEST_PROFILE_EMPTY    = 4;

	struct Zenith_ActionTestRig
	{
		Zenith_Input        m_xInput;
		Zenith_Pointers     m_xPointers;
		Zenith_InputActions m_xActions;

		Zenith_ActionTestRig()
		{
			m_xActions.Initialise(m_xInput, m_xPointers);
		}

		// One profile carrying every scheme: the default for tests that are not
		// ABOUT masks.
		void RegisterAllSchemeProfile()
		{
			m_xActions.RegisterProfile(uTEST_PROFILE_ALL, "TestAll",
				uINPUT_SCHEME_MASK_KEYBOARD | uINPUT_SCHEME_MASK_MOUSE
				| uINPUT_SCHEME_MASK_TOUCH | uINPUT_SCHEME_MASK_GAMEPAD);
		}

		// Two DISJOINT profiles, which is what the auto-switch needs to have a
		// question to answer at all.
		void RegisterSplitProfiles()
		{
			m_xActions.RegisterProfile(uTEST_PROFILE_KEYBOARD, "TestKeyboard", uINPUT_SCHEME_MASK_KEYBOARD);
			m_xActions.RegisterProfile(uTEST_PROFILE_GAMEPAD,  "TestGamepad",  uINPUT_SCHEME_MASK_GAMEPAD);
		}

		void BeginFrame()
		{
			// The device-layer frame boundary: per-frame edges and the ordered
			// transition log both live exactly one frame.
			m_xInput.DrainPendingPlatformEvents();
			m_xPointers.BeginFrame(1.0f);
		}

		void CloseFrame()
		{
			m_xActions.UpdateProfile();
			m_xActions.FinalizeReservedUI();
			m_xActions.FinalizeGameplay();
		}

		void RaiseEvent(Zenith_InputEventType eType, int32_t iCode, int32_t iDevice = 0, bool bSystemNav = false)
		{
			Zenith_InputEvent xEvent;
			xEvent.m_eType      = eType;
			xEvent.m_iCode      = iCode;
			xEvent.m_iDevice    = iDevice;
			xEvent.m_bSystemNav = bSystemNav;
			m_xInput.AppendInjectedEvent(xEvent);
		}

		void KeyDown(int32_t iKey)   { RaiseEvent(INPUT_EVENT_KEY_PRESS, iKey); }
		void KeyUp(int32_t iKey)     { RaiseEvent(INPUT_EVENT_KEY_RELEASE, iKey); }
		void PadDown(int32_t iButton, int32_t iPad = 0) { RaiseEvent(INPUT_EVENT_PAD_PRESS, iButton, iPad); }
		void PadUp(int32_t iButton, int32_t iPad = 0)   { RaiseEvent(INPUT_EVENT_PAD_RELEASE, iButton, iPad); }
		void MouseDown(int32_t iButton) { RaiseEvent(INPUT_EVENT_MOUSE_PRESS, iButton); }
		void MouseUp(int32_t iButton)   { RaiseEvent(INPUT_EVENT_MOUSE_RELEASE, iButton); }

		void TouchEvent(Zenith_InputEventType eType, int32_t iPointerId, float fX, float fY)
		{
			Zenith_InputEvent xEvent;
			xEvent.m_eType    = eType;
			xEvent.m_iCode    = iPointerId;
			xEvent.m_fX       = fX;
			xEvent.m_fY       = fY;
			xEvent.m_fAnchorX = fX;
			xEvent.m_fAnchorY = fY;
			m_xPointers.ApplyEvent(xEvent);
		}
	};

	// RAII so a failing assertion cannot leave the simulator enabled for the
	// rest of the boot batch.
	struct Zenith_ActionTestSimScope
	{
		Zenith_ActionTestSimScope()
		{
#ifdef ZENITH_INPUT_SIMULATOR
			Zenith_InputSimulator::Enable();
#endif
		}
		~Zenith_ActionTestSimScope()
		{
#ifdef ZENITH_INPUT_SIMULATOR
			Zenith_InputSimulator::DiscardPendingInjections();
			Zenith_InputSimulator::ResetAllInputState();
			Zenith_InputSimulator::Disable();
#endif
		}
	};

	// Frame contract step 7, minus the engine: everything the simulator queued
	// this "Step" becomes ordered transitions on the local device layer.
	void Zenith_DrainSimulatorInjections(Zenith_ActionTestRig& xRig)
	{
#ifdef ZENITH_INPUT_SIMULATOR
		const u_int32 uCount = Zenith_InputSimulator::GetPendingInjectionCount();
		for (u_int32 u = 0; u < uCount; u++)
		{
			xRig.m_xInput.AppendInjectedEvent(Zenith_InputSimulator::GetPendingInjection(u));
		}
		Zenith_InputSimulator::DiscardPendingInjections();
#else
		(void)xRig;
#endif
	}

	u_int32 g_uProfileChangeCount = 0;
	u_int8  g_uLastProfileSeen    = Zenith_InputActions::uPROFILE_AUTO;

	void Zenith_TestProfileChanged(void* pxUserData, u_int8 uProfileId)
	{
		(void)pxUserData;
		g_uProfileChangeCount++;
		g_uLastProfileSeen = uProfileId;
	}
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

ZENITH_TEST(InputActions, RegisterAndFindByName)
{
	Zenith_ActionTestRig xRig;
	xRig.m_xActions.RegisterAction(uTEST_ACTION_A, "Jump", INPUT_ACTION_BUTTON);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_A, Zenith_InputBinding::KeySet(ZENITH_KEY_SPACE));

	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsActionRegistered(uTEST_ACTION_A), "the action registered");
	ZENITH_ASSERT_EQ(xRig.m_xActions.FindActionByName("Jump"), uTEST_ACTION_A, "name resolves to its id");
	ZENITH_ASSERT_EQ(xRig.m_xActions.FindActionByName("NotRegistered"), uINPUT_ACTION_INVALID,
		"an unknown name resolves to the invalid id, not to 0");
	ZENITH_ASSERT_EQ(xRig.m_xActions.GetBindingCount(uTEST_ACTION_A), 1u, "one binding row");
	ZENITH_ASSERT_EQ(static_cast<int>(xRig.m_xActions.GetActionKind(uTEST_ACTION_A)),
		static_cast<int>(INPUT_ACTION_BUTTON), "kind round-trips");

	// The engine-reserved set is findable by name too — that is how a graph node
	// refers to an action without knowing its id.
	xRig.m_xActions.RegisterEngineReservedActions();
	ZENITH_ASSERT_EQ(xRig.m_xActions.FindActionByName("UI_CONFIRM"),
		static_cast<Zenith_InputActionID>(INPUT_ACTION_UI_CONFIRM), "reserved names resolve");
	ZENITH_ASSERT_EQ(xRig.m_xActions.FindActionByName("UI_NAV_LEFT"),
		static_cast<Zenith_InputActionID>(INPUT_ACTION_UI_NAV_LEFT), "reserved names resolve");
}

// ---------------------------------------------------------------------------
// Aggregation + the ordered replay (B4)
// ---------------------------------------------------------------------------

ZENITH_TEST(InputActions, ButtonKeySetAggregation)
{
	Zenith_ActionTestRig xRig;
	xRig.RegisterAllSchemeProfile();
	xRig.m_xActions.RegisterAction(uTEST_ACTION_A, "Fire", INPUT_ACTION_BUTTON);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_A,
		Zenith_InputBinding::KeySet(ZENITH_KEY_F, ZENITH_KEY_G));

	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_F);
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "any key in the set holds the action");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(uTEST_ACTION_A), "with a press edge");

	// The OTHER key of the same set arriving is not a second press.
	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_G);
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "still held");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.WasPressedThisFrame(uTEST_ACTION_A), "no second press edge");

	// ...and releasing one of them is not a release while the other is down.
	xRig.BeginFrame();
	xRig.KeyUp(ZENITH_KEY_F);
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "the set is still satisfied");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.WasReleasedThisFrame(uTEST_ACTION_A), "no release edge");

	xRig.BeginFrame();
	xRig.KeyUp(ZENITH_KEY_G);
	xRig.CloseFrame();
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "the last key up releases the action");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasReleasedThisFrame(uTEST_ACTION_A), "with a release edge");
}

ZENITH_TEST(InputActions, MultiBindingReleaseWhileOtherHeld)
{
	Zenith_ActionTestRig xRig;
	xRig.RegisterAllSchemeProfile();
	xRig.m_xActions.RegisterAction(uTEST_ACTION_A, "Fire", INPUT_ACTION_BUTTON);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_A, Zenith_InputBinding::KeySet(ZENITH_KEY_F));
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_A,
		Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_A));

	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_F);
	xRig.PadDown(ZENITH_GAMEPAD_BUTTON_A);
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "both bindings down");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(uTEST_ACTION_A), "exactly one press edge");

	// Releasing ONE binding while the other stays held must fire NOTHING: the
	// aggregate never fell.
	xRig.BeginFrame();
	xRig.KeyUp(ZENITH_KEY_F);
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "the pad binding still holds it");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.WasReleasedThisFrame(uTEST_ACTION_A), "no release edge");

	xRig.BeginFrame();
	xRig.PadUp(ZENITH_GAMEPAD_BUTTON_A);
	xRig.CloseFrame();
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "the last binding up releases it");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasReleasedThisFrame(uTEST_ACTION_A), "with a release edge");
}

ZENITH_TEST(InputActions, SecondBindingPressWhileHeld)
{
	Zenith_ActionTestRig xRig;
	xRig.RegisterAllSchemeProfile();
	xRig.m_xActions.RegisterAction(uTEST_ACTION_A, "Fire", INPUT_ACTION_BUTTON);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_A, Zenith_InputBinding::KeySet(ZENITH_KEY_F));
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_A,
		Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_A));

	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_F);
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(uTEST_ACTION_A), "first binding presses");

	xRig.BeginFrame();
	xRig.PadDown(ZENITH_GAMEPAD_BUTTON_A);
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "still held");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.WasPressedThisFrame(uTEST_ACTION_A),
		"a second binding arriving while the aggregate is already up is not a press");
}

ZENITH_TEST(InputActions, SameFrameKeyTapFiresBothEdges)
{
	Zenith_ActionTestRig xRig;
	xRig.RegisterAllSchemeProfile();
	xRig.m_xActions.RegisterAction(uTEST_ACTION_A, "Fire", INPUT_ACTION_BUTTON);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_A, Zenith_InputBinding::KeySet(ZENITH_KEY_F));

	// Press AND release inside ONE frame. Because the replay is ORDERED, the
	// aggregate genuinely rises and falls, so both edges latch.
	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_F);
	xRig.KeyUp(ZENITH_KEY_F);
	xRig.CloseFrame();

	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(uTEST_ACTION_A), "the tap pressed");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasReleasedThisFrame(uTEST_ACTION_A), "...and released");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "and is not left held");
}

ZENITH_TEST(InputActions, SecondBindingPulseWhileHeldNoEdges)
{
	Zenith_ActionTestRig xRig;
	xRig.RegisterAllSchemeProfile();
	xRig.m_xActions.RegisterAction(uTEST_ACTION_A, "Fire", INPUT_ACTION_BUTTON);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_A, Zenith_InputBinding::KeySet(ZENITH_KEY_F));
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_A,
		Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_A));

	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_F);
	xRig.CloseFrame();

	// A whole tap on the OTHER binding while the first stays held: the aggregate
	// never moved, so neither edge may fire.
	xRig.BeginFrame();
	xRig.PadDown(ZENITH_GAMEPAD_BUTTON_A);
	xRig.PadUp(ZENITH_GAMEPAD_BUTTON_A);
	xRig.CloseFrame();

	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "the held binding still holds it");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.WasPressedThisFrame(uTEST_ACTION_A), "no press edge");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.WasReleasedThisFrame(uTEST_ACTION_A), "no release edge");
}

// ---------------------------------------------------------------------------
// Axis bindings (B8)
// ---------------------------------------------------------------------------

ZENITH_TEST(InputActions, Axis2DCompositeSemantics)
{
	// The pure rule first: +y forward, UNNORMALISED diagonals, opposites cancel.
	Zenith_Maths::Vector2 xMove = Zenith_InputActions::ResolveMoveComposite(true, false, false, true);
	ZENITH_ASSERT_EQ_FLOAT(xMove.x, 1.0f, 1e-5f, "forward+right is x=1");
	ZENITH_ASSERT_EQ_FLOAT(xMove.y, 1.0f, 1e-5f, "...and y=1, NOT normalised");
	xMove = Zenith_InputActions::ResolveMoveComposite(true, true, true, true);
	ZENITH_ASSERT_EQ_FLOAT(xMove.x, 0.0f, 1e-5f, "opposite keys cancel (x)");
	ZENITH_ASSERT_EQ_FLOAT(xMove.y, 0.0f, 1e-5f, "opposite keys cancel (y)");

	Zenith_ActionTestRig xRig;
	xRig.RegisterAllSchemeProfile();
	xRig.m_xActions.RegisterAction(uTEST_ACTION_AXIS, "Move", INPUT_ACTION_AXIS2D);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_AXIS,
		Zenith_InputBinding::KeyAxis2D(ZENITH_KEY_W, ZENITH_KEY_S, ZENITH_KEY_A, ZENITH_KEY_D));

	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_W);
	xRig.KeyDown(ZENITH_KEY_D);
	xRig.CloseFrame();

	Zenith_Maths::Vector2 xValue;
	xRig.m_xActions.GetAxis2D(uTEST_ACTION_AXIS, xValue);
	ZENITH_ASSERT_EQ_FLOAT(xValue.x, 1.0f, 1e-5f, "D drives +x");
	ZENITH_ASSERT_EQ_FLOAT(xValue.y, 1.0f, 1e-5f, "W drives +y");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uTEST_ACTION_AXIS), "an axis action is held while a direction is down");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(uTEST_ACTION_AXIS), "movement started this frame");

	// Opposite key added: the composite cancels but the action stays held.
	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_S);
	xRig.CloseFrame();
	xRig.m_xActions.GetAxis2D(uTEST_ACTION_AXIS, xValue);
	ZENITH_ASSERT_EQ_FLOAT(xValue.y, 0.0f, 1e-5f, "W+S cancels");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uTEST_ACTION_AXIS), "keys are still down");
}

ZENITH_TEST(InputActions, Axis1DKeyPair)
{
	Zenith_ActionTestRig xRig;
	xRig.RegisterAllSchemeProfile();
	xRig.m_xActions.RegisterAction(uTEST_ACTION_AXIS, "Lean", INPUT_ACTION_AXIS1D);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_AXIS,
		Zenith_InputBinding::KeyAxis1D(ZENITH_KEY_Q, ZENITH_KEY_E));

	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_E);
	xRig.CloseFrame();
	ZENITH_ASSERT_EQ_FLOAT(xRig.m_xActions.GetAxis1D(uTEST_ACTION_AXIS), 1.0f, 1e-5f, "positive set");

	xRig.BeginFrame();
	xRig.KeyUp(ZENITH_KEY_E);
	xRig.KeyDown(ZENITH_KEY_Q);
	xRig.CloseFrame();
	ZENITH_ASSERT_EQ_FLOAT(xRig.m_xActions.GetAxis1D(uTEST_ACTION_AXIS), -1.0f, 1e-5f, "negative set");

	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_E);
	xRig.CloseFrame();
	ZENITH_ASSERT_EQ_FLOAT(xRig.m_xActions.GetAxis1D(uTEST_ACTION_AXIS), 0.0f, 1e-5f, "both sets cancel");
}

ZENITH_TEST(InputActions, TriggerPairSignedAxis)
{
	Zenith_ActionTestSimScope xSimScope;
	Zenith_ActionTestRig xRig;
	xRig.RegisterAllSchemeProfile();
	xRig.m_xActions.RegisterAction(uTEST_ACTION_AXIS, "Throttle", INPUT_ACTION_AXIS1D);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_AXIS, Zenith_InputBinding::GamepadTriggerPair(1.0f));

#ifdef ZENITH_INPUT_SIMULATOR
	Zenith_InputSimulator::SimulateGamepadConnected(true, 0);
	Zenith_InputSimulator::SimulateGamepadAxis(ZENITH_GAMEPAD_AXIS_RIGHT_TRIGGER, 0.75f, 0);
	Zenith_InputSimulator::SimulateGamepadAxis(ZENITH_GAMEPAD_AXIS_LEFT_TRIGGER, 0.25f, 0);
#endif

	xRig.BeginFrame();
	xRig.CloseFrame();
	// RT - LT, both already canonical [0,1] at the device layer.
	ZENITH_ASSERT_EQ_FLOAT(xRig.m_xActions.GetAxis1D(uTEST_ACTION_AXIS), 0.5f, 1e-5f, "RT - LT");
}

ZENITH_TEST(InputActions, DpadAxis2D)
{
	Zenith_ActionTestRig xRig;
	xRig.RegisterAllSchemeProfile();
	xRig.m_xActions.RegisterAction(uTEST_ACTION_AXIS, "PadMove", INPUT_ACTION_AXIS2D);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_AXIS, Zenith_InputBinding::GamepadDpadAxis2D());

	xRig.BeginFrame();
	xRig.PadDown(ZENITH_GAMEPAD_BUTTON_DPAD_UP);
	xRig.PadDown(ZENITH_GAMEPAD_BUTTON_DPAD_RIGHT);
	xRig.CloseFrame();

	Zenith_Maths::Vector2 xValue;
	xRig.m_xActions.GetAxis2D(uTEST_ACTION_AXIS, xValue);
	ZENITH_ASSERT_EQ_FLOAT(xValue.x, 1.0f, 1e-5f, "d-pad right is +x");
	ZENITH_ASSERT_EQ_FLOAT(xValue.y, 1.0f, 1e-5f, "d-pad up is +y");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uTEST_ACTION_AXIS), "held while a d-pad direction is down");

	xRig.BeginFrame();
	xRig.PadDown(ZENITH_GAMEPAD_BUTTON_DPAD_DOWN);
	xRig.CloseFrame();
	xRig.m_xActions.GetAxis2D(uTEST_ACTION_AXIS, xValue);
	ZENITH_ASSERT_EQ_FLOAT(xValue.y, 0.0f, 1e-5f, "opposite d-pad directions cancel");
}

ZENITH_TEST(InputActions, AxisAsButtonHysteresis)
{
	Zenith_ActionTestSimScope xSimScope;
	Zenith_ActionTestRig xRig;
	xRig.RegisterAllSchemeProfile();
	xRig.m_xActions.RegisterAction(uTEST_ACTION_A, "TriggerFire", INPUT_ACTION_BUTTON);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_A,
		Zenith_InputBinding::GamepadAxisAsButton(ZENITH_GAMEPAD_AXIS_RIGHT_TRIGGER, 0.7f, 0.3f));

#ifdef ZENITH_INPUT_SIMULATOR
	Zenith_InputSimulator::SimulateGamepadConnected(true, 0);

	// Below the press threshold: nothing.
	Zenith_InputSimulator::SimulateGamepadAxis(ZENITH_GAMEPAD_AXIS_RIGHT_TRIGGER, 0.5f, 0);
	xRig.BeginFrame();
	xRig.CloseFrame();
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "0.5 does not reach the 0.7 press point");

	// Past the press threshold: held, with an edge.
	Zenith_InputSimulator::SimulateGamepadAxis(ZENITH_GAMEPAD_AXIS_RIGHT_TRIGGER, 0.8f, 0);
	xRig.BeginFrame();
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "0.8 presses");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(uTEST_ACTION_A), "with a press edge");

	// Back into the HYSTERESIS BAND: still held, no edges — this is the whole
	// point of having two thresholds.
	Zenith_InputSimulator::SimulateGamepadAxis(ZENITH_GAMEPAD_AXIS_RIGHT_TRIGGER, 0.5f, 0);
	xRig.BeginFrame();
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "0.5 is inside the band, still held");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.WasReleasedThisFrame(uTEST_ACTION_A), "no release inside the band");

	// Below the release threshold: released.
	Zenith_InputSimulator::SimulateGamepadAxis(ZENITH_GAMEPAD_AXIS_RIGHT_TRIGGER, 0.2f, 0);
	xRig.BeginFrame();
	xRig.CloseFrame();
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "0.2 releases");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasReleasedThisFrame(uTEST_ACTION_A), "with a release edge");
#endif
}

ZENITH_TEST(InputActions, MouseWheelAndDeltaAxes)
{
	Zenith_ActionTestSimScope xSimScope;
	Zenith_ActionTestRig xRig;
	xRig.RegisterAllSchemeProfile();
	xRig.m_xActions.RegisterAction(uTEST_ACTION_AXIS, "Zoom", INPUT_ACTION_AXIS1D);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_AXIS, Zenith_InputBinding::MouseWheel(2.0f));
	xRig.m_xActions.RegisterAction(uTEST_ACTION_B, "Look", INPUT_ACTION_AXIS2D);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_B, Zenith_InputBinding::MouseDelta(1.0f, -1.0f));

#ifdef ZENITH_INPUT_SIMULATOR
	Zenith_InputSimulator::SimulateMouseWheel(3.0f);
#endif
	// Mouse displacement is a LEVEL, read through the device layer's own delta.
	xRig.m_xInput.UpdateMouseDeltaFromPosition(Zenith_Maths::Vector2_64(0.0, 0.0), false);
	xRig.m_xInput.UpdateMouseDeltaFromPosition(Zenith_Maths::Vector2_64(10.0, 4.0), false);

	xRig.BeginFrame();
	xRig.CloseFrame();

#ifdef ZENITH_INPUT_SIMULATOR
	ZENITH_ASSERT_EQ_FLOAT(xRig.m_xActions.GetAxis1D(uTEST_ACTION_AXIS), 6.0f, 1e-4f, "wheel * scale");
#endif
	Zenith_Maths::Vector2 xLook;
	xRig.m_xActions.GetAxis2D(uTEST_ACTION_B, xLook);
	ZENITH_ASSERT_EQ_FLOAT(xLook.x, 10.0f, 1e-4f, "delta x * scale");
	ZENITH_ASSERT_EQ_FLOAT(xLook.y, -4.0f, 1e-4f, "delta y * INDEPENDENT y scale (inverted look)");
}

// ---------------------------------------------------------------------------
// Masks, profiles, activity (B5)
// ---------------------------------------------------------------------------

ZENITH_TEST(InputActions, MaskGatesResolution)
{
	Zenith_ActionTestRig xRig;
	xRig.RegisterSplitProfiles();
	xRig.m_xActions.RegisterAction(uTEST_ACTION_A, "Fire", INPUT_ACTION_BUTTON);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_A, Zenith_InputBinding::KeySet(ZENITH_KEY_F));
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_A,
		Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_A));

	// Pin the keyboard profile so the auto-switch cannot answer the question
	// this test is asking.
	xRig.m_xActions.SetProfileOverride(uTEST_PROFILE_KEYBOARD);

	xRig.BeginFrame();
	xRig.PadDown(ZENITH_GAMEPAD_BUTTON_A);
	xRig.CloseFrame();
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uTEST_ACTION_A),
		"a GAMEPAD row does not resolve under a keyboard-only mask");

	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_F);
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "the KEYBOARD row does resolve");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(uTEST_ACTION_A), "with its press edge");
}

ZENITH_TEST(InputActions, SystemBackResolvesUnderAnyMask)
{
	Zenith_ActionTestRig xRig;
	xRig.m_xActions.RegisterProfile(uTEST_PROFILE_KEYBOARD, "TestKeyboard", uINPUT_SCHEME_MASK_KEYBOARD);
	// A profile that enables NOTHING. SYSTEM_BACK still has to work under it.
	xRig.m_xActions.RegisterProfile(uTEST_PROFILE_EMPTY, "TestEmpty", 0);

	xRig.m_xActions.RegisterAction(uTEST_ACTION_A, "Back", INPUT_ACTION_BUTTON);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_A, Zenith_InputBinding::SystemBack());
	xRig.m_xActions.RegisterAction(uTEST_ACTION_B, "Fire", INPUT_ACTION_BUTTON);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_B, Zenith_InputBinding::KeySet(ZENITH_KEY_F));

	xRig.m_xActions.SetProfileOverride(uTEST_PROFILE_EMPTY);

	xRig.BeginFrame();
	xRig.RaiseEvent(INPUT_EVENT_SYSTEM_BACK, 0, 0, /* bSystemNav */ true);
	xRig.KeyDown(ZENITH_KEY_F);
	xRig.CloseFrame();

	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(uTEST_ACTION_A),
		"SYSTEM_BACK is mask-EXEMPT and fires under an empty mask");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasReleasedThisFrame(uTEST_ACTION_A),
		"...as a PULSE — the gesture has no held phase");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "and is never left held");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uTEST_ACTION_B),
		"an ordinary keyboard row is still gated by the mask");
}

// Registering a SYSTEM_BACK row publishes the fact DOWN to the device layer.
//
// This is the whole "should the platform CONSUME Back?" contract, and it has to
// live at the bottom: Android's entry point sits BELOW Input in the layer DAG,
// so it can read a flag off Zenith_Input through the window funnel but can never
// ask Zenith_InputActions. Nothing on the platform side is compiled on win64, so
// this unit is the only thing that can catch the answer going stale.
ZENITH_TEST(InputActions, SystemBackBindingFlagsTheDeviceLayer)
{
	Zenith_ActionTestRig xRig;
	ZENITH_ASSERT_FALSE(xRig.m_xInput.HasSystemBackBinding(),
		"a fresh device layer has no SYSTEM_BACK binding — an un-bound Back must keep the platform's own behaviour");

	xRig.m_xActions.RegisterAction(uTEST_ACTION_B, "Fire", INPUT_ACTION_BUTTON);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_B, Zenith_InputBinding::KeySet(ZENITH_KEY_F));
	ZENITH_ASSERT_FALSE(xRig.m_xInput.HasSystemBackBinding(),
		"an ordinary binding does not flip it");

	xRig.m_xActions.RegisterAction(uTEST_ACTION_A, "Cancel", INPUT_ACTION_BUTTON);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_A, Zenith_InputBinding::SystemBack());
	ZENITH_ASSERT_TRUE(xRig.m_xInput.HasSystemBackBinding(),
		"a SYSTEM_BACK row flips the device-layer flag, which is what makes the platform consume Back");

	// Registrations are not per-frame state: the between-tests reset clears the
	// transient input state and must leave this alone, or a game's Back would go
	// dead for every test after the first.
	xRig.m_xInput.ResetTransientForTest();
	xRig.m_xActions.ResetTransientForTest();
	ZENITH_ASSERT_TRUE(xRig.m_xInput.HasSystemBackBinding(),
		"...and survives ResetTransientForTest, which clears state but never registrations");
}

ZENITH_TEST(InputActions, ProfileAutoSwitchOnActivity)
{
	Zenith_ActionTestRig xRig;
	xRig.RegisterSplitProfiles();
	// Boot default: the profile owning KEYBOARD on desktop; on a touch platform
	// no profile owns TOUCH here, so the FIRST registered wins — the same one.
	ZENITH_ASSERT_EQ(xRig.m_xActions.GetActiveProfile(), uTEST_PROFILE_KEYBOARD, "boot default profile");

	xRig.BeginFrame();
	xRig.PadDown(ZENITH_GAMEPAD_BUTTON_A);
	xRig.CloseFrame();
	ZENITH_ASSERT_EQ(xRig.m_xActions.GetActiveProfile(), uTEST_PROFILE_GAMEPAD,
		"pad activity switches to the profile that owns GAMEPAD");
	ZENITH_ASSERT_EQ(static_cast<int>(xRig.m_xActions.GetLastUsedDevice()),
		static_cast<int>(INPUT_SCHEME_GAMEPAD), "and the last-used device follows");

	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_F);
	xRig.CloseFrame();
	ZENITH_ASSERT_EQ(xRig.m_xActions.GetActiveProfile(), uTEST_PROFILE_KEYBOARD, "...and back again");

	// Activity for a scheme NO profile claims is ignored outright.
	xRig.BeginFrame();
	xRig.MouseDown(ZENITH_MOUSE_BUTTON_LEFT);
	xRig.CloseFrame();
	ZENITH_ASSERT_EQ(xRig.m_xActions.GetActiveProfile(), uTEST_PROFILE_KEYBOARD,
		"an unregistered scheme cannot switch the profile");
}

ZENITH_TEST(InputActions, HeldStickDoesNotRepeatActivity)
{
	Zenith_ActionTestSimScope xSimScope;
	Zenith_ActionTestRig xRig;
	xRig.RegisterSplitProfiles();

#ifdef ZENITH_INPUT_SIMULATOR
	Zenith_InputSimulator::SimulateGamepadConnected(true, 0);
	Zenith_InputSimulator::SimulateGamepadStick(ZENITH_GAMEPAD_AXIS_LEFT_X, 0.9f, 0.0f, 0);

	xRig.BeginFrame();
	xRig.CloseFrame();
	ZENITH_ASSERT_EQ(xRig.m_xActions.GetActiveProfile(), uTEST_PROFILE_GAMEPAD,
		"crossing the deadzone is pad activity");

	// The stick STAYS out. A key press now must win, which it only can if the
	// held stick stopped notifying — a repeat would outrank KEYBOARD every
	// frame and pin the pad profile forever.
	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_F);
	xRig.CloseFrame();
	ZENITH_ASSERT_EQ(xRig.m_xActions.GetActiveProfile(), uTEST_PROFILE_KEYBOARD,
		"a held stick does not re-notify");

	// Fall back inside the deadzone to RE-ARM, then cross again.
	Zenith_InputSimulator::SimulateGamepadStick(ZENITH_GAMEPAD_AXIS_LEFT_X, 0.0f, 0.0f, 0);
	xRig.BeginFrame();
	xRig.CloseFrame();
	ZENITH_ASSERT_EQ(xRig.m_xActions.GetActiveProfile(), uTEST_PROFILE_KEYBOARD, "resting stick changes nothing");

	Zenith_InputSimulator::SimulateGamepadStick(ZENITH_GAMEPAD_AXIS_LEFT_X, 0.9f, 0.0f, 0);
	xRig.BeginFrame();
	xRig.CloseFrame();
	ZENITH_ASSERT_EQ(xRig.m_xActions.GetActiveProfile(), uTEST_PROFILE_GAMEPAD,
		"a re-armed crossing notifies again");
#endif
}

ZENITH_TEST(InputActions, SystemNavExcludedFromActivity)
{
	Zenith_ActionTestRig xRig;
	xRig.RegisterSplitProfiles();

	// A navigation gesture is the SYSTEM talking. It must not be mistaken for
	// the player picking up a device.
	xRig.BeginFrame();
	xRig.RaiseEvent(INPUT_EVENT_KEY_PRESS, ZENITH_KEY_ESCAPE, 0, /* bSystemNav */ true);
	xRig.RaiseEvent(INPUT_EVENT_PAD_PRESS, ZENITH_GAMEPAD_BUTTON_B, 0, /* bSystemNav */ true);
	xRig.CloseFrame();
	ZENITH_ASSERT_EQ(static_cast<int>(xRig.m_xActions.GetLastUsedDevice()),
		static_cast<int>(INPUT_SCHEME_NONE), "system-nav events are not device activity");
	ZENITH_ASSERT_EQ(xRig.m_xActions.GetActiveProfile(), uTEST_PROFILE_KEYBOARD,
		"...and cannot switch the profile");

	// The same press WITHOUT the flag does count.
	xRig.BeginFrame();
	xRig.PadDown(ZENITH_GAMEPAD_BUTTON_B);
	xRig.CloseFrame();
	ZENITH_ASSERT_EQ(static_cast<int>(xRig.m_xActions.GetLastUsedDevice()),
		static_cast<int>(INPUT_SCHEME_GAMEPAD), "an ordinary press is activity");
}

ZENITH_TEST(InputActions, ForcedProfileSuspendsAutoAndClearReturns)
{
	Zenith_ActionTestRig xRig;
	xRig.RegisterSplitProfiles();

	xRig.m_xActions.SetProfileOverride(uTEST_PROFILE_KEYBOARD);
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsProfileOverridden(), "override latched");

	xRig.BeginFrame();
	xRig.PadDown(ZENITH_GAMEPAD_BUTTON_A);
	xRig.CloseFrame();
	ZENITH_ASSERT_EQ(xRig.m_xActions.GetActiveProfile(), uTEST_PROFILE_KEYBOARD,
		"a forced profile suspends the auto-switch");
	ZENITH_ASSERT_EQ(static_cast<int>(xRig.m_xActions.GetLastUsedDevice()),
		static_cast<int>(INPUT_SCHEME_GAMEPAD), "but the last-used device still tracks");

	xRig.m_xActions.ClearOverride();
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsProfileOverridden(), "override cleared");
	ZENITH_ASSERT_EQ(xRig.m_xActions.GetActiveProfile(), uTEST_PROFILE_GAMEPAD,
		"returning to AUTO lands on the last-used device's profile");
}

ZENITH_TEST(InputActions, LastUsedDeviceTracksFinerThanProfile)
{
	Zenith_ActionTestRig xRig;
	// ONE profile owning every desktop scheme: no activity can ever change the
	// profile, which is exactly when the glyph hook has to keep working.
	xRig.m_xActions.RegisterProfile(uTEST_PROFILE_ALL, "TestDesktop", uINPUT_SCHEME_MASK_DESKTOP);

	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_F);
	xRig.CloseFrame();
	ZENITH_ASSERT_EQ(static_cast<int>(xRig.m_xActions.GetLastUsedDevice()),
		static_cast<int>(INPUT_SCHEME_KEYBOARD), "keyboard");

	xRig.BeginFrame();
	xRig.PadDown(ZENITH_GAMEPAD_BUTTON_A);
	xRig.CloseFrame();
	ZENITH_ASSERT_EQ(static_cast<int>(xRig.m_xActions.GetLastUsedDevice()),
		static_cast<int>(INPUT_SCHEME_GAMEPAD), "the pad changed the DEVICE...");
	ZENITH_ASSERT_EQ(xRig.m_xActions.GetActiveProfile(), uTEST_PROFILE_ALL,
		"...without changing the profile that already owned it");

	// Same-frame priority: TOUCH > GAMEPAD > MOUSE > KEYBOARD.
	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_G);
	xRig.MouseDown(ZENITH_MOUSE_BUTTON_LEFT);
	xRig.TouchEvent(INPUT_EVENT_TOUCH_DOWN, 1, 5.0f, 5.0f);
	xRig.RaiseEvent(INPUT_EVENT_TOUCH_DOWN, 1, 0);
	xRig.CloseFrame();
	ZENITH_ASSERT_EQ(static_cast<int>(xRig.m_xActions.GetLastUsedDevice()),
		static_cast<int>(INPUT_SCHEME_TOUCH), "touch outranks everything in the same frame");
}

// ---------------------------------------------------------------------------
// Mask-change synthesis / rebasing (B4)
// ---------------------------------------------------------------------------

ZENITH_TEST(InputActions, MaskChangeReleasesHeldOldBinding)
{
	Zenith_ActionTestRig xRig;
	xRig.RegisterSplitProfiles();
	xRig.m_xActions.RegisterAction(uTEST_ACTION_A, "Fire", INPUT_ACTION_BUTTON);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_A, Zenith_InputBinding::KeySet(ZENITH_KEY_F));

	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_F);
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "held under the keyboard profile");

	// An AUTO switch away from the keyboard: the key is still physically down,
	// but the action's only row just left the mask.
	xRig.BeginFrame();
	xRig.PadDown(ZENITH_GAMEPAD_BUTTON_A);
	xRig.CloseFrame();
	ZENITH_ASSERT_EQ(xRig.m_xActions.GetActiveProfile(), uTEST_PROFILE_GAMEPAD, "auto-switched");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "the action is no longer held");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasReleasedThisFrame(uTEST_ACTION_A),
		"and the mask change SYNTHESIZED the release the player never made");
}

ZENITH_TEST(InputActions, MaskChangeNewlyEnabledHeldNoPressEdge)
{
	Zenith_ActionTestRig xRig;
	xRig.RegisterSplitProfiles();
	xRig.m_xActions.RegisterAction(uTEST_ACTION_A, "Fire", INPUT_ACTION_BUTTON);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_A,
		Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_A));

	xRig.m_xActions.SetProfileOverride(uTEST_PROFILE_KEYBOARD);

	// The pad button goes down while GAMEPAD is masked OFF: the source state is
	// tracked, the action is not.
	xRig.BeginFrame();
	xRig.PadDown(ZENITH_GAMEPAD_BUTTON_A);
	xRig.CloseFrame();
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "masked off");

	// Enable it. The source is ALREADY down, so the action becomes held — but a
	// press edge requires a genuine transition, and there was none.
	xRig.BeginFrame();
	xRig.m_xActions.UpdateProfile();
	xRig.m_xActions.SetProfileOverride(uTEST_PROFILE_GAMEPAD);
	xRig.m_xActions.FinalizeReservedUI();
	xRig.m_xActions.FinalizeGameplay();

	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "held tracks reality");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.WasPressedThisFrame(uTEST_ACTION_A),
		"but no press edge is invented for it");
}

ZENITH_TEST(InputActions, ForcedOverrideWhileHeldSynthesizes)
{
	Zenith_ActionTestRig xRig;
	xRig.RegisterSplitProfiles();
	xRig.m_xActions.RegisterAction(uTEST_ACTION_A, "Fire", INPUT_ACTION_BUTTON);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_A, Zenith_InputBinding::KeySet(ZENITH_KEY_F));

	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_F);
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "held");

	// A FORCED override mid-frame is a mask change like any other.
	xRig.BeginFrame();
	xRig.m_xActions.UpdateProfile();
	xRig.m_xActions.SetProfileOverride(uTEST_PROFILE_GAMEPAD);
	xRig.m_xActions.FinalizeReservedUI();
	xRig.m_xActions.FinalizeGameplay();

	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "released by the forced mask");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasReleasedThisFrame(uTEST_ACTION_A), "with a synthesized release");
}

ZENITH_TEST(InputActions, ReturnToAutoRebases)
{
	Zenith_ActionTestRig xRig;
	xRig.RegisterSplitProfiles();
	xRig.m_xActions.RegisterAction(uTEST_ACTION_A, "Fire", INPUT_ACTION_BUTTON);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_A, Zenith_InputBinding::KeySet(ZENITH_KEY_F));

	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_F);
	xRig.CloseFrame();

	xRig.BeginFrame();
	xRig.m_xActions.UpdateProfile();
	xRig.m_xActions.SetProfileOverride(uTEST_PROFILE_GAMEPAD);
	xRig.m_xActions.FinalizeReservedUI();
	xRig.m_xActions.FinalizeGameplay();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasReleasedThisFrame(uTEST_ACTION_A), "override released it");

	// Back to AUTO with the key STILL down: the rebase must restore held without
	// inventing a press, and the stale release must not linger.
	xRig.BeginFrame();
	xRig.m_xActions.UpdateProfile();
	xRig.m_xActions.ClearOverride();
	xRig.m_xActions.FinalizeReservedUI();
	xRig.m_xActions.FinalizeGameplay();

	ZENITH_ASSERT_EQ(xRig.m_xActions.GetActiveProfile(), uTEST_PROFILE_KEYBOARD, "AUTO landed on the keyboard profile");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "the still-down key rebases to held");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.WasPressedThisFrame(uTEST_ACTION_A), "no invented press edge");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.WasReleasedThisFrame(uTEST_ACTION_A), "and no stale release edge");
}

// ---------------------------------------------------------------------------
// Pointer-sourced rows + claims (B1 10e)
// ---------------------------------------------------------------------------

ZENITH_TEST(InputActions, TouchPrimaryRespectsClaims)
{
	Zenith_ActionTestRig xRig;
	xRig.RegisterAllSchemeProfile();
	xRig.m_xActions.RegisterAction(uTEST_ACTION_A, "TapFire", INPUT_ACTION_BUTTON);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_A, Zenith_InputBinding::TouchPrimary());

	xRig.BeginFrame();
	xRig.TouchEvent(INPUT_EVENT_TOUCH_DOWN, 1, 20.0f, 20.0f);
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "an unclaimed primary pointer holds the row");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(uTEST_ACTION_A), "with a press edge");

	// A widget claims the pointer during the capture walk (10c). By 10e the row
	// must behave as though the finger were gone: the gesture is consumed.
	xRig.BeginFrame();
	const Zenith_PointerHandle xHandle = xRig.m_xPointers.FindByPlatformId(1);
	ZENITH_ASSERT_TRUE(xHandle.IsValid(), "the pointer is still live");
	ZENITH_ASSERT_TRUE(xRig.m_xPointers.ClaimPointer(xHandle, 0x5150u), "claim taken");
	xRig.CloseFrame();
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "a claimed pointer is a consumed one");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasReleasedThisFrame(uTEST_ACTION_A), "and the row releases");

	// Lift that finger and let its slot retire, so the next gesture really is
	// the primary pointer rather than a leftover.
	xRig.m_xPointers.ReleaseClaim(xHandle, 0x5150u);
	xRig.BeginFrame();
	xRig.TouchEvent(INPUT_EVENT_TOUCH_UP, 1, 20.0f, 20.0f);
	xRig.CloseFrame();

	// A press and release inside ONE frame — every quick tap — still fires both
	// edges, because the down EDGE is what raises the row, not the level.
	xRig.BeginFrame();
	xRig.BeginFrame();
	xRig.TouchEvent(INPUT_EVENT_TOUCH_DOWN, 2, 30.0f, 30.0f);
	xRig.TouchEvent(INPUT_EVENT_TOUCH_UP,   2, 30.0f, 30.0f);
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(uTEST_ACTION_A), "the tap pressed");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasReleasedThisFrame(uTEST_ACTION_A), "...and released");
}

ZENITH_TEST(InputActions, ClaimedPointerSuppressesMouseButtonRow)
{
	Zenith_ActionTestRig xRig;
	xRig.RegisterAllSchemeProfile();
	xRig.m_xActions.RegisterAction(uTEST_ACTION_A, "Shoot", INPUT_ACTION_BUTTON);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_A,
		Zenith_InputBinding::MouseButton(ZENITH_MOUSE_BUTTON_LEFT));
	// A KEYBOARD row on the same action, to prove the suppression is per-ROW and
	// never touches a device the pointer has nothing to do with.
	xRig.m_xActions.RegisterAction(uTEST_ACTION_B, "ShootKey", INPUT_ACTION_BUTTON);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_B, Zenith_InputBinding::KeySet(ZENITH_KEY_F));

	// Unclaimed: the mouse row resolves normally.
	xRig.BeginFrame();
	xRig.MouseDown(ZENITH_MOUSE_BUTTON_LEFT);
	xRig.TouchEvent(INPUT_EVENT_TOUCH_DOWN, 1, 20.0f, 20.0f);
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "an unclaimed pointer leaves the row alone");

	xRig.BeginFrame();
	xRig.MouseUp(ZENITH_MOUSE_BUTTON_LEFT);
	xRig.TouchEvent(INPUT_EVENT_TOUCH_UP, 1, 20.0f, 20.0f);
	xRig.CloseFrame();
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "released");

	// Now a widget owns the pointer BEFORE 10e runs.
	xRig.BeginFrame();
	xRig.BeginFrame();
	xRig.TouchEvent(INPUT_EVENT_TOUCH_DOWN, 2, 40.0f, 40.0f);
	const Zenith_PointerHandle xHandle = xRig.m_xPointers.FindByPlatformId(2);
	ZENITH_ASSERT_TRUE(xRig.m_xPointers.ClaimPointer(xHandle, 0x5150u), "widget claimed the pointer");
	xRig.MouseDown(ZENITH_MOUSE_BUTTON_LEFT);
	xRig.KeyDown(ZENITH_KEY_F);
	xRig.CloseFrame();
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uTEST_ACTION_A),
		"a press whose pointer is claimed is suppressed");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uTEST_ACTION_B),
		"the keyboard row is NEVER claim-filtered");

	// ...and the matching RELEASE is suppressed with it, so the row cannot
	// produce a lone release edge for a press it never saw.
	xRig.BeginFrame();
	xRig.MouseUp(ZENITH_MOUSE_BUTTON_LEFT);
	xRig.CloseFrame();
	ZENITH_ASSERT_FALSE(xRig.m_xActions.WasReleasedThisFrame(uTEST_ACTION_A),
		"a suppressed press suppresses its release");
}

// ---------------------------------------------------------------------------
// Virtual sources (the WP3a seam) + the simulator path
// ---------------------------------------------------------------------------

ZENITH_TEST(InputActions, VirtualBindingReplaysAtGameplayClose)
{
	Zenith_ActionTestRig xRig;
	xRig.RegisterAllSchemeProfile();
	xRig.m_xActions.RegisterAction(uTEST_ACTION_A, "VirtualFire", INPUT_ACTION_BUTTON);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_A, Zenith_InputBinding::Virtual(3));
	xRig.m_xActions.RegisterAction(uTEST_ACTION_AXIS, "VirtualMove", INPUT_ACTION_AXIS2D);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_AXIS, Zenith_InputBinding::Virtual(4));

	// Published between the capture walk and 10e, which is where the on-screen
	// controls will live.
	xRig.BeginFrame();
	xRig.m_xActions.UpdateProfile();
	xRig.m_xActions.FinalizeReservedUI();
	xRig.m_xActions.PublishVirtualButton(3, true);
	xRig.m_xActions.PublishVirtualButton(3, true);   // idempotent: still one rise
	xRig.m_xActions.PublishVirtualAxis(4, 0.5f, -0.25f);
	xRig.m_xActions.FinalizeGameplay();

	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "a published virtual button holds the action");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(uTEST_ACTION_A), "with exactly one press edge");
	Zenith_Maths::Vector2 xValue;
	xRig.m_xActions.GetAxis2D(uTEST_ACTION_AXIS, xValue);
	ZENITH_ASSERT_EQ_FLOAT(xValue.x, 0.5f, 1e-5f, "virtual axis x");
	ZENITH_ASSERT_EQ_FLOAT(xValue.y, -0.25f, 1e-5f, "virtual axis y");

	// A whole press+release inside one frame fires both edges, exactly like a
	// device tap.
	xRig.BeginFrame();
	xRig.m_xActions.UpdateProfile();
	xRig.m_xActions.FinalizeReservedUI();
	xRig.m_xActions.PublishVirtualButton(3, false);
	xRig.m_xActions.PublishVirtualButton(3, true);
	xRig.m_xActions.PublishVirtualButton(3, false);
	xRig.m_xActions.FinalizeGameplay();
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "ends up released");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasReleasedThisFrame(uTEST_ACTION_A), "with a release edge");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(uTEST_ACTION_A), "and the re-press in between");
}

ZENITH_TEST(InputActions, VirtualSourceIsTouchSchemeMasked)
{
	Zenith_ActionTestRig xRig;
	xRig.RegisterSplitProfiles();
	// A THIRD profile owning TOUCH, so the mask can genuinely gain and lose it.
	xRig.m_xActions.RegisterProfile(uTEST_PROFILE_ALL, "TestTouchOnly", uINPUT_SCHEME_MASK_TOUCH);
	xRig.m_xActions.RegisterAction(uTEST_ACTION_A, "VirtualFire", INPUT_ACTION_BUTTON);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_A, Zenith_InputBinding::Virtual(3));

	// A VIRTUAL row belongs to the TOUCH column (B8): the on-screen controls that
	// publish it are a touch-scheme concept, so a profile without TOUCH is a
	// profile whose on-screen controls are not on screen.
	xRig.m_xActions.SetProfileOverride(uTEST_PROFILE_KEYBOARD);

	xRig.BeginFrame();
	xRig.m_xActions.UpdateProfile();
	xRig.m_xActions.FinalizeReservedUI();
	xRig.m_xActions.PublishVirtualButton(3, true);
	xRig.m_xActions.FinalizeGameplay();
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uTEST_ACTION_A),
		"a virtual publish does not resolve under a mask without TOUCH");

	// Gaining TOUCH while the source is ALREADY published is a mask change like
	// any other: held tracks reality, and no press edge is invented for it.
	xRig.BeginFrame();
	xRig.m_xActions.UpdateProfile();
	xRig.m_xActions.SetProfileOverride(uTEST_PROFILE_ALL);
	xRig.m_xActions.FinalizeReservedUI();
	xRig.m_xActions.FinalizeGameplay();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "gaining TOUCH resolves the still-held source");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.WasPressedThisFrame(uTEST_ACTION_A), "with no invented press edge");

	// ...which is exactly why an on-screen control must publish a FALSE when it
	// is masked out, rather than merely going quiet: a source left standing at
	// true would resurrect the action the moment the mask came back.
	xRig.BeginFrame();
	xRig.m_xActions.UpdateProfile();
	xRig.m_xActions.FinalizeReservedUI();
	xRig.m_xActions.PublishVirtualButton(3, false);
	xRig.m_xActions.FinalizeGameplay();
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "the published release lands");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasReleasedThisFrame(uTEST_ACTION_A), "with a release edge");
}

ZENITH_TEST(InputActions, GamepadBindingViaSimulator)
{
	Zenith_ActionTestSimScope xSimScope;
	Zenith_ActionTestRig xRig;
	xRig.RegisterAllSchemeProfile();
	xRig.m_xActions.RegisterAction(uTEST_ACTION_A, "PadFire", INPUT_ACTION_BUTTON);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_A,
		Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_A));
	xRig.m_xActions.RegisterAction(uTEST_ACTION_AXIS, "PadLook", INPUT_ACTION_AXIS2D);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_AXIS,
		Zenith_InputBinding::GamepadStick(ZENITH_GAMEPAD_AXIS_RIGHT_X, 1.0f, -1.0f));

#ifdef ZENITH_INPUT_SIMULATOR
	// The whole point of WP0's injection path: a simulated pad reaches the action
	// layer through the SAME ordered transitions a real one does.
	xRig.BeginFrame();
	Zenith_InputSimulator::SimulateGamepadConnected(true, 0);
	Zenith_InputSimulator::SimulateGamepadButtonDown(ZENITH_GAMEPAD_BUTTON_A, 0);
	Zenith_InputSimulator::SimulateGamepadStick(ZENITH_GAMEPAD_AXIS_RIGHT_X, 0.6f, 0.4f, 0);
	Zenith_DrainSimulatorInjections(xRig);
	xRig.CloseFrame();

	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "the simulated pad button holds the action");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(uTEST_ACTION_A), "with a press edge");

	Zenith_Maths::Vector2 xLook;
	xRig.m_xActions.GetAxis2D(uTEST_ACTION_AXIS, xLook);
	ZENITH_ASSERT_EQ_FLOAT(xLook.x, 0.6f, 1e-5f, "simulated stick x");
	ZENITH_ASSERT_EQ_FLOAT(xLook.y, -0.4f, 1e-5f, "...with the row's independent y inversion");

	xRig.BeginFrame();
	Zenith_InputSimulator::SimulateGamepadButtonUp(ZENITH_GAMEPAD_BUTTON_A, 0);
	Zenith_DrainSimulatorInjections(xRig);
	xRig.CloseFrame();
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "and the simulated release releases it");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasReleasedThisFrame(uTEST_ACTION_A), "with a release edge");
#endif
}

// ---------------------------------------------------------------------------
// The engine-reserved UI actions (B1 10b / 10c)
// ---------------------------------------------------------------------------

ZENITH_TEST(InputActions, UINavActionsDriveFocus)
{
	Zenith_ActionTestRig xRig;
	xRig.m_xActions.RegisterEngineDefaultProfiles();
	xRig.m_xActions.RegisterEngineReservedActions();
	// This test drives focus/confirm with KEYBOARD + GAMEPAD, so it needs a
	// scheme mask that resolves them -- the engine's own default profile is
	// platform-conditional (Windows: desktop KB|MOUSE|PAD; Android: TOUCH only,
	// since a real game replaces it before any of this matters). A game's first
	// RegisterProfile call always wholesale-replaces the engine default (see
	// RegisterProfile), so this mirrors exactly what any real game with a
	// keyboard/gamepad profile does, making the test platform-independent
	// instead of accidentally passing only on desktop.
	xRig.m_xActions.RegisterProfile(1, "TestDesktop", uINPUT_SCHEME_MASK_DESKTOP);

	// The canvas reads these five and nothing else; each merges the arrow key
	// and the d-pad direction into ONE answer.
	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_UP);
	xRig.m_xActions.UpdateProfile();
	xRig.m_xActions.FinalizeReservedUI();
	// Deliberately asserted BEFORE FinalizeGameplay: the reserved set is closed
	// at 10b so the canvas can consume it before any widget takes a claim.
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(INPUT_ACTION_UI_NAV_UP),
		"the arrow key closes UI_NAV_UP at the reserved stage");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.WasPressedThisFrame(INPUT_ACTION_UI_NAV_DOWN), "and only that one");
	xRig.m_xActions.FinalizeGameplay();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(INPUT_ACTION_UI_NAV_UP),
		"the gameplay stage does not re-close, and does not clear, an already-closed action");

	xRig.BeginFrame();
	xRig.KeyUp(ZENITH_KEY_UP);
	xRig.PadDown(ZENITH_GAMEPAD_BUTTON_DPAD_DOWN);
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(INPUT_ACTION_UI_NAV_DOWN),
		"the d-pad drives the SAME action the arrow key does");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasReleasedThisFrame(INPUT_ACTION_UI_NAV_UP), "and the arrow released");

	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_LEFT);
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(INPUT_ACTION_UI_NAV_LEFT), "left");

	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_RIGHT);
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(INPUT_ACTION_UI_NAV_RIGHT), "right");
}

ZENITH_TEST(InputActions, UIConfirmSingleOwner)
{
	Zenith_ActionTestRig xRig;
	xRig.m_xActions.RegisterEngineDefaultProfiles();
	xRig.m_xActions.RegisterEngineReservedActions();
	// This test drives focus/confirm with KEYBOARD + GAMEPAD, so it needs a
	// scheme mask that resolves them -- the engine's own default profile is
	// platform-conditional (Windows: desktop KB|MOUSE|PAD; Android: TOUCH only,
	// since a real game replaces it before any of this matters). A game's first
	// RegisterProfile call always wholesale-replaces the engine default (see
	// RegisterProfile), so this mirrors exactly what any real game with a
	// keyboard/gamepad profile does, making the test platform-independent
	// instead of accidentally passing only on desktop.
	xRig.m_xActions.RegisterProfile(1, "TestDesktop", uINPUT_SCHEME_MASK_DESKTOP);

	// ONE action carries every confirm source, so the canvas has exactly one
	// question to ask and the button has none.
	ZENITH_ASSERT_EQ(xRig.m_xActions.GetBindingCount(INPUT_ACTION_UI_CONFIRM), 2u,
		"UI_CONFIRM is device-only: one key set + one pad button");

	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_ENTER);
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(INPUT_ACTION_UI_CONFIRM), "Enter confirms");

	xRig.BeginFrame();
	xRig.KeyUp(ZENITH_KEY_ENTER);
	xRig.KeyDown(ZENITH_KEY_SPACE);
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(INPUT_ACTION_UI_CONFIRM), "Space confirms too");

	xRig.BeginFrame();
	xRig.KeyUp(ZENITH_KEY_SPACE);
	xRig.CloseFrame();

	xRig.BeginFrame();
	xRig.PadDown(ZENITH_GAMEPAD_BUTTON_A);
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(INPUT_ACTION_UI_CONFIRM), "pad A confirms");

	// Claim-INDEPENDENCE: a widget owning the pointer must not be able to eat a
	// keyboard/pad confirm, which is exactly why 10b runs before the capture
	// walk and reserved rows reject pointer sources.
	xRig.BeginFrame();
	xRig.PadUp(ZENITH_GAMEPAD_BUTTON_A);
	xRig.CloseFrame();

	xRig.BeginFrame();
	xRig.TouchEvent(INPUT_EVENT_TOUCH_DOWN, 1, 10.0f, 10.0f);
	const Zenith_PointerHandle xHandle = xRig.m_xPointers.FindByPlatformId(1);
	ZENITH_ASSERT_TRUE(xRig.m_xPointers.ClaimPointer(xHandle, 0x5150u), "a widget owns the pointer");
	xRig.KeyDown(ZENITH_KEY_ENTER);
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(INPUT_ACTION_UI_CONFIRM),
		"a claimed pointer cannot suppress a keyboard confirm");
}

// ---------------------------------------------------------------------------
// Boot wiring + the test-boundary reset
// ---------------------------------------------------------------------------

ZENITH_TEST(InputActions, EngineDefaultProfilesClearedByFirstGameRegistration)
{
	Zenith_ActionTestRig xRig;
	xRig.m_xActions.RegisterEngineDefaultProfiles();

	ZENITH_ASSERT_TRUE(xRig.m_xActions.AreEngineDefaultProfilesActive(), "the engine default is installed");
	const bool bEngineProfileRegistered =
		xRig.m_xActions.IsProfileRegistered(Zenith_InputActions::uPROFILE_ENGINE_DESKTOP)
		|| xRig.m_xActions.IsProfileRegistered(Zenith_InputActions::uPROFILE_ENGINE_TOUCH);
	ZENITH_ASSERT_TRUE(bEngineProfileRegistered, "the platform's engine profile is registered");
	ZENITH_ASSERT_NE(xRig.m_xActions.GetActiveSchemeMask(), static_cast<u_int8>(0),
		"...and it is active, so a game with no profiles of its own still resolves bindings");

	// The FIRST game registration replaces them outright rather than competing
	// with them for a scheme.
	xRig.m_xActions.RegisterProfile(uTEST_PROFILE_KEYBOARD, "GameKeyboard", uINPUT_SCHEME_MASK_KEYBOARD);

	ZENITH_ASSERT_FALSE(xRig.m_xActions.AreEngineDefaultProfilesActive(), "the defaults are gone");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsProfileRegistered(Zenith_InputActions::uPROFILE_ENGINE_DESKTOP),
		"the desktop default was cleared");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsProfileRegistered(Zenith_InputActions::uPROFILE_ENGINE_TOUCH),
		"the touch default was cleared");
	ZENITH_ASSERT_EQ(xRig.m_xActions.GetActiveProfile(), uTEST_PROFILE_KEYBOARD,
		"and the game's own profile is what boots");
}

ZENITH_TEST(InputActions, ResetTransientPreservesRegistrations)
{
	g_uProfileChangeCount = 0;

	Zenith_ActionTestRig xRig;
	xRig.RegisterSplitProfiles();
	xRig.m_xActions.RegisterAction(uTEST_ACTION_A, "Fire", INPUT_ACTION_BUTTON);
	xRig.m_xActions.RegisterBinding(uTEST_ACTION_A, Zenith_InputBinding::KeySet(ZENITH_KEY_F));
	xRig.m_xActions.AddProfileChangedCallback(&Zenith_TestProfileChanged, nullptr);

	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_F);
	xRig.CloseFrame();
	xRig.m_xActions.SetProfileOverride(uTEST_PROFILE_GAMEPAD);
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsProfileOverridden(), "override latched");

	xRig.m_xActions.ResetTransientForTest();

	// Registrations are what a game installed at boot; no test boundary may take
	// them away.
	ZENITH_ASSERT_EQ(xRig.m_xActions.FindActionByName("Fire"), uTEST_ACTION_A, "the action survives");
	ZENITH_ASSERT_EQ(xRig.m_xActions.GetBindingCount(uTEST_ACTION_A), 1u, "its bindings survive");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsProfileRegistered(uTEST_PROFILE_KEYBOARD), "profiles survive");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsProfileRegistered(uTEST_PROFILE_GAMEPAD), "profiles survive");

	// ...but every transient does go.
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "held state cleared");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.WasPressedThisFrame(uTEST_ACTION_A), "edges cleared");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.WasReleasedThisFrame(uTEST_ACTION_A), "edges cleared");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsProfileOverridden(), "the override returns to AUTO");
	ZENITH_ASSERT_EQ(xRig.m_xActions.GetActiveProfile(), uTEST_PROFILE_KEYBOARD, "back on the boot default");
	ZENITH_ASSERT_EQ(static_cast<int>(xRig.m_xActions.GetLastUsedDevice()),
		static_cast<int>(INPUT_SCHEME_NONE), "the last-used device is forgotten");

	// The source shadow goes with it: the key the previous test left down must
	// not re-hold the action on the next frame.
	xRig.BeginFrame();
	xRig.CloseFrame();
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uTEST_ACTION_A), "no held state leaks across the boundary");
	ZENITH_ASSERT_GT(g_uProfileChangeCount, 0u, "the registered callback really was still installed");
}
