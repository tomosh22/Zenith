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
#include "Input/Zenith_InputSimulator.h"   // the LIVE UpdateMenu drive injects real key edges
#include "Input/Zenith_KeyCodes.h"         // ZENITH_KEY_DOWN / ZENITH_KEY_ENTER (raw, deliberately)
#include "ZenithECS/Zenith_Entity.h"       // a default (INVALID) entity is what keeps the live drive headless
#include "Zenithmon/Source/UI/ZM_UI_BattleHUD.h"
#include "Zenithmon/Source/Battle/ZM_BattleAI.h"              // ZM_AI_TIER_GREEDY (the fixture battle's enemy tier)
#include "Zenithmon/Source/Battle/ZM_BattleDirectorCore.h"   // ZM_MapEventToOp (carries-text cross-table lock)
#include "Zenithmon/Source/Battle/ZM_BattleEvent.h"
#include "Zenithmon/Source/Battle/ZM_BattleMonster.h"         // ZM_BattleMonsterSpec (the fixture battle's sides)
#include "Zenithmon/Source/Battle/ZM_BattleTypes.h"
#include "Zenithmon/Source/Data/ZM_MoveData.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_ItemData.h"   // ZM_ITEM_CATCHORB (the Catch action's submitted item)

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
// ZM_UI_BattleHUD. Six PURE statics (no scene / graphics / core):
// MenuRootItemCount, MenuRootItemAtIndex, MenuItemCount, MenuMoveCursor,
// MenuConfirm, MenuCancel. (The first two arrived with the S6 catch gate below.)
// These are the whole decision surface the director's per-frame input drive
// delegates to, so pinning them here means a single-key windowed drive is enough
// to exercise the wiring (the arithmetic is proven hermetically). Every fixture is
// deterministic; no RequestSkip needed.
// ============================================================================

// ---- MenuItemCount ----------------------------------------------------------

ZENITH_TEST(ZM_BattleHUD, HudMenu_ItemCounts)
{
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::MenuItemCount(ZM_BATTLE_MENU_ACTION_ROOT, 4, true), 3,
		"a catch-allowed action root offers three items (Fight, Catch, Run)");
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::MenuItemCount(ZM_BATTLE_MENU_MOVE_SELECT, 3, true), 3,
		"move-select offers one item per available move");
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::MenuItemCount(ZM_BATTLE_MENU_HIDDEN, 4, true), 0,
		"a hidden menu offers no items");
}

// ---- MenuMoveCursor: clamp, no wrap ----------------------------------------

ZENITH_TEST(ZM_BattleHUD, HudMenu_CursorClampsNoWrap)
{
	// The 3-item action root (Fight, Catch, Run) exercises the clamp end to end.
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::MenuMoveCursor(0, +1, 3), 1,
		"+1 from 0 in a 3-item menu advances to 1");
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::MenuMoveCursor(1, +1, 3), 2,
		"+1 from 1 advances to 2 (the last item)");
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::MenuMoveCursor(2, +1, 3), 2,
		"+1 from the last item clamps (never wraps to 0)");
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::MenuMoveCursor(2, -1, 3), 1,
		"-1 from 2 moves to 1");
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::MenuMoveCursor(0, -1, 3), 0,
		"-1 from the first item clamps (never wraps to the last)");
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::MenuMoveCursor(0, +1, 0), 0,
		"an empty menu (n<=0) guards the cursor at 0");
}

// ---- MenuConfirm ------------------------------------------------------------

ZENITH_TEST(ZM_BattleHUD, HudMenu_FightOpensMoveSelect)
{
	const bool abSel4[4] = { true, true, true, true };
	const ZM_BattleMenuConfirmResult xResult =
		ZM_UI_BattleHUD::MenuConfirm(ZM_BATTLE_MENU_ACTION_ROOT, 0, abSel4, 4, /* bCanCatch */ true);
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
		ZM_UI_BattleHUD::MenuConfirm(ZM_BATTLE_MENU_ACTION_ROOT, 2, abSel4, 4, /* bCanCatch */ true);
	ZENITH_ASSERT_EQ(xResult.m_eKind, ZM_BATTLE_MENU_CONFIRM_SUBMIT,
		"Run (root cursor 2) submits an action immediately");
	ZENITH_ASSERT_EQ(xResult.m_xAction.m_eKind, ZM_ACTION_RUN,
		"the submitted action is a RUN");
	ZENITH_ASSERT_EQ(xResult.m_eNextScreen, ZM_BATTLE_MENU_HIDDEN,
		"submitting Run hides the menu");
}

