#pragma once

#include "Core/Zenith_Engine.h"
#include "Input/Zenith_InputActions.h"
#include "Input/Zenith_KeyCodes.h"
#include "Input/Zenith_Pointers.h"
#include "Maths/Zenith_Maths.h"

// ============================================================================
// TilePuzzle_Bindings -- TilePuzzle's ACTION TABLE (input program C2), and the
// ONE place in this game's PRODUCTION code where a raw device code may be
// spelled.
//
// Everything goes through the engine action layer (g_xEngine.Actions()): a
// reader asks "was ESCAPE pressed", and the binding table below decides whether
// the keyboard or a gamepad answered. The one exception is POINTER POSITION,
// which is not an action at all -- see the board-pointer section further down.
//
// THREE PROFILES (B5). A scheme may live in at most ONE registered profile, and
// the FIRST game RegisterProfile call CLEARS the engine's platform default
// wholesale -- so Register() installs all three or none.
//
//   P_DESKTOP { KEYBOARD | MOUSE }   P_TOUCH { TOUCH }   P_GAMEPAD { GAMEPAD }
//
// KEYBOARD and MOUSE share one profile because a desktop player has a hand on
// each and splitting them would kill the cursor keys the instant they clicked.
// TOUCH is its own profile because this is an Android game (PORTRAIT): Android
// boots into P_TOUCH (ResolveBootDefaultProfile picks whichever profile owns
// TOUCH there, and whichever owns KEYBOARD on Windows), where the MOUSE column
// is masked out and PRIMARY answers on its TOUCH_PRIMARY row instead.
//
// Line-by-line the table below is the C2 contract:
//
//   | Action       | Kind   | KEYBOARD     | MOUSE | TOUCH         | GAMEPAD  |
//   | CURSOR_UP    | BUTTON | W / Up       | --    | --            | dpad Up  |
//   | CURSOR_DOWN  | BUTTON | S / Down     | --    | --            | dpad Down|
//   | CURSOR_LEFT  | BUTTON | A / Left     | --    | --            | dpad Left|
//   | CURSOR_RIGHT | BUTTON | D / Right    | --    | --            | dpadRight|
//   | SELECT       | BUTTON | Space        | --    | --            | A        |
//   | RESET_LEVEL  | BUTTON | R            | --    | --            | X        |
//   | NEXT_LEVEL   | BUTTON | N            | --    | --            | Y        |
//   | ESCAPE       | BUTTON | Escape       | --    | --            | Start    |
//   | PRIMARY      | BUTTON | --           | LMB   | TOUCH_PRIMARY | --       |
//
// ---------------------------------------------------------------------------
// ★ THE CURSOR IS A BUTTON PER DIRECTION, NOT AN AXIS2D. One press must be
// exactly one cell of cursor travel; an AXIS2D is a LEVEL, and a level cannot
// say how many steps a held key is worth. (This is the same reason ZM's battle
// cursor is MENU_UP / MENU_DOWN rather than a re-read of MOVE.)
//
// ★ PRIMARY HAS NO PAD ROW, AND THAT IS THE SPECIFIED NARROWING. PRIMARY is the
// press half of a POINTER gesture -- picking a shape up, grabbing the plunger,
// dismissing an overlay -- and every one of those needs a POSITION the pad
// cannot supply. Binding a face button to it would produce a press that lands
// wherever the mouse happened to be left. The pad's scope is the cursor and menu
// verbs above; pointer-position gameplay is deliberately not pad-operable.
//
// ★ PRIMARY IS CLAIM-AWARE ON BOTH ITS ROWS, and that is engine behaviour rather
// than anything spelled here: at frame-contract step 10e the action layer
// SUPPRESSES a pointer-sourced transition whose pointer a UI widget claimed --
// TOUCH_PRIMARY by construction, and MOUSE_BUTTON because the Windows mouse
// reaches pointer 0 through the same permanent B3 projection a finger does. So a
// tap that a button consumed can never also dismiss the tutorial behind it.
//
// Engine-reserved ids 0-15 are the UI nav set; this game's ids start at 16
// (uINPUT_ACTION_FIRST_GAME_ID) and stay below 126 -- the engine's own test
// actions occupy 126/127 and RegisterAction asserts on a collision.
//
// ---------------------------------------------------------------------------
// WHAT IS *NOT* AN ACTION: POINTER POSITION.
//
// Dragging a shape across the board, hit-testing which shape was grabbed,
// pulling the pinball plunger and swiping the cat cafe all need to know WHERE
// the finger is, which no binding row can carry. Those read the pointer table
// (g_xEngine.Pointers()) directly, through ReadBoardPointer / IsBoardPointerDown
// below. B3 makes that ONE code path for both platforms: the mouse feeds pointer
// 0 on Windows and the first finger feeds it on Android.
//
// ★ A DIRECT POINTER READ GETS NO CLAIM SUPPRESSION FOR FREE -- the action layer
// does that at 10e for its own rows, and gameplay reading the table itself is
// underneath that. So ReadBoardPointer applies the rule ITSELF: a CLAIMED
// pointer is reported as no pointer at all. Without it, a press a UI button took
// would also pick up whatever shape sat behind the button.
// ============================================================================

