#include "Zenith.h"

// ============================================================================
// ZM_Tests_StarterScreen (S8) -- unit tests for ZM_UI_StarterChoice, the by-value
// non-ECS presenter ZM_UI_MenuStack raises for the starter pick.
//
// WHAT THIS FILE PINS:
//   * The authored cell-name contract (CellElementName <-> CellIndexFromElementName
//     <-> ChoiceFromElementName), TOTAL over null, foreign, PREFIX and out-of-range
//     inputs -- a drift here silently grants the WRONG starter, or none at all.
//   * The cell COUNT derived from the shipped ZM_GetStarterChoiceCount(), so a
//     fourth starter can never leave a cell unrendered.
//   * FormatCellLabel writing into a caller buffer WITHOUT overflow and always
//     null-terminated, naming the chosen species -- and yielding an EMPTY label for
//     an unregistered choice rather than reaching a species table with a sentinel.
//   * ZM-D-188: CANCEL IS IGNORED on this screen (CancelIsIgnored), and every cell
//     stays shown + focusable (CellIsAlwaysShown).
//   * The COMPOSITION: cell name -> choice -> the SHIPPED ZM_ApplyStarterChoice
//     grants exactly the species that cell shows.
//
// ★ WHAT THIS FILE CANNOT SEE, AND WHERE THAT HOLE IS PLUGGED. Every unit here is
// PURE -- no scene, no canvas, no graphics -- so NOT ONE of them can observe the
// AUTHORED screen. Author only two of the three cells, mis-type one cell's name in
// its AddStep_CreateUIButton call so FindElement returns nullptr every frame, or
// author the cells and forget the panel, and every assertion below stays green while
// FrontEnd.zscen ships a broken screen. The gate that CAN see that is the
// committed-bytes unit in Tests/ZM_Tests_CommittedSceneBytes.cpp
// (FrontEndCarriesEveryStarterScreenElementName), which needles the shipped scene
// file for these very constants and runs on every Null CI boot. The two halves are
// not substitutes for each other; neither is optional.
//
// PURITY. No disk, no ECS, no scene: the presenter's only instance state is a cell
// mirror driven by the PUBLIC SelectCell, so even the Reset unit needs no canvas.
// Category ZM_Starter (the suite the shipped starter-table units already use).
// ============================================================================

#include <cstring>

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"     // near-miss ROOT names + the screen id
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"     // ZM_GetSpeciesName + the literal species ids
#include "Zenithmon/Source/Party/ZM_GameState.h"      // ZM_MakeNewGameState (the composition fixture)
#include "Zenithmon/Source/Party/ZM_StarterChoice.h"  // the SHIPPED data layer under test-by-composition
#include "Zenithmon/Source/UI/ZM_UI_SaveSlots.h"      // a sibling screen's row name (a foreign-name fixture)
#include "Zenithmon/Source/UI/ZM_UI_StarterChoice.h"

namespace
{
	// The three cells, spelled LONGHAND: cell index -> the choice it must show -> the
	// species that choice must grant. Recomputing any of these from CellChoice /
	// ZM_ResolvePlayerStarterSpecies would move consistently under any bug -- this table
	// is an INDEPENDENT statement of GameDesignDocument.md section 4 (F01/F02/F03).
	struct ZM_ExpectedCell
	{
		u_int             m_uCell;
		ZM_STARTER_CHOICE m_eChoice;
		ZM_SPECIES_ID     m_eSpecies;
	};

	const ZM_ExpectedCell axEXPECTED_CELLS[] =
	{
		{ 0u, ZM_STARTER_CHOICE_FERNFAWN, ZM_SPECIES_FERNFAWN },
		{ 1u, ZM_STARTER_CHOICE_KINDLET,  ZM_SPECIES_KINDLET  },
		{ 2u, ZM_STARTER_CHOICE_FINLET,   ZM_SPECIES_FINLET   },
	};
	constexpr u_int uEXPECTED_CELL_COUNT =
		(u_int)(sizeof(axEXPECTED_CELLS) / sizeof(axEXPECTED_CELLS[0]));

