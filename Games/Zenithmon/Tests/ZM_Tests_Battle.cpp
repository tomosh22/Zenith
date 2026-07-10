#include "Zenith.h"

// ============================================================================
// ZM_Tests_Battle -- S2 Box 1 battle-engine keystone tests (category ZM_Battle).
//
// The 14 tests in Docs S2_Box1_Design section 5: monster-build/stat parity,
// the ZM_BattleEvent POD, the Gen-V damage pipeline, effectiveness percents,
// turn order, engine header emission, the fully-specified Nibbin-vs-Strayling
// characterization stream, three single-hit scenarios (first-damage / super /
// immune), and a 50-battle fuzz soak.
//
// The scenario golden (test 10) and every pinned number are cross-checked by an
// OFFLINE reference oracle (scratchpad/zm_battle_ref.py) whose PCG32 is
// validated against the S1 ZM_Tests_BattleRNG golden stream and whose stat math
// is validated against the S1 ZM_Tests_StatCalc golden vectors -- so the golden
// is an independent oracle, not an echo of the engine.
//
// The engine emits NO strings (ZM-D-010); the test-local ZM_BattleEventToString
// and ZM_ValidateEventStream below give failures a self-contained, element-by-
// element diagnostic. See DecisionLog ZM-D-027 / the S2 box-1 design spec.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Battle/ZM_BattleEngine.h"
#include "Zenithmon/Source/Battle/ZM_MoveExecutor.h"
#include "Zenithmon/Source/Battle/ZM_BattleMonster.h"
#include "Zenithmon/Source/Battle/ZM_BattleEvent.h"
#include "Zenithmon/Source/Battle/ZM_DamageCalc.h"
#include "Zenithmon/Source/Battle/ZM_BattleTypes.h"
#include "Zenithmon/Source/Data/ZM_Types.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_MoveData.h"
#include "Zenithmon/Source/Data/ZM_NatureData.h"
#include "Zenithmon/Source/Data/ZM_AbilityData.h"
#include "Zenithmon/Source/Data/ZM_TypeChart.h"
#include "Zenithmon/Source/Data/ZM_StatCalc.h"
#include "Zenithmon/Source/Data/ZM_BattleRNG.h"

// ============================================================================
// Test-local helpers (anonymous namespace: no cross-TU ODR clashes).
// ============================================================================
namespace
{
	// ---- event-kind -> short name (diagnostics only) ----------------------
	const char* ZM_EventKindName(ZM_BATTLE_EVENT eKind)
	{
		switch (eKind)
		{
		case ZM_BATTLE_EVENT_BATTLE_BEGIN:    return "BATTLE_BEGIN";
		case ZM_BATTLE_EVENT_TURN_BEGIN:      return "TURN_BEGIN";
		case ZM_BATTLE_EVENT_TURN_END:        return "TURN_END";
		case ZM_BATTLE_EVENT_SWITCH_IN:       return "SWITCH_IN";
		case ZM_BATTLE_EVENT_MOVE_USED:       return "MOVE_USED";
		case ZM_BATTLE_EVENT_MOVE_MISSED:     return "MOVE_MISSED";
		case ZM_BATTLE_EVENT_CRIT:            return "CRIT";
		case ZM_BATTLE_EVENT_SUPER_EFFECTIVE: return "SUPER_EFFECTIVE";
		case ZM_BATTLE_EVENT_NOT_EFFECTIVE:   return "NOT_EFFECTIVE";
		case ZM_BATTLE_EVENT_IMMUNE:          return "IMMUNE";
		case ZM_BATTLE_EVENT_DAMAGE_DEALT:    return "DAMAGE_DEALT";
		case ZM_BATTLE_EVENT_FAINT:           return "FAINT";
		case ZM_BATTLE_EVENT_BATTLE_END:      return "BATTLE_END";
		default:                              return "?";
		}
	}

	// Compact one-line stringification of a ZM_BattleEvent (all 7 scalar fields).
	void ZM_BattleEventToString(const ZM_BattleEvent& xEvent, char* acOut, size_t uSize)
	{
		snprintf(acOut, uSize, "[%s side=%u slot=%u move=%u species=%u amt=%d aux=%d]",
			ZM_EventKindName(xEvent.m_eKind), xEvent.m_uSide, xEvent.m_uSlot,
			xEvent.m_uMoveId, xEvent.m_uSpeciesId, xEvent.m_iAmount, xEvent.m_iAux);
	}

