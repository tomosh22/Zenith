#include "Zenith.h"

// ============================================================================
// ZM_Tests_BattleHUD -- S5 item-4 (SC4) unit tests for ZM_UI_BattleHUD, the
// battle HUD owned by ZM_BattleDirector. Everything here is PURE: no ECS, no
// scene, no graphics, no baked assets -- just the engine + the HUD's four static
// formatters (FormatBattleLogLine / FormatHpPanel / ComputeHpFraction /
// ComputeVisibleGlyphCount). Every fixture is deterministic and hermetic, so no
// RequestSkip is needed.
//
// The output contract is asserted as SUBSTRINGS for name-bearing log lines (the
// exact copy is presentation and may be re-worded) and exact for the numeric
// surface. The subject species of a MOVE_USED / FAINT line is
// (m_uSide == ZM_SIDE_PLAYER ? playerActive : enemyActive), and an enemy-side
// line starts with the "Foe " prefix.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/UI/ZM_UI_BattleHUD.h"
#include "Zenithmon/Source/Battle/ZM_BattleDirectorCore.h"   // ZM_MapEventToOp (carries-text cross-table lock)
#include "Zenithmon/Source/Battle/ZM_BattleEvent.h"
#include "Zenithmon/Source/Battle/ZM_BattleTypes.h"
#include "Zenithmon/Source/Data/ZM_MoveData.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"

#include <string>

namespace
{
	// The two species used across the fixtures (both confirmed present in the dex).
	constexpr ZM_SPECIES_ID eZM_HUD_PLAYER_SPECIES = ZM_SPECIES_FERNFAWN;
	constexpr ZM_SPECIES_ID eZM_HUD_ENEMY_SPECIES  = ZM_SPECIES_KINDLET;
	// The first non-NONE move in the table -- a real, named move row.
	constexpr ZM_MOVE_ID    eZM_HUD_MOVE           = ZM_MOVE_RAMBASH;

	// Substring / prefix helpers over the returned log lines (the frozen contract
	// asserts substrings, never full strings, for name-bearing lines).
	bool Contains(const std::string& strHaystack, const char* szNeedle)
	{
		return strHaystack.find(szNeedle) != std::string::npos;
	}

	bool StartsWith(const std::string& strHaystack, const char* szPrefix)
	{
		return strHaystack.rfind(szPrefix, 0) == 0;
	}

	// Build one FormatBattleLogLine call with the standard active-species pair so
	// individual tests only vary the event.
	std::string FormatLine(const ZM_BattleEvent& xEvent)
	{
		return ZM_UI_BattleHUD::FormatBattleLogLine(
			xEvent, eZM_HUD_PLAYER_SPECIES, eZM_HUD_ENEMY_SPECIES);
	}
}

// ---- SWITCH_IN --------------------------------------------------------------

ZENITH_TEST(ZM_BattleHUD, Format_SwitchIn_ContainsSpeciesAndAppeared)
{
	const ZM_BattleEvent xEvent = ZM_MakeEvent(
		ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_PLAYER, 0u, ZM_MOVE_NONE, ZM_SPECIES_FERNFAWN);
	const std::string strLine = FormatLine(xEvent);
	ZENITH_ASSERT_TRUE(Contains(strLine, ZM_GetSpeciesName(ZM_SPECIES_FERNFAWN)),
		"SWITCH_IN must name the switched-in species; got \"%s\"", strLine.c_str());
	ZENITH_ASSERT_TRUE(Contains(strLine, "appeared"),
		"SWITCH_IN must say \"appeared\"; got \"%s\"", strLine.c_str());
}

// ---- MOVE_USED --------------------------------------------------------------