	// Names that must NEVER resolve to a cell: the screen's own non-cell elements, the
	// sibling screens' controls, and the near misses a prefix match would swallow.
	const char* const aszFOREIGN_NAMES[] =
	{
		ZM_UI_StarterChoice::szPANEL_NAME,
		ZM_UI_StarterChoice::szHEADER_NAME,
		ZM_UI_MenuStack::szROOT_PARTY_NAME,
		ZM_UI_MenuStack::szROOT_EXIT_NAME,
		ZM_UI_SaveSlots::szCANCEL_NAME,
		"",
		"Menu_StarterCell",        // a PREFIX of every cell name
		"Menu_StarterCell0Extra",  // cell 0 with a suffix -- a prefix match would take it
		"Menu_StarterCell3",       // one past the last cell
		"menu_startercell0",       // case differs; strcmp is exact
		"Menu_StarterCel0",
	};
	constexpr u_int uFOREIGN_NAME_COUNT =
		(u_int)(sizeof(aszFOREIGN_NAMES) / sizeof(aszFOREIGN_NAMES[0]));
}

// ============================================================================
// 1-2. The cell set: exactly the shipped starters, in table order, one per cell.
// ============================================================================

ZENITH_TEST(ZM_Starter, StarterScreen_CellsAreTheThreeStartersInTableOrder)
{
	// The picker shows the trio in the order the table declares them (ZM-D-188 makes it
	// a VERTICAL column, so "table order" is literally top-to-bottom on screen). The
	// expected values are the longhand table above, never CellChoice's own answer.
	ZENITH_ASSERT_EQ(ZM_UI_StarterChoice::uCELL_COUNT, uEXPECTED_CELL_COUNT,
		"the screen must show exactly the three authored starters");

	for (u_int u = 0u; u < uEXPECTED_CELL_COUNT; ++u)
	{
		const ZM_ExpectedCell& xExpected = axEXPECTED_CELLS[u];
		ZENITH_ASSERT_EQ((u_int)ZM_UI_StarterChoice::CellChoice(xExpected.m_uCell),
			(u_int)xExpected.m_eChoice,
			"cell %u must show starter choice %u", xExpected.m_uCell, (u_int)xExpected.m_eChoice);
		ZENITH_ASSERT_TRUE(ZM_IsRegisteredStarterChoice(ZM_UI_StarterChoice::CellChoice(xExpected.m_uCell)),
			"cell %u must show a REGISTERED choice -- an unregistered one would render an "
			"empty cell the player could press for nothing", xExpected.m_uCell);
		// ...and the shipped resolver must agree with the longhand species column, which is
		// what makes the composition test below a statement about the SCREEN, not the table.
		ZENITH_ASSERT_EQ((u_int)ZM_ResolvePlayerStarterSpecies(xExpected.m_eChoice),
			(u_int)xExpected.m_eSpecies,
			"choice %u must resolve to the authored species", (u_int)xExpected.m_eChoice);
	}
}

ZENITH_TEST(ZM_Starter, StarterScreen_CellCountEqualsStarterChoiceCount)
{
	// THE TRIPWIRE FOR A FOURTH STARTER, and it needs BOTH halves.
	//
	// ★ The derived half is nearly a tautology and is here as documentation, not as
	// teeth: uCELL_COUNT IS static_cast<u_int>(ZM_STARTER_CHOICE_COUNT) and
	// ZM_GetStarterChoiceCount() returns that same enumerator, so this compares an
	// expression with a re-computation of itself -- when the table grows, BOTH sides move
	// together and it stays green. (The same reasoning
	// SaveScreen_RowCountEqualsSlotCount spells out for the save screen.)
	ZENITH_ASSERT_EQ(ZM_UI_StarterChoice::uCELL_COUNT, ZM_GetStarterChoiceCount(),
		"one cell per REGISTERED starter -- the screen count must be derived from the "
		"shipped table accessor, never re-spelled");

	// ★ THE HAND-WRITTEN LITERAL IS THE ACTUAL TRIPWIRE. It reds the moment a starter is
	// added or removed, which is the review this screen must not skip: the authoring loop
	// in Zenithmon.cpp, the deduced name table in ZM_UI_StarterChoice.cpp and the panel
	// geometry all have to grow with it, and only one of those three (the name table) has
	// its own compile-time guard.
	ZENITH_ASSERT_EQ(ZM_UI_StarterChoice::uCELL_COUNT, 3u,
		"the starter screen shows exactly three cells (F01/F02/F03). If a fourth starter "
		"was added on purpose: widen this literal, add its cell name in "
		"ZM_UI_StarterChoice.cpp, and CHECK THE PANEL STILL FITS THE COLUMN.");

	// Every cell in the derived range names a real element (a zero-initialised tail would
	// hand back an empty name and render a blank, unpressable row).
	for (u_int u = 0u; u < ZM_UI_StarterChoice::uCELL_COUNT; ++u)
	{
		const char* szName = ZM_UI_StarterChoice::CellElementName(u);
		ZENITH_ASSERT_NOT_NULL(szName, "cell %u has no element name", u);
		ZENITH_ASSERT_TRUE(szName[0] != '\0', "cell %u has an EMPTY element name", u);
	}
}