	// Log two event streams element-by-element (used on any scenario mismatch).
	void ZM_LogTwoStreams(const Zenith_Vector<ZM_BattleEvent>& xExpected,
		const Zenith_Vector<ZM_BattleEvent>& xActual)
	{
		const u_int uExp = xExpected.GetSize();
		const u_int uAct = xActual.GetSize();
		const u_int uMax = (uExp > uAct) ? uExp : uAct;
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_Battle] stream mismatch: expected %u events, actual %u events", uExp, uAct);
		char acExp[192];
		char acAct[192];
		for (u_int i = 0; i < uMax; ++i)
		{
			if (i < uExp) { ZM_BattleEventToString(xExpected.Get(i), acExp, sizeof(acExp)); }
			else          { snprintf(acExp, sizeof(acExp), "<none>"); }
			if (i < uAct) { ZM_BattleEventToString(xActual.Get(i), acAct, sizeof(acAct)); }
			else          { snprintf(acAct, sizeof(acAct), "<none>"); }
			const bool bEq = (i < uExp && i < uAct && xExpected.Get(i) == xActual.Get(i));
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  [%2u] %s exp=%s act=%s",
				i, bEq ? "   " : ">>>", acExp, acAct);
		}
	}

	// --- ZM_BattleEvent well-formedness (spec section 2.3, rules 1..8) ------
	// Returns true iff the stream satisfies every soak invariant; on the first
	// violation it logs the offending index + event and returns false. Needs the
	// engine for party sizes (rule 4 slot bounds) and per-monster max HP (rule 5).
	bool ZM_ValidateEventStream(const Zenith_Vector<ZM_BattleEvent>& xEvents,
		const ZM_BattleEngine& xEngine, const char* szLabel)
	{
		const ZM_BattleState& xState = xEngine.GetState();
		const u_int uCount = xEvents.GetSize();

		// Non-variadic on purpose (no nested variadic-macro forwarding -- an MSVC
		// preprocessor hazard). The offending event's own fields are dumped in
		// acBuf, so a fixed reason string is enough to localize the violation.
		char acBuf[192];
		#define ZM_VALIDATE_FAIL(uIndex, szReason)                                            \
			do {                                                                              \
				if ((u_int)(uIndex) < uCount) { ZM_BattleEventToString(xEvents.Get((uIndex)), acBuf, sizeof(acBuf)); } \
				else                          { snprintf(acBuf, sizeof(acBuf), "<oob>"); }     \
				Zenith_Log(LOG_CATEGORY_UNITTEST, "[ZM_Battle] validate(%s) FAIL @%u %s: %s",  \
					szLabel, (u_int)(uIndex), acBuf, (szReason));                              \
				return false;                                                                 \
			} while (0)

		// Rule 1: non-empty; events[0] == BATTLE_BEGIN.
		if (uCount == 0)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[ZM_Battle] validate(%s) FAIL: empty stream", szLabel);
			return false;
		}
		if (xEvents.Get(0).m_eKind != ZM_BATTLE_EVENT_BATTLE_BEGIN)
		{
			ZM_VALIDATE_FAIL(0u, "rule1: first event must be BATTLE_BEGIN");
		}

		// Rule 2 + 8: at most one BATTLE_END; if present it is last; amount in {0,1,2}.
		int iEndIdx = -1;
		for (u_int i = 0; i < uCount; ++i)
		{
			if (xEvents.Get(i).m_eKind == ZM_BATTLE_EVENT_BATTLE_END)
			{
				if (iEndIdx >= 0) { ZM_VALIDATE_FAIL(i, "rule2: more than one BATTLE_END"); }
				iEndIdx = (int)i;
			}
		}
		if (iEndIdx >= 0)
		{
			if ((u_int)iEndIdx != uCount - 1u) { ZM_VALIDATE_FAIL((u_int)iEndIdx, "rule2: BATTLE_END is not last"); }
			const int iWinner = xEvents.Get((u_int)iEndIdx).m_iAmount;
			if (iWinner < 0 || iWinner > (int)ZM_SIDE_COUNT) { ZM_VALIDATE_FAIL((u_int)iEndIdx, "rule8: BATTLE_END amount out of {0,1,2}"); }
		}

		// Rule 3: TURN_BEGIN(n)/TURN_END(n) balanced, n increments 1,2,3...
		int iExpectTurn = 1;
		bool bInTurn = false;
		for (u_int i = 0; i < uCount; ++i)
		{
			const ZM_BattleEvent& x = xEvents.Get(i);
			if (x.m_eKind == ZM_BATTLE_EVENT_TURN_BEGIN)
			{
				if (bInTurn)                  { ZM_VALIDATE_FAIL(i, "rule3: TURN_BEGIN without closing prior turn"); }
				if (x.m_iAux != iExpectTurn)  { ZM_VALIDATE_FAIL(i, "rule3: TURN_BEGIN number out of sequence"); }
				bInTurn = true;
			}
			else if (x.m_eKind == ZM_BATTLE_EVENT_TURN_END)
			{
				if (!bInTurn)                 { ZM_VALIDATE_FAIL(i, "rule3: TURN_END without a TURN_BEGIN"); }
				if (x.m_iAux != iExpectTurn)  { ZM_VALIDATE_FAIL(i, "rule3: TURN_END number out of sequence"); }
				bInTurn = false;
				++iExpectTurn;
			}
		}
		if (bInTurn) { ZM_VALIDATE_FAIL(uCount - 1u, "rule3: unbalanced -- last TURN_BEGIN never closed"); }

		// Per-monster fainted tracking (rule 7). Box-1 is 1v1 slot 0, but keep it
		// general to party size / side.
		bool aabFainted[ZM_SIDE_COUNT][uZM_MAX_PARTY_SIZE] = {};

		for (u_int i = 0; i < uCount; ++i)
		{
			const ZM_BattleEvent& x = xEvents.Get(i);
			const ZM_BATTLE_EVENT eKind = x.m_eKind;

			const bool bSubjectEvent =
				eKind == ZM_BATTLE_EVENT_SWITCH_IN   || eKind == ZM_BATTLE_EVENT_MOVE_USED    ||
				eKind == ZM_BATTLE_EVENT_MOVE_MISSED || eKind == ZM_BATTLE_EVENT_CRIT         ||
				eKind == ZM_BATTLE_EVENT_SUPER_EFFECTIVE || eKind == ZM_BATTLE_EVENT_NOT_EFFECTIVE ||
				eKind == ZM_BATTLE_EVENT_IMMUNE      || eKind == ZM_BATTLE_EVENT_DAMAGE_DEALT ||
				eKind == ZM_BATTLE_EVENT_FAINT;

			// Rule 4: side/slot bounds + move id validity for every subject event.
			if (bSubjectEvent)
			{
				if (x.m_uSide >= (u_int)ZM_SIDE_COUNT) { ZM_VALIDATE_FAIL(i, "rule4: side out of range"); }
				const u_int uPartySize = xState.Side((ZM_SIDE)x.m_uSide).m_xParty.GetSize();
				if (x.m_uSlot >= uPartySize) { ZM_VALIDATE_FAIL(i, "rule4: slot out of range for party"); }
				const bool bMoveOk = (x.m_uMoveId < (u_int)ZM_MOVE_COUNT) || (x.m_uMoveId == (u_int)ZM_MOVE_NONE);
				if (!bMoveOk) { ZM_VALIDATE_FAIL(i, "rule4: move id invalid"); }
			}

			switch (eKind)
			{
			case ZM_BATTLE_EVENT_SWITCH_IN:
				aabFainted[x.m_uSide][x.m_uSlot] = false;   // rule 7: switch-in re-fills the slot
				break;

			case ZM_BATTLE_EVENT_MOVE_USED:
				// Rule 7: a fainted monster must not act.
				if (aabFainted[x.m_uSide][x.m_uSlot]) { ZM_VALIDATE_FAIL(i, "rule7: fainted monster emitted MOVE_USED"); }
				break;

			case ZM_BATTLE_EVENT_DAMAGE_DEALT:
			{
				// Rule 5: amount >= 0 and remaining HP in [0, maxHP].
				if (x.m_iAmount < 0) { ZM_VALIDATE_FAIL(i, "rule5: DAMAGE_DEALT amount < 0"); }
				const u_int uMaxHP = xState.Side((ZM_SIDE)x.m_uSide).m_xParty.Get(x.m_uSlot).m_auMaxStat[ZM_STAT_HP];
				if (x.m_iAux < 0 || (u_int)x.m_iAux > uMaxHP)
				{
					ZM_VALIDATE_FAIL(i, "rule5: remaining HP not in [0,maxHP]");
				}
				break;
			}

			case ZM_BATTLE_EVENT_CRIT:
			{
				// Rule 6: CRIT sits immediately before SUPER/NOT/DAMAGE of the same defender.
				if (i + 1u >= uCount) { ZM_VALIDATE_FAIL(i, "rule6: CRIT with no following event"); }
				const ZM_BattleEvent& n = xEvents.Get(i + 1u);
				const bool bOk = (n.m_eKind == ZM_BATTLE_EVENT_SUPER_EFFECTIVE ||
				                  n.m_eKind == ZM_BATTLE_EVENT_NOT_EFFECTIVE ||
				                  n.m_eKind == ZM_BATTLE_EVENT_DAMAGE_DEALT) &&
				                  n.m_uSide == x.m_uSide && n.m_uSlot == x.m_uSlot;
				if (!bOk) { ZM_VALIDATE_FAIL(i, "rule6: CRIT not immediately before same-defender SUPER/NOT/DAMAGE"); }
				break;
			}

			case ZM_BATTLE_EVENT_SUPER_EFFECTIVE:
			{
				if (x.m_iAmount != 200 && x.m_iAmount != 400) { ZM_VALIDATE_FAIL(i, "rule6: SUPER_EFFECTIVE amount not in {200,400}"); }
				if (i + 1u >= uCount) { ZM_VALIDATE_FAIL(i, "rule6: SUPER_EFFECTIVE with no following event"); }
				const ZM_BattleEvent& n = xEvents.Get(i + 1u);
				if (n.m_eKind != ZM_BATTLE_EVENT_DAMAGE_DEALT || n.m_uSide != x.m_uSide || n.m_uSlot != x.m_uSlot)
				{
					ZM_VALIDATE_FAIL(i, "rule6: SUPER_EFFECTIVE not immediately before same-defender DAMAGE_DEALT");
				}
				break;
			}

			case ZM_BATTLE_EVENT_NOT_EFFECTIVE:
			{
				if (x.m_iAmount != 25 && x.m_iAmount != 50) { ZM_VALIDATE_FAIL(i, "rule6: NOT_EFFECTIVE amount not in {25,50}"); }
				if (i + 1u >= uCount) { ZM_VALIDATE_FAIL(i, "rule6: NOT_EFFECTIVE with no following event"); }
				const ZM_BattleEvent& n = xEvents.Get(i + 1u);
				if (n.m_eKind != ZM_BATTLE_EVENT_DAMAGE_DEALT || n.m_uSide != x.m_uSide || n.m_uSlot != x.m_uSlot)
				{
					ZM_VALIDATE_FAIL(i, "rule6: NOT_EFFECTIVE not immediately before same-defender DAMAGE_DEALT");
				}
				break;
			}

			case ZM_BATTLE_EVENT_IMMUNE:
			case ZM_BATTLE_EVENT_MOVE_MISSED:
			{
				// Rule 6: a missed/immune hit produces no damage/crit/effectiveness for that group.
				if (i + 1u < uCount)
				{
					const ZM_BATTLE_EVENT eNext = xEvents.Get(i + 1u).m_eKind;
					const bool bBad = (eNext == ZM_BATTLE_EVENT_DAMAGE_DEALT ||
					                   eNext == ZM_BATTLE_EVENT_CRIT ||
					                   eNext == ZM_BATTLE_EVENT_SUPER_EFFECTIVE ||
					                   eNext == ZM_BATTLE_EVENT_NOT_EFFECTIVE);
					if (bBad) { ZM_VALIDATE_FAIL(i, "rule6: IMMUNE/MISSED followed by damage/crit/effectiveness"); }
				}
				break;
			}

			case ZM_BATTLE_EVENT_FAINT:
			{
				// Rule 7: at most once per (side,slot); preceded by DAMAGE_DEALT (iAux==0) same monster.
				if (aabFainted[x.m_uSide][x.m_uSlot]) { ZM_VALIDATE_FAIL(i, "rule7: duplicate FAINT for the same monster"); }
				if (i == 0u) { ZM_VALIDATE_FAIL(i, "rule7: FAINT with no preceding DAMAGE_DEALT"); }
				const ZM_BattleEvent& p = xEvents.Get(i - 1u);
				const bool bPrevOk = p.m_eKind == ZM_BATTLE_EVENT_DAMAGE_DEALT &&
				                     p.m_uSide == x.m_uSide && p.m_uSlot == x.m_uSlot && p.m_iAux == 0;
				if (!bPrevOk) { ZM_VALIDATE_FAIL(i, "rule7: FAINT not preceded by same-monster DAMAGE_DEALT(remaining==0)"); }
				aabFainted[x.m_uSide][x.m_uSlot] = true;
				break;
			}

			default:
				break;
			}
		}

		#undef ZM_VALIDATE_FAIL
		return true;
	}

	// --- spec builders ------------------------------------------------------
	ZM_BattleMonsterSpec MakeSpec(ZM_SPECIES_ID eSpecies, u_int uLevel, ZM_MOVE_ID eMove0)
	{
		ZM_BattleMonsterSpec xSpec;
		xSpec.m_eSpecies = eSpecies;
		xSpec.m_uLevel = uLevel;
		for (u_int i = 0; i < ZM_STAT_COUNT; ++i) { xSpec.m_auIV[i] = 31u; xSpec.m_auEV[i] = 0u; }
		xSpec.m_eNature = ZM_NATURE_FERAL;
		xSpec.m_eAbility = ZM_ABILITY_NONE;
		xSpec.m_aeMoves[0] = eMove0;
		xSpec.m_aeMoves[1] = ZM_MOVE_NONE;
		xSpec.m_aeMoves[2] = ZM_MOVE_NONE;
		xSpec.m_aeMoves[3] = ZM_MOVE_NONE;
		xSpec.m_bOverrideBaseStats = false;
		return xSpec;
	}

	// Same, with an explicit base-stat override (6 values in ZM_STAT order).
	ZM_BattleMonsterSpec MakeSpecOverride(ZM_SPECIES_ID eSpecies, u_int uLevel, ZM_MOVE_ID eMove0,
		u_int uHP, u_int uATK, u_int uDEF, u_int uSPA, u_int uSPD, u_int uSPE)
	{
		ZM_BattleMonsterSpec xSpec = MakeSpec(eSpecies, uLevel, eMove0);
		xSpec.m_bOverrideBaseStats = true;
		xSpec.m_xBaseStatsOverride.m_au[ZM_STAT_HP]        = uHP;
		xSpec.m_xBaseStatsOverride.m_au[ZM_STAT_ATTACK]    = uATK;
		xSpec.m_xBaseStatsOverride.m_au[ZM_STAT_DEFENSE]   = uDEF;
		xSpec.m_xBaseStatsOverride.m_au[ZM_STAT_SPATTACK]  = uSPA;
		xSpec.m_xBaseStatsOverride.m_au[ZM_STAT_SPDEFENSE] = uSPD;
		xSpec.m_xBaseStatsOverride.m_au[ZM_STAT_SPEED]     = uSPE;
		return xSpec;
	}

	// One-liner around ZM_CalcDamage with the box-1 identity weather/burn/screen.
	u_int CalcDmg(u_int uLevel, u_int uPower, u_int uAttack, u_int uDefense,
		bool bStab, u_int uEffPercent, bool bCrit, u_int uRoll)
	{
		ZM_DamageInput xIn;
		xIn.uLevel = uLevel;
		xIn.uPower = uPower;
		xIn.uAttack = uAttack;
		xIn.uDefense = uDefense;
		xIn.bStab = bStab;
		xIn.uEffectivenessPercent = uEffPercent;
		xIn.bCrit = bCrit;
		xIn.uRandomPercent = uRoll;
		return ZM_CalcDamage(xIn);
	}

	// The scenario-10 override teams (spec section 5).
	ZM_BattleMonsterSpec MakeScenarioNibbin()
	{
		return MakeSpecOverride(ZM_SPECIES_NIBBIN, 10u, ZM_MOVE_RAMBASH, 50u, 60u, 45u, 40u, 45u, 70u);
	}
	ZM_BattleMonsterSpec MakeScenarioStrayling()
	{
		return MakeSpecOverride(ZM_SPECIES_STRAYLING, 10u, ZM_MOVE_RAMBASH, 45u, 50u, 40u, 35u, 40u, 55u);
	}

	ZM_BattleConfig MakeTrainerConfig()
	{
		ZM_BattleConfig xCfg;
		xCfg.m_bIsWild = false;
		xCfg.m_bCanCatch = false;
		xCfg.m_bCanFlee = false;
		xCfg.m_uLevelCap = 0u;
		return xCfg;
	}

	ZM_BattleAction MoveAction(u_int uSlot)
	{
		ZM_BattleAction xAction;
		xAction.m_eKind = ZM_ACTION_MOVE;
		xAction.m_uMoveSlot = uSlot;
		return xAction;
	}

	// First MOVE_USED after Begin/first turn -> the side that acted first.
	u_int FirstActorSide(const ZM_BattleEngine& xEngine)
	{
		const Zenith_Vector<ZM_BattleEvent>& xEvents = xEngine.GetEvents();
		for (u_int i = 0; i < xEvents.GetSize(); ++i)
		{
			if (xEvents.Get(i).m_eKind == ZM_BATTLE_EVENT_MOVE_USED) { return xEvents.Get(i).m_uSide; }
		}
		return (u_int)ZM_SIDE_COUNT;
	}
}