// Catch (root cursor 1) submits a ZM_ACTION_ITEM carrying the catch orb, then hides
// the menu -- the battle engine's catch path interprets the item (SC4).
ZENITH_TEST(ZM_BattleHUD, HudMenu_CatchEmitsCatchAction)
{
	const bool abSel4[4] = { true, true, true, true };
	const ZM_BattleMenuConfirmResult xResult =
		ZM_UI_BattleHUD::MenuConfirm(ZM_BATTLE_MENU_ACTION_ROOT, 1 /*CATCH*/, abSel4, 3, /* bCanCatch */ true);
	ZENITH_ASSERT_EQ(xResult.m_eKind, ZM_BATTLE_MENU_CONFIRM_SUBMIT,
		"Catch (root cursor 1) submits an action immediately");
	ZENITH_ASSERT_EQ(xResult.m_xAction.m_eKind, ZM_ACTION_ITEM,
		"the submitted action is an ITEM (a thrown ball)");
	ZENITH_ASSERT_EQ((u_int)xResult.m_xAction.m_eItem, (u_int)ZM_ITEM_CATCHORB,
		"the thrown ball is the catch orb");
	ZENITH_ASSERT_EQ(xResult.m_eNextScreen, ZM_BATTLE_MENU_HIDDEN,
		"submitting Catch hides the menu");
}

ZENITH_TEST(ZM_BattleHUD, HudMenu_MoveSelectEmitsMoveAction)
{
	const bool abSel4[4] = { true, true, true, true };
	const ZM_BattleMenuConfirmResult xResult =
		ZM_UI_BattleHUD::MenuConfirm(ZM_BATTLE_MENU_MOVE_SELECT, 2, abSel4, 4, /* bCanCatch */ true);
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
		ZM_UI_BattleHUD::MenuConfirm(ZM_BATTLE_MENU_MOVE_SELECT, 1, abSel, 4, /* bCanCatch */ true);
	ZENITH_ASSERT_EQ(xBlocked.m_eKind, ZM_BATTLE_MENU_CONFIRM_NONE,
		"confirming an unselectable move is a no-op (no submit)");

	const ZM_BattleMenuConfirmResult xOk =
		ZM_UI_BattleHUD::MenuConfirm(ZM_BATTLE_MENU_MOVE_SELECT, 0, abSel, 4, /* bCanCatch */ true);
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
// S6 -- the CATCH-ENTRY GATE (Shortfalls 1.5 deferral (a)). The battle menu used
// to offer Catch unconditionally. That is inert while every battle is wild, but a
// TRAINER battle -- and the Battle Tower, which ALREADY sets m_bCanCatch = false
// (ZM_BattleTower.cpp) -- is a real, reachable config with catching off, and
// ZM_BattleEngine::SubmitAction / DoItemAction ASSERT on an ITEM (catch) action
// unless the config allows it. The root list is therefore DYNAMIC, and the tests
// below pin both halves: the entry is absent, AND the indices stay coherent with
// it absent (an off-by-one here would submit the WRONG action, which is strictly
// worse than the bug being fixed).
// ============================================================================

ZENITH_TEST(ZM_BattleHUD, HudMenu_RootItemCountGatesCatch)
{
	// FAILS IF: MenuRootItemCount stops consulting bCanCatch (e.g. returns
	// ZM_BATTLE_MENU_ROOT_COUNT unconditionally, the pre-fix behaviour).
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::MenuRootItemCount(true), 3,
		"a catch-allowed root offers Fight, Catch and Run");
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::MenuRootItemCount(false), 2,
		"a catch-DISALLOWED root offers only Fight and Run");
	// FAILS IF: MenuItemCount's ACTION_ROOT arm stops routing through
	// MenuRootItemCount -- the path UpdateMenu actually clamps the cursor with.
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::MenuItemCount(ZM_BATTLE_MENU_ACTION_ROOT, 4, false), 2,
		"the root item count is what MenuItemCount reports for ACTION_ROOT");
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::MenuItemCount(ZM_BATTLE_MENU_MOVE_SELECT, 3, false), 3,
		"the catch gate must not touch the move list's item count");
}

