#include "Zenith.h"

// ============================================================================
// ZM_Tests_BagScreen -- S6 item 2 (SC6) unit tests for ZM_UI_Bag, the overworld
// bag screen: the authored row-name contract, the paging arithmetic (page count /
// clamp / row -> stack / visible rows), the CYCLING pocket arithmetic, the row and
// header formatters, the confirm-by-name pocket/page state machine, the STARTER
// bag end to end, and the headless half of the presentation seam (it short-circuits
// before it touches a scene). Everything here is PURE -- no ECS, no scene, no
// graphics, no baked assets -- so every fixture is deterministic and hermetic and no
// RequestSkip is needed. Category ZM_BagScreen.
//
// Every bag fixture is built through the REAL ZM_Bag::Add (never by poking the
// pocket arrays), so a change to the bag's own invariants shows up here rather than
// being papered over by a hand-built aggregate.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "ZenithECS/Zenith_Entity.h"                    // the invalid-handle presentation guard
#include "Zenithmon/Components/ZM_UI_MenuStack.h"       // the host's ROOT names (near-miss fixtures)
#include "Zenithmon/Source/Data/ZM_ItemData.h"
#include "Zenithmon/Source/Party/ZM_Bag.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"        // ZM_MakeStarterGameState (the end-to-end fixture)
#include "Zenithmon/Source/UI/ZM_UI_Bag.h"
#include "Zenithmon/Source/UI/ZM_UI_Dex.h"              // a dex CELL name (near-miss fixture)

#include <string>

namespace
{
	constexpr u_int uPOCKET_COUNT = static_cast<u_int>(ZM_ITEM_CATEGORY_COUNT);

	// The cycling assertions need at least two pockets to be non-vacuous, and the
	// multi-page fixture below needs a pocket that can hold more than one page.
	static_assert(uPOCKET_COUNT > 1u, "the pocket-cycling tests need at least two pockets");
	static_assert(uZM_BAG_MAX_STACKS_PER_POCKET > ZM_UI_Bag::uROWS_PER_PAGE,
		"the multi-page fixture needs a pocket that can hold more than one page");

	bool Contains(const std::string& strBody, const char* szNeedle)
	{
		return strBody.find(szNeedle) != std::string::npos;
	}

	// A TM pocket holding uWanted distinct stacks, built through the real mutator. The
	// TM block is the only category with enough distinct ids to overflow a page, and its
	// ids are contiguous from ZM_ITEM_TM_TITANBEAM.
	constexpr ZM_ITEM_CATEGORY eMULTI_PAGE_POCKET = ZM_ITEM_CATEGORY_TM;

	void SeedMultiPagePocket(ZM_Bag& xBag, u_int uWanted)
	{
		for (u_int u = 0u; u < uWanted; ++u)
		{
			const ZM_ITEM_ID eItem =
				static_cast<ZM_ITEM_ID>(static_cast<u_int>(ZM_ITEM_TM_TITANBEAM) + u);
			ZENITH_ASSERT_TRUE(ZM_GetItemData(eItem).m_eCategory == eMULTI_PAGE_POCKET,
				"fixture item %u must live in the multi-page pocket", static_cast<u_int>(eItem));
			ZENITH_ASSERT_TRUE(xBag.Add(eItem, 1u), "the fixture add must be accepted");
		}
		ZENITH_ASSERT_EQ(xBag.PocketStackCount(eMULTI_PAGE_POCKET), uWanted,
			"the fixture pocket really holds %u stacks", uWanted);
	}

	// Walk a screen onto eWanted using ONLY the public confirm surface (there is no
	// SetPocket -- the pocket is player-driven state, not a test hook).
	void StepScreenToPocket(ZM_UI_Bag& xScreen, ZM_ITEM_CATEGORY eWanted, const ZM_Bag& xBag)
	{
		for (u_int u = 0u; u < uPOCKET_COUNT && xScreen.GetPocket() != eWanted; ++u)
		{
			xScreen.Confirm(ZM_UI_Bag::szNEXT_POCKET_NAME, xBag);
		}
		ZENITH_ASSERT_TRUE(xScreen.GetPocket() == eWanted,
			"the walk really reached pocket %s", ZM_ItemCategoryToString(eWanted));
	}
}

// ---- RowElementName ---------------------------------------------------------

