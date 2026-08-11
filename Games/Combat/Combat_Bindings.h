#pragma once

#include "Core/Zenith_Engine.h"
#include "Input/Zenith_InputActions.h"
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"

// ============================================================================
// Combat_Bindings -- Combat's ACTION TABLE (input program C2), and the ONE
// place in this game's PRODUCTION code where a raw device code may be spelled.
//
// Everything goes through the engine action layer (g_xEngine.Actions()): a
// reader asks "was LIGHT_ATTACK pressed", and the binding table below decides
// whether the mouse, a gamepad or a thumb on glass answered.
//
// TWO PROFILES (B5). A scheme may live in at most ONE registered profile, and
// the FIRST game RegisterProfile call CLEARS the engine's platform default
// wholesale -- so Register() installs both or neither.
//
//   P_DESKTOP { KEYBOARD | MOUSE }   P_GAMEPAD { GAMEPAD }
//
// KEYBOARD and MOUSE share ONE profile on purpose: this game's hands are on
// WASD and the mouse at the same time, and splitting them would make the
// auto-switch drop movement the instant the player clicked. The pad is its own
// profile precisely because it is the opposite -- a pad player's hands are
// nowhere near the keyboard.
//
// NO TOUCH PROFILE, AND THAT IS THE ANDROID STORY TOO. There are no on-screen
// controls in this game, so a tap has nothing to publish into. Android instead
// boots into P_DESKTOP (ResolveBootDefaultProfile finds no TOUCH profile and
// falls back to the FIRST registered one -- which is why P_DESKTOP is
// registered first) and taps reach the MOUSE column through the permanent B3
// pointer->mouse projection, claim-filtered at 10e so a tap that a UI widget
// took can never also swing the sword.
//
// Line-by-line the table below is the C2 contract:
//
//   | Action         | Kind   | KEYBOARD      | MOUSE | GAMEPAD             |
//   | MOVE           | AXIS2D | WASD + arrows | --    | left stick + dpad   |
//   | LIGHT_ATTACK   | BUTTON | --            | LMB   | X                   |
//   | HEAVY_ATTACK   | BUTTON | --            | RMB   | Y                   |
//   | DODGE          | BUTTON | Space         | --    | B                   |
//   | PAUSE          | BUTTON | P             | --    | Start               |
//   | RESTART        | BUTTON | R             | --    | Back                |
//   | RETURN_TO_MENU | BUTTON | Escape        | --    | B                   |
//
// ★ PAD B IS BOUND TWICE -- DODGE AND RETURN_TO_MENU -- AND THAT IS THE TABLE
// AS SPECIFIED, NOT AN OVERSIGHT HERE. The consequence is real and observable:
// RETURN_TO_MENU is consumed by the GameFlow graph at component order 60,
// which runs BEFORE the player component (order 101) fires "PlayerTick", so on
// a pad the arena unloads on the same frame the dodge would have started and
// the dodge is never reached. Combat_SimPad_Test pins BOTH consequences of the
// one button so the collision is visible in CI rather than folklore. The fix
// (when the table is next revised) is a distinct pad button for one of them --
// this game has no per-context action maps, and the graph's state gate cannot
// separate two actions that are live in the SAME state.
//
// PAUSE / RESTART / RETURN_TO_MENU are consumed by BOOT-AUTHORED GRAPHS
// (OnActionPressed nodes named with the strings below), not by C++ -- which is
// why the action NAMES are contract rather than decoration.
//
// Engine-reserved ids 0-15 are the UI nav set; this game's ids start at 16
// (uINPUT_ACTION_FIRST_GAME_ID).
// ============================================================================

namespace Combat_Bindings
{
	// ---- Profiles (B5) -----------------------------------------------------
	// Ids are this game's own; 0xF0 / 0xF1 / 0xFF are engine-reserved.
	// P_DESKTOP is registered FIRST because it doubles as the Android fallback.
	inline constexpr u_int8 uPROFILE_DESKTOP = 0u;
	inline constexpr u_int8 uPROFILE_GAMEPAD = 1u;

	inline constexpr const char* szPROFILE_DESKTOP_NAME = "P_DESKTOP";
	inline constexpr const char* szPROFILE_GAMEPAD_NAME = "P_GAMEPAD";

	// ---- Actions -----------------------------------------------------------
	// Contiguous from uINPUT_ACTION_FIRST_GAME_ID; COMBAT_ACTION_COUNT is the
	// COUNT (7), not the next id.
	enum COMBAT_ACTION : Zenith_InputActionID
	{
		COMBAT_ACTION_MOVE           = uINPUT_ACTION_FIRST_GAME_ID,        // 16
		COMBAT_ACTION_LIGHT_ATTACK   = uINPUT_ACTION_FIRST_GAME_ID + 1u,   // 17
		COMBAT_ACTION_HEAVY_ATTACK   = uINPUT_ACTION_FIRST_GAME_ID + 2u,   // 18
		COMBAT_ACTION_DODGE          = uINPUT_ACTION_FIRST_GAME_ID + 3u,   // 19
		COMBAT_ACTION_PAUSE          = uINPUT_ACTION_FIRST_GAME_ID + 4u,   // 20
		COMBAT_ACTION_RESTART        = uINPUT_ACTION_FIRST_GAME_ID + 5u,   // 21
		COMBAT_ACTION_RETURN_TO_MENU = uINPUT_ACTION_FIRST_GAME_ID + 6u,   // 22

		COMBAT_ACTION_COUNT = 7u
	};