// ============================================================================
// 3-5. The name maps: round trip, totality, and the foreign / prefix rejections.
// ============================================================================

ZENITH_TEST(ZM_Starter, StarterScreen_CellNamesAreDistinctAndRoundTrip)
{
	for (u_int u = 0u; u < ZM_UI_StarterChoice::uCELL_COUNT; ++u)
	{
		const char* szName = ZM_UI_StarterChoice::CellElementName(u);
		ZENITH_ASSERT_EQ(ZM_UI_StarterChoice::CellIndexFromElementName(szName), (int)u,
			"cell %u's name '%s' must resolve back to cell %u", u, szName, u);

		// Distinct from every other cell AND from the screen's non-cell elements: two cells
		// sharing a name would make the canvas resolve one widget for two rows.
		for (u_int uOther = 0u; uOther < ZM_UI_StarterChoice::uCELL_COUNT; ++uOther)
		{
			if (uOther == u) { continue; }
			ZENITH_ASSERT_FALSE(
				std::strcmp(szName, ZM_UI_StarterChoice::CellElementName(uOther)) == 0,
				"cells %u and %u share the element name '%s'", u, uOther, szName);
		}
		ZENITH_ASSERT_FALSE(std::strcmp(szName, ZM_UI_StarterChoice::szPANEL_NAME) == 0,
			"cell %u collides with the panel name", u);
		ZENITH_ASSERT_FALSE(std::strcmp(szName, ZM_UI_StarterChoice::szHEADER_NAME) == 0,
			"cell %u collides with the header name", u);
	}

	ZENITH_ASSERT_FALSE(
		std::strcmp(ZM_UI_StarterChoice::szPANEL_NAME, ZM_UI_StarterChoice::szHEADER_NAME) == 0,
		"the panel and the header share an element name");
}

ZENITH_TEST(ZM_Starter, StarterScreen_CellNameIsTotalForAnOutOfRangeCell)
{
	// TOTAL, never asserting: an out-of-range cell yields "" (a valid, never-matching
	// name) rather than indexing off the end of the literal table.
	const u_int auOutOfRange[] =
	{
		ZM_UI_StarterChoice::uCELL_COUNT,
		ZM_UI_StarterChoice::uCELL_COUNT + 1u,
		1000u,
		0xFFFFFFFFu,
	};
	for (const u_int uCell : auOutOfRange)
	{
		const char* szName = ZM_UI_StarterChoice::CellElementName(uCell);
		ZENITH_ASSERT_NOT_NULL(szName, "out-of-range cell %u returned a NULL name", uCell);
		ZENITH_ASSERT_TRUE(szName[0] == '\0',
			"out-of-range cell %u must return an EMPTY name, got '%s'", uCell, szName);
		ZENITH_ASSERT_EQ(ZM_UI_StarterChoice::CellIndexFromElementName(szName), -1,
			"the empty name must not resolve to a cell");
	}
}

ZENITH_TEST(ZM_Starter, StarterScreen_CellIndexRejectsNullForeignAndPrefixNames)
{
	// NULL is the shape ResolveFocusedElementName hands back when nothing is focused, so
	// it travels the real dispatch path every frame -- it must be answered, not crashed on.
	ZENITH_ASSERT_EQ(ZM_UI_StarterChoice::CellIndexFromElementName(nullptr), -1,
		"a null focused name must not resolve to a cell");

	for (u_int u = 0u; u < uFOREIGN_NAME_COUNT; ++u)
	{
		ZENITH_ASSERT_EQ(ZM_UI_StarterChoice::CellIndexFromElementName(aszFOREIGN_NAMES[u]), -1,
			"'%s' must not resolve to a starter cell (EXACT compare, never a prefix match)",
			aszFOREIGN_NAMES[u]);
	}
}

