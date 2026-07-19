#include "Zenith.h"

// ============================================================================
// ZM_Tests_PartyScreen -- S6 item 2 (SC4) unit tests for ZM_UI_Party, the
// overworld party screen: the authored slot-name contract, the visible-slot
// count, the row / summary formatters, and the confirm-cancel state machine,
// plus the headless half of the presentation seam (it short-circuits before it
// touches a scene). Everything here is PURE -- no ECS, no scene, no graphics, no
// baked assets -- so every fixture is deterministic and hermetic and no
// RequestSkip is needed. Category ZM_PartyScreen.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "ZenithECS/Zenith_Entity.h"                    // the invalid-handle presentation guard
#include "Zenithmon/Components/ZM_UI_MenuStack.h"       // the host's ROOT names (near-miss fixtures)
#include "Zenithmon/Source/Battle/ZM_BattleTypes.h"     // uZM_MAX_PARTY_SIZE / uZM_MAX_MOVES
#include "Zenithmon/Source/Data/ZM_AbilityData.h"       // ZM_GetAbilityName
#include "Zenithmon/Source/Data/ZM_MoveData.h"          // ZM_GetMoveName / ZM_MOVE_NONE
#include "Zenithmon/Source/Data/ZM_NatureData.h"        // ZM_GetNatureName
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"       // ZM_GetSpeciesName
#include "Zenithmon/Source/Party/ZM_Monster.h"
#include "Zenithmon/Source/Party/ZM_Party.h"
#include "Zenithmon/Source/UI/ZM_UI_BattleHUD.h"        // FormatHpPanel (the row's shared core)
#include "Zenithmon/Source/UI/ZM_UI_Party.h"

#include <string>

// The screen must carry exactly one widget per party slot -- asserted in the
// production TU too, and re-asserted here so the contract fails at the test that
// depends on it rather than silently hiding a member.
static_assert(ZM_UI_Party::uMAX_SLOTS == uZM_MAX_PARTY_SIZE,
	"ZM_UI_Party::uMAX_SLOTS must match the party capacity");

namespace
{
	// The shared record fixture: the starter species, at a level high enough that the
	// derived learnset has taught it a FULL four-move set (the level-up entries are
	// spread across 1..50, so a level-5 starter would know exactly one move and the
	// move-line assertions below would be near-vacuous).
	constexpr ZM_SPECIES_ID eFIXTURE_SPECIES = ZM_SPECIES_FERNFAWN;
	constexpr u_int uFIXTURE_LEVEL = 50u;

	// Lines in a '\n'-separated body (a body with no newline is one line).
	u_int CountLines(const std::string& strBody)
	{
		u_int uLines = 1u;
		for (const char c : strBody)
		{
			if (c == '\n') { ++uLines; }
		}
		return uLines;
	}

	u_int CountFilledMoves(const ZM_Monster& xRecord)
	{
		u_int uFilled = 0u;
		for (u_int u = 0u; u < uZM_MAX_MOVES; ++u)
		{
			if (xRecord.m_axMoves[u].m_eMove != ZM_MOVE_NONE) { ++uFilled; }
		}
		return uFilled;
	}

	bool Contains(const std::string& strBody, const char* szNeedle)
	{
		return strBody.find(szNeedle) != std::string::npos;
	}
}

// ---- SlotElementName --------------------------------------------------------

ZENITH_TEST(ZM_PartyScreen, SlotElementName_EveryInRangeSlotIsDistinctAndNonEmpty)
{
	for (u_int u = 0u; u < ZM_UI_Party::uMAX_SLOTS; ++u)
	{
		const char* szName = ZM_UI_Party::SlotElementName(u);
		ZENITH_ASSERT_NOT_NULL(szName, "slot %u has a name", u);
		ZENITH_ASSERT_TRUE(szName[0] != '\0', "slot %u's name is not the empty string", u);
		for (u_int v = 0u; v < u; ++v)
		{
			ZENITH_ASSERT_FALSE(std::string(szName) == ZM_UI_Party::SlotElementName(v),
				"slot %u and slot %u must not share an element name", u, v);
		}
	}
}

ZENITH_TEST(ZM_PartyScreen, SlotElementName_OutOfRangeIsEmpty)
{
	ZENITH_ASSERT_STREQ(ZM_UI_Party::SlotElementName(ZM_UI_Party::uMAX_SLOTS), "",
		"the first out-of-range slot has no authored element");
	ZENITH_ASSERT_STREQ(ZM_UI_Party::SlotElementName(99u), "",
		"a wildly out-of-range slot has no authored element (never a dangling pointer)");
}

