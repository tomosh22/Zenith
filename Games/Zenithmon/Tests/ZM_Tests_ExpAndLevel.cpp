#include "Zenith.h"

// ============================================================================
// ZM_Tests_ExpAndLevel -- S2 box-4 (ZM_ExpAndLevel) unit tests. Covers the four
// exp-curve families, the per-species derived accessors (growth rate / base-exp
// yield / EV yield / evolve level), the exp-award formula, EV accumulation +
// caps, level-up stat recompute, mid-battle move-learn, pure evolution, and the
// engine integration (config-gated exp emission) -- including the CRITICAL
// zero-perturbation byte-identity guard.
//
// Category split (spec section 5): the pure-math / data / pure-mutation tests are
// ZM_Data; the engine-integration (event-stream) tests are ZM_Battle.
//
// The ZM_ValidateEventStream / MakeSpec helpers in ZM_Tests_Battle.cpp live in an
// anonymous namespace and are NOT visible here, so this TU carries its OWN local
// spec/battle/stream builders and does exact expected-stream comparisons via
// ZM_MakeEvent + element-by-element equality.
//
// Local box policies asserted here (pending the eventual landed decision entry):
//   * 4-move overflow = SKIP (no learn, no MOVE_LEARNED) -- MoveLearn_FourMoveOverflow.
//   * Evolution = LEVEL-trigger only (pure ZM_Evolve; item/trade/friendship out of scope).
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Battle/ZM_ExpAndLevel.h"
#include "Zenithmon/Source/Battle/ZM_BattleEngine.h"
#include "Zenithmon/Source/Battle/ZM_BattleMonster.h"
#include "Zenithmon/Source/Battle/ZM_BattleEvent.h"
#include "Zenithmon/Source/Battle/ZM_BattleTypes.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_MoveData.h"
#include "Zenithmon/Source/Data/ZM_NatureData.h"
#include "Zenithmon/Source/Data/ZM_StatCalc.h"
#include "Zenithmon/Source/Data/ZM_Learnsets.h"
#include "Collections/Zenith_Vector.h"

// ============================================================================
// Test-local helpers (anonymous namespace: no cross-TU ODR clashes).
// ============================================================================
namespace
{
	// ---- event diagnostics (compact one-line stringify) --------------------
	const char* ExpKindName(ZM_BATTLE_EVENT eKind)
	{
		switch (eKind)
		{
		case ZM_BATTLE_EVENT_BATTLE_BEGIN:      return "BATTLE_BEGIN";
		case ZM_BATTLE_EVENT_TURN_BEGIN:        return "TURN_BEGIN";
		case ZM_BATTLE_EVENT_TURN_END:          return "TURN_END";
		case ZM_BATTLE_EVENT_SWITCH_IN:         return "SWITCH_IN";
		case ZM_BATTLE_EVENT_MOVE_USED:         return "MOVE_USED";
		case ZM_BATTLE_EVENT_DAMAGE_DEALT:      return "DAMAGE_DEALT";
		case ZM_BATTLE_EVENT_FAINT:             return "FAINT";
		case ZM_BATTLE_EVENT_BATTLE_END:        return "BATTLE_END";
		case ZM_BATTLE_EVENT_EXP_GAINED:        return "EXP_GAINED";
		case ZM_BATTLE_EVENT_LEVEL_UP:          return "LEVEL_UP";
		case ZM_BATTLE_EVENT_MOVE_LEARNED:      return "MOVE_LEARNED";
		case ZM_BATTLE_EVENT_EVOLUTION_QUEUED:  return "EVOLUTION_QUEUED";
		default:                                return "?";
		}
	}

	void ExpEventToString(const ZM_BattleEvent& xEvent, char* acOut, size_t uSize)
	{
		snprintf(acOut, uSize, "[%s side=%u slot=%u move=%u species=%u amt=%d aux=%d tag=%d]",
			ExpKindName(xEvent.m_eKind), xEvent.m_uSide, xEvent.m_uSlot,
			xEvent.m_uMoveId, xEvent.m_uSpeciesId, xEvent.m_iAmount, xEvent.m_iAux,
			xEvent.m_iTag);
	}

	void LogTwoStreams(const Zenith_Vector<ZM_BattleEvent>& xExpected,
		const Zenith_Vector<ZM_BattleEvent>& xActual)
	{
		const u_int uExp = xExpected.GetSize();
		const u_int uAct = xActual.GetSize();
		const u_int uMax = (uExp > uAct) ? uExp : uAct;
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_ExpAndLevel] stream mismatch: expected %u events, actual %u events", uExp, uAct);
		char acExp[192];
		char acAct[192];
		for (u_int i = 0; i < uMax; ++i)
		{
			if (i < uExp) { ExpEventToString(xExpected.Get(i), acExp, sizeof(acExp)); }
			else          { snprintf(acExp, sizeof(acExp), "<none>"); }
			if (i < uAct) { ExpEventToString(xActual.Get(i), acAct, sizeof(acAct)); }
			else          { snprintf(acAct, sizeof(acAct), "<none>"); }
			const bool bEq = (i < uExp && i < uAct && xExpected.Get(i) == xActual.Get(i));
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  [%2u] %s exp=%s act=%s",
				i, bEq ? "   " : ">>>", acExp, acAct);
		}
	}

	// Exact element-by-element stream compare with a self-contained dump on drift.
	void AssertStreamEq(const Zenith_Vector<ZM_BattleEvent>& xExpected,
		const Zenith_Vector<ZM_BattleEvent>& xActual, const char* szLabel)
	{
		bool bMatch = (xExpected.GetSize() == xActual.GetSize());
		const u_int uMin = (xExpected.GetSize() < xActual.GetSize()) ? xExpected.GetSize() : xActual.GetSize();
		for (u_int i = 0; i < uMin; ++i)
		{
			if (!(xExpected.Get(i) == xActual.Get(i))) { bMatch = false; }
		}
		if (!bMatch) { LogTwoStreams(xExpected, xActual); }

		ZENITH_ASSERT_EQ(xActual.GetSize(), xExpected.GetSize(), "%s: stream length mismatch", szLabel);
		for (u_int i = 0; i < uMin; ++i)
		{
			char acE[192];
			char acA[192];
			ExpEventToString(xExpected.Get(i), acE, sizeof(acE));
			ExpEventToString(xActual.Get(i), acA, sizeof(acA));
			ZENITH_ASSERT_TRUE(xExpected.Get(i) == xActual.Get(i), "%s event[%u] exp=%s act=%s", szLabel, i, acE, acA);
		}
	}

	// ---- event-stream search helpers ---------------------------------------
	const ZM_BattleEvent* FindKind(const Zenith_Vector<ZM_BattleEvent>& xEvents, ZM_BATTLE_EVENT eKind)
	{
		for (u_int i = 0; i < xEvents.GetSize(); ++i)
		{
			if (xEvents.Get(i).m_eKind == eKind) { return &xEvents.Get(i); }
		}
		return nullptr;
	}
	int IndexOfKind(const Zenith_Vector<ZM_BattleEvent>& xEvents, ZM_BATTLE_EVENT eKind)
	{
		for (u_int i = 0; i < xEvents.GetSize(); ++i)
		{
			if (xEvents.Get(i).m_eKind == eKind) { return (int)i; }
		}
		return -1;
	}
	bool HasKind(const Zenith_Vector<ZM_BattleEvent>& xEvents, ZM_BATTLE_EVENT eKind)
	{
		return FindKind(xEvents, eKind) != nullptr;
	}
	u_int CountKind(const Zenith_Vector<ZM_BattleEvent>& xEvents, ZM_BATTLE_EVENT eKind)
	{
		u_int uCount = 0u;
		for (u_int i = 0; i < xEvents.GetSize(); ++i)
		{
			if (xEvents.Get(i).m_eKind == eKind) { ++uCount; }
		}
		return uCount;
	}

	// State fields which the EXP/EV award path has no authority to perturb. EXP
	// totals and the four progression-ledger fields are asserted separately by
	// callers because the enabled path is expected to differ there.
	void AssertNonProgressionMonsterEq(const ZM_BattleMonster& xExpected,
		const ZM_BattleMonster& xActual, u_int uSide, u_int uSlot)
	{
		ZENITH_ASSERT_EQ((u_int)xActual.m_eSpecies, (u_int)xExpected.m_eSpecies,
			"side %u slot %u species", uSide, uSlot);
		ZENITH_ASSERT_EQ(xActual.m_uLevel, xExpected.m_uLevel,
			"side %u slot %u level", uSide, uSlot);
		ZENITH_ASSERT_EQ((u_int)xActual.m_eNature, (u_int)xExpected.m_eNature,
			"side %u slot %u nature", uSide, uSlot);
		ZENITH_ASSERT_EQ((u_int)xActual.m_eAbility, (u_int)xExpected.m_eAbility,
			"side %u slot %u ability", uSide, uSlot);
		ZENITH_ASSERT_EQ(xActual.m_uCurHP, xExpected.m_uCurHP,
			"side %u slot %u current HP", uSide, uSlot);
		ZENITH_ASSERT_EQ((u_int)xActual.m_eStatus, (u_int)xExpected.m_eStatus,
			"side %u slot %u major status", uSide, uSlot);
		ZENITH_ASSERT_EQ(xActual.m_uStatusCounter, xExpected.m_uStatusCounter,
			"side %u slot %u status counter", uSide, uSlot);
		ZENITH_ASSERT_EQ(xActual.m_uVolatileMask, xExpected.m_uVolatileMask,
			"side %u slot %u volatile mask", uSide, uSlot);
		ZENITH_ASSERT_EQ(xActual.m_uConfuseTurns, xExpected.m_uConfuseTurns,
			"side %u slot %u confuse turns", uSide, uSlot);
		ZENITH_ASSERT_EQ(xActual.m_uTrapTurns, xExpected.m_uTrapTurns,
			"side %u slot %u trap turns", uSide, uSlot);
		ZENITH_ASSERT_EQ(xActual.m_uTauntTurns, xExpected.m_uTauntTurns,
			"side %u slot %u taunt turns", uSide, uSlot);
		ZENITH_ASSERT_EQ(xActual.m_uLockTurns, xExpected.m_uLockTurns,
			"side %u slot %u lock turns", uSide, uSlot);
		ZENITH_ASSERT_EQ(xActual.m_uChargeMoveSlot, xExpected.m_uChargeMoveSlot,
			"side %u slot %u charge slot", uSide, uSlot);
		ZENITH_ASSERT_EQ(xActual.m_uLockMoveSlot, xExpected.m_uLockMoveSlot,
			"side %u slot %u lock slot", uSide, uSlot);
		ZENITH_ASSERT_EQ((u_int)xActual.m_eLeechSourceSide, (u_int)xExpected.m_eLeechSourceSide,
			"side %u slot %u leech source", uSide, uSlot);
		ZENITH_ASSERT_EQ(xActual.m_bEndureThisTurn, xExpected.m_bEndureThisTurn,
			"side %u slot %u endure", uSide, uSlot);
		ZENITH_ASSERT_EQ(xActual.m_iCritStage, xExpected.m_iCritStage,
			"side %u slot %u crit stage", uSide, uSlot);
		for (u_int i = 0u; i < ZM_STAT_COUNT; ++i)
		{
			ZENITH_ASSERT_EQ(xActual.m_auIV[i], xExpected.m_auIV[i],
				"side %u slot %u IV %u", uSide, uSlot, i);
			ZENITH_ASSERT_EQ(xActual.m_auEV[i], xExpected.m_auEV[i],
				"side %u slot %u EV %u", uSide, uSlot, i);
			ZENITH_ASSERT_EQ(xActual.m_auMaxStat[i], xExpected.m_auMaxStat[i],
				"side %u slot %u max stat %u", uSide, uSlot, i);
			ZENITH_ASSERT_EQ(xActual.m_xBaseStats.m_au[i], xExpected.m_xBaseStats.m_au[i],
				"side %u slot %u base stat %u", uSide, uSlot, i);
		}
		for (u_int i = 0u; i < uZM_MAX_MOVES; ++i)
		{
			ZENITH_ASSERT_EQ((u_int)xActual.m_axMoves[i].m_eMove,
				(u_int)xExpected.m_axMoves[i].m_eMove,
				"side %u slot %u move id %u", uSide, uSlot, i);
			ZENITH_ASSERT_EQ(xActual.m_axMoves[i].m_uCurPP, xExpected.m_axMoves[i].m_uCurPP,
				"side %u slot %u current PP %u", uSide, uSlot, i);
			ZENITH_ASSERT_EQ(xActual.m_axMoves[i].m_uMaxPP, xExpected.m_axMoves[i].m_uMaxPP,
				"side %u slot %u max PP %u", uSide, uSlot, i);
		}
		for (u_int i = 0u; i < ZM_BATTLE_STAT_COUNT; ++i)
		{
			ZENITH_ASSERT_EQ(xActual.m_aiStage[i], xExpected.m_aiStage[i],
				"side %u slot %u battle stage %u", uSide, uSlot, i);
		}
	}

	void AssertNonProgressionStateEq(const ZM_BattleState& xExpected,
		const ZM_BattleState& xActual)
	{
		for (u_int uSide = 0u; uSide < (u_int)ZM_SIDE_COUNT; ++uSide)
		{
			const ZM_BattleSide& xExpectedSide = xExpected.Side((ZM_SIDE)uSide);
			const ZM_BattleSide& xActualSide = xActual.Side((ZM_SIDE)uSide);
			ZENITH_ASSERT_EQ(xActualSide.m_xParty.GetSize(), xExpectedSide.m_xParty.GetSize(),
				"side %u party size", uSide);
			ZENITH_ASSERT_EQ(xActualSide.m_uActiveSlot, xExpectedSide.m_uActiveSlot,
				"side %u active slot", uSide);
			for (u_int i = 0u; i < ZM_SCREEN_COUNT; ++i)
			{
				ZENITH_ASSERT_EQ(xActualSide.m_auScreenTurns[i], xExpectedSide.m_auScreenTurns[i],
					"side %u screen %u", uSide, i);
			}
			ZENITH_ASSERT_EQ(xActualSide.m_uHazardSpikeLayers, xExpectedSide.m_uHazardSpikeLayers,
				"side %u spike layers", uSide);
			const u_int uCount = xActualSide.m_xParty.GetSize() < xExpectedSide.m_xParty.GetSize()
				? xActualSide.m_xParty.GetSize() : xExpectedSide.m_xParty.GetSize();
			for (u_int uSlot = 0u; uSlot < uCount; ++uSlot)
			{
				AssertNonProgressionMonsterEq(xExpectedSide.m_xParty.Get(uSlot),
					xActualSide.m_xParty.Get(uSlot), uSide, uSlot);
			}
		}
		ZENITH_ASSERT_EQ((u_int)xActual.m_xField.m_eWeather, (u_int)xExpected.m_xField.m_eWeather);
		ZENITH_ASSERT_EQ(xActual.m_xField.m_uWeatherTurns, xExpected.m_xField.m_uWeatherTurns);
		ZENITH_ASSERT_EQ(xActual.m_xField.m_uTurnCounter, xExpected.m_xField.m_uTurnCounter);
	}

	// ---- spec builders -----------------------------------------------------
	// Override-base-stat spec (pencil-verifiable level-up/exp goldens). Four move
	// slots so a mid-battle level-up move-learn can be suppressed by filling them.
	ZM_BattleMonsterSpec MakeSpecOv(ZM_SPECIES_ID eSpecies, u_int uLevel,
		u_int uHP, u_int uATK, u_int uDEF, u_int uSPA, u_int uSPD, u_int uSPE,
		u_int uIV, u_int uEV,
		ZM_MOVE_ID eM0 = ZM_MOVE_NONE, ZM_MOVE_ID eM1 = ZM_MOVE_NONE,
		ZM_MOVE_ID eM2 = ZM_MOVE_NONE, ZM_MOVE_ID eM3 = ZM_MOVE_NONE)
	{
		ZM_BattleMonsterSpec xSpec;
		xSpec.m_eSpecies = eSpecies;
		xSpec.m_uLevel = uLevel;
		for (u_int i = 0; i < ZM_STAT_COUNT; ++i) { xSpec.m_auIV[i] = uIV; xSpec.m_auEV[i] = uEV; }
		xSpec.m_eNature = ZM_NATURE_FERAL;
		xSpec.m_eAbility = ZM_ABILITY_NONE;
		xSpec.m_aeMoves[0] = eM0; xSpec.m_aeMoves[1] = eM1; xSpec.m_aeMoves[2] = eM2; xSpec.m_aeMoves[3] = eM3;
		xSpec.m_bOverrideBaseStats = true;
		xSpec.m_xBaseStatsOverride.m_au[ZM_STAT_HP]        = uHP;
		xSpec.m_xBaseStatsOverride.m_au[ZM_STAT_ATTACK]    = uATK;
		xSpec.m_xBaseStatsOverride.m_au[ZM_STAT_DEFENSE]   = uDEF;
		xSpec.m_xBaseStatsOverride.m_au[ZM_STAT_SPATTACK]  = uSPA;
		xSpec.m_xBaseStatsOverride.m_au[ZM_STAT_SPDEFENSE] = uSPD;
		xSpec.m_xBaseStatsOverride.m_au[ZM_STAT_SPEED]     = uSPE;
		return xSpec;
	}

	// Real-species spec (no base-stat override) -- evolution tests + real-data
	// derivations. IV 31 / EV 0 (mainline "perfect" defaults).
	ZM_BattleMonsterSpec MakeSpecReal(ZM_SPECIES_ID eSpecies, u_int uLevel,
		ZM_MOVE_ID eM0 = ZM_MOVE_RAMBASH, ZM_MOVE_ID eM1 = ZM_MOVE_NONE,
		ZM_MOVE_ID eM2 = ZM_MOVE_NONE, ZM_MOVE_ID eM3 = ZM_MOVE_NONE)
	{
		ZM_BattleMonsterSpec xSpec;
		xSpec.m_eSpecies = eSpecies;
		xSpec.m_uLevel = uLevel;
		for (u_int i = 0; i < ZM_STAT_COUNT; ++i) { xSpec.m_auIV[i] = 31u; xSpec.m_auEV[i] = 0u; }
		xSpec.m_eNature = ZM_NATURE_FERAL;
		xSpec.m_eAbility = ZM_ABILITY_NONE;
		xSpec.m_aeMoves[0] = eM0; xSpec.m_aeMoves[1] = eM1; xSpec.m_aeMoves[2] = eM2; xSpec.m_aeMoves[3] = eM3;
		xSpec.m_bOverrideBaseStats = false;
		return xSpec;
	}

	ZM_BattleConfig MakeExpConfig(bool bAward, bool bTrainer, u_int uLevelCap = 0u,
		u_int uSideMask = 1u << (u_int)ZM_SIDE_PLAYER)
	{
		ZM_BattleConfig xCfg;
		xCfg.m_bIsWild = false;
		xCfg.m_bCanCatch = false;
		xCfg.m_bCanFlee = false;
		xCfg.m_uLevelCap = uLevelCap;
		xCfg.m_bAwardExp = bAward;
		xCfg.m_bIsTrainerBattle = bTrainer;
		xCfg.m_uExpAwardSideMask = uSideMask;
		return xCfg;
	}

	ZM_BattleAction MoveAction(u_int uSlot)
	{
		ZM_BattleAction xAction;
		xAction.m_eKind = ZM_ACTION_MOVE;
		xAction.m_uMoveSlot = uSlot;
		return xAction;
	}

	ZM_BattleAction SwitchAction(u_int uSlot)
	{
		ZM_BattleAction xAction;
		xAction.m_eKind = ZM_ACTION_SWITCH;
		xAction.m_uSwitchSlot = uSlot;
		return xAction;
	}

	// Both sides pick move slot 0 each turn until the battle terminates.
	void RunToCompletion(ZM_BattleEngine& xEngine, u_int uGuard = 500u)
	{
		u_int uTurns = 0u;
		while (!xEngine.IsOver() && uTurns < uGuard)
		{
			xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
			xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
			xEngine.ResolveTurn();
			++uTurns;
		}
	}

	ZM_BaseStats MakeYield(u_int a, u_int b, u_int c, u_int d, u_int e, u_int f)
	{
		ZM_BaseStats x;
		x.m_au[ZM_STAT_HP] = a; x.m_au[ZM_STAT_ATTACK] = b; x.m_au[ZM_STAT_DEFENSE] = c;
		x.m_au[ZM_STAT_SPATTACK] = d; x.m_au[ZM_STAT_SPDEFENSE] = e; x.m_au[ZM_STAT_SPEED] = f;
		return x;
	}

	// First learnset entry whose level >= 2 (a genuine mid-level learn -- entry 0
	// is always level 1). Returns false if a species has none (shouldn't happen).
	bool FirstLevelUpEntry(ZM_SPECIES_ID eId, u_int& uLevelOut, ZM_MOVE_ID& eMoveOut)
	{
		const ZM_Learnset xLs = ZM_GetSpeciesLearnset(eId);
		for (u_int k = 0; k < xLs.m_uCount; ++k)
		{
			if (xLs.m_axMoves[k].m_uLevel >= 2u)
			{
				uLevelOut = xLs.m_axMoves[k].m_uLevel;
				eMoveOut  = xLs.m_axMoves[k].m_eMove;
				return true;
			}
		}
		return false;
	}
}

