#pragma once

#include "Core/Zenith_Engine.h"
#include "Input/Zenith_InputActions.h"
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"

// ============================================================================
// ZM_Bindings -- Zenithmon's ACTION TABLE (input program C2), and the ONE place
// in this game's PRODUCTION code where a raw device code may be spelled.
//
// Everything goes through the engine action layer (g_xEngine.Actions()):
// a reader asks "is RUN held", and the binding table below decides whether
// the keyboard, a gamepad or a thumb on glass answered.
//
// THREE PROFILES, ONE SCHEME EACH (B5). A scheme may live in at most ONE
// registered profile, and the FIRST game RegisterProfile call CLEARS the
// engine's platform default wholesale -- so Register() installs all three
// together or none of them. MOUSE deliberately belongs to no profile: this game
// has no mouse-sourced ACTION, and menu taps ride the pointer table
// (Zenith_UICanvas::NotifyPointerActivate), which is not scheme-gated.
//
// ONE VIRTUAL SOURCE PER ACTION, NEVER PER WIDGET. A Zenith_UIVirtualButton
// targets an ACTION NAME and publishes to whatever VIRTUAL row that action
// carries, so the physical "A" button retargets between INTERACT (overworld) and
// CONFIRM (dialogue / menu / battle) by naming a different action -- and the two
// actions can never light up together, which is exactly what a shared source id
// would have caused.
//
// Line-by-line the table below is the C2 contract:
//
//   | Action    | Kind   | KEYBOARD              | TOUCH        | GAMEPAD          |
//   | MOVE      | AXIS2D | WASD + arrows         | VIRTUAL stick| left stick + dpad|
//   | RUN       | BUTTON | LShift / RShift       | VIRTUAL B    | B                |
//   | INTERACT  | BUTTON | E                     | VIRTUAL A    | X                |
//   | CONFIRM   | BUTTON | Enter / Space         | VIRTUAL A    | A                |
//   | CANCEL    | BUTTON | Escape / Backspace    | VIRTUAL B    | B                |
//   |           |        | + SYSTEM_BACK (exempt)|              |                  |
//   | MENU      | BUTTON | M / Tab               | VIRTUAL menu | Start            |
//   | MENU_UP   | BUTTON | W / Up                | -- (taps)    | dpad Up          |
//   | MENU_DOWN | BUTTON | S / Down              | -- (taps)    | dpad Down        |
//
// Engine-reserved ids 0-15 are the UI nav set; this game's ids start at 16
// (uINPUT_ACTION_FIRST_GAME_ID) and MENU_UP / MENU_DOWN are BUTTONS rather than
// an axis because one press must be exactly one cursor step.
// ============================================================================

namespace ZM_Bindings
{
	// ---- Profiles (B5) -----------------------------------------------------
	// Ids are this game's own; 0xF0 / 0xF1 / 0xFF are engine-reserved.
	inline constexpr u_int8 uPROFILE_KEYBOARD = 0u;
	inline constexpr u_int8 uPROFILE_TOUCH    = 1u;
	inline constexpr u_int8 uPROFILE_GAMEPAD  = 2u;

	inline constexpr const char* szPROFILE_KEYBOARD_NAME = "P_KEYBOARD";
	inline constexpr const char* szPROFILE_TOUCH_NAME    = "P_TOUCH";
	inline constexpr const char* szPROFILE_GAMEPAD_NAME  = "P_GAMEPAD";

	// ---- Actions -----------------------------------------------------------
	// Ids are contiguous from uINPUT_ACTION_FIRST_GAME_ID; ZM_ACTION_COUNT is
	// deliberately the COUNT (8), not the next id, and the boot units
	// static_assert their walk against it.
	enum ZM_ACTION : Zenith_InputActionID
	{
		ZM_ACTION_MOVE      = uINPUT_ACTION_FIRST_GAME_ID,        // 16
		ZM_ACTION_RUN       = uINPUT_ACTION_FIRST_GAME_ID + 1u,   // 17
		ZM_ACTION_INTERACT  = uINPUT_ACTION_FIRST_GAME_ID + 2u,   // 18
		ZM_ACTION_CONFIRM   = uINPUT_ACTION_FIRST_GAME_ID + 3u,   // 19
		ZM_ACTION_CANCEL    = uINPUT_ACTION_FIRST_GAME_ID + 4u,   // 20
		ZM_ACTION_MENU      = uINPUT_ACTION_FIRST_GAME_ID + 5u,   // 21
		ZM_ACTION_MENU_UP   = uINPUT_ACTION_FIRST_GAME_ID + 6u,   // 22
		ZM_ACTION_MENU_DOWN = uINPUT_ACTION_FIRST_GAME_ID + 7u,   // 23

		ZM_ACTION_COUNT = 8u
	};