// ---- SlotIndexFromElementName ----------------------------------------------

ZENITH_TEST(ZM_PartyScreen, SlotIndex_RoundTripsEverySlot)
{
	for (u_int u = 0u; u < ZM_UI_Party::uMAX_SLOTS; ++u)
	{
		ZENITH_ASSERT_EQ(ZM_UI_Party::SlotIndexFromElementName(ZM_UI_Party::SlotElementName(u)),
			static_cast<int>(u), "slot %u round-trips through its element name", u);
	}
}

ZENITH_TEST(ZM_PartyScreen, SlotIndex_NullEmptyAndForeignNamesAreNegative)
{
	ZENITH_ASSERT_EQ(ZM_UI_Party::SlotIndexFromElementName(nullptr), -1,
		"a null name is not a party slot");
	ZENITH_ASSERT_EQ(ZM_UI_Party::SlotIndexFromElementName(""), -1,
		"the empty name is not a party slot");
	ZENITH_ASSERT_EQ(ZM_UI_Party::SlotIndexFromElementName(ZM_UI_MenuStack::szROOT_PARTY_NAME), -1,
		"the ROOT 'Party' entry is not a party SLOT (the two contracts must not collide)");
	ZENITH_ASSERT_EQ(ZM_UI_Party::SlotIndexFromElementName("Menu_PartySlot6"), -1,
		"a one-past-the-end slot name resolves to no slot");
	ZENITH_ASSERT_EQ(ZM_UI_Party::SlotIndexFromElementName("Menu_PartySlot"), -1,
		"a truncated slot name resolves to no slot");
}

// ---- VisibleSlotCount -------------------------------------------------------

ZENITH_TEST(ZM_PartyScreen, VisibleSlotCount_ClampsToCapacity)
{
	ZENITH_ASSERT_EQ(ZM_UI_Party::VisibleSlotCount(0u), 0u, "an empty party shows no rows");
	ZENITH_ASSERT_EQ(ZM_UI_Party::VisibleSlotCount(1u), 1u, "a lone lead shows one row");
	ZENITH_ASSERT_EQ(ZM_UI_Party::VisibleSlotCount(ZM_UI_Party::uMAX_SLOTS),
		ZM_UI_Party::uMAX_SLOTS, "a full party shows every row");
	ZENITH_ASSERT_EQ(ZM_UI_Party::VisibleSlotCount(ZM_UI_Party::uMAX_SLOTS + 1u),
		ZM_UI_Party::uMAX_SLOTS, "an over-full count clamps to the widget capacity");
	ZENITH_ASSERT_EQ(ZM_UI_Party::VisibleSlotCount(1000u), ZM_UI_Party::uMAX_SLOTS,
		"a wildly over-full count still clamps (never indexes past the widgets)");
}

// ---- FormatPartyRow ---------------------------------------------------------

ZENITH_TEST(ZM_PartyScreen, FormatPartyRow_CarriesSpeciesLevelAndHp)
{
	const std::string strRow = ZM_UI_Party::FormatPartyRow(eFIXTURE_SPECIES, uFIXTURE_LEVEL, 17u, 21u);
	ZENITH_ASSERT_TRUE(Contains(strRow, ZM_GetSpeciesName(eFIXTURE_SPECIES)),
		"the row names the species ('%s')", strRow.c_str());
	ZENITH_ASSERT_TRUE(Contains(strRow, "Lv50"), "the row carries the level ('%s')", strRow.c_str());
	ZENITH_ASSERT_TRUE(Contains(strRow, "17/21"),
		"the row carries the '<cur>/<max>' HP substring ('%s')", strRow.c_str());
	// The row must remain the SHARED battle HP-panel string plus the marker -- never a
	// second, drifting copy of that formatter.
	ZENITH_ASSERT_STREQ(strRow.c_str(),
		ZM_UI_BattleHUD::FormatHpPanel(eFIXTURE_SPECIES, uFIXTURE_LEVEL, 17u, 21u).c_str(),
		"a healthy row is exactly the shared FormatHpPanel string");
}