ZENITH_TEST(ZM_BattleHUD, HudMenu_RootItemAtIndexGatesCatch)
{
	// Catch allowed: the mapping is the identity, so every already-green windowed
	// drive (which walks Fight(0) -> Catch(1) -> Run(2)) is unchanged.
	// FAILS IF: the gated mapping stops degenerating to the identity when catching is
	// allowed -- which would silently repoint every existing cursor position.
	ZENITH_ASSERT_EQ((u_int)ZM_UI_BattleHUD::MenuRootItemAtIndex(0, true), (u_int)ZM_BATTLE_MENU_FIGHT,
		"catch allowed: index 0 is Fight");
	ZENITH_ASSERT_EQ((u_int)ZM_UI_BattleHUD::MenuRootItemAtIndex(1, true), (u_int)ZM_BATTLE_MENU_CATCH,
		"catch allowed: index 1 is Catch");
	ZENITH_ASSERT_EQ((u_int)ZM_UI_BattleHUD::MenuRootItemAtIndex(2, true), (u_int)ZM_BATTLE_MENU_RUN,
		"catch allowed: index 2 is Run");

	// Catch disallowed: Run CLOSES THE GAP. This is the index arithmetic that would
	// otherwise submit the wrong action.
	// FAILS IF: the gated mapping keeps Run at index 2 (leaving index 1 dead) or maps
	// index 1 to Catch anyway.
	ZENITH_ASSERT_EQ((u_int)ZM_UI_BattleHUD::MenuRootItemAtIndex(0, false), (u_int)ZM_BATTLE_MENU_FIGHT,
		"catch disallowed: index 0 is still Fight");
	ZENITH_ASSERT_EQ((u_int)ZM_UI_BattleHUD::MenuRootItemAtIndex(1, false), (u_int)ZM_BATTLE_MENU_RUN,
		"catch disallowed: index 1 is RUN -- the Catch entry does not exist");

	// Out of range in BOTH directions yields the sentinel, never a real entry.
	// FAILS IF: the bounds check is dropped and the raw index is cast to an item --
	// index 2 would then read as Run in a 2-entry list, i.e. a phantom duplicate.
	ZENITH_ASSERT_EQ((u_int)ZM_UI_BattleHUD::MenuRootItemAtIndex(2, false), (u_int)ZM_BATTLE_MENU_ROOT_COUNT,
		"catch disallowed: index 2 is past the end and maps to no entry");
	ZENITH_ASSERT_EQ((u_int)ZM_UI_BattleHUD::MenuRootItemAtIndex(3, true), (u_int)ZM_BATTLE_MENU_ROOT_COUNT,
		"catch allowed: index 3 is past the end and maps to no entry");
	ZENITH_ASSERT_EQ((u_int)ZM_UI_BattleHUD::MenuRootItemAtIndex(-1, true), (u_int)ZM_BATTLE_MENU_ROOT_COUNT,
		"a negative index maps to no entry");
}