// ############################################################################
// A. Exp-curve math (ZM_Data)
// ############################################################################

// Each curve's exact cumulative total at level 100 (spec section 1.1).
ZENITH_TEST(ZM_Data, ExpCurve_L100Anchors)
{
	ZENITH_ASSERT_EQ(ZM_ExpForLevel(ZM_GROWTH_FAST,        100u),  800000u);
	ZENITH_ASSERT_EQ(ZM_ExpForLevel(ZM_GROWTH_MEDIUM_FAST, 100u), 1000000u);
	ZENITH_ASSERT_EQ(ZM_ExpForLevel(ZM_GROWTH_MEDIUM_SLOW, 100u), 1059860u);
	ZENITH_ASSERT_EQ(ZM_ExpForLevel(ZM_GROWTH_SLOW,        100u), 1250000u);
}

// The four curves at level 50.
ZENITH_TEST(ZM_Data, ExpCurve_L50Anchors)
{
	ZENITH_ASSERT_EQ(ZM_ExpForLevel(ZM_GROWTH_FAST,        50u), 100000u);
	ZENITH_ASSERT_EQ(ZM_ExpForLevel(ZM_GROWTH_MEDIUM_FAST, 50u), 125000u);
	ZENITH_ASSERT_EQ(ZM_ExpForLevel(ZM_GROWTH_MEDIUM_SLOW, 50u), 117360u);
	ZENITH_ASSERT_EQ(ZM_ExpForLevel(ZM_GROWTH_SLOW,        50u), 156250u);
}

// Level 1 is 0 for every curve (special-cased); level 2 anchors (spec section 1.1).
ZENITH_TEST(ZM_Data, ExpCurve_L1ZeroAndL2Anchors)
{
	ZENITH_ASSERT_EQ(ZM_ExpForLevel(ZM_GROWTH_FAST,        1u), 0u);
	ZENITH_ASSERT_EQ(ZM_ExpForLevel(ZM_GROWTH_MEDIUM_FAST, 1u), 0u);
	ZENITH_ASSERT_EQ(ZM_ExpForLevel(ZM_GROWTH_MEDIUM_SLOW, 1u), 0u);
	ZENITH_ASSERT_EQ(ZM_ExpForLevel(ZM_GROWTH_SLOW,        1u), 0u);

	ZENITH_ASSERT_EQ(ZM_ExpForLevel(ZM_GROWTH_FAST,        2u),  6u);
	ZENITH_ASSERT_EQ(ZM_ExpForLevel(ZM_GROWTH_MEDIUM_FAST, 2u),  8u);
	ZENITH_ASSERT_EQ(ZM_ExpForLevel(ZM_GROWTH_MEDIUM_SLOW, 2u),  9u);
	ZENITH_ASSERT_EQ(ZM_ExpForLevel(ZM_GROWTH_SLOW,        2u), 10u);
}

// Independent low/mid/high vectors for every family. These literals are not
// produced through either curve function and therefore catch shared drift.
ZENITH_TEST(ZM_Data, ExpCurve_LowMidHighLiteralVectors)
{
	struct ZM_CurveVector
	{
		u_int m_uLevel;
		u_int m_auExp[ZM_GROWTH_COUNT];
	};
	const ZM_CurveVector axVectors[] =
	{
		{ 5u,  { 100u, 125u, 135u, 156u } },
		{ 25u, { 12500u, 15625u, 11735u, 19531u } },
		{ 75u, { 337500u, 421875u, 429235u, 527343u } },
	};
	for (u_int v = 0u; v < (u_int)(sizeof(axVectors) / sizeof(axVectors[0])); ++v)
	{
		for (u_int r = 0u; r < (u_int)ZM_GROWTH_COUNT; ++r)
		{
			ZENITH_ASSERT_EQ(ZM_ExpForLevel((ZM_GROWTH_RATE)r, axVectors[v].m_uLevel),
				axVectors[v].m_auExp[r], "literal curve %u level %u", r, axVectors[v].m_uLevel);
		}
	}
}

// Every adjacent threshold is strictly increasing, and the inverse resolves both
// the exact threshold and threshold-minus-one at every level for every curve.
ZENITH_TEST(ZM_Data, ExpCurve_AllLevelsStrictAndInverseBoundary)
{
	for (u_int r = 0; r < ZM_GROWTH_COUNT; ++r)
	{
		const ZM_GROWTH_RATE eRate = (ZM_GROWTH_RATE)r;
		u_int uPrev = ZM_ExpForLevel(eRate, 1u);
		for (u_int uLevel = 2u; uLevel <= 100u; ++uLevel)
		{
			const u_int uCur = ZM_ExpForLevel(eRate, uLevel);
			ZENITH_ASSERT_GT(uCur, uPrev, "curve %u not strictly increasing at L%u", r, uLevel);
			ZENITH_ASSERT_EQ(ZM_LevelForExp(eRate, uCur), uLevel,
				"curve %u exact threshold inverse at L%u", r, uLevel);
			ZENITH_ASSERT_EQ(ZM_LevelForExp(eRate, uCur - 1u), uLevel - 1u,
				"curve %u threshold-minus-one inverse at L%u", r, uLevel);
			uPrev = uCur;
		}
	}
}

// LevelForExp clamps to [1,100]: exp 0 -> 1, exp above the L100 total -> 100.
ZENITH_TEST(ZM_Data, ExpCurve_LevelForExpClamps)
{
	for (u_int r = 0; r < ZM_GROWTH_COUNT; ++r)
	{
		const ZM_GROWTH_RATE eRate = (ZM_GROWTH_RATE)r;
		ZENITH_ASSERT_EQ(ZM_LevelForExp(eRate, 0u), 1u, "exp 0 should map to L1 (curve %u)", r);
		ZENITH_ASSERT_EQ(ZM_LevelForExp(eRate, 5000000u), 100u, "huge exp should clamp to L100 (curve %u)", r);
		// One-below-cap total still resolves below 100; the exact cap total is 100.
		ZENITH_ASSERT_EQ(ZM_LevelForExp(eRate, ZM_ExpForLevel(eRate, 100u)), 100u, "cap total -> L100 (curve %u)", r);
	}
}

// MEDIUM_SLOW dips negative near n=1..2 in closed form; the implementation must
// clamp to 0 (no underflow / huge unsigned value) at low levels.
ZENITH_TEST(ZM_Data, ExpCurve_MediumSlowLowNClamp)
{
	ZENITH_ASSERT_EQ(ZM_ExpForLevel(ZM_GROWTH_MEDIUM_SLOW, 1u), 0u);
	ZENITH_ASSERT_EQ(ZM_ExpForLevel(ZM_GROWTH_MEDIUM_SLOW, 2u), 9u);
	// No underflow: L3 stays a small sane positive, and the curve is ordered.
	const u_int uL3 = ZM_ExpForLevel(ZM_GROWTH_MEDIUM_SLOW, 3u);
	ZENITH_ASSERT_GE(uL3, ZM_ExpForLevel(ZM_GROWTH_MEDIUM_SLOW, 2u));
	ZENITH_ASSERT_LT(uL3, 1000u, "MEDIUM_SLOW L3 underflowed to a huge value");
}

// ############################################################################
// B. Per-species derived accessors (ZM_Data)
// ############################################################################

// Growth rate is derived from rarity: COMMON->FAST, UNCOMMON->MEDIUM_FAST,
// RARE->MEDIUM_SLOW, LEGENDARY->SLOW (spec section 1.2).
ZENITH_TEST(ZM_Data, GrowthRate_FromRarity)
{
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesGrowthRate(ZM_SPECIES_NIBBIN),   (u_int)ZM_GROWTH_FAST);         // COMMON
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesGrowthRate(ZM_SPECIES_STRAYLING),(u_int)ZM_GROWTH_FAST);         // COMMON
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesGrowthRate(ZM_SPECIES_SPARKIT),  (u_int)ZM_GROWTH_MEDIUM_FAST);  // UNCOMMON
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesGrowthRate(ZM_SPECIES_FERNFAWN), (u_int)ZM_GROWTH_MEDIUM_SLOW);  // RARE (starter)
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesGrowthRate(ZM_SPECIES_ZENARIS),  (u_int)ZM_GROWTH_SLOW);         // LEGENDARY
}

// Base exp yield == max(1, BST*2/5), pinned against the (independently-tested)
// base-stat-total accessor (spec section 1.3).
ZENITH_TEST(ZM_Data, BaseExpYield_Formula)
{
	const ZM_SPECIES_ID aeCheck[] = { ZM_SPECIES_NIBBIN, ZM_SPECIES_SYLVASTAG, ZM_SPECIES_ZENARIS };
	for (u_int i = 0; i < (u_int)(sizeof(aeCheck) / sizeof(aeCheck[0])); ++i)
	{
		const ZM_SPECIES_ID eId = aeCheck[i];
		const u_int uBst = ZM_GetSpeciesBaseStatTotal(eId);
		u_int uExpected = (uBst * 2u) / 5u;
		if (uExpected == 0u) { uExpected = 1u; }
		ZENITH_ASSERT_EQ(ZM_GetSpeciesBaseExpYield(eId), uExpected,
			"%s base-exp yield != max(1, BST*2/5)", ZM_GetSpeciesName(eId));
	}
}

// Base exp yield is non-decreasing along an evolution chain (BST grows per stage).
ZENITH_TEST(ZM_Data, BaseExpYield_NonDecreasingOnChain)
{
	const u_int uStage1 = ZM_GetSpeciesBaseExpYield(ZM_SPECIES_FERNFAWN);
	const u_int uStage2 = ZM_GetSpeciesBaseExpYield(ZM_SPECIES_THICKETBUCK);
	const u_int uStage3 = ZM_GetSpeciesBaseExpYield(ZM_SPECIES_SYLVASTAG);
	ZENITH_ASSERT_LE(uStage1, uStage2, "F01 stage1 exp yield > stage2");
	ZENITH_ASSERT_LE(uStage2, uStage3, "F01 stage2 exp yield > stage3");
}

// EV yield sets exactly one stat (the species' highest base stat; ties -> lowest
// index) to the evo-stage amount (spec section 1.5).
ZENITH_TEST(ZM_Data, EVYield_SingleStatAndStageAmount)
{
	// stage1 (evolves) -> 1, stage2 -> 2, stage3 -> 3, single-stage-final -> 3.
	struct { ZM_SPECIES_ID eId; u_int uAmount; } aCase[] = {
		{ ZM_SPECIES_NIBBIN,   1u },   // stage1, evolves
		{ ZM_SPECIES_HOARDEL,  2u },   // stage2, evolves
		{ ZM_SPECIES_GRAINMAW, 3u },   // stage3, final
		{ ZM_SPECIES_AURICORN, 3u },   // single-stage final (evoStage 1, evolvesTo NONE)
	};

	for (u_int c = 0; c < (u_int)(sizeof(aCase) / sizeof(aCase[0])); ++c)
	{
		const ZM_SPECIES_ID eId = aCase[c].eId;
		const ZM_BaseStats xBase = ZM_GetSpeciesBaseStats(eId);

		// Expected max-stat index: strictly-greater keeps the lowest index on a tie.
		u_int uMaxIdx = 0u;
		for (u_int i = 1u; i < ZM_STAT_COUNT; ++i)
		{
			if (xBase.m_au[i] > xBase.m_au[uMaxIdx]) { uMaxIdx = i; }
		}

		const ZM_BaseStats xYield = ZM_GetSpeciesEVYield(eId);
		for (u_int i = 0; i < ZM_STAT_COUNT; ++i)
		{
			const u_int uWant = (i == uMaxIdx) ? aCase[c].uAmount : 0u;
			ZENITH_ASSERT_EQ(xYield.m_au[i], uWant,
				"%s EV yield stat %u", ZM_GetSpeciesName(eId), i);
		}
	}
}

// Evolve level: stage1-with-evolves-to -> 16, stage2-with-evolves-to -> 36,
// final (evolvesTo NONE) -> 0 (spec section 1.6).
ZENITH_TEST(ZM_Data, EvolveLevel_ByStage)
{
	ZENITH_ASSERT_EQ(ZM_GetSpeciesEvolveLevel(ZM_SPECIES_NIBBIN),   16u);  // stage1 -> 16
	ZENITH_ASSERT_EQ(ZM_GetSpeciesEvolveLevel(ZM_SPECIES_HOARDEL),  36u);  // stage2 -> 36
	ZENITH_ASSERT_EQ(ZM_GetSpeciesEvolveLevel(ZM_SPECIES_GRAINMAW),  0u);  // stage3 final
	ZENITH_ASSERT_EQ(ZM_GetSpeciesEvolveLevel(ZM_SPECIES_WARDHUND),  0u);  // 2-stage family final
	ZENITH_ASSERT_EQ(ZM_GetSpeciesEvolveLevel(ZM_SPECIES_AURICORN),  0u);  // single-stage
}

// ############################################################################
// C. Exp-award formula (ZM_Data)
// ############################################################################

// Independent literal anchors: Fernfawn's derived base yield is 133, so L5 is
// wild 95 / trainer 142; modern nonparticipant shares are 47 / 71 respectively.
// Expected values are deliberately not derived through the function under test.
ZENITH_TEST(ZM_Data, CalcExpGain_LiteralWildTrainerAndHalfAnchors)
{
	ZENITH_ASSERT_EQ(ZM_GetSpeciesBaseExpYield(ZM_SPECIES_FERNFAWN), 133u);
	ZENITH_ASSERT_EQ(ZM_CalcExpGain(ZM_SPECIES_FERNFAWN, 5u, false), 95u);
	ZENITH_ASSERT_EQ(ZM_CalcExpGain(ZM_SPECIES_FERNFAWN, 5u, true), 142u);
	ZENITH_ASSERT_EQ(ZM_CalcExpGain(ZM_SPECIES_FERNFAWN, 5u, false) / 2u, 47u);
	ZENITH_ASSERT_EQ(ZM_CalcExpGain(ZM_SPECIES_FERNFAWN, 5u, true) / 2u, 71u);
}

// Trainer exp is the x1.5 variant (applied AFTER the /7 divide) and never less
// than the wild value.
ZENITH_TEST(ZM_Data, CalcExpGain_TrainerRoundingOrderLiteralL1)
{
	// Nibbin base yield 110: floor(110/7)=15, then floor(15*3/2)=22.
	// Scaling before /7 would produce 23, so this fixture discriminates ordering.
	ZENITH_ASSERT_EQ(ZM_GetSpeciesBaseExpYield(ZM_SPECIES_NIBBIN), 110u);
	ZENITH_ASSERT_EQ(ZM_CalcExpGain(ZM_SPECIES_NIBBIN, 1u, false), 15u);
	ZENITH_ASSERT_EQ(ZM_CalcExpGain(ZM_SPECIES_NIBBIN, 1u, true), 22u);
}

// Exp is always at least 1, even for the lowest species/level inputs.
ZENITH_TEST(ZM_Data, CalcExpGain_AlwaysAtLeastOne)
{
	const ZM_SPECIES_ID aeCheck[] = { ZM_SPECIES_NIBBIN, ZM_SPECIES_PIPWIT, ZM_SPECIES_FERNFAWN };
	for (u_int i = 0; i < (u_int)(sizeof(aeCheck) / sizeof(aeCheck[0])); ++i)
	{
		ZENITH_ASSERT_GE(ZM_CalcExpGain(aeCheck[i], 1u, false), 1u, "wild exp floor");
		ZENITH_ASSERT_GE(ZM_CalcExpGain(aeCheck[i], 1u, true),  1u, "trainer exp floor");
	}
}

// Hostile defeated levels clamp to 1..100 before wide multiplication; UINT_MAX
// must be identical to level 100 rather than wrapping through a small award.
ZENITH_TEST(ZM_Data, CalcExpGain_HostileLevelBoundsClamp)
{
	ZENITH_ASSERT_EQ(ZM_CalcExpGain(ZM_SPECIES_FERNFAWN, 0u, false),
		ZM_CalcExpGain(ZM_SPECIES_FERNFAWN, 1u, false));
	ZENITH_ASSERT_EQ(ZM_CalcExpGain(ZM_SPECIES_FERNFAWN, ~0u, true),
		ZM_CalcExpGain(ZM_SPECIES_FERNFAWN, 100u, true));
}

// ############################################################################
// D. EV accumulation + caps (ZM_Data)
// ############################################################################

// AddEVYield adds into the correct stat, leaving the others untouched.
ZENITH_TEST(ZM_Data, AddEVYield_AddsIntoStat)
{
	u_int auEV[ZM_STAT_COUNT] = { 0u, 0u, 0u, 0u, 0u, 0u };
	ZM_AddEVYield(auEV, MakeYield(0u, 3u, 0u, 0u, 0u, 0u));   // +3 ATTACK
	ZENITH_ASSERT_EQ(auEV[ZM_STAT_ATTACK], 3u);
	ZENITH_ASSERT_EQ(auEV[ZM_STAT_HP], 0u);
	ZENITH_ASSERT_EQ(auEV[ZM_STAT_DEFENSE], 0u);
	ZENITH_ASSERT_EQ(auEV[ZM_STAT_SPEED], 0u);
}