ZENITH_TEST(ZM_PartyScreen, FormatPartyRow_MarksOnlyAFaintedMember)
{
	const std::string strFainted = ZM_UI_Party::FormatPartyRow(eFIXTURE_SPECIES, uFIXTURE_LEVEL, 0u, 21u);
	ZENITH_ASSERT_TRUE(Contains(strFainted, "FAINTED"),
		"a member at 0 HP is marked fainted ('%s')", strFainted.c_str());

	const std::string strBarelyAlive = ZM_UI_Party::FormatPartyRow(eFIXTURE_SPECIES, uFIXTURE_LEVEL, 1u, 21u);
	ZENITH_ASSERT_FALSE(Contains(strBarelyAlive, "FAINTED"),
		"a member at 1 HP is NOT marked fainted ('%s')", strBarelyAlive.c_str());
}

// ---- FormatSummary ----------------------------------------------------------

ZENITH_TEST(ZM_PartyScreen, FormatSummary_HasTheRowTheLabelsAndOneLinePerMove)
{
	const ZM_Monster xRecord = ZM_BuildMonsterRecord(eFIXTURE_SPECIES, uFIXTURE_LEVEL);
	const std::string strSummary = ZM_UI_Party::FormatSummary(xRecord);

	ZENITH_ASSERT_TRUE(Contains(strSummary, ZM_UI_Party::FormatPartyRow(
			xRecord.m_eSpecies, xRecord.m_uLevel, xRecord.m_uCurrentHp, xRecord.GetMaxHP()).c_str()),
		"the summary opens with the list row ('%s')", strSummary.c_str());
	ZENITH_ASSERT_TRUE(Contains(strSummary, "Nature: "), "the summary labels the nature");
	ZENITH_ASSERT_TRUE(Contains(strSummary, ZM_GetNatureName(xRecord.m_eNature)),
		"the summary resolves the nature through the real table accessor");
	ZENITH_ASSERT_TRUE(Contains(strSummary, "Ability: "), "the summary labels the ability");
	ZENITH_ASSERT_TRUE(Contains(strSummary, ZM_GetAbilityName(xRecord.m_eAbility)),
		"the summary resolves the ability through the real table accessor");
	ZENITH_ASSERT_TRUE(Contains(strSummary, "Status: "), "the summary labels the status");

	const u_int uFilled = CountFilledMoves(xRecord);
	ZENITH_ASSERT_GT(uFilled, 0u,
		"the fixture record must know at least one move (the move-line assertions would be vacuous)");
	for (u_int u = 0u; u < uZM_MAX_MOVES; ++u)
	{
		const ZM_MoveSlot& xSlot = xRecord.m_axMoves[u];
		if (xSlot.m_eMove == ZM_MOVE_NONE) { continue; }
		ZENITH_ASSERT_TRUE(Contains(strSummary, ZM_GetMoveName(xSlot.m_eMove)),
			"the summary names move slot %u", u);
	}
	// Exactly 4 header lines + one per filled move: a dropped or duplicated line fails.
	ZENITH_ASSERT_EQ(CountLines(strSummary), 4u + uFilled,
		"the summary is 4 header lines plus one line per non-empty move slot ('%s')",
		strSummary.c_str());
}

ZENITH_TEST(ZM_PartyScreen, FormatSummary_MapsTheMajorStatusToItsLabel)
{
	// There is no ZM_Get*Name accessor for ZM_MAJOR_STATUS, so the summary owns that
	// mapping itself -- and every fixture record is built healthy, so without this the
	// whole switch could be deleted for a constant "OK" and stay green.
	ZM_Monster xRecord = ZM_BuildMonsterRecord(eFIXTURE_SPECIES, uFIXTURE_LEVEL);
	ZENITH_ASSERT_TRUE(xRecord.m_eStatus == ZM_MAJOR_STATUS_NONE,
		"a freshly built record starts with no major status");
	const std::string strHealthy = ZM_UI_Party::FormatSummary(xRecord);
	ZENITH_ASSERT_TRUE(Contains(strHealthy, "Status: OK"),
		"an unafflicted member reads as healthy ('%s')", strHealthy.c_str());
	const u_int uLines = CountLines(strHealthy);

	xRecord.m_eStatus = ZM_MAJOR_STATUS_BURN;
	const std::string strBurned = ZM_UI_Party::FormatSummary(xRecord);
	ZENITH_ASSERT_TRUE(Contains(strBurned, "Status: Burned"),
		"a burned member carries the burn label ('%s')", strBurned.c_str());
	ZENITH_ASSERT_FALSE(Contains(strBurned, "Status: OK"),
		"...and no longer reads as healthy ('%s')", strBurned.c_str());

	xRecord.m_eStatus = ZM_MAJOR_STATUS_TOXIC;
	const std::string strToxic = ZM_UI_Party::FormatSummary(xRecord);
	ZENITH_ASSERT_TRUE(Contains(strToxic, "Status: Badly Poisoned"),
		"toxic is distinguished from ordinary poison ('%s')", strToxic.c_str());
	ZENITH_ASSERT_FALSE(Contains(strToxic, "Status: Poisoned"),
		"...and does not collapse onto the plain poison label ('%s')", strToxic.c_str());

	// The label is one FIELD on the status line, never an extra line.
	ZENITH_ASSERT_EQ(CountLines(strBurned), uLines, "a status label adds no line ('%s')", strBurned.c_str());
	ZENITH_ASSERT_EQ(CountLines(strToxic), uLines, "a status label adds no line ('%s')", strToxic.c_str());
}