// ============================================================================
// 1. BuildMonster_StatsMatchStatCalc -- natural build == ZM_CalcStat per stat.
// ============================================================================
ZENITH_TEST(ZM_Battle, BuildMonster_StatsMatchStatCalc)
{
	ZM_BattleMonsterSpec xSpec = MakeSpec(ZM_SPECIES_NIBBIN, 10u, ZM_MOVE_RAMBASH);
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(xSpec);

	const ZM_BaseStats xBase = ZM_GetSpeciesBaseStats(ZM_SPECIES_NIBBIN);
	for (u_int i = 0; i < ZM_STAT_COUNT; ++i)
	{
		const u_int uExpected = ZM_CalcStat((ZM_STAT)i, xBase.m_au[i], 31u, 0u, 10u, ZM_NATURE_FERAL);
		ZENITH_ASSERT_EQ(xMon.m_auMaxStat[i], uExpected, "stat %u mismatch", i);
	}

	// Current HP == max HP; not fainted.
	ZENITH_ASSERT_EQ(xMon.m_uCurHP, xMon.m_auMaxStat[ZM_STAT_HP]);
	ZENITH_ASSERT_FALSE(xMon.IsFainted());

	// Move slot 0 == Rambash with its full PP; empty slots are NONE / 0 PP.
	ZENITH_ASSERT_EQ((u_int)xMon.m_axMoves[0].m_eMove, (u_int)ZM_MOVE_RAMBASH);
	const u_int uRambashPP = ZM_GetMoveData(ZM_MOVE_RAMBASH).m_uPP;
	ZENITH_ASSERT_EQ(xMon.m_axMoves[0].m_uCurPP, uRambashPP);
	ZENITH_ASSERT_EQ(xMon.m_axMoves[0].m_uMaxPP, uRambashPP);
	for (u_int i = 1; i < uZM_MAX_MOVES; ++i)
	{
		ZENITH_ASSERT_EQ((u_int)xMon.m_axMoves[i].m_eMove, (u_int)ZM_MOVE_NONE, "slot %u not empty", i);
		ZENITH_ASSERT_EQ(xMon.m_axMoves[i].m_uCurPP, 0u);
		ZENITH_ASSERT_EQ(xMon.m_axMoves[i].m_uMaxPP, 0u);
	}

	// Stages all 0; status NONE; volatiles 0.
	for (u_int i = 0; i < ZM_BATTLE_STAT_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ(xMon.m_aiStage[i], 0, "stage %u nonzero", i);
	}
	ZENITH_ASSERT_EQ((u_int)xMon.m_eStatus, (u_int)ZM_MAJOR_STATUS_NONE);
	ZENITH_ASSERT_EQ(xMon.m_uVolatileMask, (u_int)ZM_VOLATILE_NONE);
}

// ============================================================================
// 2. BuildMonster_OverrideBaseStats -- exact derived table from the override.
// ============================================================================
ZENITH_TEST(ZM_Battle, BuildMonster_OverrideBaseStats)
{
	// Nibbin override {HP50,ATK60,DEF45,SPA40,SPD45,SPE70} @ L10 IV31 EV0 FERAL.
	ZM_BattleMonster xNibbin = ZM_BuildBattleMonster(MakeScenarioNibbin());
	ZENITH_ASSERT_EQ(xNibbin.m_auMaxStat[ZM_STAT_HP],        33u);
	ZENITH_ASSERT_EQ(xNibbin.m_auMaxStat[ZM_STAT_ATTACK],    20u);
	ZENITH_ASSERT_EQ(xNibbin.m_auMaxStat[ZM_STAT_DEFENSE],   17u);
	ZENITH_ASSERT_EQ(xNibbin.m_auMaxStat[ZM_STAT_SPATTACK],  16u);
	ZENITH_ASSERT_EQ(xNibbin.m_auMaxStat[ZM_STAT_SPDEFENSE], 17u);
	ZENITH_ASSERT_EQ(xNibbin.m_auMaxStat[ZM_STAT_SPEED],     22u);
	ZENITH_ASSERT_EQ(xNibbin.m_uCurHP, 33u);

	// Strayling override {HP45,ATK50,DEF40,SPA35,SPD40,SPE55}.
	ZM_BattleMonster xStrayling = ZM_BuildBattleMonster(MakeScenarioStrayling());
	ZENITH_ASSERT_EQ(xStrayling.m_auMaxStat[ZM_STAT_HP],        32u);
	ZENITH_ASSERT_EQ(xStrayling.m_auMaxStat[ZM_STAT_ATTACK],    18u);
	ZENITH_ASSERT_EQ(xStrayling.m_auMaxStat[ZM_STAT_DEFENSE],   16u);
	ZENITH_ASSERT_EQ(xStrayling.m_auMaxStat[ZM_STAT_SPATTACK],  15u);
	ZENITH_ASSERT_EQ(xStrayling.m_auMaxStat[ZM_STAT_SPDEFENSE], 16u);
	ZENITH_ASSERT_EQ(xStrayling.m_auMaxStat[ZM_STAT_SPEED],     19u);
	ZENITH_ASSERT_EQ(xStrayling.m_uCurHP, 32u);

	// Nibbin faster than Strayling (drives the tie-break-free scenario order).
	ZENITH_ASSERT_GT(xNibbin.m_auMaxStat[ZM_STAT_SPEED], xStrayling.m_auMaxStat[ZM_STAT_SPEED]);
}

// ============================================================================
// 3. BuildMonster_DeepOwnCopy -- built monsters are independent of the spec and
//    of each other (the battle deep-owns by value).
// ============================================================================
ZENITH_TEST(ZM_Battle, BuildMonster_DeepOwnCopy)
{
	ZM_BattleMonsterSpec xSpec = MakeScenarioNibbin();

	ZM_BattleMonster xA = ZM_BuildBattleMonster(xSpec);
	const u_int uFullHP = xA.m_auMaxStat[ZM_STAT_HP];
	ZENITH_ASSERT_EQ(xA.m_uCurHP, uFullHP);

	// Mutating the built monster must not touch the spec.
	xA.m_uCurHP = 0u;
	xA.m_axMoves[0].m_uCurPP = 0u;
	ZENITH_ASSERT_EQ(xSpec.m_uLevel, 10u);
	ZENITH_ASSERT_EQ((u_int)xSpec.m_aeMoves[0], (u_int)ZM_MOVE_RAMBASH);
	ZENITH_ASSERT_TRUE(xSpec.m_bOverrideBaseStats);

	// Re-building from the same (unmutated) spec yields a full-HP monster again.
	ZM_BattleMonster xB = ZM_BuildBattleMonster(xSpec);
	ZENITH_ASSERT_EQ(xB.m_uCurHP, uFullHP);
	ZENITH_ASSERT_EQ(xB.m_axMoves[0].m_uCurPP, ZM_GetMoveData(ZM_MOVE_RAMBASH).m_uPP);
	// ...and xA's mutation did not bleed into xB.
	ZENITH_ASSERT_EQ(xA.m_uCurHP, 0u);

	// The engine's party is a deep-owned copy: rigging its HP leaves the spec whole.
	ZM_BattleMonsterSpec axPlayer[1] = { MakeScenarioNibbin() };
	ZM_BattleMonsterSpec axEnemy[1]  = { MakeScenarioStrayling() };
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeTrainerConfig(), axPlayer, 1u, axEnemy, 1u, 0x1234ull, 54ull);
	xEngine.GetStateMutable().Side(ZM_SIDE_PLAYER).m_xParty.Get(0).m_uCurHP = 1u;
	ZENITH_ASSERT_EQ(axPlayer[0].m_uLevel, 10u);
	ZENITH_ASSERT_EQ((u_int)axPlayer[0].m_eSpecies, (u_int)ZM_SPECIES_NIBBIN);
	ZENITH_ASSERT_EQ(xEngine.GetState().Side(ZM_SIDE_PLAYER).m_xParty.Get(0).m_uCurHP, 1u);
}