// Per-stat cap is 252 regardless of how much is added.
ZENITH_TEST(ZM_Data, AddEVYield_PerStatCap252)
{
	u_int auEV[ZM_STAT_COUNT] = { 0u, 0u, 0u, 0u, 0u, 0u };
	ZM_AddEVYield(auEV, MakeYield(3u,   0u, 0u, 0u, 0u, 0u));
	ZM_AddEVYield(auEV, MakeYield(300u, 0u, 0u, 0u, 0u, 0u));   // overshoots -> clamps
	ZENITH_ASSERT_EQ(auEV[ZM_STAT_HP], 252u);
}

// Total cap is 510: filling to 508 then adding a 3-yield lands only 2, and any
// further add lands 0.
ZENITH_TEST(ZM_Data, AddEVYield_TotalCap510)
{
	u_int auEV[ZM_STAT_COUNT] = { 252u, 252u, 4u, 0u, 0u, 0u };   // total 508
	ZM_AddEVYield(auEV, MakeYield(0u, 0u, 0u, 3u, 0u, 0u));       // SPATTACK +3 -> only 2 fit
	ZENITH_ASSERT_EQ(auEV[ZM_STAT_SPATTACK], 2u);

	u_int uTotal = 0u;
	for (u_int i = 0; i < ZM_STAT_COUNT; ++i) { uTotal += auEV[i]; }
	ZENITH_ASSERT_EQ(uTotal, 510u);

	ZM_AddEVYield(auEV, MakeYield(0u, 0u, 0u, 0u, 5u, 0u));       // total already 510 -> 0 land
	ZENITH_ASSERT_EQ(auEV[ZM_STAT_SPDEFENSE], 0u);
	uTotal = 0u;
	for (u_int i = 0; i < ZM_STAT_COUNT; ++i) { uTotal += auEV[i]; }
	ZENITH_ASSERT_EQ(uTotal, 510u);
}

// Repeated yields accumulate; a maxed stat stops growing while others still can,
// until the running total reaches 510.
ZENITH_TEST(ZM_Data, AddEVYield_RepeatedAccumulateMaxedStops)
{
	u_int auEV[ZM_STAT_COUNT] = { 0u, 0u, 0u, 0u, 0u, 0u };
	ZM_AddEVYield(auEV, MakeYield(200u, 0u, 0u, 0u, 0u, 0u));   // HP 200
	ZM_AddEVYield(auEV, MakeYield(200u, 0u, 0u, 0u, 0u, 0u));   // HP -> 252 (per-stat cap)
	ZM_AddEVYield(auEV, MakeYield(0u, 200u, 0u, 0u, 0u, 0u));   // ATK 200 (total 452)
	ZM_AddEVYield(auEV, MakeYield(0u, 0u, 200u, 0u, 0u, 0u));   // DEF: only 58 fits (total 510)
	ZM_AddEVYield(auEV, MakeYield(0u, 0u, 0u, 100u, 0u, 0u));   // SPATK: 0 fits

	ZENITH_ASSERT_EQ(auEV[ZM_STAT_HP],       252u);
	ZENITH_ASSERT_EQ(auEV[ZM_STAT_ATTACK],   200u);
	ZENITH_ASSERT_EQ(auEV[ZM_STAT_DEFENSE],   58u);
	ZENITH_ASSERT_EQ(auEV[ZM_STAT_SPATTACK],   0u);

	u_int uTotal = 0u;
	for (u_int i = 0; i < ZM_STAT_COUNT; ++i) { uTotal += auEV[i]; }
	ZENITH_ASSERT_EQ(uTotal, 510u);
}

// Build normalizes hostile input in canonical stat order: per-stat 252, total
// 510, leaving {252,252,6,0,0,0} from six UINT_MAX values.
ZENITH_TEST(ZM_Data, EVNormalize_BuildCanonicalCaps)
{
	ZM_BattleMonsterSpec xSpec = MakeSpecReal(ZM_SPECIES_NIBBIN, 10u);
	for (u_int i = 0u; i < ZM_STAT_COUNT; ++i) { xSpec.m_auEV[i] = ~0u; }
	const ZM_BattleMonster xMon = ZM_BuildBattleMonster(xSpec);
	ZENITH_ASSERT_EQ(xMon.m_auEV[ZM_STAT_HP], 252u);
	ZENITH_ASSERT_EQ(xMon.m_auEV[ZM_STAT_ATTACK], 252u);
	ZENITH_ASSERT_EQ(xMon.m_auEV[ZM_STAT_DEFENSE], 6u);
	ZENITH_ASSERT_EQ(xMon.m_auEV[ZM_STAT_SPATTACK], 0u);
	ZENITH_ASSERT_EQ(xMon.m_auEV[ZM_STAT_SPDEFENSE], 0u);
	ZENITH_ASSERT_EQ(xMon.m_auEV[ZM_STAT_SPEED], 0u);
}

// AddEVYield repairs malformed persisted input before adding and uses wide totals.
ZENITH_TEST(ZM_Data, EVNormalize_AddYieldDefensiveUINTMax)
{
	u_int auEV[ZM_STAT_COUNT] = { ~0u, ~0u, ~0u, ~0u, ~0u, ~0u };
	ZM_AddEVYield(auEV, MakeYield(~0u, ~0u, ~0u, ~0u, ~0u, ~0u));
	ZENITH_ASSERT_EQ(auEV[ZM_STAT_HP], 252u);
	ZENITH_ASSERT_EQ(auEV[ZM_STAT_ATTACK], 252u);
	ZENITH_ASSERT_EQ(auEV[ZM_STAT_DEFENSE], 6u);
	ZENITH_ASSERT_EQ(auEV[ZM_STAT_SPATTACK], 0u);
	ZENITH_ASSERT_EQ(auEV[ZM_STAT_SPDEFENSE], 0u);
	ZENITH_ASSERT_EQ(auEV[ZM_STAT_SPEED], 0u);
}

// ############################################################################
// E. Level-up recompute (ZM_Data, ZM_BuildBattleMonster w/ override base stats)
// ############################################################################

// Build sets m_uCurExp to the curve total for the build level (spec section 3.3).
ZENITH_TEST(ZM_Data, LevelUp_BuildCurExpMatchesCurve)
{
	// Grainmaw is COMMON -> FAST; ExpForLevel(FAST,10) == 800.
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(
		MakeSpecOv(ZM_SPECIES_GRAINMAW, 10u, 100u, 100u, 100u, 100u, 100u, 100u, 0u, 0u, ZM_MOVE_RAMBASH));
	ZENITH_ASSERT_EQ(xMon.m_uCurExp, ZM_ExpForLevel(ZM_GROWTH_FAST, 10u));
	ZENITH_ASSERT_EQ(xMon.m_uCurExp, 800u);
	// Stored base stats came from the override (level-up recompute source).
	ZENITH_ASSERT_EQ(xMon.m_xBaseStats.m_au[ZM_STAT_HP], 100u);
	ZENITH_ASSERT_EQ(xMon.m_xBaseStats.m_au[ZM_STAT_SPEED], 100u);
}

ZENITH_TEST(ZM_Data, LevelUp_CurrentPartialExpBuildAndBegin)
{
	ZM_BattleMonsterSpec xPlayer = MakeSpecReal(ZM_SPECIES_GRAINMAW, 10u, ZM_MOVE_RAMBASH);
	xPlayer.m_uCurExp = 900u;   // FAST L10 band is [800,1063]
	const ZM_BattleMonster xBuilt = ZM_BuildBattleMonster(xPlayer);
	ZENITH_ASSERT_EQ(xBuilt.m_uCurExp, 900u);

	ZM_BattleMonsterSpec xEnemy = MakeSpecReal(ZM_SPECIES_FERNFAWN, 5u, ZM_MOVE_RAMBASH);
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeExpConfig(false, false), &xPlayer, 1u, &xEnemy, 1u, 1ull, 54ull);
	ZENITH_ASSERT_EQ(xEngine.GetState().Side(ZM_SIDE_PLAYER).Active().m_uCurExp, 900u);
}

ZENITH_TEST(ZM_Data, LevelUp_CurrentExpSentinelAndMalformedBandsNormalize)
{
	ZM_BattleMonsterSpec xSpec = MakeSpecReal(ZM_SPECIES_GRAINMAW, 10u);
	ZENITH_ASSERT_EQ(xSpec.m_uCurExp, uZM_EXP_UNSPECIFIED);
	ZENITH_ASSERT_EQ(ZM_BuildBattleMonster(xSpec).m_uCurExp, 800u);

	xSpec.m_uCurExp = 800u;     // exact lower edge is retained
	ZENITH_ASSERT_EQ(ZM_BuildBattleMonster(xSpec).m_uCurExp, 800u);
	xSpec.m_uCurExp = 1063u;    // exact upper edge (next threshold - 1) is retained
	ZENITH_ASSERT_EQ(ZM_BuildBattleMonster(xSpec).m_uCurExp, 1063u);
	xSpec.m_uCurExp = 0u;       // explicit zero is data, not the omitted sentinel
	ZENITH_ASSERT_EQ(ZM_BuildBattleMonster(xSpec).m_uCurExp, 800u);
	xSpec.m_uCurExp = 500000u;  // above the L11 threshold -> top of the L10 band
	ZENITH_ASSERT_EQ(ZM_BuildBattleMonster(xSpec).m_uCurExp, 1063u);

	xSpec.m_uLevel = 100u;
	xSpec.m_uCurExp = 123u;     // L100 has one deterministic terminal value
	ZENITH_ASSERT_EQ(ZM_BuildBattleMonster(xSpec).m_uCurExp, 800000u);
}

// A single-threshold award levels the mon once, recomputes all six stats from the
// STORED base stats, carries the HP delta, and emits EXP_GAINED then LEVEL_UP.
ZENITH_TEST(ZM_Data, LevelUp_SingleLevelExactStream)
{
	// Four FIRE moves fill every slot -> a level-up move-learn is suppressed
	// (Grainmaw is NORMAL-typed and final-stage, so no move-learn / evolution).
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(
		MakeSpecOv(ZM_SPECIES_GRAINMAW, 10u, 100u, 100u, 100u, 100u, 100u, 100u, 0u, 0u,
			ZM_MOVE_CINDERSPIT, ZM_MOVE_FLARELASH, ZM_MOVE_PYREBURST, ZM_MOVE_MAGMAFALL));

	const u_int uOldHP = xMon.m_auMaxStat[ZM_STAT_HP];
	ZENITH_ASSERT_EQ(uOldHP, 40u);                 // ZM_CalcStat(HP,100,0,0,10,FERAL)
	ZENITH_ASSERT_EQ(xMon.m_uCurHP, 40u);          // full at build

	const u_int uNewExp = 1064u;
	const u_int uGain   = 264u;

	Zenith_Vector<ZM_BattleEvent> xEvents;
	const u_int uGained = ZM_ApplyExpGain(xMon, uGain, xEvents, ZM_SIDE_PLAYER, 0u, 100u);
	ZENITH_ASSERT_EQ(uGained, 1u);
	ZENITH_ASSERT_EQ(xMon.m_uLevel, 11u);
	ZENITH_ASSERT_EQ(xMon.m_uCurExp, uNewExp);

	// Independent literal stat vector for base=100, IV=EV=0, neutral, level 11.
	const u_int auExpectedStats[ZM_STAT_COUNT] = { 43u, 27u, 27u, 27u, 27u, 27u };
	for (u_int i = 0; i < ZM_STAT_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ(xMon.m_auMaxStat[i], auExpectedStats[i], "literal stat %u at L11", i);
	}
	const u_int uNewHP = 43u;
	// Full-HP mon carries the delta -> full at the new max.
	ZENITH_ASSERT_EQ(xMon.m_uCurHP, uNewHP);

	Zenith_Vector<ZM_BattleEvent> xExpected;
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_EXP_GAINED, ZM_SIDE_PLAYER, 0u, ZM_MOVE_NONE,
		ZM_SPECIES_GRAINMAW, (int)uGain, (int)uNewExp));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_LEVEL_UP, ZM_SIDE_PLAYER, 0u, ZM_MOVE_NONE,
		ZM_SPECIES_GRAINMAW, 11, (int)uNewHP));
	AssertStreamEq(xExpected, xEvents, "single-level");
}

// One large award crosses several thresholds; the loop lands on LevelForExp and
// emits one LEVEL_UP per level gained.
ZENITH_TEST(ZM_Data, LevelUp_MultiLevelLoops)
{
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(
		MakeSpecOv(ZM_SPECIES_GRAINMAW, 10u, 100u, 100u, 100u, 100u, 100u, 100u, 0u, 0u,
			ZM_MOVE_CINDERSPIT, ZM_MOVE_FLARELASH, ZM_MOVE_PYREBURST, ZM_MOVE_MAGMAFALL));

	const u_int uTargetExp = 6400u;
	const u_int uGain = 5600u;

	Zenith_Vector<ZM_BattleEvent> xEvents;
	const u_int uGained = ZM_ApplyExpGain(xMon, uGain, xEvents, ZM_SIDE_PLAYER, 0u, 100u);

	ZENITH_ASSERT_EQ(uGained, 10u);
	ZENITH_ASSERT_EQ(xMon.m_uLevel, 20u);
	ZENITH_ASSERT_EQ(xMon.m_uCurExp, uTargetExp);

	const u_int auExpectedStats[ZM_STAT_COUNT] = { 70u, 45u, 45u, 45u, 45u, 45u };
	for (u_int i = 0; i < ZM_STAT_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ(xMon.m_auMaxStat[i], auExpectedStats[i], "literal stat %u at L20", i);
	}

	Zenith_Vector<ZM_BattleEvent> xExpected;
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_EXP_GAINED, ZM_SIDE_PLAYER, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_GRAINMAW, 5600, 6400));
	const u_int auLevelHP[10] = { 43u, 46u, 49u, 52u, 55u, 58u, 61u, 64u, 67u, 70u };
	for (u_int i = 0u; i < 10u; ++i)
	{
		xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_LEVEL_UP, ZM_SIDE_PLAYER, 0u,
			ZM_MOVE_NONE, ZM_SPECIES_GRAINMAW, (int)(11u + i), (int)auLevelHP[i]));
	}
	AssertStreamEq(xExpected, xEvents, "multi-level-literal");
}

// The HP delta is ADDED to current HP (not reset to full) for a damaged mon.
ZENITH_TEST(ZM_Data, LevelUp_HPDeltaCarryDamaged)
{
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(
		MakeSpecOv(ZM_SPECIES_GRAINMAW, 10u, 100u, 100u, 100u, 100u, 100u, 100u, 0u, 0u,
			ZM_MOVE_CINDERSPIT, ZM_MOVE_FLARELASH, ZM_MOVE_PYREBURST, ZM_MOVE_MAGMAFALL));

	const u_int uOldHP = xMon.m_auMaxStat[ZM_STAT_HP];     // 40
	xMon.m_uCurHP = uOldHP - 5u;                            // damaged to 35

	Zenith_Vector<ZM_BattleEvent> xEvents;
	const u_int uGain = ZM_ExpForLevel(ZM_GROWTH_FAST, 11u) - xMon.m_uCurExp;
	ZM_ApplyExpGain(xMon, uGain, xEvents, ZM_SIDE_PLAYER, 0u, 100u);

	const u_int uNewHP = xMon.m_auMaxStat[ZM_STAT_HP];      // 43
	ZENITH_ASSERT_EQ(xMon.m_uCurHP, (uOldHP - 5u) + (uNewHP - uOldHP));   // 38, NOT 43
	ZENITH_ASSERT_EQ(xMon.m_uCurHP, uNewHP - 5u);
}

// A mon already at the level cap: ZM_ApplyExpGain returns 0 and emits nothing.
ZENITH_TEST(ZM_Data, LevelUp_AtCapNoOp)
{
	// At L100 (natural cap).
	ZM_BattleMonster xMax = ZM_BuildBattleMonster(
		MakeSpecOv(ZM_SPECIES_GRAINMAW, 100u, 100u, 100u, 100u, 100u, 100u, 100u, 0u, 0u, ZM_MOVE_RAMBASH));
	const ZM_BattleMonster xMaxBefore = xMax;
	Zenith_Vector<ZM_BattleEvent> xE1;
	ZENITH_ASSERT_EQ(ZM_ApplyExpGain(xMax, 1000000u, xE1, ZM_SIDE_PLAYER, 0u, 100u), 0u);
	ZENITH_ASSERT_EQ(xE1.GetSize(), 0u);
	ZENITH_ASSERT_EQ(xMax.m_uLevel, 100u);
	ZENITH_ASSERT_EQ(xMax.m_uCurExp, xMaxBefore.m_uCurExp);
	ZENITH_ASSERT_EQ(xMax.m_uCurHP, xMaxBefore.m_uCurHP);
	ZENITH_ASSERT_EQ((u_int)xMax.m_eSpecies, (u_int)xMaxBefore.m_eSpecies);
	ZENITH_ASSERT_FALSE(xMax.m_bLevelledThisBattle);
	ZENITH_ASSERT_FALSE(xMax.m_bEvolutionQueued);
	for (u_int i = 0u; i < ZM_STAT_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ(xMax.m_auMaxStat[i], xMaxBefore.m_auMaxStat[i]);
		ZENITH_ASSERT_EQ(xMax.m_auEV[i], xMaxBefore.m_auEV[i]);
		ZENITH_ASSERT_EQ(xMax.m_auIV[i], xMaxBefore.m_auIV[i]);
	}
	for (u_int i = 0u; i < uZM_MAX_MOVES; ++i)
	{
		ZENITH_ASSERT_EQ((u_int)xMax.m_axMoves[i].m_eMove, (u_int)xMaxBefore.m_axMoves[i].m_eMove);
		ZENITH_ASSERT_EQ(xMax.m_axMoves[i].m_uCurPP, xMaxBefore.m_axMoves[i].m_uCurPP);
	}

	// At an explicit uMaxLevel == current level.
	ZM_BattleMonster xCapped = ZM_BuildBattleMonster(
		MakeSpecOv(ZM_SPECIES_GRAINMAW, 50u, 100u, 100u, 100u, 100u, 100u, 100u, 0u, 0u, ZM_MOVE_RAMBASH));
	const ZM_BattleMonster xCappedBefore = xCapped;
	Zenith_Vector<ZM_BattleEvent> xE2;
	ZENITH_ASSERT_EQ(ZM_ApplyExpGain(xCapped, 1000000u, xE2, ZM_SIDE_PLAYER, 0u, 50u), 0u);
	ZENITH_ASSERT_EQ(xE2.GetSize(), 0u);
	AssertNonProgressionMonsterEq(xCappedBefore, xCapped, ZM_SIDE_PLAYER, 0u);
	ZENITH_ASSERT_EQ(xCapped.m_uCurExp, xCappedBefore.m_uCurExp);
	ZENITH_ASSERT_EQ(xCapped.m_uParticipantMask, xCappedBefore.m_uParticipantMask);
	ZENITH_ASSERT_EQ(xCapped.m_bDefeatCredited, xCappedBefore.m_bDefeatCredited);
	ZENITH_ASSERT_EQ(xCapped.m_bLevelledThisBattle, xCappedBefore.m_bLevelledThisBattle);
	ZENITH_ASSERT_EQ(xCapped.m_bEvolutionQueued, xCappedBefore.m_bEvolutionQueued);
}