// ============================================================================
// 6-7. Name -> choice, and the totality of the choice map.
// ============================================================================

ZENITH_TEST(ZM_Starter, StarterScreen_ChoiceFromElementNameRoundTripsEveryRegisteredChoice)
{
	// Every REGISTERED choice must be reachable from some cell name, and each cell name
	// must land on its own choice: a walk over the whole enum, so a starter with no cell
	// (or two cells on one starter) is a failure here, not a content surprise.
	for (u_int u = 0u; u < ZM_GetStarterChoiceCount(); ++u)
	{
		const ZM_STARTER_CHOICE eChoice = (ZM_STARTER_CHOICE)u;
		const char* szName = ZM_UI_StarterChoice::CellElementName(u);
		ZENITH_ASSERT_EQ((u_int)ZM_UI_StarterChoice::ChoiceFromElementName(szName), (u_int)eChoice,
			"'%s' must resolve to starter choice %u", szName, u);
		ZENITH_ASSERT_TRUE(
			ZM_IsRegisteredStarterChoice(ZM_UI_StarterChoice::ChoiceFromElementName(szName)),
			"'%s' must resolve to a REGISTERED choice", szName);
	}
}

ZENITH_TEST(ZM_Starter, StarterScreen_CellChoiceIsTotalOverSentinelAndOutOfRangeCells)
{
	// The sentinel and every garbage cell answer NONE -- which ZM_IsRegisteredStarterChoice
	// rejects, so the confirm arm eats them before ZM_ApplyStarterChoice is ever reached.
	const u_int auOutOfRange[] =
	{
		(u_int)ZM_STARTER_CHOICE_COUNT,
		(u_int)ZM_STARTER_CHOICE_NONE,
		ZM_UI_StarterChoice::uCELL_COUNT + 7u,
		0xFFFFFFFFu,
	};
	for (const u_int uCell : auOutOfRange)
	{
		const ZM_STARTER_CHOICE eChoice = ZM_UI_StarterChoice::CellChoice(uCell);
		ZENITH_ASSERT_EQ((u_int)eChoice, (u_int)ZM_STARTER_CHOICE_NONE,
			"cell %u must answer the NONE sentinel", uCell);
		ZENITH_ASSERT_FALSE(ZM_IsRegisteredStarterChoice(eChoice),
			"cell %u's answer must not be a registered choice", uCell);
	}

	// ZM_STARTER_CHOICE_NONE aliases the count, so the sentinel and "one past the end" are
	// the same value -- pinned so a later append that breaks the alias is caught here.
	ZENITH_ASSERT_EQ((u_int)ZM_STARTER_CHOICE_NONE, (u_int)ZM_STARTER_CHOICE_COUNT,
		"the NONE sentinel must keep aliasing the count");
}

// ============================================================================
// 8-10. The label: bounded, named, and EMPTY for anything unregistered.
// ============================================================================

ZENITH_TEST(ZM_Starter, StarterScreen_FormatCellLabelIsNullTerminatedWithinATinyCapacity)
{
	// snprintf semantics, pinned: a capacity far below the species name must TRUNCATE and
	// still terminate, and the bytes past the capacity must be untouched. Present hands
	// this a stack buffer every frame, so an overrun here is a stack smash in the menu.
	for (u_int uCapacity = 1u; uCapacity <= 8u; ++uCapacity)
	{
		char acBuffer[16];
		std::memset(acBuffer, '#', sizeof(acBuffer));
		ZM_UI_StarterChoice::FormatCellLabel(ZM_STARTER_CHOICE_FERNFAWN, acBuffer, uCapacity);

		ZENITH_ASSERT_TRUE(std::strlen(acBuffer) < (size_t)uCapacity,
			"capacity %u produced a %u-char label", uCapacity, (u_int)std::strlen(acBuffer));
		for (u_int u = uCapacity; u < (u_int)sizeof(acBuffer); ++u)
		{
			ZENITH_ASSERT_TRUE(acBuffer[u] == '#',
				"capacity %u wrote past the buffer at byte %u", uCapacity, u);
		}
	}

	// The degenerate arguments the presenter can never pass but a caller might: neither may
	// write anything, and neither may crash.
	char acGuard[4] = { '#', '#', '#', '#' };
	ZM_UI_StarterChoice::FormatCellLabel(ZM_STARTER_CHOICE_KINDLET, acGuard, 0u);
	ZENITH_ASSERT_TRUE(acGuard[0] == '#', "a zero capacity must write nothing");
	ZM_UI_StarterChoice::FormatCellLabel(ZM_STARTER_CHOICE_KINDLET, nullptr, 32u);
}