ZENITH_TEST(ZM_BattleHUD, HudMenu_CatchDisallowedNeverSubmitsACatch)
{
	const bool abSel4[4] = { true, true, true, true };

	// Index 0 is Fight either way.
	const ZM_BattleMenuConfirmResult xFight =
		ZM_UI_BattleHUD::MenuConfirm(ZM_BATTLE_MENU_ACTION_ROOT, 0, abSel4, 4, /* bCanCatch */ false);
	ZENITH_ASSERT_EQ(xFight.m_eKind, ZM_BATTLE_MENU_CONFIRM_OPEN_MOVES,
		"catch disallowed: Fight still opens the move list");

	// Index 1 is RUN, not Catch. This is THE regression clause: before the gate,
	// confirming index 1 submitted {ZM_ACTION_ITEM, CATCHORB} -- the action
	// ZM_BattleEngine::SubmitAction asserts on when m_bCanCatch is false.
	// FAILS IF: MenuConfirm's ACTION_ROOT arm goes back to comparing the cursor
	// against ZM_BATTLE_MENU_CATCH directly instead of resolving the entry.
	const ZM_BattleMenuConfirmResult xIndex1 =
		ZM_UI_BattleHUD::MenuConfirm(ZM_BATTLE_MENU_ACTION_ROOT, 1, abSel4, 4, /* bCanCatch */ false);
	ZENITH_ASSERT_EQ(xIndex1.m_eKind, ZM_BATTLE_MENU_CONFIRM_SUBMIT,
		"catch disallowed: index 1 still submits an action");
	ZENITH_ASSERT_EQ(xIndex1.m_xAction.m_eKind, ZM_ACTION_RUN,
		"catch disallowed: the action at index 1 is RUN, never a thrown ball");

	// Index 2 no longer exists, so it submits NOTHING (rather than falling through to
	// a real entry). FAILS IF: MenuRootItemAtIndex's bounds check is dropped.
	const ZM_BattleMenuConfirmResult xPastEnd =
		ZM_UI_BattleHUD::MenuConfirm(ZM_BATTLE_MENU_ACTION_ROOT, 2, abSel4, 4, /* bCanCatch */ false);
	ZENITH_ASSERT_EQ(xPastEnd.m_eKind, ZM_BATTLE_MENU_CONFIRM_NONE,
		"catch disallowed: a cursor past the end submits nothing");

	// TOTALITY over every cursor a clamped (or corrupted) menu could hold: with
	// catching off, NO cursor value may produce an ITEM action.
	// FAILS IF: any future root arm reintroduces an ungated catch path.
	for (int iCursor = -2; iCursor <= 5; ++iCursor)
	{
		const ZM_BattleMenuConfirmResult xR =
			ZM_UI_BattleHUD::MenuConfirm(ZM_BATTLE_MENU_ACTION_ROOT, iCursor, abSel4, 4, /* bCanCatch */ false);
		ZENITH_ASSERT_FALSE(
			xR.m_eKind == ZM_BATTLE_MENU_CONFIRM_SUBMIT && xR.m_xAction.m_eKind == ZM_ACTION_ITEM,
			"cursor %d must not submit an ITEM (catch) action when catching is disallowed", iCursor);
	}

	// ...and the catch-allowed root is UNCHANGED: index 1 still throws the orb, so the
	// gate did not cost the shipped wild-battle behaviour.
	const ZM_BattleMenuConfirmResult xAllowed =
		ZM_UI_BattleHUD::MenuConfirm(ZM_BATTLE_MENU_ACTION_ROOT, 1, abSel4, 4, /* bCanCatch */ true);
	ZENITH_ASSERT_EQ(xAllowed.m_xAction.m_eKind, ZM_ACTION_ITEM,
		"catch allowed: index 1 still submits the catch item");
	ZENITH_ASSERT_EQ((u_int)xAllowed.m_xAction.m_eItem, (u_int)ZM_ITEM_CATCHORB,
		"catch allowed: the thrown ball is still the catch orb");
}

