#include "Zenith.h"

// ============================================================================
// ZM_Tests_DexScreen -- S6 item 2 (SC5) unit tests for ZM_UI_Dex, the overworld
// dex screen: the runtime-built grid's cell-name contract, the paging arithmetic
// (page count / clamp / cell -> species / visible cells), the caught-gated cell
// label and the completion header, the confirm-by-name paging state machine, and
// the headless half of the presentation seam (it short-circuits before it touches
// a scene). Everything here is PURE -- no ECS, no scene, no graphics, no baked
// assets -- so every fixture is deterministic and hermetic and no RequestSkip is
// needed. Category ZM_DexScreen.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "ZenithECS/Zenith_Entity.h"                    // the invalid-handle presentation guard
#include "Zenithmon/Components/ZM_UI_MenuStack.h"       // the host's ROOT names (near-miss fixtures)
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"       // ZM_SPECIES_COUNT / ZM_GetSpeciesName
#include "Zenithmon/Source/Party/ZM_GameState.h"        // ZM_SpeciesSet
#include "Zenithmon/Source/UI/ZM_UI_Dex.h"
#include "Zenithmon/Source/UI/ZM_UI_Party.h"            // a party SLOT name (near-miss fixture)

#include <string>

namespace
{
	constexpr u_int uDEX_SIZE = static_cast<u_int>(ZM_SPECIES_COUNT);

	// The zero-padding fixtures below read the 1st / 10th / 100th dex entries.
	static_assert(uDEX_SIZE >= 100u, "the zero-padding fixtures need a three-digit dex");

	// The page geometry, derived INDEPENDENTLY of ZM_UI_Dex::PageCount (a walk, not the
	// same ceil expression), so a broken formula cannot agree with its own mirror. Every
	// expectation below is written against these -- never a magic number -- so the tests
	// survive the dex roster growing.
	u_int PagesByWalk()
	{
		u_int uPages = 0u;
		for (u_int u = 0u; u < uDEX_SIZE; u += ZM_UI_Dex::uCELL_COUNT)
		{
			++uPages;
		}
		return uPages;
	}

	u_int LastPage()          { return PagesByWalk() - 1u; }
	u_int LastPageCellCount() { return uDEX_SIZE - LastPage() * ZM_UI_Dex::uCELL_COUNT; }

	bool Contains(const std::string& strBody, const char* szNeedle)
	{
		return strBody.find(szNeedle) != std::string::npos;
	}
}

// ---- CellElementName --------------------------------------------------------

ZENITH_TEST(ZM_DexScreen, CellElementName_EveryInRangeCellIsDistinctAndNonEmpty)
{
	for (u_int u = 0u; u < ZM_UI_Dex::uCELL_COUNT; ++u)
	{
		const char* szName = ZM_UI_Dex::CellElementName(u);
		ZENITH_ASSERT_NOT_NULL(szName, "cell %u has a name", u);
		ZENITH_ASSERT_TRUE(szName[0] != '\0', "cell %u's name is not the empty string", u);
		for (u_int v = 0u; v < u; ++v)
		{
			ZENITH_ASSERT_FALSE(std::string(szName) == ZM_UI_Dex::CellElementName(v),
				"cell %u and cell %u must not share an element name", u, v);
		}
	}
}

ZENITH_TEST(ZM_DexScreen, CellElementName_OutOfRangeIsEmpty)
{
	ZENITH_ASSERT_STREQ(ZM_UI_Dex::CellElementName(ZM_UI_Dex::uCELL_COUNT), "",
		"the first out-of-range cell has no element name");
	ZENITH_ASSERT_STREQ(ZM_UI_Dex::CellElementName(9999u), "",
		"a wildly out-of-range cell has no element name (never a dangling pointer)");
}

// ---- CellIndexFromElementName ----------------------------------------------

