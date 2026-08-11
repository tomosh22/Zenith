#pragma once

#include "Core/Zenith_Engine.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputActions.h"
#include "Input/Zenith_KeyCodes.h"
#include "Input/Zenith_Pointers.h"
#include "Maths/Zenith_Maths.h"

// ============================================================================
// CB_Bindings -- CityBuilder's ACTION TABLE (input program C2), and the ONE
// place in this game's PRODUCTION code where a raw device code may be spelled.
//
// Everything goes through the engine action layer (g_xEngine.Actions()): a
// reader asks "is PRIMARY held" or "was TOOL_5 pressed", and the binding table
// below decides whether the keyboard, the mouse or a gamepad answered. The one
// exception is CURSOR POSITION, which is not an action at all -- see the last
// section.
//
// TWO PROFILES (B5). A scheme may live in at most ONE registered profile, and
// the FIRST game RegisterProfile call CLEARS the engine's platform default
// wholesale -- so Register() installs both or neither.
//
//   P_DESKTOP { KEYBOARD | MOUSE }   P_GAMEPAD { GAMEPAD }
//
// KEYBOARD and MOUSE share ONE profile because this is a city builder played
// with both hands at once: the left pans on WASD and cycles tools on the number
// row while the right drags roads and orbits with the mouse. Splitting them
// would make the auto-switch drop the camera pan the instant the player
// clicked. The pad is its own profile for the opposite reason.
//
// P_DESKTOP is registered FIRST because it doubles as the boot default
// (ResolveBootDefaultProfile picks whichever profile owns KEYBOARD on Windows,
// and falls back to the first registered one elsewhere). CityBuilder is
// WINDOWS-ONLY -- there is no Android configuration and therefore no TOUCH
// profile to consider.
//
// Line-by-line the table below is the C2 contract:
//
//   | Action              | Kind   | KEYBOARD        | MOUSE       | GAMEPAD    |
//   | PRIMARY             | BUTTON | --              | LMB         | -- (narrow)|
//   | SECONDARY           | BUTTON | --              | RMB         | -- (narrow)|
//   | PAN                 | AXIS2D | WASD            | --          | left stick |
//   | ORBIT_MODIFIER      | BUTTON | --              | RMB         | --         |
//   | ORBIT_DELTA         | AXIS2D | --              | MOUSE_DELTA | --         |
//   | ORBIT_RATE          | AXIS2D | --              | --          | right stick|
//   | PAN_DRAG_MODIFIER   | BUTTON | --              | MMB         | --         |
//   | PAN_DRAG_DELTA      | AXIS2D | --              | MOUSE_DELTA | --         |
//   | ROTATE              | AXIS1D | Q (-) / E (+)   | --          | --         |
//   | ZOOM_DELTA          | AXIS1D | --              | wheel       | --         |
//   | ZOOM_RATE           | AXIS1D | --              | --          | RT - LT    |
//   | TOOL_1 .. TOOL_0    | BUTTON | 1 2 3 4 5 6 7 8 9 0 | --      | --         |
//   | TOOL_T/B/L/K        | BUTTON | T / B / L / K   | --          | --         |
//   | TOOL_PREV/TOOL_NEXT | BUTTON | --              | --          | dpad L / R |
//   | PAUSE               | BUTTON | P               | --          | Start      |
//   | SPEED_DOWN/UP       | BUTTON | , / .           | --          | dpad Dn/Up |
//   | ROAD_CLASS          | BUTTON | R               | --          | Y          |
//   | TAX_DOWN / TAX_UP   | BUTTON | - / =           | --          | --         |
//   | LOAN                | BUTTON | G               | --          | --         |
//   | QUICKSAVE/QUICKLOAD | BUTTON | F5 / F9         | --          | --         |
//   | NEW_CITY            | BUTTON | F8              | --          | --         |
//   | IGNITE              | BUTTON | F               | --          | --         |
//   | POLICY_1 .. POLICY_4| BUTTON | F1 F2 F3 F4     | --          | --         |
//
// ---------------------------------------------------------------------------
// SIX ROWS OF THIS TABLE CARRY A DESIGN DECISION WORTH READING.
//
// * THE GROUND-PICKING TOOLS ARE DELIBERATELY NOT PAD-OPERABLE. PRIMARY and
// SECONDARY are the press halves of a POINTER gesture: every consumer of them
// (draw a road node, drag-paint a zone, place a service, bulldoze, paint a
// district, drop a transit stop or a conduit, sculpt with the terraform brush)
// also needs to know WHERE on the ground the cursor is, and a face button
// cannot supply a position -- it would build wherever the mouse was last left.
// So the pad's scope is the camera, the sim clock and TOOL CYCLING, and the
// ground tools stay mouse-only until this game grows a pad cursor. That is the
// specified narrowing, not an omission.
//
// * PRIMARY AND SECONDARY ARE CLAIM-AWARE, and that is engine behaviour rather
// than anything spelled here: at frame-contract step 10e the action layer
// SUPPRESSES a pointer-sourced transition whose pointer a UI widget claimed
// (press and matching release together). So a click that a HUD tool button
// consumed can no longer ALSO lay a road on the ground behind it -- which the
// retired raw IsMouseButtonHeld polls could not distinguish, because the device
// layer knows nothing about widgets.
//
// * SECONDARY AND ORBIT_MODIFIER ARE BOTH RMB, AND THAT IS THE TABLE AS
// SPECIFIED. It is also exactly what the retired code did: CB_RoadController
// polled RMB to end a road while CB_CityCameraComponent polled the same button
// to orbit, so a right-drag both finished the in-progress road and turned the
// camera. Nothing here separates them and nothing needs to: ending an already-
// finished road is idempotent, and the two consumers are the tool layer and the
// camera, which the game deliberately lets run at the same time.
//
// * ORBIT AND PAN-DRAG READ THE SAME MOUSE_DELTA THROUGH TWO ACTIONS. They are
// two different gestures (right-drag turns, middle-drag slides) that happen to
// share one source, and each is gated by its own MODIFIER row. One action could
// not carry both, because the camera applies them with different signs and
// different speeds. Both rows keep scale (1, 1): the sign convention is a
// camera-FEEL decision that the two consumers disagree about (orbit uses +x/+y,
// pan-drag uses -x/+y), so folding a sign into the shared device row would have
// to pick a winner. Signs stay in CB_CityCameraComponent, unchanged.
//
// * ZOOM IS TWO ACTIONS BECAUSE A WHEEL AND A TRIGGER ARE DIFFERENT QUANTITIES,
// and ORBIT IS TWO FOR THE SAME REASON. A wheel and a mouse report DISPLACEMENT
// already integrated over the frame (notches turned, pixels moved), so
// multiplying them by dt would make the response frame-rate dependent -- exactly
// the bug the raw GetMouseWheelDelta / GetMouseDelta paths never had. A trigger
// or a stick reports a deflection, i.e. a RATE, which only becomes a distance or
// an angle after multiplying by dt. One action cannot carry both meanings, so
// ZOOM_DELTA/ORBIT_DELTA are the mouse and ZOOM_RATE/ORBIT_RATE are the pad. The
// camera applies both halves every frame; a resting stick or trigger contributes
// exactly zero (the device layer's radial deadzone) and a still mouse likewise,
// so there is no mode switch and nothing to arbitrate.
//
// * THE PAD'S D-PAD COLLIDES WITH THE ENGINE-RESERVED UI-NAV SET, BY DESIGN.
// Ids 0-15 bind dpad Up/Down/Left/Right to UI_NAV_UP/DOWN/LEFT/RIGHT, and
// SPEED_UP / SPEED_DOWN / TOOL_PREV / TOOL_NEXT bind the same four buttons.
// BOTH fire: the reserved rows close at 10b and drive CANVAS FOCUS, these close
// at 10e and drive the sim clock and the tool palette. That is benign here
// because CityBuilder's HUD is never focus-navigated during play -- it has no
// modal menu, and its buttons are clicked, not tabbed -- so there is no focused
// element for the reserved rows to move. TilePuzzle carries the identical
// overlap for the same reason.
//
// ---------------------------------------------------------------------------
// WHO CONSUMES WHAT. CityBuilder has no Behaviour Graphs, so nothing resolves an
// action by NAME: every consumer is C++ and goes through the readers at the
// bottom of this file. The names are still spelled once here rather than
// invented per call site, because they are what an editor rebind screen would
// show and what a future graph node would name.
//
//   CB_ToolSystem              TOOL_1..TOOL_K (the 14 selection rows)
//   CB_RoadController          PRIMARY / SECONDARY edges + TOOL_6 (service cycle)
//   CB_CityCameraComponent     PAN / ROTATE / ORBIT_* / PAN_DRAG_* / ZOOM_*
//   CB_CityManagerComponent    everything else, plus TOOL_PREV/NEXT
//
// Engine-reserved ids 0-15 are the UI nav set; this game's ids start at 16
// (uINPUT_ACTION_FIRST_GAME_ID) and stay below 126 -- the engine's own test
// actions occupy 126/127 and RegisterAction asserts on a collision.
// ============================================================================

