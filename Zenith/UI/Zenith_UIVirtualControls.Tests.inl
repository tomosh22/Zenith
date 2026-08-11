#include "Core/Zenith_TestFramework.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UIVirtualButton.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputActions.h"
#include "Input/Zenith_Pointers.h"
#include "DataStream/Zenith_DataStream.h"
#ifdef ZENITH_INPUT_SIMULATOR
#include "Input/Zenith_InputSimulator.h"
#endif

// ============================================================================
// WP3a: the on-screen controls (B9) and their frame-contract step 10d wiring.
//
// Every test here drives the REAL frame order against LOCAL instances of the
// device layer, the pointer table and the action layer (B13), so no engine
// registration is touched and no window is required:
//
//   BeginFrame        -> device frame boundary + pointer retire/edge reset
//   ...touches...     -> injected into the DEVICE layer, so both the staged
//                        touch stream (the pointer table's source) and the B3
//                        mouse projection see them exactly as Android does
//   OpenFrame         -> step 8   (activity -> profile, action frame opened)
//   CloseFrame        -> step 10b (reserved UI close)
//                        step 10c (standard-control capture walk)
//                        step 10d (VIRTUAL producers: claim + publish)
//                        step 10e (gameplay close, claim suppression)
//
// The simulator is enabled throughout: a TOOLS build suppresses widget input
// while the editor is STOPPED (the canvas is being authored, not played), which
// is exactly the state a unit at boot runs in. Enabling it also makes the
// editor viewport remap a no-op, so the raw surface coordinates below ARE the
// canvas-space coordinates the controls hit-test against.
// ============================================================================

namespace
{
	// Game action ids (>= uINPUT_ACTION_FIRST_GAME_ID; 0-15 are engine-reserved).
	static constexpr Zenith_InputActionID uVC_ACTION_MOVE  = 20;   // AXIS2D <- Virtual(0)
	static constexpr Zenith_InputActionID uVC_ACTION_FIRE  = 21;   // BUTTON <- Virtual(1)
	static constexpr Zenith_InputActionID uVC_ACTION_ALT   = 22;   // BUTTON <- Virtual(2)
	static constexpr Zenith_InputActionID uVC_ACTION_TAP   = 23;   // BUTTON <- TouchPrimary
	static constexpr Zenith_InputActionID uVC_ACTION_CLICK = 24;   // BUTTON <- MouseButton(LEFT)

	// The VIRTUAL source ids those actions' virtual rows name. A control resolves
	// them by ACTION NAME, so nothing below hard-codes one into a widget.
	static constexpr u_int16 uVC_SOURCE_MOVE = 0;
	static constexpr u_int16 uVC_SOURCE_FIRE = 1;
	static constexpr u_int16 uVC_SOURCE_ALT  = 2;

	static constexpr u_int8 uVC_PROFILE_ALL     = 20;
	static constexpr u_int8 uVC_PROFILE_TOUCH   = 21;
	static constexpr u_int8 uVC_PROFILE_DESKTOP = 22;

	static constexpr float fVC_DT = 0.016f;

	// RAII so a failing assertion cannot leave the simulator enabled for the rest
	// of the boot batch.
	struct Zenith_VirtualControlSimScope
	{
		Zenith_VirtualControlSimScope()
		{
#ifdef ZENITH_INPUT_SIMULATOR
			Zenith_InputSimulator::Enable();
#endif
		}
		~Zenith_VirtualControlSimScope()
		{
#ifdef ZENITH_INPUT_SIMULATOR
			Zenith_InputSimulator::DiscardPendingInjections();
			Zenith_InputSimulator::ResetAllInputState();
			Zenith_InputSimulator::Disable();
#endif
		}
	};

	struct Zenith_VirtualControlRig
	{
		Zenith_Input        m_xInput;
		Zenith_Pointers     m_xPointers;
		Zenith_InputActions m_xActions;

		Zenith_VirtualControlRig()
		{
			m_xActions.Initialise(m_xInput, m_xPointers);

			m_xActions.RegisterAction(uVC_ACTION_MOVE, "VC_Move", INPUT_ACTION_AXIS2D);
			m_xActions.RegisterBinding(uVC_ACTION_MOVE, Zenith_InputBinding::Virtual(uVC_SOURCE_MOVE));

			m_xActions.RegisterAction(uVC_ACTION_FIRE, "VC_Fire", INPUT_ACTION_BUTTON);
			m_xActions.RegisterBinding(uVC_ACTION_FIRE, Zenith_InputBinding::Virtual(uVC_SOURCE_FIRE));

			m_xActions.RegisterAction(uVC_ACTION_ALT, "VC_Alt", INPUT_ACTION_BUTTON);
			m_xActions.RegisterBinding(uVC_ACTION_ALT, Zenith_InputBinding::Virtual(uVC_SOURCE_ALT));

			m_xActions.RegisterAction(uVC_ACTION_TAP, "VC_Tap", INPUT_ACTION_BUTTON);
			m_xActions.RegisterBinding(uVC_ACTION_TAP, Zenith_InputBinding::TouchPrimary());

			m_xActions.RegisterAction(uVC_ACTION_CLICK, "VC_Click", INPUT_ACTION_BUTTON);
			m_xActions.RegisterBinding(uVC_ACTION_CLICK, Zenith_InputBinding::MouseButton(ZENITH_MOUSE_BUTTON_LEFT));
		}

		// One profile carrying every scheme: the default for tests that are not
		// ABOUT masks. TOUCH is in it, so the on-screen controls are live.
		void RegisterAllSchemeProfile()
		{
			m_xActions.RegisterProfile(uVC_PROFILE_ALL, "VCAll",
				uINPUT_SCHEME_MASK_KEYBOARD | uINPUT_SCHEME_MASK_MOUSE
				| uINPUT_SCHEME_MASK_TOUCH | uINPUT_SCHEME_MASK_GAMEPAD);
		}