ZENITH_TEST(ZM_BattleHUD, HudMenu_GatedRootCursorStaysCoherent)
{
	const bool abSel4[4] = { true, true, true, true };
	const int  iGatedCount = ZM_UI_BattleHUD::MenuItemCount(ZM_BATTLE_MENU_ACTION_ROOT, 4, false);

	// Walking DOWN off the end of the shortened list clamps to the last real entry
	// (Run at index 1) -- it never parks on the index the Catch entry used to occupy.
	// FAILS IF: MenuItemCount's ACTION_ROOT arm stops routing through MenuRootItemCount,
	// or MenuMoveCursor stops clamping to the count it is handed. It says NOTHING about
	// UpdateMenu -- this block calls the statics directly and never enters it. The claim
	// that the LIVE clamp is fed the gated count belongs to (and is only pinned by)
	// HudMenu_LiveGateNeverSubmitsACatchWhenDisallowed below.
	int iCursor = 0;
	for (int i = 0; i < 5; ++i)
	{
		iCursor = ZM_UI_BattleHUD::MenuMoveCursor(iCursor, +1, iGatedCount);
	}
	ZENITH_ASSERT_EQ(iCursor, 1, "the gated root clamps the cursor at its last entry (Run)");

	// A cursor LEFT OVER from a longer list re-clamps into range on the next nav read
	// (MenuMoveCursor clamps even for a zero delta), so a stale index 2 can never
	// address a missing entry. FAILS IF: MenuMoveCursor stops clamping a zero delta.
	ZENITH_ASSERT_EQ(ZM_UI_BattleHUD::MenuMoveCursor(2, 0, iGatedCount), 1,
		"a stale cursor past the gated end re-clamps to the last entry");

	// Every in-range cursor of the gated list resolves to a REAL entry that submits
	// something -- no dead index, no silent no-op row.
	for (int i = 0; i < iGatedCount; ++i)
	{
		ZENITH_ASSERT_TRUE(
			ZM_UI_BattleHUD::MenuRootItemAtIndex(i, false) != ZM_BATTLE_MENU_ROOT_COUNT,
			"gated root index %d must resolve to a real entry", i);
		const ZM_BattleMenuConfirmResult xR =
			ZM_UI_BattleHUD::MenuConfirm(ZM_BATTLE_MENU_ACTION_ROOT, i, abSel4, 4, /* bCanCatch */ false);
		ZENITH_ASSERT_TRUE(xR.m_eKind != ZM_BATTLE_MENU_CONFIRM_NONE,
			"gated root index %d must do something on confirm", i);
	}
}

// ============================================================================
// The LIVE half of the catch gate. Everything above this point calls the PURE
// statics with a literal bCanCatch, and ZM_BattleDirectorCore's own unit only asks
// IsCatchAllowed() in isolation -- so NOTHING connected the accessor to the menu.
// VERIFIED: replacing UpdateMenu's
//     const bool bCanCatch = xCore.IsCatchAllowed();
// with a hard-coded `true` left every other test in the repo green while fully
// restoring the shortfall's failure mode (a m_bCanCatch == false battle offering
// Catch, then tripping ZM_BattleEngine::SubmitAction's assert). This test is the one
// that reddens under that mutation: it Begins a REAL catch-disallowed battle, drives
// the REAL UpdateMenu with REAL key edges, and demands that confirming cursor index 1
// yields RUN. Under the mutation index 1 resolves to Catch and the action is an ITEM.
//
// It stays a hermetic headless unit because UpdateMenu is best-effort about UI: an
// INVALID Zenith_Entity resolves no Zenith_UIComponent, so the visual refresh is
// skipped (ZM_UI_BattleHUD.cpp) while the whole state machine still runs. No scene,
// no graphics, no baked asset.
// ============================================================================