namespace CB_Bindings
{
	// ---- Profiles (B5) -----------------------------------------------------
	// Ids are this game's own; 0xF0 / 0xF1 / 0xFF are engine-reserved.
	inline constexpr u_int8 uPROFILE_DESKTOP = 0u;
	inline constexpr u_int8 uPROFILE_GAMEPAD = 1u;

	inline constexpr const char* szPROFILE_DESKTOP_NAME = "P_DESKTOP";
	inline constexpr const char* szPROFILE_GAMEPAD_NAME = "P_GAMEPAD";

	// ---- Actions -----------------------------------------------------------
	// Contiguous from uINPUT_ACTION_FIRST_GAME_ID; CB_ACTION_COUNT is the COUNT
	// (42), not the next id.
	enum CB_ACTION : Zenith_InputActionID
	{
		CB_ACTION_PRIMARY           = uINPUT_ACTION_FIRST_GAME_ID,         // 16
		CB_ACTION_SECONDARY         = uINPUT_ACTION_FIRST_GAME_ID + 1u,    // 17
		CB_ACTION_PAN               = uINPUT_ACTION_FIRST_GAME_ID + 2u,    // 18
		CB_ACTION_ORBIT_MODIFIER    = uINPUT_ACTION_FIRST_GAME_ID + 3u,    // 19
		CB_ACTION_ORBIT_DELTA       = uINPUT_ACTION_FIRST_GAME_ID + 4u,    // 20
		CB_ACTION_ORBIT_RATE        = uINPUT_ACTION_FIRST_GAME_ID + 5u,    // 21
		CB_ACTION_PAN_DRAG_MODIFIER = uINPUT_ACTION_FIRST_GAME_ID + 6u,    // 22
		CB_ACTION_PAN_DRAG_DELTA    = uINPUT_ACTION_FIRST_GAME_ID + 7u,    // 23
		CB_ACTION_ROTATE            = uINPUT_ACTION_FIRST_GAME_ID + 8u,    // 24
		CB_ACTION_ZOOM_DELTA        = uINPUT_ACTION_FIRST_GAME_ID + 9u,    // 25
		CB_ACTION_ZOOM_RATE         = uINPUT_ACTION_FIRST_GAME_ID + 10u,   // 26

