#pragma once

#include "Core/Zenith_Engine.h"
#include "Input/Zenith_InputActions.h"
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"

// ============================================================================
// RenderTest_Bindings -- RenderTest's ACTION TABLE (input program C2), and the
// ONE place in this game's PRODUCTION code where a raw device code may be
// spelled.
//
// Everything goes through the engine action layer (g_xEngine.Actions()): a
// reader asks "is AIM held", and the binding table below decides whether the
// keyboard, the mouse or a gamepad answered.
//
// TWO PROFILES (B5). A scheme may live in at most ONE registered profile, and
// the FIRST game RegisterProfile call CLEARS the engine's platform default
// wholesale -- so Register() installs both or neither.
//
//   P_DESKTOP { KEYBOARD | MOUSE }   P_GAMEPAD { GAMEPAD }
//
// KEYBOARD and MOUSE share ONE profile because this is a third-person shooter:
// the player's left hand is on WASD and the right on the mouse at the same
// time, and splitting them would make the auto-switch drop movement the instant
// the player aimed. The pad is its own profile for the opposite reason.
//
// P_DESKTOP is registered FIRST because it doubles as the boot default
// (ResolveBootDefaultProfile picks whichever profile owns KEYBOARD on Windows,
// and falls back to the first registered one elsewhere).
//
// Line-by-line the table below is the C2 contract:
//
//   | Action              | Kind   | KEYBOARD      | MOUSE       | GAMEPAD          |
//   | MOVE                | AXIS2D | WASD + arrows | --          | left stick       |
//   | SPRINT              | BUTTON | LShift/RShift | --          | L3               |
//   | JUMP                | BUTTON | Space         | --          | A                |
//   | AIM                 | BUTTON | --            | RMB         | LT (hysteresis)  |
//   | FIRE                | BUTTON | --            | LMB         | RT (hysteresis)  |
//   | INTERACT            | BUTTON | E             | --          | X                |
//   | RELOAD              | BUTTON | R             | --          | LB               |
//   | CYCLE_TENNIS_CAMERA | BUTTON | T             | --          | R3               |
//   | LOOK_DELTA          | AXIS2D | --            | MOUSE_DELTA | --               |
//   | LOOK_RATE           | AXIS2D | --            | --          | right stick      |
//
// ---------------------------------------------------------------------------
// THREE ROWS OF THIS TABLE CARRY A DESIGN DECISION WORTH READING.
//
// ★ JUMP IS ONE ACTION QUERIED TWO WAYS. The retired C++ polled SPACE twice per
// frame -- WasKeyPressedThisFrame for the jump impulse and IsKeyDown for
// jetpack thrust -- and a jetpack-equipped player holding Space gets BOTH on the
// same frame (which is why the ascent cap must sit above the jump velocity; see
// RenderTest_PlayerComponent's JetpackThrustRaisesAndCapsVy unit). Splitting
// that into a JUMP and a THRUST action would have to bind them to the same
// button anyway, and would let a rebind separate two things the design says are
// one. So: one action, WasPressedThisFrame for the pop, IsHeld for the thrust.
//
// ★ AIM AND FIRE REACH THE PAD THROUGH TRIGGERS, NOT BUTTONS. GAMEPAD_AXIS_AS_
// BUTTON turns an analog trigger into a button with press/release HYSTERESIS
// (press at 0.55, release at 0.45). The gap is the point: a trigger resting near
// its threshold would otherwise chatter press/release edges every frame, and
// FIRE is edge-consumed by the graph. The row's own held flag IS the latch, so
// LT and RT cannot fight even though they sit on one device.
//
// ★ LOOK IS TWO ACTIONS BECAUSE A MOUSE AND A STICK ARE DIFFERENT QUANTITIES.
// A mouse reports DISPLACEMENT already integrated over the frame (pixels moved),
// so multiplying it by dt would make the camera's response depend on frame rate
// -- exactly the bug the raw GetMouseDelta path never had. A stick reports a
// deflection, i.e. a RATE, which only becomes an angle after multiplying by dt.
// One AXIS2D action cannot carry both meanings, so LOOK_DELTA is the mouse
// (scales -1/-1, folding the old `yaw -= delta * sens` sign convention into the
// binding) and LOOK_RATE is the pad's right stick (scales -1/-1: stick-right
// turns the camera right, matching mouse-right, and stick-up raises the pitch).
// FollowCamera applies both every frame; a resting stick contributes exactly
// zero (the device layer's radial deadzone), and a still mouse likewise, so
// there is no mode switch and nothing to arbitrate.
//
// The camera is FREE-CURSOR mouse-look and stays that way: MOUSE_DELTA is not
// claim-filtered (only pointer-sourced BUTTON rows are, at 10e), so LOOK_DELTA
// reads exactly the displacement GetMouseDelta reported, cursor visible.
//
// NO TOUCH PROFILE: this game has no on-screen controls, so a tap has nothing to
// publish into. Android would boot into P_DESKTOP and taps would reach the MOUSE
// column through the permanent B3 pointer->mouse projection.
//
// FIRE / INTERACT / RELOAD / CYCLE_TENNIS_CAMERA are consumed by the BOOT-
// AUTHORED RenderTest_PlayerActions.bgraph (OnActionPressed nodes named with the
// strings below), not by C++ -- which is why the action NAMES are contract
// rather than decoration.
//
// Engine-reserved ids 0-15 are the UI nav set; this game's ids start at 16
// (uINPUT_ACTION_FIRST_GAME_ID).
// ============================================================================