	// Action NAMES. These are the strings an on-screen control's SetAction takes
	// and the strings a Behaviour Graph node would name, so they are contract,
	// not decoration.
	inline constexpr const char* szACTION_MOVE      = "Move";
	inline constexpr const char* szACTION_RUN       = "Run";
	inline constexpr const char* szACTION_INTERACT  = "Interact";
	inline constexpr const char* szACTION_CONFIRM   = "Confirm";
	inline constexpr const char* szACTION_CANCEL    = "Cancel";
	inline constexpr const char* szACTION_MENU      = "Menu";
	inline constexpr const char* szACTION_MENU_UP   = "MenuUp";
	inline constexpr const char* szACTION_MENU_DOWN = "MenuDown";

	// ---- Virtual sources (the on-screen controls publish into these) --------
	// One per action that a widget can target. Never share an id between two
	// actions: a shared source holds BOTH actions at once.
	inline constexpr u_int16 uVIRTUAL_MOVE     = 0u;
	inline constexpr u_int16 uVIRTUAL_INTERACT = 1u;
	inline constexpr u_int16 uVIRTUAL_RUN      = 2u;
	inline constexpr u_int16 uVIRTUAL_CONFIRM  = 3u;
	inline constexpr u_int16 uVIRTUAL_CANCEL   = 4u;
	inline constexpr u_int16 uVIRTUAL_MENU     = 5u;

	// ---- Registration ------------------------------------------------------
	// Takes the layer BY REFERENCE so a boot unit can install this exact table
	// into a LOCAL Zenith_InputActions and never touch the engine's (B13).
	// Called once per process from Project_RegisterGameComponents.
	inline void Register(Zenith_InputActions& xActions)
	{
		// Profiles FIRST: the first call clears the engine defaults, and doing
		// that after the actions were bound would leave a window in which a
		// registered row resolved against a profile set this game does not own.
		xActions.RegisterProfile(uPROFILE_KEYBOARD, szPROFILE_KEYBOARD_NAME, uINPUT_SCHEME_MASK_KEYBOARD);
		xActions.RegisterProfile(uPROFILE_TOUCH,    szPROFILE_TOUCH_NAME,    uINPUT_SCHEME_MASK_TOUCH);
		xActions.RegisterProfile(uPROFILE_GAMEPAD,  szPROFILE_GAMEPAD_NAME,  uINPUT_SCHEME_MASK_GAMEPAD);

		// MOVE -- the only AXIS2D. Four rows, which is the per-action maximum:
		// keyboard composite, the on-screen stick, the pad's left stick, and the
		// pad's d-pad as a composite. The stick's y scale is -1 because GLFW
		// reports a forward push as -1 and the engine convention is +y FORWARD.
		xActions.RegisterAction(ZM_ACTION_MOVE, szACTION_MOVE, INPUT_ACTION_AXIS2D);
		xActions.RegisterBinding(ZM_ACTION_MOVE,
			Zenith_InputBinding::KeyAxis2D(ZENITH_KEY_W, ZENITH_KEY_S, ZENITH_KEY_A, ZENITH_KEY_D)
				.WithKey(Zenith_InputBinding::uSET_FORWARD, ZENITH_KEY_UP)
				.WithKey(Zenith_InputBinding::uSET_BACK,    ZENITH_KEY_DOWN)
				.WithKey(Zenith_InputBinding::uSET_LEFT,    ZENITH_KEY_LEFT)
				.WithKey(Zenith_InputBinding::uSET_RIGHT,   ZENITH_KEY_RIGHT));
		xActions.RegisterBinding(ZM_ACTION_MOVE, Zenith_InputBinding::Virtual(uVIRTUAL_MOVE));
		xActions.RegisterBinding(ZM_ACTION_MOVE,
			Zenith_InputBinding::GamepadStick(ZENITH_GAMEPAD_AXIS_LEFT_X, 1.0f, -1.0f));
		xActions.RegisterBinding(ZM_ACTION_MOVE, Zenith_InputBinding::GamepadDpadAxis2D());

		// RUN -- held, not edged: the speed selector samples it every frame.
		xActions.RegisterAction(ZM_ACTION_RUN, szACTION_RUN, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(ZM_ACTION_RUN,
			Zenith_InputBinding::KeySet(ZENITH_KEY_LEFT_SHIFT, ZENITH_KEY_RIGHT_SHIFT));
		xActions.RegisterBinding(ZM_ACTION_RUN, Zenith_InputBinding::Virtual(uVIRTUAL_RUN));
		xActions.RegisterBinding(ZM_ACTION_RUN,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_B));

		// INTERACT -- talk to / examine. Pad X so it is not the same face button
		// as CONFIRM: the two are live in DIFFERENT contexts on touch, but on a
		// pad both exist at once and aliasing them would open a menu every time
		// the player talked to somebody.
		xActions.RegisterAction(ZM_ACTION_INTERACT, szACTION_INTERACT, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(ZM_ACTION_INTERACT, Zenith_InputBinding::KeySet(ZENITH_KEY_E));
		xActions.RegisterBinding(ZM_ACTION_INTERACT, Zenith_InputBinding::Virtual(uVIRTUAL_INTERACT));
		xActions.RegisterBinding(ZM_ACTION_INTERACT,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_X));

		xActions.RegisterAction(ZM_ACTION_CONFIRM, szACTION_CONFIRM, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(ZM_ACTION_CONFIRM,
			Zenith_InputBinding::KeySet(ZENITH_KEY_ENTER, ZENITH_KEY_SPACE));
		xActions.RegisterBinding(ZM_ACTION_CONFIRM, Zenith_InputBinding::Virtual(uVIRTUAL_CONFIRM));
		xActions.RegisterBinding(ZM_ACTION_CONFIRM,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_A));

		// CANCEL carries the platform BACK gesture, and that row is MASK-EXEMPT:
		// Android's system Back must close a menu whatever profile is active, and
		// it is excluded from activity detection so it cannot itself change one.
		// Four rows again -- the per-action maximum.
		xActions.RegisterAction(ZM_ACTION_CANCEL, szACTION_CANCEL, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(ZM_ACTION_CANCEL,
			Zenith_InputBinding::KeySet(ZENITH_KEY_ESCAPE, ZENITH_KEY_BACKSPACE));
		xActions.RegisterBinding(ZM_ACTION_CANCEL, Zenith_InputBinding::SystemBack());
		xActions.RegisterBinding(ZM_ACTION_CANCEL, Zenith_InputBinding::Virtual(uVIRTUAL_CANCEL));
		xActions.RegisterBinding(ZM_ACTION_CANCEL,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_B));

		xActions.RegisterAction(ZM_ACTION_MENU, szACTION_MENU, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(ZM_ACTION_MENU,
			Zenith_InputBinding::KeySet(ZENITH_KEY_M, ZENITH_KEY_TAB));
		xActions.RegisterBinding(ZM_ACTION_MENU, Zenith_InputBinding::Virtual(uVIRTUAL_MENU));
		xActions.RegisterBinding(ZM_ACTION_MENU,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_START));

		// The battle menu's vertical cursor. Same keys as forward / back movement
		// by design (the old ReadMenuVertical walked the movement sets), but as
		// their OWN actions so a rebind of walking cannot silently rebind the
		// battle cursor. No TOUCH row: a touch player taps the entry directly.
		xActions.RegisterAction(ZM_ACTION_MENU_UP, szACTION_MENU_UP, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(ZM_ACTION_MENU_UP,
			Zenith_InputBinding::KeySet(ZENITH_KEY_W, ZENITH_KEY_UP));
		xActions.RegisterBinding(ZM_ACTION_MENU_UP,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_DPAD_UP));

		xActions.RegisterAction(ZM_ACTION_MENU_DOWN, szACTION_MENU_DOWN, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(ZM_ACTION_MENU_DOWN,
			Zenith_InputBinding::KeySet(ZENITH_KEY_S, ZENITH_KEY_DOWN));
		xActions.RegisterBinding(ZM_ACTION_MENU_DOWN,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_DPAD_DOWN));
	}