ZENITH_TEST(ZM_BagScreen, RowElementName_EveryInRangeRowIsDistinctAndNonEmpty)
{
	for (u_int u = 0u; u < ZM_UI_Bag::uROWS_PER_PAGE; ++u)
	{
		const char* szName = ZM_UI_Bag::RowElementName(u);
		ZENITH_ASSERT_NOT_NULL(szName, "row %u has a name", u);
		ZENITH_ASSERT_TRUE(szName[0] != '\0', "row %u's name is not the empty string", u);
		for (u_int v = 0u; v < u; ++v)
		{
			ZENITH_ASSERT_FALSE(std::string(szName) == ZM_UI_Bag::RowElementName(v),
				"row %u and row %u must not share an element name", u, v);
		}
	}
}

ZENITH_TEST(ZM_BagScreen, RowElementName_OutOfRangeIsEmpty)
{
	ZENITH_ASSERT_STREQ(ZM_UI_Bag::RowElementName(ZM_UI_Bag::uROWS_PER_PAGE), "",
		"the first out-of-range row has no element name");
	ZENITH_ASSERT_STREQ(ZM_UI_Bag::RowElementName(9999u), "",
		"a wildly out-of-range row has no element name (never a dangling pointer)");
}

// ---- RowIndexFromElementName ------------------------------------------------

ZENITH_TEST(ZM_BagScreen, RowIndex_RoundTripsEveryRow)
{
	for (u_int u = 0u; u < ZM_UI_Bag::uROWS_PER_PAGE; ++u)
	{
		ZENITH_ASSERT_EQ(ZM_UI_Bag::RowIndexFromElementName(ZM_UI_Bag::RowElementName(u)),
			static_cast<int>(u), "row %u round-trips through its element name", u);
	}
}

ZENITH_TEST(ZM_BagScreen, RowIndex_NullEmptyAndForeignNamesAreNegative)
{
	ZENITH_ASSERT_EQ(ZM_UI_Bag::RowIndexFromElementName(nullptr), -1,
		"a null name is not a bag row");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::RowIndexFromElementName(""), -1,
		"the empty name is not a bag row");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::RowIndexFromElementName(ZM_UI_MenuStack::szROOT_BAG_NAME), -1,
		"the ROOT 'Bag' entry is not a bag ROW (the two contracts must not collide)");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::RowIndexFromElementName(ZM_UI_Dex::CellElementName(0u)), -1,
		"a dex cell is not a bag row");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::RowIndexFromElementName("Menu_BagRow8"), -1,
		"the one-past-the-end row name resolves to no row");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::RowIndexFromElementName(ZM_UI_Bag::szNEXT_PAGE_NAME), -1,
		"a nav button is not a row -- Present relies on telling them apart");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::RowIndexFromElementName(ZM_UI_Bag::szNEXT_POCKET_NAME), -1,
		"...including the pocket buttons");
}

// ---- PageCount --------------------------------------------------------------

ZENITH_TEST(ZM_BagScreen, PageCount_IsNeverZeroAndRoundsUp)
{
	ZENITH_ASSERT_EQ(ZM_UI_Bag::PageCount(0u), 1u,
		"an EMPTY pocket still shows ONE blank page (0 pages would leave ClampPage nothing "
		"to clamp to)");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::PageCount(1u), 1u, "a single stack fits on one page");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::PageCount(ZM_UI_Bag::uROWS_PER_PAGE), 1u,
		"an exactly-full page is ONE page, not two");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::PageCount(ZM_UI_Bag::uROWS_PER_PAGE + 1u), 2u,
		"one stack past a full page opens a second page");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::PageCount(ZM_UI_Bag::uROWS_PER_PAGE * 3u), 3u,
		"three exactly-full pages are three pages");
}

// ---- ClampPage --------------------------------------------------------------

ZENITH_TEST(ZM_BagScreen, ClampPage_ClampsBothEnds)
{
	const u_int uStacks = ZM_UI_Bag::uROWS_PER_PAGE * 2u + 1u;   // three pages
	const int iLast = static_cast<int>(ZM_UI_Bag::PageCount(uStacks)) - 1;
	ZENITH_ASSERT_EQ(iLast, 2, "the fixture really spans three pages");

	ZENITH_ASSERT_EQ(ZM_UI_Bag::ClampPage(-1, uStacks), 0, "a negative page clamps to the first");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::ClampPage(-9999, uStacks), 0, "...however negative");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::ClampPage(iLast + 1, uStacks), iLast,
		"one page past the end clamps to the last page");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::ClampPage(9999, uStacks), iLast, "...however far past");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::ClampPage(0, uStacks), 0, "an in-range page is unchanged");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::ClampPage(1, uStacks), 1, "...anywhere in the range");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::ClampPage(iLast, uStacks), iLast, "...including the last page");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::ClampPage(3, 0u), 0,
		"an empty pocket clamps everything onto its single blank page");
}