namespace RenderTest_Bindings
{
	// ---- Profiles (B5) -----------------------------------------------------
	// Ids are this game's own; 0xF0 / 0xF1 / 0xFF are engine-reserved.
	inline constexpr u_int8 uPROFILE_DESKTOP = 0u;
	inline constexpr u_int8 uPROFILE_GAMEPAD = 1u;

	inline constexpr const char* szPROFILE_DESKTOP_NAME = "P_DESKTOP";
	inline constexpr const char* szPROFILE_GAMEPAD_NAME = "P_GAMEPAD";

	// ---- Actions -----------------------------------------------------------
	// Contiguous from uINPUT_ACTION_FIRST_GAME_ID; RENDERTEST_ACTION_COUNT is
	// the COUNT (10), not the next id. Every id stays below 126 -- the engine's
	// own test actions occupy 126/127 and RegisterAction asserts on a collision.
	enum RENDERTEST_ACTION : Zenith_InputActionID
	{
		RENDERTEST_ACTION_MOVE                = uINPUT_ACTION_FIRST_GAME_ID,        // 16
		RENDERTEST_ACTION_SPRINT              = uINPUT_ACTION_FIRST_GAME_ID + 1u,   // 17
		RENDERTEST_ACTION_JUMP                = uINPUT_ACTION_FIRST_GAME_ID + 2u,   // 18
		RENDERTEST_ACTION_AIM                 = uINPUT_ACTION_FIRST_GAME_ID + 3u,   // 19
		RENDERTEST_ACTION_FIRE                = uINPUT_ACTION_FIRST_GAME_ID + 4u,   // 20
		RENDERTEST_ACTION_INTERACT            = uINPUT_ACTION_FIRST_GAME_ID + 5u,   // 21
		RENDERTEST_ACTION_RELOAD              = uINPUT_ACTION_FIRST_GAME_ID + 6u,   // 22
		RENDERTEST_ACTION_CYCLE_TENNIS_CAMERA = uINPUT_ACTION_FIRST_GAME_ID + 7u,   // 23
		RENDERTEST_ACTION_LOOK_DELTA          = uINPUT_ACTION_FIRST_GAME_ID + 8u,   // 24
		RENDERTEST_ACTION_LOOK_RATE           = uINPUT_ACTION_FIRST_GAME_ID + 9u,   // 25

		RENDERTEST_ACTION_COUNT = 10u
	};

	static_assert(RENDERTEST_ACTION_LOOK_RATE < 126u,
		"Game action ids must stay below the engine's reserved test ids 126/127");

	// Action NAMES. The four graph-consumed ones are the strings the boot-
	// authored PlayerActions graph's action nodes carry, so they are contract,
	// not decoration.
	inline constexpr const char* szACTION_MOVE                = "Move";
	inline constexpr const char* szACTION_SPRINT              = "Sprint";
	inline constexpr const char* szACTION_JUMP                = "Jump";
	inline constexpr const char* szACTION_AIM                 = "Aim";
	inline constexpr const char* szACTION_FIRE                = "Fire";
	inline constexpr const char* szACTION_INTERACT            = "Interact";
	inline constexpr const char* szACTION_RELOAD              = "Reload";
	inline constexpr const char* szACTION_CYCLE_TENNIS_CAMERA = "CycleTennisCamera";
	inline constexpr const char* szACTION_LOOK_DELTA          = "LookDelta";
	inline constexpr const char* szACTION_LOOK_RATE           = "LookRate";

	// ---- Gamepad trigger hysteresis ----------------------------------------
	// Shared by AIM (LT) and FIRE (RT) so the two halves of one design decision
	// cannot drift apart. Press at/above, release at/below.
	inline constexpr float fTRIGGER_PRESS_AT   = 0.55f;
	inline constexpr float fTRIGGER_RELEASE_AT = 0.45f;

