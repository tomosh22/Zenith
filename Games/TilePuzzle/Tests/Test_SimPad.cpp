#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

/**
 * TilePuzzle_SimPad_Test -- the GAMEPAD column of TilePuzzle's C2 binding table
 * (TilePuzzle_Bindings.h), end to end on the real input path. No key, no mouse,
 * no C++ shortcut: a simulated pad drives every pad-bound row through the device
 * layer, the auto profile switch and the action layer's 10e close, and the test
 * reads the result back through the game's OWN readers.
 *
 * WHY IT IS WORTH A WHOLE TEST.
 *
 *   - THE AUTO PROFILE SWITCH is what makes the pad column work at all. The game
 *     boots into P_DESKTOP (ResolveBootDefaultProfile picks whichever profile
 *     owns KEYBOARD on Windows), and the first pad PRESS EDGE is what makes
 *     P_GAMEPAD active -- after which only the pad answers. Every assertion below
 *     is meaningless if that never happens, so it is checked first and reported
 *     separately.
 *   - NOTHING ELSE IN THIS GAME TOUCHES A PAD. The eight pad rows have no other
 *     coverage of any kind, and a row that silently stopped resolving would look
 *     exactly like a game that simply has no pad support.
 *   - THE ROWS MUST NOT ALIAS. Each press asserts that its OWN action edged and
 *     that none of the other seven did -- the property that a table with four
 *     cursor directions on four d-pad faces most easily loses.
 *
 * ★ WHAT "END TO END" MEANS HERE, AND WHERE IT STOPS. Six of these eight actions
 * (the four CURSOR_* rows, SELECT, RESET_LEVEL, NEXT_LEVEL) have NO production
 * consumer in this game today: the board is driven by dragging a POINTER, and the
 * C2 table registers the cursor/menu verbs without this migration inventing
 * gameplay to consume them. So "end to end" is device -> profile -> binding row ->
 * action edge -> the reader a consumer would call, which is the whole of the path
 * this work package owns. ESCAPE is the one row with live consumers
 * (TilePuzzle_GameComponent and Pinball_GameComponent both read it every frame),
 * and it is deliberately pressed while the MAIN MENU is up, where that consumer
 * is state-gated off -- pressing it inside a level would unload the level and
 * prove nothing about the binding.
 *
 * ★ THE PRESS ORDER IS NOT ARBITRARY. The engine's reserved UI-nav actions (ids
 * 0-15) carry the SAME d-pad faces and the SAME A button, by design -- so a pad
 * press also moves UI focus, and A activates whatever the menu had focused. A is
 * therefore LAST, so a menu activation cannot disturb any assertion before it.
 *
 * requiresGraphics is FALSE: nothing here reads a pixel or needs a scene, so CI
 * sees it.
 */

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputActions.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"

#include "TilePuzzle/TilePuzzle_Bindings.h"

namespace
{
	// One row of the C2 table's GAMEPAD column. The pad codes are spelled RAW on
	// purpose: a test that re-read them out of the binding table would agree with
	// itself no matter what the table said.
	struct PadRow
	{
		int32_t              m_iPadButton;
		Zenith_InputActionID m_uAction;
		const char*          m_szActionName;
		const char*          m_szPadName;
	};