	// Action NAMES. These are the strings the boot-authored graphs' action
	// nodes carry, so they are contract, not decoration.
	inline constexpr const char* szACTION_MOVE           = "Move";
	inline constexpr const char* szACTION_LIGHT_ATTACK   = "LightAttack";
	inline constexpr const char* szACTION_HEAVY_ATTACK   = "HeavyAttack";
	inline constexpr const char* szACTION_DODGE          = "Dodge";
	inline constexpr const char* szACTION_PAUSE          = "Pause";
	inline constexpr const char* szACTION_RESTART        = "Restart";
	inline constexpr const char* szACTION_RETURN_TO_MENU = "ReturnToMenu";

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

		// MOVE -- the only AXIS2D. Three rows: the keyboard composite (WASD and
		// the arrows on ONE row, so they can never disagree), the pad's left
		// stick, and its d-pad as a second composite. The stick's y scale is -1
		// because GLFW reports a forward push as -1 and the engine convention
		// is +y FORWARD.
		xActions.RegisterAction(COMBAT_ACTION_MOVE, szACTION_MOVE, INPUT_ACTION_AXIS2D);
		xActions.RegisterBinding(COMBAT_ACTION_MOVE,
			Zenith_InputBinding::KeyAxis2D(ZENITH_KEY_W, ZENITH_KEY_S, ZENITH_KEY_A, ZENITH_KEY_D)
				.WithKey(Zenith_InputBinding::uSET_FORWARD, ZENITH_KEY_UP)
				.WithKey(Zenith_InputBinding::uSET_BACK,    ZENITH_KEY_DOWN)
				.WithKey(Zenith_InputBinding::uSET_LEFT,    ZENITH_KEY_LEFT)
				.WithKey(Zenith_InputBinding::uSET_RIGHT,   ZENITH_KEY_RIGHT));
		xActions.RegisterBinding(COMBAT_ACTION_MOVE,
			Zenith_InputBinding::GamepadStick(ZENITH_GAMEPAD_AXIS_LEFT_X, 1.0f, -1.0f));
		xActions.RegisterBinding(COMBAT_ACTION_MOVE, Zenith_InputBinding::GamepadDpadAxis2D());

		// LIGHT_ATTACK -- the mouse rows are claim-filtered at 10e, so a click
		// a UI widget took never also swings.
		xActions.RegisterAction(COMBAT_ACTION_LIGHT_ATTACK, szACTION_LIGHT_ATTACK, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(COMBAT_ACTION_LIGHT_ATTACK,
			Zenith_InputBinding::MouseButton(ZENITH_MOUSE_BUTTON_LEFT));
		xActions.RegisterBinding(COMBAT_ACTION_LIGHT_ATTACK,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_X));

		xActions.RegisterAction(COMBAT_ACTION_HEAVY_ATTACK, szACTION_HEAVY_ATTACK, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(COMBAT_ACTION_HEAVY_ATTACK,
			Zenith_InputBinding::MouseButton(ZENITH_MOUSE_BUTTON_RIGHT));
		xActions.RegisterBinding(COMBAT_ACTION_HEAVY_ATTACK,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_Y));

		xActions.RegisterAction(COMBAT_ACTION_DODGE, szACTION_DODGE, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(COMBAT_ACTION_DODGE,
			Zenith_InputBinding::KeySet(ZENITH_KEY_SPACE));
		xActions.RegisterBinding(COMBAT_ACTION_DODGE,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_B));

		// The three graph-consumed actions. Their consumers are state-gated
		// inside Combat_GameFlow.bgraph; the binding rows themselves are not.
		xActions.RegisterAction(COMBAT_ACTION_PAUSE, szACTION_PAUSE, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(COMBAT_ACTION_PAUSE,
			Zenith_InputBinding::KeySet(ZENITH_KEY_P));
		xActions.RegisterBinding(COMBAT_ACTION_PAUSE,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_START));

		xActions.RegisterAction(COMBAT_ACTION_RESTART, szACTION_RESTART, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(COMBAT_ACTION_RESTART,
			Zenith_InputBinding::KeySet(ZENITH_KEY_R));
		xActions.RegisterBinding(COMBAT_ACTION_RESTART,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_BACK));

		// ★ Pad B again -- see the header note. Deliberate, specified, and
		// pinned by Combat_SimPad_Test.
		xActions.RegisterAction(COMBAT_ACTION_RETURN_TO_MENU, szACTION_RETURN_TO_MENU, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(COMBAT_ACTION_RETURN_TO_MENU,
			Zenith_InputBinding::KeySet(ZENITH_KEY_ESCAPE));
		xActions.RegisterBinding(COMBAT_ACTION_RETURN_TO_MENU,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_B));
	}

	// ---- Readers (NON-CONSUMING reads of CLOSED state) ---------------------
	//
	// ★ THESE ARE FRAME-CONTRACT READS, NOT POLLS. The action layer closes at
	// step 10e and game logic runs at step 11, so a consumer inside an ECS
	// OnUpdate (or a graph node the component drove) sees this frame's edges.
	// Code that runs OUTSIDE the main loop must drive a LOCAL
	// Zenith_InputActions instead -- these would answer with whatever the last
	// real frame left behind.

	// +y is FORWARD, x is RIGHT, diagonals UNNORMALISED, opposite keys cancel.
	inline Zenith_Maths::Vector2 ReadMove()
	{
		Zenith_Maths::Vector2 xMove(0.0f);
		g_xEngine.Actions().GetAxis2D(COMBAT_ACTION_MOVE, xMove);
		return xMove;
	}

	inline bool ReadLightAttackPressed() { return g_xEngine.Actions().WasPressedThisFrame(COMBAT_ACTION_LIGHT_ATTACK); }
	inline bool ReadHeavyAttackPressed() { return g_xEngine.Actions().WasPressedThisFrame(COMBAT_ACTION_HEAVY_ATTACK); }
	inline bool ReadDodgePressed()       { return g_xEngine.Actions().WasPressedThisFrame(COMBAT_ACTION_DODGE); }
}