ZENITH_TEST(ZM_PartyScreen, FormatSummary_MovelessRecordIsJustTheHeader)
{
	ZM_Monster xRecord = ZM_BuildMonsterRecord(eFIXTURE_SPECIES, uFIXTURE_LEVEL);
	for (u_int u = 0u; u < uZM_MAX_MOVES; ++u)
	{
		xRecord.m_axMoves[u] = ZM_MoveSlot{};   // ZM_MOVE_NONE, 0/0 PP
	}
	const std::string strSummary = ZM_UI_Party::FormatSummary(xRecord);
	ZENITH_ASSERT_EQ(CountLines(strSummary), 4u,
		"a record with no moves yields exactly the four header lines ('%s')", strSummary.c_str());
	ZENITH_ASSERT_FALSE(Contains(strSummary, "  PP "),
		"no move line is emitted for an empty slot ('%s')", strSummary.c_str());
}

ZENITH_TEST(ZM_PartyScreen, FormatSummary_GappedMovesetStillListsEveryFilledSlot)
{
	// The moveset can be GAPPED (an empty slot before a filled one); the summary must
	// skip holes, not stop at the first one.
	ZM_Monster xRecord = ZM_BuildMonsterRecord(eFIXTURE_SPECIES, uFIXTURE_LEVEL);
	ZENITH_ASSERT_GT(CountFilledMoves(xRecord), 1u,
		"the fixture needs at least two moves to punch a hole in");
	const ZM_MoveSlot xKept = xRecord.m_axMoves[1];
	xRecord.m_axMoves[0] = ZM_MoveSlot{};   // hole in slot 0, a real move still in slot 1

	const std::string strSummary = ZM_UI_Party::FormatSummary(xRecord);
	ZENITH_ASSERT_TRUE(Contains(strSummary, ZM_GetMoveName(xKept.m_eMove)),
		"the move behind the hole is still listed ('%s')", strSummary.c_str());
	ZENITH_ASSERT_EQ(CountLines(strSummary), 4u + CountFilledMoves(xRecord),
		"the line count still matches the filled-slot count ('%s')", strSummary.c_str());
}

// ---- The Confirm / Cancel state machine ------------------------------------

ZENITH_TEST(ZM_PartyScreen, Fresh_SummaryClosedOnTheFirstSlot)
{
	ZM_UI_Party xScreen;
	ZENITH_ASSERT_FALSE(xScreen.IsSummaryOpen(), "a fresh party screen shows no summary");
	ZENITH_ASSERT_EQ(xScreen.GetCursor(), 0, "a fresh party screen sits on the first slot");
}

ZENITH_TEST(ZM_PartyScreen, Confirm_TogglesTheSummary)
{
	ZM_UI_Party xScreen;
	xScreen.Confirm();
	ZENITH_ASSERT_TRUE(xScreen.IsSummaryOpen(), "confirm on the list opens the member summary");
	xScreen.Confirm();
	ZENITH_ASSERT_FALSE(xScreen.IsSummaryOpen(),
		"confirm again closes it (a toggle -- there is no per-member action menu yet)");
	xScreen.Confirm();
	ZENITH_ASSERT_TRUE(xScreen.IsSummaryOpen(), "the toggle keeps working across presses");
}