ZENITH_TEST(ZM_BattleHUD, Format_MoveUsed_PlayerSubjectMoveAndUsed)
{
	const ZM_BattleEvent xEvent = ZM_MakeEvent(
		ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER, 0u, eZM_HUD_MOVE);
	const std::string strLine = FormatLine(xEvent);
	ZENITH_ASSERT_TRUE(Contains(strLine, ZM_GetSpeciesName(eZM_HUD_PLAYER_SPECIES)),
		"player MOVE_USED must name the player-active species; got \"%s\"", strLine.c_str());
	ZENITH_ASSERT_TRUE(Contains(strLine, ZM_GetMoveName(eZM_HUD_MOVE)),
		"player MOVE_USED must name the move; got \"%s\"", strLine.c_str());
	ZENITH_ASSERT_TRUE(Contains(strLine, "used"),
		"player MOVE_USED must say \"used\"; got \"%s\"", strLine.c_str());
	ZENITH_ASSERT_FALSE(StartsWith(strLine, "Foe "),
		"a player-side line must NOT be Foe-prefixed; got \"%s\"", strLine.c_str());
}

ZENITH_TEST(ZM_BattleHUD, Format_MoveUsed_EnemyStartsWithFoe)
{
	const ZM_BattleEvent xEvent = ZM_MakeEvent(
		ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_ENEMY, 0u, eZM_HUD_MOVE);
	const std::string strLine = FormatLine(xEvent);
	ZENITH_ASSERT_TRUE(StartsWith(strLine, "Foe "),
		"an enemy-side line must start with \"Foe \"; got \"%s\"", strLine.c_str());
	ZENITH_ASSERT_TRUE(Contains(strLine, ZM_GetSpeciesName(eZM_HUD_ENEMY_SPECIES)),
		"enemy MOVE_USED must name the enemy-active species; got \"%s\"", strLine.c_str());
}

// ---- Effectiveness / accuracy / crit / immunity -----------------------------

ZENITH_TEST(ZM_BattleHUD, Format_MoveMissed)
{
	const ZM_BattleEvent xEvent = ZM_MakeEvent(
		ZM_BATTLE_EVENT_MOVE_MISSED, ZM_SIDE_ENEMY, 0u, eZM_HUD_MOVE);
	ZENITH_ASSERT_TRUE(Contains(FormatLine(xEvent), "missed"),
		"MOVE_MISSED must say \"missed\"");
}

ZENITH_TEST(ZM_BattleHUD, Format_Crit)
{
	const ZM_BattleEvent xEvent = ZM_MakeEvent(ZM_BATTLE_EVENT_CRIT, ZM_SIDE_PLAYER);
	ZENITH_ASSERT_TRUE(Contains(FormatLine(xEvent), "critical"),
		"CRIT must say \"critical\"");
}

ZENITH_TEST(ZM_BattleHUD, Format_SuperEffective)
{
	const ZM_BattleEvent xEvent = ZM_MakeEvent(
		ZM_BATTLE_EVENT_SUPER_EFFECTIVE, ZM_SIDE_PLAYER, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 200);
	ZENITH_ASSERT_TRUE(Contains(FormatLine(xEvent), "super effective"),
		"SUPER_EFFECTIVE must say \"super effective\"");
}

ZENITH_TEST(ZM_BattleHUD, Format_NotEffective)
{
	const ZM_BattleEvent xEvent = ZM_MakeEvent(
		ZM_BATTLE_EVENT_NOT_EFFECTIVE, ZM_SIDE_PLAYER, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 50);
	ZENITH_ASSERT_TRUE(Contains(FormatLine(xEvent), "not very effective"),
		"NOT_EFFECTIVE must say \"not very effective\"");
}

ZENITH_TEST(ZM_BattleHUD, Format_Immune)
{
	const ZM_BattleEvent xEvent = ZM_MakeEvent(ZM_BATTLE_EVENT_IMMUNE, ZM_SIDE_PLAYER);
	ZENITH_ASSERT_TRUE(Contains(FormatLine(xEvent), "doesn't affect"),
		"IMMUNE must say \"doesn't affect\"");
}

