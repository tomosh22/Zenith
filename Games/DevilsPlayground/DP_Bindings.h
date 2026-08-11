#pragma once

#include "Core/Zenith_Engine.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputActions.h"
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"

// ============================================================================
// DP_Bindings -- DevilsPlayground's ACTION TABLE (input program C2), and the
// ONE place in this game's PRODUCTION code where a raw device code may be
// spelled. It replaces the retired DP_Input naming layer wholesale: that header
// named the verbs but still polled Zenith_Input for a hard-wired key, so no
// pad, no rebind and no second device could ever answer.
//
// Everything goes through the engine action layer (g_xEngine.Actions()): a
// reader asks "was INTERACT pressed", and the binding table below decides
// whether the keyboard, the mouse or a gamepad answered. The one exception is
// CURSOR POSITION, which is not an action at all -- see the last section.
//
// TWO PROFILES (B5). A scheme may live in at most ONE registered profile, and
// the FIRST game RegisterProfile call CLEARS the engine's platform default
// wholesale -- so Register() installs both or neither.
//
//   P_DESKTOP { KEYBOARD | MOUSE }   P_GAMEPAD { GAMEPAD }
//
// KEYBOARD and MOUSE share ONE profile because this game is played with both
// hands at once: the left drives the possessed villager on WASD while the right
// clicks the next body to jump into. Splitting them would make the auto-switch
// drop movement the instant the player clicked. The pad is its own profile for
// the opposite reason -- a pad player's hands are nowhere near the keyboard.
//
// NO TOUCH PROFILE. There are no on-screen controls in this game, so a tap has
// nothing to publish into. Android would boot into P_DESKTOP
// (ResolveBootDefaultProfile finds no TOUCH profile and falls back to the FIRST
// registered one -- which is why P_DESKTOP is registered first) and taps would
// reach the MOUSE column through the permanent B3 pointer->mouse projection.
//
// Line-by-line the table below is the C2 contract:
//
//   | Action        | Kind   | KEYBOARD        | MOUSE | GAMEPAD          |
//   | MOVE          | AXIS2D | WASD + arrows   | --    | left stick       |
//   | SPRINT        | BUTTON | Shift (either)  | --    | L3               |
//   | WALK_QUIET    | BUTTON | Ctrl (either)   | --    | LB               |
//   | INTERACT      | BUTTON | F               | --    | X                |
//   | DROP          | BUTTON | G               | --    | Y                |
//   | ABILITY       | BUTTON | Space           | --    | RB               |
//   | CAMERA_ROTATE | AXIS1D | Q (-) / E (+)   | --    | right-stick X    |
//   | CAMERA_TOGGLE | BUTTON | C               | --    | R3               |
//   | HELP          | BUTTON | H               | --    | Back             |
//   | ESCAPE        | BUTTON | Escape          | --    | Start            |
//   | RESTART       | BUTTON | R               | --    | Y  *             |
//   | QUIT_TO_MENU  | BUTTON | Q  *            | --    | X  *             |
//   | POSSESS       | BUTTON | --              | LMB   | -- (narrowed)    |
//   | ZOOM_DELTA    | AXIS1D | --              | wheel | --               |
//   | ZOOM_RATE     | AXIS1D | --              | --    | RT - LT          |
//
// ---------------------------------------------------------------------------
// FOUR ROWS OF THIS TABLE CARRY A DESIGN DECISION WORTH READING.
//
// * PAD Y AND X ARE EACH BOUND TWICE -- DROP/RESTART and INTERACT/QUIT_TO_MENU
// -- AND THAT IS THE TABLE AS SPECIFIED, NOT AN OVERSIGHT HERE. The same is
// true of KEYBOARD Q, which is both CAMERA_ROTATE's negative half and
// QUIT_TO_MENU (exactly as the retired raw polls had it). Nothing here
// separates them, and nothing here needs to: RESTART and QUIT_TO_MENU are
// consumed ONLY by DP_PauseMenu.bgraph, whose first gate is
// (shown || runOver) -- the pause overlay must already be up, or the run
// already over. While that gate is closed the two pad presses are inert and
// only DROP / INTERACT act; while it is open the gameplay scene is paused, so
// the villager shim and the interactables are not ticking to see them. That
// STATE GATE IS THE GAME'S OWN and stays where it is: this migration does not
// invent per-context action masks, and adding them would be a gameplay change.
//
// * POSSESS HAS NO PAD ROW, AND THAT IS THE SPECIFIED NARROWING. Possession is
// the press half of a POINTER gesture -- DPNode_PickVillagerUnderCursor scores
// every villager by screen distance to the CURSOR and takes the closest within
// 120 px. A face button would produce a press that lands wherever the mouse was
// last left. The pad's scope is movement, the verbs and the camera; click-to-
// possess is deliberately not pad-operable until this game grows a pad cursor.
//
// * POSSESS IS CLAIM-AWARE, and that is engine behaviour rather than anything
// spelled here: at frame-contract step 10e the action layer SUPPRESSES a
// pointer-sourced transition whose pointer a UI widget claimed. So a click that
// a HUD button consumed can never also possess the villager behind it.
//
// * ZOOM IS TWO ACTIONS BECAUSE A WHEEL AND A TRIGGER ARE DIFFERENT QUANTITIES.
// A wheel reports DISPLACEMENT already integrated over the frame (notches
// turned), so multiplying it by dt would make the zoom depend on frame rate --
// exactly the bug the raw GetMouseWheelDelta path never had. A trigger reports
// a deflection, i.e. a RATE, which only becomes a distance after multiplying by
// dt. One AXIS1D action cannot carry both meanings, so ZOOM_DELTA is the wheel
// and ZOOM_RATE is the pad's RT-LT pair. DPOrbitCamera applies both every
// frame; a resting trigger contributes exactly zero and a still wheel likewise,
// so there is no mode switch and nothing to arbitrate.
//
// ---------------------------------------------------------------------------
// WHO CONSUMES WHAT. DP's graphs do NOT read input through OnActionPressed
// nodes: every one of them is driven by a custom event its C++ shim fires at
// the exact frame position the retired inline call sat, with this frame's
// input STAGED onto the blackboard first (DP_PlayerControl's clickPressed /
// dropPressed, DP_PauseMenu's escPressed / rPressed / qPressed). The readers
// below are what those shims stage from, so the action NAMES here are not
// graph contract -- the BLACKBOARD KEY names are, and they are unchanged.
//
// Engine-reserved ids 0-15 are the UI nav set; this game's ids start at 16
// (uINPUT_ACTION_FIRST_GAME_ID) and stay below 126 -- the engine's own test
// actions occupy 126/127 and RegisterAction asserts on a collision.
// ============================================================================