namespace TilePuzzle_Bindings
{
	// ---- Profiles (B5) -----------------------------------------------------
	// Ids are this game's own; 0xF0 / 0xF1 / 0xFF are engine-reserved.
	inline constexpr u_int8 uPROFILE_DESKTOP = 0u;
	inline constexpr u_int8 uPROFILE_TOUCH   = 1u;
	inline constexpr u_int8 uPROFILE_GAMEPAD = 2u;

	inline constexpr const char* szPROFILE_DESKTOP_NAME = "P_DESKTOP";
	inline constexpr const char* szPROFILE_TOUCH_NAME   = "P_TOUCH";
	inline constexpr const char* szPROFILE_GAMEPAD_NAME = "P_GAMEPAD";

	// ---- Actions -----------------------------------------------------------
	// Contiguous from uINPUT_ACTION_FIRST_GAME_ID; TILEPUZZLE_ACTION_COUNT is the
	// COUNT (9), not the next id.
	enum TILEPUZZLE_ACTION : Zenith_InputActionID
	{
		TILEPUZZLE_ACTION_CURSOR_UP    = uINPUT_ACTION_FIRST_GAME_ID,        // 16
		TILEPUZZLE_ACTION_CURSOR_DOWN  = uINPUT_ACTION_FIRST_GAME_ID + 1u,   // 17
		TILEPUZZLE_ACTION_CURSOR_LEFT  = uINPUT_ACTION_FIRST_GAME_ID + 2u,   // 18
		TILEPUZZLE_ACTION_CURSOR_RIGHT = uINPUT_ACTION_FIRST_GAME_ID + 3u,   // 19
		TILEPUZZLE_ACTION_SELECT       = uINPUT_ACTION_FIRST_GAME_ID + 4u,   // 20
		TILEPUZZLE_ACTION_RESET_LEVEL  = uINPUT_ACTION_FIRST_GAME_ID + 5u,   // 21
		TILEPUZZLE_ACTION_NEXT_LEVEL   = uINPUT_ACTION_FIRST_GAME_ID + 6u,   // 22
		TILEPUZZLE_ACTION_ESCAPE       = uINPUT_ACTION_FIRST_GAME_ID + 7u,   // 23
		TILEPUZZLE_ACTION_PRIMARY      = uINPUT_ACTION_FIRST_GAME_ID + 8u,   // 24

		TILEPUZZLE_ACTION_COUNT = 9u
	};

	static_assert(TILEPUZZLE_ACTION_PRIMARY < 126u,
		"Game action ids must stay below the engine's reserved test ids 126/127");

	// Action NAMES. These are the strings a Behaviour Graph's OnActionPressed
	// node or an on-screen control's SetAction would carry, so they are contract
	// rather than decoration.
	inline constexpr const char* szACTION_CURSOR_UP    = "CursorUp";
	inline constexpr const char* szACTION_CURSOR_DOWN  = "CursorDown";
	inline constexpr const char* szACTION_CURSOR_LEFT  = "CursorLeft";
	inline constexpr const char* szACTION_CURSOR_RIGHT = "CursorRight";
	inline constexpr const char* szACTION_SELECT       = "Select";
	inline constexpr const char* szACTION_RESET_LEVEL  = "ResetLevel";
	inline constexpr const char* szACTION_NEXT_LEVEL   = "NextLevel";
	inline constexpr const char* szACTION_ESCAPE       = "Escape";
	inline constexpr const char* szACTION_PRIMARY      = "Primary";

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
		xActions.RegisterProfile(uPROFILE_TOUCH, szPROFILE_TOUCH_NAME,
			uINPUT_SCHEME_MASK_TOUCH);
		xActions.RegisterProfile(uPROFILE_GAMEPAD, szPROFILE_GAMEPAD_NAME,
			uINPUT_SCHEME_MASK_GAMEPAD);