		// Two DISJOINT profiles: a desktop one (the boot default on Windows,
		// because it owns the KEYBOARD) and a touch-only one the first finger
		// switches to.
		void RegisterSplitProfiles()
		{
			m_xActions.RegisterProfile(uVC_PROFILE_DESKTOP, "VCDesktop",
				uINPUT_SCHEME_MASK_KEYBOARD | uINPUT_SCHEME_MASK_MOUSE | uINPUT_SCHEME_MASK_GAMEPAD);
			m_xActions.RegisterProfile(uVC_PROFILE_TOUCH, "VCTouch", uINPUT_SCHEME_MASK_TOUCH);
		}

		void BeginFrame(float fDisplayScale = 1.0f)
		{
			m_xInput.DrainPendingPlatformEvents();
			m_xPointers.BeginFrame(fDisplayScale);
		}

		// Injected into the DEVICE layer, not straight into the pointer table: it
		// is what stages the touch (the pointer table's own source) AND what runs
		// the B3 projection onto the mouse view, so a test sees the same pair of
		// consequences a real finger produces.
		void Touch(Zenith_InputEventType eType, int32_t iPointerId, float fX, float fY)
		{
			Zenith_InputEvent xEvent;
			xEvent.m_eType    = eType;
			xEvent.m_iCode    = iPointerId;
			xEvent.m_fX       = fX;
			xEvent.m_fY       = fY;
			xEvent.m_fAnchorX = fX;
			xEvent.m_fAnchorY = fY;
			m_xInput.AppendInjectedEvent(xEvent);
		}

		// Step 8, plus the pointer table's consume of what the touches staged.
		void OpenFrame()
		{
			m_xPointers.ConsumeStagedTail(m_xInput);
			m_xActions.UpdateProfile();
		}

		// Steps 10b -> 10c -> 10d -> 10e, in the order the frame contract fixes.
		void CloseFrame(Zenith_UI::Zenith_UICanvas& xCanvas)
		{
			m_xActions.FinalizeReservedUI();
			xCanvas.UpdatePointerInput(m_xPointers, fVC_DT);
			xCanvas.UpdateVirtualControls(m_xActions, m_xPointers, fVC_DT);
			m_xActions.FinalizeGameplay();
		}

		void RunFrame(Zenith_UI::Zenith_UICanvas& xCanvas)
		{
			OpenFrame();
			CloseFrame(xCanvas);
		}

		Zenith_Maths::Vector2 GetMoveAxis()
		{
			Zenith_Maths::Vector2 xValue;
			m_xActions.GetAxis2D(uVC_ACTION_MOVE, xValue);
			return xValue;
		}
	};

	// Bounds (100,100)-(300,300): anchor and pivot are top-left by default, so
	// these are absolute regardless of the size the window reports.
	Zenith_UI::Zenith_UIVirtualStick* Zenith_AddTestStick(Zenith_UI::Zenith_UICanvas& xCanvas,
		Zenith_UI::Zenith_UIVirtualStick::StickMode eMode)
	{
		Zenith_UI::Zenith_UIVirtualStick* pxStick = new Zenith_UI::Zenith_UIVirtualStick("TestStick");
		pxStick->SetPosition(100.0f, 100.0f);
		pxStick->SetSize(200.0f, 200.0f);
		pxStick->SetMode(eMode);
		pxStick->SetRadius(100.0f);
		pxStick->SetDeadzoneFraction(0.15f);
		pxStick->SetAction("VC_Move");
		xCanvas.AddElement(pxStick);
		return pxStick;
	}