// ---- ClampPocket / StepPocket (these CYCLE, unlike the page) ----------------

ZENITH_TEST(ZM_BagScreen, ClampPocket_WrapsRatherThanClamping)
{
	for (u_int u = 0u; u < uPOCKET_COUNT; ++u)
	{
		ZENITH_ASSERT_EQ(ZM_UI_Bag::ClampPocket(static_cast<int>(u)), static_cast<int>(u),
			"an in-range pocket %u is unchanged", u);
	}
	ZENITH_ASSERT_EQ(ZM_UI_Bag::ClampPocket(static_cast<int>(uPOCKET_COUNT)), 0,
		"one past the last pocket WRAPS to the first (clamping would strand the player)");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::ClampPocket(-1), static_cast<int>(uPOCKET_COUNT) - 1,
		"one before the first pocket wraps to the last");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::ClampPocket(static_cast<int>(uPOCKET_COUNT) * 3 + 2), 2,
		"several whole cycles past the end still land on the right pocket");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::ClampPocket(-static_cast<int>(uPOCKET_COUNT) * 3 - 1),
		static_cast<int>(uPOCKET_COUNT) - 1,
		"...and several whole cycles before the start (C++ '%' truncates towards zero)");
}

ZENITH_TEST(ZM_BagScreen, StepPocket_CyclesBothWaysAndAFullCycleReturns)
{
	const ZM_ITEM_CATEGORY eFirst = static_cast<ZM_ITEM_CATEGORY>(0u);
	const ZM_ITEM_CATEGORY eLast = static_cast<ZM_ITEM_CATEGORY>(uPOCKET_COUNT - 1u);

	ZENITH_ASSERT_TRUE(ZM_UI_Bag::StepPocket(eFirst, 1) == static_cast<ZM_ITEM_CATEGORY>(1u),
		"stepping forward off the first pocket lands on the second");
	ZENITH_ASSERT_TRUE(ZM_UI_Bag::StepPocket(eLast, 1) == eFirst,
		"stepping forward off the LAST pocket wraps to the first");
	ZENITH_ASSERT_TRUE(ZM_UI_Bag::StepPocket(eFirst, -1) == eLast,
		"stepping back off the FIRST pocket wraps to the last");

	// A whole cycle of forward steps returns to the start -- expressed against
	// ZM_ITEM_CATEGORY_COUNT, never a literal, so adding a pocket cannot rot this.
	ZM_ITEM_CATEGORY eWalk = eFirst;
	for (u_int u = 0u; u < uPOCKET_COUNT; ++u)
	{
		eWalk = ZM_UI_Bag::StepPocket(eWalk, 1);
	}
	ZENITH_ASSERT_TRUE(eWalk == eFirst, "a full forward cycle returns to the starting pocket");
	for (u_int u = 0u; u < uPOCKET_COUNT; ++u)
	{
		eWalk = ZM_UI_Bag::StepPocket(eWalk, -1);
	}
	ZENITH_ASSERT_TRUE(eWalk == eFirst, "...and so does a full backward cycle");

	// Every pocket must be REACHABLE by stepping, or an item category would be
	// unviewable (the whole reason the pockets cycle instead of clamping).
	for (u_int u = 0u; u < uPOCKET_COUNT; ++u)
	{
		eWalk = ZM_UI_Bag::StepPocket(eWalk, 1);
		ZENITH_ASSERT_TRUE(static_cast<u_int>(eWalk) < uPOCKET_COUNT,
			"step %u stays inside the pocket range", u);
	}
}

// ---- StackIndexForRow -------------------------------------------------------