ZENITH_TEST(ZM_Starter, StarterScreen_FormatCellLabelNamesTheChosenSpecies)
{
	// The POSITIVE half -- without it an always-empty formatter would satisfy the
	// unregistered clause below and ship three blank cells. Each label must be exactly the
	// species name from the compiled dex row named by the LONGHAND table above.
	for (u_int u = 0u; u < uEXPECTED_CELL_COUNT; ++u)
	{
		const ZM_ExpectedCell& xExpected = axEXPECTED_CELLS[u];
		char acLabel[ZM_UI_StarterChoice::uLABEL_CAPACITY];
		ZM_UI_StarterChoice::FormatCellLabel(xExpected.m_eChoice, acLabel,
			ZM_UI_StarterChoice::uLABEL_CAPACITY);

		ZENITH_ASSERT_STREQ(acLabel, ZM_GetSpeciesName(xExpected.m_eSpecies),
			"cell %u must be labelled with its own species name", xExpected.m_uCell);
		ZENITH_ASSERT_TRUE(acLabel[0] != '\0',
			"cell %u rendered an EMPTY label -- the player would see a blank row",
			xExpected.m_uCell);

		// ...and distinct from its neighbours, or two cells read the same and the pick is a
		// coin flip.
		for (u_int uOther = 0u; uOther < uEXPECTED_CELL_COUNT; ++uOther)
		{
			if (uOther == u) { continue; }
			char acOther[ZM_UI_StarterChoice::uLABEL_CAPACITY];
			ZM_UI_StarterChoice::FormatCellLabel(axEXPECTED_CELLS[uOther].m_eChoice, acOther,
				ZM_UI_StarterChoice::uLABEL_CAPACITY);
			ZENITH_ASSERT_FALSE(std::strcmp(acLabel, acOther) == 0,
				"cells %u and %u render the same label '%s'", u, uOther, acLabel);
		}
	}
}

ZENITH_TEST(ZM_Starter, StarterScreen_FormatCellLabelIsEmptyForAnUnregisteredChoice)
{
	// ★ THE GUARD THAT KEEPS A SENTINEL OUT OF THE SPECIES ACCESSORS. The observable
	// proxy for "it never asked the species table" is the string: ZM_GetSpeciesName's own
	// out-of-range answer is the literal "NONE", so a formatter that had forwarded the
	// sentinel would render "NONE" in the cell. An EMPTY buffer is the only answer that
	// proves the lookup was refused rather than performed and stringified.
	const ZM_STARTER_CHOICE aeUnregistered[] =
	{
		ZM_STARTER_CHOICE_NONE,
		ZM_STARTER_CHOICE_COUNT,
		(ZM_STARTER_CHOICE)((u_int)ZM_STARTER_CHOICE_COUNT + 1u),
		(ZM_STARTER_CHOICE)1000u,
		(ZM_STARTER_CHOICE)0xFFFFFFFFu,
	};
	for (const ZM_STARTER_CHOICE eChoice : aeUnregistered)
	{
		char acLabel[ZM_UI_StarterChoice::uLABEL_CAPACITY];
		std::memset(acLabel, '#', sizeof(acLabel));
		ZM_UI_StarterChoice::FormatCellLabel(eChoice, acLabel, ZM_UI_StarterChoice::uLABEL_CAPACITY);

		ZENITH_ASSERT_TRUE(acLabel[0] == '\0',
			"choice %u must render an EMPTY label, got '%s'", (u_int)eChoice, acLabel);
		ZENITH_ASSERT_FALSE(std::strcmp(acLabel, ZM_GetSpeciesName(ZM_SPECIES_NONE)) == 0,
			"choice %u reached the species table -- it rendered its out-of-range answer",
			(u_int)eChoice);
	}
}