// ---- FAINT ------------------------------------------------------------------

ZENITH_TEST(ZM_BattleHUD, Format_Faint_EnemySubject)
{
	const ZM_BattleEvent xEvent = ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, ZM_SIDE_ENEMY);
	const std::string strLine = FormatLine(xEvent);
	ZENITH_ASSERT_TRUE(StartsWith(strLine, "Foe "),
		"an enemy-side FAINT must start with \"Foe \"; got \"%s\"", strLine.c_str());
	ZENITH_ASSERT_TRUE(Contains(strLine, ZM_GetSpeciesName(eZM_HUD_ENEMY_SPECIES)),
		"enemy FAINT must name the enemy-active species; got \"%s\"", strLine.c_str());
	ZENITH_ASSERT_TRUE(Contains(strLine, "fainted"),
		"FAINT must say \"fainted\"; got \"%s\"", strLine.c_str());
}

ZENITH_TEST(ZM_BattleHUD, Format_Faint_PlayerNoFoe)
{
	const ZM_BattleEvent xEvent = ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, ZM_SIDE_PLAYER);
	const std::string strLine = FormatLine(xEvent);
	ZENITH_ASSERT_TRUE(Contains(strLine, ZM_GetSpeciesName(eZM_HUD_PLAYER_SPECIES)),
		"player FAINT must name the player-active species; got \"%s\"", strLine.c_str());
	ZENITH_ASSERT_TRUE(Contains(strLine, "fainted"),
		"FAINT must say \"fainted\"; got \"%s\"", strLine.c_str());
	ZENITH_ASSERT_FALSE(StartsWith(strLine, "Foe "),
		"a player-side FAINT must NOT be Foe-prefixed; got \"%s\"", strLine.c_str());
}

// ---- BATTLE_END: the three winner cases are mutually distinct ---------------

ZENITH_TEST(ZM_BattleHUD, Format_BattleEnd_ThreeDistinctBanners)
{
	const std::string strWin = FormatLine(ZM_MakeEvent(
		ZM_BATTLE_EVENT_BATTLE_END, ZM_SIDE_COUNT, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE,
		(int)ZM_SIDE_PLAYER));
	const std::string strLoss = FormatLine(ZM_MakeEvent(
		ZM_BATTLE_EVENT_BATTLE_END, ZM_SIDE_COUNT, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE,
		(int)ZM_SIDE_ENEMY));
	const std::string strDraw = FormatLine(ZM_MakeEvent(
		ZM_BATTLE_EVENT_BATTLE_END, ZM_SIDE_COUNT, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE,
		(int)ZM_SIDE_COUNT));

	ZENITH_ASSERT_FALSE(strWin.empty(),  "BATTLE_END (player win) must be non-empty");
	ZENITH_ASSERT_FALSE(strLoss.empty(), "BATTLE_END (enemy win) must be non-empty");
	ZENITH_ASSERT_FALSE(strDraw.empty(), "BATTLE_END (draw) must be non-empty");

	ZENITH_ASSERT_NE(strWin, strLoss,
		"player-win and enemy-win banners must differ");
	ZENITH_ASSERT_NE(strWin, strDraw,
		"player-win and draw banners must differ");
	ZENITH_ASSERT_NE(strLoss, strDraw,
		"enemy-win and draw banners must differ");
}

// ---- Framing kinds are exactly empty ----------------------------------------

ZENITH_TEST(ZM_BattleHUD, Format_FramingKindsEmpty)
{
	const ZM_BATTLE_EVENT aeFraming[] = {
		ZM_BATTLE_EVENT_BATTLE_BEGIN,
		ZM_BATTLE_EVENT_TURN_BEGIN,
		ZM_BATTLE_EVENT_TURN_END,
		ZM_BATTLE_EVENT_EVOLUTION_QUEUED,
	};
	for (ZM_BATTLE_EVENT eKind : aeFraming)
	{
		const std::string strLine = FormatLine(ZM_MakeEvent(eKind, ZM_SIDE_PLAYER));
		ZENITH_ASSERT_TRUE(strLine.empty(),
			"framing kind %u must format to the empty string; got \"%s\"",
			(u_int)eKind, strLine.c_str());
	}
}