// A uMaxLevel below the reachable level clamps the loop and the stored exp total.
ZENITH_TEST(ZM_Data, LevelUp_LevelCapParamClamps)
{
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(
		MakeSpecOv(ZM_SPECIES_GRAINMAW, 10u, 100u, 100u, 100u, 100u, 100u, 100u, 0u, 0u,
			ZM_MOVE_CINDERSPIT, ZM_MOVE_FLARELASH, ZM_MOVE_PYREBURST, ZM_MOVE_MAGMAFALL));

	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_ApplyExpGain(xMon, 1000000u, xEvents, ZM_SIDE_PLAYER, 0u, 15u);   // cap L15

	ZENITH_ASSERT_EQ(xMon.m_uLevel, 15u);
	ZENITH_ASSERT_EQ(xMon.m_uCurExp, ZM_ExpForLevel(ZM_GROWTH_FAST, 15u));   // 2700 (clamped)
	// The event reports the amount actually credited into the cap room.
	const ZM_BattleEvent* pxExp = FindKind(xEvents, ZM_BATTLE_EVENT_EXP_GAINED);
	ZENITH_ASSERT_NOT_NULL(pxExp);
	if (pxExp)
	{
		ZENITH_ASSERT_EQ(pxExp->m_iAmount, 1900);
		ZENITH_ASSERT_EQ(pxExp->m_iAux, (int)ZM_ExpForLevel(ZM_GROWTH_FAST, 15u));
	}
	// No LEVEL_UP past the cap.
	for (u_int i = 0; i < xEvents.GetSize(); ++i)
	{
		if (xEvents.Get(i).m_eKind == ZM_BATTLE_EVENT_LEVEL_UP)
		{
			ZENITH_ASSERT_LE(xEvents.Get(i).m_iAmount, 15, "a LEVEL_UP exceeded the cap");
		}
	}
}

// Zero gain is inert; UINT_MAX gain saturates to room without wrapping. A zero
// max-level argument means the natural L100 cap; a positive cap below the current
// level is a strict no-op and never delevels.
ZENITH_TEST(ZM_Data, LevelUp_ZeroUINTMaxAndHostileCapBounds)
{
	ZM_BattleMonster xZero = ZM_BuildBattleMonster(MakeSpecReal(ZM_SPECIES_GRAINMAW, 10u));
	const u_int uBefore = xZero.m_uCurExp;
	Zenith_Vector<ZM_BattleEvent> xZeroEvents;
	ZENITH_ASSERT_EQ(ZM_ApplyExpGain(xZero, 0u, xZeroEvents, ZM_SIDE_PLAYER, 0u, 100u), 0u);
	ZENITH_ASSERT_EQ(xZero.m_uCurExp, uBefore);
	ZENITH_ASSERT_EQ(xZeroEvents.GetSize(), 0u);

	ZM_BattleMonster xHuge = ZM_BuildBattleMonster(MakeSpecReal(ZM_SPECIES_GRAINMAW, 99u));
	const u_int uRoom = 800000u - xHuge.m_uCurExp;
	Zenith_Vector<ZM_BattleEvent> xHugeEvents;
	ZENITH_ASSERT_EQ(ZM_ApplyExpGain(xHuge, ~0u, xHugeEvents, ZM_SIDE_PLAYER, 0u, ~0u), 1u);
	ZENITH_ASSERT_EQ(xHuge.m_uLevel, 100u);
	ZENITH_ASSERT_EQ(xHuge.m_uCurExp, 800000u);
	ZENITH_ASSERT_GT(xHugeEvents.GetSize(), 0u);
	if (xHugeEvents.GetSize() > 0u)
	{
		ZENITH_ASSERT_EQ(xHugeEvents.Get(0).m_iAmount, (int)uRoom);
	}

	ZM_BattleMonster xNatural = ZM_BuildBattleMonster(MakeSpecOv(ZM_SPECIES_GRAINMAW, 10u,
		100u, 100u, 100u, 100u, 100u, 100u, 0u, 0u,
		ZM_MOVE_CINDERSPIT, ZM_MOVE_FLARELASH, ZM_MOVE_PYREBURST, ZM_MOVE_MAGMAFALL));
	Zenith_Vector<ZM_BattleEvent> xNaturalEvents;
	ZENITH_ASSERT_EQ(ZM_ApplyExpGain(xNatural, 264u, xNaturalEvents,
		ZM_SIDE_PLAYER, 0u, 0u), 1u);
	ZENITH_ASSERT_EQ(xNatural.m_uLevel, 11u);
	ZENITH_ASSERT_EQ(xNatural.m_uCurExp, 1064u);
	Zenith_Vector<ZM_BattleEvent> xNaturalExpected;
	xNaturalExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_EXP_GAINED, ZM_SIDE_PLAYER,
		0u, ZM_MOVE_NONE, ZM_SPECIES_GRAINMAW, 264, 1064));
	xNaturalExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_LEVEL_UP, ZM_SIDE_PLAYER,
		0u, ZM_MOVE_NONE, ZM_SPECIES_GRAINMAW, 11, 43));
	AssertStreamEq(xNaturalExpected, xNaturalEvents, "zero-means-natural-cap");

	ZM_BattleMonster xNoDelevel = ZM_BuildBattleMonster(MakeSpecReal(ZM_SPECIES_GRAINMAW, 10u));
	const u_int uNoDelevelExp = xNoDelevel.m_uCurExp;
	Zenith_Vector<ZM_BattleEvent> xNoDelevelEvents;
	ZM_ApplyExpGain(xNoDelevel, 100u, xNoDelevelEvents, ZM_SIDE_PLAYER, 0u, 5u);
	ZENITH_ASSERT_EQ(xNoDelevel.m_uLevel, 10u);
	ZENITH_ASSERT_EQ(xNoDelevel.m_uCurExp, uNoDelevelExp);
	ZENITH_ASSERT_EQ(xNoDelevelEvents.GetSize(), 0u);
}

// A below-threshold award emits EXP_GAINED only (no LEVEL_UP), banking the exp.
ZENITH_TEST(ZM_Data, LevelUp_BelowThresholdExpOnly)
{
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(
		MakeSpecOv(ZM_SPECIES_GRAINMAW, 10u, 100u, 100u, 100u, 100u, 100u, 100u, 0u, 0u, ZM_MOVE_RAMBASH));

	Zenith_Vector<ZM_BattleEvent> xEvents;
	const u_int uGained = ZM_ApplyExpGain(xMon, 10u, xEvents, ZM_SIDE_PLAYER, 0u, 100u);   // 800->810 < 1064

	ZENITH_ASSERT_EQ(uGained, 0u);
	ZENITH_ASSERT_EQ(xMon.m_uLevel, 10u);
	ZENITH_ASSERT_EQ(xMon.m_uCurExp, 810u);
	ZENITH_ASSERT_EQ(xEvents.GetSize(), 1u);
	ZENITH_ASSERT_EQ((u_int)xEvents.Get(0).m_eKind, (u_int)ZM_BATTLE_EVENT_EXP_GAINED);
	ZENITH_ASSERT_FALSE(HasKind(xEvents, ZM_BATTLE_EVENT_LEVEL_UP));
}

// EVs set on the mon feed the EV/4 term at the next level-up recompute.
ZENITH_TEST(ZM_Data, LevelUp_EVTermEntersRecompute)
{
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(
		MakeSpecOv(ZM_SPECIES_GRAINMAW, 10u, 100u, 100u, 100u, 100u, 100u, 100u, 0u, 0u, ZM_MOVE_RAMBASH));

	xMon.m_auEV[ZM_STAT_ATTACK] = 252u;   // EVs applied later, mainline-faithful

	Zenith_Vector<ZM_BattleEvent> xEvents;
	const u_int uGain = ZM_ExpForLevel(ZM_GROWTH_FAST, 11u) - xMon.m_uCurExp;
	ZM_ApplyExpGain(xMon, uGain, xEvents, ZM_SIDE_PLAYER, 0u, 100u);

	const u_int uWithEV = ZM_CalcStat(ZM_STAT_ATTACK, 100u, 0u, 252u, 11u, ZM_NATURE_FERAL);
	const u_int uNoEV   = ZM_CalcStat(ZM_STAT_ATTACK, 100u, 0u,   0u, 11u, ZM_NATURE_FERAL);
	ZENITH_ASSERT_EQ(xMon.m_auMaxStat[ZM_STAT_ATTACK], uWithEV);
	ZENITH_ASSERT_GT(uWithEV, uNoEV, "EV/4 term should raise the recomputed stat");
}

// ############################################################################
// F. Move-learn on level-up (ZM_Data)
// ############################################################################

// A level-up whose learnset entry lands in an EMPTY slot learns the move (PP from
// the table) and emits MOVE_LEARNED with the destination slot index.
ZENITH_TEST(ZM_Data, MoveLearn_EmptySlotLearns)
{
	// Fulgurun: ELECTRIC, final-stage (no evolution). CINDERSPIT (FIRE) sits in
	// slot 0 -- guaranteed NOT in an ELECTRIC learnset, so no accidental dedupe.
	const ZM_SPECIES_ID eSp = ZM_SPECIES_FULGURUN;
	u_int uLearnLevel = 0u;
	ZM_MOVE_ID eLearnMove = ZM_MOVE_NONE;
	const bool bFound = FirstLevelUpEntry(eSp, uLearnLevel, eLearnMove);
	ZENITH_ASSERT_TRUE(bFound && uLearnLevel > 1u, "no safe level-up learnset entry");
	if (!bFound || uLearnLevel <= 1u) { return; }

	const ZM_GROWTH_RATE eRate = ZM_GetSpeciesGrowthRate(eSp);
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(
		MakeSpecReal(eSp, uLearnLevel - 1u, ZM_MOVE_CINDERSPIT));
	ZENITH_ASSERT_EQ((u_int)xMon.m_axMoves[1].m_eMove, (u_int)ZM_MOVE_NONE, "slot 1 should start empty");

	Zenith_Vector<ZM_BattleEvent> xEvents;
	const u_int uGain = ZM_ExpForLevel(eRate, uLearnLevel) - xMon.m_uCurExp;
	ZM_ApplyExpGain(xMon, uGain, xEvents, ZM_SIDE_PLAYER, 0u, 100u);

	ZENITH_ASSERT_EQ(xMon.m_uLevel, uLearnLevel);

	const ZM_BattleEvent* pxLearn = FindKind(xEvents, ZM_BATTLE_EVENT_MOVE_LEARNED);
	ZENITH_ASSERT_NOT_NULL(pxLearn, "a free-slot level-up learn should emit MOVE_LEARNED");
	if (pxLearn)
	{
		ZENITH_ASSERT_EQ(pxLearn->m_uSide, (u_int)ZM_SIDE_PLAYER);
		ZENITH_ASSERT_EQ(pxLearn->m_uMoveId, (u_int)eLearnMove);
		ZENITH_ASSERT_EQ(pxLearn->m_uSpeciesId, (u_int)eSp);
		ZENITH_ASSERT_EQ(pxLearn->m_iAmount, 1);   // first empty slot (slot 0 filled)
	}

	// The move landed in slot 1 with full PP from the table.
	const u_int uPP = ZM_GetMoveData(eLearnMove).m_uPP;
	ZENITH_ASSERT_EQ((u_int)xMon.m_axMoves[1].m_eMove, (u_int)eLearnMove);
	ZENITH_ASSERT_EQ(xMon.m_axMoves[1].m_uCurPP, uPP);
	ZENITH_ASSERT_EQ(xMon.m_axMoves[1].m_uMaxPP, uPP);
}

// RULING: a level-up move that would be a 5th move is SKIPPED (no auto-replace,
// no MOVE_LEARNED) when all four slots are full.
ZENITH_TEST(ZM_Data, MoveLearn_FourMoveOverflowSkips)
{
	const ZM_SPECIES_ID eSp = ZM_SPECIES_FULGURUN;
	u_int uLearnLevel = 0u;
	ZM_MOVE_ID eLearnMove = ZM_MOVE_NONE;
	const bool bFound = FirstLevelUpEntry(eSp, uLearnLevel, eLearnMove);
	ZENITH_ASSERT_TRUE(bFound && uLearnLevel > 1u, "no safe level-up learnset entry");
	if (!bFound || uLearnLevel <= 1u) { return; }

	const ZM_GROWTH_RATE eRate = ZM_GetSpeciesGrowthRate(eSp);
	// Four FIRE moves fill every slot; none is the (ELECTRIC/NORMAL) learn target.
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(
		MakeSpecReal(eSp, uLearnLevel - 1u, ZM_MOVE_CINDERSPIT, ZM_MOVE_FLARELASH, ZM_MOVE_PYREBURST, ZM_MOVE_MAGMAFALL));

	Zenith_Vector<ZM_BattleEvent> xEvents;
	const u_int uGain = ZM_ExpForLevel(eRate, uLearnLevel) - xMon.m_uCurExp;
	ZM_ApplyExpGain(xMon, uGain, xEvents, ZM_SIDE_PLAYER, 0u, 100u);

	ZENITH_ASSERT_EQ(xMon.m_uLevel, uLearnLevel);
	ZENITH_ASSERT_FALSE(HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_LEARNED), "overflow must NOT emit MOVE_LEARNED");

	// The four original moves are untouched (no auto-replace).
	ZENITH_ASSERT_EQ((u_int)xMon.m_axMoves[0].m_eMove, (u_int)ZM_MOVE_CINDERSPIT);
	ZENITH_ASSERT_EQ((u_int)xMon.m_axMoves[1].m_eMove, (u_int)ZM_MOVE_FLARELASH);
	ZENITH_ASSERT_EQ((u_int)xMon.m_axMoves[2].m_eMove, (u_int)ZM_MOVE_PYREBURST);
	ZENITH_ASSERT_EQ((u_int)xMon.m_axMoves[3].m_eMove, (u_int)ZM_MOVE_MAGMAFALL);
}

// A learnset move the mon already knows is not re-learned (dedupe, no event).
ZENITH_TEST(ZM_Data, MoveLearn_DedupeNoRelearn)
{
	const ZM_SPECIES_ID eSp = ZM_SPECIES_FULGURUN;
	u_int uLearnLevel = 0u;
	ZM_MOVE_ID eLearnMove = ZM_MOVE_NONE;
	const bool bFound = FirstLevelUpEntry(eSp, uLearnLevel, eLearnMove);
	ZENITH_ASSERT_TRUE(bFound && uLearnLevel > 1u, "no safe level-up learnset entry");
	if (!bFound || uLearnLevel <= 1u) { return; }

	const ZM_GROWTH_RATE eRate = ZM_GetSpeciesGrowthRate(eSp);
	// The mon already knows the learn target in slot 0; slots 1-3 empty.
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(MakeSpecReal(eSp, uLearnLevel - 1u, eLearnMove));

	Zenith_Vector<ZM_BattleEvent> xEvents;
	const u_int uGain = ZM_ExpForLevel(eRate, uLearnLevel) - xMon.m_uCurExp;
	ZM_ApplyExpGain(xMon, uGain, xEvents, ZM_SIDE_PLAYER, 0u, 100u);

	ZENITH_ASSERT_EQ(xMon.m_uLevel, uLearnLevel);
	ZENITH_ASSERT_FALSE(HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_LEARNED), "a known move must not re-learn");
	// The first empty slot is untouched (nothing was written).
	ZENITH_ASSERT_EQ((u_int)xMon.m_axMoves[1].m_eMove, (u_int)ZM_MOVE_NONE);
}

// ############################################################################
// G. Evolution (ZM_Data, real species -- no override) [LEVEL-trigger only]
// ############################################################################

// CheckEvolveEligible: below the evo level -> NONE; at/above -> the target species.
ZENITH_TEST(ZM_Data, Evolve_CheckEligibleStage1)
{
	ZM_BattleMonster xBelow = ZM_BuildBattleMonster(MakeSpecReal(ZM_SPECIES_NIBBIN, 15u));
	ZENITH_ASSERT_EQ((u_int)ZM_CheckEvolveEligible(xBelow), (u_int)ZM_SPECIES_NONE);

	ZM_BattleMonster xAt = ZM_BuildBattleMonster(MakeSpecReal(ZM_SPECIES_NIBBIN, 16u));
	ZENITH_ASSERT_EQ((u_int)ZM_CheckEvolveEligible(xAt), (u_int)ZM_SPECIES_HOARDEL);

	ZM_BattleMonster xAbove = ZM_BuildBattleMonster(MakeSpecReal(ZM_SPECIES_NIBBIN, 30u));
	ZENITH_ASSERT_EQ((u_int)ZM_CheckEvolveEligible(xAbove), (u_int)ZM_SPECIES_HOARDEL);
}

// Stage-2 evolves at 36.
ZENITH_TEST(ZM_Data, Evolve_CheckEligibleStage2)
{
	ZM_BattleMonster xBelow = ZM_BuildBattleMonster(MakeSpecReal(ZM_SPECIES_HOARDEL, 35u));
	ZENITH_ASSERT_EQ((u_int)ZM_CheckEvolveEligible(xBelow), (u_int)ZM_SPECIES_NONE);

	ZM_BattleMonster xAt = ZM_BuildBattleMonster(MakeSpecReal(ZM_SPECIES_HOARDEL, 36u));
	ZENITH_ASSERT_EQ((u_int)ZM_CheckEvolveEligible(xAt), (u_int)ZM_SPECIES_GRAINMAW);
}

// A final-stage / single-stage species is never eligible, even at L100.
ZENITH_TEST(ZM_Data, Evolve_CheckEligibleFinalNever)
{
	ZM_BattleMonster xFinal = ZM_BuildBattleMonster(MakeSpecReal(ZM_SPECIES_GRAINMAW, 100u));
	ZENITH_ASSERT_EQ((u_int)ZM_CheckEvolveEligible(xFinal), (u_int)ZM_SPECIES_NONE);

	ZM_BattleMonster xSingle = ZM_BuildBattleMonster(MakeSpecReal(ZM_SPECIES_AURICORN, 100u));
	ZENITH_ASSERT_EQ((u_int)ZM_CheckEvolveEligible(xSingle), (u_int)ZM_SPECIES_NONE);
}

// ZM_Evolve mutates the species, restats from the NEW species' table, carries the
// HP delta (full mon), and returns true.
ZENITH_TEST(ZM_Data, Evolve_MutatesAndRestats)
{
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(MakeSpecReal(ZM_SPECIES_NIBBIN, 16u));
	const u_int uOldHP = xMon.m_auMaxStat[ZM_STAT_HP];
	ZENITH_ASSERT_EQ(xMon.m_uCurHP, uOldHP);   // full at build

	const bool bEvolved = ZM_Evolve(xMon);
	ZENITH_ASSERT_TRUE(bEvolved, "ZM_Evolve should succeed at the evo level");
	ZENITH_ASSERT_EQ((u_int)xMon.m_eSpecies, (u_int)ZM_SPECIES_HOARDEL);

	// Stored base stats now come from Hoardel's table (an override would NOT carry).
	const ZM_BaseStats xHoardel = ZM_GetSpeciesBaseStats(ZM_SPECIES_HOARDEL);
	for (u_int i = 0; i < ZM_STAT_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ(xMon.m_xBaseStats.m_au[i], xHoardel.m_au[i], "base stat %u not refreshed", i);
		const u_int uWant = ZM_CalcStat((ZM_STAT)i, xHoardel.m_au[i], 31u, 0u, 16u, ZM_NATURE_FERAL);
		ZENITH_ASSERT_EQ(xMon.m_auMaxStat[i], uWant, "stat %u not restated post-evolve", i);
	}

	const u_int uNewHP = xMon.m_auMaxStat[ZM_STAT_HP];
	ZENITH_ASSERT_GE(uNewHP, uOldHP);
	ZENITH_ASSERT_EQ(xMon.m_uCurHP, uNewHP);   // full mon stays full
}