ZENITH_TEST(ZM_DexScreen, CellIndex_RoundTripsEveryCell)
{
	for (u_int u = 0u; u < ZM_UI_Dex::uCELL_COUNT; ++u)
	{
		ZENITH_ASSERT_EQ(ZM_UI_Dex::CellIndexFromElementName(ZM_UI_Dex::CellElementName(u)),
			static_cast<int>(u), "cell %u round-trips through its element name", u);
	}
}

ZENITH_TEST(ZM_DexScreen, CellIndex_NullEmptyAndForeignNamesAreNegative)
{
	ZENITH_ASSERT_EQ(ZM_UI_Dex::CellIndexFromElementName(nullptr), -1,
		"a null name is not a dex cell");
	ZENITH_ASSERT_EQ(ZM_UI_Dex::CellIndexFromElementName(""), -1,
		"the empty name is not a dex cell");
	ZENITH_ASSERT_EQ(ZM_UI_Dex::CellIndexFromElementName(ZM_UI_MenuStack::szROOT_DEX_NAME), -1,
		"the ROOT 'Dex' entry is not a dex CELL (the two contracts must not collide)");
	ZENITH_ASSERT_EQ(ZM_UI_Dex::CellIndexFromElementName(ZM_UI_Party::SlotElementName(0u)), -1,
		"a party slot is not a dex cell");
	ZENITH_ASSERT_EQ(ZM_UI_Dex::CellIndexFromElementName("Menu_DexCell30"), -1,
		"the one-past-the-end cell name resolves to no cell");
	ZENITH_ASSERT_EQ(ZM_UI_Dex::CellIndexFromElementName(ZM_UI_Dex::szNEXT_NAME), -1,
		"a page button is not a cell -- Present relies on telling them apart");
}

// ---- PageCount --------------------------------------------------------------

ZENITH_TEST(ZM_DexScreen, PageCount_IsNeverZeroAndRoundsUp)
{
	ZENITH_ASSERT_EQ(ZM_UI_Dex::PageCount(0u), 1u,
		"an empty dex still shows ONE blank page (0 pages would leave ClampPage nothing to clamp to)");
	ZENITH_ASSERT_EQ(ZM_UI_Dex::PageCount(1u), 1u, "a single entry fits on one page");
	ZENITH_ASSERT_EQ(ZM_UI_Dex::PageCount(ZM_UI_Dex::uCELL_COUNT), 1u,
		"an exactly-full page is ONE page, not two");
	ZENITH_ASSERT_EQ(ZM_UI_Dex::PageCount(ZM_UI_Dex::uCELL_COUNT + 1u), 2u,
		"one entry past a full page opens a second page");
	ZENITH_ASSERT_EQ(ZM_UI_Dex::PageCount(ZM_UI_Dex::uCELL_COUNT * 3u), 3u,
		"three exactly-full pages are three pages");
}

ZENITH_TEST(ZM_DexScreen, PageCount_MatchesTheRealDexSize)
{
	ZENITH_ASSERT_EQ(ZM_UI_Dex::PageCount(uDEX_SIZE), PagesByWalk(),
		"the live dex (%u species, %u per page) pages exactly", uDEX_SIZE, ZM_UI_Dex::uCELL_COUNT);
	ZENITH_ASSERT_GT(PagesByWalk(), 1u,
		"the dex must span more than one page or every paging assertion below is vacuous");
}

// ---- ClampPage --------------------------------------------------------------

