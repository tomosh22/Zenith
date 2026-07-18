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