namespace DP_Bindings
{
	// ---- Profiles (B5) -----------------------------------------------------
	// Ids are this game's own; 0xF0 / 0xF1 / 0xFF are engine-reserved.
	// P_DESKTOP is registered FIRST because it doubles as the Android fallback.
	inline constexpr u_int8 uPROFILE_DESKTOP = 0u;
	inline constexpr u_int8 uPROFILE_GAMEPAD = 1u;

	inline constexpr const char* szPROFILE_DESKTOP_NAME = "P_DESKTOP";
	inline constexpr const char* szPROFILE_GAMEPAD_NAME = "P_GAMEPAD";

	// ---- Actions -----------------------------------------------------------
	// Contiguous from uINPUT_ACTION_FIRST_GAME_ID; DP_ACTION_COUNT is the
	// COUNT (15), not the next id.
	enum DP_ACTION : Zenith_InputActionID
	{
		DP_ACTION_MOVE          = uINPUT_ACTION_FIRST_GAME_ID,         // 16
		DP_ACTION_SPRINT        = uINPUT_ACTION_FIRST_GAME_ID + 1u,    // 17
		DP_ACTION_WALK_QUIET    = uINPUT_ACTION_FIRST_GAME_ID + 2u,    // 18
		DP_ACTION_INTERACT      = uINPUT_ACTION_FIRST_GAME_ID + 3u,    // 19
		DP_ACTION_DROP          = uINPUT_ACTION_FIRST_GAME_ID + 4u,    // 20
		DP_ACTION_ABILITY       = uINPUT_ACTION_FIRST_GAME_ID + 5u,    // 21
		DP_ACTION_CAMERA_ROTATE = uINPUT_ACTION_FIRST_GAME_ID + 6u,    // 22
		DP_ACTION_CAMERA_TOGGLE = uINPUT_ACTION_FIRST_GAME_ID + 7u,    // 23
		DP_ACTION_HELP          = uINPUT_ACTION_FIRST_GAME_ID + 8u,    // 24
		DP_ACTION_ESCAPE        = uINPUT_ACTION_FIRST_GAME_ID + 9u,    // 25
		DP_ACTION_RESTART       = uINPUT_ACTION_FIRST_GAME_ID + 10u,   // 26
		DP_ACTION_QUIT_TO_MENU  = uINPUT_ACTION_FIRST_GAME_ID + 11u,   // 27
		DP_ACTION_POSSESS       = uINPUT_ACTION_FIRST_GAME_ID + 12u,   // 28
		DP_ACTION_ZOOM_DELTA    = uINPUT_ACTION_FIRST_GAME_ID + 13u,   // 29
		DP_ACTION_ZOOM_RATE     = uINPUT_ACTION_FIRST_GAME_ID + 14u,   // 30