// ZM_Evolve carries the HP delta for a damaged mon (not reset to full).
ZENITH_TEST(ZM_Data, Evolve_HPDeltaCarryDamaged)
{
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(MakeSpecReal(ZM_SPECIES_NIBBIN, 16u));
	const u_int uOldHP = xMon.m_auMaxStat[ZM_STAT_HP];
	xMon.m_uCurHP = uOldHP - 3u;

	ZENITH_ASSERT_TRUE(ZM_Evolve(xMon));
	const u_int uNewHP = xMon.m_auMaxStat[ZM_STAT_HP];
	ZENITH_ASSERT_EQ(xMon.m_uCurHP, (uOldHP - 3u) + (uNewHP - uOldHP));
	ZENITH_ASSERT_EQ(xMon.m_uCurHP, uNewHP - 3u);
}

// Below the evo level, or at a final stage, ZM_Evolve is a no-op returning false.
ZENITH_TEST(ZM_Data, Evolve_BelowLevelAndFinalNoOp)
{
	// Below level.
	ZM_BattleMonster xLow = ZM_BuildBattleMonster(MakeSpecReal(ZM_SPECIES_NIBBIN, 15u));
	const ZM_BaseStats xNibbin = ZM_GetSpeciesBaseStats(ZM_SPECIES_NIBBIN);
	ZENITH_ASSERT_FALSE(ZM_Evolve(xLow), "should not evolve below the evo level");
	ZENITH_ASSERT_EQ((u_int)xLow.m_eSpecies, (u_int)ZM_SPECIES_NIBBIN);
	for (u_int i = 0; i < ZM_STAT_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ(xLow.m_xBaseStats.m_au[i], xNibbin.m_au[i], "base stat %u changed on a no-op", i);
	}

	// Final stage.
	ZM_BattleMonster xFinal = ZM_BuildBattleMonster(MakeSpecReal(ZM_SPECIES_GRAINMAW, 100u));
	ZENITH_ASSERT_FALSE(ZM_Evolve(xFinal), "a final-stage species never evolves");
	ZENITH_ASSERT_EQ((u_int)xFinal.m_eSpecies, (u_int)ZM_SPECIES_GRAINMAW);
}

// Applying EXP mid-battle marks that a level was gained but never queues or
// mutates evolution; terminal ZM_BattleEngine settlement owns the queue event.
ZENITH_TEST(ZM_Data, Evolve_ApplyExpGainDefersQueue)
{
	// Nibbin: COMMON -> FAST. No learnset entry lands at exactly L16, so the
	// L15->L16 step is a clean EXP_GAINED / LEVEL_UP pair.
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(MakeSpecReal(ZM_SPECIES_NIBBIN, 15u, ZM_MOVE_RAMBASH));

	const u_int uNewExp = ZM_ExpForLevel(ZM_GROWTH_FAST, 16u);   // 3276
	const u_int uGain   = uNewExp - xMon.m_uCurExp;              // 3276 - 2700
	const u_int uNewHP  = ZM_CalcStat(ZM_STAT_HP, ZM_GetSpeciesBaseStats(ZM_SPECIES_NIBBIN).m_au[ZM_STAT_HP],
		31u, 0u, 16u, ZM_NATURE_FERAL);

	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_ApplyExpGain(xMon, uGain, xEvents, ZM_SIDE_PLAYER, 0u, 100u);

	ZENITH_ASSERT_EQ(xMon.m_uLevel, 16u);
	ZENITH_ASSERT_TRUE(xMon.m_bLevelledThisBattle);
	ZENITH_ASSERT_FALSE(xMon.m_bEvolutionQueued);
	// The species must NOT mutate mid-battle (post-battle contract).
	ZENITH_ASSERT_EQ((u_int)xMon.m_eSpecies, (u_int)ZM_SPECIES_NIBBIN);

	Zenith_Vector<ZM_BattleEvent> xExpected;
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_EXP_GAINED, ZM_SIDE_PLAYER, 0u, ZM_MOVE_NONE,
		ZM_SPECIES_NIBBIN, (int)uGain, (int)uNewExp));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_LEVEL_UP, ZM_SIDE_PLAYER, 0u, ZM_MOVE_NONE,
		ZM_SPECIES_NIBBIN, 16, (int)uNewHP));
	AssertStreamEq(xExpected, xEvents, "evolution-deferred");
}

// A hostile old max-HP cannot underflow HP carry, and a single call takes exactly
// one immediate edge even when the resulting form is already eligible.
ZENITH_TEST(ZM_Data, Evolve_GuardedHPAndOneEdgePerCall)
{
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(MakeSpecReal(ZM_SPECIES_NIBBIN, 100u));
	xMon.m_auMaxStat[ZM_STAT_HP] = ~0u;
	xMon.m_uCurHP = 2u;
	xMon.m_bEvolutionQueued = true;
	xMon.m_bLevelledThisBattle = true;
	ZENITH_ASSERT_TRUE(ZM_Evolve(xMon));
	ZENITH_ASSERT_EQ((u_int)xMon.m_eSpecies, (u_int)ZM_SPECIES_HOARDEL);
	ZENITH_ASSERT_EQ(xMon.m_uCurHP, 1u);
	ZENITH_ASSERT_FALSE(xMon.m_bEvolutionQueued);
	ZENITH_ASSERT_FALSE(xMon.m_bLevelledThisBattle);

	ZENITH_ASSERT_TRUE(ZM_Evolve(xMon));
	ZENITH_ASSERT_EQ((u_int)xMon.m_eSpecies, (u_int)ZM_SPECIES_GRAINMAW);
	ZENITH_ASSERT_FALSE(ZM_Evolve(xMon), "final form must stop the chain");
}

// Eligibility inspection is pure: it must not consume a queue or mutate state.
ZENITH_TEST(ZM_Data, Evolve_CheckEligiblePure)
{
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(MakeSpecReal(ZM_SPECIES_NIBBIN, 20u));
	xMon.m_bEvolutionQueued = true;
	const u_int uHP = xMon.m_uCurHP;
	const ZM_SPECIES_ID eBefore = xMon.m_eSpecies;
	ZENITH_ASSERT_EQ((u_int)ZM_CheckEvolveEligible(xMon), (u_int)ZM_SPECIES_HOARDEL);
	ZENITH_ASSERT_EQ((u_int)xMon.m_eSpecies, (u_int)eBefore);
	ZENITH_ASSERT_EQ(xMon.m_uCurHP, uHP);
	ZENITH_ASSERT_TRUE(xMon.m_bEvolutionQueued);
}

// ############################################################################
// H. Engine integration -- config-gated exp emission (ZM_Battle)
// ############################################################################

// A faint with m_bAwardExp=true appends FAINT -> EXP_GAINED (exact ZM_CalcExpGain
// value) before TURN_END/BATTLE_END, and the victor's EVs reflect the yield.
ZENITH_TEST(ZM_Battle, Engine_FaintAwardsExpOrderingAndFields)
{
	// Player Kindlet OHKOs a weak L5 Fernfawn with a super-effective FLARELASH.
	ZM_BattleMonsterSpec axPlayer[1] = {
		MakeSpecOv(ZM_SPECIES_KINDLET, 30u, 250u, 10u, 250u, 250u, 250u, 250u, 31u, 0u, ZM_MOVE_FLARELASH) };
	ZM_BattleMonsterSpec axEnemy[1] = {
		MakeSpecOv(ZM_SPECIES_FERNFAWN, 5u, 15u, 10u, 10u, 10u, 10u, 10u, 31u, 0u, ZM_MOVE_RAMBASH) };

	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeExpConfig(true, false), axPlayer, 1u, axEnemy, 1u, 0x1234ull, 54ull);
	RunToCompletion(xEngine);
	ZENITH_ASSERT_TRUE(xEngine.IsOver());
	ZENITH_ASSERT_EQ((u_int)xEngine.GetWinnerSide(), (u_int)ZM_SIDE_PLAYER);

	const Zenith_Vector<ZM_BattleEvent>& xEvents = xEngine.GetEvents();
	const int iFaint = IndexOfKind(xEvents, ZM_BATTLE_EVENT_FAINT);
	const int iExp   = IndexOfKind(xEvents, ZM_BATTLE_EVENT_EXP_GAINED);
	const int iTurnEnd = IndexOfKind(xEvents, ZM_BATTLE_EVENT_TURN_END);
	ZENITH_ASSERT_GE(iFaint, 0, "expected a FAINT");
	ZENITH_ASSERT_GE(iExp, 0, "expected an EXP_GAINED");
	ZENITH_ASSERT_GE(iTurnEnd, 0, "expected a TURN_END");
	ZENITH_ASSERT_LT(iFaint, iExp, "EXP_GAINED must follow FAINT");
	ZENITH_ASSERT_LT(iExp, iTurnEnd, "EXP_GAINED must precede TURN_END");

	// Exact EXP_GAINED fields (victor side/slot/species; wild value; new total).
	const u_int uGain = 95u;
	const u_int uNewTotal = 21855u;   // MEDIUM_SLOW L30 floor 21760 + literal gross 95
	const ZM_BattleEvent xWant = ZM_MakeEvent(ZM_BATTLE_EVENT_EXP_GAINED, ZM_SIDE_PLAYER, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_KINDLET, (int)uGain, (int)uNewTotal);
	char acE[192];
	char acA[192];
	ExpEventToString(xWant, acE, sizeof(acE));
	ExpEventToString(xEvents.Get((u_int)iExp), acA, sizeof(acA));
	ZENITH_ASSERT_TRUE(xEvents.Get((u_int)iExp) == xWant, "EXP_GAINED exp=%s act=%s", acE, acA);

	// The victor's EVs reflect Fernfawn's yield accrued into a fresh (0) vector.
	u_int auWantEV[ZM_STAT_COUNT] = { 0u, 0u, 0u, 0u, 0u, 0u };
	ZM_AddEVYield(auWantEV, ZM_GetSpeciesEVYield(ZM_SPECIES_FERNFAWN));
	const ZM_BattleMonster& xVictor = xEngine.GetState().Side(ZM_SIDE_PLAYER).Active();
	for (u_int i = 0; i < ZM_STAT_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ(xVictor.m_auEV[i], auWantEV[i], "victor EV stat %u mismatch", i);
	}
}

// CRITICAL byte-identity guard: the SAME battle+seed with m_bAwardExp=false emits
// none of the four exp kinds and is otherwise IDENTICAL to the true-flag stream
// with those kinds removed (the exp path draws no RNG -> zero perturbation).
ZENITH_TEST(ZM_Battle, Engine_ZeroPerturbationByteIdentity)
{
	// One stochastic hit consumes the ordinary crit/roll draws, then KOs the enemy.
	ZM_BattleMonsterSpec axPlayer[1] = {
		MakeSpecOv(ZM_SPECIES_SYLVASTAG, 30u, 250u, 250u, 250u, 250u, 250u, 250u, 31u, 0u, ZM_MOVE_RAMBASH) };
	// Fernfawn yields ATTACK EV. Pin that stat at its per-stat cap so the enabled
	// award still executes/emits EXP while every EV remains directly comparable.
	axPlayer[0].m_auEV[ZM_STAT_ATTACK] = 252u;
	ZM_BattleMonsterSpec axEnemy[1] = {
		MakeSpecOv(ZM_SPECIES_FERNFAWN, 5u, 15u, 10u, 10u, 10u, 10u, 10u, 31u, 0u, ZM_MOVE_RAMBASH) };

	ZM_BattleEngine xTrue;
	xTrue.Begin(MakeExpConfig(true, false), axPlayer, 1u, axEnemy, 1u, 0x1234ull, 54ull);
	xTrue.GetStateMutable().Side(ZM_SIDE_ENEMY).Active().m_uCurHP = 1u;
	xTrue.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xTrue.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xTrue.ResolveTurn();

	ZM_BattleEngine xFalse;
	xFalse.Begin(MakeExpConfig(false, false), axPlayer, 1u, axEnemy, 1u, 0x1234ull, 54ull);
	xFalse.GetStateMutable().Side(ZM_SIDE_ENEMY).Active().m_uCurHP = 1u;
	xFalse.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xFalse.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xFalse.ResolveTurn();

	ZENITH_ASSERT_TRUE(xTrue.IsOver() && xFalse.IsOver());
	ZENITH_ASSERT_EQ((u_int)xTrue.GetWinnerSide(), (u_int)xFalse.GetWinnerSide());

	const Zenith_Vector<ZM_BattleEvent>& xT = xTrue.GetEvents();
	const Zenith_Vector<ZM_BattleEvent>& xF = xFalse.GetEvents();

	// The false stream contains NONE of the exp-box kinds.
	ZENITH_ASSERT_FALSE(HasKind(xF, ZM_BATTLE_EVENT_EXP_GAINED), "flag off must not emit EXP_GAINED");
	ZENITH_ASSERT_FALSE(HasKind(xF, ZM_BATTLE_EVENT_LEVEL_UP), "flag off must not emit LEVEL_UP");
	ZENITH_ASSERT_FALSE(HasKind(xF, ZM_BATTLE_EVENT_MOVE_LEARNED), "flag off must not emit MOVE_LEARNED");
	ZENITH_ASSERT_FALSE(HasKind(xF, ZM_BATTLE_EVENT_EVOLUTION_QUEUED), "flag off must not emit EVOLUTION_QUEUED");
	// And the true stream really did exercise the exp path.
	ZENITH_ASSERT_TRUE(HasKind(xT, ZM_BATTLE_EVENT_EXP_GAINED), "flag on should emit EXP_GAINED on a faint");

	// Strip the four exp kinds from the true stream -> must equal the false stream.
	Zenith_Vector<ZM_BattleEvent> xFiltered;
	for (u_int i = 0; i < xT.GetSize(); ++i)
	{
		const ZM_BATTLE_EVENT eKind = xT.Get(i).m_eKind;
		if (eKind == ZM_BATTLE_EVENT_EXP_GAINED || eKind == ZM_BATTLE_EVENT_LEVEL_UP
			|| eKind == ZM_BATTLE_EVENT_MOVE_LEARNED || eKind == ZM_BATTLE_EVENT_EVOLUTION_QUEUED)
		{
			continue;
		}
		xFiltered.PushBack(xT.Get(i));
	}
	AssertStreamEq(xFiltered, xF, "zero-perturbation");
	AssertNonProgressionStateEq(xTrue.GetState(), xFalse.GetState());
	ZM_BattleRNG xTrueRNG = xTrue.GetState().m_xRNG;
	ZM_BattleRNG xFalseRNG = xFalse.GetState().m_xRNG;
	for (u_int i = 0u; i < 4u; ++i)
	{
		ZENITH_ASSERT_EQ(xTrueRNG.Next(), xFalseRNG.Next(),
			"progression must not perturb subsequent RNG output %u", i);
	}
	const ZM_BattleMonster& xOffWinner = xFalse.GetState().Side(ZM_SIDE_PLAYER).Active();
	const ZM_BattleMonster& xOffLoser = xFalse.GetState().Side(ZM_SIDE_ENEMY).Active();
	const ZM_BattleMonster& xOnWinner = xTrue.GetState().Side(ZM_SIDE_PLAYER).Active();
	const ZM_BattleMonster& xOnLoser = xTrue.GetState().Side(ZM_SIDE_ENEMY).Active();
	ZENITH_ASSERT_EQ(xOffWinner.m_uCurExp, 21760u);
	ZENITH_ASSERT_EQ(xOffLoser.m_uCurExp, 135u);
	ZENITH_ASSERT_EQ(xOnWinner.m_uCurExp, 21855u);
	ZENITH_ASSERT_EQ(xOnLoser.m_uCurExp, 135u);
	for (u_int i = 0u; i < ZM_STAT_COUNT; ++i)
	{
		const u_int uWinnerEV = i == ZM_STAT_ATTACK ? 252u : 0u;
		ZENITH_ASSERT_EQ(xOffWinner.m_auEV[i], uWinnerEV);
		ZENITH_ASSERT_EQ(xOnWinner.m_auEV[i], uWinnerEV);
		ZENITH_ASSERT_EQ(xOffLoser.m_auEV[i], 0u);
		ZENITH_ASSERT_EQ(xOnLoser.m_auEV[i], 0u);
	}
	ZENITH_ASSERT_FALSE(xOffWinner.m_bDefeatCredited);
	ZENITH_ASSERT_FALSE(xOffLoser.m_bDefeatCredited);
	ZENITH_ASSERT_EQ(xOffWinner.m_uParticipantMask, 0u);
	ZENITH_ASSERT_EQ(xOffLoser.m_uParticipantMask, 0u);
	ZENITH_ASSERT_FALSE(xOffWinner.m_bLevelledThisBattle);
	ZENITH_ASSERT_FALSE(xOffWinner.m_bEvolutionQueued);
	ZENITH_ASSERT_FALSE(xOffLoser.m_bLevelledThisBattle);
	ZENITH_ASSERT_FALSE(xOffLoser.m_bEvolutionQueued);
	ZENITH_ASSERT_EQ(xOnWinner.m_uParticipantMask, 0u);
	ZENITH_ASSERT_EQ(xOnLoser.m_uParticipantMask, 1u);
	ZENITH_ASSERT_FALSE(xOnWinner.m_bDefeatCredited);
	ZENITH_ASSERT_TRUE(xOnLoser.m_bDefeatCredited);
	ZENITH_ASSERT_FALSE(xOnWinner.m_bLevelledThisBattle);
	ZENITH_ASSERT_FALSE(xOnWinner.m_bEvolutionQueued);
	ZENITH_ASSERT_FALSE(xOnLoser.m_bLevelledThisBattle);
	ZENITH_ASSERT_FALSE(xOnLoser.m_bEvolutionQueued);
}

// A trainer battle (m_bIsTrainerBattle=true) awards the x1.5 exp variant.
ZENITH_TEST(ZM_Battle, Engine_TrainerBattleExpX1p5)
{
	ZM_BattleMonsterSpec axPlayer[1] = {
		MakeSpecOv(ZM_SPECIES_KINDLET, 30u, 250u, 10u, 250u, 250u, 250u, 250u, 31u, 0u, ZM_MOVE_FLARELASH) };
	ZM_BattleMonsterSpec axEnemy[1] = {
		MakeSpecOv(ZM_SPECIES_FERNFAWN, 20u, 15u, 10u, 10u, 10u, 10u, 10u, 31u, 0u, ZM_MOVE_RAMBASH) };

	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeExpConfig(true, true), axPlayer, 1u, axEnemy, 1u, 0x1234ull, 54ull);
	RunToCompletion(xEngine);
	ZENITH_ASSERT_EQ((u_int)xEngine.GetWinnerSide(), (u_int)ZM_SIDE_PLAYER);

	const ZM_BattleEvent* pxExp = FindKind(xEngine.GetEvents(), ZM_BATTLE_EVENT_EXP_GAINED);
	ZENITH_ASSERT_NOT_NULL(pxExp);
	if (pxExp)
	{
		ZENITH_ASSERT_EQ(pxExp->m_iAmount, 570, "literal trainer gross (wild is 380)");
	}
}