namespace
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scoped simulator ownership, matching ZM_Tests_Overworld.cpp's InputScope: a unit
	// that injects key edges must not leak them into whatever boots next in the batch.
	struct HudInputScope
	{
		HudInputScope()
		{
			Zenith_InputSimulator::Enable();
			Zenith_InputSimulator::ResetAllInputState();
		}

		~HudInputScope()
		{
			Zenith_InputSimulator::ResetAllInputState();
			Zenith_InputSimulator::Disable();
		}
	};

	// zm_instant_battles for the duration, so the intro drains in a single Tick and the
	// director reaches AWAIT_INPUT -- the exact state ZM_BattleDirector calls UpdateMenu
	// from -- without any wall-clock pacing.
	struct HudInstantBattleScope
	{
		HudInstantBattleScope()  { ZM_SetInstantBattlesForTests(true);  }
		~HudInstantBattleScope() { ZM_SetInstantBattlesForTests(false); }
	};

	// A REAL battle whose ONLY varying rule is the catch permission, drained to
	// AWAIT_INPUT. The config is built here rather than copied from a helper so the
	// m_bCanCatch line is visible at the fixture site.
	void BeginBattleAwaitingInput(ZM_BattleDirectorCore& xCore, bool bCanCatch)
	{
		const ZM_BattleMonsterSpec xPlayer = ZM_BuildWildEnemySpec(eZM_HUD_PLAYER_SPECIES, 5u);
		const ZM_BattleMonsterSpec xEnemy  = ZM_BuildWildEnemySpec(eZM_HUD_ENEMY_SPECIES, 5u);
		ZM_BattleConfig xConfig;
		xConfig.m_bIsWild   = true;
		xConfig.m_bCanFlee  = true;
		xConfig.m_bCanCatch = bCanCatch;   // THE rule under test; read back via IsCatchAllowed()
		xCore.Begin(&xPlayer, 1u, &xEnemy, 1u, xConfig, 0xB4771E60ull, ZM_AI_TIER_GREEDY);
		xCore.Tick(0.0f);   // instant: one tick drains the whole intro
	}

	// One frame of the live menu drive: assert a single key edge, run UpdateMenu, then
	// close the frame out (clear the pressed edges + release the key) exactly as the main
	// loop would. Returns UpdateMenu's submitted flag.
	bool DriveMenuFrame(ZM_UI_BattleHUD& xHud, Zenith_Entity& xEntity,
		const ZM_BattleDirectorCore& xCore, ZM_BattleAction& xActionOut, Zenith_KeyCode eKey)
	{
		Zenith_InputSimulator::SimulateKeyDown(eKey);
		const bool bSubmitted = xHud.UpdateMenu(xEntity, xCore, xActionOut);
		Zenith_InputSimulator::EndTestFrame();
		Zenith_InputSimulator::SimulateKeyUp(eKey);
		return bSubmitted;
	}
#endif
}