	// Bounds (500,100)-(600,200).
	Zenith_UI::Zenith_UIVirtualButton* Zenith_AddTestVirtualButton(Zenith_UI::Zenith_UICanvas& xCanvas,
		const char* szAction)
	{
		Zenith_UI::Zenith_UIVirtualButton* pxButton = new Zenith_UI::Zenith_UIVirtualButton("TestVirtualButton");
		pxButton->SetPosition(500.0f, 100.0f);
		pxButton->SetSize(100.0f, 100.0f);
		pxButton->SetAction(szAction);
		xCanvas.AddElement(pxButton);
		return pxButton;
	}
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

ZENITH_TEST(UIVirtualControls, VirtualStickDeadzoneAndClamp)
{
	using Stick = Zenith_UI::Zenith_UIVirtualStick;

	const Zenith_Maths::Vector2 xBase(200.0f, 200.0f);

	// Inside the deadzone the axis is EXACTLY zero, not merely small: a resting
	// thumb must not walk a character across a room.
	Zenith_Maths::Vector2 xAxis = Stick::ResolveAxis(xBase, Zenith_Maths::Vector2(205.0f, 200.0f), 100.0f, 0.15f);
	ZENITH_ASSERT_EQ_FLOAT(xAxis.x, 0.0f, 1e-5f, "5px of a 15px deadzone is zero");
	ZENITH_ASSERT_EQ_FLOAT(xAxis.y, 0.0f, 1e-5f, "...on both axes");

	// AT the radius the magnitude is exactly 1...
	xAxis = Stick::ResolveAxis(xBase, Zenith_Maths::Vector2(300.0f, 200.0f), 100.0f, 0.15f);
	ZENITH_ASSERT_EQ_FLOAT(xAxis.x, 1.0f, 1e-5f, "the rim is full tilt");
	// ...and beyond it, still 1: a thumb that leaves the ring does not overdrive.
	xAxis = Stick::ResolveAxis(xBase, Zenith_Maths::Vector2(900.0f, 200.0f), 100.0f, 0.15f);
	ZENITH_ASSERT_EQ_FLOAT(xAxis.x, 1.0f, 1e-5f, "past the rim CLAMPS to 1");

	// The band is RESCALED, so leaving the deadzone starts at 0 rather than
	// jumping to deadzone/radius. Half way along the 85px band is 0.5.
	xAxis = Stick::ResolveAxis(xBase, Zenith_Maths::Vector2(200.0f + 15.0f + 42.5f, 200.0f), 100.0f, 0.15f);
	ZENITH_ASSERT_EQ_FLOAT(xAxis.x, 0.5f, 1e-4f, "the live band is rescaled to [0,1]");

	// Canvas +Y is DOWN and the action layer's +Y is FORWARD: pulling the thumb
	// UP the screen must read as +y.
	xAxis = Stick::ResolveAxis(xBase, Zenith_Maths::Vector2(200.0f, 100.0f), 100.0f, 0.15f);
	ZENITH_ASSERT_EQ_FLOAT(xAxis.y, 1.0f, 1e-5f, "up the screen is +y FORWARD");
	xAxis = Stick::ResolveAxis(xBase, Zenith_Maths::Vector2(200.0f, 300.0f), 100.0f, 0.15f);
	ZENITH_ASSERT_EQ_FLOAT(xAxis.y, -1.0f, 1e-5f, "down the screen is -y");

	// ...and the same curve reaches the ACTION through a real frame.
	Zenith_VirtualControlSimScope xSimScope;
	Zenith_UI::Zenith_UICanvas xCanvas;
	Zenith_AddTestStick(xCanvas, Stick::StickMode::FIXED);

	Zenith_VirtualControlRig xRig;
	xRig.RegisterAllSchemeProfile();

	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_DOWN, 1, 200.0f, 200.0f);   // dead centre
	xRig.RunFrame(xCanvas);
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uVC_ACTION_MOVE), "an engaged stick HOLDS its axis action");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(uVC_ACTION_MOVE), "with a press edge on the down");
	ZENITH_ASSERT_EQ_FLOAT(xRig.GetMoveAxis().x, 0.0f, 1e-5f, "a thumb at the centre publishes zero");

	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_MOVE, 1, 300.0f, 200.0f);
	xRig.RunFrame(xCanvas);
	ZENITH_ASSERT_EQ_FLOAT(xRig.GetMoveAxis().x, 1.0f, 1e-5f, "the published axis follows the thumb");
	ZENITH_ASSERT_EQ_FLOAT(xRig.GetMoveAxis().y, 0.0f, 1e-5f, "and stays on-axis");
}

ZENITH_TEST(UIVirtualControls, VirtualStickFloatingRecentre)
{
	using Stick = Zenith_UI::Zenith_UIVirtualStick;

	Zenith_VirtualControlSimScope xSimScope;
	Zenith_UI::Zenith_UICanvas xCanvas;
	Stick* pxStick = Zenith_AddTestStick(xCanvas, Stick::StickMode::FLOATING);

	Zenith_VirtualControlRig xRig;
	xRig.RegisterAllSchemeProfile();

	// Down well away from the rect's centre (200,200).
	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_DOWN, 1, 140.0f, 260.0f);
	xRig.RunFrame(xCanvas);

	ZENITH_ASSERT_TRUE(pxStick->IsEngaged(), "the floating stick engaged");
	ZENITH_ASSERT_EQ_FLOAT(pxStick->GetBaseCentre().x, 140.0f, 1e-4f, "the base RE-CENTRED on the down position");
	ZENITH_ASSERT_EQ_FLOAT(pxStick->GetBaseCentre().y, 260.0f, 1e-4f, "...on both axes");
	ZENITH_ASSERT_EQ_FLOAT(xRig.GetMoveAxis().x, 0.0f, 1e-5f, "so the FIRST frame reads zero -- no jump");
	ZENITH_ASSERT_EQ_FLOAT(xRig.GetMoveAxis().y, 0.0f, 1e-5f, "...on both axes");

	// The contrast that makes the recentre meaningful: measured from the FIXED
	// base, that very same down position is most of a full tilt.
	const Zenith_Maths::Vector2 xFixedAxis = Stick::ResolveAxis(
		Zenith_Maths::Vector2(200.0f, 200.0f), Zenith_Maths::Vector2(140.0f, 260.0f), 100.0f, 0.15f);
	ZENITH_ASSERT_GT(xFixedAxis.x * xFixedAxis.x + xFixedAxis.y * xFixedAxis.y, 0.5f,
		"a FIXED stick would have read a large tilt from the same touch");

	// From the new base, a radius' travel up the screen is full forward.
	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_MOVE, 1, 140.0f, 160.0f);
	xRig.RunFrame(xCanvas);
	ZENITH_ASSERT_EQ_FLOAT(xRig.GetMoveAxis().y, 1.0f, 1e-5f, "travel is measured from the RECENTRED base");

	// A second gesture re-centres again, somewhere else entirely.
	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_UP, 1, 140.0f, 160.0f);
	xRig.RunFrame(xCanvas);
	xRig.BeginFrame();
	xRig.RunFrame(xCanvas);

	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_DOWN, 2, 280.0f, 130.0f);
	xRig.RunFrame(xCanvas);
	ZENITH_ASSERT_EQ_FLOAT(pxStick->GetBaseCentre().x, 280.0f, 1e-4f, "every gesture re-centres");
	ZENITH_ASSERT_EQ_FLOAT(pxStick->GetBaseCentre().y, 130.0f, 1e-4f, "...on both axes");
}