// ============================================================================
// 4. Event_PODEquality -- POD, defaulted ==, and ZM_MakeEvent defaults.
// ============================================================================
ZENITH_TEST(ZM_Battle, Event_PODEquality)
{
	static_assert(std::is_trivially_copyable_v<ZM_BattleEvent>, "ZM_BattleEvent must be trivially copyable");

	const ZM_BattleEvent xA = ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_ENEMY, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 10, 22);
	const ZM_BattleEvent xB = ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_ENEMY, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 10, 22);
	ZENITH_ASSERT_TRUE(xA == xB);

	// Every single differing field breaks equality.
	{ ZM_BattleEvent x = xA; x.m_eKind = ZM_BATTLE_EVENT_FAINT;   ZENITH_ASSERT_FALSE(x == xA); }
	{ ZM_BattleEvent x = xA; x.m_uSide = ZM_SIDE_PLAYER;          ZENITH_ASSERT_FALSE(x == xA); }
	{ ZM_BattleEvent x = xA; x.m_uSlot = 1u;                      ZENITH_ASSERT_FALSE(x == xA); }
	{ ZM_BattleEvent x = xA; x.m_uMoveId = (u_int)ZM_MOVE_RAMBASH; ZENITH_ASSERT_FALSE(x == xA); }
	{ ZM_BattleEvent x = xA; x.m_uSpeciesId = (u_int)ZM_SPECIES_NIBBIN; ZENITH_ASSERT_FALSE(x == xA); }
	{ ZM_BattleEvent x = xA; x.m_iAmount = 11;                    ZENITH_ASSERT_FALSE(x == xA); }
	{ ZM_BattleEvent x = xA; x.m_iAux = 21;                       ZENITH_ASSERT_FALSE(x == xA); }

	// ZM_MakeEvent defaults: side=COUNT, slot=0, move=NONE, species=NONE, amount=0, aux=0.
	const ZM_BattleEvent xDef = ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_BEGIN);
	ZENITH_ASSERT_EQ((u_int)xDef.m_eKind, (u_int)ZM_BATTLE_EVENT_BATTLE_BEGIN);
	ZENITH_ASSERT_EQ(xDef.m_uSide, (u_int)ZM_SIDE_COUNT);
	ZENITH_ASSERT_EQ(xDef.m_uSlot, 0u);
	ZENITH_ASSERT_EQ(xDef.m_uMoveId, (u_int)ZM_MOVE_NONE);
	ZENITH_ASSERT_EQ(xDef.m_uSpeciesId, (u_int)ZM_SPECIES_NONE);
	ZENITH_ASSERT_EQ(xDef.m_iAmount, 0);
	ZENITH_ASSERT_EQ(xDef.m_iAux, 0);
}

// ============================================================================
// 5. Damage_GenVGoldenVectors -- pinned ZM_CalcDamage inputs -> hand-derived
//    outputs (cross-checked by the offline oracle).
// ============================================================================
ZENITH_TEST(ZM_Battle, Damage_GenVGoldenVectors)
{
	// Rambash base case (a=6, t=337, base=8): non-crit r85 -> 9; crit r100 -> 18.
	ZENITH_ASSERT_EQ(CalcDmg(10u, 45u, 20u, 16u, true, 100u, false, 85u), 9u);
	ZENITH_ASSERT_EQ(CalcDmg(10u, 45u, 20u, 16u, true, 100u, true, 100u), 18u);
	// Same base, no STAB, r85 -> 6.
	ZENITH_ASSERT_EQ(CalcDmg(10u, 45u, 20u, 16u, false, 100u, false, 85u), 6u);
	// Effectiveness scaling (STAB, r100): 200 -> 24, 50 -> 6, 400 -> 48.
	ZENITH_ASSERT_EQ(CalcDmg(10u, 45u, 20u, 16u, true, 200u, false, 100u), 24u);
	ZENITH_ASSERT_EQ(CalcDmg(10u, 45u, 20u, 16u, true, 50u, false, 100u), 6u);
	ZENITH_ASSERT_EQ(CalcDmg(10u, 45u, 20u, 16u, true, 400u, false, 100u), 48u);
	// Immune (eff 0) -> exactly 0.
	ZENITH_ASSERT_EQ(CalcDmg(10u, 45u, 20u, 16u, true, 0u, false, 100u), 0u);
	// Min-1 floor: tiny attacker vs huge defense but not immune -> clamps to 1.
	ZENITH_ASSERT_EQ(CalcDmg(1u, 10u, 5u, 255u, false, 25u, false, 85u), 1u);
}

// ============================================================================
// 6. Damage_StageMultipliers -- ZM_ApplyStatStage boundary values.
// ============================================================================
ZENITH_TEST(ZM_Battle, Damage_StageMultipliers)
{
	ZENITH_ASSERT_EQ(ZM_ApplyStatStage(200u,  0), 200u);
	ZENITH_ASSERT_EQ(ZM_ApplyStatStage(200u, +1), 300u);
	ZENITH_ASSERT_EQ(ZM_ApplyStatStage(200u, +2), 400u);
	ZENITH_ASSERT_EQ(ZM_ApplyStatStage(200u, -1), 133u);
	ZENITH_ASSERT_EQ(ZM_ApplyStatStage(200u, -6), 50u);
	// Full positive/negative extremes: +6 -> x4, -6 -> x1/4 (of 200 -> 50).
	ZENITH_ASSERT_EQ(ZM_ApplyStatStage(200u, +6), 800u);
}

// ============================================================================
// 7. Effectiveness_PercentMapping -- integer percents {0,25,50,100,200,400}.
// ============================================================================
ZENITH_TEST(ZM_Battle, Effectiveness_PercentMapping)
{
	// Mono defenders (second type NONE): neutral is 100, NOT 50.
	ZENITH_ASSERT_EQ(ZM_EffectivenessPercent(ZM_TYPE_NORMAL, ZM_TYPE_NORMAL,  ZM_TYPE_NONE), 100u);
	ZENITH_ASSERT_EQ(ZM_EffectivenessPercent(ZM_TYPE_NORMAL, ZM_TYPE_PHANTOM, ZM_TYPE_NONE),   0u);
	ZENITH_ASSERT_EQ(ZM_EffectivenessPercent(ZM_TYPE_NORMAL, ZM_TYPE_STONE,   ZM_TYPE_NONE),  50u);
	ZENITH_ASSERT_EQ(ZM_EffectivenessPercent(ZM_TYPE_FIRE,   ZM_TYPE_GRASS,   ZM_TYPE_NONE), 200u);
	// Dual defenders: 2x*2x -> 400; 0.5x*0.5x -> 25; 0x*anything -> 0.
	ZENITH_ASSERT_EQ(ZM_EffectivenessPercent(ZM_TYPE_FIRE,   ZM_TYPE_GRASS,   ZM_TYPE_SWARM), 400u);
	ZENITH_ASSERT_EQ(ZM_EffectivenessPercent(ZM_TYPE_FIRE,   ZM_TYPE_FIRE,    ZM_TYPE_STONE),  25u);
	ZENITH_ASSERT_EQ(ZM_EffectivenessPercent(ZM_TYPE_NORMAL, ZM_TYPE_PHANTOM, ZM_TYPE_IRON),    0u);
	// A duplicated second slot is treated as single-typed (never squared).
	ZENITH_ASSERT_EQ(ZM_EffectivenessPercent(ZM_TYPE_FIRE,   ZM_TYPE_GRASS,   ZM_TYPE_GRASS), 200u);
}