		// The 14 tool-selection rows, IN PALETTE-KEY ORDER (1-9, 0, T, B, L, K).
		// CB_ToolSystem holds the parallel CB_ETool table and a static_assert
		// that the two stay the same length -- the tool IDENTITY belongs to the
		// tool system, the KEY belongs here.
		CB_ACTION_TOOL_1            = uINPUT_ACTION_FIRST_GAME_ID + 11u,   // 27
		CB_ACTION_TOOL_2            = uINPUT_ACTION_FIRST_GAME_ID + 12u,   // 28
		CB_ACTION_TOOL_3            = uINPUT_ACTION_FIRST_GAME_ID + 13u,   // 29
		CB_ACTION_TOOL_4            = uINPUT_ACTION_FIRST_GAME_ID + 14u,   // 30
		CB_ACTION_TOOL_5            = uINPUT_ACTION_FIRST_GAME_ID + 15u,   // 31
		CB_ACTION_TOOL_6            = uINPUT_ACTION_FIRST_GAME_ID + 16u,   // 32
		CB_ACTION_TOOL_7            = uINPUT_ACTION_FIRST_GAME_ID + 17u,   // 33
		CB_ACTION_TOOL_8            = uINPUT_ACTION_FIRST_GAME_ID + 18u,   // 34
		CB_ACTION_TOOL_9            = uINPUT_ACTION_FIRST_GAME_ID + 19u,   // 35
		CB_ACTION_TOOL_0            = uINPUT_ACTION_FIRST_GAME_ID + 20u,   // 36
		CB_ACTION_TOOL_T            = uINPUT_ACTION_FIRST_GAME_ID + 21u,   // 37
		CB_ACTION_TOOL_B            = uINPUT_ACTION_FIRST_GAME_ID + 22u,   // 38
		CB_ACTION_TOOL_L            = uINPUT_ACTION_FIRST_GAME_ID + 23u,   // 39
		CB_ACTION_TOOL_K            = uINPUT_ACTION_FIRST_GAME_ID + 24u,   // 40

		CB_ACTION_TOOL_PREV         = uINPUT_ACTION_FIRST_GAME_ID + 25u,   // 41
		CB_ACTION_TOOL_NEXT         = uINPUT_ACTION_FIRST_GAME_ID + 26u,   // 42

		CB_ACTION_PAUSE             = uINPUT_ACTION_FIRST_GAME_ID + 27u,   // 43
		CB_ACTION_SPEED_DOWN        = uINPUT_ACTION_FIRST_GAME_ID + 28u,   // 44
		CB_ACTION_SPEED_UP          = uINPUT_ACTION_FIRST_GAME_ID + 29u,   // 45
		CB_ACTION_ROAD_CLASS        = uINPUT_ACTION_FIRST_GAME_ID + 30u,   // 46
		CB_ACTION_TAX_DOWN          = uINPUT_ACTION_FIRST_GAME_ID + 31u,   // 47
		CB_ACTION_TAX_UP            = uINPUT_ACTION_FIRST_GAME_ID + 32u,   // 48
		CB_ACTION_LOAN              = uINPUT_ACTION_FIRST_GAME_ID + 33u,   // 49
		CB_ACTION_QUICKSAVE         = uINPUT_ACTION_FIRST_GAME_ID + 34u,   // 50
		CB_ACTION_QUICKLOAD         = uINPUT_ACTION_FIRST_GAME_ID + 35u,   // 51
		CB_ACTION_NEW_CITY          = uINPUT_ACTION_FIRST_GAME_ID + 36u,   // 52
		CB_ACTION_IGNITE            = uINPUT_ACTION_FIRST_GAME_ID + 37u,   // 53

		// The four policy ordinances. The retired code carried a 4-entry KEYCODE
		// array indexed in lockstep with a CB_EPolicy array; it is now a 4-entry
		// ACTION array (auPOLICY_ACTIONS below) indexed exactly the same way, so
		// the call site's loop is unchanged and only the array's element type
		// moved from "device fact" to "named action".
		CB_ACTION_POLICY_1          = uINPUT_ACTION_FIRST_GAME_ID + 38u,   // 54
		CB_ACTION_POLICY_2          = uINPUT_ACTION_FIRST_GAME_ID + 39u,   // 55
		CB_ACTION_POLICY_3          = uINPUT_ACTION_FIRST_GAME_ID + 40u,   // 56
		CB_ACTION_POLICY_4          = uINPUT_ACTION_FIRST_GAME_ID + 41u,   // 57