		// The four cursor directions. WASD and the arrows share ONE KeySet per
		// direction (a KeySet is an OR over its keys), so the two spellings can
		// never disagree about which way is up.
		xActions.RegisterAction(TILEPUZZLE_ACTION_CURSOR_UP, szACTION_CURSOR_UP, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(TILEPUZZLE_ACTION_CURSOR_UP,
			Zenith_InputBinding::KeySet(ZENITH_KEY_W, ZENITH_KEY_UP));
		xActions.RegisterBinding(TILEPUZZLE_ACTION_CURSOR_UP,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_DPAD_UP));

		xActions.RegisterAction(TILEPUZZLE_ACTION_CURSOR_DOWN, szACTION_CURSOR_DOWN, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(TILEPUZZLE_ACTION_CURSOR_DOWN,
			Zenith_InputBinding::KeySet(ZENITH_KEY_S, ZENITH_KEY_DOWN));
		xActions.RegisterBinding(TILEPUZZLE_ACTION_CURSOR_DOWN,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_DPAD_DOWN));

		xActions.RegisterAction(TILEPUZZLE_ACTION_CURSOR_LEFT, szACTION_CURSOR_LEFT, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(TILEPUZZLE_ACTION_CURSOR_LEFT,
			Zenith_InputBinding::KeySet(ZENITH_KEY_A, ZENITH_KEY_LEFT));
		xActions.RegisterBinding(TILEPUZZLE_ACTION_CURSOR_LEFT,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_DPAD_LEFT));

		xActions.RegisterAction(TILEPUZZLE_ACTION_CURSOR_RIGHT, szACTION_CURSOR_RIGHT, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(TILEPUZZLE_ACTION_CURSOR_RIGHT,
			Zenith_InputBinding::KeySet(ZENITH_KEY_D, ZENITH_KEY_RIGHT));
		xActions.RegisterBinding(TILEPUZZLE_ACTION_CURSOR_RIGHT,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_DPAD_RIGHT));

		// SELECT / RESET_LEVEL / NEXT_LEVEL -- the menu verbs. Their pad faces are
		// deliberately all DIFFERENT: unlike Combat's shared B, no two of these
		// are ever live in the same state without being distinguishable.
		xActions.RegisterAction(TILEPUZZLE_ACTION_SELECT, szACTION_SELECT, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(TILEPUZZLE_ACTION_SELECT,
			Zenith_InputBinding::KeySet(ZENITH_KEY_SPACE));
		xActions.RegisterBinding(TILEPUZZLE_ACTION_SELECT,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_A));

		xActions.RegisterAction(TILEPUZZLE_ACTION_RESET_LEVEL, szACTION_RESET_LEVEL, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(TILEPUZZLE_ACTION_RESET_LEVEL,
			Zenith_InputBinding::KeySet(ZENITH_KEY_R));
		xActions.RegisterBinding(TILEPUZZLE_ACTION_RESET_LEVEL,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_X));

		xActions.RegisterAction(TILEPUZZLE_ACTION_NEXT_LEVEL, szACTION_NEXT_LEVEL, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(TILEPUZZLE_ACTION_NEXT_LEVEL,
			Zenith_InputBinding::KeySet(ZENITH_KEY_N));
		xActions.RegisterBinding(TILEPUZZLE_ACTION_NEXT_LEVEL,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_Y));

		// ESCAPE -- back out of whatever screen is up. The only cursor/menu verb
		// with a live consumer today (both game components read it every frame).
		xActions.RegisterAction(TILEPUZZLE_ACTION_ESCAPE, szACTION_ESCAPE, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(TILEPUZZLE_ACTION_ESCAPE,
			Zenith_InputBinding::KeySet(ZENITH_KEY_ESCAPE));
		xActions.RegisterBinding(TILEPUZZLE_ACTION_ESCAPE,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_START));

		// PRIMARY -- the press half of a pointer gesture. Two rows, one per
		// platform, and the engine claim-filters BOTH at 10e (see the header
		// note). No pad row by design.
		xActions.RegisterAction(TILEPUZZLE_ACTION_PRIMARY, szACTION_PRIMARY, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(TILEPUZZLE_ACTION_PRIMARY,
			Zenith_InputBinding::MouseButton(ZENITH_MOUSE_BUTTON_LEFT));
		xActions.RegisterBinding(TILEPUZZLE_ACTION_PRIMARY,
			Zenith_InputBinding::TouchPrimary());
	}