// ============================================================================
// 8. TurnOrder_SpeedPriorityTie -- speed, priority override, seeded tie-break.
//    Observed via the first MOVE_USED of turn 1.
// ============================================================================
ZENITH_TEST(ZM_Battle, TurnOrder_SpeedPriorityTie)
{
	const ZM_BattleConfig xCfg = MakeTrainerConfig();

	// (a) Faster effective speed acts first. Both NORMAL (Rambash, prio 0);
	//     bulky (high HP, low ATK) so neither faints turn 1.
	{
		ZM_BattleMonsterSpec axFast[1] = { MakeSpecOverride(ZM_SPECIES_NIBBIN, 50u, ZM_MOVE_RAMBASH, 200u, 10u, 200u, 10u, 200u, 200u) };
		ZM_BattleMonsterSpec axSlow[1] = { MakeSpecOverride(ZM_SPECIES_NIBBIN, 50u, ZM_MOVE_RAMBASH, 200u, 10u, 200u, 10u, 200u,  10u) };

		ZM_BattleEngine xP;   // player faster
		xP.Begin(xCfg, axFast, 1u, axSlow, 1u, 0x1ull, 54ull);
		xP.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
		xP.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
		xP.ResolveTurn();
		ZENITH_ASSERT_EQ(FirstActorSide(xP), (u_int)ZM_SIDE_PLAYER);

		ZM_BattleEngine xE;   // enemy faster
		xE.Begin(xCfg, axSlow, 1u, axFast, 1u, 0x1ull, 54ull);
		xE.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
		xE.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
		xE.ResolveTurn();
		ZENITH_ASSERT_EQ(FirstActorSide(xE), (u_int)ZM_SIDE_ENEMY);
	}

	// (b) Higher priority overrides speed: the SLOWER side holds Quickjab (prio 1),
	//     the faster side holds Rambash (prio 0) -> the slower side acts first.
	{
		ZM_BattleMonsterSpec axSlowHiPri[1] = { MakeSpecOverride(ZM_SPECIES_NIBBIN, 50u, ZM_MOVE_QUICKJAB, 200u, 10u, 200u, 10u, 200u,  10u) };
		ZM_BattleMonsterSpec axFastLoPri[1] = { MakeSpecOverride(ZM_SPECIES_NIBBIN, 50u, ZM_MOVE_RAMBASH,  200u, 10u, 200u, 10u, 200u, 200u) };

		ZM_BattleEngine xEngine;   // player is slow but high-priority
		xEngine.Begin(xCfg, axSlowHiPri, 1u, axFastLoPri, 1u, 0x1ull, 54ull);
		xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
		xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
		xEngine.ResolveTurn();
		ZENITH_ASSERT_EQ(FirstActorSide(xEngine), (u_int)ZM_SIDE_PLAYER);
	}

	// (c) Exact speed tie -> single seeded RandBelow(2) tie-break, deterministic.
	//     tie-break = (first RNG output & 1): seed 2 -> PLAYER, seed 1 -> ENEMY
	//     (derived from the validated PCG32 oracle). Identical bulky monsters.
	{
		ZM_BattleMonsterSpec axP[1] = { MakeSpecOverride(ZM_SPECIES_NIBBIN, 50u, ZM_MOVE_RAMBASH, 200u, 10u, 200u, 10u, 200u, 100u) };
		ZM_BattleMonsterSpec axE[1] = { MakeSpecOverride(ZM_SPECIES_NIBBIN, 50u, ZM_MOVE_RAMBASH, 200u, 10u, 200u, 10u, 200u, 100u) };

		ZM_BattleEngine xSeed2;   // firstNext even -> PLAYER first
		xSeed2.Begin(xCfg, axP, 1u, axE, 1u, 2ull, 54ull);
		xSeed2.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
		xSeed2.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
		xSeed2.ResolveTurn();
		ZENITH_ASSERT_EQ(FirstActorSide(xSeed2), (u_int)ZM_SIDE_PLAYER);

		ZM_BattleEngine xSeed1;   // firstNext odd -> ENEMY first
		xSeed1.Begin(xCfg, axP, 1u, axE, 1u, 1ull, 54ull);
		xSeed1.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
		xSeed1.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
		xSeed1.ResolveTurn();
		ZENITH_ASSERT_EQ(FirstActorSide(xSeed1), (u_int)ZM_SIDE_ENEMY);
	}
}

// ============================================================================
// 9. Engine_BeginEmitsHeader -- Begin emits exactly BATTLE_BEGIN + two SWITCH_IN.
// ============================================================================
ZENITH_TEST(ZM_Battle, Engine_BeginEmitsHeader)
{
	ZM_BattleMonsterSpec axPlayer[1] = { MakeScenarioNibbin() };
	ZM_BattleMonsterSpec axEnemy[1]  = { MakeScenarioStrayling() };

	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeTrainerConfig(), axPlayer, 1u, axEnemy, 1u, 0x1234ull, 54ull);

	Zenith_Vector<ZM_BattleEvent> xExpected;
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_BEGIN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_PLAYER, 0u, ZM_MOVE_NONE, ZM_SPECIES_NIBBIN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_ENEMY,  0u, ZM_MOVE_NONE, ZM_SPECIES_STRAYLING));

	const Zenith_Vector<ZM_BattleEvent>& xActual = xEngine.GetEvents();
	ZENITH_ASSERT_EQ(xActual.GetSize(), xExpected.GetSize());
	if (xActual.GetSize() == xExpected.GetSize())
	{
		for (u_int i = 0; i < xExpected.GetSize(); ++i)
		{
			char acE[192];
			char acA[192];
			ZM_BattleEventToString(xExpected.Get(i), acE, sizeof(acE));
			ZM_BattleEventToString(xActual.Get(i), acA, sizeof(acA));
			ZENITH_ASSERT_TRUE(xExpected.Get(i) == xActual.Get(i), "header[%u] exp=%s act=%s", i, acE, acA);
		}
	}

	ZENITH_ASSERT_EQ(xEngine.GetState().Side(ZM_SIDE_PLAYER).m_uActiveSlot, 0u);
	ZENITH_ASSERT_EQ(xEngine.GetState().Side(ZM_SIDE_ENEMY).m_uActiveSlot, 0u);
	ZENITH_ASSERT_FALSE(xEngine.IsOver());
}

// ============================================================================
// 10. Scenario_NibbinVsStrayling_ExactStream -- full characterization golden.
//     Derived by the offline oracle (scratchpad/zm_battle_ref.py, seed 0x1234).
// ============================================================================
ZENITH_TEST(ZM_Battle, Scenario_NibbinVsStrayling_ExactStream)
{
	ZM_BattleMonsterSpec axPlayer[1] = { MakeScenarioNibbin() };
	ZM_BattleMonsterSpec axEnemy[1]  = { MakeScenarioStrayling() };

	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeTrainerConfig(), axPlayer, 1u, axEnemy, 1u, 0x1234ull, 54ull);

	u_int uGuard = 0u;
	while (!xEngine.IsOver() && uGuard < 500u)
	{
		xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
		xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
		xEngine.ResolveTurn();
		++uGuard;
	}
	ZENITH_ASSERT_TRUE(xEngine.IsOver(), "battle did not terminate");

	// --- offline-derived golden stream (27 events) ---
	Zenith_Vector<ZM_BattleEvent> xExpected;
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_BEGIN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_PLAYER, 0, ZM_MOVE_NONE, ZM_SPECIES_NIBBIN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_ENEMY, 0, ZM_MOVE_NONE, ZM_SPECIES_STRAYLING));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_BEGIN, ZM_SIDE_COUNT, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER, 0, ZM_MOVE_RAMBASH));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_ENEMY, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 10, 22));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_ENEMY, 0, ZM_MOVE_RAMBASH));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_PLAYER, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 10, 23));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_END, ZM_SIDE_COUNT, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_BEGIN, ZM_SIDE_COUNT, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 2));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER, 0, ZM_MOVE_RAMBASH));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_ENEMY, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 10, 12));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_ENEMY, 0, ZM_MOVE_RAMBASH));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_PLAYER, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 9, 14));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_END, ZM_SIDE_COUNT, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 2));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_BEGIN, ZM_SIDE_COUNT, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 3));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER, 0, ZM_MOVE_RAMBASH));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_ENEMY, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 10, 2));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_ENEMY, 0, ZM_MOVE_RAMBASH));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_PLAYER, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 10, 4));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_END, ZM_SIDE_COUNT, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 3));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_BEGIN, ZM_SIDE_COUNT, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 4));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER, 0, ZM_MOVE_RAMBASH));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_ENEMY, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 9));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, ZM_SIDE_ENEMY));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_END, ZM_SIDE_COUNT, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 4));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_END));

	const Zenith_Vector<ZM_BattleEvent>& xActual = xEngine.GetEvents();

	// Element-by-element compare; on any drift, dump both streams to localize.
	bool bMatch = (xExpected.GetSize() == xActual.GetSize());
	const u_int uMin = (xExpected.GetSize() < xActual.GetSize()) ? xExpected.GetSize() : xActual.GetSize();
	for (u_int i = 0; i < uMin; ++i)
	{
		if (!(xExpected.Get(i) == xActual.Get(i))) { bMatch = false; }
	}
	if (!bMatch) { ZM_LogTwoStreams(xExpected, xActual); }

	ZENITH_ASSERT_EQ(xActual.GetSize(), xExpected.GetSize(), "scenario stream length mismatch");
	for (u_int i = 0; i < uMin; ++i)
	{
		char acE[192];
		char acA[192];
		ZM_BattleEventToString(xExpected.Get(i), acE, sizeof(acE));
		ZM_BattleEventToString(xActual.Get(i), acA, sizeof(acA));
		ZENITH_ASSERT_TRUE(xExpected.Get(i) == xActual.Get(i), "event[%u] exp=%s act=%s", i, acE, acA);
	}

	ZENITH_ASSERT_EQ((u_int)xEngine.GetWinnerSide(), (u_int)ZM_SIDE_PLAYER);
	ZENITH_ASSERT_TRUE(ZM_ValidateEventStream(xActual, xEngine, "scenario10"));
}