ZENITH_TEST(ZM_BagScreen, StackIndexForRow_MapsPagesInOrder)
{
	const u_int uStacks = ZM_UI_Bag::uROWS_PER_PAGE + 3u;   // two pages, the second partial

	ZENITH_ASSERT_EQ(ZM_UI_Bag::StackIndexForRow(0u, 0u, uStacks), 0,
		"page 0 row 0 is the pocket's first stack");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::StackIndexForRow(0u, 1u, uStacks), 1,
		"rows advance one stack at a time within a page");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::StackIndexForRow(1u, 0u, uStacks),
		static_cast<int>(ZM_UI_Bag::uROWS_PER_PAGE), "page 1 starts a whole page in");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::StackIndexForRow(1u, 2u, uStacks),
		static_cast<int>(ZM_UI_Bag::uROWS_PER_PAGE + 2u),
		"...and keeps its offset within the page");
	// The LAST valid stack lands on the last live row of the trailing page.
	ZENITH_ASSERT_EQ(ZM_UI_Bag::StackIndexForRow(1u, 2u, uStacks), static_cast<int>(uStacks - 1u),
		"the final stack sits on the last live row");
}

ZENITH_TEST(ZM_BagScreen, StackIndexForRow_RejectsDeadAndOutOfRangeRows)
{
	const u_int uStacks = ZM_UI_Bag::uROWS_PER_PAGE + 3u;

	ZENITH_ASSERT_EQ(ZM_UI_Bag::StackIndexForRow(1u, 3u, uStacks), -1,
		"the FIRST row past the end of the pocket maps to no stack");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::StackIndexForRow(1u, ZM_UI_Bag::uROWS_PER_PAGE - 1u, uStacks), -1,
		"...and so does the trailing page's last widget");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::StackIndexForRow(0u, ZM_UI_Bag::uROWS_PER_PAGE, uStacks), -1,
		"an out-of-range row index maps to no stack");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::StackIndexForRow(2u, 0u, uStacks), -1,
		"a page past the end maps to no stack");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::StackIndexForRow(0u, 0u, 0u), -1,
		"an EMPTY pocket's blank page maps nothing (never index 0 into an empty pocket)");
}

// ---- VisibleRowCount --------------------------------------------------------

ZENITH_TEST(ZM_BagScreen, VisibleRowCount_FullPartialEmptyAndPastTheEnd)
{
	const u_int uStacks = ZM_UI_Bag::uROWS_PER_PAGE * 2u + 3u;   // three pages, the last partial

	ZENITH_ASSERT_EQ(ZM_UI_Bag::VisibleRowCount(0u, uStacks), ZM_UI_Bag::uROWS_PER_PAGE,
		"a full page shows every row");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::VisibleRowCount(1u, uStacks), ZM_UI_Bag::uROWS_PER_PAGE,
		"...and so does the second full page");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::VisibleRowCount(2u, uStacks), 3u,
		"the trailing page shows exactly the remainder");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::VisibleRowCount(3u, uStacks), 0u,
		"a page past the end shows nothing");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::VisibleRowCount(0u, 0u), 0u,
		"an EMPTY pocket shows no rows (the '(empty)' notice is not a row that maps to a stack)");

	// The per-page counts must sum back to the whole pocket -- a dropped or
	// double-counted stack fails here even if each page looked plausible alone.
	u_int uTotal = 0u;
	for (u_int u = 0u; u < ZM_UI_Bag::PageCount(uStacks); ++u)
	{
		uTotal += ZM_UI_Bag::VisibleRowCount(u, uStacks);
	}
	ZENITH_ASSERT_EQ(uTotal, uStacks, "every stack is reachable on exactly one page");
}

// ---- The formatters ---------------------------------------------------------

ZENITH_TEST(ZM_BagScreen, FormatRow_CarriesTheItemNameAndCount)
{
	const std::string strRow = ZM_UI_Bag::FormatRow(ZM_ITEM_CATCHORB, 5u);
	ZENITH_ASSERT_TRUE(Contains(strRow, ZM_GetItemName(ZM_ITEM_CATCHORB)),
		"the row names the item through ZM_GetItemName ('%s')", strRow.c_str());
	ZENITH_ASSERT_TRUE(Contains(strRow, "x5"),
		"the row carries the stack count ('%s')", strRow.c_str());

	// The count must actually track, and a DIFFERENT item must read differently --
	// otherwise a stubbed formatter would satisfy both assertions above.
	ZENITH_ASSERT_FALSE(strRow == ZM_UI_Bag::FormatRow(ZM_ITEM_CATCHORB, 6u),
		"the row tracks the count");
	ZENITH_ASSERT_FALSE(strRow == ZM_UI_Bag::FormatRow(ZM_ITEM_SALVE, 5u),
		"the row tracks the item");
	ZENITH_ASSERT_TRUE(Contains(ZM_UI_Bag::FormatRow(ZM_ITEM_SALVE, 3u), ZM_GetItemName(ZM_ITEM_SALVE)),
		"...naming the second item too");
}