// In a recoil double-KO, the fainted participant earns nothing but a living bench
// nonparticipant still receives its independent half award and full EV yield.
ZENITH_TEST(ZM_Battle, Engine_DoubleKOLivingBenchGetsHalf)
{
	ZM_BattleMonsterSpec axPlayer[2] = {
		MakeSpecOv(ZM_SPECIES_NIBBIN, 10u, 200u, 60u, 200u, 10u, 200u, 250u, 31u, 0u, ZM_MOVE_RECKLESSRUSH),
		MakeSpecOv(ZM_SPECIES_SYLVASTAG, 30u, 200u, 100u, 200u, 100u, 200u, 100u, 31u, 0u, ZM_MOVE_MINDSHEAR) };
	ZM_BattleMonsterSpec axEnemy[1] = {
		MakeSpecOv(ZM_SPECIES_FERNFAWN, 5u, 12u, 10u, 40u, 10u, 10u, 10u, 31u, 0u, ZM_MOVE_RAMBASH) };

	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeExpConfig(true, false), axPlayer, 2u, axEnemy, 1u, 0x1234ull, 54ull);
	ZM_BattleMonster& xActive = xEngine.GetStateMutable().Side(ZM_SIDE_PLAYER).Active();
	ZM_BattleMonster& xEnemy = xEngine.GetStateMutable().Side(ZM_SIDE_ENEMY).Active();
	xActive.m_auMaxStat[ZM_STAT_ATTACK] = 20u;
	xActive.m_auMaxStat[ZM_STAT_SPEED] = 100u;
	xActive.m_uCurHP = 1u;
	xEnemy.m_auMaxStat[ZM_STAT_DEFENSE] = 16u;
	xEnemy.m_auMaxStat[ZM_STAT_SPEED] = 10u;
	xEnemy.m_uCurHP = 1u;
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xEngine.ResolveTurn();

	Zenith_Vector<ZM_BattleEvent> xExpected;
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_BEGIN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_PLAYER, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NIBBIN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_ENEMY, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_FERNFAWN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_BEGIN, ZM_SIDE_COUNT, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER, 0u,
		ZM_MOVE_RECKLESSRUSH));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_ENEMY, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 25, 0));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, ZM_SIDE_ENEMY, 0u));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_RECOIL, ZM_SIDE_PLAYER, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 8, 0));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, ZM_SIDE_PLAYER, 0u));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_EXP_GAINED, ZM_SIDE_PLAYER, 1u,
		ZM_MOVE_NONE, ZM_SPECIES_SYLVASTAG, 47, 21807));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_END, ZM_SIDE_COUNT, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_END, ZM_SIDE_COUNT, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)ZM_SIDE_PLAYER));
	AssertStreamEq(xExpected, xEngine.GetEvents(), "recoil-double-ko");
	const ZM_BattleMonster& xBench = xEngine.GetState().Side(ZM_SIDE_PLAYER).m_xParty.Get(1u);
	ZENITH_ASSERT_EQ(xBench.m_auEV[ZM_STAT_ATTACK], 1u);
}

// A mid-battle KO that grants enough exp emits LEVEL_UP (after EXP_GAINED, before
// TURN_END) and the victor's stats rise.
ZENITH_TEST(ZM_Battle, Engine_MidBattleLevelUp)
{
	// Player L5 OHKOs an L50 enemy rigged to 1 HP -> a large exp award levels it up.
	ZM_BattleMonsterSpec axPlayer[1] = {
		MakeSpecOv(ZM_SPECIES_NIBBIN, 5u, 100u, 100u, 100u, 100u, 100u, 250u, 31u, 0u, ZM_MOVE_RAMBASH) };
	ZM_BattleMonsterSpec axEnemy[1] = {
		MakeSpecOv(ZM_SPECIES_SYLVASTAG, 50u, 100u, 100u, 100u, 100u, 100u, 1u, 31u, 0u, ZM_MOVE_RAMBASH) };

	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeExpConfig(true, true), axPlayer, 1u, axEnemy, 1u, 0x1234ull, 54ull);

	const u_int uOldSpeed = xEngine.GetState().Side(ZM_SIDE_PLAYER).Active().m_auMaxStat[ZM_STAT_SPEED];
	xEngine.GetStateMutable().Side(ZM_SIDE_ENEMY).Active().m_uCurHP = 1u;
	RunToCompletion(xEngine);
	ZENITH_ASSERT_EQ((u_int)xEngine.GetWinnerSide(), (u_int)ZM_SIDE_PLAYER);

	const Zenith_Vector<ZM_BattleEvent>& xEvents = xEngine.GetEvents();
	const int iExp = IndexOfKind(xEvents, ZM_BATTLE_EVENT_EXP_GAINED);
	const int iLevel = IndexOfKind(xEvents, ZM_BATTLE_EVENT_LEVEL_UP);
	const int iTurnEnd = IndexOfKind(xEvents, ZM_BATTLE_EVENT_TURN_END);
	ZENITH_ASSERT_GE(iExp, 0);
	ZENITH_ASSERT_GE(iLevel, 0, "a big exp award should level the victor up");
	ZENITH_ASSERT_LT(iExp, iLevel, "LEVEL_UP must follow EXP_GAINED");
	ZENITH_ASSERT_LT(iLevel, iTurnEnd, "LEVEL_UP must precede TURN_END");

	const ZM_BattleMonster& xVictor = xEngine.GetState().Side(ZM_SIDE_PLAYER).Active();
	ZENITH_ASSERT_GT(xVictor.m_uLevel, 5u, "victor should have leveled up");
	ZENITH_ASSERT_GT(xVictor.m_auMaxStat[ZM_STAT_SPEED], uOldSpeed, "leveling up should raise a stat");
}

// The config level cap bounds mid-battle level-ups.
ZENITH_TEST(ZM_Battle, Engine_LevelCapBoundsLevelUp)
{
	ZM_BattleMonsterSpec axPlayer[1] = {
		MakeSpecOv(ZM_SPECIES_NIBBIN, 5u, 100u, 100u, 100u, 100u, 100u, 250u, 31u, 0u, ZM_MOVE_RAMBASH) };
	ZM_BattleMonsterSpec axEnemy[1] = {
		MakeSpecOv(ZM_SPECIES_SYLVASTAG, 50u, 100u, 100u, 100u, 100u, 100u, 1u, 31u, 0u, ZM_MOVE_RAMBASH) };

	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeExpConfig(true, true, 6u), axPlayer, 1u, axEnemy, 1u, 0x1234ull, 54ull);   // cap L6
	xEngine.GetStateMutable().Side(ZM_SIDE_ENEMY).Active().m_uCurHP = 1u;
	RunToCompletion(xEngine);

	const ZM_BattleMonster& xVictor = xEngine.GetState().Side(ZM_SIDE_PLAYER).Active();
	ZENITH_ASSERT_EQ(xVictor.m_uLevel, 6u, "level cap should stop the victor at L6");

	// No LEVEL_UP event ever reports a level above the cap.
	const Zenith_Vector<ZM_BattleEvent>& xEvents = xEngine.GetEvents();
	for (u_int i = 0; i < xEvents.GetSize(); ++i)
	{
		if (xEvents.Get(i).m_eKind == ZM_BATTLE_EVENT_LEVEL_UP)
		{
			ZENITH_ASSERT_LE(xEvents.Get(i).m_iAmount, 6, "a LEVEL_UP exceeded the config cap");
		}
	}
}

// A turn with no faint (flag on) emits no exp events at all.
ZENITH_TEST(ZM_Battle, Engine_NoFaintNoExpEvents)
{
	// Bulky mirror match; a single turn never KOs.
	ZM_BattleMonsterSpec axPlayer[1] = {
		MakeSpecOv(ZM_SPECIES_NIBBIN, 30u, 250u, 20u, 250u, 20u, 250u, 100u, 31u, 0u, ZM_MOVE_RAMBASH) };
	ZM_BattleMonsterSpec axEnemy[1] = {
		MakeSpecOv(ZM_SPECIES_STRAYLING, 30u, 250u, 20u, 250u, 20u, 250u, 90u, 31u, 0u, ZM_MOVE_RAMBASH) };

	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeExpConfig(true, false), axPlayer, 1u, axEnemy, 1u, 0x1234ull, 54ull);
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
	xEngine.ResolveTurn();

	ZENITH_ASSERT_FALSE(xEngine.IsOver(), "neither mon should faint in one turn");
	ZENITH_ASSERT_FALSE(HasKind(xEngine.GetEvents(), ZM_BATTLE_EVENT_EXP_GAINED), "no faint -> no exp");
	ZENITH_ASSERT_FALSE(HasKind(xEngine.GetEvents(), ZM_BATTLE_EVENT_LEVEL_UP));
}

// Normal enabling is player-only: a player faint is defeat-credited, but the enemy
// gains no EXP/EV and the disabled enemy participation ledger remains untouched.
ZENITH_TEST(ZM_Battle, Engine_PlayerOnlyGateDoesNotProgressEnemy)
{
	// Enemy Kindlet OHKOs (and outspeeds) a weak L5 player Fernfawn.
	ZM_BattleMonsterSpec axPlayer[1] = {
		MakeSpecOv(ZM_SPECIES_FERNFAWN, 5u, 15u, 10u, 10u, 10u, 10u, 10u, 31u, 0u, ZM_MOVE_RAMBASH) };
	ZM_BattleMonsterSpec axEnemy[1] = {
		MakeSpecOv(ZM_SPECIES_KINDLET, 30u, 250u, 10u, 250u, 250u, 250u, 250u, 31u, 0u, ZM_MOVE_FLARELASH) };

	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeExpConfig(true, false), axPlayer, 1u, axEnemy, 1u, 0x1234ull, 54ull);
	RunToCompletion(xEngine);
	ZENITH_ASSERT_EQ((u_int)xEngine.GetWinnerSide(), (u_int)ZM_SIDE_ENEMY);

	ZENITH_ASSERT_FALSE(HasKind(xEngine.GetEvents(), ZM_BATTLE_EVENT_EXP_GAINED));
	const ZM_BattleMonster& xPlayer = xEngine.GetState().Side(ZM_SIDE_PLAYER).Active();
	const ZM_BattleMonster& xEnemy = xEngine.GetState().Side(ZM_SIDE_ENEMY).Active();
	ZENITH_ASSERT_TRUE(xPlayer.m_bDefeatCredited);
	ZENITH_ASSERT_EQ(xPlayer.m_uParticipantMask, 0u);
	ZENITH_ASSERT_EQ(xEnemy.m_uCurExp, 21760u);
	for (u_int i = 0u; i < ZM_STAT_COUNT; ++i) { ZENITH_ASSERT_EQ(xEnemy.m_auEV[i], 0u); }

	ZM_BattleEngine xEnemyConfigured;
	xEnemyConfigured.Begin(MakeExpConfig(true, false, 0u, 1u << (u_int)ZM_SIDE_ENEMY),
		axPlayer, 1u, axEnemy, 1u, 0x1234ull, 54ull);
	RunToCompletion(xEnemyConfigured);
	const ZM_BattleEvent* pxEnemyExp = FindKind(xEnemyConfigured.GetEvents(), ZM_BATTLE_EVENT_EXP_GAINED);
	ZENITH_ASSERT_NOT_NULL(pxEnemyExp);
	if (pxEnemyExp != nullptr)
	{
		ZENITH_ASSERT_EQ(pxEnemyExp->m_uSide, (u_int)ZM_SIDE_ENEMY);
		ZENITH_ASSERT_EQ(pxEnemyExp->m_iAmount, 95);
	}
}

// Representative direct fixed-damage KO: the complete stream is pinned, including
// the literal gross award and terminal placement.
ZENITH_TEST(ZM_Battle, Engine_DirectKOGoldenFullStream)
{
	ZM_BattleMonsterSpec axPlayer[1] = {
		MakeSpecOv(ZM_SPECIES_SYLVASTAG, 30u, 200u, 100u, 200u, 100u, 200u, 250u,
			31u, 0u, ZM_MOVE_MINDSHEAR) };
	ZM_BattleMonsterSpec axEnemy[1] = {
		MakeSpecOv(ZM_SPECIES_FERNFAWN, 5u, 20u, 10u, 10u, 10u, 10u, 10u,
			31u, 0u, ZM_MOVE_RAMBASH) };
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeExpConfig(true, false), axPlayer, 1u, axEnemy, 1u, 0x1234ull, 54ull);
	xEngine.GetStateMutable().Side(ZM_SIDE_ENEMY).Active().m_uCurHP = 1u;
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xEngine.ResolveTurn();

	Zenith_Vector<ZM_BattleEvent> xExpected;
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_BEGIN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_PLAYER, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_SYLVASTAG));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_ENEMY, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_FERNFAWN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_BEGIN, ZM_SIDE_COUNT, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER, 0u,
		ZM_MOVE_MINDSHEAR));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_ENEMY, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 30, 0));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, ZM_SIDE_ENEMY, 0u));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_EXP_GAINED, ZM_SIDE_PLAYER, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_SYLVASTAG, 95, 21855));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_END, ZM_SIDE_COUNT, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_END, ZM_SIDE_COUNT, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)ZM_SIDE_PLAYER));
	AssertStreamEq(xExpected, xEngine.GetEvents(), "direct-ko-progression");
	const ZM_BattleMonster& xDefeated = xEngine.GetState().Side(ZM_SIDE_ENEMY).Active();
	ZENITH_ASSERT_TRUE(xDefeated.m_bDefeatCredited);
	ZENITH_ASSERT_EQ(xDefeated.m_uParticipantMask, 1u);
}

// Award-off is the exact legacy fixed-damage stream: no progression event or
// transient ledger mutation appears anywhere.
ZENITH_TEST(ZM_Battle, Engine_AwardOffExactLegacyGolden)
{
	ZM_BattleMonsterSpec axPlayer[1] = {
		MakeSpecOv(ZM_SPECIES_SYLVASTAG, 30u, 200u, 100u, 200u, 100u, 200u, 250u,
			31u, 0u, ZM_MOVE_MINDSHEAR) };
	axPlayer[0].m_uCurExp = 21800u;
	ZM_BattleMonsterSpec axEnemy[1] = {
		MakeSpecOv(ZM_SPECIES_FERNFAWN, 5u, 20u, 10u, 10u, 10u, 10u, 10u,
			31u, 0u, ZM_MOVE_RAMBASH) };
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeExpConfig(false, false), axPlayer, 1u, axEnemy, 1u, 0x1234ull, 54ull);
	xEngine.GetStateMutable().Side(ZM_SIDE_ENEMY).Active().m_uCurHP = 1u;
	const ZM_BattleMonster xWinnerBefore = xEngine.GetState().Side(ZM_SIDE_PLAYER).Active();
	const ZM_BattleMonster xDefeatedBefore = xEngine.GetState().Side(ZM_SIDE_ENEMY).Active();
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xEngine.ResolveTurn();

	Zenith_Vector<ZM_BattleEvent> xExpected;
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_BEGIN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_PLAYER, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_SYLVASTAG));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_ENEMY, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_FERNFAWN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_BEGIN, ZM_SIDE_COUNT, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER, 0u,
		ZM_MOVE_MINDSHEAR));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_ENEMY, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 30, 0));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, ZM_SIDE_ENEMY, 0u));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_END, ZM_SIDE_COUNT, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_END, ZM_SIDE_COUNT, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)ZM_SIDE_PLAYER));
	AssertStreamEq(xExpected, xEngine.GetEvents(), "direct-ko-legacy");
	const ZM_BattleMonster& xWinner = xEngine.GetState().Side(ZM_SIDE_PLAYER).Active();
	const ZM_BattleMonster& xDefeated = xEngine.GetState().Side(ZM_SIDE_ENEMY).Active();
	ZENITH_ASSERT_EQ(xWinner.m_uCurExp, 21800u);
	ZENITH_ASSERT_EQ(xDefeated.m_uCurExp, 135u);
	ZENITH_ASSERT_EQ(xWinner.m_uLevel, xWinnerBefore.m_uLevel);
	ZENITH_ASSERT_EQ(xDefeated.m_uLevel, xDefeatedBefore.m_uLevel);
	ZENITH_ASSERT_EQ(xWinner.m_uCurHP, xWinnerBefore.m_uCurHP);
	ZENITH_ASSERT_EQ(xDefeated.m_uCurHP, 0u);
	for (u_int i = 0u; i < ZM_STAT_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ(xWinner.m_auEV[i], xWinnerBefore.m_auEV[i]);
		ZENITH_ASSERT_EQ(xDefeated.m_auEV[i], xDefeatedBefore.m_auEV[i]);
		ZENITH_ASSERT_EQ(xWinner.m_auMaxStat[i], xWinnerBefore.m_auMaxStat[i]);
		ZENITH_ASSERT_EQ(xDefeated.m_auMaxStat[i], xDefeatedBefore.m_auMaxStat[i]);
	}
	ZENITH_ASSERT_EQ(xWinnerBefore.m_axMoves[0].m_uCurPP, 15u);
	ZENITH_ASSERT_EQ(xWinner.m_axMoves[0].m_uCurPP, 14u);
	ZENITH_ASSERT_EQ(xDefeated.m_axMoves[0].m_uCurPP,
		xDefeatedBefore.m_axMoves[0].m_uCurPP);
	ZENITH_ASSERT_EQ(xWinner.m_uParticipantMask, 0u);
	ZENITH_ASSERT_EQ(xDefeated.m_uParticipantMask, 0u);
	ZENITH_ASSERT_FALSE(xWinner.m_bDefeatCredited);
	ZENITH_ASSERT_FALSE(xDefeated.m_bDefeatCredited);
	ZENITH_ASSERT_FALSE(xWinner.m_bLevelledThisBattle);
	ZENITH_ASSERT_FALSE(xWinner.m_bEvolutionQueued);
	ZENITH_ASSERT_FALSE(xDefeated.m_bLevelledThisBattle);
	ZENITH_ASSERT_FALSE(xDefeated.m_bEvolutionQueued);
}