// ============================================================================
// 11. Scenario_FirstDamageNumber_MatchesFormula -- turn-1 first DAMAGE_DEALT.
//     Seed 0x1234 draws crit=false (RandBelow(24)=23) then roll 88 -> 10 dmg;
//     Nibbin ATK 20 vs Strayling DEF 16, STAB, neutral.
// ============================================================================
ZENITH_TEST(ZM_Battle, Scenario_FirstDamageNumber_MatchesFormula)
{
	ZM_BattleMonsterSpec axPlayer[1] = { MakeScenarioNibbin() };
	ZM_BattleMonsterSpec axEnemy[1]  = { MakeScenarioStrayling() };

	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeTrainerConfig(), axPlayer, 1u, axEnemy, 1u, 0x1234ull, 54ull);
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
	xEngine.ResolveTurn();

	// The hand-plugged pipeline value for the first hit (non-crit, roll 88).
	const u_int uExpected = CalcDmg(10u, 45u, 20u, 16u, true, 100u, false, 88u);
	ZENITH_ASSERT_EQ(uExpected, 10u);

	// First DAMAGE_DEALT in the stream must equal that formula value.
	const Zenith_Vector<ZM_BattleEvent>& xEvents = xEngine.GetEvents();
	bool bFound = false;
	for (u_int i = 0; i < xEvents.GetSize(); ++i)
	{
		if (xEvents.Get(i).m_eKind == ZM_BATTLE_EVENT_DAMAGE_DEALT)
		{
			ZENITH_ASSERT_EQ((u_int)xEvents.Get(i).m_iAmount, uExpected, "first DAMAGE_DEALT != formula");
			ZENITH_ASSERT_EQ(xEvents.Get(i).m_uSide, (u_int)ZM_SIDE_ENEMY, "first hit should target the enemy");
			bFound = true;
			break;
		}
	}
	ZENITH_ASSERT_TRUE(bFound, "no DAMAGE_DEALT emitted in turn 1");
}

// ============================================================================
// 12. Scenario_SuperEffective_EmitsEvent -- FIRE move vs mono-GRASS defender
//     emits SUPER_EFFECTIVE(200) immediately before DAMAGE_DEALT.
// ============================================================================
ZENITH_TEST(ZM_Battle, Scenario_SuperEffective_EmitsEvent)
{
	// Player: Kindlet (FIRE) with Flarelash (FIRE special) -- fast, bulky, hits hard.
	ZM_BattleMonsterSpec axPlayer[1] = { MakeSpecOverride(ZM_SPECIES_KINDLET, 30u, ZM_MOVE_FLARELASH, 60u, 40u, 40u, 200u, 40u, 200u) };
	// Enemy: Fernfawn (mono GRASS), bulky so it survives the first hit.
	ZM_BattleMonsterSpec axEnemy[1]  = { MakeSpecOverride(ZM_SPECIES_FERNFAWN, 30u, ZM_MOVE_RAMBASH, 240u, 10u, 200u, 10u, 200u, 10u) };

	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeTrainerConfig(), axPlayer, 1u, axEnemy, 1u, 0x1234ull, 54ull);
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
	xEngine.ResolveTurn();

	const Zenith_Vector<ZM_BattleEvent>& xEvents = xEngine.GetEvents();
	bool bFound = false;
	for (u_int i = 0; i < xEvents.GetSize(); ++i)
	{
		if (xEvents.Get(i).m_eKind == ZM_BATTLE_EVENT_SUPER_EFFECTIVE)
		{
			ZENITH_ASSERT_EQ(xEvents.Get(i).m_iAmount, 200, "FIRE vs mono-GRASS percent should be 200");
			ZENITH_ASSERT_EQ(xEvents.Get(i).m_uSide, (u_int)ZM_SIDE_ENEMY);
			// Immediately followed by the DAMAGE_DEALT of the same defender.
			ZENITH_ASSERT_TRUE(i + 1u < xEvents.GetSize(), "SUPER_EFFECTIVE is the last event");
			if (i + 1u < xEvents.GetSize())
			{
				ZENITH_ASSERT_EQ((u_int)xEvents.Get(i + 1u).m_eKind, (u_int)ZM_BATTLE_EVENT_DAMAGE_DEALT);
				ZENITH_ASSERT_EQ(xEvents.Get(i + 1u).m_uSide, (u_int)ZM_SIDE_ENEMY);
			}
			bFound = true;
			break;
		}
	}
	ZENITH_ASSERT_TRUE(bFound, "no SUPER_EFFECTIVE emitted");
	ZENITH_ASSERT_TRUE(ZM_ValidateEventStream(xEvents, xEngine, "super"));
}

// ============================================================================
// 13. Scenario_Immune_EmitsImmune -- NORMAL move vs mono-PHANTOM defender emits
//     IMMUNE, no DAMAGE_DEALT for that defender, and leaves its HP untouched.
// ============================================================================
ZENITH_TEST(ZM_Battle, Scenario_Immune_EmitsImmune)
{
	// Player: Nibbin (NORMAL) Rambash -- fast so it acts first.
	ZM_BattleMonsterSpec axPlayer[1] = { MakeSpecOverride(ZM_SPECIES_NIBBIN, 30u, ZM_MOVE_RAMBASH, 200u, 200u, 200u, 10u, 200u, 200u) };
	// Enemy: Wispet (mono PHANTOM) with Hexbolt (PHANTOM) -- also immune back, so
	// neither deals damage this turn (keeps the assertion crisp).
	ZM_BattleMonsterSpec axEnemy[1]  = { MakeSpecOverride(ZM_SPECIES_WISPET, 30u, ZM_MOVE_HEXBOLT, 200u, 10u, 200u, 10u, 200u, 10u) };

	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeTrainerConfig(), axPlayer, 1u, axEnemy, 1u, 0x1234ull, 54ull);

	const u_int uEnemyMaxHP = xEngine.GetState().Side(ZM_SIDE_ENEMY).m_xParty.Get(0).m_auMaxStat[ZM_STAT_HP];

	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
	xEngine.ResolveTurn();

	const Zenith_Vector<ZM_BattleEvent>& xEvents = xEngine.GetEvents();
	bool bImmuneOnEnemy = false;
	bool bAnyDamageToEnemy = false;
	for (u_int i = 0; i < xEvents.GetSize(); ++i)
	{
		if (xEvents.Get(i).m_eKind == ZM_BATTLE_EVENT_IMMUNE && xEvents.Get(i).m_uSide == (u_int)ZM_SIDE_ENEMY)
		{
			bImmuneOnEnemy = true;
		}
		if (xEvents.Get(i).m_eKind == ZM_BATTLE_EVENT_DAMAGE_DEALT && xEvents.Get(i).m_uSide == (u_int)ZM_SIDE_ENEMY)
		{
			bAnyDamageToEnemy = true;
		}
	}
	ZENITH_ASSERT_TRUE(bImmuneOnEnemy, "NORMAL vs PHANTOM should emit IMMUNE for the enemy");
	ZENITH_ASSERT_FALSE(bAnyDamageToEnemy, "immune hit must not deal damage");

	// Enemy HP unchanged (still full).
	ZENITH_ASSERT_EQ(xEngine.GetState().Side(ZM_SIDE_ENEMY).m_xParty.Get(0).m_uCurHP, uEnemyMaxHP);
	ZENITH_ASSERT_TRUE(ZM_ValidateEventStream(xEvents, xEngine, "immune"));
}