ZENITH_TEST(ZM_DexScreen, ClampPage_ClampsBothEnds)
{
	const int iLast = static_cast<int>(LastPage());

	ZENITH_ASSERT_EQ(ZM_UI_Dex::ClampPage(-1, uDEX_SIZE), 0, "a negative page clamps to the first");
	ZENITH_ASSERT_EQ(ZM_UI_Dex::ClampPage(-9999, uDEX_SIZE), 0, "...however negative");
	ZENITH_ASSERT_EQ(ZM_UI_Dex::ClampPage(iLast + 1, uDEX_SIZE), iLast,
		"one page past the end clamps to the last page");
	ZENITH_ASSERT_EQ(ZM_UI_Dex::ClampPage(9999, uDEX_SIZE), iLast, "...however far past");
	ZENITH_ASSERT_EQ(ZM_UI_Dex::ClampPage(0, uDEX_SIZE), 0, "an in-range page is unchanged");
	ZENITH_ASSERT_EQ(ZM_UI_Dex::ClampPage(1, uDEX_SIZE), 1, "...at either end of the range");
	ZENITH_ASSERT_EQ(ZM_UI_Dex::ClampPage(iLast, uDEX_SIZE), iLast, "...including the last page itself");
	ZENITH_ASSERT_EQ(ZM_UI_Dex::ClampPage(3, 0u), 0,
		"an empty dex clamps everything onto its single blank page");
}

// ---- SpeciesIndexForCell ----------------------------------------------------

ZENITH_TEST(ZM_DexScreen, SpeciesIndexForCell_MapsPagesRowMajor)
{
	ZENITH_ASSERT_EQ(ZM_UI_Dex::SpeciesIndexForCell(0u, 0u, uDEX_SIZE), 0,
		"page 0 cell 0 is the first dex entry");
	ZENITH_ASSERT_EQ(ZM_UI_Dex::SpeciesIndexForCell(0u, 1u, uDEX_SIZE), 1,
		"cells advance one entry at a time within a page");
	ZENITH_ASSERT_EQ(ZM_UI_Dex::SpeciesIndexForCell(1u, 0u, uDEX_SIZE),
		static_cast<int>(ZM_UI_Dex::uCELL_COUNT), "page 1 starts a whole page in");
	ZENITH_ASSERT_EQ(ZM_UI_Dex::SpeciesIndexForCell(1u, 2u, uDEX_SIZE),
		static_cast<int>(ZM_UI_Dex::uCELL_COUNT + 2u), "...and keeps its offset within the page");

	// The LAST valid entry lands on the last live cell of the trailing page.
	ZENITH_ASSERT_EQ(ZM_UI_Dex::SpeciesIndexForCell(LastPage(), LastPageCellCount() - 1u, uDEX_SIZE),
		static_cast<int>(uDEX_SIZE - 1u), "the final dex entry sits on the last live cell");
}

ZENITH_TEST(ZM_DexScreen, SpeciesIndexForCell_RejectsDeadAndOutOfRangeCells)
{
	// The FIRST cell past the end of the dex on the trailing page.
	ZENITH_ASSERT_EQ(ZM_UI_Dex::SpeciesIndexForCell(LastPage(), LastPageCellCount(), uDEX_SIZE), -1,
		"the first cell past the end of the dex maps to no species");
	ZENITH_ASSERT_EQ(ZM_UI_Dex::SpeciesIndexForCell(LastPage(), ZM_UI_Dex::uCELL_COUNT - 1u, uDEX_SIZE),
		(LastPageCellCount() == ZM_UI_Dex::uCELL_COUNT)
			? static_cast<int>(uDEX_SIZE - 1u)
			: -1,
		"the trailing page's last WIDGET is live only when the page is exactly full");
	ZENITH_ASSERT_EQ(ZM_UI_Dex::SpeciesIndexForCell(0u, ZM_UI_Dex::uCELL_COUNT, uDEX_SIZE), -1,
		"an out-of-range cell index maps to no species");
	ZENITH_ASSERT_EQ(ZM_UI_Dex::SpeciesIndexForCell(LastPage() + 1u, 0u, uDEX_SIZE), -1,
		"a page past the end maps to no species");
	ZENITH_ASSERT_EQ(ZM_UI_Dex::SpeciesIndexForCell(0u, 0u, 0u), -1,
		"an empty dex's blank page maps nothing (never index 0 into an empty table)");
}

// ---- VisibleCellCount -------------------------------------------------------