		DP_ACTION_COUNT = 15u
	};

	static_assert(DP_ACTION_ZOOM_RATE < 126u,
		"Game action ids must stay below the engine's reserved test ids 126/127");

	// Action NAMES. Nothing in this game resolves an action by name today (the
	// graphs read staged blackboard facts, not OnActionPressed nodes), but the
	// names are what an editor rebind screen or a future action node would use,
	// so they are spelled once here rather than invented at each call site.
	inline constexpr const char* szACTION_MOVE          = "Move";
	inline constexpr const char* szACTION_SPRINT        = "Sprint";
	inline constexpr const char* szACTION_WALK_QUIET    = "WalkQuiet";
	inline constexpr const char* szACTION_INTERACT      = "Interact";
	inline constexpr const char* szACTION_DROP          = "Drop";
	inline constexpr const char* szACTION_ABILITY       = "Ability";
	inline constexpr const char* szACTION_CAMERA_ROTATE = "CameraRotate";
	inline constexpr const char* szACTION_CAMERA_TOGGLE = "CameraToggle";
	inline constexpr const char* szACTION_HELP          = "Help";
	inline constexpr const char* szACTION_ESCAPE        = "Escape";
	inline constexpr const char* szACTION_RESTART       = "Restart";
	inline constexpr const char* szACTION_QUIT_TO_MENU  = "QuitToMenu";
	inline constexpr const char* szACTION_POSSESS       = "Possess";
	inline constexpr const char* szACTION_ZOOM_DELTA    = "ZoomDelta";
	inline constexpr const char* szACTION_ZOOM_RATE     = "ZoomRate";

	// ---- Registration ------------------------------------------------------
	// Takes the layer BY REFERENCE so a test can install this exact table into
	// a LOCAL Zenith_InputActions and never touch the engine's (B13). Called
	// once per process from Project_RegisterGameComponents.
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
		// reports a forward push as -1 and the engine convention is +y FORWARD,
		// which is exactly what the retired ReadMoveVillager produced (W/Up ->
		// +y). Diagonals stay UNNORMALISED and opposite keys CANCEL, again
		// matching the retired hand-rolled composite.
		xActions.RegisterAction(DP_ACTION_MOVE, szACTION_MOVE, INPUT_ACTION_AXIS2D);
		xActions.RegisterBinding(DP_ACTION_MOVE,
			Zenith_InputBinding::KeyAxis2D(ZENITH_KEY_W, ZENITH_KEY_S, ZENITH_KEY_A, ZENITH_KEY_D)
				.WithKey(Zenith_InputBinding::uSET_FORWARD, ZENITH_KEY_UP)
				.WithKey(Zenith_InputBinding::uSET_BACK,    ZENITH_KEY_DOWN)
				.WithKey(Zenith_InputBinding::uSET_LEFT,    ZENITH_KEY_LEFT)
				.WithKey(Zenith_InputBinding::uSET_RIGHT,   ZENITH_KEY_RIGHT));
		xActions.RegisterBinding(DP_ACTION_MOVE,
			Zenith_InputBinding::GamepadStick(ZENITH_GAMEPAD_AXIS_LEFT_X, 1.0f, -1.0f));