		CB_ACTION_COUNT = 42u
	};

	static_assert(CB_ACTION_POLICY_4 < 126u,
		"Game action ids must stay below the engine's reserved test ids 126/127");

	// The 14 tool-selection actions, in the palette-key order the table above
	// lists them. CB_ToolSystem walks this and maps index -> CB_ETool.
	inline constexpr u_int32 uTOOL_SELECT_COUNT = 14u;
	inline constexpr Zenith_InputActionID auTOOL_SELECT_ACTIONS[uTOOL_SELECT_COUNT] = {
		CB_ACTION_TOOL_1, CB_ACTION_TOOL_2, CB_ACTION_TOOL_3, CB_ACTION_TOOL_4,
		CB_ACTION_TOOL_5, CB_ACTION_TOOL_6, CB_ACTION_TOOL_7, CB_ACTION_TOOL_8,
		CB_ACTION_TOOL_9, CB_ACTION_TOOL_0, CB_ACTION_TOOL_T, CB_ACTION_TOOL_B,
		CB_ACTION_TOOL_L, CB_ACTION_TOOL_K,
	};

	// The four policy rows, indexed in lockstep with the caller's CB_EPolicy
	// array (see the enum note above).
	inline constexpr u_int32 uPOLICY_COUNT = 4u;
	inline constexpr Zenith_InputActionID auPOLICY_ACTIONS[uPOLICY_COUNT] = {
		CB_ACTION_POLICY_1, CB_ACTION_POLICY_2, CB_ACTION_POLICY_3, CB_ACTION_POLICY_4,
	};

	// ---- Action NAMES ------------------------------------------------------
	// Nothing in this game resolves an action by name today (there are no
	// Behaviour Graphs here), but the names are what an editor rebind screen or
	// a future action node would use, so they are spelled once here.
	inline constexpr const char* szACTION_PRIMARY           = "Primary";
	inline constexpr const char* szACTION_SECONDARY         = "Secondary";
	inline constexpr const char* szACTION_PAN               = "Pan";
	inline constexpr const char* szACTION_ORBIT_MODIFIER    = "OrbitModifier";
	inline constexpr const char* szACTION_ORBIT_DELTA       = "OrbitDelta";
	inline constexpr const char* szACTION_ORBIT_RATE        = "OrbitRate";
	inline constexpr const char* szACTION_PAN_DRAG_MODIFIER = "PanDragModifier";
	inline constexpr const char* szACTION_PAN_DRAG_DELTA    = "PanDragDelta";
	inline constexpr const char* szACTION_ROTATE            = "Rotate";
	inline constexpr const char* szACTION_ZOOM_DELTA        = "ZoomDelta";
	inline constexpr const char* szACTION_ZOOM_RATE         = "ZoomRate";
	inline constexpr const char* szACTION_TOOL_PREV         = "ToolPrev";
	inline constexpr const char* szACTION_TOOL_NEXT         = "ToolNext";
	inline constexpr const char* szACTION_PAUSE             = "Pause";
	inline constexpr const char* szACTION_SPEED_DOWN        = "SpeedDown";
	inline constexpr const char* szACTION_SPEED_UP          = "SpeedUp";
	inline constexpr const char* szACTION_ROAD_CLASS        = "RoadClass";
	inline constexpr const char* szACTION_TAX_DOWN          = "TaxDown";
	inline constexpr const char* szACTION_TAX_UP            = "TaxUp";
	inline constexpr const char* szACTION_LOAN              = "Loan";
	inline constexpr const char* szACTION_QUICKSAVE         = "QuickSave";
	inline constexpr const char* szACTION_QUICKLOAD         = "QuickLoad";
	inline constexpr const char* szACTION_NEW_CITY          = "NewCity";
	inline constexpr const char* szACTION_IGNITE            = "Ignite";

	// The 14 tool rows + the 4 policy rows are named positionally.
	inline constexpr const char* aszTOOL_SELECT_NAMES[uTOOL_SELECT_COUNT] = {
		"Tool1", "Tool2", "Tool3", "Tool4", "Tool5", "Tool6", "Tool7",
		"Tool8", "Tool9", "Tool0", "ToolT", "ToolB", "ToolL", "ToolK",
	};
	inline constexpr const char* aszPOLICY_NAMES[uPOLICY_COUNT] = {
		"Policy1", "Policy2", "Policy3", "Policy4",
	};

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