// ============================================================================
// 11-13. Confirm resolution, the selection mirror, and the ZM-D-188 rulings.
// ============================================================================

ZENITH_TEST(ZM_Starter, StarterScreen_ConfirmResolutionRejectsNullForeignAndPanelNames)
{
	// This is the exact input the confirm arm feeds: the canvas's focused element NAME,
	// which is null when nothing is focused and the PANEL / HEADER / another screen's
	// control when the focus has wandered. Every one of them must be inert.
	ZENITH_ASSERT_EQ((u_int)ZM_UI_StarterChoice::ChoiceFromElementName(nullptr),
		(u_int)ZM_STARTER_CHOICE_NONE, "a null focused name must grant nothing");

	for (u_int u = 0u; u < uFOREIGN_NAME_COUNT; ++u)
	{
		const ZM_STARTER_CHOICE eChoice =
			ZM_UI_StarterChoice::ChoiceFromElementName(aszFOREIGN_NAMES[u]);
		ZENITH_ASSERT_EQ((u_int)eChoice, (u_int)ZM_STARTER_CHOICE_NONE,
			"'%s' must resolve to NONE", aszFOREIGN_NAMES[u]);
		ZENITH_ASSERT_FALSE(ZM_IsRegisteredStarterChoice(eChoice),
			"'%s' must not reach the grant", aszFOREIGN_NAMES[u]);
	}
}

ZENITH_TEST(ZM_Starter, StarterScreen_ResetReturnsTheScreenToItsOpeningState)
{
	// A fresh presenter opens with NO selection; a mid-flight one must return to exactly
	// that on Reset, or a batched test (or a reopened screen) inherits a stale cursor
	// pointing at a cell the canvas is not focusing.
	ZM_UI_StarterChoice xFresh;
	ZENITH_ASSERT_EQ(xFresh.GetSelectedCell(), -1, "a fresh screen has no selection");

	ZM_UI_StarterChoice xScreen;
	xScreen.SelectCell(2);
	ZENITH_ASSERT_EQ(xScreen.GetSelectedCell(), 2, "the mirror must adopt a live cell");
	xScreen.Reset();
	ZENITH_ASSERT_EQ(xScreen.GetSelectedCell(), xFresh.GetSelectedCell(),
		"Reset must return the screen to its opening state");

	// The mirror is TOTAL: the -1 a failed name lookup yields, and any out-of-range cell,
	// clear it rather than storing a selection nothing is drawing.
	xScreen.SelectCell(1);
	xScreen.SelectCell(-1);
	ZENITH_ASSERT_EQ(xScreen.GetSelectedCell(), -1, "-1 must clear the mirror");
	xScreen.SelectCell(1);
	xScreen.SelectCell((int)ZM_UI_StarterChoice::uCELL_COUNT);
	ZENITH_ASSERT_EQ(xScreen.GetSelectedCell(), -1, "an out-of-range cell must clear the mirror");
}

ZENITH_TEST(ZM_Starter, StarterScreen_CancelIsIgnoredSoTheScreenIsNeverLeftPartyless)
{
	// ★ ZM-D-188, THE RULING THIS WHOLE SCREEN TURNS ON. The starter arm must NOT pop on
	// cancel -- it behaves like the DIALOGUE arm. The reason is spelled out below rather
	// than asserted in prose: a popped picker leaves the party EMPTY, and ZM_CanEnterBattle
	// keys on emptiness alone, so that save is refused every battle from then on.
	ZENITH_ASSERT_TRUE(ZM_UI_StarterChoice::CancelIsIgnored(),
		"cancel must stay a NO-OP on the starter screen (ZM-D-188): popping it would leave "
		"a PARTYLESS save that ZM_CanEnterBattle then refuses every battle for");

	const ZM_GameState xPartyless = ZM_MakeNewGameState();
	ZENITH_ASSERT_TRUE(xPartyless.m_xParty.IsEmpty(),
		"a new game is partyless until the starter is granted");
	ZENITH_ASSERT_FALSE(ZM_CanEnterBattle(xPartyless),
		"...and a partyless state cannot enter a battle -- which is exactly what escaping "
		"the picker would strand the player in");

	// Its companion ruling: no cell is ever hidden, so the authored nav links can never
	// point at a hidden target (which the engine drops with NO spatial fallback). There is
	// no Back element on this screen either -- cancel has nothing to aim at by construction.
	ZENITH_ASSERT_TRUE(ZM_UI_StarterChoice::CellIsAlwaysShown(),
		"every starter cell stays visible + focusable");

	// The screen id itself: distinct, in range, and NOT the empty-stack sentinel (the
	// ZM_MENU_SCREEN_SHOP / _SAVE precedent).
	ZENITH_ASSERT_NE((u_int)ZM_MENU_SCREEN_STARTER, (u_int)ZM_MENU_SCREEN_NONE,
		"the starter screen must not alias the empty-stack sentinel");
	ZENITH_ASSERT_TRUE((u_int)ZM_MENU_SCREEN_STARTER < (u_int)ZM_MENU_SCREEN_COUNT,
		"the starter screen must be a real screen id");
}