	const PadRow g_axPadRows[] = {
		// Start first: it is the only pad face the engine's reserved UI set does
		// NOT also claim, so the profile switch is observed on a press that
		// cannot move menu focus.
		{ ZENITH_GAMEPAD_BUTTON_START,      TilePuzzle_Bindings::TILEPUZZLE_ACTION_ESCAPE,
		  TilePuzzle_Bindings::szACTION_ESCAPE,       "Start" },
		{ ZENITH_GAMEPAD_BUTTON_DPAD_UP,    TilePuzzle_Bindings::TILEPUZZLE_ACTION_CURSOR_UP,
		  TilePuzzle_Bindings::szACTION_CURSOR_UP,    "dpad Up" },
		{ ZENITH_GAMEPAD_BUTTON_DPAD_DOWN,  TilePuzzle_Bindings::TILEPUZZLE_ACTION_CURSOR_DOWN,
		  TilePuzzle_Bindings::szACTION_CURSOR_DOWN,  "dpad Down" },
		{ ZENITH_GAMEPAD_BUTTON_DPAD_LEFT,  TilePuzzle_Bindings::TILEPUZZLE_ACTION_CURSOR_LEFT,
		  TilePuzzle_Bindings::szACTION_CURSOR_LEFT,  "dpad Left" },
		{ ZENITH_GAMEPAD_BUTTON_DPAD_RIGHT, TilePuzzle_Bindings::TILEPUZZLE_ACTION_CURSOR_RIGHT,
		  TilePuzzle_Bindings::szACTION_CURSOR_RIGHT, "dpad Right" },
		{ ZENITH_GAMEPAD_BUTTON_X,          TilePuzzle_Bindings::TILEPUZZLE_ACTION_RESET_LEVEL,
		  TilePuzzle_Bindings::szACTION_RESET_LEVEL,  "X" },
		{ ZENITH_GAMEPAD_BUTTON_Y,          TilePuzzle_Bindings::TILEPUZZLE_ACTION_NEXT_LEVEL,
		  TilePuzzle_Bindings::szACTION_NEXT_LEVEL,   "Y" },
		// A last -- see the header note about UI_CONFIRM sharing this face.
		{ ZENITH_GAMEPAD_BUTTON_A,          TilePuzzle_Bindings::TILEPUZZLE_ACTION_SELECT,
		  TilePuzzle_Bindings::szACTION_SELECT,       "A" },
	};
	constexpr int iPAD_ROW_COUNT = static_cast<int>(sizeof(g_axPadRows) / sizeof(g_axPadRows[0]));

	// The readers a real consumer would call, in the same order as the table.
	// Going through these rather than straight at Actions() is deliberate: it is
	// what makes a reader wired to the wrong id a RED rather than a silent
	// always-false in whatever eventually consumes it.
	bool ReadRowPressed(int iRow)
	{
		switch (iRow)
		{
		case 0: return TilePuzzle_Bindings::WasEscapePressed();
		case 1: return TilePuzzle_Bindings::WasCursorUpPressed();
		case 2: return TilePuzzle_Bindings::WasCursorDownPressed();
		case 3: return TilePuzzle_Bindings::WasCursorLeftPressed();
		case 4: return TilePuzzle_Bindings::WasCursorRightPressed();
		case 5: return TilePuzzle_Bindings::WasResetLevelPressed();
		case 6: return TilePuzzle_Bindings::WasNextLevelPressed();
		case 7: return TilePuzzle_Bindings::WasSelectPressed();
		default: return false;
		}
	}

	int    g_iPadRow           = 0;
	bool   g_bPadPressPending  = false;
	bool   g_bPadSawFirstStep  = false;
	u_int8 g_uPadBootProfile   = Zenith_InputActions::uPROFILE_AUTO;
	u_int8 g_uPadProfileAfterFirstPress = Zenith_InputActions::uPROFILE_AUTO;
	bool   g_abPadRowEdgeSeen[iPAD_ROW_COUNT]  = {};
	bool   g_abPadRowCrossTalk[iPAD_ROW_COUNT] = {};
	bool   g_bPadDone          = false;
}

static void Setup_TilePuzzleSimPad()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);

	g_iPadRow = 0;
	g_bPadPressPending = false;
	g_bPadSawFirstStep = false;
	g_uPadBootProfile = Zenith_InputActions::uPROFILE_AUTO;
	g_uPadProfileAfterFirstPress = Zenith_InputActions::uPROFILE_AUTO;
	for (int i = 0; i < iPAD_ROW_COUNT; ++i)
	{
		g_abPadRowEdgeSeen[i] = false;
		g_abPadRowCrossTalk[i] = false;
	}
	g_bPadDone = false;

	// The harness wiped every simulated device before Setup ran, so the pad has
	// to announce itself again: activity detection skips a disconnected pad, and
	// without the profile switch not one row below resolves.
	Zenith_InputSimulator::SimulateGamepadConnected(true, 0);
}