	// ---- Readers (NON-CONSUMING reads of CLOSED state) ---------------------
	//
	// ★ THESE ARE FRAME-CONTRACT READS, NOT POLLS. The action layer closes at
	// step 10e and game logic runs at step 11, so a consumer inside an ECS
	// OnUpdate sees this frame's edges. Code that runs OUTSIDE the main loop must
	// drive a LOCAL Zenith_InputActions instead -- these would answer with
	// whatever the last real frame left behind.

	inline bool WasCursorUpPressed()    { return g_xEngine.Actions().WasPressedThisFrame(TILEPUZZLE_ACTION_CURSOR_UP); }
	inline bool WasCursorDownPressed()  { return g_xEngine.Actions().WasPressedThisFrame(TILEPUZZLE_ACTION_CURSOR_DOWN); }
	inline bool WasCursorLeftPressed()  { return g_xEngine.Actions().WasPressedThisFrame(TILEPUZZLE_ACTION_CURSOR_LEFT); }
	inline bool WasCursorRightPressed() { return g_xEngine.Actions().WasPressedThisFrame(TILEPUZZLE_ACTION_CURSOR_RIGHT); }
	inline bool WasSelectPressed()      { return g_xEngine.Actions().WasPressedThisFrame(TILEPUZZLE_ACTION_SELECT); }
	inline bool WasResetLevelPressed()  { return g_xEngine.Actions().WasPressedThisFrame(TILEPUZZLE_ACTION_RESET_LEVEL); }
	inline bool WasNextLevelPressed()   { return g_xEngine.Actions().WasPressedThisFrame(TILEPUZZLE_ACTION_NEXT_LEVEL); }
	inline bool WasEscapePressed()      { return g_xEngine.Actions().WasPressedThisFrame(TILEPUZZLE_ACTION_ESCAPE); }

	inline bool WasPrimaryPressed()  { return g_xEngine.Actions().WasPressedThisFrame(TILEPUZZLE_ACTION_PRIMARY); }
	inline bool WasPrimaryReleased() { return g_xEngine.Actions().WasReleasedThisFrame(TILEPUZZLE_ACTION_PRIMARY); }
	inline bool IsPrimaryHeld()      { return g_xEngine.Actions().IsHeld(TILEPUZZLE_ACTION_PRIMARY); }

	// ---- The board pointer (POSITION, which no action can carry) -----------
	//
	// The primary pointer -- pointer 0, which B3 keeps in step with the mouse on
	// Windows and with the first finger on Android -- as gameplay is allowed to
	// see it. Returns false when there is no pointer record at all, and ALSO when
	// the one there is has been CLAIMED by a UI widget: a claimed pointer is a
	// consumed one, and treating it as board input is exactly the double-actuation
	// the claim exists to prevent.
	//
	// bOutDown distinguishes "the finger is still down" from "the record is still
	// here". They differ for exactly one frame -- the terminal UP/CANCEL frame,
	// which the slot survives so its owner can see the edge -- and that frame is
	// where a release-gesture reader (the cat cafe swipe) takes its end position.
	//
	// Positions are RAW SURFACE PIXELS, the same space Zenith_Input::GetMousePosition
	// reported, so screen-space consumers need no remapping.
	inline bool ReadBoardPointer(Zenith_Maths::Vector2& xOutPosition, bool& bOutDown)
	{
		bOutDown = false;

		const Zenith_Pointers& xPointers = g_xEngine.Pointers();
		const Zenith_Pointer* pxPointer = xPointers.Resolve(xPointers.GetPrimaryPointer());
		if (pxPointer == nullptr || pxPointer->IsClaimed())
		{
			return false;
		}

		xOutPosition = pxPointer->m_xPosition;
		bOutDown = pxPointer->IsDown();
		return true;
	}

	// The common case: "is a board-usable pointer down, and where". This is the
	// direct replacement for the retired IsMouseButtonHeld + GetMousePosition
	// pair, claim rule included.
	inline bool IsBoardPointerDown(Zenith_Maths::Vector2& xOutPosition)
	{
		bool bDown = false;
		return ReadBoardPointer(xOutPosition, bDown) && bDown;
	}
}