// ============================================================================
// 14. Fuzz_Smoke_50Battles_Invariants -- 50 seeded 1v1 battles (mono-type
//     species, damaging FIRE move -> never immune -> guaranteed termination),
//     each capped at 500 turns; per battle assert termination and, throughout,
//     HP/PP/stage bounds, no fainted actor acts, and full stream well-formedness.
// ============================================================================
ZENITH_TEST(ZM_Battle, Fuzz_Smoke_50Battles_Invariants)
{
	// A spread of MONO-type species (types[1] == NONE) for effectiveness variety.
	static const ZM_SPECIES_ID aeMono[] = {
		ZM_SPECIES_NIBBIN,   ZM_SPECIES_STRAYLING, ZM_SPECIES_KINDLET,  ZM_SPECIES_SCORCHEL,
		ZM_SPECIES_FINLET,   ZM_SPECIES_MINNET,    ZM_SPECIES_FERNFAWN, ZM_SPECIES_THICKETBUCK,
		ZM_SPECIES_SPARKIT,  ZM_SPECIES_FRISKET,   ZM_SPECIES_SCRAPLING,ZM_SPECIES_RUBBLET,
		ZM_SPECIES_WISPET,   ZM_SPECIES_SHADELET,  ZM_SPECIES_SLAGLET,  ZM_SPECIES_FAYLING,
		ZM_SPECIES_TRANCET,  ZM_SPECIES_OOZEL,     ZM_SPECIES_BURRIT,   ZM_SPECIES_WYRMLING,
	};
	const u_int uMonoCount = sizeof(aeMono) / sizeof(aeMono[0]);
	const ZM_BattleConfig xCfg = MakeTrainerConfig();

	for (u_int uBattle = 0; uBattle < 50u; ++uBattle)
	{
		// Deterministic per-battle selection RNG (independent of the engine RNG).
		ZM_BattleRNG xSel(0x5EED0000ull + uBattle, 54ull);
		const ZM_SPECIES_ID ePlayer = aeMono[xSel.RandBelow(uMonoCount)];
		const ZM_SPECIES_ID eEnemy   = aeMono[xSel.RandBelow(uMonoCount)];
		const u_int uPlayerLvl = 10u + xSel.RandBelow(21u);   // 10..30 (bounds HP)
		const u_int uEnemyLvl  = 10u + xSel.RandBelow(21u);
		const u_int uPlayerSpe = 40u + xSel.RandBelow(60u);   // varied speed (incl. ties)
		const u_int uEnemySpe  = 40u + xSel.RandBelow(60u);

		// Glass-cannon SPECIAL profile: high SpA, low HP/SpDef -> fast KO, well
		// within PP. Flarelash (FIRE special, acc 100, no secondary) never hits an
		// immunity, so both sides always deal >=1 damage -> battle must terminate.
		ZM_BattleMonsterSpec axPlayer[1] = { MakeSpecOverride(ePlayer, uPlayerLvl, ZM_MOVE_FLARELASH, 25u, 40u, 40u, 130u, 30u, uPlayerSpe) };
		ZM_BattleMonsterSpec axEnemy[1]  = { MakeSpecOverride(eEnemy,  uEnemyLvl,  ZM_MOVE_FLARELASH, 25u, 40u, 40u, 130u, 30u, uEnemySpe) };
		// Fill all 4 slots with the same move so slot-cycling never runs a slot dry.
		for (u_int s = 1; s < uZM_MAX_MOVES; ++s)
		{
			axPlayer[0].m_aeMoves[s] = ZM_MOVE_FLARELASH;
			axEnemy[0].m_aeMoves[s]  = ZM_MOVE_FLARELASH;
		}

		ZM_BattleEngine xEngine;
		xEngine.Begin(xCfg, axPlayer, 1u, axEnemy, 1u, 0xB0000000ull + uBattle * 0x9E3779B1ull, 54ull);

		u_int uTurns = 0u;
		while (!xEngine.IsOver() && uTurns < 500u)
		{
			const u_int uSlot = uTurns % uZM_MAX_MOVES;
			// Defensive: never submit into a spent slot (would trip SubmitAction's
			// PP assert). With the glass-cannon this can't happen (<=2 turns), but
			// if it ever did, break and let the IsOver assert flag it cleanly.
			const ZM_BattleMonster& xActP = xEngine.GetState().Side(ZM_SIDE_PLAYER).m_xParty.Get(0);
			const ZM_BattleMonster& xActE = xEngine.GetState().Side(ZM_SIDE_ENEMY).m_xParty.Get(0);
			if (xActP.m_axMoves[uSlot].m_uCurPP == 0u || xActE.m_axMoves[uSlot].m_uCurPP == 0u) { break; }
			xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(uSlot));
			xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(uSlot));
			xEngine.ResolveTurn();
			++uTurns;

			// Per-turn state invariants on both active monsters.
			for (u_int uSide = 0; uSide < (u_int)ZM_SIDE_COUNT; ++uSide)
			{
				const ZM_BattleMonster& xMon = xEngine.GetState().Side((ZM_SIDE)uSide).m_xParty.Get(0);
				ZENITH_ASSERT_LE(xMon.m_uCurHP, xMon.m_auMaxStat[ZM_STAT_HP], "battle %u: HP > max", uBattle);
				for (u_int m = 0; m < uZM_MAX_MOVES; ++m)
				{
					ZENITH_ASSERT_LE(xMon.m_axMoves[m].m_uCurPP, xMon.m_axMoves[m].m_uMaxPP, "battle %u: PP > max", uBattle);
				}
				for (u_int st = 0; st < ZM_BATTLE_STAT_COUNT; ++st)
				{
					ZENITH_ASSERT_GE(xMon.m_aiStage[st], iZM_MIN_STAGE, "battle %u: stage < -6", uBattle);
					ZENITH_ASSERT_LE(xMon.m_aiStage[st], iZM_MAX_STAGE, "battle %u: stage > +6", uBattle);
				}
			}
		}

		ZENITH_ASSERT_LT(uTurns, 500u, "battle %u did not terminate in < 500 turns", uBattle);
		ZENITH_ASSERT_TRUE(xEngine.IsOver(), "battle %u not over after loop", uBattle);
		ZENITH_ASSERT_TRUE(ZM_ValidateEventStream(xEngine.GetEvents(), xEngine, "fuzz"),
			"battle %u produced a malformed event stream", uBattle);
	}
}

// ============================================================================
// S2 box-2 SC1 -- the ZM_MoveExecutor seam. These exercise ZM_MoveExecutor /
// ZM_MoveContext directly (a bare state + a context view), separate from the
// full engine turn loop. SC1 is a byte-identical extraction, so the box-1
// goldens above are untouched; these lock the seam's behaviour in isolation.
// ============================================================================
namespace
{
	// Populate a bare 1v1 state (both parties slot 0, seeded RNG) for driving the
	// executor without the engine. Out-param avoids relying on ZM_BattleState copy.
	void BuildBattleState(ZM_BattleState& xState, const ZM_BattleMonsterSpec& xPlayerSpec,
		const ZM_BattleMonsterSpec& xEnemySpec, u_int64 ulSeed, u_int64 ulSeq = 54ull)
	{
		ZM_BattleSide& xPlayer = xState.Side(ZM_SIDE_PLAYER);
		ZM_BattleSide& xEnemy  = xState.Side(ZM_SIDE_ENEMY);
		xPlayer.m_xParty.Clear();
		xEnemy.m_xParty.Clear();
		xPlayer.m_xParty.PushBack(ZM_BuildBattleMonster(xPlayerSpec));
		xEnemy.m_xParty.PushBack(ZM_BuildBattleMonster(xEnemySpec));
		xPlayer.m_uActiveSlot = 0u;
		xEnemy.m_uActiveSlot  = 0u;
		xState.m_xField = ZM_FieldState();
		xState.m_xRNG.Seed(ulSeed, ulSeq);
	}

	// A move context viewing xState + xEvents, attacker eAtk using move slot uSlot.
	ZM_MoveContext MakeCtx(ZM_BattleState& xState, Zenith_Vector<ZM_BattleEvent>& xEvents,
		ZM_SIDE eAtk, u_int uSlot)
	{
		ZM_MoveContext xCtx;
		xCtx.m_pxState   = &xState;
		xCtx.m_pxEvents  = &xEvents;
		xCtx.m_eAtk      = eAtk;
		xCtx.m_eDef      = (eAtk == ZM_SIDE_PLAYER) ? ZM_SIDE_ENEMY : ZM_SIDE_PLAYER;
		xCtx.m_uMoveSlot = uSlot;
		return xCtx;
	}
}

// ============================================================================
// SC1.1 Executor_RoutesNoneThroughDamagingHit -- a damaging effect-NONE move via
//   ZM_MoveExecutor::Execute emits the same MOVE_USED + DAMAGE_DEALT group the
//   engine produces (matches golden events [4]/[5]: seed 0x1234, Nibbin Rambash
//   vs Strayling -> non-crit roll 88 -> 10 dmg, Strayling 32 -> 22 HP remaining).
// ============================================================================
ZENITH_TEST(ZM_Battle, Executor_RoutesNoneThroughDamagingHit)
{
	ZM_BattleState xState;
	BuildBattleState(xState, MakeScenarioNibbin(), MakeScenarioStrayling(), 0x1234ull, 54ull);

	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);

	// Neutral, non-crit, no faint -> exactly MOVE_USED then DAMAGE_DEALT.
	ZENITH_ASSERT_EQ(xEvents.GetSize(), 2u, "NONE move should emit MOVE_USED + DAMAGE_DEALT");
	if (xEvents.GetSize() == 2u)
	{
		const ZM_BattleEvent xUsed = ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER, 0u, ZM_MOVE_RAMBASH);
		const ZM_BattleEvent xDmg  = ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_ENEMY, 0u,
			ZM_MOVE_NONE, ZM_SPECIES_NONE, 10, 22);
		char acE[192];
		char acA[192];
		ZM_BattleEventToString(xUsed, acE, sizeof(acE));
		ZM_BattleEventToString(xEvents.Get(0), acA, sizeof(acA));
		ZENITH_ASSERT_TRUE(xEvents.Get(0) == xUsed, "event[0] exp=%s act=%s", acE, acA);
		ZM_BattleEventToString(xDmg, acE, sizeof(acE));
		ZM_BattleEventToString(xEvents.Get(1), acA, sizeof(acA));
		ZENITH_ASSERT_TRUE(xEvents.Get(1) == xDmg, "event[1] exp=%s act=%s", acE, acA);
	}
}