		// PRIMARY / SECONDARY -- the two ground-tool buttons, claim-filtered at
		// 10e. No pad rows by design (see the header note): every consumer also
		// needs a cursor POSITION. Consumers read BOTH the held level (drag-paint,
		// terraform brush) and the press edge (place a node, a stop, a service),
		// which is why they are BUTTON rather than anything narrower.
		xActions.RegisterAction(CB_ACTION_PRIMARY, szACTION_PRIMARY, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(CB_ACTION_PRIMARY,
			Zenith_InputBinding::MouseButton(ZENITH_MOUSE_BUTTON_LEFT));

		xActions.RegisterAction(CB_ACTION_SECONDARY, szACTION_SECONDARY, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(CB_ACTION_SECONDARY,
			Zenith_InputBinding::MouseButton(ZENITH_MOUSE_BUTTON_RIGHT));

		// PAN -- the camera slide. WASD only (no arrows: the arrows are the
		// engine-reserved UI-nav set's keyboard column, and this game's HUD has
		// no other keyboard verb to give them). +y is FORWARD, so W is the
		// forward key; the stick's y scale is -1 because GLFW reports a forward
		// push as -1. The consumer scales the whole vector by distance * speed *
		// dt, exactly as the retired four IsKeyDown polls did.
		xActions.RegisterAction(CB_ACTION_PAN, szACTION_PAN, INPUT_ACTION_AXIS2D);
		xActions.RegisterBinding(CB_ACTION_PAN,
			Zenith_InputBinding::KeyAxis2D(ZENITH_KEY_W, ZENITH_KEY_S, ZENITH_KEY_A, ZENITH_KEY_D));
		xActions.RegisterBinding(CB_ACTION_PAN,
			Zenith_InputBinding::GamepadStick(ZENITH_GAMEPAD_AXIS_LEFT_X, 1.0f, -1.0f));

		// ORBIT -- right-drag turns the camera. The MODIFIER gates the DELTA so a
		// bare mouse move never rotates the view (free-cursor game: the cursor
		// picks the ground every frame).
		xActions.RegisterAction(CB_ACTION_ORBIT_MODIFIER, szACTION_ORBIT_MODIFIER, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(CB_ACTION_ORBIT_MODIFIER,
			Zenith_InputBinding::MouseButton(ZENITH_MOUSE_BUTTON_RIGHT));

		xActions.RegisterAction(CB_ACTION_ORBIT_DELTA, szACTION_ORBIT_DELTA, INPUT_ACTION_AXIS2D);
		xActions.RegisterBinding(CB_ACTION_ORBIT_DELTA,
			Zenith_InputBinding::MouseDelta(1.0f, 1.0f));

		// ORBIT_RATE -- the pad's right stick as a RATE the consumer multiplies by
		// dt. Straight passthrough scales, which is what reproduces the mouse's
		// sign convention on a stick: stick-RIGHT gives +x like mouse-right (same
		// yaw direction), and stick-DOWN gives +y (GLFW reports stick-up as -1)
		// like dragging the mouse down, which is the direction that tilts the view
		// toward top-down.
		xActions.RegisterAction(CB_ACTION_ORBIT_RATE, szACTION_ORBIT_RATE, INPUT_ACTION_AXIS2D);
		xActions.RegisterBinding(CB_ACTION_ORBIT_RATE,
			Zenith_InputBinding::GamepadStick(ZENITH_GAMEPAD_AXIS_RIGHT_X, 1.0f, 1.0f));

		// PAN_DRAG -- middle-drag slides the camera. Same MOUSE_DELTA source as
		// ORBIT_DELTA, different gesture, own modifier (see the header note).
		xActions.RegisterAction(CB_ACTION_PAN_DRAG_MODIFIER, szACTION_PAN_DRAG_MODIFIER, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(CB_ACTION_PAN_DRAG_MODIFIER,
			Zenith_InputBinding::MouseButton(ZENITH_MOUSE_BUTTON_MIDDLE));

		xActions.RegisterAction(CB_ACTION_PAN_DRAG_DELTA, szACTION_PAN_DRAG_DELTA, INPUT_ACTION_AXIS2D);
		xActions.RegisterBinding(CB_ACTION_PAN_DRAG_DELTA,
			Zenith_InputBinding::MouseDelta(1.0f, 1.0f));

		// ROTATE -- an AXIS1D, because the orbit yaw integrates it every frame
		// (yaw += value * speed * dt). Q is the negative half and E the positive
		// one, exactly as the retired two IsKeyDown polls summed to. No pad row:
		// the pad rotates through ORBIT_RATE's right stick, which is the same
		// quantity on a better control.
		xActions.RegisterAction(CB_ACTION_ROTATE, szACTION_ROTATE, INPUT_ACTION_AXIS1D);
		xActions.RegisterBinding(CB_ACTION_ROTATE,
			Zenith_InputBinding::KeyAxis1D(ZENITH_KEY_Q, ZENITH_KEY_E));

		// ZOOM_DELTA -- the wheel's DISPLACEMENT this frame, scale 1 so the value
		// the camera reads is bit-for-bit the one GetMouseWheelDelta reported
		// before. Zoom SPEED stays game-side: a tuning value, not a device fact.
		xActions.RegisterAction(CB_ACTION_ZOOM_DELTA, szACTION_ZOOM_DELTA, INPUT_ACTION_AXIS1D);
		xActions.RegisterBinding(CB_ACTION_ZOOM_DELTA,
			Zenith_InputBinding::MouseWheel(1.0f));

		// ZOOM_RATE -- RT minus LT as a RATE the consumer multiplies by dt.
		// Positive (RT) zooms IN, matching a wheel pushed forward, because
		// CB_CameraController::Zoom takes a positive delta as "move closer".
		xActions.RegisterAction(CB_ACTION_ZOOM_RATE, szACTION_ZOOM_RATE, INPUT_ACTION_AXIS1D);
		xActions.RegisterBinding(CB_ACTION_ZOOM_RATE,
			Zenith_InputBinding::GamepadTriggerPair(1.0f));

		// The 14 tool-selection rows. Keyboard only: on a pad the palette is
		// walked with TOOL_PREV / TOOL_NEXT below, which is what a 14-entry
		// palette wants from a controller anyway.
		{
			const int32_t aiToolKeys[uTOOL_SELECT_COUNT] = {
				ZENITH_KEY_1, ZENITH_KEY_2, ZENITH_KEY_3, ZENITH_KEY_4,
				ZENITH_KEY_5, ZENITH_KEY_6, ZENITH_KEY_7, ZENITH_KEY_8,
				ZENITH_KEY_9, ZENITH_KEY_0, ZENITH_KEY_T, ZENITH_KEY_B,
				ZENITH_KEY_L, ZENITH_KEY_K,
			};
			for (u_int32 u = 0; u < uTOOL_SELECT_COUNT; ++u)
			{
				xActions.RegisterAction(auTOOL_SELECT_ACTIONS[u], aszTOOL_SELECT_NAMES[u], INPUT_ACTION_BUTTON);
				xActions.RegisterBinding(auTOOL_SELECT_ACTIONS[u],
					Zenith_InputBinding::KeySet(aiToolKeys[u]));
			}
		}

		// TOOL_PREV / TOOL_NEXT -- the pad's way onto the palette. They step the
		// game's OWN toolbar order (CB_CityManagerComponent::ToolDescs) through
		// SelectUITool, so the pad reaches every entry the mouse can click --
		// including the eight service sub-types, which the keyboard can only
		// reach by re-pressing 6. Keyboard-less by design: the number row already
		// selects directly, so a keyboard cycle would be a second way to do a
		// thing that is already one keystroke.
		xActions.RegisterAction(CB_ACTION_TOOL_PREV, szACTION_TOOL_PREV, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(CB_ACTION_TOOL_PREV,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_DPAD_LEFT));

		xActions.RegisterAction(CB_ACTION_TOOL_NEXT, szACTION_TOOL_NEXT, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(CB_ACTION_TOOL_NEXT,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_DPAD_RIGHT));

		// PAUSE + the two speed steps -- the sim clock.
		xActions.RegisterAction(CB_ACTION_PAUSE, szACTION_PAUSE, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(CB_ACTION_PAUSE, Zenith_InputBinding::KeySet(ZENITH_KEY_P));
		xActions.RegisterBinding(CB_ACTION_PAUSE,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_START));

		xActions.RegisterAction(CB_ACTION_SPEED_DOWN, szACTION_SPEED_DOWN, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(CB_ACTION_SPEED_DOWN, Zenith_InputBinding::KeySet(ZENITH_KEY_COMMA));
		xActions.RegisterBinding(CB_ACTION_SPEED_DOWN,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_DPAD_DOWN));

		xActions.RegisterAction(CB_ACTION_SPEED_UP, szACTION_SPEED_UP, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(CB_ACTION_SPEED_UP, Zenith_InputBinding::KeySet(ZENITH_KEY_PERIOD));
		xActions.RegisterBinding(CB_ACTION_SPEED_UP,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_DPAD_UP));

		// ROAD_CLASS -- cycles small -> medium -> large.
		xActions.RegisterAction(CB_ACTION_ROAD_CLASS, szACTION_ROAD_CLASS, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(CB_ACTION_ROAD_CLASS, Zenith_InputBinding::KeySet(ZENITH_KEY_R));
		xActions.RegisterBinding(CB_ACTION_ROAD_CLASS,
			Zenith_InputBinding::GamepadButton(ZENITH_GAMEPAD_BUTTON_Y));

		// The economy verbs. Keyboard only per the C2 table -- they are budget
		// screen material rather than moment-to-moment play, and the pad's four
		// face buttons are better spent elsewhere than on a tax nudge.
		xActions.RegisterAction(CB_ACTION_TAX_DOWN, szACTION_TAX_DOWN, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(CB_ACTION_TAX_DOWN, Zenith_InputBinding::KeySet(ZENITH_KEY_MINUS));

		xActions.RegisterAction(CB_ACTION_TAX_UP, szACTION_TAX_UP, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(CB_ACTION_TAX_UP, Zenith_InputBinding::KeySet(ZENITH_KEY_EQUAL));

		xActions.RegisterAction(CB_ACTION_LOAN, szACTION_LOAN, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(CB_ACTION_LOAN, Zenith_InputBinding::KeySet(ZENITH_KEY_G));

		// Session verbs.
		xActions.RegisterAction(CB_ACTION_QUICKSAVE, szACTION_QUICKSAVE, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(CB_ACTION_QUICKSAVE, Zenith_InputBinding::KeySet(ZENITH_KEY_F5));

		xActions.RegisterAction(CB_ACTION_QUICKLOAD, szACTION_QUICKLOAD, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(CB_ACTION_QUICKLOAD, Zenith_InputBinding::KeySet(ZENITH_KEY_F9));

		xActions.RegisterAction(CB_ACTION_NEW_CITY, szACTION_NEW_CITY, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(CB_ACTION_NEW_CITY, Zenith_InputBinding::KeySet(ZENITH_KEY_F8));

		// IGNITE -- the disaster drill: start a fire under the cursor. A POINTER
		// gesture like the ground tools (it picks the ground point), so it is
		// keyboard-only for the same reason they are pad-less.
		xActions.RegisterAction(CB_ACTION_IGNITE, szACTION_IGNITE, INPUT_ACTION_BUTTON);
		xActions.RegisterBinding(CB_ACTION_IGNITE, Zenith_InputBinding::KeySet(ZENITH_KEY_F));

		// The four policy ordinances, F1-F4.
		{
			const int32_t aiPolicyKeys[uPOLICY_COUNT] = {
				ZENITH_KEY_F1, ZENITH_KEY_F2, ZENITH_KEY_F3, ZENITH_KEY_F4,
			};
			for (u_int32 u = 0; u < uPOLICY_COUNT; ++u)
			{
				xActions.RegisterAction(auPOLICY_ACTIONS[u], aszPOLICY_NAMES[u], INPUT_ACTION_BUTTON);
				xActions.RegisterBinding(auPOLICY_ACTIONS[u],
					Zenith_InputBinding::KeySet(aiPolicyKeys[u]));
			}
		}
	}

	// ---- Readers (NON-CONSUMING reads of CLOSED state) ---------------------
	//
	// * THESE ARE FRAME-CONTRACT READS, NOT POLLS. The action layer closes at
	// step 10e and game logic runs at step 11, so a consumer inside an ECS
	// OnUpdate sees this frame's edges. Code that runs OUTSIDE the main loop
	// must drive a LOCAL Zenith_InputActions instead -- these would answer with
	// whatever the last real frame left behind.
	//
	// They are also NON-CONSUMING: two readers in the same frame both see an
	// edge, which is exactly what the retired raw polls did (PRIMARY is read by
	// the road controller, the terraform brush, the district brush and the
	// transit tool in one frame, and only the active TOOL makes one of them act).

	// -- the two pointer buttons ---------------------------------------------
	inline bool IsPrimaryHeld()      { return g_xEngine.Actions().IsHeld(CB_ACTION_PRIMARY); }
	inline bool IsSecondaryHeld()    { return g_xEngine.Actions().IsHeld(CB_ACTION_SECONDARY); }
	inline bool WasPrimaryPressed()  { return g_xEngine.Actions().WasPressedThisFrame(CB_ACTION_PRIMARY); }
	inline bool WasSecondaryPressed(){ return g_xEngine.Actions().WasPressedThisFrame(CB_ACTION_SECONDARY); }

	// -- camera ---------------------------------------------------------------
	// +y is FORWARD, x is RIGHT, diagonals UNNORMALISED, opposite keys cancel.
	inline Zenith_Maths::Vector2 ReadPan()
	{
		Zenith_Maths::Vector2 xPan(0.0f);
		g_xEngine.Actions().GetAxis2D(CB_ACTION_PAN, xPan);
		return xPan;
	}

	// Mouse displacement this frame, in raw surface pixels -- a DISPLACEMENT, so
	// the caller must NOT multiply it by dt.
	inline Zenith_Maths::Vector2 ReadOrbitDelta()
	{
		Zenith_Maths::Vector2 xDelta(0.0f);
		g_xEngine.Actions().GetAxis2D(CB_ACTION_ORBIT_DELTA, xDelta);
		return xDelta;
	}
	inline Zenith_Maths::Vector2 ReadPanDragDelta()
	{
		Zenith_Maths::Vector2 xDelta(0.0f);
		g_xEngine.Actions().GetAxis2D(CB_ACTION_PAN_DRAG_DELTA, xDelta);
		return xDelta;
	}

	// Pad right-stick deflection: a RATE, so the caller multiplies by dt.
	inline Zenith_Maths::Vector2 ReadOrbitRate()
	{
		Zenith_Maths::Vector2 xRate(0.0f);
		g_xEngine.Actions().GetAxis2D(CB_ACTION_ORBIT_RATE, xRate);
		return xRate;
	}

	inline bool IsOrbitModifierHeld()   { return g_xEngine.Actions().IsHeld(CB_ACTION_ORBIT_MODIFIER); }
	inline bool IsPanDragModifierHeld() { return g_xEngine.Actions().IsHeld(CB_ACTION_PAN_DRAG_MODIFIER); }

	// -1 rotates the orbit yaw one way, +1 the other; the caller scales by its
	// own rotate speed and by dt.
	inline float ReadRotate()    { return g_xEngine.Actions().GetAxis1D(CB_ACTION_ROTATE); }
	// Wheel notches this frame -- a DISPLACEMENT, so NOT multiplied by dt.
	inline float ReadZoomDelta() { return g_xEngine.Actions().GetAxis1D(CB_ACTION_ZOOM_DELTA); }
	// Trigger deflection -- a RATE, so the caller multiplies by dt.
	inline float ReadZoomRate()  { return g_xEngine.Actions().GetAxis1D(CB_ACTION_ZOOM_RATE); }

	// -- tools ----------------------------------------------------------------
	// uIndex walks auTOOL_SELECT_ACTIONS; CB_ToolSystem owns the parallel
	// CB_ETool table.
	inline bool WasToolSelectPressed(u_int32 uIndex)
	{
		return (uIndex < uTOOL_SELECT_COUNT)
			&& g_xEngine.Actions().WasPressedThisFrame(auTOOL_SELECT_ACTIONS[uIndex]);
	}
	// The services key re-press that cycles the placed service sub-type. It is
	// the SAME row that selects the services tool -- the game's rule is "press 6
	// again while it is already active", and the row cannot know that, so the
	// re-press test stays in CB_RoadController.
	inline bool WasServiceCyclePressed() { return g_xEngine.Actions().WasPressedThisFrame(CB_ACTION_TOOL_6); }

	inline bool WasToolPrevPressed() { return g_xEngine.Actions().WasPressedThisFrame(CB_ACTION_TOOL_PREV); }
	inline bool WasToolNextPressed() { return g_xEngine.Actions().WasPressedThisFrame(CB_ACTION_TOOL_NEXT); }

	// -- sim clock, economy, session -----------------------------------------
	inline bool WasPausePressed()     { return g_xEngine.Actions().WasPressedThisFrame(CB_ACTION_PAUSE); }
	inline bool WasSpeedDownPressed() { return g_xEngine.Actions().WasPressedThisFrame(CB_ACTION_SPEED_DOWN); }
	inline bool WasSpeedUpPressed()   { return g_xEngine.Actions().WasPressedThisFrame(CB_ACTION_SPEED_UP); }
	inline bool WasRoadClassPressed() { return g_xEngine.Actions().WasPressedThisFrame(CB_ACTION_ROAD_CLASS); }
	inline bool WasTaxDownPressed()   { return g_xEngine.Actions().WasPressedThisFrame(CB_ACTION_TAX_DOWN); }
	inline bool WasTaxUpPressed()     { return g_xEngine.Actions().WasPressedThisFrame(CB_ACTION_TAX_UP); }
	inline bool WasLoanPressed()      { return g_xEngine.Actions().WasPressedThisFrame(CB_ACTION_LOAN); }
	inline bool WasQuickSavePressed() { return g_xEngine.Actions().WasPressedThisFrame(CB_ACTION_QUICKSAVE); }
	inline bool WasQuickLoadPressed() { return g_xEngine.Actions().WasPressedThisFrame(CB_ACTION_QUICKLOAD); }
	inline bool WasNewCityPressed()   { return g_xEngine.Actions().WasPressedThisFrame(CB_ACTION_NEW_CITY); }
	inline bool WasIgnitePressed()    { return g_xEngine.Actions().WasPressedThisFrame(CB_ACTION_IGNITE); }

	// uIndex walks auPOLICY_ACTIONS, in lockstep with the caller's CB_EPolicy
	// array.
	inline bool WasPolicyPressed(u_int32 uIndex)
	{
		return (uIndex < uPOLICY_COUNT)
			&& g_xEngine.Actions().WasPressedThisFrame(auPOLICY_ACTIONS[uIndex]);
	}

	// ---- The cursor (POSITION, which no action can carry) ------------------
	//
	// CB_ToolSystem::PickGroundPoint needs to know WHERE the cursor is so it can
	// unproject it and ray-march the terrain; no binding row carries that.
	// PRIMARY says a click happened, not where. So the picker reads the position
	// here, and this is the ONLY reason this header names Zenith_Input at all.
	//
	// * IT IS THE DEVICE POSITION, CLAIM-GUARDED BY HAND. The device position is
	// what the retired g_xEngine.Input().GetMousePosition call reported (raw
	// surface pixels, the space the unproject maths already expects), and unlike
	// the pointer table it exists on a frame with no button down -- which matters
	// here, because the road tool ghosts a preview under a HOVERING cursor, and a
	// pointer-table read would make that a silent no-pick (the mouse only
	// allocates a pointer slot on a PRESS edge).
	//
	// The claim rule therefore has to be applied HERE rather than inherited: the
	// action layer applies it at 10e to its own rows, and a direct device read
	// bypasses that. A CLAIMED primary pointer means a UI widget owns the
	// gesture, so the pick REFUSES -- a drag that started on a HUD tool button
	// can never also sculpt the ground under it. No live pointer at all means no
	// claim, so hover-time picking is unaffected.
	inline bool ReadCursorPosition(Zenith_Maths::Vector2_64& xOutPosition)
	{
		const Zenith_Pointers& xPointers = g_xEngine.Pointers();
		const Zenith_Pointer* pxPrimary = xPointers.Resolve(xPointers.GetPrimaryPointer());
		if (pxPrimary != nullptr && pxPrimary->IsClaimed())
		{
			return false;
		}
		g_xEngine.Input().GetMousePosition(xOutPosition);
		return true;
	}
}