ZENITH_TEST(ZM_DexScreen, VisibleCellCount_FullPagesTrailingPageAndPastTheEnd)
{
	for (u_int u = 0u; u < LastPage(); ++u)
	{
		ZENITH_ASSERT_EQ(ZM_UI_Dex::VisibleCellCount(u, uDEX_SIZE), ZM_UI_Dex::uCELL_COUNT,
			"page %u is a FULL page", u);
	}
	ZENITH_ASSERT_EQ(ZM_UI_Dex::VisibleCellCount(LastPage(), uDEX_SIZE), LastPageCellCount(),
		"the trailing page shows exactly the remainder");
	ZENITH_ASSERT_EQ(ZM_UI_Dex::VisibleCellCount(LastPage() + 1u, uDEX_SIZE), 0u,
		"a page past the end shows nothing");
	ZENITH_ASSERT_EQ(ZM_UI_Dex::VisibleCellCount(0u, 0u), 0u,
		"an empty dex's blank page shows nothing");

	// The per-page counts must sum back to the whole dex -- a dropped or double-counted
	// entry fails here even if every individual page happened to look plausible.
	u_int uTotal = 0u;
	for (u_int u = 0u; u < PagesByWalk(); ++u)
	{
		uTotal += ZM_UI_Dex::VisibleCellCount(u, uDEX_SIZE);
	}
	ZENITH_ASSERT_EQ(uTotal, uDEX_SIZE, "every dex entry is reachable on exactly one page");
}

// ---- FormatCell / FormatCompletion -----------------------------------------

ZENITH_TEST(ZM_DexScreen, FormatCell_CaughtCarriesTheZeroPaddedNumberAndTheName)
{
	const ZM_SPECIES_ID eFirst = static_cast<ZM_SPECIES_ID>(0u);
	const std::string strFirst = ZM_UI_Dex::FormatCell(eFirst, true);
	ZENITH_ASSERT_TRUE(Contains(strFirst, "#001"),
		"the first entry is dex number 001, zero-padded ('%s')", strFirst.c_str());
	ZENITH_ASSERT_TRUE(Contains(strFirst, ZM_GetSpeciesName(eFirst)),
		"a caught entry names its species ('%s')", strFirst.c_str());

	// The padding must shrink as the number grows -- a fixed "00" prefix would fail here.
	const std::string strTenth = ZM_UI_Dex::FormatCell(static_cast<ZM_SPECIES_ID>(9u), true);
	ZENITH_ASSERT_TRUE(Contains(strTenth, "#010"),
		"a two-digit dex number carries ONE leading zero ('%s')", strTenth.c_str());
	const std::string strHundredth = ZM_UI_Dex::FormatCell(static_cast<ZM_SPECIES_ID>(99u), true);
	ZENITH_ASSERT_TRUE(Contains(strHundredth, "#100"),
		"a three-digit dex number carries NO leading zero ('%s')", strHundredth.c_str());
}

ZENITH_TEST(ZM_DexScreen, FormatCell_UncaughtShowsTheNumberButHidesTheName)
{
	// Hiding the name until the species is caught is the whole point of a dex, so the
	// ABSENCE is the assertion that matters here.
	const ZM_SPECIES_ID eFirst = static_cast<ZM_SPECIES_ID>(0u);
	const std::string strHidden = ZM_UI_Dex::FormatCell(eFirst, false);
	ZENITH_ASSERT_TRUE(Contains(strHidden, "#001"),
		"an unseen entry still shows its dex number ('%s')", strHidden.c_str());
	ZENITH_ASSERT_FALSE(Contains(strHidden, ZM_GetSpeciesName(eFirst)),
		"an unseen entry must NOT leak its species name ('%s')", strHidden.c_str());
	ZENITH_ASSERT_FALSE(strHidden == ZM_UI_Dex::FormatCell(eFirst, true),
		"the caught and uncaught labels must differ");
}