// ============================================================================
// 14. The composition: cell name -> choice -> the SHIPPED grant.
// ============================================================================

ZENITH_TEST(ZM_Starter, StarterScreen_CellNameToChoiceToApplyGrantsTheChosenSpecies)
{
	// ★ THE ONE UNIT THAT WALKS THE WHOLE SEAM the confirm arm walks: the focused element
	// NAME -> ChoiceFromElementName -> the SHIPPED ZM_ApplyStarterChoice, on a state from
	// the SHIPPED ZM_MakeNewGameState. The presenter grants NOTHING itself, so this is what
	// proves the cell a player presses and the monster they receive are the same starter.
	for (u_int u = 0u; u < uEXPECTED_CELL_COUNT; ++u)
	{
		const ZM_ExpectedCell& xExpected = axEXPECTED_CELLS[u];

		ZM_GameState xState = ZM_MakeNewGameState();
		ZENITH_ASSERT_TRUE(xState.m_xParty.IsEmpty(), "the fixture must start partyless");

		const char* szCellName = ZM_UI_StarterChoice::CellElementName(xExpected.m_uCell);
		const ZM_STARTER_CHOICE eChoice = ZM_UI_StarterChoice::ChoiceFromElementName(szCellName);

		// GRANT THEN POP is the arm's order, and this is its precondition: the grant reports
		// whether it happened, and only a TRUE may close the screen.
		ZENITH_ASSERT_TRUE(ZM_ApplyStarterChoice(xState, eChoice),
			"pressing '%s' must grant a starter", szCellName);

		ZENITH_ASSERT_EQ(xState.m_xParty.Count(), 1u,
			"'%s' must grant exactly one monster", szCellName);
		// ★ BAIL BEFORE Get(0). ZM_Party::Get is bounds-ASSERTED, and Zenith_Assert calls
		// Zenith_DebugBreak() in every configuration -- so on a failed grant, reading the
		// lead anyway would not report this test as red, it would END the whole boot-unit
		// run. The assertions above have already recorded the failure. (The shipped
		// Apply_GrantsExactlyOneLevelFiveLeadForEveryChoice guards the same way.)
		if (xState.m_xParty.Count() != 1u)
		{
			return;
		}
		ZENITH_ASSERT_EQ((u_int)xState.m_xParty.Get(0u).m_eSpecies, (u_int)xExpected.m_eSpecies,
			"'%s' must grant the species its own label names", szCellName);

		// The LABEL and the GRANT must name the same creature -- the property a player can
		// actually see, and the one a shuffled cell table would break silently.
		char acLabel[ZM_UI_StarterChoice::uLABEL_CAPACITY];
		ZM_UI_StarterChoice::FormatCellLabel(eChoice, acLabel, ZM_UI_StarterChoice::uLABEL_CAPACITY);
		ZENITH_ASSERT_STREQ(acLabel, ZM_GetSpeciesName(xState.m_xParty.Get(0u).m_eSpecies),
			"the cell's label and the granted monster must name the same species");

		// ...and the whole point of the screen: the file can now fight.
		ZENITH_ASSERT_TRUE(ZM_CanEnterBattle(xState),
			"after the grant the player must be able to enter a battle");
	}
}