// ---- Totality: every kind formats without crashing / asserting --------------

ZENITH_TEST(ZM_BattleHUD, Format_TotalOverEveryKind_NoCrash)
{
	for (u_int k = 0; k < (u_int)ZM_BATTLE_EVENT_COUNT; ++k)
	{
		const ZM_BattleEvent xEvent = ZM_MakeEvent(
			(ZM_BATTLE_EVENT)k, ZM_SIDE_ENEMY, 0u, eZM_HUD_MOVE, ZM_SPECIES_FERNFAWN,
			(int)ZM_SIDE_PLAYER);
		// Reaching this call for every kind IS the assertion (no crash / no assert).
		const std::string strLine = FormatLine(xEvent);
		(void)strLine;
	}
	ZENITH_ASSERT_TRUE(true,
		"FormatBattleLogLine returned for every ZM_BATTLE_EVENT kind");
}

// ---- FormatHpPanel ----------------------------------------------------------

ZENITH_TEST(ZM_BattleHUD, HpPanel_ContainsNameLevelHp)
{
	const std::string strPanel = ZM_UI_BattleHUD::FormatHpPanel(
		ZM_SPECIES_FERNFAWN, 5u, 18u, 19u);
	ZENITH_ASSERT_TRUE(Contains(strPanel, ZM_GetSpeciesName(ZM_SPECIES_FERNFAWN)),
		"HP panel must name the species; got \"%s\"", strPanel.c_str());
	ZENITH_ASSERT_TRUE(Contains(strPanel, "5"),
		"HP panel must show the level; got \"%s\"", strPanel.c_str());
	ZENITH_ASSERT_TRUE(Contains(strPanel, "18/19"),
		"HP panel must show cur/max HP; got \"%s\"", strPanel.c_str());
}

ZENITH_TEST(ZM_BattleHUD, HpPanel_ZeroHp)
{
	const std::string strPanel = ZM_UI_BattleHUD::FormatHpPanel(
		ZM_SPECIES_FERNFAWN, 5u, 0u, 19u);
	ZENITH_ASSERT_TRUE(Contains(strPanel, "0/19"),
		"a fainted HP panel must show 0/max; got \"%s\"", strPanel.c_str());
}

// ---- ComputeHpFraction ------------------------------------------------------

ZENITH_TEST(ZM_BattleHUD, HpFraction_HalfFullZeroGuardClamp)
{
	const float fFull = ZM_UI_BattleHUD::ComputeHpFraction(19u, 19u);
	ZENITH_ASSERT_EQ_FLOAT(fFull, 1.0f, 1e-4f, "full HP => fraction 1.0");

	const float fHalf = ZM_UI_BattleHUD::ComputeHpFraction(9u, 18u);
	ZENITH_ASSERT_EQ_FLOAT(fHalf, 0.5f, 1e-4f, "9/18 => fraction 0.5");

	const float fZero = ZM_UI_BattleHUD::ComputeHpFraction(0u, 0u);
	ZENITH_ASSERT_EQ_FLOAT(fZero, 0.0f, 1e-4f, "max==0 guard => 0.0, not NaN");
	ZENITH_ASSERT_TRUE(fZero == fZero, "max==0 guard must not produce NaN");

	const float fClamp = ZM_UI_BattleHUD::ComputeHpFraction(25u, 19u);
	ZENITH_ASSERT_EQ_FLOAT(fClamp, 1.0f, 1e-4f, "cur>max clamps to 1.0");
}

// ---- ComputeVisibleGlyphCount ----------------------------------------------