	// ---- Readers (NON-CONSUMING reads of CLOSED state) ---------------------
	//
	// ★ THESE ARE FRAME-CONTRACT READS, NOT POLLS. The action layer closes at
	// step 10e, and game logic runs at step 11, so a consumer inside an ECS
	// OnUpdate sees this frame's edges. Code that runs OUTSIDE the main loop (a
	// boot unit) must drive a LOCAL Zenith_InputActions instead -- these would
	// answer with whatever the last real frame left behind.

	inline Zenith_Maths::Vector2 ReadMove()
	{
		Zenith_Maths::Vector2 xMove(0.0f);
		g_xEngine.Actions().GetAxis2D(ZM_ACTION_MOVE, xMove);
		return xMove;
	}

	inline bool ReadRunHeld()          { return g_xEngine.Actions().IsHeld(ZM_ACTION_RUN); }
	inline bool ReadInteractPressed()  { return g_xEngine.Actions().WasPressedThisFrame(ZM_ACTION_INTERACT); }
	inline bool ReadConfirmPressed()   { return g_xEngine.Actions().WasPressedThisFrame(ZM_ACTION_CONFIRM); }
	inline bool ReadCancelPressed()    { return g_xEngine.Actions().WasPressedThisFrame(ZM_ACTION_CANCEL); }
	inline bool ReadMenuPressed()      { return g_xEngine.Actions().WasPressedThisFrame(ZM_ACTION_MENU); }

	// -1 up / +1 down / 0 none -- EDGE, so one press is exactly one step. Both
	// edges in the same frame cancel, exactly as the pre-migration reader did.
	inline int ReadMenuVertical()
	{
		const Zenith_InputActions& xActions = g_xEngine.Actions();
		const bool bUp   = xActions.WasPressedThisFrame(ZM_ACTION_MENU_UP);
		const bool bDown = xActions.WasPressedThisFrame(ZM_ACTION_MENU_DOWN);
		return (bDown ? 1 : 0) - (bUp ? 1 : 0);
	}
}