		// SPRINT -- either shift on one row (a KeySet is an OR over its keys,
		// which is what the retired reader spelled by hand), or clicking the
		// left stick, which is where a pad player's thumb already is while
		// pushing it forward.
		xActions.RegisterAction(DP_ACTION_SPRINT, szACTION_SPRINT, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(DP_ACTION_SPRINT,
			Zenith_InputBinding::KeySet(ZENITH_KEY_LEFT_SHIFT, ZENITH_KEY_RIGHT_SHIFT));
		xActions.RegisterBinding(DP_ACTION_SPRINT,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_LEFT_THUMB));

		// WALK_QUIET -- either ctrl, or the left bumper. Sprint wins ties, and
		// that arbitration lives in DP_Villager.bgraph (chain T2), NOT here:
		// both actions are simply held, and the graph decides.
		xActions.RegisterAction(DP_ACTION_WALK_QUIET, szACTION_WALK_QUIET, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(DP_ACTION_WALK_QUIET,
			Zenith_InputBinding::KeySet(ZENITH_KEY_LEFT_CONTROL, ZENITH_KEY_RIGHT_CONTROL));
		xActions.RegisterBinding(DP_ACTION_WALK_QUIET,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_LEFT_BUMPER));

		// INTERACT -- the F-press every DPInteractable_Base leaf polls while a
		// possessed villager is inside its radius.
		xActions.RegisterAction(DP_ACTION_INTERACT, szACTION_INTERACT, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(DP_ACTION_INTERACT,
			Zenith_InputBinding::KeySet(ZENITH_KEY_F));
		xActions.RegisterBinding(DP_ACTION_INTERACT,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_X));

		// DROP -- staged into DP_PlayerControl.bgraph as "dropPressed". Edge,
		// not level, so holding it does not keep dropping.
		xActions.RegisterAction(DP_ACTION_DROP, szACTION_DROP, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(DP_ACTION_DROP,
			Zenith_InputBinding::KeySet(ZENITH_KEY_G));
		xActions.RegisterBinding(DP_ACTION_DROP,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_Y));

		// ABILITY -- the archetype-specific verb the HUD cheat sheet advertises.
		// Registered per the C2 contract; it has NO production consumer yet (the
		// retired DP_Input reader had none either), so wiring one is
		// a gameplay change rather than part of this migration.
		xActions.RegisterAction(DP_ACTION_ABILITY, szACTION_ABILITY, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(DP_ACTION_ABILITY,
			Zenith_InputBinding::KeySet(ZENITH_KEY_SPACE));
		xActions.RegisterBinding(DP_ACTION_ABILITY,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_RIGHT_BUMPER));

		// CAMERA_ROTATE -- an AXIS1D, because the orbit yaw integrates it every
		// frame (yaw += value * speed * dt). Q is the negative half and E the
		// positive one, exactly as the retired reader's two IsKeyDown polls
		// summed to. The pad answers with right-stick X, which is a deflection
		// and therefore already the same RATE quantity the keys produce.
		xActions.RegisterAction(DP_ACTION_CAMERA_ROTATE, szACTION_CAMERA_ROTATE, INPUT_ACTION_AXIS1D);
		xActions.RegisterBinding(DP_ACTION_CAMERA_ROTATE,
			Zenith_InputBinding::KeyAxis1D(ZENITH_KEY_Q, ZENITH_KEY_E));
		xActions.RegisterBinding(DP_ACTION_CAMERA_ROTATE,
			Zenith_InputBinding::GamepadAxis(ZENITH_GAMEPAD_AXIS_RIGHT_X, 1.0f));