ZENITH_TEST(ZM_BattleHUD, Reveal_InstantIsFull)
{
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::ComputeVisibleGlyphCount(10, 0.0f, true), 10,
		"instant reveal must show the full glyph count");
}

ZENITH_TEST(ZM_BattleHUD, Reveal_TimedRampClamp)
{
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::ComputeVisibleGlyphCount(10, 0.0f, false), 0,
		"timed reveal at t=0 shows nothing");
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::ComputeVisibleGlyphCount(10, 1000000.0f, false), 10,
		"timed reveal after a huge elapse is fully revealed (clamped to total)");
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::ComputeVisibleGlyphCount(10, -5.0f, false), 0,
		"a negative elapsed clamps to 0");

	// A mid elapsed: RATE is an impl constant, so assert only the invariant range.
	const int iMid = ZM_UI_BattleHUD::ComputeVisibleGlyphCount(10, 0.35f, false);
	ZENITH_ASSERT_GE(iMid, 0, "visible glyph count is never negative");
	ZENITH_ASSERT_LE(iMid, 10, "visible glyph count never exceeds the total");
}

// ---- carries-text <-> non-empty cross-table invariant ----------------------

// The HUD's Update scan selects events whose op ZM_MapEventToOp(kind).m_bCarriesText is
// true, then renders them via FormatBattleLogLine. Lock the invariant those two tables
// must jointly satisfy: EVERY text-carrying kind formats to a NON-EMPTY line -- otherwise
// the HUD would pick an event and show a blank log (the exact class of bug the SC4
// instant-mode fix addressed). The reverse is intentionally allowed (e.g. DAMAGE_DEALT
// has a line but is presented via the HP bar, not the text path).
ZENITH_TEST(ZM_BattleHUD, Format_CarriesTextImpliesNonEmpty)
{
	for (u_int k = 0u; k < (u_int)ZM_BATTLE_EVENT_COUNT; ++k)
	{
		const ZM_BattleEvent xEvent = ZM_MakeEvent(
			(ZM_BATTLE_EVENT)k, ZM_SIDE_ENEMY, 0u, eZM_HUD_MOVE, eZM_HUD_ENEMY_SPECIES,
			(int)ZM_SIDE_PLAYER, 0, 0);
		if (ZM_MapEventToOp(xEvent).m_bCarriesText)
		{
			ZENITH_ASSERT_FALSE(FormatLine(xEvent).empty(),
				"a text-carrying event kind must format to a non-empty log line");
		}
	}
}

// ============================================================================
// S5 item 4 (SC5) -- the pure Fight/Run battle-menu state machine on
// ZM_UI_BattleHUD. Four PURE statics (no scene / graphics / core): MenuItemCount,
// MenuMoveCursor, MenuConfirm, MenuCancel. These are the whole decision surface
// the director's per-frame input drive delegates to, so pinning them here means a
// single-key windowed drive is enough to exercise the wiring (the arithmetic is
// proven hermetically). Every fixture is deterministic; no RequestSkip needed.
// ============================================================================

// ---- MenuItemCount ----------------------------------------------------------

ZENITH_TEST(ZM_BattleHUD, HudMenu_ItemCounts)
{
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::MenuItemCount(ZM_BATTLE_MENU_ACTION_ROOT, 4), 2,
		"the action root always offers exactly two items (Fight, Run)");
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::MenuItemCount(ZM_BATTLE_MENU_MOVE_SELECT, 3), 3,
		"move-select offers one item per available move");
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::MenuItemCount(ZM_BATTLE_MENU_HIDDEN, 4), 0,
		"a hidden menu offers no items");
}

// ---- MenuMoveCursor: clamp, no wrap ----------------------------------------