ZENITH_TEST(UIVirtualControls, VirtualControlTouchTargetIsDensityScaled)
{
	using Element = Zenith_UI::Zenith_UIElement;

	// A 10x10 authored control with an 8px slop. At 1x the touch target is grown
	// to the 57px minimum; at 3x both terms scale with the panel.
	const Zenith_Maths::Vector4 xBounds(100.0f, 100.0f, 110.0f, 110.0f);

	Zenith_Maths::Vector4 xRect = Element::ResolveTouchTargetRect(xBounds, 8.0f, 1.0f);
	ZENITH_ASSERT_EQ_FLOAT(xRect.z - xRect.x, 57.0f, 1e-4f, "grown to the minimum touch target at 1x");
	ZENITH_ASSERT_EQ_FLOAT((xRect.x + xRect.z) * 0.5f, 105.0f, 1e-4f, "...about its own centre, not its corner");

	xRect = Element::ResolveTouchTargetRect(xBounds, 8.0f, 3.0f);
	ZENITH_ASSERT_EQ_FLOAT(xRect.z - xRect.x, 171.0f, 1e-4f, "57 LOGICAL px is 171 device px at 3x");

	// A big control keeps its own size plus the SCALED slop, rather than being
	// clamped to the minimum.
	const Zenith_Maths::Vector4 xBig(0.0f, 0.0f, 200.0f, 200.0f);
	xRect = Element::ResolveTouchTargetRect(xBig, 8.0f, 2.0f);
	ZENITH_ASSERT_EQ_FLOAT(xRect.z - xRect.x, 232.0f, 1e-4f, "200 + 2 * (8 * 2x) of slop");

	// ...and the same rule decides a real claim. The 10x10 button reaches 45px
	// out at 2x and only 28.5px at 1x, so the SAME touch lands on the control on
	// a dense panel and misses on a sparse one.
	Zenith_VirtualControlSimScope xSimScope;
	Zenith_UI::Zenith_UICanvas xCanvas;
	Zenith_UI::Zenith_UIVirtualButton* pxButton = Zenith_AddTestVirtualButton(xCanvas, "VC_Fire");
	pxButton->SetPosition(100.0f, 100.0f);
	pxButton->SetSize(10.0f, 10.0f);
	pxButton->SetHitSlop(8.0f);

	Zenith_VirtualControlRig xRig;
	xRig.RegisterAllSchemeProfile();

	xRig.BeginFrame(1.0f);
	xRig.Touch(INPUT_EVENT_TOUCH_DOWN, 1, 150.0f, 150.0f);
	xRig.RunFrame(xCanvas);
	ZENITH_ASSERT_FALSE(pxButton->IsHeld(), "45px away is outside the 1x touch target");

	xRig.BeginFrame(1.0f);
	xRig.Touch(INPUT_EVENT_TOUCH_UP, 1, 150.0f, 150.0f);
	xRig.RunFrame(xCanvas);
	xRig.BeginFrame(2.0f);
	xRig.RunFrame(xCanvas);

	xRig.BeginFrame(2.0f);
	xRig.Touch(INPUT_EVENT_TOUCH_DOWN, 2, 150.0f, 150.0f);
	xRig.RunFrame(xCanvas);
	ZENITH_ASSERT_TRUE(pxButton->IsHeld(), "...and INSIDE it once the panel is 2x");
}

// ---------------------------------------------------------------------------
// Publishing (step 10d -> the 10e replay)
// ---------------------------------------------------------------------------

ZENITH_TEST(UIVirtualControls, SameFrameVirtualTapFiresBothEdges)
{
	Zenith_VirtualControlSimScope xSimScope;
	Zenith_UI::Zenith_UICanvas xCanvas;
	Zenith_UI::Zenith_UIVirtualButton* pxButton = Zenith_AddTestVirtualButton(xCanvas, "VC_Fire");

	Zenith_VirtualControlRig xRig;
	xRig.RegisterAllSchemeProfile();

	// Press AND release inside ONE frame -- every quick tap on glass. Both
	// publishes land as ORDERED virtual transitions, so the 10e replay sees a
	// genuine rise and fall rather than a level that never moved.
	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_DOWN, 1, 550.0f, 150.0f);
	xRig.Touch(INPUT_EVENT_TOUCH_UP,   1, 550.0f, 150.0f);
	xRig.RunFrame(xCanvas);

	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(uVC_ACTION_FIRE), "the tap pressed");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasReleasedThisFrame(uVC_ACTION_FIRE), "...and released");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uVC_ACTION_FIRE), "and is not left held");
	ZENITH_ASSERT_FALSE(pxButton->IsHeld(), "the widget ends the frame idle too");
}

ZENITH_TEST(UIVirtualControls, SkippedRenderFrameDoesNotLatchVirtualControls)
{
	Zenith_VirtualControlSimScope xSimScope;
	Zenith_UI::Zenith_UICanvas xCanvas;
	Zenith_AddTestStick(xCanvas, Zenith_UI::Zenith_UIVirtualStick::StickMode::FIXED);

	Zenith_VirtualControlRig xRig;
	xRig.RegisterAllSchemeProfile();

	// NOT ONE Update() OR Render() CALL IN THIS TEST. The visual pass is skipped
	// on any frame that submits no render work (scene transitions, and the whole
	// of a headless run), and a control that decided anything there would freeze
	// at whatever it last saw. A full gesture therefore has to complete with the
	// input phase alone.
	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_DOWN, 1, 200.0f, 200.0f);
	xRig.RunFrame(xCanvas);
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uVC_ACTION_MOVE), "the gesture started without a visual pass");

	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_MOVE, 1, 300.0f, 200.0f);
	xRig.RunFrame(xCanvas);
	ZENITH_ASSERT_EQ_FLOAT(xRig.GetMoveAxis().x, 1.0f, 1e-5f, "and the axis tracks (+x)");

	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_MOVE, 1, 200.0f, 100.0f);
	xRig.RunFrame(xCanvas);
	ZENITH_ASSERT_EQ_FLOAT(xRig.GetMoveAxis().x, 0.0f, 1e-5f, "...and again (x back to 0)");
	ZENITH_ASSERT_EQ_FLOAT(xRig.GetMoveAxis().y, 1.0f, 1e-5f, "...and again (+y forward)");

	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_UP, 1, 200.0f, 100.0f);
	xRig.RunFrame(xCanvas);
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uVC_ACTION_MOVE), "and the lift releases");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasReleasedThisFrame(uVC_ACTION_MOVE), "with a release edge");
	ZENITH_ASSERT_EQ_FLOAT(xRig.GetMoveAxis().x, 0.0f, 1e-5f, "and the axis is zeroed, not frozen");
	ZENITH_ASSERT_EQ_FLOAT(xRig.GetMoveAxis().y, 0.0f, 1e-5f, "...on both axes");
}