ZENITH_TEST(ZM_BattleHUD, HudMenu_LiveGateNeverSubmitsACatchWhenDisallowed)
{
#ifdef ZENITH_INPUT_SIMULATOR
	HudInputScope         xInput;
	HudInstantBattleScope xInstant;

	// Raw key codes throughout, deliberately: this drive characterises the bindings the
	// live menu actually consumes rather than restating ZM_InputActions' constants.
	//
	// A default Zenith_Entity is INVALID (no scene data), so UpdateMenu's
	// TryGetComponent<Zenith_UIComponent> path is skipped entirely -- that is what keeps
	// this live drive a hermetic headless unit.
	Zenith_Entity xNoEntity;

	// ---- catching DISALLOWED (the Battle Tower's shipped config; every S7 trainer) ----
	{
		ZM_BattleDirectorCore xCore;
		BeginBattleAwaitingInput(xCore, /* bCanCatch */ false);
		ZENITH_ASSERT_TRUE(xCore.IsAwaitingInput(),
			"the instant intro must drain to AWAIT_INPUT before the menu is driven");
		// Without this the whole case would be vacuous: it must be a battle that really
		// does forbid catching, read back through the accessor UpdateMenu consults.
		ZENITH_ASSERT_FALSE(xCore.IsCatchAllowed(),
			"the fixture battle must actually disallow catching");

		ZM_UI_BattleHUD xHud;
		ZM_BattleAction xAction;

		// Frame 1: UpdateMenu opens the root (screen was HIDDEN) and consumes one DOWN
		// edge. The cursor lands on index 1 -- the slot Catch occupies in a wild battle.
		ZENITH_ASSERT_FALSE(DriveMenuFrame(xHud, xNoEntity, xCore, xAction, ZENITH_KEY_DOWN),
			"a bare nav frame must not submit an action");
		ZENITH_ASSERT_EQ(xHud.GetMenuScreen(), ZM_BATTLE_MENU_ACTION_ROOT,
			"a fresh AWAIT_INPUT turn opens the action root");
		ZENITH_ASSERT_EQ(xHud.GetMenuCursor(), 1,
			"one DOWN edge moves the live root cursor to index 1");

		// Frame 2: confirm index 1. THE clause. FAILS IF: UpdateMenu stops reading
		// xCore.IsCatchAllowed() (e.g. hard-codes true) -- index 1 then resolves to Catch
		// and the submitted action becomes the ITEM the engine asserts on.
		const bool bSubmitted =
			DriveMenuFrame(xHud, xNoEntity, xCore, xAction, ZENITH_KEY_ENTER);
		ZENITH_ASSERT_TRUE(bSubmitted,
			"confirming index 1 of the gated root submits an action");
		ZENITH_ASSERT_NE((u_int)xAction.m_eKind, (u_int)ZM_ACTION_ITEM,
			"a catch-disallowed battle must NEVER submit an ITEM (thrown ball) action");
		ZENITH_ASSERT_EQ((u_int)xAction.m_eKind, (u_int)ZM_ACTION_RUN,
			"catch disallowed: the live entry at index 1 is RUN");
		ZENITH_ASSERT_EQ(xHud.GetMenuScreen(), ZM_BATTLE_MENU_HIDDEN,
			"a submitted action hides the live menu");
	}

	// ---- catching ALLOWED: the MIRROR, so this proves the GATE and not merely "index 1
	//      is never a catch". Same drive, same cursor, opposite config, opposite action.
	{
		ZM_BattleDirectorCore xCore;
		BeginBattleAwaitingInput(xCore, /* bCanCatch */ true);
		ZENITH_ASSERT_TRUE(xCore.IsAwaitingInput(),
			"the instant intro must drain to AWAIT_INPUT before the menu is driven");
		ZENITH_ASSERT_TRUE(xCore.IsCatchAllowed(),
			"the mirror fixture must actually allow catching");

		ZM_UI_BattleHUD xHud;
		ZM_BattleAction xAction;

		ZENITH_ASSERT_FALSE(DriveMenuFrame(xHud, xNoEntity, xCore, xAction, ZENITH_KEY_DOWN),
			"a bare nav frame must not submit an action");
		ZENITH_ASSERT_EQ(xHud.GetMenuCursor(), 1,
			"one DOWN edge moves the live root cursor to index 1");

		// FAILS IF: the gate is inverted, or UpdateMenu stops offering Catch at all --
		// which would silently cost wild battles the shipped catch action.
		ZENITH_ASSERT_TRUE(DriveMenuFrame(xHud, xNoEntity, xCore, xAction, ZENITH_KEY_ENTER),
			"confirming index 1 of the ungated root submits an action");
		ZENITH_ASSERT_EQ((u_int)xAction.m_eKind, (u_int)ZM_ACTION_ITEM,
			"catch allowed: the live entry at index 1 throws a ball");
		ZENITH_ASSERT_EQ((u_int)xAction.m_eItem, (u_int)ZM_ITEM_CATCHORB,
			"catch allowed: the thrown ball is the catch orb");
	}
#else
	ZENITH_SKIP("input simulator is unavailable in this configuration");
#endif
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
		ZM_UI_BattleHUD::MenuConfirm(ZM_BATTLE_MENU_MOVE_SELECT, 1, abSel, n, /* bCanCatch */ true, aiRaw);
	ZENITH_ASSERT_EQ(xR.m_eKind, ZM_BATTLE_MENU_CONFIRM_SUBMIT, "a selectable move submits");
	ZENITH_ASSERT_EQ(xR.m_xAction.m_eKind, ZM_ACTION_MOVE, "the submitted action is a MOVE");
	ZENITH_ASSERT_EQ(xR.m_xAction.m_uMoveSlot, 2u, "the submitted move slot is the RAW slot of the 2nd filled move");

	// Backward-compat: the nullptr rawslot overload maps the cursor straight to the slot.
	const bool abSel4[uZM_MAX_MOVES] = { true, true, true, true };
	const ZM_BattleMenuConfirmResult xIdentity =
		ZM_UI_BattleHUD::MenuConfirm(ZM_BATTLE_MENU_MOVE_SELECT, 2, abSel4, 4, /* bCanCatch */ true);
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