ZENITH_TEST(ZM_BagScreen, FormatHeader_CarriesThePocketNameAndMoney)
{
	const std::string strHeader = ZM_UI_Bag::FormatHeader(ZM_ITEM_CATEGORY_BALL, 3000u);
	ZENITH_ASSERT_TRUE(Contains(strHeader, ZM_ItemCategoryToString(ZM_ITEM_CATEGORY_BALL)),
		"the header names the pocket through ZM_ItemCategoryToString ('%s')", strHeader.c_str());
	ZENITH_ASSERT_TRUE(Contains(strHeader, "3000"),
		"the header carries the money figure ('%s')", strHeader.c_str());

	ZENITH_ASSERT_FALSE(strHeader == ZM_UI_Bag::FormatHeader(ZM_ITEM_CATEGORY_MEDICINE, 3000u),
		"the header tracks the pocket");
	ZENITH_ASSERT_FALSE(strHeader == ZM_UI_Bag::FormatHeader(ZM_ITEM_CATEGORY_BALL, 2999u),
		"the header tracks the money");
	ZENITH_ASSERT_TRUE(Contains(ZM_UI_Bag::FormatHeader(ZM_ITEM_CATEGORY_TM, 0u), "0"),
		"a broke player still reads a money figure");
}

ZENITH_TEST(ZM_BagScreen, FormatEmptyPocket_IsAVisibleNotice)
{
	const std::string strEmpty = ZM_UI_Bag::FormatEmptyPocket();
	ZENITH_ASSERT_FALSE(strEmpty.empty(),
		"the empty-pocket notice must be a real label -- an empty string would draw nothing "
		"and the pocket would just look broken");
	// It must not collide with a real row, or a player could not tell an empty pocket
	// from an item whose label happened to match.
	ZENITH_ASSERT_FALSE(strEmpty == ZM_UI_Bag::FormatRow(ZM_ITEM_CATCHORB, 1u),
		"the notice is distinguishable from a real row");
}

// ---- The Confirm state machine ----------------------------------------------

ZENITH_TEST(ZM_BagScreen, Fresh_StartsOnTheFirstPocketPageAndRow)
{
	ZM_UI_Bag xScreen;
	ZENITH_ASSERT_TRUE(xScreen.GetPocket() == ZM_ITEM_CATEGORY_BALL,
		"a fresh bag screen opens on the BALL pocket");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 0, "...on the first page");
	ZENITH_ASSERT_EQ(xScreen.GetCursor(), 0, "...on the first row");
}

ZENITH_TEST(ZM_BagScreen, Confirm_PagesWithinTheCurrentPocketAndStopsAtBothEnds)
{
	ZM_Bag xBag;
	xBag.Clear();
	SeedMultiPagePocket(xBag, ZM_UI_Bag::uROWS_PER_PAGE + 1u);   // exactly two pages

	ZM_UI_Bag xScreen;
	StepScreenToPocket(xScreen, eMULTI_PAGE_POCKET, xBag);
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 0, "the pocket walk left the page at 0");

	ZENITH_ASSERT_FALSE(xScreen.Confirm(ZM_UI_Bag::szPREV_PAGE_NAME, xBag),
		"Prev on page 0 reports no change");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 0, "...and never goes negative");

	ZENITH_ASSERT_TRUE(xScreen.Confirm(ZM_UI_Bag::szNEXT_PAGE_NAME, xBag),
		"Next pages forward inside the pocket");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 1, "...to page 1");
	ZENITH_ASSERT_FALSE(xScreen.Confirm(ZM_UI_Bag::szNEXT_PAGE_NAME, xBag),
		"Next on the LAST page reports no change");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 1, "...and never runs past the end");

	ZENITH_ASSERT_TRUE(xScreen.Confirm(ZM_UI_Bag::szPREV_PAGE_NAME, xBag), "Prev pages back");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 0, "...one page at a time");
}