// ---------------------------------------------------------------------------
// Disarm rules: retarget, hide, mask change (B9)
// ---------------------------------------------------------------------------

ZENITH_TEST(UIVirtualControls, RetargetWhileHeldDisarmsUntilFreshDown)
{
	Zenith_VirtualControlSimScope xSimScope;
	Zenith_UI::Zenith_UICanvas xCanvas;
	Zenith_UI::Zenith_UIVirtualButton* pxButton = Zenith_AddTestVirtualButton(xCanvas, "VC_Fire");

	Zenith_VirtualControlRig xRig;
	xRig.RegisterAllSchemeProfile();

	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_DOWN, 1, 550.0f, 150.0f);
	xRig.RunFrame(xCanvas);
	const Zenith_PointerHandle xHandle = xRig.m_xPointers.FindByPlatformId(1);
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uVC_ACTION_FIRE), "held on the original action");
	ZENITH_ASSERT_TRUE(xRig.m_xPointers.IsClaimed(xHandle), "and the control owns the pointer");

	// Retarget MID-HOLD (this is what a WP3b context switch does at step 9).
	pxButton->SetAction("VC_Alt");

	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_MOVE, 1, 552.0f, 152.0f);
	xRig.RunFrame(xCanvas);

	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uVC_ACTION_FIRE), "the OLD action is no longer held");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasReleasedThisFrame(uVC_ACTION_FIRE),
		"...and it received a genuine RELEASE transition, not a silent drop");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uVC_ACTION_ALT),
		"the NEW action does NOT inherit the finger already on the glass");
	ZENITH_ASSERT_TRUE(pxButton->IsDisarmed(), "the control is disarmed");
	ZENITH_ASSERT_TRUE(xRig.m_xPointers.IsClaimed(xHandle),
		"but the CLAIM is kept -- the gesture stays consumed, so nothing downstream sees it either");

	// Still down, still nothing: a disarm lasts until a FRESH down.
	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_MOVE, 1, 554.0f, 154.0f);
	xRig.RunFrame(xCanvas);
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uVC_ACTION_ALT), "holding on does not arm the new action");
	ZENITH_ASSERT_TRUE(xRig.m_xPointers.IsClaimed(xHandle), "the claim is still held");

	// Lift: the claim goes back and the disarm lifts with it.
	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_UP, 1, 554.0f, 154.0f);
	xRig.RunFrame(xCanvas);
	ZENITH_ASSERT_FALSE(pxButton->IsDisarmed(), "the gesture ended, so the disarm ended with it");
	ZENITH_ASSERT_FALSE(xRig.m_xPointers.IsClaimed(xHandle), "and the claim was handed back");

	// A FRESH down drives the NEW action.
	xRig.BeginFrame();
	xRig.RunFrame(xCanvas);
	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_DOWN, 2, 550.0f, 150.0f);
	xRig.RunFrame(xCanvas);
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uVC_ACTION_ALT), "a fresh down drives the retargeted action");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(uVC_ACTION_ALT), "with its press edge");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uVC_ACTION_FIRE), "and the old action stays quiet");
}