ZENITH_TEST(ZM_BattleHUD, HudMenu_CursorClampsNoWrap)
{
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::MenuMoveCursor(0, +1, 2), 1,
		"+1 from 0 in a 2-item menu advances to 1");
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::MenuMoveCursor(1, +1, 2), 1,
		"+1 from the last item clamps (never wraps to 0)");
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::MenuMoveCursor(1, -1, 2), 0,
		"-1 from 1 moves to 0");
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::MenuMoveCursor(0, -1, 2), 0,
		"-1 from the first item clamps (never wraps to the last)");
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::MenuMoveCursor(0, +1, 0), 0,
		"an empty menu (n<=0) guards the cursor at 0");
}

// ---- MenuConfirm ------------------------------------------------------------

ZENITH_TEST(ZM_BattleHUD, HudMenu_FightOpensMoveSelect)
{
	const bool abSel4[4] = { true, true, true, true };
	const ZM_BattleMenuConfirmResult xResult =
		ZM_UI_BattleHUD::MenuConfirm(ZM_BATTLE_MENU_ACTION_ROOT, 0, abSel4, 4);
	ZENITH_ASSERT_EQ(xResult.m_eKind, ZM_BATTLE_MENU_CONFIRM_OPEN_MOVES,
		"Fight (root cursor 0) opens the move list, not a submit");
	ZENITH_ASSERT_EQ(xResult.m_eNextScreen, ZM_BATTLE_MENU_MOVE_SELECT,
		"Fight advances the menu to MOVE_SELECT");
	ZENITH_ASSERT_EQ(xResult.m_iNextCursor, 0,
		"MOVE_SELECT opens on the first move");
}

ZENITH_TEST(ZM_BattleHUD, HudMenu_RunEmitsRunAction)
{
	const bool abSel4[4] = { true, true, true, true };
	const ZM_BattleMenuConfirmResult xResult =
		ZM_UI_BattleHUD::MenuConfirm(ZM_BATTLE_MENU_ACTION_ROOT, 1, abSel4, 4);
	ZENITH_ASSERT_EQ(xResult.m_eKind, ZM_BATTLE_MENU_CONFIRM_SUBMIT,
		"Run (root cursor 1) submits an action immediately");
	ZENITH_ASSERT_EQ(xResult.m_xAction.m_eKind, ZM_ACTION_RUN,
		"the submitted action is a RUN");
	ZENITH_ASSERT_EQ(xResult.m_eNextScreen, ZM_BATTLE_MENU_HIDDEN,
		"submitting Run hides the menu");
}

ZENITH_TEST(ZM_BattleHUD, HudMenu_MoveSelectEmitsMoveAction)
{
	const bool abSel4[4] = { true, true, true, true };
	const ZM_BattleMenuConfirmResult xResult =
		ZM_UI_BattleHUD::MenuConfirm(ZM_BATTLE_MENU_MOVE_SELECT, 2, abSel4, 4);
	ZENITH_ASSERT_EQ(xResult.m_eKind, ZM_BATTLE_MENU_CONFIRM_SUBMIT,
		"confirming a selectable move submits an action");
	ZENITH_ASSERT_EQ(xResult.m_xAction.m_eKind, ZM_ACTION_MOVE,
		"the submitted action is a MOVE");
	ZENITH_ASSERT_EQ(xResult.m_xAction.m_uMoveSlot, 2u,
		"the submitted move slot is the cursor position");
	ZENITH_ASSERT_EQ(xResult.m_eNextScreen, ZM_BATTLE_MENU_HIDDEN,
		"submitting a move hides the menu");
}