// Second-sweep weather KO golden. The enemy switch makes the player participate
// against slot 1; SAND's FAINT is credited once after weather/status/abilities.
ZENITH_TEST(ZM_Battle, Engine_WeatherEOTKOGoldenAndNoDoubleCredit)
{
	ZM_BattleMonsterSpec axPlayer[1] = {
		MakeSpecOv(ZM_SPECIES_SYLVASTAG, 30u, 200u, 100u, 200u, 100u, 200u, 250u,
			31u, 0u, ZM_MOVE_MISTVEIL) };
	ZM_BattleMonsterSpec axEnemy[2] = {
		MakeSpecOv(ZM_SPECIES_NIBBIN, 5u, 100u, 10u, 100u, 10u, 100u, 10u,
			31u, 0u, ZM_MOVE_MISTVEIL),
		MakeSpecOv(ZM_SPECIES_FERNFAWN, 5u, 8u, 10u, 10u, 10u, 10u, 10u,
			31u, 0u, ZM_MOVE_MISTVEIL) };
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeExpConfig(true, false), axPlayer, 1u, axEnemy, 2u, 0x1234ull, 54ull);
	xEngine.GetStateMutable().m_xField.m_eWeather = ZM_WEATHER_SAND;
	xEngine.GetStateMutable().m_xField.m_uWeatherTurns = 2u;
	xEngine.GetStateMutable().Side(ZM_SIDE_ENEMY).m_xParty.Get(1u).m_auMaxStat[ZM_STAT_HP] = 8u;
	xEngine.GetStateMutable().Side(ZM_SIDE_ENEMY).m_xParty.Get(1u).m_uCurHP = 1u;
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY, SwitchAction(1u));
	xEngine.ResolveTurn();

	Zenith_Vector<ZM_BattleEvent> xExpected;
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_BEGIN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_PLAYER, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_SYLVASTAG));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_ENEMY, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NIBBIN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_BEGIN, ZM_SIDE_COUNT, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_ENEMY, 1u,
		ZM_MOVE_NONE, ZM_SPECIES_FERNFAWN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER, 0u,
		ZM_MOVE_MISTVEIL));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_STAT_STAGE_CHANGED, ZM_SIDE_PLAYER, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 2, (int)ZM_BATTLE_STAT_SPDEFENSE));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_WEATHER_DAMAGE, ZM_SIDE_ENEMY, 1u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 1, 0, (int)ZM_WEATHER_SAND));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, ZM_SIDE_ENEMY, 1u));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_EXP_GAINED, ZM_SIDE_PLAYER, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_SYLVASTAG, 95, 21855));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_END, ZM_SIDE_COUNT, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	AssertStreamEq(xExpected, xEngine.GetEvents(), "weather-eot-ko");

	ZENITH_ASSERT_TRUE(xEngine.DoSwitch(ZM_SIDE_ENEMY, 0u));
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xEngine.ResolveTurn();
	ZENITH_ASSERT_EQ(CountKind(xEngine.GetEvents(), ZM_BATTLE_EVENT_EXP_GAINED), 1u,
		"both award sweeps must keep the old faint exactly-once");
}

ZENITH_TEST(ZM_Battle, Engine_BurnEOTKOGoldenFullStream)
{
	ZM_BattleMonsterSpec xPlayer = MakeSpecOv(ZM_SPECIES_SYLVASTAG, 30u,
		200u, 100u, 200u, 100u, 200u, 250u, 31u, 0u, ZM_MOVE_MISTVEIL);
	ZM_BattleMonsterSpec xEnemy = MakeSpecOv(ZM_SPECIES_FERNFAWN, 5u,
		8u, 10u, 10u, 10u, 10u, 10u, 31u, 0u, ZM_MOVE_MISTVEIL);
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeExpConfig(true, false), &xPlayer, 1u, &xEnemy, 1u, 0xB401ull, 54ull);
	ZM_BattleMonster& xVictim = xEngine.GetStateMutable().Side(ZM_SIDE_ENEMY).Active();
	xVictim.m_auMaxStat[ZM_STAT_HP] = 8u;
	xVictim.m_uCurHP = 1u;
	xVictim.m_eStatus = ZM_MAJOR_STATUS_BURN;
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xEngine.ResolveTurn();

	Zenith_Vector<ZM_BattleEvent> xExpected;
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_BEGIN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_PLAYER, 0u, ZM_MOVE_NONE, ZM_SPECIES_SYLVASTAG));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_ENEMY, 0u, ZM_MOVE_NONE, ZM_SPECIES_FERNFAWN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_BEGIN, ZM_SIDE_COUNT, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER, 0u, ZM_MOVE_MISTVEIL));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_STAT_STAGE_CHANGED, ZM_SIDE_PLAYER, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 2, (int)ZM_BATTLE_STAT_SPDEFENSE));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_ENEMY, 0u, ZM_MOVE_MISTVEIL));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_STAT_STAGE_CHANGED, ZM_SIDE_ENEMY, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 2, (int)ZM_BATTLE_STAT_SPDEFENSE));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_STATUS_DAMAGE, ZM_SIDE_ENEMY, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 1, 0, (int)ZM_MAJOR_STATUS_BURN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, ZM_SIDE_ENEMY, 0u));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_EXP_GAINED, ZM_SIDE_PLAYER, 0u, ZM_MOVE_NONE, ZM_SPECIES_SYLVASTAG, 95, 21855));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_END, ZM_SIDE_COUNT, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_END, ZM_SIDE_COUNT, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)ZM_SIDE_PLAYER));
	AssertStreamEq(xExpected, xEngine.GetEvents(), "burn-eot-ko");
}

ZENITH_TEST(ZM_Battle, Engine_TrapEOTKOGoldenFullStream)
{
	ZM_BattleMonsterSpec xPlayer = MakeSpecOv(ZM_SPECIES_SYLVASTAG, 30u,
		200u, 100u, 200u, 100u, 200u, 250u, 31u, 0u, ZM_MOVE_MISTVEIL);
	ZM_BattleMonsterSpec xEnemy = MakeSpecOv(ZM_SPECIES_FERNFAWN, 5u,
		8u, 10u, 10u, 10u, 10u, 10u, 31u, 0u, ZM_MOVE_MISTVEIL);
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeExpConfig(true, false), &xPlayer, 1u, &xEnemy, 1u, 0xB402ull, 54ull);
	ZM_BattleMonster& xVictim = xEngine.GetStateMutable().Side(ZM_SIDE_ENEMY).Active();
	xVictim.m_auMaxStat[ZM_STAT_HP] = 8u;
	xVictim.m_uCurHP = 1u;
	xVictim.m_uVolatileMask |= (u_int)ZM_VOLATILE_TRAP;
	xVictim.m_uTrapTurns = 2u;
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xEngine.ResolveTurn();

	Zenith_Vector<ZM_BattleEvent> xExpected;
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_BEGIN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_PLAYER, 0u, ZM_MOVE_NONE, ZM_SPECIES_SYLVASTAG));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_ENEMY, 0u, ZM_MOVE_NONE, ZM_SPECIES_FERNFAWN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_BEGIN, ZM_SIDE_COUNT, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER, 0u, ZM_MOVE_MISTVEIL));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_STAT_STAGE_CHANGED, ZM_SIDE_PLAYER, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 2, (int)ZM_BATTLE_STAT_SPDEFENSE));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_ENEMY, 0u, ZM_MOVE_MISTVEIL));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_STAT_STAGE_CHANGED, ZM_SIDE_ENEMY, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 2, (int)ZM_BATTLE_STAT_SPDEFENSE));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_STATUS_DAMAGE, ZM_SIDE_ENEMY, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 1, 0, ZM_VolatileDamageTag(ZM_VOLATILE_TRAP)));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, ZM_SIDE_ENEMY, 0u));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_EXP_GAINED, ZM_SIDE_PLAYER, 0u, ZM_MOVE_NONE, ZM_SPECIES_SYLVASTAG, 95, 21855));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_END, ZM_SIDE_COUNT, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_END, ZM_SIDE_COUNT, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)ZM_SIDE_PLAYER));
	AssertStreamEq(xExpected, xEngine.GetEvents(), "trap-eot-ko");
}

ZENITH_TEST(ZM_Battle, Engine_ThornmailKOGoldenFullStream)
{
	ZM_BattleMonsterSpec xPlayer = MakeSpecOv(ZM_SPECIES_SYLVASTAG, 30u,
		200u, 100u, 200u, 100u, 200u, 10u, 31u, 0u, ZM_MOVE_MISTVEIL);
	xPlayer.m_eAbility = ZM_ABILITY_THORNMAIL;
	ZM_BattleMonsterSpec xEnemy = MakeSpecOv(ZM_SPECIES_FERNFAWN, 5u,
		8u, 10u, 10u, 10u, 10u, 250u, 31u, 0u, ZM_MOVE_GNASHDOWN);
	xEnemy.m_eAbility = ZM_ABILITY_DEADAIM;
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeExpConfig(true, false), &xPlayer, 1u, &xEnemy, 1u, 0xB403ull, 54ull);
	ZM_BattleMonster& xDefender = xEngine.GetStateMutable().Side(ZM_SIDE_PLAYER).Active();
	ZM_BattleMonster& xAttacker = xEngine.GetStateMutable().Side(ZM_SIDE_ENEMY).Active();
	xDefender.m_auMaxStat[ZM_STAT_HP] = 8u;
	xDefender.m_uCurHP = 8u;
	xAttacker.m_auMaxStat[ZM_STAT_HP] = 8u;
	xAttacker.m_uCurHP = 1u;
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xEngine.ResolveTurn();

	Zenith_Vector<ZM_BattleEvent> xExpected;
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_BEGIN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_PLAYER, 0u, ZM_MOVE_NONE, ZM_SPECIES_SYLVASTAG));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_ENEMY, 0u, ZM_MOVE_NONE, ZM_SPECIES_FERNFAWN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_BEGIN, ZM_SIDE_COUNT, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_ENEMY, 0u, ZM_MOVE_GNASHDOWN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_PLAYER, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 4, 4));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_ABILITY_TRIGGER, ZM_SIDE_PLAYER, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 1, 0, (int)ZM_ABILITY_THORNMAIL));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, ZM_SIDE_ENEMY, 0u));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER, 0u, ZM_MOVE_MISTVEIL));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_STAT_STAGE_CHANGED, ZM_SIDE_PLAYER, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 2, (int)ZM_BATTLE_STAT_SPDEFENSE));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_EXP_GAINED, ZM_SIDE_PLAYER, 0u, ZM_MOVE_NONE, ZM_SPECIES_SYLVASTAG, 95, 21855));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_END, ZM_SIDE_COUNT, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_END, ZM_SIDE_COUNT, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)ZM_SIDE_PLAYER));
	AssertStreamEq(xExpected, xEngine.GetEvents(), "thornmail-ko");
}

ZENITH_TEST(ZM_Battle, Engine_EOTDoubleKOGoldenFullStream)
{
	ZM_BattleMonsterSpec axPlayer[2] = {
		MakeSpecOv(ZM_SPECIES_NIBBIN, 30u, 8u, 100u, 100u, 100u, 100u, 250u, 31u, 0u, ZM_MOVE_MISTVEIL),
		MakeSpecReal(ZM_SPECIES_SYLVASTAG, 30u, ZM_MOVE_MINDSHEAR) };
	ZM_BattleMonsterSpec xEnemy = MakeSpecOv(ZM_SPECIES_FERNFAWN, 5u,
		8u, 10u, 10u, 10u, 10u, 10u, 31u, 0u, ZM_MOVE_MISTVEIL);
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeExpConfig(true, false), axPlayer, 2u, &xEnemy, 1u, 0xB404ull, 54ull);
	ZM_BattleMonster& xPlayerActive = xEngine.GetStateMutable().Side(ZM_SIDE_PLAYER).Active();
	ZM_BattleMonster& xEnemyActive = xEngine.GetStateMutable().Side(ZM_SIDE_ENEMY).Active();
	xPlayerActive.m_auMaxStat[ZM_STAT_HP] = 8u;
	xEnemyActive.m_auMaxStat[ZM_STAT_HP] = 8u;
	xPlayerActive.m_uCurHP = 1u;
	xEnemyActive.m_uCurHP = 1u;
	xPlayerActive.m_eStatus = ZM_MAJOR_STATUS_BURN;
	xEnemyActive.m_eStatus = ZM_MAJOR_STATUS_BURN;
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xEngine.ResolveTurn();

	Zenith_Vector<ZM_BattleEvent> xExpected;
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_BEGIN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_PLAYER, 0u, ZM_MOVE_NONE, ZM_SPECIES_NIBBIN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_ENEMY, 0u, ZM_MOVE_NONE, ZM_SPECIES_FERNFAWN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_BEGIN, ZM_SIDE_COUNT, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER, 0u, ZM_MOVE_MISTVEIL));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_STAT_STAGE_CHANGED, ZM_SIDE_PLAYER, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 2, (int)ZM_BATTLE_STAT_SPDEFENSE));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_ENEMY, 0u, ZM_MOVE_MISTVEIL));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_STAT_STAGE_CHANGED, ZM_SIDE_ENEMY, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 2, (int)ZM_BATTLE_STAT_SPDEFENSE));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_STATUS_DAMAGE, ZM_SIDE_PLAYER, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 1, 0, (int)ZM_MAJOR_STATUS_BURN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, ZM_SIDE_PLAYER, 0u));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_STATUS_DAMAGE, ZM_SIDE_ENEMY, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 1, 0, (int)ZM_MAJOR_STATUS_BURN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, ZM_SIDE_ENEMY, 0u));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_EXP_GAINED, ZM_SIDE_PLAYER, 1u, ZM_MOVE_NONE, ZM_SPECIES_SYLVASTAG, 47, 21807));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_END, ZM_SIDE_COUNT, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_END, ZM_SIDE_COUNT, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)ZM_SIDE_PLAYER));
	AssertStreamEq(xExpected, xEngine.GetEvents(), "eot-double-ko");
}

// Two opponents keep independent ledgers. Recipient order is participants by
// ascending slot, then nonparticipants by ascending slot; every living recipient
// receives the full EV yield even when its EXP share is half.
ZENITH_TEST(ZM_Battle, Engine_ModernPartySharePerOpponentLedgerAndOrder)
{
	ZM_BattleMonsterSpec axPlayer[3] = {
		MakeSpecOv(ZM_SPECIES_SYLVASTAG, 30u, 200u, 100u, 200u, 100u, 200u, 250u, 31u, 0u, ZM_MOVE_MINDSHEAR),
		MakeSpecOv(ZM_SPECIES_SYLVASTAG, 30u, 200u, 100u, 200u, 100u, 200u, 240u, 31u, 0u, ZM_MOVE_MINDSHEAR),
		MakeSpecOv(ZM_SPECIES_SYLVASTAG, 30u, 200u, 100u, 200u, 100u, 200u, 230u, 31u, 0u, ZM_MOVE_MINDSHEAR) };
	ZM_BattleMonsterSpec axEnemy[2] = {
		MakeSpecOv(ZM_SPECIES_FERNFAWN, 5u, 20u, 10u, 10u, 10u, 10u, 10u, 31u, 0u, ZM_MOVE_MISTVEIL),
		MakeSpecOv(ZM_SPECIES_FERNFAWN, 5u, 20u, 10u, 10u, 10u, 10u, 10u, 31u, 0u, ZM_MOVE_MISTVEIL) };
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeExpConfig(true, false), axPlayer, 3u, axEnemy, 2u, 0x1234ull, 54ull);
	ZENITH_ASSERT_TRUE(xEngine.DoSwitch(ZM_SIDE_PLAYER, 1u));
	ZENITH_ASSERT_TRUE(xEngine.DoSwitch(ZM_SIDE_PLAYER, 0u));
	ZENITH_ASSERT_TRUE(xEngine.DoSwitch(ZM_SIDE_PLAYER, 1u));   // repeat: no duplicate bit
	xEngine.GetStateMutable().Side(ZM_SIDE_ENEMY).Active().m_uCurHP = 1u;
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xEngine.ResolveTurn();

	ZENITH_ASSERT_TRUE(xEngine.DoSwitch(ZM_SIDE_ENEMY, 1u));
	ZENITH_ASSERT_TRUE(xEngine.DoSwitch(ZM_SIDE_PLAYER, 2u));
	xEngine.GetStateMutable().Side(ZM_SIDE_ENEMY).Active().m_uCurHP = 1u;
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xEngine.ResolveTurn();

	const ZM_BattleSide& xEnemies = xEngine.GetState().Side(ZM_SIDE_ENEMY);
	ZENITH_ASSERT_EQ(xEnemies.m_xParty.Get(0u).m_uParticipantMask, 3u); // slots 0,1
	ZENITH_ASSERT_EQ(xEnemies.m_xParty.Get(1u).m_uParticipantMask, 6u); // slots 1,2
	ZM_BattleEvent axExpEvents[6] = {};
	u_int uExpCount = 0u;
	for (u_int i = 0u; i < xEngine.GetEvents().GetSize(); ++i)
	{
		const ZM_BattleEvent& xEvent = xEngine.GetEvents().Get(i);
		if (xEvent.m_eKind == ZM_BATTLE_EVENT_EXP_GAINED)
		{
			if (uExpCount < 6u) { axExpEvents[uExpCount] = xEvent; }
			++uExpCount;
		}
	}
	ZENITH_ASSERT_EQ(uExpCount, 6u);
	if (uExpCount == 6u)
	{
		const ZM_BattleEvent axExpected[6] =
		{
			ZM_MakeEvent(ZM_BATTLE_EVENT_EXP_GAINED, ZM_SIDE_PLAYER, 0u,
				ZM_MOVE_NONE, ZM_SPECIES_SYLVASTAG, 95, 21855),
			ZM_MakeEvent(ZM_BATTLE_EVENT_EXP_GAINED, ZM_SIDE_PLAYER, 1u,
				ZM_MOVE_NONE, ZM_SPECIES_SYLVASTAG, 95, 21855),
			ZM_MakeEvent(ZM_BATTLE_EVENT_EXP_GAINED, ZM_SIDE_PLAYER, 2u,
				ZM_MOVE_NONE, ZM_SPECIES_SYLVASTAG, 47, 21807),
			ZM_MakeEvent(ZM_BATTLE_EVENT_EXP_GAINED, ZM_SIDE_PLAYER, 1u,
				ZM_MOVE_NONE, ZM_SPECIES_SYLVASTAG, 95, 21950),
			ZM_MakeEvent(ZM_BATTLE_EVENT_EXP_GAINED, ZM_SIDE_PLAYER, 2u,
				ZM_MOVE_NONE, ZM_SPECIES_SYLVASTAG, 95, 21902),
			ZM_MakeEvent(ZM_BATTLE_EVENT_EXP_GAINED, ZM_SIDE_PLAYER, 0u,
				ZM_MOVE_NONE, ZM_SPECIES_SYLVASTAG, 47, 21902),
		};
		for (u_int i = 0u; i < 6u; ++i)
		{
			ZENITH_ASSERT_TRUE(axExpEvents[i] == axExpected[i],
				"complete EXP event/order mismatch %u", i);
		}
	}
	const ZM_BattleSide& xPlayers = xEngine.GetState().Side(ZM_SIDE_PLAYER);
	const u_int auExpectedExp[3] = { 21902u, 21950u, 21902u };
	for (u_int i = 0u; i < 3u; ++i)
	{
		ZENITH_ASSERT_EQ(xPlayers.m_xParty.Get(i).m_uCurExp, auExpectedExp[i]);
		for (u_int uStat = 0u; uStat < ZM_STAT_COUNT; ++uStat)
		{
			const u_int uExpectedEV = uStat == ZM_STAT_ATTACK ? 2u : 0u;
			ZENITH_ASSERT_EQ(xPlayers.m_xParty.Get(i).m_auEV[uStat], uExpectedEV,
				"full EV yield slot %u stat %u", i, uStat);
		}
	}
}