ZENITH_TEST(UIVirtualControls, VirtualZeroWhenHiddenOrMaskChange)
{
	using Stick = Zenith_UI::Zenith_UIVirtualStick;

	// ---- Hidden mid-gesture ------------------------------------------------
	{
		Zenith_VirtualControlSimScope xSimScope;
		Zenith_UI::Zenith_UICanvas xCanvas;
		Stick* pxStick = Zenith_AddTestStick(xCanvas, Stick::StickMode::FIXED);

		Zenith_VirtualControlRig xRig;
		xRig.RegisterAllSchemeProfile();

		xRig.BeginFrame();
		xRig.Touch(INPUT_EVENT_TOUCH_DOWN, 1, 300.0f, 200.0f);
		xRig.RunFrame(xCanvas);
		const Zenith_PointerHandle xHandle = xRig.m_xPointers.FindByPlatformId(1);
		ZENITH_ASSERT_EQ_FLOAT(xRig.GetMoveAxis().x, 1.0f, 1e-5f, "engaged at full tilt");

		// Hidden with the finger still down. The 10d walk is deliberately blind to
		// visibility for exactly this: an element it stopped visiting could never
		// publish the release, and the action would stay held forever with the
		// last axis frozen into it.
		pxStick->SetVisible(false);

		xRig.BeginFrame();
		xRig.Touch(INPUT_EVENT_TOUCH_MOVE, 1, 300.0f, 200.0f);
		xRig.RunFrame(xCanvas);
		ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uVC_ACTION_MOVE), "hiding released the action");
		ZENITH_ASSERT_TRUE(xRig.m_xActions.WasReleasedThisFrame(uVC_ACTION_MOVE), "with a release edge");
		ZENITH_ASSERT_EQ_FLOAT(xRig.GetMoveAxis().x, 0.0f, 1e-5f, "and ZEROED the axis");
		ZENITH_ASSERT_EQ_FLOAT(xRig.GetMoveAxis().y, 0.0f, 1e-5f, "...on both axes");
		ZENITH_ASSERT_TRUE(xRig.m_xPointers.IsClaimed(xHandle), "the claim is KEPT -- the gesture stays consumed");
		ZENITH_ASSERT_TRUE(pxStick->IsDisarmed(), "and the control is disarmed");

		// Shown again with the SAME finger down: still nothing, until a fresh down.
		pxStick->SetVisible(true);
		xRig.BeginFrame();
		xRig.Touch(INPUT_EVENT_TOUCH_MOVE, 1, 300.0f, 200.0f);
		xRig.RunFrame(xCanvas);
		ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uVC_ACTION_MOVE),
			"re-showing does not resurrect a gesture that started before it");

		xRig.BeginFrame();
		xRig.Touch(INPUT_EVENT_TOUCH_UP, 1, 300.0f, 200.0f);
		xRig.RunFrame(xCanvas);
		xRig.BeginFrame();
		xRig.RunFrame(xCanvas);

		xRig.BeginFrame();
		xRig.Touch(INPUT_EVENT_TOUCH_DOWN, 2, 300.0f, 200.0f);
		xRig.RunFrame(xCanvas);
		ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uVC_ACTION_MOVE), "a fresh down engages again");
	}

	// ---- Mask change mid-gesture -------------------------------------------
	{
		Zenith_VirtualControlSimScope xSimScope;
		Zenith_UI::Zenith_UICanvas xCanvas;
		Stick* pxStick = Zenith_AddTestStick(xCanvas, Stick::StickMode::FIXED);

		Zenith_VirtualControlRig xRig;
		xRig.RegisterSplitProfiles();
		xRig.m_xActions.SetProfileOverride(uVC_PROFILE_TOUCH);

		xRig.BeginFrame();
		xRig.Touch(INPUT_EVENT_TOUCH_DOWN, 1, 300.0f, 200.0f);
		xRig.RunFrame(xCanvas);
		const Zenith_PointerHandle xHandle = xRig.m_xPointers.FindByPlatformId(1);
		ZENITH_ASSERT_EQ_FLOAT(xRig.GetMoveAxis().x, 1.0f, 1e-5f, "engaged under the touch profile");

		// The profile loses TOUCH while the thumb is still on the glass.
		xRig.BeginFrame();
		xRig.Touch(INPUT_EVENT_TOUCH_MOVE, 1, 300.0f, 200.0f);
		xRig.OpenFrame();
		xRig.m_xActions.SetProfileOverride(uVC_PROFILE_DESKTOP);
		xRig.CloseFrame(xCanvas);

		ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uVC_ACTION_MOVE), "the masked-out action is released");
		ZENITH_ASSERT_TRUE(xRig.m_xActions.WasReleasedThisFrame(uVC_ACTION_MOVE), "with a release edge");
		ZENITH_ASSERT_EQ_FLOAT(xRig.GetMoveAxis().x, 0.0f, 1e-5f, "and a zeroed axis");
		ZENITH_ASSERT_TRUE(xRig.m_xPointers.IsClaimed(xHandle), "the claim is KEPT");
		ZENITH_ASSERT_TRUE(pxStick->IsDisarmed(), "and the control is disarmed");

		// Back to TOUCH with the finger STILL down. This is the case the zeroing
		// exists for: if the widget had merely stopped publishing, the virtual
		// source would still read HELD and the mask rebase would resurrect the
		// action without anybody touching anything.
		xRig.BeginFrame();
		xRig.Touch(INPUT_EVENT_TOUCH_MOVE, 1, 300.0f, 200.0f);
		xRig.OpenFrame();
		xRig.m_xActions.SetProfileOverride(uVC_PROFILE_TOUCH);
		xRig.CloseFrame(xCanvas);
		ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uVC_ACTION_MOVE),
			"returning to the touch profile does NOT resurrect a stuck virtual source");
		ZENITH_ASSERT_EQ_FLOAT(xRig.GetMoveAxis().x, 0.0f, 1e-5f, "and the axis stays zero");
	}
}

ZENITH_TEST(UIVirtualControls, FirstTouchInKeyboardProfileIsClaimedSameFrame)
{
	Zenith_VirtualControlSimScope xSimScope;
	Zenith_UI::Zenith_UICanvas xCanvas;
	Zenith_UI::Zenith_UIVirtualStick* pxStick =
		Zenith_AddTestStick(xCanvas, Zenith_UI::Zenith_UIVirtualStick::StickMode::FIXED);

	Zenith_VirtualControlRig xRig;
	xRig.RegisterSplitProfiles();
	ZENITH_ASSERT_EQ(xRig.m_xActions.GetActiveProfile(), uVC_PROFILE_DESKTOP,
		"boots on the desktop profile, where the on-screen controls do not exist");

	// The VERY FIRST finger, arriving while a keyboard/mouse profile is active.
	// Step 8 runs before the UI phase, so by the time 10d walks the canvas the
	// profile has already switched and the control is live -- the first touch is
	// claimed in the SAME frame it arrived, not swallowed and replayed on the
	// next one.
	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_DOWN, 1, 300.0f, 200.0f);
	xRig.RunFrame(xCanvas);

	ZENITH_ASSERT_EQ(xRig.m_xActions.GetActiveProfile(), uVC_PROFILE_TOUCH,
		"the touch switched the profile at step 8");
	const Zenith_PointerHandle xHandle = xRig.m_xPointers.FindByPlatformId(1);
	ZENITH_ASSERT_TRUE(xRig.m_xPointers.IsClaimed(xHandle), "and the control claimed that same first touch");
	ZENITH_ASSERT_TRUE(pxStick->IsEngaged(), "the control is engaged on frame one");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uVC_ACTION_MOVE), "and the action is already held");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(uVC_ACTION_MOVE), "with its press edge");
}