static bool Step_TilePuzzleSimPad(int /*iFrame*/)
{
	if (g_bPadDone)
	{
		return false;
	}

	if (!g_bPadSawFirstStep)
	{
		// Before any injection: whatever profile the game booted into. On Windows
		// that is P_DESKTOP; the point of recording it is that "switched to
		// P_GAMEPAD" below is a CHANGE rather than a coincidence.
		g_bPadSawFirstStep = true;
		g_uPadBootProfile = g_xEngine.Actions().GetActiveProfile();
	}

	if (g_bPadPressPending)
	{
		// ★ C1b -- this reads the edge raised by the PREVIOUS Step's injection.
		// A Step runs at PumpAutomatedTest, before this frame's step 7/8, so what
		// it sees on the action layer is the state closed at 10e LAST frame.
		// Reading it in the pressing Step would always be false.
		if (ReadRowPressed(g_iPadRow))
		{
			g_abPadRowEdgeSeen[g_iPadRow] = true;
		}
		for (int i = 0; i < iPAD_ROW_COUNT; ++i)
		{
			if (i != g_iPadRow && ReadRowPressed(i))
			{
				g_abPadRowCrossTalk[g_iPadRow] = true;
			}
		}
		if (g_iPadRow == 0)
		{
			g_uPadProfileAfterFirstPress = g_xEngine.Actions().GetActiveProfile();
		}

		Zenith_InputSimulator::SimulateGamepadButtonUp(g_axPadRows[g_iPadRow].m_iPadButton);
		g_bPadPressPending = false;

		if (++g_iPadRow >= iPAD_ROW_COUNT)
		{
			g_bPadDone = true;
			return false;
		}
		return true;
	}

	Zenith_InputSimulator::SimulateGamepadButtonDown(g_axPadRows[g_iPadRow].m_iPadButton);
	g_bPadPressPending = true;
	return true;
}

static bool Verify_TilePuzzleSimPad()
{
	Zenith_InputSimulator::ClearFixedDt();

	if (!g_bPadDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[TilePuzzle_SimPad] never completed -- stopped on row %d (%s)",
			g_iPadRow, g_iPadRow < iPAD_ROW_COUNT ? g_axPadRows[g_iPadRow].m_szPadName : "?");
		return false;
	}

	if (g_uPadProfileAfterFirstPress != TilePuzzle_Bindings::uPROFILE_GAMEPAD)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[TilePuzzle_SimPad] the first pad press did not switch the active profile to P_GAMEPAD "
			"(booted %u, now %u) -- pad rows resolve only while P_GAMEPAD is active, so nothing "
			"below would mean anything",
			static_cast<u_int32>(g_uPadBootProfile),
			static_cast<u_int32>(g_uPadProfileAfterFirstPress));
		return false;
	}

	bool bAllRows = true;
	for (int i = 0; i < iPAD_ROW_COUNT; ++i)
	{
		if (!g_abPadRowEdgeSeen[i])
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[TilePuzzle_SimPad] pad %s raised no \"%s\" press edge -- that row of the C2 table's "
				"GAMEPAD column, or its reader, regressed",
				g_axPadRows[i].m_szPadName, g_axPadRows[i].m_szActionName);
			bAllRows = false;
		}
		if (g_abPadRowCrossTalk[i])
		{
			// Distinct from the check above on purpose: the row firing AND another
			// row firing with it means two actions share a pad face, not that a
			// binding went missing.
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[TilePuzzle_SimPad] pad %s edged \"%s\" AND at least one other action in the same "
				"frame -- two rows of the pad column are aliased",
				g_axPadRows[i].m_szPadName, g_axPadRows[i].m_szActionName);
			bAllRows = false;
		}
	}
	if (!bAllRows)
	{
		return false;
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST,
		"[TilePuzzle_SimPad] the pad took the profile (%u -> P_GAMEPAD) and drove all %d rows of the "
		"GAMEPAD column -- Escape, the four CURSOR_* directions, ResetLevel, NextLevel and Select -- "
		"each with no cross-talk",
		static_cast<u_int32>(g_uPadBootProfile), iPAD_ROW_COUNT);
	return true;
}

static const Zenith_AutomatedTest g_xTilePuzzleSimPadTest = {
	"TilePuzzle_SimPad_Test",
	&Setup_TilePuzzleSimPad,
	&Step_TilePuzzleSimPad,
	&Verify_TilePuzzleSimPad,
	/*maxFrames*/ 600,
	/*bRequiresGraphics*/ false,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xTilePuzzleSimPadTest);

#endif // ZENITH_INPUT_SIMULATOR