	// ---- Registration ------------------------------------------------------
	// Takes the layer BY REFERENCE so a test can install this exact table into a
	// LOCAL Zenith_InputActions and never touch the engine's (B13). Called once
	// per process from Project_RegisterGameComponents.
	inline void Register(Zenith_InputActions& xActions)
	{
		// Profiles FIRST: the first call clears the engine defaults, and doing
		// that after the actions were bound would leave a window in which a
		// registered row resolved against a profile set this game does not own.
		xActions.RegisterProfile(uPROFILE_DESKTOP, szPROFILE_DESKTOP_NAME,
			uINPUT_SCHEME_MASK_KEYBOARD | uINPUT_SCHEME_MASK_MOUSE);
		xActions.RegisterProfile(uPROFILE_GAMEPAD, szPROFILE_GAMEPAD_NAME,
			uINPUT_SCHEME_MASK_GAMEPAD);

		// MOVE -- WASD and the arrows on ONE row so they can never disagree,
		// plus the pad's left stick. The stick's y scale is -1 because GLFW
		// reports a forward push as -1 and the engine convention is +y FORWARD.
		xActions.RegisterAction(RENDERTEST_ACTION_MOVE, szACTION_MOVE, INPUT_ACTION_AXIS2D);
		xActions.RegisterBinding(RENDERTEST_ACTION_MOVE,
			Zenith_InputBinding::KeyAxis2D(ZENITH_KEY_W, ZENITH_KEY_S, ZENITH_KEY_A, ZENITH_KEY_D)
				.WithKey(Zenith_InputBinding::uSET_FORWARD, ZENITH_KEY_UP)
				.WithKey(Zenith_InputBinding::uSET_BACK,    ZENITH_KEY_DOWN)
				.WithKey(Zenith_InputBinding::uSET_LEFT,    ZENITH_KEY_LEFT)
				.WithKey(Zenith_InputBinding::uSET_RIGHT,   ZENITH_KEY_RIGHT));
		xActions.RegisterBinding(RENDERTEST_ACTION_MOVE,
			Zenith_InputBinding::GamepadStick(ZENITH_GAMEPAD_AXIS_LEFT_X, 1.0f, -1.0f));

		// SPRINT -- either shift on one row (a KeySet is an OR over its keys),
		// or clicking the left stick, which is where a pad player's thumb
		// already is while pushing it forward.
		xActions.RegisterAction(RENDERTEST_ACTION_SPRINT, szACTION_SPRINT, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(RENDERTEST_ACTION_SPRINT,
			Zenith_InputBinding::KeySet(ZENITH_KEY_LEFT_SHIFT, ZENITH_KEY_RIGHT_SHIFT));
		xActions.RegisterBinding(RENDERTEST_ACTION_SPRINT,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_LEFT_THUMB));

		// JUMP -- see the header note: ONE action, both a press and a hold
		// query. The jetpack reads IsHeld on the same id the jump pop edges on.
		xActions.RegisterAction(RENDERTEST_ACTION_JUMP, szACTION_JUMP, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(RENDERTEST_ACTION_JUMP,
			Zenith_InputBinding::KeySet(ZENITH_KEY_SPACE));
		xActions.RegisterBinding(RENDERTEST_ACTION_JUMP,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_A));

		// AIM -- RMB, claim-filtered at 10e so a click a UI widget took never
		// also aims; on a pad, the left trigger past the hysteresis band.
		xActions.RegisterAction(RENDERTEST_ACTION_AIM, szACTION_AIM, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(RENDERTEST_ACTION_AIM,
			Zenith_InputBinding::MouseButton(ZENITH_MOUSE_BUTTON_RIGHT));
		xActions.RegisterBinding(RENDERTEST_ACTION_AIM,
			Zenith_InputBinding::GamepadAxisAsButton(ZENITH_GAMEPAD_AXIS_LEFT_TRIGGER,
				fTRIGGER_PRESS_AT, fTRIGGER_RELEASE_AT));

		// FIRE -- graph-consumed (OnActionPressed). LMB, or the right trigger.
		xActions.RegisterAction(RENDERTEST_ACTION_FIRE, szACTION_FIRE, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(RENDERTEST_ACTION_FIRE,
			Zenith_InputBinding::MouseButton(ZENITH_MOUSE_BUTTON_LEFT));
		xActions.RegisterBinding(RENDERTEST_ACTION_FIRE,
			Zenith_InputBinding::GamepadAxisAsButton(ZENITH_GAMEPAD_AXIS_RIGHT_TRIGGER,
				fTRIGGER_PRESS_AT, fTRIGGER_RELEASE_AT));

		// The three remaining graph-consumed presses.
		xActions.RegisterAction(RENDERTEST_ACTION_INTERACT, szACTION_INTERACT, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(RENDERTEST_ACTION_INTERACT,
			Zenith_InputBinding::KeySet(ZENITH_KEY_E));
		xActions.RegisterBinding(RENDERTEST_ACTION_INTERACT,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_X));

		xActions.RegisterAction(RENDERTEST_ACTION_RELOAD, szACTION_RELOAD, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(RENDERTEST_ACTION_RELOAD,
			Zenith_InputBinding::KeySet(ZENITH_KEY_R));
		xActions.RegisterBinding(RENDERTEST_ACTION_RELOAD,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_LEFT_BUMPER));

		xActions.RegisterAction(RENDERTEST_ACTION_CYCLE_TENNIS_CAMERA,
			szACTION_CYCLE_TENNIS_CAMERA, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(RENDERTEST_ACTION_CYCLE_TENNIS_CAMERA,
			Zenith_InputBinding::KeySet(ZENITH_KEY_T));
		xActions.RegisterBinding(RENDERTEST_ACTION_CYCLE_TENNIS_CAMERA,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_RIGHT_THUMB));

		// LOOK_DELTA -- mouse DISPLACEMENT in raw surface pixels. The -1 scales
		// carry the retired `yaw -= delta * sensitivity` sign convention, so the
		// consumer adds rather than subtracts. Sensitivity stays game-side: it
		// is a tuning value, not a device fact.
		xActions.RegisterAction(RENDERTEST_ACTION_LOOK_DELTA, szACTION_LOOK_DELTA, INPUT_ACTION_AXIS2D);
		xActions.RegisterBinding(RENDERTEST_ACTION_LOOK_DELTA,
			Zenith_InputBinding::MouseDelta(-1.0f, -1.0f));

		// LOOK_RATE -- the pad's right stick as a RATE the consumer multiplies
		// by dt. Independent per-axis inversion: x is -1 so stick-right turns
		// the camera right (matching mouse-right), y is -1 because GLFW reports
		// stick-up as -1 and pushing up must raise the pitch.
		xActions.RegisterAction(RENDERTEST_ACTION_LOOK_RATE, szACTION_LOOK_RATE, INPUT_ACTION_AXIS2D);
		xActions.RegisterBinding(RENDERTEST_ACTION_LOOK_RATE,
			Zenith_InputBinding::GamepadStick(ZENITH_GAMEPAD_AXIS_RIGHT_X, -1.0f, -1.0f));
	}

	// ---- Readers (NON-CONSUMING reads of CLOSED state) ---------------------
	//
	// ★ THESE ARE FRAME-CONTRACT READS, NOT POLLS. The action layer closes at
	// step 10e and game logic runs at step 11, so a consumer inside an ECS
	// OnUpdate / OnLateUpdate sees this frame's edges. Code that runs OUTSIDE
	// the main loop must drive the contract itself (the unit fixture in
	// RenderTest_PlayerComponent.Tests.inl does exactly that) -- these would
	// otherwise answer with whatever the last real frame left behind.

	// +y is FORWARD, x is RIGHT, diagonals UNNORMALISED, opposite keys cancel.
	inline Zenith_Maths::Vector2 ReadMove()
	{
		Zenith_Maths::Vector2 xMove(0.0f);
		g_xEngine.Actions().GetAxis2D(RENDERTEST_ACTION_MOVE, xMove);
		return xMove;
	}

	// Mouse displacement this frame, already sign-corrected by the binding.
	inline Zenith_Maths::Vector2 ReadLookDelta()
	{
		Zenith_Maths::Vector2 xLook(0.0f);
		g_xEngine.Actions().GetAxis2D(RENDERTEST_ACTION_LOOK_DELTA, xLook);
		return xLook;
	}

	// Pad right-stick deflection: a RATE, so the caller multiplies by dt.
	inline Zenith_Maths::Vector2 ReadLookRate()
	{
		Zenith_Maths::Vector2 xLook(0.0f);
		g_xEngine.Actions().GetAxis2D(RENDERTEST_ACTION_LOOK_RATE, xLook);
		return xLook;
	}

	inline bool IsSprintHeld()   { return g_xEngine.Actions().IsHeld(RENDERTEST_ACTION_SPRINT); }
	inline bool WasJumpPressed() { return g_xEngine.Actions().WasPressedThisFrame(RENDERTEST_ACTION_JUMP); }
	inline bool IsJumpHeld()     { return g_xEngine.Actions().IsHeld(RENDERTEST_ACTION_JUMP); }
	inline bool IsAimHeld()      { return g_xEngine.Actions().IsHeld(RENDERTEST_ACTION_AIM); }
}