ZENITH_TEST(ZM_BattleHUD, HudMenu_UnselectableMoveStays)
{
	// Slot 1 is unselectable (e.g. no PP); slot 0 is selectable.
	const bool abSel[4] = { true, false, true, true };

	const ZM_BattleMenuConfirmResult xBlocked =
		ZM_UI_BattleHUD::MenuConfirm(ZM_BATTLE_MENU_MOVE_SELECT, 1, abSel, 4);
	ZENITH_ASSERT_EQ(xBlocked.m_eKind, ZM_BATTLE_MENU_CONFIRM_NONE,
		"confirming an unselectable move is a no-op (no submit)");

	const ZM_BattleMenuConfirmResult xOk =
		ZM_UI_BattleHUD::MenuConfirm(ZM_BATTLE_MENU_MOVE_SELECT, 0, abSel, 4);
	ZENITH_ASSERT_EQ(xOk.m_eKind, ZM_BATTLE_MENU_CONFIRM_SUBMIT,
		"a selectable move at the cursor submits");
	ZENITH_ASSERT_EQ(xOk.m_xAction.m_eKind, ZM_ACTION_MOVE,
		"the submitted action is a MOVE");
	ZENITH_ASSERT_EQ(xOk.m_xAction.m_uMoveSlot, 0u,
		"the submitted move slot is 0");
}

// ---- MenuCancel -------------------------------------------------------------

ZENITH_TEST(ZM_BattleHUD, HudMenu_CancelReturnsToRoot)
{
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::MenuCancel(ZM_BATTLE_MENU_MOVE_SELECT),
		ZM_BATTLE_MENU_ACTION_ROOT,
		"cancelling out of the move list returns to the action root");
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::MenuCancel(ZM_BATTLE_MENU_ACTION_ROOT),
		ZM_BATTLE_MENU_ACTION_ROOT,
		"cancelling at the action root stays at the action root");
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::MenuCancel(ZM_BATTLE_MENU_HIDDEN),
		ZM_BATTLE_MENU_HIDDEN,
		"cancelling a hidden menu leaves it hidden");
}

// ============================================================================
// S5 item 5 (SC3) -- the gapped-moveset compaction fix on ZM_UI_BattleHUD. A real
// party lead can carry a SPARSE moveset (filled slots interleaved with ZM_MOVE_NONE
// holes -- e.g. a forgotten move). BuildFilledMoveMenu compacts the filled slots
// into contiguous cursor order and reports each cursor's RAW slot, so a cursor pick
// maps back to the correct underlying move slot; MenuConfirm's optional rawslot
// argument translates the cursor to that raw slot (nullptr keeps the identity map).
// Pure statics; hermetic; no RequestSkip needed.
// ============================================================================