// ============================================================================
// SC1.2 Executor_ApplyDamagingHit_MatchesCalcDmg -- the emitted DAMAGE_DEALT and
//   the returned damage both equal a direct ZM_CalcDamage over the same inputs
//   (crit/roll re-derived from a mirror RNG on the same seed/stream).
// ============================================================================
ZENITH_TEST(ZM_Battle, Executor_ApplyDamagingHit_MatchesCalcDmg)
{
	ZM_BattleState xState;
	BuildBattleState(xState, MakeScenarioNibbin(), MakeScenarioStrayling(), 0x1234ull, 54ull);

	// Mirror ApplyDamagingHit's draw order (crit Chance(1,24), then roll 85..100).
	ZM_BattleRNG xMirror(0x1234ull, 54ull);
	const bool  bCrit = xMirror.Chance(1u, 24u);
	const u_int uRoll = xMirror.RandRange(85u, 100u);

	// Rebuild the damage inputs exactly as ApplyDamagingHit does (stage 0 identity).
	const ZM_BattleMonster& xAtk = xState.Side(ZM_SIDE_PLAYER).Active();
	const ZM_BattleMonster& xDef = xState.Side(ZM_SIDE_ENEMY).Active();
	const ZM_MoveData& xMove = ZM_GetMoveData(xAtk.m_axMoves[0].m_eMove);
	const bool bPhysical = (xMove.m_eCategory == ZM_MOVE_CATEGORY_PHYSICAL);
	const ZM_SpeciesData& xAtkSpecies = ZM_GetSpeciesData(xAtk.m_eSpecies);
	const ZM_SpeciesData& xDefSpecies = ZM_GetSpeciesData(xDef.m_eSpecies);

	ZM_DamageInput xIn;
	xIn.uLevel  = xAtk.m_uLevel;
	xIn.uPower  = xMove.m_uPower;
	xIn.uAttack = ZM_ApplyStatStage(
		bPhysical ? xAtk.m_auMaxStat[ZM_STAT_ATTACK] : xAtk.m_auMaxStat[ZM_STAT_SPATTACK],
		bPhysical ? xAtk.m_aiStage[ZM_BATTLE_STAT_ATTACK] : xAtk.m_aiStage[ZM_BATTLE_STAT_SPATTACK]);
	xIn.uDefense = ZM_ApplyStatStage(
		bPhysical ? xDef.m_auMaxStat[ZM_STAT_DEFENSE] : xDef.m_auMaxStat[ZM_STAT_SPDEFENSE],
		bPhysical ? xDef.m_aiStage[ZM_BATTLE_STAT_DEFENSE] : xDef.m_aiStage[ZM_BATTLE_STAT_SPDEFENSE]);
	xIn.bStab = (xMove.m_eType == xAtkSpecies.m_aeTypes[0] || xMove.m_eType == xAtkSpecies.m_aeTypes[1]);
	xIn.uEffectivenessPercent = ZM_EffectivenessPercent(xMove.m_eType, xDefSpecies.m_aeTypes[0], xDefSpecies.m_aeTypes[1]);
	xIn.bCrit = bCrit;
	xIn.uRandomPercent = uRoll;
	const u_int uExpectedDmg = ZM_CalcDamage(xIn);

	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	const u_int uReturned = ZM_MoveExecutor::ApplyDamagingHit(xCtx);

	ZENITH_ASSERT_EQ(uReturned, uExpectedDmg, "ApplyDamagingHit return != direct ZM_CalcDamage");

	bool bFound = false;
	for (u_int i = 0; i < xEvents.GetSize(); ++i)
	{
		if (xEvents.Get(i).m_eKind == ZM_BATTLE_EVENT_DAMAGE_DEALT)
		{
			ZENITH_ASSERT_EQ((u_int)xEvents.Get(i).m_iAmount, uExpectedDmg, "emitted DAMAGE_DEALT amount != direct ZM_CalcDamage");
			bFound = true;
			break;
		}
	}
	ZENITH_ASSERT_TRUE(bFound, "ApplyDamagingHit emitted no DAMAGE_DEALT");
}

// ============================================================================
// SC1.3 Executor_MaybeFaint_EmitsFaintAtZero -- MaybeFaint emits exactly one
//   FAINT(side=Def) when the defender's active HP is 0, and nothing when HP > 0.
// ============================================================================
ZENITH_TEST(ZM_Battle, Executor_MaybeFaint_EmitsFaintAtZero)
{
	ZM_BattleState xState;
	BuildBattleState(xState, MakeScenarioNibbin(), MakeScenarioStrayling(), 0x1ull, 54ull);

	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);

	// HP > 0 -> no FAINT.
	xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP = 5u;
	xCtx.MaybeFaint(ZM_SIDE_ENEMY);
	ZENITH_ASSERT_EQ(xEvents.GetSize(), 0u, "MaybeFaint must not emit while HP > 0");

	// HP == 0 -> exactly one FAINT(enemy, slot 0).
	xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP = 0u;
	xCtx.MaybeFaint(ZM_SIDE_ENEMY);
	ZENITH_ASSERT_EQ(xEvents.GetSize(), 1u, "MaybeFaint must emit exactly one FAINT at 0 HP");
	if (xEvents.GetSize() == 1u)
	{
		ZENITH_ASSERT_TRUE(xEvents.Get(0) == ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, ZM_SIDE_ENEMY, 0u),
			"FAINT should target the defender side/slot");
	}
}

// ============================================================================
// SC1.4 Executor_Context_AccessorsResolve -- Atk/Def/AtkSide/DefSide/Move/RNG
//   resolve to the right monster/side/move/rng for both attacker roles.
// ============================================================================
ZENITH_TEST(ZM_Battle, Executor_Context_AccessorsResolve)
{
	ZM_BattleState xState;
	// Distinct moves per side so Move() is unambiguous by attacker role.
	BuildBattleState(xState, MakeSpec(ZM_SPECIES_NIBBIN, 10u, ZM_MOVE_RAMBASH),
		MakeSpec(ZM_SPECIES_STRAYLING, 10u, ZM_MOVE_QUICKJAB), 0x99ull, 54ull);

	Zenith_Vector<ZM_BattleEvent> xEvents;

	// Player attacks: Atk == Nibbin, Def == Strayling, sides map straight, move Rambash.
	ZM_MoveContext xP = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZENITH_ASSERT_EQ((u_int)xP.Atk().m_eSpecies, (u_int)ZM_SPECIES_NIBBIN);
	ZENITH_ASSERT_EQ((u_int)xP.Def().m_eSpecies, (u_int)ZM_SPECIES_STRAYLING);
	ZENITH_ASSERT_TRUE(&xP.AtkSide() == &xState.Side(ZM_SIDE_PLAYER), "AtkSide should be the player side");
	ZENITH_ASSERT_TRUE(&xP.DefSide() == &xState.Side(ZM_SIDE_ENEMY), "DefSide should be the enemy side");
	ZENITH_ASSERT_EQ((u_int)xP.Move().m_eId, (u_int)ZM_MOVE_RAMBASH);
	ZENITH_ASSERT_TRUE(&xP.RNG() == &xState.m_xRNG, "RNG should alias the state RNG");

	// Enemy attacks: roles swap; move Quickjab.
	ZM_MoveContext xE = MakeCtx(xState, xEvents, ZM_SIDE_ENEMY, 0u);
	ZENITH_ASSERT_EQ((u_int)xE.Atk().m_eSpecies, (u_int)ZM_SPECIES_STRAYLING);
	ZENITH_ASSERT_EQ((u_int)xE.Def().m_eSpecies, (u_int)ZM_SPECIES_NIBBIN);
	ZENITH_ASSERT_TRUE(&xE.AtkSide() == &xState.Side(ZM_SIDE_ENEMY), "AtkSide should be the enemy side");
	ZENITH_ASSERT_TRUE(&xE.DefSide() == &xState.Side(ZM_SIDE_PLAYER), "DefSide should be the player side");
	ZENITH_ASSERT_EQ((u_int)xE.Move().m_eId, (u_int)ZM_MOVE_QUICKJAB);
	ZENITH_ASSERT_TRUE(&xE.RNG() == &xState.m_xRNG, "RNG should alias the state RNG");
}

// ============================================================================
// SC1.5 Executor_AccuracyStageZeroIsIdentity -- with acc/eva stages both 0 the
//   folded accuracy math is identity: the stage-0 executor hits/misses on exactly
//   the same RandBelow(100) boundary as the raw base-accuracy check. Proven on a
//   move that can miss (Torrent Cannon, acc 80), for both a hit seed and a miss
//   seed, using a mirror RNG (same seed/stream) to predict the base decision.
// ============================================================================
ZENITH_TEST(ZM_Battle, Executor_AccuracyStageZeroIsIdentity)
{
	const ZM_MOVE_ID eMove = ZM_MOVE_TORRENTCANNON;   // damaging, effect NONE, accuracy 80
	const u_int uAcc = ZM_GetMoveData(eMove).m_uAccuracy;
	ZENITH_ASSERT_TRUE(uAcc != uZM_MOVE_ACCURACY_ALWAYS_HITS && uAcc < 100u, "test needs a move that can miss");

	// The fold at net stage 0 is the identity.
	ZENITH_ASSERT_EQ(ZM_ApplyStatStage(uAcc, 0), uAcc);

	// Attacker holds the can-miss move; bulky defender so a connecting hit never
	// faints -- the accuracy check is then the sole possible MOVE_MISSED source.
	const ZM_BattleMonsterSpec xPlayer = MakeSpecOverride(ZM_SPECIES_NIBBIN,    20u, eMove,           200u, 40u,  40u, 40u,  40u, 100u);
	const ZM_BattleMonsterSpec xEnemy  = MakeSpecOverride(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH, 240u, 40u, 200u, 40u, 200u,  10u);

	bool bTestedHit  = false;
	bool bTestedMiss = false;
	for (u_int64 ulSeed = 1ull; ulSeed <= 256ull && !(bTestedHit && bTestedMiss); ++ulSeed)
	{
		// Mirror draws the executor's FIRST draw (RandBelow(100)) to predict the
		// base-accuracy decision on this seed/stream.
		ZM_BattleRNG xMirror(ulSeed, 54ull);
		const bool bBaseMiss = (xMirror.RandBelow(100u) >= uAcc);
		if (bBaseMiss ? bTestedMiss : bTestedHit) { continue; }   // one of each is enough

		ZM_BattleState xState;
		BuildBattleState(xState, xPlayer, xEnemy, ulSeed, 54ull);
		// Precondition of the identity: attacker accuracy + defender evasion stages 0.
		ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[ZM_BATTLE_STAT_ACCURACY], 0);
		ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_ENEMY).Active().m_aiStage[ZM_BATTLE_STAT_EVASION], 0);

		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
		ZM_MoveExecutor::Execute(xCtx);

		bool bExecMiss = false;
		for (u_int i = 0; i < xEvents.GetSize(); ++i)
		{
			if (xEvents.Get(i).m_eKind == ZM_BATTLE_EVENT_MOVE_MISSED) { bExecMiss = true; break; }
		}
		ZENITH_ASSERT_TRUE(bExecMiss == bBaseMiss, "stage-0 executor accuracy decision must equal the base-accuracy check");

		if (bBaseMiss) { bTestedMiss = true; } else { bTestedHit = true; }
	}
	ZENITH_ASSERT_TRUE(bTestedHit,  "no HIT seed found -- hit path not exercised");
	ZENITH_ASSERT_TRUE(bTestedMiss, "no MISS seed found -- miss path not exercised");
}