		// CAMERA_TOGGLE -- flips birds-eye / third-person, overriding the
		// possession-driven auto switch until the next possession change.
		xActions.RegisterAction(DP_ACTION_CAMERA_TOGGLE, szACTION_CAMERA_TOGGLE, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(DP_ACTION_CAMERA_TOGGLE,
			Zenith_InputBinding::KeySet(ZENITH_KEY_C));
		xActions.RegisterBinding(DP_ACTION_CAMERA_TOGGLE,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_RIGHT_THUMB));

		// HELP -- the full-screen instructional overlay.
		xActions.RegisterAction(DP_ACTION_HELP, szACTION_HELP, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(DP_ACTION_HELP,
			Zenith_InputBinding::KeySet(ZENITH_KEY_H));
		xActions.RegisterBinding(DP_ACTION_HELP,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_BACK));

		// ESCAPE / RESTART / QUIT_TO_MENU -- the three rows DPPauseMenuController
		// stages onto DP_PauseMenu.bgraph. See the header note about the pad Y/X
		// and keyboard Q collisions: the graph's (shown || runOver) gate is what
		// separates them, and it is the GAME's gate, not a binding-table one.
		xActions.RegisterAction(DP_ACTION_ESCAPE, szACTION_ESCAPE, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(DP_ACTION_ESCAPE,
			Zenith_InputBinding::KeySet(ZENITH_KEY_ESCAPE));
		xActions.RegisterBinding(DP_ACTION_ESCAPE,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_START));

		xActions.RegisterAction(DP_ACTION_RESTART, szACTION_RESTART, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(DP_ACTION_RESTART,
			Zenith_InputBinding::KeySet(ZENITH_KEY_R));
		xActions.RegisterBinding(DP_ACTION_RESTART,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_Y));

		xActions.RegisterAction(DP_ACTION_QUIT_TO_MENU, szACTION_QUIT_TO_MENU, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(DP_ACTION_QUIT_TO_MENU,
			Zenith_InputBinding::KeySet(ZENITH_KEY_Q));
		xActions.RegisterBinding(DP_ACTION_QUIT_TO_MENU,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_X));

		// POSSESS -- LMB only, claim-filtered at 10e. No pad row by design (see
		// the header note): every consumer of it also needs a cursor POSITION.
		xActions.RegisterAction(DP_ACTION_POSSESS, szACTION_POSSESS, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(DP_ACTION_POSSESS,
			Zenith_InputBinding::MouseButton(ZENITH_MOUSE_BUTTON_LEFT));

		// ZOOM_DELTA -- the wheel's DISPLACEMENT this frame, scale 1 so the
		// value the camera reads is bit-for-bit the one GetMouseWheelDelta
		// reported before. Zoom SPEED stays game-side: it is a tuning value,
		// not a device fact.
		xActions.RegisterAction(DP_ACTION_ZOOM_DELTA, szACTION_ZOOM_DELTA, INPUT_ACTION_AXIS1D);
		xActions.RegisterBinding(DP_ACTION_ZOOM_DELTA,
			Zenith_InputBinding::MouseWheel(1.0f));

		// ZOOM_RATE -- RT minus LT as a RATE the consumer multiplies by dt.
		// Positive (RT) zooms IN, matching a wheel pushed forward.
		xActions.RegisterAction(DP_ACTION_ZOOM_RATE, szACTION_ZOOM_RATE, INPUT_ACTION_AXIS1D);
		xActions.RegisterBinding(DP_ACTION_ZOOM_RATE,
			Zenith_InputBinding::GamepadTriggerPair(1.0f));
	}

	// ---- Readers (NON-CONSUMING reads of CLOSED state) ---------------------
	//
	// * THESE ARE FRAME-CONTRACT READS, NOT POLLS. The action layer closes at
	// step 10e and game logic runs at step 11, so a consumer inside an ECS
	// OnUpdate (or a graph node the component drove) sees this frame's edges.
	// Code that runs OUTSIDE the main loop must drive a LOCAL
	// Zenith_InputActions instead -- these would answer with whatever the last
	// real frame left behind.
	//
	// They are also NON-CONSUMING: two readers in the same frame both see an
	// edge. That is unchanged from the retired DP_Input readers, and the
	// mutual exclusion the game needs is still written out explicitly (the
	// pause graph's shown/runOver gate, the villager graph's sprint-wins-ties).