ZENITH_TEST(ZM_DexScreen, FormatCompletion_CarriesBothNumbers)
{
	// Assert the WHOLE string, never a lone digit: "Dex 7/152" contains a bare "7" only by
	// accident of today's roster, so a digit search would start passing on the TOTAL the
	// moment the dex grows (170 species satisfies "7", 200 satisfies "0") -- silent rot in
	// the one assertion that pins the caught count.
	const std::string strTotal = std::to_string(uDEX_SIZE);

	const std::string strHeader = ZM_UI_Dex::FormatCompletion(7u, uDEX_SIZE);
	const std::string strWantSeven = "Dex 7/" + strTotal;
	ZENITH_ASSERT_STREQ(strHeader.c_str(), strWantSeven.c_str(),
		"the header carries the caught count AND the dex total");

	const std::string strEmpty = ZM_UI_Dex::FormatCompletion(0u, uDEX_SIZE);
	const std::string strWantZero = "Dex 0/" + strTotal;
	ZENITH_ASSERT_STREQ(strEmpty.c_str(), strWantZero.c_str(),
		"a fresh save reads zero caught");
	ZENITH_ASSERT_FALSE(strEmpty == strHeader, "the header tracks the caught count");
}

// ---- The Confirm paging state machine --------------------------------------

ZENITH_TEST(ZM_DexScreen, Fresh_StartsOnTheFirstPageAndCell)
{
	ZM_UI_Dex xScreen;
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 0, "a fresh dex screen opens on the first page");
	ZENITH_ASSERT_EQ(xScreen.GetCursor(), 0, "...on the first cell");
}

ZENITH_TEST(ZM_DexScreen, Confirm_NextAdvancesAndPrevGoesBack)
{
	ZM_UI_Dex xScreen;
	ZENITH_ASSERT_TRUE(xScreen.Confirm(ZM_UI_Dex::szNEXT_NAME, uDEX_SIZE),
		"confirming Next off page 0 pages forward");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 1, "...to page 1");
	ZENITH_ASSERT_TRUE(xScreen.Confirm(ZM_UI_Dex::szNEXT_NAME, uDEX_SIZE), "and again");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 2, "...to page 2");

	ZENITH_ASSERT_TRUE(xScreen.Confirm(ZM_UI_Dex::szPREV_NAME, uDEX_SIZE),
		"confirming Prev pages back");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 1, "...one page at a time");
}

ZENITH_TEST(ZM_DexScreen, Confirm_ClampsAtBothEnds)
{
	ZM_UI_Dex xScreen;
	ZENITH_ASSERT_FALSE(xScreen.Confirm(ZM_UI_Dex::szPREV_NAME, uDEX_SIZE),
		"Prev on page 0 reports no change");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 0, "...and never goes negative");

	const int iLast = static_cast<int>(LastPage());
	for (int i = 0; i < iLast; ++i)
	{
		ZENITH_ASSERT_TRUE(xScreen.Confirm(ZM_UI_Dex::szNEXT_NAME, uDEX_SIZE),
			"paging forward to the last page (step %d)", i);
	}
	ZENITH_ASSERT_EQ(xScreen.GetPage(), iLast, "the walk really reached the last page");
	ZENITH_ASSERT_FALSE(xScreen.Confirm(ZM_UI_Dex::szNEXT_NAME, uDEX_SIZE),
		"Next on the LAST page reports no change");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), iLast, "...and never runs past the end");
}