// ---------------------------------------------------------------------------
// Claim suppression at 10e (B1)
// ---------------------------------------------------------------------------

ZENITH_TEST(UIVirtualControls, StandardButtonDownSuppressesTouchPrimary)
{
	Zenith_VirtualControlSimScope xSimScope;
	Zenith_UI::Zenith_UICanvas xCanvas;

	Zenith_UI::Zenith_UIButton* pxButton = new Zenith_UI::Zenith_UIButton("Go", "SuppressionTestButton");
	pxButton->SetPosition(100.0f, 100.0f);
	pxButton->SetSize(200.0f, 50.0f);
	xCanvas.AddElement(pxButton);

	Zenith_VirtualControlRig xRig;
	xRig.RegisterAllSchemeProfile();

	// A press the button takes at 10c is a press gameplay must never see.
	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_DOWN, 1, 150.0f, 120.0f);
	xRig.RunFrame(xCanvas);
	ZENITH_ASSERT_TRUE(xRig.m_xPointers.IsClaimed(xRig.m_xPointers.FindByPlatformId(1)), "the button claimed it");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uVC_ACTION_TAP), "so the TOUCH_PRIMARY row is suppressed");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.WasPressedThisFrame(uVC_ACTION_TAP), "with no press edge either");

	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_UP, 1, 150.0f, 120.0f);
	xRig.RunFrame(xCanvas);
	xRig.BeginFrame();
	xRig.RunFrame(xCanvas);

	// The same press somewhere no widget wants reaches gameplay untouched -- the
	// suppression is a CLAIM rule, not a blanket "UI exists" rule.
	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_DOWN, 2, 900.0f, 600.0f);
	xRig.RunFrame(xCanvas);
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uVC_ACTION_TAP), "an unclaimed press still drives gameplay");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(uVC_ACTION_TAP), "with its press edge");
}

ZENITH_TEST(UIVirtualControls, VirtualStickDownSuppressesTouchPrimary)
{
	Zenith_VirtualControlSimScope xSimScope;
	Zenith_UI::Zenith_UICanvas xCanvas;
	Zenith_AddTestStick(xCanvas, Zenith_UI::Zenith_UIVirtualStick::StickMode::FIXED);

	Zenith_VirtualControlRig xRig;
	xRig.RegisterAllSchemeProfile();

	// The stick claims at 10d, which still runs BEFORE the gameplay close -- so a
	// virtual producer consumes a gesture exactly as a standard control does.
	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_DOWN, 1, 200.0f, 200.0f);
	xRig.RunFrame(xCanvas);
	ZENITH_ASSERT_TRUE(xRig.m_xPointers.IsClaimed(xRig.m_xPointers.FindByPlatformId(1)), "the stick claimed it");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uVC_ACTION_TAP), "so the TOUCH_PRIMARY row is suppressed");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uVC_ACTION_MOVE), "while the stick's OWN action is driven");

	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_UP, 1, 200.0f, 200.0f);
	xRig.RunFrame(xCanvas);
	xRig.BeginFrame();
	xRig.RunFrame(xCanvas);

	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_DOWN, 2, 900.0f, 600.0f);
	xRig.RunFrame(xCanvas);
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uVC_ACTION_TAP), "a press outside the control still reaches gameplay");
}

ZENITH_TEST(UIVirtualControls, ClaimedProjectionClickSuppressesGameplayMouseAction)
{
	Zenith_VirtualControlSimScope xSimScope;
	Zenith_UI::Zenith_UICanvas xCanvas;
	Zenith_AddTestStick(xCanvas, Zenith_UI::Zenith_UIVirtualStick::StickMode::FIXED);

	Zenith_VirtualControlRig xRig;
	xRig.RegisterAllSchemeProfile();

	// B3 makes the first finger and the mouse view the same thing, so this touch
	// ALSO raises a MOUSE_PRESS transition. A MOUSE_BUTTON row is claim-filtered
	// on exactly that basis: the projected click belongs to whoever owns the
	// pointer, and here that is the stick.
	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_DOWN, 1, 200.0f, 200.0f);
	xRig.RunFrame(xCanvas);
	// GetPrimaryPointerId, not IsMouseButtonHeld: the held-key queries are
	// sim-aware and the simulator is enabled here, so the raw projection state is
	// the honest witness that B3 ran at all.
	ZENITH_ASSERT_EQ(xRig.m_xInput.GetPrimaryPointerId(), 1,
		"the B3 projection took this finger as the mouse view");
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(uVC_ACTION_CLICK),
		"and the claimed pointer suppresses the MOUSE_BUTTON row it raised");

	// The matching RELEASE is suppressed with it, so the row cannot emit a lone
	// release edge for a press it never saw.
	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_UP, 1, 200.0f, 200.0f);
	xRig.RunFrame(xCanvas);
	ZENITH_ASSERT_FALSE(xRig.m_xActions.WasReleasedThisFrame(uVC_ACTION_CLICK),
		"a suppressed press suppresses its release");

	xRig.BeginFrame();
	xRig.RunFrame(xCanvas);

	// Away from every control, the same projected click drives gameplay.
	xRig.BeginFrame();
	xRig.Touch(INPUT_EVENT_TOUCH_DOWN, 2, 900.0f, 600.0f);
	xRig.RunFrame(xCanvas);
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(uVC_ACTION_CLICK),
		"an unclaimed projected click still drives the MOUSE_BUTTON row");
}

// ---------------------------------------------------------------------------
// Serialization
// ---------------------------------------------------------------------------