ZENITH_TEST(ZM_BagScreen, Confirm_PocketChangeCyclesAndAlwaysResetsThePage)
{
	ZM_Bag xBag;
	xBag.Clear();
	SeedMultiPagePocket(xBag, ZM_UI_Bag::uROWS_PER_PAGE + 1u);

	ZM_UI_Bag xScreen;
	StepScreenToPocket(xScreen, eMULTI_PAGE_POCKET, xBag);
	ZENITH_ASSERT_TRUE(xScreen.Confirm(ZM_UI_Bag::szNEXT_PAGE_NAME, xBag), "park on page 1");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 1, "the fixture really is off page 0");

	// Changing pocket MUST drop the page: the new pocket may hold fewer pages, and a
	// stale page would show a blank list until something happened to clamp it.
	ZENITH_ASSERT_TRUE(xScreen.Confirm(ZM_UI_Bag::szNEXT_POCKET_NAME, xBag),
		"the next-pocket button reports a change");
	ZENITH_ASSERT_TRUE(xScreen.GetPocket() == ZM_UI_Bag::StepPocket(eMULTI_PAGE_POCKET, 1),
		"...onto the next pocket");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 0, "changing pocket RESETS the page to 0");

	// ...and the same for the prev button, from a paged state again.
	StepScreenToPocket(xScreen, eMULTI_PAGE_POCKET, xBag);
	ZENITH_ASSERT_TRUE(xScreen.Confirm(ZM_UI_Bag::szNEXT_PAGE_NAME, xBag), "park on page 1 again");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 1, "the second fixture really is off page 0");
	ZENITH_ASSERT_TRUE(xScreen.Confirm(ZM_UI_Bag::szPREV_POCKET_NAME, xBag),
		"the prev-pocket button reports a change");
	ZENITH_ASSERT_TRUE(xScreen.GetPocket() == ZM_UI_Bag::StepPocket(eMULTI_PAGE_POCKET, -1),
		"...onto the previous pocket");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 0, "...and it too resets the page");

	// The pockets CYCLE: a full lap of next presses returns to where it started.
	const ZM_ITEM_CATEGORY eStart = xScreen.GetPocket();
	for (u_int u = 0u; u < uPOCKET_COUNT; ++u)
	{
		ZENITH_ASSERT_TRUE(xScreen.Confirm(ZM_UI_Bag::szNEXT_POCKET_NAME, xBag),
			"pocket step %u is accepted (the row never dead-ends)", u);
	}
	ZENITH_ASSERT_TRUE(xScreen.GetPocket() == eStart,
		"a full lap of pocket steps returns to the starting pocket");
}

ZENITH_TEST(ZM_BagScreen, Confirm_ARowAndANullNameAreInert)
{
	ZM_Bag xBag;
	xBag.Clear();
	SeedMultiPagePocket(xBag, ZM_UI_Bag::uROWS_PER_PAGE + 1u);

	ZM_UI_Bag xScreen;
	StepScreenToPocket(xScreen, eMULTI_PAGE_POCKET, xBag);
	ZENITH_ASSERT_TRUE(xScreen.Confirm(ZM_UI_Bag::szNEXT_PAGE_NAME, xBag), "park on page 1");

	// A row has no item action menu yet (SC7+), so confirming one must be a silent
	// no-op rather than paging or crashing.
	ZENITH_ASSERT_FALSE(xScreen.Confirm(ZM_UI_Bag::RowElementName(0u), xBag),
		"confirming a row changes nothing");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 1, "...the page is untouched");
	ZENITH_ASSERT_TRUE(xScreen.GetPocket() == eMULTI_PAGE_POCKET, "...and so is the pocket");

	ZENITH_ASSERT_FALSE(xScreen.Confirm(nullptr, xBag),
		"a null focused name (nothing focused) changes nothing");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 1, "...the page is still untouched");
	ZENITH_ASSERT_TRUE(xScreen.GetPocket() == eMULTI_PAGE_POCKET, "...and so is the pocket");

	ZENITH_ASSERT_FALSE(xScreen.Confirm(ZM_UI_MenuStack::szROOT_BAG_NAME, xBag),
		"a foreign element name changes nothing");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 1, "...nor does a foreign name move the page");
}