ZENITH_TEST(ZM_Battle, Engine_FaintedRecipientGetsNeither)
{
	ZM_BattleMonsterSpec axPlayer[3] = {
		MakeSpecReal(ZM_SPECIES_SYLVASTAG, 30u, ZM_MOVE_MINDSHEAR),
		MakeSpecReal(ZM_SPECIES_SYLVASTAG, 30u, ZM_MOVE_MINDSHEAR),
		MakeSpecReal(ZM_SPECIES_SYLVASTAG, 30u, ZM_MOVE_MINDSHEAR) };
	ZM_BattleMonsterSpec axEnemy[1] = { MakeSpecReal(ZM_SPECIES_FERNFAWN, 5u, ZM_MOVE_MISTVEIL) };
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeExpConfig(true, false), axPlayer, 3u, axEnemy, 1u, 2ull, 54ull);
	ZENITH_ASSERT_TRUE(xEngine.DoSwitch(ZM_SIDE_PLAYER, 1u));
	xEngine.GetStateMutable().Side(ZM_SIDE_PLAYER).m_xParty.Get(0u).m_uCurHP = 0u;
	xEngine.GetStateMutable().Side(ZM_SIDE_ENEMY).Active().m_uCurHP = 1u;
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xEngine.ResolveTurn();
	const ZM_BattleSide& xPlayers = xEngine.GetState().Side(ZM_SIDE_PLAYER);
	ZENITH_ASSERT_EQ(xPlayers.m_xParty.Get(0u).m_uCurExp, 21760u);
	for (u_int uSlot = 0u; uSlot < 3u; ++uSlot)
	{
		for (u_int uStat = 0u; uStat < ZM_STAT_COUNT; ++uStat)
		{
			const u_int uExpected = (uSlot > 0u && uStat == ZM_STAT_ATTACK) ? 1u : 0u;
			ZENITH_ASSERT_EQ(xPlayers.m_xParty.Get(uSlot).m_auEV[uStat], uExpected,
				"fainted-recipient EV slot %u stat %u", uSlot, uStat);
		}
	}
	ZENITH_ASSERT_EQ(xPlayers.m_xParty.Get(1u).m_uCurExp, 21855u);
	ZENITH_ASSERT_EQ(xPlayers.m_xParty.Get(2u).m_uCurExp, 21807u);
}

// The same award's EV mutation occurs before a level-up restat; level-capped
// recipients still accrue EVs but emit no EXP event when there is no room.
ZENITH_TEST(ZM_Battle, Engine_EVBeforeLevelRestatAndEVAtCap)
{
	ZM_BattleMonsterSpec xPlayer = MakeSpecOv(ZM_SPECIES_SYLVASTAG, 99u,
		100u, 100u, 100u, 100u, 100u, 250u, 31u, 0u, ZM_MOVE_MINDSHEAR);
	xPlayer.m_auEV[ZM_STAT_ATTACK] = 251u;
	xPlayer.m_uCurExp = 1059765u; // MEDIUM_SLOW L100 threshold minus literal gross 95
	ZM_BattleMonsterSpec xEnemy = MakeSpecReal(ZM_SPECIES_FERNFAWN, 5u, ZM_MOVE_MISTVEIL);
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeExpConfig(true, false), &xPlayer, 1u, &xEnemy, 1u, 3ull, 54ull);
	xEngine.GetStateMutable().Side(ZM_SIDE_ENEMY).Active().m_uCurHP = 1u;
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xEngine.ResolveTurn();
	const ZM_BattleMonster& xWinner = xEngine.GetState().Side(ZM_SIDE_PLAYER).Active();
	ZENITH_ASSERT_EQ(xWinner.m_uLevel, 100u);
	ZENITH_ASSERT_EQ(xWinner.m_auEV[ZM_STAT_ATTACK], 252u);
	ZENITH_ASSERT_EQ(xWinner.m_auMaxStat[ZM_STAT_ATTACK], 299u);
	ZENITH_ASSERT_GT(xWinner.m_auMaxStat[ZM_STAT_ATTACK], 298u);

	ZM_BattleMonsterSpec xCapPlayer = MakeSpecReal(ZM_SPECIES_SYLVASTAG, 100u, ZM_MOVE_MINDSHEAR);
	ZM_BattleEngine xCap;
	xCap.Begin(MakeExpConfig(true, false), &xCapPlayer, 1u, &xEnemy, 1u, 4ull, 54ull);
	xCap.GetStateMutable().Side(ZM_SIDE_ENEMY).Active().m_uCurHP = 1u;
	xCap.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xCap.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xCap.ResolveTurn();
	ZENITH_ASSERT_EQ(xCap.GetState().Side(ZM_SIDE_PLAYER).Active().m_auEV[ZM_STAT_ATTACK], 1u);
	ZENITH_ASSERT_FALSE(HasKind(xCap.GetEvents(), ZM_BATTLE_EVENT_EXP_GAINED));
}

// A reserve can level and learn from its half share, then switch in next turn and
// select/use the newly populated move slot.
ZENITH_TEST(ZM_Battle, Engine_ReserveLearnsThenUsesMoveMidBattle)
{
	ZM_BattleMonsterSpec axPlayer[2] = {
		MakeSpecOv(ZM_SPECIES_SYLVASTAG, 30u, 200u, 100u, 200u, 100u, 200u, 250u, 31u, 0u, ZM_MOVE_MINDSHEAR),
		MakeSpecOv(ZM_SPECIES_NIBBIN, 4u, 200u, 100u, 200u, 100u, 200u, 240u,
			31u, 0u, ZM_MOVE_MISTVEIL) };
	axPlayer[1].m_uCurExp = 53u; // FAST L5 threshold 100 minus literal half share 47
	ZM_BattleMonsterSpec axEnemy[2] = {
		MakeSpecReal(ZM_SPECIES_FERNFAWN, 5u, ZM_MOVE_MISTVEIL),
		MakeSpecOv(ZM_SPECIES_FERNFAWN, 5u, 250u, 10u, 250u, 10u, 250u, 10u,
			31u, 0u, ZM_MOVE_MISTVEIL) };
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeExpConfig(true, false), axPlayer, 2u, axEnemy, 2u, 0x1234ull, 54ull);
	xEngine.GetStateMutable().Side(ZM_SIDE_ENEMY).Active().m_uCurHP = 1u;
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xEngine.ResolveTurn();

	ZENITH_ASSERT_TRUE(xEngine.DoSwitch(ZM_SIDE_ENEMY, 1u));
	ZENITH_ASSERT_TRUE(xEngine.DoSwitch(ZM_SIDE_PLAYER, 1u));
	ZM_BattleMonster& xLearned = xEngine.GetStateMutable().Side(ZM_SIDE_PLAYER).Active();
	ZM_BattleMonster& xSecondEnemy = xEngine.GetStateMutable().Side(ZM_SIDE_ENEMY).Active();
	xLearned.m_auMaxStat[ZM_STAT_ATTACK] = 16u;
	xSecondEnemy.m_auMaxStat[ZM_STAT_DEFENSE] = 16u;
	xSecondEnemy.m_auMaxStat[ZM_STAT_SPEED] = 10u;
	xSecondEnemy.m_uCurHP = 6u;
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(1u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xEngine.ResolveTurn();

	Zenith_Vector<ZM_BattleEvent> xExpected;
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_BEGIN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_PLAYER, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_SYLVASTAG));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_ENEMY, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_FERNFAWN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_BEGIN, ZM_SIDE_COUNT, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER, 0u,
		ZM_MOVE_MINDSHEAR));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_ENEMY, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 30, 0));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, ZM_SIDE_ENEMY, 0u));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_EXP_GAINED, ZM_SIDE_PLAYER, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_SYLVASTAG, 95, 21855));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_EXP_GAINED, ZM_SIDE_PLAYER, 1u,
		ZM_MOVE_NONE, ZM_SPECIES_NIBBIN, 47, 100));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_LEVEL_UP, ZM_SIDE_PLAYER, 1u,
		ZM_MOVE_NONE, ZM_SPECIES_NIBBIN, 5, 36));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_LEARNED, ZM_SIDE_PLAYER, 1u,
		ZM_MOVE_QUICKJAB, ZM_SPECIES_NIBBIN, 1, 0));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_END, ZM_SIDE_COUNT, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_ENEMY, 1u,
		ZM_MOVE_NONE, ZM_SPECIES_FERNFAWN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_PLAYER, 1u,
		ZM_MOVE_NONE, ZM_SPECIES_NIBBIN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_BEGIN, ZM_SIDE_COUNT, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 2));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER, 1u,
		ZM_MOVE_QUICKJAB));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_ENEMY, 1u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 6, 0));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, ZM_SIDE_ENEMY, 1u));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_EXP_GAINED, ZM_SIDE_PLAYER, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_SYLVASTAG, 95, 21950));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_EXP_GAINED, ZM_SIDE_PLAYER, 1u,
		ZM_MOVE_NONE, ZM_SPECIES_NIBBIN, 95, 195));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_LEVEL_UP, ZM_SIDE_PLAYER, 1u,
		ZM_MOVE_NONE, ZM_SPECIES_NIBBIN, 6, 41));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_END, ZM_SIDE_COUNT, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 2));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_END, ZM_SIDE_COUNT, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)ZM_SIDE_PLAYER));
	AssertStreamEq(xExpected, xEngine.GetEvents(), "learned-move-continuation");
	const ZM_BattleMonster& xReserve = xEngine.GetState().Side(ZM_SIDE_PLAYER).m_xParty.Get(1u);
	ZENITH_ASSERT_EQ((u_int)xReserve.m_axMoves[1].m_eMove, (u_int)ZM_MOVE_QUICKJAB);
	ZENITH_ASSERT_EQ(xReserve.m_axMoves[1].m_uCurPP, 29u);
	ZENITH_ASSERT_EQ(xReserve.m_axMoves[1].m_uMaxPP, 30u);
}

// Begin rebuilds every battle-only ledger field. With awards off, even the
// initial matchup remains untracked.
ZENITH_TEST(ZM_Battle, Engine_ProgressionLedgerResetsOnBegin)
{
	ZM_BattleMonsterSpec axPlayer[2] = {
		MakeSpecReal(ZM_SPECIES_SYLVASTAG, 30u, ZM_MOVE_MINDSHEAR),
		MakeSpecReal(ZM_SPECIES_SYLVASTAG, 30u, ZM_MOVE_MINDSHEAR) };
	ZM_BattleMonsterSpec axEnemy[1] = { MakeSpecReal(ZM_SPECIES_FERNFAWN, 5u, ZM_MOVE_MISTVEIL) };
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeExpConfig(true, false), axPlayer, 2u, axEnemy, 1u, 6ull, 54ull);
	ZENITH_ASSERT_TRUE(xEngine.DoSwitch(ZM_SIDE_PLAYER, 1u));
	ZENITH_ASSERT_EQ(xEngine.GetState().Side(ZM_SIDE_ENEMY).Active().m_uParticipantMask, 3u);
	xEngine.GetStateMutable().Side(ZM_SIDE_ENEMY).Active().m_bDefeatCredited = true;
	xEngine.GetStateMutable().Side(ZM_SIDE_PLAYER).Active().m_bLevelledThisBattle = true;
	xEngine.GetStateMutable().Side(ZM_SIDE_PLAYER).Active().m_bEvolutionQueued = true;

	xEngine.Begin(MakeExpConfig(true, false), axPlayer, 2u, axEnemy, 1u, 7ull, 54ull);
	ZENITH_ASSERT_EQ(xEngine.GetState().Side(ZM_SIDE_ENEMY).Active().m_uParticipantMask, 1u);
	ZENITH_ASSERT_FALSE(xEngine.GetState().Side(ZM_SIDE_ENEMY).Active().m_bDefeatCredited);
	ZENITH_ASSERT_FALSE(xEngine.GetState().Side(ZM_SIDE_PLAYER).Active().m_bLevelledThisBattle);
	ZENITH_ASSERT_FALSE(xEngine.GetState().Side(ZM_SIDE_PLAYER).Active().m_bEvolutionQueued);

	xEngine.Begin(MakeExpConfig(false, false), axPlayer, 2u, axEnemy, 1u, 8ull, 54ull);
	ZENITH_ASSERT_EQ(xEngine.GetState().Side(ZM_SIDE_ENEMY).Active().m_uParticipantMask, 0u);
}

// Evolution notification is terminal-only, after TURN_END and directly before
// BATTLE_END. A canceled/unchanged overlevelled form can queue again after a later
// level gain, and a multi-level award still queues one immediate edge.
ZENITH_TEST(ZM_Battle, Engine_EvolutionTerminalRequeueAndOneStep)
{
	ZM_BattleMonsterSpec xEnemy = MakeSpecReal(ZM_SPECIES_FERNFAWN, 5u, ZM_MOVE_MISTVEIL);
	ZM_BattleMonsterSpec xPlayer = MakeSpecOv(ZM_SPECIES_NIBBIN, 15u,
		200u, 250u, 200u, 100u, 200u, 250u, 31u, 0u, ZM_MOVE_MINDSHEAR);
	xPlayer.m_uCurExp = 3181u; // FAST L16 threshold 3276 minus gross 95
	ZM_BattleEngine xFirst;
	xFirst.Begin(MakeExpConfig(true, false), &xPlayer, 1u, &xEnemy, 1u, 9ull, 54ull);
	xFirst.GetStateMutable().Side(ZM_SIDE_ENEMY).Active().m_uCurHP = 1u;
	xFirst.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xFirst.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xFirst.ResolveTurn();
	ZENITH_ASSERT_EQ(CountKind(xFirst.GetEvents(), ZM_BATTLE_EVENT_EVOLUTION_QUEUED), 1u);
	const u_int uFirstCount = xFirst.GetEvents().GetSize();
	ZENITH_ASSERT_GE(uFirstCount, 2u);
	if (uFirstCount >= 2u)
	{
		ZENITH_ASSERT_EQ((u_int)xFirst.GetEvents().Get(uFirstCount - 2u).m_eKind,
			(u_int)ZM_BATTLE_EVENT_EVOLUTION_QUEUED);
		ZENITH_ASSERT_EQ((u_int)xFirst.GetEvents().Get(uFirstCount - 1u).m_eKind,
			(u_int)ZM_BATTLE_EVENT_BATTLE_END);
	}
	const ZM_BattleMonster& xFirstMon = xFirst.GetState().Side(ZM_SIDE_PLAYER).Active();
	ZENITH_ASSERT_EQ((u_int)xFirstMon.m_eSpecies, (u_int)ZM_SPECIES_NIBBIN);
	ZENITH_ASSERT_TRUE(xFirstMon.m_bEvolutionQueued);

	// Treat the first queue as canceled: keep Nibbin, then gain a later level.
	ZM_BattleMonsterSpec xLater = xPlayer;
	xLater.m_uLevel = 16u;
	xLater.m_uCurExp = 3835u; // FAST L17 threshold 3930 minus 95
	ZM_BattleEngine xSecond;
	xSecond.Begin(MakeExpConfig(true, false), &xLater, 1u, &xEnemy, 1u, 10ull, 54ull);
	xSecond.GetStateMutable().Side(ZM_SIDE_ENEMY).Active().m_uCurHP = 1u;
	xSecond.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xSecond.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xSecond.ResolveTurn();
	ZENITH_ASSERT_EQ(xSecond.GetState().Side(ZM_SIDE_PLAYER).Active().m_uLevel, 17u);
	ZENITH_ASSERT_EQ(CountKind(xSecond.GetEvents(), ZM_BATTLE_EVENT_EVOLUTION_QUEUED), 1u);

	ZM_BattleMonsterSpec xMulti = MakeSpecOv(ZM_SPECIES_NIBBIN, 15u,
		200u, 250u, 200u, 100u, 200u, 250u, 31u, 0u,
		ZM_MOVE_MINDSHEAR, ZM_MOVE_CINDERSPIT, ZM_MOVE_FLARELASH, ZM_MOVE_PYREBURST);
	ZM_BattleEngine xThird;
	xThird.Begin(MakeExpConfig(true, false), &xMulti, 1u, &xEnemy, 1u, 11ull, 54ull);
	xThird.GetStateMutable().Side(ZM_SIDE_PLAYER).Active().m_uCurExp = 51105u;
	xThird.GetStateMutable().Side(ZM_SIDE_ENEMY).Active().m_uCurHP = 1u;
	xThird.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xThird.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xThird.ResolveTurn();

	const u_int auLevelHP[25] = {
		94u, 100u, 105u, 110u, 116u, 121u, 126u, 132u, 137u, 142u,
		148u, 153u, 158u, 163u, 169u, 174u, 179u, 185u, 190u, 195u,
		201u, 206u, 211u, 217u, 222u };
	Zenith_Vector<ZM_BattleEvent> xExpected;
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_BEGIN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_PLAYER, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NIBBIN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_ENEMY, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_FERNFAWN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_BEGIN, ZM_SIDE_COUNT, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER, 0u,
		ZM_MOVE_MINDSHEAR));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_ENEMY, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 15, 0));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, ZM_SIDE_ENEMY, 0u));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_EXP_GAINED, ZM_SIDE_PLAYER, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NIBBIN, 95, 51200));
	for (u_int i = 0u; i < 25u; ++i)
	{
		xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_LEVEL_UP, ZM_SIDE_PLAYER, 0u,
			ZM_MOVE_NONE, ZM_SPECIES_NIBBIN, (int)(16u + i), (int)auLevelHP[i]));
	}
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_END, ZM_SIDE_COUNT, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_EVOLUTION_QUEUED, ZM_SIDE_PLAYER, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_HOARDEL, 40, 0));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_END, ZM_SIDE_COUNT, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)ZM_SIDE_PLAYER));
	AssertStreamEq(xExpected, xThird.GetEvents(), "multi-level-one-edge-evolution");
	const ZM_BattleMonster& xMultiResult = xThird.GetState().Side(ZM_SIDE_PLAYER).Active();
	ZENITH_ASSERT_EQ(xMultiResult.m_uLevel, 40u);
	ZENITH_ASSERT_EQ((u_int)xMultiResult.m_eSpecies, (u_int)ZM_SPECIES_NIBBIN);
	ZENITH_ASSERT_EQ(xMultiResult.m_xBaseStats.m_au[ZM_STAT_HP], 200u);
	ZENITH_ASSERT_TRUE(xMultiResult.m_bEvolutionQueued);
}

// An enabled but empty recipient set still consumes the defeated monster's
// exactly-once credit bit; a later sweep cannot resurrect an award.
ZENITH_TEST(ZM_Battle, Engine_NoEligibleRecipientStillCreditsDefeat)
{
	ZM_BattleMonsterSpec xPlayer = MakeSpecOv(ZM_SPECIES_NIBBIN, 30u,
		200u, 250u, 200u, 10u, 200u, 250u, 31u, 0u, ZM_MOVE_RECKLESSRUSH);
	ZM_BattleMonsterSpec xEnemy = MakeSpecReal(ZM_SPECIES_FERNFAWN, 5u, ZM_MOVE_RAMBASH);
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeExpConfig(true, false), &xPlayer, 1u, &xEnemy, 1u, 12ull, 54ull);
	xEngine.GetStateMutable().Side(ZM_SIDE_PLAYER).Active().m_uCurHP = 1u;
	xEngine.GetStateMutable().Side(ZM_SIDE_ENEMY).Active().m_uCurHP = 1u;
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xEngine.ResolveTurn();
	ZENITH_ASSERT_TRUE(xEngine.GetState().Side(ZM_SIDE_ENEMY).Active().m_bDefeatCredited);
	ZENITH_ASSERT_FALSE(HasKind(xEngine.GetEvents(), ZM_BATTLE_EVENT_EXP_GAINED));
}