ZENITH_TEST(ZM_BattleHUD, HudMenu_GappedMovesetCompaction)
{
	// A sparse moveset: real moves in RAW slots 0 and 2, holes (ZM_MOVE_NONE) in 1 and 3.
	const ZM_MOVE_ID aeMoves[uZM_MAX_MOVES] = { ZM_MOVE_RAMBASH, ZM_MOVE_NONE, ZM_MOVE_RAMBASH, ZM_MOVE_NONE };
	const u_int      auCurPP[uZM_MAX_MOVES] = { 5u, 0u, 5u, 0u };

	const char* aszName[uZM_MAX_MOVES] = {};
	bool        abSel[uZM_MAX_MOVES]   = {};
	int         aiRaw[uZM_MAX_MOVES]   = {};

	const int n = ZM_UI_BattleHUD::BuildFilledMoveMenu(aeMoves, auCurPP, aszName, abSel, aiRaw);

	// The two filled slots are compacted into cursor order 0,1 -> raw slots 0,2.
	ZENITH_ASSERT_EQ(n, 2, "only the two filled slots are offered");
	ZENITH_ASSERT_EQ(aiRaw[0], 0, "cursor 0 maps to raw slot 0");
	ZENITH_ASSERT_EQ(aiRaw[1], 2, "cursor 1 maps to raw slot 2 (the hole at raw slot 1 is skipped)");
	ZENITH_ASSERT_TRUE(abSel[0] && abSel[1], "both filled moves have PP and are selectable");
	ZENITH_ASSERT_TRUE(aszName[0] != nullptr && aszName[0][0] != '\0', "cursor 0 has a non-empty name");
	ZENITH_ASSERT_TRUE(aszName[1] != nullptr && aszName[1][0] != '\0', "cursor 1 has a non-empty name");

	// Confirming cursor 1 submits a MOVE whose slot is the RAW slot (2), not the cursor.
	const ZM_BattleMenuConfirmResult xR =
		ZM_UI_BattleHUD::MenuConfirm(ZM_BATTLE_MENU_MOVE_SELECT, 1, abSel, n, aiRaw);
	ZENITH_ASSERT_EQ(xR.m_eKind, ZM_BATTLE_MENU_CONFIRM_SUBMIT, "a selectable move submits");
	ZENITH_ASSERT_EQ(xR.m_xAction.m_eKind, ZM_ACTION_MOVE, "the submitted action is a MOVE");
	ZENITH_ASSERT_EQ(xR.m_xAction.m_uMoveSlot, 2u, "the submitted move slot is the RAW slot of the 2nd filled move");

	// Backward-compat: the nullptr rawslot overload maps the cursor straight to the slot.
	const bool abSel4[uZM_MAX_MOVES] = { true, true, true, true };
	const ZM_BattleMenuConfirmResult xIdentity =
		ZM_UI_BattleHUD::MenuConfirm(ZM_BATTLE_MENU_MOVE_SELECT, 2, abSel4, 4);
	ZENITH_ASSERT_EQ(xIdentity.m_eKind, ZM_BATTLE_MENU_CONFIRM_SUBMIT, "the identity path submits");
	ZENITH_ASSERT_EQ(xIdentity.m_xAction.m_uMoveSlot, 2u, "no rawslot arg -> cursor 2 maps straight to slot 2");

	// LEADING gap {_,A,_,B}: filled moves at raw slots 1 and 3 (the first filled slot is
	// NOT slot 0, so the dense cursor index and the raw slot diverge from cursor 0).
	const ZM_MOVE_ID aeLeadGap[uZM_MAX_MOVES] = { ZM_MOVE_NONE, ZM_MOVE_RAMBASH, ZM_MOVE_NONE, ZM_MOVE_RAMBASH };
	const u_int      auLeadGapPP[uZM_MAX_MOVES] = { 0u, 5u, 0u, 5u };
	const char* aszLg[uZM_MAX_MOVES] = {}; bool abLg[uZM_MAX_MOVES] = {}; int aiLg[uZM_MAX_MOVES] = {};
	const int nLead = ZM_UI_BattleHUD::BuildFilledMoveMenu(aeLeadGap, auLeadGapPP, aszLg, abLg, aiLg);
	ZENITH_ASSERT_EQ(nLead, 2, "leading gap: two filled slots offered");
	ZENITH_ASSERT_EQ(aiLg[0], 1, "leading gap: cursor 0 maps to raw slot 1");
	ZENITH_ASSERT_EQ(aiLg[1], 3, "leading gap: cursor 1 maps to raw slot 3");
	ZENITH_ASSERT_TRUE(abLg[0] && abLg[1], "leading gap: both filled moves selectable");

	// ALL-EMPTY {_,_,_,_}: no moves offered (a fainted/moveless active would show none).
	const ZM_MOVE_ID aeEmpty[uZM_MAX_MOVES] = { ZM_MOVE_NONE, ZM_MOVE_NONE, ZM_MOVE_NONE, ZM_MOVE_NONE };
	const u_int      auEmptyPP[uZM_MAX_MOVES] = { 0u, 0u, 0u, 0u };
	const char* aszEm[uZM_MAX_MOVES] = {}; bool abEm[uZM_MAX_MOVES] = {}; int aiEm[uZM_MAX_MOVES] = {};
	const int nEmpty = ZM_UI_BattleHUD::BuildFilledMoveMenu(aeEmpty, auEmptyPP, aszEm, abEm, aiEm);
	ZENITH_ASSERT_EQ(nEmpty, 0, "all-empty moveset offers no moves");
}