ZENITH_TEST(ZM_BagScreen, Reset_ReturnsAMidFlightScreenToFresh)
{
	ZM_Bag xBag;
	xBag.Clear();
	SeedMultiPagePocket(xBag, ZM_UI_Bag::uROWS_PER_PAGE + 1u);

	ZM_UI_Bag xScreen;
	StepScreenToPocket(xScreen, eMULTI_PAGE_POCKET, xBag);
	ZENITH_ASSERT_TRUE(xScreen.Confirm(ZM_UI_Bag::szNEXT_PAGE_NAME, xBag), "page off the first page");
	Zenith_Entity xNoRoot;
	xScreen.Hide(xNoRoot);   // ...and clear the cursor, so ALL THREE fields are mid-flight
	ZENITH_ASSERT_TRUE(xScreen.GetPocket() == eMULTI_PAGE_POCKET, "the fixture really is mid-flight");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 1, "...on the page");
	ZENITH_ASSERT_EQ(xScreen.GetCursor(), -1, "...and on the cursor");

	xScreen.Reset();
	ZENITH_ASSERT_TRUE(xScreen.GetPocket() == ZM_ITEM_CATEGORY_BALL,
		"Reset returns to the first pocket");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 0, "Reset returns to the first page");
	ZENITH_ASSERT_EQ(xScreen.GetCursor(), 0, "Reset returns the cursor to the first row");
}

// ---- The STARTER bag, end to end -------------------------------------------

ZENITH_TEST(ZM_BagScreen, StarterBag_ListsTheSeededPocketsAndMoney)
{
	const ZM_GameState xState = ZM_MakeStarterGameState();

	// The seeded pockets are resolved through the ITEM TABLE, never a hard-coded index,
	// so re-categorising an item cannot silently make this test read the wrong pocket.
	const ZM_ITEM_CATEGORY eBallPocket = ZM_GetItemData(ZM_ITEM_CATCHORB).m_eCategory;
	const ZM_ITEM_CATEGORY eHealPocket = ZM_GetItemData(ZM_ITEM_SALVE).m_eCategory;
	ZENITH_ASSERT_FALSE(eBallPocket == eHealPocket,
		"the two seeded items must live in DIFFERENT pockets or the pocket assertions are vacuous");

	const u_int uBallStacks = xState.m_xBag.PocketStackCount(eBallPocket);
	ZENITH_ASSERT_EQ(uBallStacks, 1u, "the starter seeds exactly one ball stack");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::VisibleRowCount(0u, uBallStacks), 1u,
		"...so page 0 of that pocket shows exactly one row");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::StackIndexForRow(0u, 0u, uBallStacks), 0,
		"...on row 0");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::StackIndexForRow(0u, 1u, uBallStacks), -1,
		"...with row 1 dead");

	const ZM_ItemStack& xBallStack = xState.m_xBag.PocketStack(eBallPocket, 0u);
	ZENITH_ASSERT_TRUE(xBallStack.m_eItem == ZM_ITEM_CATCHORB,
		"the ball pocket's first stack is the seeded Catch Orb");
	ZENITH_ASSERT_GT(xBallStack.m_uCount, 0u, "...with a real count");
	const std::string strBallRow = ZM_UI_Bag::FormatRow(xBallStack.m_eItem, xBallStack.m_uCount);
	ZENITH_ASSERT_TRUE(Contains(strBallRow, ZM_GetItemName(ZM_ITEM_CATCHORB)),
		"the ball row names the Catch Orb ('%s')", strBallRow.c_str());
	ZENITH_ASSERT_TRUE(Contains(strBallRow, std::to_string(xBallStack.m_uCount).c_str()),
		"...and carries its seeded count ('%s')", strBallRow.c_str());

	const ZM_ItemStack& xHealStack = xState.m_xBag.PocketStack(eHealPocket, 0u);
	ZENITH_ASSERT_TRUE(xHealStack.m_eItem == ZM_ITEM_SALVE,
		"the medicine pocket's first stack is the seeded Salve");
	ZENITH_ASSERT_TRUE(Contains(ZM_UI_Bag::FormatRow(xHealStack.m_eItem, xHealStack.m_uCount),
		ZM_GetItemName(ZM_ITEM_SALVE)), "the medicine row names the Salve");

	// The header carries the seeded purse (read off the state -- the starting figure is
	// tuning data owned by ZM_MakeStarterGameState, not a constant to duplicate here).
	ZENITH_ASSERT_GT(xState.m_uMoney, 0u,
		"the starter must seed some money or the header assertion is vacuous");
	const std::string strHeader = ZM_UI_Bag::FormatHeader(eBallPocket, xState.m_uMoney);
	ZENITH_ASSERT_TRUE(Contains(strHeader, std::to_string(xState.m_uMoney).c_str()),
		"the header carries the seeded money ('%s')", strHeader.c_str());
	ZENITH_ASSERT_TRUE(Contains(strHeader, ZM_ItemCategoryToString(eBallPocket)),
		"...alongside the pocket name ('%s')", strHeader.c_str());

	// A pocket the starter never seeds reads as empty -- the notice path, not a row.
	const ZM_ITEM_CATEGORY eKeyPocket = ZM_GetItemData(ZM_ITEM_ANGLEROD).m_eCategory;
	ZENITH_ASSERT_EQ(xState.m_xBag.PocketStackCount(eKeyPocket), 0u,
		"the starter seeds no key items");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::VisibleRowCount(0u, 0u), 0u, "...so that pocket lists no rows");
	ZENITH_ASSERT_EQ(ZM_UI_Bag::PageCount(0u), 1u, "...on its one blank page");
}