ZENITH_TEST(UIVirtualControls, VirtualControlSerializationRoundTrip)
{
	using Stick = Zenith_UI::Zenith_UIVirtualStick;

	// A TRANSIENT canvas -- the exact payload a scene's Zenith_UIComponent writes
	// (its WriteToDataStream is this call) -- carrying BOTH new control types
	// plus an existing widget, so the APPENDED enum values are proved not to
	// have disturbed the tag of anything already on disk.
	Zenith_UI::Zenith_UICanvas xSource;

	Stick* pxStick = new Stick("SerializedStick");
	pxStick->SetPosition(40.0f, 50.0f);
	pxStick->SetSize(180.0f, 190.0f);
	pxStick->SetMode(Stick::StickMode::FLOATING);
	pxStick->SetRadius(123.0f);
	pxStick->SetDeadzoneFraction(0.25f);
	pxStick->SetActivationSlop(17.0f);
	pxStick->SetKnobColor({0.1f, 0.2f, 0.3f, 0.4f});
	pxStick->SetAction("VC_Move");
	xSource.AddElement(pxStick);

	Zenith_UI::Zenith_UIVirtualButton* pxVirtualButton =
		new Zenith_UI::Zenith_UIVirtualButton("SerializedVirtualButton");
	pxVirtualButton->SetPosition(600.0f, 700.0f);
	pxVirtualButton->SetSize(88.0f, 99.0f);
	pxVirtualButton->SetHitSlop(23.0f);
	pxVirtualButton->SetPressedColor({0.9f, 0.8f, 0.7f, 0.6f});
	pxVirtualButton->SetAction("VC_Fire");
	xSource.AddElement(pxVirtualButton);

	Zenith_UI::Zenith_UIButton* pxPlainButton = new Zenith_UI::Zenith_UIButton("Menu", "SerializedPlainButton");
	xSource.AddElement(pxPlainButton);

	Zenith_DataStream xStream;
	xSource.WriteToDataStream(xStream);

	xStream.SetCursor(0);
	Zenith_UI::Zenith_UICanvas xLoaded;
	xLoaded.ReadFromDataStream(xStream);

	ZENITH_ASSERT_EQ(xLoaded.GetElementCount(), static_cast<size_t>(3), "all three elements round-tripped");

	Zenith_UI::Zenith_UIElement* pxLoadedStickElement = xLoaded.FindElement("SerializedStick");
	ZENITH_ASSERT_NOT_NULL(pxLoadedStickElement, "the stick came back by name");
	ZENITH_ASSERT_EQ(pxLoadedStickElement->GetType(), Zenith_UI::UIElementType::VirtualStick,
		"...as a VirtualStick, so the factory and the appended tag agree");

	Stick* pxLoadedStick = static_cast<Stick*>(pxLoadedStickElement);
	ZENITH_ASSERT_EQ(static_cast<int>(pxLoadedStick->GetMode()), static_cast<int>(Stick::StickMode::FLOATING), "mode");
	ZENITH_ASSERT_EQ_FLOAT(pxLoadedStick->GetRadius(), 123.0f, 1e-4f, "radius");
	ZENITH_ASSERT_EQ_FLOAT(pxLoadedStick->GetDeadzoneFraction(), 0.25f, 1e-4f, "deadzone fraction");
	ZENITH_ASSERT_EQ_FLOAT(pxLoadedStick->GetActivationSlop(), 17.0f, 1e-4f, "activation slop");
	ZENITH_ASSERT_EQ_FLOAT(pxLoadedStick->GetKnobColor().z, 0.3f, 1e-4f, "knob colour");
	ZENITH_ASSERT_TRUE(pxLoadedStick->GetActionName() == "VC_Move", "action name");
	ZENITH_ASSERT_EQ_FLOAT(pxLoadedStick->GetPosition().x, 40.0f, 1e-4f, "base-element transform (position)");
	ZENITH_ASSERT_EQ_FLOAT(pxLoadedStick->GetSize().y, 190.0f, 1e-4f, "base-element transform (size)");
	ZENITH_ASSERT_FALSE(pxLoadedStick->IsEngaged(), "a deserialized control is never mid-gesture");

	Zenith_UI::Zenith_UIElement* pxLoadedVBElement = xLoaded.FindElement("SerializedVirtualButton");
	ZENITH_ASSERT_NOT_NULL(pxLoadedVBElement, "the virtual button came back by name");
	ZENITH_ASSERT_EQ(pxLoadedVBElement->GetType(), Zenith_UI::UIElementType::VirtualButton, "...as a VirtualButton");

	Zenith_UI::Zenith_UIVirtualButton* pxLoadedVB =
		static_cast<Zenith_UI::Zenith_UIVirtualButton*>(pxLoadedVBElement);
	ZENITH_ASSERT_EQ_FLOAT(pxLoadedVB->GetHitSlop(), 23.0f, 1e-4f, "hit slop");
	ZENITH_ASSERT_EQ_FLOAT(pxLoadedVB->GetPressedColor().x, 0.9f, 1e-4f, "pressed colour");
	ZENITH_ASSERT_TRUE(pxLoadedVB->GetActionName() == "VC_Fire", "action name");
	ZENITH_ASSERT_EQ_FLOAT(pxLoadedVB->GetSize().x, 88.0f, 1e-4f, "base-element transform (size)");
	ZENITH_ASSERT_FALSE(pxLoadedVB->IsHeld(), "a deserialized control is never mid-gesture");

	Zenith_UI::Zenith_UIElement* pxLoadedPlain = xLoaded.FindElement("SerializedPlainButton");
	ZENITH_ASSERT_NOT_NULL(pxLoadedPlain, "the pre-existing widget came back too");
	ZENITH_ASSERT_EQ(pxLoadedPlain->GetType(), Zenith_UI::UIElementType::Button,
		"...still a Button: appending to UIElementType renumbered nothing");
}