ZENITH_TEST(ZM_PartyScreen, Cancel_IsConsumedOnlyWhileTheSummaryIsOpen)
{
	ZM_UI_Party xScreen;
	ZENITH_ASSERT_FALSE(xScreen.Cancel(),
		"cancel on the list is NOT consumed -- the menu stack pops the party screen");
	ZENITH_ASSERT_FALSE(xScreen.IsSummaryOpen(), "an unconsumed cancel leaves the model alone");

	xScreen.Confirm();
	ZENITH_ASSERT_TRUE(xScreen.Cancel(),
		"cancel with the summary open IS consumed (the stack must not pop)");
	ZENITH_ASSERT_FALSE(xScreen.IsSummaryOpen(), "...and the summary is now closed");
	ZENITH_ASSERT_FALSE(xScreen.Cancel(), "the NEXT cancel falls through to the stack");
}

ZENITH_TEST(ZM_PartyScreen, Reset_ReturnsAMidFlightScreenToFresh)
{
	ZM_UI_Party xScreen;
	xScreen.Confirm();   // summary open
	ZENITH_ASSERT_TRUE(xScreen.IsSummaryOpen(), "the fixture really is mid-flight");

	xScreen.Reset();
	ZENITH_ASSERT_FALSE(xScreen.IsSummaryOpen(), "Reset closes the summary");
	ZENITH_ASSERT_EQ(xScreen.GetCursor(), 0, "Reset returns the cursor to the first slot");
}

// ---- Presentation seam (headless half) -------------------------------------

ZENITH_TEST(ZM_PartyScreen, Present_OnInvalidRootIsABestEffortNoOp)
{
	// Both presentation methods short-circuit on an INVALID root handle before they
	// touch any scene, so the guard itself is headlessly testable. The contract pinned
	// here is that presentation NEVER mutates the model.
	ZM_UI_Party xScreen;
	xScreen.Confirm();   // summary open, cursor 0

	ZM_Party xParty;
	ZENITH_ASSERT_TRUE(xParty.Add(ZM_BuildMonsterRecord(eFIXTURE_SPECIES, uFIXTURE_LEVEL)),
		"the fixture party takes its lead");

	Zenith_Entity xNoRoot;
	ZENITH_ASSERT_FALSE(xNoRoot.IsValid(), "a default-constructed entity handle is invalid");
	xScreen.Present(xNoRoot, xParty);

	ZENITH_ASSERT_TRUE(xScreen.IsSummaryOpen(), "presenting to a missing root never closes the summary");
	ZENITH_ASSERT_EQ(xScreen.GetCursor(), 0,
		"presenting a NON-empty party to a missing root never moves the cursor");
}

ZENITH_TEST(ZM_PartyScreen, Hide_ClearsTheCursorButKeepsTheSessionState)
{
	// Hide means "not presented", which is exactly what GetCursor's -1 contracts; the
	// summary FLAG is session state the menu stack drops through Reset, not here.
	ZM_UI_Party xScreen;
	xScreen.Confirm();
	ZENITH_ASSERT_EQ(xScreen.GetCursor(), 0, "the fixture starts on a real slot");

	Zenith_Entity xNoRoot;
	xScreen.Hide(xNoRoot);
	ZENITH_ASSERT_EQ(xScreen.GetCursor(), -1, "a hidden screen focuses no slot");
	ZENITH_ASSERT_TRUE(xScreen.IsSummaryOpen(), "Hide does not drop the session state (Reset owns that)");

	// ...and with no slot under the cursor, confirm can no longer toggle a summary that
	// nothing is drawing.
	xScreen.Confirm();
	ZENITH_ASSERT_TRUE(xScreen.IsSummaryOpen(), "confirm on a hidden screen changes nothing");
}

ZENITH_TEST(ZM_PartyScreen, Present_OnAnEmptyPartyLeavesNoMemberUnderTheCursor)
{
	// The empty-party verdict is settled from the MODEL (the count) before any UI
	// resolve, so it is reachable headlessly -- ZM_Party::Get asserts past the count, so
	// an empty party must never leave a slot index under the cursor. A cleared cursor
	// must then make confirm inert: otherwise the flag would claim a summary Present
	// suppresses, and the player's first Escape would be eaten closing that invisible
	// summary instead of popping the screen.
	ZM_UI_Party xScreen;
	const ZM_Party xEmpty;
	Zenith_Entity xNoRoot;

	xScreen.Present(xNoRoot, xEmpty);
	ZENITH_ASSERT_EQ(xScreen.GetCursor(), -1, "an empty party leaves no slot under the cursor");

	xScreen.Confirm();
	ZENITH_ASSERT_FALSE(xScreen.IsSummaryOpen(), "confirm cannot open a summary over no member");
	ZENITH_ASSERT_FALSE(xScreen.Cancel(),
		"...so the first cancel pops the screen rather than being swallowed");
}