// ---- Presentation seam (headless half) -------------------------------------

ZENITH_TEST(ZM_BagScreen, Present_OnInvalidRootIsABestEffortNoOp)
{
	// Present short-circuits on an INVALID root handle right after settling the pocket /
	// page clamp and before it touches any scene, so the guard itself is headlessly
	// testable. The contract pinned here is that presentation never disturbs the model.
	ZM_Bag xBag;
	xBag.Clear();
	SeedMultiPagePocket(xBag, ZM_UI_Bag::uROWS_PER_PAGE + 1u);

	ZM_UI_Bag xScreen;
	StepScreenToPocket(xScreen, eMULTI_PAGE_POCKET, xBag);
	ZENITH_ASSERT_TRUE(xScreen.Confirm(ZM_UI_Bag::szNEXT_PAGE_NAME, xBag), "park on page 1");

	Zenith_Entity xNoRoot;
	ZENITH_ASSERT_FALSE(xNoRoot.IsValid(), "a default-constructed entity handle is invalid");
	xScreen.Present(xNoRoot, xBag, 1234u);

	ZENITH_ASSERT_TRUE(xScreen.GetPocket() == eMULTI_PAGE_POCKET,
		"presenting to a missing root never moves the pocket");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 1, "...nor the page (it is still in range)");
	ZENITH_ASSERT_EQ(xScreen.GetCursor(), 0, "...nor the cursor");

	// ...and the MODEL is untouched too: presentation is strictly read-only over the bag.
	ZENITH_ASSERT_EQ(xBag.PocketStackCount(eMULTI_PAGE_POCKET), ZM_UI_Bag::uROWS_PER_PAGE + 1u,
		"presentation never mutates the bag");
}

ZENITH_TEST(ZM_BagScreen, Hide_ClearsTheCursorButKeepsThePocketAndPage)
{
	// Hide means "not presented", which is exactly what GetCursor's -1 contracts; the
	// pocket and page are session state the menu stack drops through Reset, not here.
	ZM_Bag xBag;
	xBag.Clear();
	SeedMultiPagePocket(xBag, ZM_UI_Bag::uROWS_PER_PAGE + 1u);

	ZM_UI_Bag xScreen;
	StepScreenToPocket(xScreen, eMULTI_PAGE_POCKET, xBag);
	ZENITH_ASSERT_TRUE(xScreen.Confirm(ZM_UI_Bag::szNEXT_PAGE_NAME, xBag), "park on page 1");

	Zenith_Entity xNoRoot;
	xScreen.Hide(xNoRoot);
	ZENITH_ASSERT_EQ(xScreen.GetCursor(), -1, "a hidden screen focuses no row");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 1, "Hide does not drop the session page (Reset owns that)");
	ZENITH_ASSERT_TRUE(xScreen.GetPocket() == eMULTI_PAGE_POCKET, "...nor the session pocket");

	// ...and a hidden screen still pages by name: the pocket/page are model state, not
	// widget state.
	ZENITH_ASSERT_TRUE(xScreen.Confirm(ZM_UI_Bag::szPREV_PAGE_NAME, xBag),
		"the paging model keeps working while nothing is drawn");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 0, "...back to the first page");
}