ZENITH_TEST(ZM_DexScreen, Confirm_ACellAndANullNameAreInert)
{
	// A cell has no per-species detail panel yet (out of scope for SC5), so confirming
	// one must be a silent no-op rather than paging or crashing.
	ZM_UI_Dex xScreen;
	ZENITH_ASSERT_TRUE(xScreen.Confirm(ZM_UI_Dex::szNEXT_NAME, uDEX_SIZE), "park on page 1");

	ZENITH_ASSERT_FALSE(xScreen.Confirm(ZM_UI_Dex::CellElementName(0u), uDEX_SIZE),
		"confirming a cell changes nothing");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 1, "...the page is untouched");
	ZENITH_ASSERT_FALSE(xScreen.Confirm(nullptr, uDEX_SIZE),
		"a null focused name (nothing focused) changes nothing");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 1, "...the page is still untouched");
	ZENITH_ASSERT_FALSE(xScreen.Confirm(ZM_UI_MenuStack::szROOT_DEX_NAME, uDEX_SIZE),
		"a foreign element name changes nothing");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 1, "...nor does a foreign name move the page");
}

ZENITH_TEST(ZM_DexScreen, Reset_ReturnsAMidFlightScreenToFresh)
{
	ZM_UI_Dex xScreen;
	ZENITH_ASSERT_TRUE(xScreen.Confirm(ZM_UI_Dex::szNEXT_NAME, uDEX_SIZE), "page off the first page");
	Zenith_Entity xNoRoot;
	xScreen.Hide(xNoRoot);   // ...and clear the cursor, so BOTH fields are mid-flight
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 1, "the fixture really is mid-flight");
	ZENITH_ASSERT_EQ(xScreen.GetCursor(), -1, "...on both fields");

	xScreen.Reset();
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 0, "Reset returns to the first page");
	ZENITH_ASSERT_EQ(xScreen.GetCursor(), 0, "Reset returns the cursor to the first cell");
}

// ---- Presentation seam (headless half) -------------------------------------

ZENITH_TEST(ZM_DexScreen, Present_OnInvalidRootIsABestEffortNoOp)
{
	// Present short-circuits on an INVALID root handle before it touches any scene (and
	// before the grid build-once), so the guard itself is headlessly testable. The
	// contract pinned here is that presentation never disturbs the paging model.
	ZM_UI_Dex xScreen;
	ZENITH_ASSERT_TRUE(xScreen.Confirm(ZM_UI_Dex::szNEXT_NAME, uDEX_SIZE), "park on page 1");

	ZM_SpeciesSet xCaught;
	xCaught.Mark(static_cast<ZM_SPECIES_ID>(0u));
	ZENITH_ASSERT_EQ(xCaught.Count(), 1u, "the fixture set really holds one caught species");

	Zenith_Entity xNoRoot;
	ZENITH_ASSERT_FALSE(xNoRoot.IsValid(), "a default-constructed entity handle is invalid");
	xScreen.Present(xNoRoot, xCaught);

	ZENITH_ASSERT_EQ(xScreen.GetPage(), 1, "presenting to a missing root never moves the page");
	ZENITH_ASSERT_EQ(xScreen.GetCursor(), 0, "...nor the cursor");
}

ZENITH_TEST(ZM_DexScreen, Hide_ClearsTheCursorButKeepsThePage)
{
	// Hide means "not presented", which is exactly what GetCursor's -1 contracts; the
	// PAGE is session state the menu stack drops through Reset, not here.
	ZM_UI_Dex xScreen;
	ZENITH_ASSERT_TRUE(xScreen.Confirm(ZM_UI_Dex::szNEXT_NAME, uDEX_SIZE), "park on page 1");

	Zenith_Entity xNoRoot;
	xScreen.Hide(xNoRoot);
	ZENITH_ASSERT_EQ(xScreen.GetCursor(), -1, "a hidden screen focuses no cell");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 1, "Hide does not drop the session page (Reset owns that)");

	// ...and a hidden screen still pages by name: the page is model state, not a widget.
	ZENITH_ASSERT_TRUE(xScreen.Confirm(ZM_UI_Dex::szPREV_NAME, uDEX_SIZE),
		"the paging model keeps working while nothing is drawn");
	ZENITH_ASSERT_EQ(xScreen.GetPage(), 0, "...back to the first page");
}