	// +y is FORWARD, x is RIGHT, diagonals UNNORMALISED, opposite keys cancel.
	inline Zenith_Maths::Vector2 ReadMove()
	{
		Zenith_Maths::Vector2 xMove(0.0f);
		g_xEngine.Actions().GetAxis2D(DP_ACTION_MOVE, xMove);
		return xMove;
	}

	// -1 rotates the orbit yaw one way, +1 the other; the caller scales by its
	// own rotate speed and by dt.
	inline float ReadCameraRotate() { return g_xEngine.Actions().GetAxis1D(DP_ACTION_CAMERA_ROTATE); }

	// Wheel notches this frame -- a DISPLACEMENT, so the caller must NOT
	// multiply it by dt.
	inline float ReadZoomDelta() { return g_xEngine.Actions().GetAxis1D(DP_ACTION_ZOOM_DELTA); }

	// Trigger deflection -- a RATE, so the caller multiplies by dt.
	inline float ReadZoomRate() { return g_xEngine.Actions().GetAxis1D(DP_ACTION_ZOOM_RATE); }

	inline bool IsSprintHeld()    { return g_xEngine.Actions().IsHeld(DP_ACTION_SPRINT); }
	inline bool IsWalkQuietHeld() { return g_xEngine.Actions().IsHeld(DP_ACTION_WALK_QUIET); }

	inline bool WasInteractPressed()     { return g_xEngine.Actions().WasPressedThisFrame(DP_ACTION_INTERACT); }
	inline bool WasDropPressed()         { return g_xEngine.Actions().WasPressedThisFrame(DP_ACTION_DROP); }
	inline bool WasAbilityPressed()      { return g_xEngine.Actions().WasPressedThisFrame(DP_ACTION_ABILITY); }
	inline bool WasCameraTogglePressed() { return g_xEngine.Actions().WasPressedThisFrame(DP_ACTION_CAMERA_TOGGLE); }
	inline bool WasHelpTogglePressed()   { return g_xEngine.Actions().WasPressedThisFrame(DP_ACTION_HELP); }
	inline bool WasEscapePressed()       { return g_xEngine.Actions().WasPressedThisFrame(DP_ACTION_ESCAPE); }
	inline bool WasRestartPressed()      { return g_xEngine.Actions().WasPressedThisFrame(DP_ACTION_RESTART); }
	inline bool WasQuitToMenuPressed()   { return g_xEngine.Actions().WasPressedThisFrame(DP_ACTION_QUIT_TO_MENU); }
	inline bool WasPossessPressed()      { return g_xEngine.Actions().WasPressedThisFrame(DP_ACTION_POSSESS); }

	// ---- The cursor (POSITION, which no action can carry) ------------------
	//
	// DPNode_PickVillagerUnderCursor needs to know WHERE the cursor is, which no
	// binding row carries -- POSSESS says a click happened, not where. So the
	// pick reads the position here, and this is the ONLY reason this header
	// still names Zenith_Input at all.
	//
	// * IT IS THE DEVICE POSITION, NOT THE POINTER TABLE, AND THAT IS
	// DELIBERATE. TilePuzzle reads the pointer table because it drags a shape
	// for the whole duration of a gesture, on Windows and on Android, and needs
	// the claim rule applied by hand. DP does neither: the position is read on
	// exactly one frame -- the one POSSESS edged on -- and the action layer has
	// ALREADY suppressed that edge if a widget claimed the pointer, so the claim
	// rule is applied upstream and cannot be bypassed here. The pointer table
	// would additionally have no record at all on a frame with no button down,
	// which would turn any future hover-time consumer into a silent no-pick.
	//
	// Positions are RAW SURFACE PIXELS, the same space the retired
	// g_xEngine.Input().GetMousePosition call reported, so the screen-space pick
	// maths needs no remapping.
	inline void ReadCursorPosition(Zenith_Maths::Vector2_64& xOutPosition)
	{
		g_xEngine.Input().GetMousePosition(xOutPosition);
	}
}
