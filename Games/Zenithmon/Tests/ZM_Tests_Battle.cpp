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

// ============================================================================
// S2 box-2 SC2 -- stat-stage effects + STATUS-category dispatch (DecisionLog
// ZM-D-033 PHASE M/E). Each golden/number is cross-checked by the offline oracle
// (scratchpad/zm_battle_ref.py), whose box-1 27-event stream re-derives byte-
// identical after the SC2 augmented-draw-order port (the oracle's own gate).
//
// SHARED EVENT CONTRACT (the executor emits these exact encodings):
//  * STAT_STAGE_CHANGED: m_uSide/m_uSlot = the TARGET; m_iAmount = signed stage
//    delta actually applied (new-old); m_iAux = (int) the ZM_BATTLE_STAT.
//  * MOVE_FAILED: m_iAux = a ZM_MOVE_FAIL_REASON (SC2 uses ZM_MOVE_FAIL_STAT_MAXED).
//  * A STATUS-category primary applies the stat effect with NO crit/roll/proc
//    draw (after PP + MOVE_USED, + accuracy iff the move can miss). A damaging
//    move's stat SECONDARY runs ApplyDamagingHit, then draws RandBelow(100) <
//    m_uEffectChance (chance>=100 => no draw) to gate the effect.
// ============================================================================
namespace
{
	// First event of a given kind, or nullptr.
	const ZM_BattleEvent* ZM_FindKind(const Zenith_Vector<ZM_BattleEvent>& xEvents, ZM_BATTLE_EVENT eKind)
	{
		for (u_int i = 0; i < xEvents.GetSize(); ++i)
		{
			if (xEvents.Get(i).m_eKind == eKind) { return &xEvents.Get(i); }
		}
		return nullptr;
	}

	bool ZM_HasKind(const Zenith_Vector<ZM_BattleEvent>& xEvents, ZM_BATTLE_EVENT eKind)
	{
		return ZM_FindKind(xEvents, eKind) != nullptr;
	}

	// Find a STAT_STAGE_CHANGED for (side, battle-stat). Returns the applied delta.
	bool ZM_FindStageChange(const Zenith_Vector<ZM_BattleEvent>& xEvents, ZM_SIDE eSide,
		ZM_BATTLE_STAT eStat, int& iAmountOut)
	{
		for (u_int i = 0; i < xEvents.GetSize(); ++i)
		{
			const ZM_BattleEvent& x = xEvents.Get(i);
			if (x.m_eKind == ZM_BATTLE_EVENT_STAT_STAGE_CHANGED &&
				x.m_uSide == (u_int)eSide && (u_int)x.m_iAux == (u_int)eStat)
			{
				iAmountOut = x.m_iAmount;
				return true;
			}
		}
		return false;
	}

	// Count STAT_STAGE_CHANGED events targeting a side (order-independent checks).
	u_int ZM_CountStageChanges(const Zenith_Vector<ZM_BattleEvent>& xEvents, ZM_SIDE eSide)
	{
		u_int uCount = 0u;
		for (u_int i = 0; i < xEvents.GetSize(); ++i)
		{
			const ZM_BattleEvent& x = xEvents.Get(i);
			if (x.m_eKind == ZM_BATTLE_EVENT_STAT_STAGE_CHANGED && x.m_uSide == (u_int)eSide) { ++uCount; }
		}
		return uCount;
	}

	// Execute one move (slot 0) from the player over a bare 1v1 state (no engine
	// turn loop): isolates the executor's effect dispatch. State + events are the
	// caller's so post-conditions (stages/HP/RNG) can be inspected.
	void ZM_RunPlayerMove(const ZM_BattleMonsterSpec& xPlayer, const ZM_BattleMonsterSpec& xEnemy,
		u_int64 ulSeed, ZM_BattleState& xStateOut, Zenith_Vector<ZM_BattleEvent>& xEventsOut)
	{
		BuildBattleState(xStateOut, xPlayer, xEnemy, ulSeed, 54ull);
		ZM_MoveContext xCtx = MakeCtx(xStateOut, xEventsOut, ZM_SIDE_PLAYER, 0u);
		ZM_MoveExecutor::Execute(xCtx);
	}

	// Exact element-by-element stream compare (dumps both on drift).
	void ZM_AssertStreamEquals(const Zenith_Vector<ZM_BattleEvent>& xExpected,
		const Zenith_Vector<ZM_BattleEvent>& xActual, const char* szLabel)
	{
		bool bMatch = (xExpected.GetSize() == xActual.GetSize());
		const u_int uMin = (xExpected.GetSize() < xActual.GetSize()) ? xExpected.GetSize() : xActual.GetSize();
		for (u_int i = 0; i < uMin; ++i)
		{
			if (!(xExpected.Get(i) == xActual.Get(i))) { bMatch = false; }
		}
		if (!bMatch) { ZM_LogTwoStreams(xExpected, xActual); }
		ZENITH_ASSERT_EQ(xActual.GetSize(), xExpected.GetSize(), "%s: stream length mismatch", szLabel);
		for (u_int i = 0; i < uMin; ++i)
		{
			char acE[192];
			char acA[192];
			ZM_BattleEventToString(xExpected.Get(i), acE, sizeof(acE));
			ZM_BattleEventToString(xActual.Get(i), acA, sizeof(acA));
			ZENITH_ASSERT_TRUE(xExpected.Get(i) == xActual.Get(i), "%s event[%u] exp=%s act=%s", szLabel, i, acE, acA);
		}
	}

	// One apply-check for a single-stat STATUS move: execute it, assert the target
	// stage moved by the signed magnitude and STAT_STAGE_CHANGED carried that delta.
	// iSign = -1 for a LOWER row (target = enemy) / +1 for a RAISE row (target = self).
	void ZM_CheckSingleStatApply(ZM_MOVE_ID eMove, ZM_BATTLE_STAT eStat, int iSign, ZM_SIDE eTargetSide)
	{
		const int iMag = ZM_GetMoveData(eMove).m_iEffectMagnitude;   // stage count (1..3)
		const int iExpected = iSign * iMag;

		ZM_BattleState xState;
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_RunPlayerMove(MakeSpec(ZM_SPECIES_NIBBIN, 20u, eMove),
			MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, xState, xEvents);

		int iAmount = 0;
		const bool bFound = ZM_FindStageChange(xEvents, eTargetSide, eStat, iAmount);
		ZENITH_ASSERT_TRUE(bFound, "move %u should emit STAT_STAGE_CHANGED for stat %u", (u_int)eMove, (u_int)eStat);
		ZENITH_ASSERT_EQ(iAmount, iExpected, "STAT_STAGE_CHANGED delta must equal sign*magnitude");
		ZENITH_ASSERT_EQ(xState.Side(eTargetSide).Active().m_aiStage[eStat], iExpected, "resulting stage != sign*magnitude");
		// Exactly one stat changed on the target for a single-stat move.
		ZENITH_ASSERT_EQ(ZM_CountStageChanges(xEvents, eTargetSide), 1u, "single-stat move changed more than one stat");
	}
}

// ---- Per-stat apply: the 7 LOWER_* kinds (target = enemy) --------------------
ZENITH_TEST(ZM_Battle, Stat_LowerAttack_Warcry)
{
	ZM_CheckSingleStatApply(ZM_MOVE_WARCRY, ZM_BATTLE_STAT_ATTACK, -1, ZM_SIDE_ENEMY);
}
ZENITH_TEST(ZM_Battle, Stat_LowerSpAttack_IonVeil)
{
	ZM_CheckSingleStatApply(ZM_MOVE_IONVEIL, ZM_BATTLE_STAT_SPATTACK, -1, ZM_SIDE_ENEMY);
}
ZENITH_TEST(ZM_Battle, Stat_LowerSpDefense_DreadGaze)
{
	ZM_CheckSingleStatApply(ZM_MOVE_DREADGAZE, ZM_BATTLE_STAT_SPDEFENSE, -1, ZM_SIDE_ENEMY);
}
ZENITH_TEST(ZM_Battle, Stat_LowerSpeed_CowerGlare)
{
	ZM_CheckSingleStatApply(ZM_MOVE_COWERGLARE, ZM_BATTLE_STAT_SPEED, -1, ZM_SIDE_ENEMY);
}
ZENITH_TEST(ZM_Battle, Stat_LowerAccuracy_Ashveil)
{
	ZM_CheckSingleStatApply(ZM_MOVE_ASHVEIL, ZM_BATTLE_STAT_ACCURACY, -1, ZM_SIDE_ENEMY);
}
ZENITH_TEST(ZM_Battle, Stat_LowerEvasion_Clearvision)
{
	ZM_CheckSingleStatApply(ZM_MOVE_CLEARVISION, ZM_BATTLE_STAT_EVASION, -1, ZM_SIDE_ENEMY);
}

// LOWER_DEFENSE has NO STATUS-category carrier in the move table, so its only
// vehicle is a damaging move's chance-100 secondary (Close Bout). A chance>=100
// secondary applies unconditionally after the hit; a very bulky defender survives
// so the drop is observable.
ZENITH_TEST(ZM_Battle, Stat_LowerDefense_CloseBout)
{
	const ZM_BattleMonsterSpec xPlayer = MakeSpecOverride(ZM_SPECIES_NIBBIN,    20u, ZM_MOVE_CLOSEBOUT, 80u, 40u,  40u, 40u,  40u, 60u);
	const ZM_BattleMonsterSpec xEnemy  = MakeSpecOverride(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH,  250u, 40u, 250u, 40u, 250u, 10u);

	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunPlayerMove(xPlayer, xEnemy, 0x1234ull, xState, xEvents);

	ZENITH_ASSERT_GT(xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP, 0u, "defender must survive to observe the secondary");
	const int iMag = ZM_GetMoveData(ZM_MOVE_CLOSEBOUT).m_iEffectMagnitude;
	int iAmount = 0;
	const bool bFound = ZM_FindStageChange(xEvents, ZM_SIDE_ENEMY, ZM_BATTLE_STAT_DEFENSE, iAmount);
	ZENITH_ASSERT_TRUE(bFound, "Close Bout's chance-100 secondary should lower enemy DEFENSE");
	ZENITH_ASSERT_EQ(iAmount, -iMag);
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_ENEMY).Active().m_aiStage[ZM_BATTLE_STAT_DEFENSE], -iMag);
	// The damaging hit landed first: DAMAGE_DEALT precedes STAT_STAGE_CHANGED.
	ZENITH_ASSERT_TRUE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT), "Close Bout is a damaging move");
}

// ---- Per-stat apply: single-stat RAISE_* kinds (target = self) ---------------
ZENITH_TEST(ZM_Battle, Stat_RaiseAttack_Whetclaw)
{
	ZM_CheckSingleStatApply(ZM_MOVE_WHETCLAW, ZM_BATTLE_STAT_ATTACK, +1, ZM_SIDE_PLAYER);
}
ZENITH_TEST(ZM_Battle, Stat_RaiseDefense_FirmUp)
{
	ZM_CheckSingleStatApply(ZM_MOVE_FIRMUP, ZM_BATTLE_STAT_DEFENSE, +1, ZM_SIDE_PLAYER);
}
ZENITH_TEST(ZM_Battle, Stat_RaiseSpAttack_KindleUp)
{
	ZM_CheckSingleStatApply(ZM_MOVE_KINDLEUP, ZM_BATTLE_STAT_SPATTACK, +1, ZM_SIDE_PLAYER);
}
ZENITH_TEST(ZM_Battle, Stat_RaiseSpDefense_MistVeil)
{
	ZM_CheckSingleStatApply(ZM_MOVE_MISTVEIL, ZM_BATTLE_STAT_SPDEFENSE, +1, ZM_SIDE_PLAYER);
}
ZENITH_TEST(ZM_Battle, Stat_RaiseSpeed_Quicken)
{
	ZM_CheckSingleStatApply(ZM_MOVE_QUICKEN, ZM_BATTLE_STAT_SPEED, +1, ZM_SIDE_PLAYER);
}
ZENITH_TEST(ZM_Battle, Stat_RaiseEvasion_Foresight)
{
	ZM_CheckSingleStatApply(ZM_MOVE_FORESIGHT, ZM_BATTLE_STAT_EVASION, +1, ZM_SIDE_PLAYER);
}

// ---- Multi-stat RAISE_* + RAISE_ALL (target = self) --------------------------
// Emits one STAT_STAGE_CHANGED per stat. Assertions are order-INDEPENDENT (final
// stages + one change per listed stat) so the executor's chosen emit order is
// a free implementation detail; the oracle derives ZM_BATTLE_STAT enum order.
namespace
{
	void ZM_CheckMultiStatRaise(ZM_MOVE_ID eMove, const ZM_BATTLE_STAT* peStats, u_int uStatCount)
	{
		const int iMag = ZM_GetMoveData(eMove).m_iEffectMagnitude;

		ZM_BattleState xState;
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_RunPlayerMove(MakeSpec(ZM_SPECIES_NIBBIN, 20u, eMove),
			MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, xState, xEvents);

		// Exactly one STAT_STAGE_CHANGED per listed stat, each +magnitude; every
		// other battle-stat slot on the user stays 0.
		ZENITH_ASSERT_EQ(ZM_CountStageChanges(xEvents, ZM_SIDE_PLAYER), uStatCount, "wrong number of stat changes");
		for (u_int uStat = 0; uStat < (u_int)ZM_BATTLE_STAT_COUNT; ++uStat)
		{
			bool bListed = false;
			for (u_int j = 0; j < uStatCount; ++j) { if ((u_int)peStats[j] == uStat) { bListed = true; } }
			int iAmount = 0;
			const bool bChanged = ZM_FindStageChange(xEvents, ZM_SIDE_PLAYER, (ZM_BATTLE_STAT)uStat, iAmount);
			if (bListed)
			{
				ZENITH_ASSERT_TRUE(bChanged, "listed stat %u should change", uStat);
				ZENITH_ASSERT_EQ(iAmount, iMag, "listed stat delta != magnitude");
				ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[uStat], iMag);
			}
			else
			{
				ZENITH_ASSERT_FALSE(bChanged, "unlisted stat %u must not change", uStat);
				ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[uStat], 0, "unlisted stat %u nonzero", uStat);
			}
		}
	}
}

ZENITH_TEST(ZM_Battle, Stat_RaiseAttackSpeed_Wyrmdance)
{
	const ZM_BATTLE_STAT aeStats[] = { ZM_BATTLE_STAT_ATTACK, ZM_BATTLE_STAT_SPEED };
	ZM_CheckMultiStatRaise(ZM_MOVE_WYRMDANCE, aeStats, 2u);
}
ZENITH_TEST(ZM_Battle, Stat_RaiseAttackDefense_BulkStance)
{
	const ZM_BATTLE_STAT aeStats[] = { ZM_BATTLE_STAT_ATTACK, ZM_BATTLE_STAT_DEFENSE };
	ZM_CheckMultiStatRaise(ZM_MOVE_BULKSTANCE, aeStats, 2u);
}
ZENITH_TEST(ZM_Battle, Stat_RaiseSpatkSpdef_SereneMind)
{
	const ZM_BATTLE_STAT aeStats[] = { ZM_BATTLE_STAT_SPATTACK, ZM_BATTLE_STAT_SPDEFENSE };
	ZM_CheckMultiStatRaise(ZM_MOVE_SERENEMIND, aeStats, 2u);
}
ZENITH_TEST(ZM_Battle, Stat_RaiseDefSpdef_Bastion)
{
	const ZM_BATTLE_STAT aeStats[] = { ZM_BATTLE_STAT_DEFENSE, ZM_BATTLE_STAT_SPDEFENSE };
	ZM_CheckMultiStatRaise(ZM_MOVE_BASTION, aeStats, 2u);
}
ZENITH_TEST(ZM_Battle, Stat_RaiseAll_RallyCry)
{
	// RAISE_ALL == the 5 battle stats (NOT accuracy/evasion).
	const ZM_BATTLE_STAT aeStats[] = {
		ZM_BATTLE_STAT_ATTACK, ZM_BATTLE_STAT_DEFENSE, ZM_BATTLE_STAT_SPATTACK,
		ZM_BATTLE_STAT_SPDEFENSE, ZM_BATTLE_STAT_SPEED };
	ZM_CheckMultiStatRaise(ZM_MOVE_RALLYCRY, aeStats, 5u);
}

// ============================================================================
// Clamp to [-6,+6]: a fully-maxed stat -> MOVE_FAILED(STAT_MAXED), stage frozen.
// ============================================================================
ZENITH_TEST(ZM_Battle, Stat_RaiseClampsAtPlus6)
{
	ZM_BattleState xState;
	BuildBattleState(xState, MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_WHETCLAW),
		MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, 54ull);

	// Raise ATTACK repeatedly until pinned at +6 (magnitude-agnostic).
	int iGuard = 0;
	while (xState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[ZM_BATTLE_STAT_ATTACK] < iZM_MAX_STAGE && iGuard < 12)
	{
		Zenith_Vector<ZM_BattleEvent> xStep;
		ZM_MoveContext xCtx = MakeCtx(xState, xStep, ZM_SIDE_PLAYER, 0u);
		ZM_MoveExecutor::Execute(xCtx);
		++iGuard;
	}
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[ZM_BATTLE_STAT_ATTACK], iZM_MAX_STAGE, "should reach +6");

	// One more use at +6: MOVE_FAILED(STAT_MAXED), no STAT_STAGE_CHANGED, stage held.
	Zenith_Vector<ZM_BattleEvent> xMaxed;
	ZM_MoveContext xCtx = MakeCtx(xState, xMaxed, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);

	const ZM_BattleEvent* pxFail = ZM_FindKind(xMaxed, ZM_BATTLE_EVENT_MOVE_FAILED);
	ZENITH_ASSERT_NOT_NULL(pxFail, "a maxed raise must emit MOVE_FAILED");
	if (pxFail != nullptr)
	{
		ZENITH_ASSERT_EQ((u_int)pxFail->m_iAux, (u_int)ZM_MOVE_FAIL_STAT_MAXED, "MOVE_FAILED reason must be STAT_MAXED");
	}
	ZENITH_ASSERT_FALSE(ZM_HasKind(xMaxed, ZM_BATTLE_EVENT_STAT_STAGE_CHANGED), "no STAT_STAGE_CHANGED when already maxed");
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[ZM_BATTLE_STAT_ATTACK], iZM_MAX_STAGE, "stage must stay +6");
}

ZENITH_TEST(ZM_Battle, Stat_LowerClampsAtMinus6)
{
	ZM_BattleState xState;
	BuildBattleState(xState, MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_COWERGLARE),
		MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, 54ull);

	// Cower Glare lowers enemy SPEED; drive it to -6 (magnitude-agnostic).
	int iGuard = 0;
	while (xState.Side(ZM_SIDE_ENEMY).Active().m_aiStage[ZM_BATTLE_STAT_SPEED] > iZM_MIN_STAGE && iGuard < 12)
	{
		Zenith_Vector<ZM_BattleEvent> xStep;
		ZM_MoveContext xCtx = MakeCtx(xState, xStep, ZM_SIDE_PLAYER, 0u);
		ZM_MoveExecutor::Execute(xCtx);
		++iGuard;
	}
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_ENEMY).Active().m_aiStage[ZM_BATTLE_STAT_SPEED], iZM_MIN_STAGE, "should reach -6");

	Zenith_Vector<ZM_BattleEvent> xMaxed;
	ZM_MoveContext xCtx = MakeCtx(xState, xMaxed, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);

	const ZM_BattleEvent* pxFail = ZM_FindKind(xMaxed, ZM_BATTLE_EVENT_MOVE_FAILED);
	ZENITH_ASSERT_NOT_NULL(pxFail, "a bottomed-out lower must emit MOVE_FAILED");
	if (pxFail != nullptr)
	{
		ZENITH_ASSERT_EQ((u_int)pxFail->m_iAux, (u_int)ZM_MOVE_FAIL_STAT_MAXED, "MOVE_FAILED reason must be STAT_MAXED");
	}
	ZENITH_ASSERT_FALSE(ZM_HasKind(xMaxed, ZM_BATTLE_EVENT_STAT_STAGE_CHANGED), "no STAT_STAGE_CHANGED when already at -6");
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_ENEMY).Active().m_aiStage[ZM_BATTLE_STAT_SPEED], iZM_MIN_STAGE, "stage must stay -6");
}

// STAT_STAGE_CHANGED m_iAmount is the delta ACTUALLY applied (new-old), so a
// partial clamp reports the truncated delta, not the nominal magnitude.
ZENITH_TEST(ZM_Battle, Stat_PartialClampDeltaApplied)
{
	ZM_BattleState xState;
	// Frostward is a mag-2 RAISE_DEFENSE (self). Pre-rig DEFENSE to +5 so a +2 use
	// can only apply +1 before pinning at +6.
	BuildBattleState(xState, MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_FROSTWARD),
		MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, 54ull);
	ZENITH_ASSERT_EQ(ZM_GetMoveData(ZM_MOVE_FROSTWARD).m_iEffectMagnitude, 2, "test needs a mag-2 raise");
	xState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[ZM_BATTLE_STAT_DEFENSE] = 5;

	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);

	int iAmount = 0;
	const bool bFound = ZM_FindStageChange(xEvents, ZM_SIDE_PLAYER, ZM_BATTLE_STAT_DEFENSE, iAmount);
	ZENITH_ASSERT_TRUE(bFound, "a partial raise still emits STAT_STAGE_CHANGED");
	ZENITH_ASSERT_EQ(iAmount, 1, "applied delta at +5 with a +2 raise must be +1");
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[ZM_BATTLE_STAT_DEFENSE], iZM_MAX_STAGE);
	ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_FAILED), "a partial (nonzero) raise must not fail");
}

// ============================================================================
// Target side: a RAISE hits SELF (attacker); a LOWER hits the OPPONENT.
// ============================================================================
ZENITH_TEST(ZM_Battle, Stat_RaiseTargetsSelfNotOpponent)
{
	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunPlayerMove(MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_WHETCLAW),
		MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, xState, xEvents);

	const ZM_BattleEvent* pxChange = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_STAT_STAGE_CHANGED);
	ZENITH_ASSERT_NOT_NULL(pxChange, "Whetclaw should raise a stat");
	if (pxChange != nullptr)
	{
		ZENITH_ASSERT_EQ(pxChange->m_uSide, (u_int)ZM_SIDE_PLAYER, "a self-raise targets the attacker");
		ZENITH_ASSERT_EQ(pxChange->m_uSlot, 0u);
	}
	// The opponent's stages are untouched.
	for (u_int i = 0; i < (u_int)ZM_BATTLE_STAT_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_ENEMY).Active().m_aiStage[i], 0, "opponent stage %u changed by a self-raise", i);
	}
}

ZENITH_TEST(ZM_Battle, Stat_LowerTargetsOpponentNotSelf)
{
	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunPlayerMove(MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_WARCRY),
		MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, xState, xEvents);

	const ZM_BattleEvent* pxChange = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_STAT_STAGE_CHANGED);
	ZENITH_ASSERT_NOT_NULL(pxChange, "Warcry should lower a stat");
	if (pxChange != nullptr)
	{
		ZENITH_ASSERT_EQ(pxChange->m_uSide, (u_int)ZM_SIDE_ENEMY, "an opponent-lower targets the defender");
	}
	for (u_int i = 0; i < (u_int)ZM_BATTLE_STAT_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[i], 0, "attacker stage %u changed by an opponent-lower", i);
	}
}

// The full STAT_STAGE_CHANGED encoding: side/slot = target, amount = delta, aux =
// (int) the ZM_BATTLE_STAT. Pinned on Warcry (LOWER_ATTACK, mag 1) into a stage-0
// enemy so amount == -1 and aux == ATTACK.
ZENITH_TEST(ZM_Battle, StatStageChanged_EncodingFields)
{
	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunPlayerMove(MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_WARCRY),
		MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, xState, xEvents);

	const ZM_BattleEvent* pxChange = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_STAT_STAGE_CHANGED);
	ZENITH_ASSERT_NOT_NULL(pxChange, "expected a STAT_STAGE_CHANGED");
	if (pxChange != nullptr)
	{
		ZENITH_ASSERT_EQ(pxChange->m_uSide, (u_int)ZM_SIDE_ENEMY, "side = target");
		ZENITH_ASSERT_EQ(pxChange->m_uSlot, 0u, "slot = target active slot");
		ZENITH_ASSERT_EQ(pxChange->m_iAmount, -1, "amount = signed delta applied");
		ZENITH_ASSERT_EQ((u_int)pxChange->m_iAux, (u_int)ZM_BATTLE_STAT_ATTACK, "aux = (int) the ZM_BATTLE_STAT");
	}
}

// A maxed STATUS primary still spends the turn: MOVE_USED is emitted BEFORE the
// MOVE_FAILED (the fail is discovered during effect application, after M1).
ZENITH_TEST(ZM_Battle, MoveFailed_MaxedEmitsMoveUsedThenFailed)
{
	ZM_BattleState xState;
	BuildBattleState(xState, MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_WHETCLAW),
		MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, 54ull);
	// Pre-pin ATTACK at +6 so the very next use fails.
	xState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[ZM_BATTLE_STAT_ATTACK] = iZM_MAX_STAGE;

	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);

	// Find MOVE_USED and MOVE_FAILED indices; MOVE_USED must come first.
	int iUsed = -1;
	int iFail = -1;
	for (u_int i = 0; i < xEvents.GetSize(); ++i)
	{
		if (xEvents.Get(i).m_eKind == ZM_BATTLE_EVENT_MOVE_USED && iUsed < 0)   { iUsed = (int)i; }
		if (xEvents.Get(i).m_eKind == ZM_BATTLE_EVENT_MOVE_FAILED && iFail < 0) { iFail = (int)i; }
	}
	ZENITH_ASSERT_TRUE(iUsed >= 0, "a failed status move still announces MOVE_USED");
	ZENITH_ASSERT_TRUE(iFail >= 0, "expected MOVE_FAILED");
	ZENITH_ASSERT_TRUE(iUsed < iFail, "MOVE_USED must precede MOVE_FAILED");
}

// ============================================================================
// STATUS-category primary draws NO crit/roll/proc RNG. After executing one, the
// state RNG is byte-identical to a fresh mirror on the same seed (0 draws), so
// the next output matches. Whetclaw is ALWAYS_HITS (no accuracy draw either);
// Warcry is acc 100 -> effAcc 100 -> also no accuracy draw.
// ============================================================================
ZENITH_TEST(ZM_Battle, Status_PrimaryConsumesNoCritRollDraw)
{
	const u_int64 ulSeed = 0xABCDEFull;

	{
		ZM_BattleState xState;
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_RunPlayerMove(MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_WHETCLAW),
			MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), ulSeed, xState, xEvents);
		ZM_BattleRNG xMirror(ulSeed, 54ull);
		ZENITH_ASSERT_TRUE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_STAT_STAGE_CHANGED), "Whetclaw should still apply");
		ZENITH_ASSERT_EQ(xState.m_xRNG.Next(), xMirror.Next(), "Whetclaw (STATUS primary) must consume 0 RNG draws");
	}
	{
		ZM_BattleState xState;
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_RunPlayerMove(MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_WARCRY),
			MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), ulSeed, xState, xEvents);
		ZM_BattleRNG xMirror(ulSeed, 54ull);
		ZENITH_ASSERT_EQ(xState.m_xRNG.Next(), xMirror.Next(), "Warcry (acc-100 STATUS primary) must consume 0 RNG draws");
	}
}

// A STATUS move that CAN miss (Shiver Cry, acc 95) applies nothing on a missed
// accuracy roll: MOVE_MISSED, no STAT_STAGE_CHANGED, target stage untouched.
ZENITH_TEST(ZM_Battle, Status_CanMissMoveAppliesNothingOnMiss)
{
	const ZM_MOVE_ID eMove = ZM_MOVE_SHIVERCRY;                 // STATUS, LOWER_SPATTACK, acc 95
	const u_int uAcc = ZM_GetMoveData(eMove).m_uAccuracy;
	ZENITH_ASSERT_TRUE(uAcc != uZM_MOVE_ACCURACY_ALWAYS_HITS && uAcc < 100u, "test needs a can-miss status move");

	bool bTested = false;
	for (u_int64 ulSeed = 1ull; ulSeed <= 512ull && !bTested; ++ulSeed)
	{
		ZM_BattleRNG xMirror(ulSeed, 54ull);
		if (xMirror.RandBelow(100u) < uAcc) { continue; }       // want a MISS seed

		ZM_BattleState xState;
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_RunPlayerMove(MakeSpec(ZM_SPECIES_NIBBIN, 20u, eMove),
			MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), ulSeed, xState, xEvents);

		ZENITH_ASSERT_TRUE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_MISSED), "expected a MOVE_MISSED on the rigged miss seed");
		ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_STAT_STAGE_CHANGED), "a missed status move applies no stage change");
		ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_ENEMY).Active().m_aiStage[ZM_BATTLE_STAT_SPATTACK], 0, "target SpAtk must be untouched on a miss");
		bTested = true;
	}
	ZENITH_ASSERT_TRUE(bTested, "no miss seed found for the can-miss status move");
}

// ============================================================================
// Accuracy/evasion stage fold: effAcc = base acc folded by (attacker ACCURACY
// stage - defender EVASION stage) via ZM_ApplyStatStage; a draw is pulled only
// when effAcc < 100. Driven on Torrent Cannon (acc 80, effect NONE).
// ============================================================================
namespace
{
	// Build the acc-fold fixture: attacker holds the can-miss move; a very bulky
	// defender so a connecting hit never faints (so MOVE_MISSED is the only miss
	// signal). Rig acc/eva stages via the out params after building.
	ZM_BattleMonsterSpec ZM_AccAttacker(ZM_MOVE_ID eMove)
	{
		return MakeSpecOverride(ZM_SPECIES_NIBBIN, 20u, eMove, 200u, 40u, 40u, 40u, 40u, 100u);
	}
	ZM_BattleMonsterSpec ZM_AccDefender()
	{
		return MakeSpecOverride(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH, 250u, 40u, 250u, 40u, 250u, 10u);
	}

	// Execute Torrent Cannon on a fresh state with rigged acc/eva stages; return
	// whether MOVE_MISSED was emitted.
	bool ZM_ExecMiss(u_int64 ulSeed, int iAtkAccStage, int iDefEvaStage)
	{
		ZM_BattleState xState;
		BuildBattleState(xState, ZM_AccAttacker(ZM_MOVE_TORRENTCANNON), ZM_AccDefender(), ulSeed, 54ull);
		xState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[ZM_BATTLE_STAT_ACCURACY] = iAtkAccStage;
		xState.Side(ZM_SIDE_ENEMY).Active().m_aiStage[ZM_BATTLE_STAT_EVASION]  = iDefEvaStage;
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
		ZM_MoveExecutor::Execute(xCtx);
		return ZM_HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_MISSED);
	}
}

// +1 accuracy on an 80-acc move -> effAcc 120 -> no draw -> a base-miss becomes a
// hit.
ZENITH_TEST(ZM_Battle, Accuracy_StageRaisesHitRate)
{
	const u_int uAcc = ZM_GetMoveData(ZM_MOVE_TORRENTCANNON).m_uAccuracy;   // 80
	bool bTested = false;
	for (u_int64 ulSeed = 1ull; ulSeed <= 512ull && !bTested; ++ulSeed)
	{
		ZM_BattleRNG xMirror(ulSeed, 54ull);
		if (xMirror.RandBelow(100u) < uAcc) { continue; }     // need a base-MISS seed (V >= 80)
		ZENITH_ASSERT_TRUE(ZM_ExecMiss(ulSeed, 0, 0),  "base (stage 0) should MISS on this seed");
		ZENITH_ASSERT_FALSE(ZM_ExecMiss(ulSeed, +1, 0), "+1 accuracy (effAcc>=100, no draw) should convert to a HIT");
		bTested = true;
	}
	ZENITH_ASSERT_TRUE(bTested, "no base-miss seed found");
}

// -2 accuracy on an 80-acc move -> effAcc 40 -> a base-hit becomes a miss.
ZENITH_TEST(ZM_Battle, Accuracy_StageLowersHitRate)
{
	const u_int uAcc = ZM_GetMoveData(ZM_MOVE_TORRENTCANNON).m_uAccuracy;   // 80
	bool bTested = false;
	for (u_int64 ulSeed = 1ull; ulSeed <= 512ull && !bTested; ++ulSeed)
	{
		ZM_BattleRNG xMirror(ulSeed, 54ull);
		const u_int uV = xMirror.RandBelow(100u);
		if (!(uV >= 40u && uV < uAcc)) { continue; }          // base hit (V<80) but effAcc-40 miss (V>=40)
		ZENITH_ASSERT_FALSE(ZM_ExecMiss(ulSeed, 0, 0),  "base (stage 0) should HIT on this seed");
		ZENITH_ASSERT_TRUE(ZM_ExecMiss(ulSeed, -2, 0), "-2 accuracy (effAcc 40) should MISS");
		bTested = true;
	}
	ZENITH_ASSERT_TRUE(bTested, "no straddling seed found");
}

// +2 defender evasion on an 80-acc move -> effAcc 40 -> a base-hit becomes a miss.
ZENITH_TEST(ZM_Battle, Evasion_StageLowersHitRate)
{
	const u_int uAcc = ZM_GetMoveData(ZM_MOVE_TORRENTCANNON).m_uAccuracy;   // 80
	bool bTested = false;
	for (u_int64 ulSeed = 1ull; ulSeed <= 512ull && !bTested; ++ulSeed)
	{
		ZM_BattleRNG xMirror(ulSeed, 54ull);
		const u_int uV = xMirror.RandBelow(100u);
		if (!(uV >= 40u && uV < uAcc)) { continue; }
		ZENITH_ASSERT_FALSE(ZM_ExecMiss(ulSeed, 0, 0),  "base (stage 0) should HIT on this seed");
		ZENITH_ASSERT_TRUE(ZM_ExecMiss(ulSeed, 0, +2), "+2 evasion (effAcc 40) should MISS");
		bTested = true;
	}
	ZENITH_ASSERT_TRUE(bTested, "no straddling seed found");
}

// Equal attacker-accuracy and defender-evasion stages net to zero: the fold is
// identity, so the hit/miss decision matches stage 0 on every seed (hit + miss).
ZENITH_TEST(ZM_Battle, AccEva_EqualStagesNetZeroIdentity)
{
	bool bSawHit = false;
	bool bSawMiss = false;
	for (u_int64 ulSeed = 1ull; ulSeed <= 40ull; ++ulSeed)
	{
		const bool bBase = ZM_ExecMiss(ulSeed, 0, 0);
		const bool bNet0 = ZM_ExecMiss(ulSeed, +2, +2);
		ZENITH_ASSERT_TRUE(bBase == bNet0, "net-zero acc/eva must match the stage-0 decision (seed %llu)", (unsigned long long)ulSeed);
		if (bBase) { bSawMiss = true; } else { bSawHit = true; }
	}
	ZENITH_ASSERT_TRUE(bSawHit,  "expected at least one HIT across the seed sweep");
	ZENITH_ASSERT_TRUE(bSawMiss, "expected at least one MISS across the seed sweep");
}

// ============================================================================
// Damaging-move stat SECONDARY: ApplyDamagingHit runs first, then a proc draw
// (RandBelow(100) < chance) gates the stat effect. Saltspray (WATER special,
// acc 100, chance 20, LOWER_SPDEFENSE) into a bulky NORMAL defender (WATER is
// neutral, survives) -> draw order is crit, roll, proc.
// ============================================================================
namespace
{
	ZM_BattleMonsterSpec ZM_ProcAttacker()
	{
		return MakeSpecOverride(ZM_SPECIES_NIBBIN, 25u, ZM_MOVE_SALTSPRAY, 60u, 40u, 40u, 60u, 40u, 100u);
	}
	ZM_BattleMonsterSpec ZM_ProcDefender()
	{
		return MakeSpecOverride(ZM_SPECIES_STRAYLING, 25u, ZM_MOVE_RAMBASH, 250u, 40u, 250u, 40u, 250u, 10u);
	}

	// Mirror the executor's draw stream for a single-hit damaging + secondary move:
	// crit (Chance 1/24), roll (RandRange 85..100), then the proc gate.
	bool ZM_PredictProc(u_int64 ulSeed, u_int uChance)
	{
		ZM_BattleRNG xMirror(ulSeed, 54ull);
		xMirror.Chance(1u, 24u);          // crit
		xMirror.RandRange(85u, 100u);     // roll
		return xMirror.RandBelow(100u) < uChance;
	}
}

// A rigged seed that FORCES the proc -> DAMAGE_DEALT then the stat drop.
ZENITH_TEST(ZM_Battle, Secondary_ProcForced_AppliesStatDrop)
{
	const u_int uChance = ZM_GetMoveData(ZM_MOVE_SALTSPRAY).m_uEffectChance;   // 20
	const int   iMag    = ZM_GetMoveData(ZM_MOVE_SALTSPRAY).m_iEffectMagnitude;
	bool bTested = false;
	for (u_int64 ulSeed = 1ull; ulSeed <= 512ull && !bTested; ++ulSeed)
	{
		if (!ZM_PredictProc(ulSeed, uChance)) { continue; }
		ZM_BattleState xState;
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_RunPlayerMove(ZM_ProcAttacker(), ZM_ProcDefender(), ulSeed, xState, xEvents);

		ZENITH_ASSERT_GT(xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP, 0u, "defender must survive so the secondary can proc");
		int iAmount = 0;
		const bool bFound = ZM_FindStageChange(xEvents, ZM_SIDE_ENEMY, ZM_BATTLE_STAT_SPDEFENSE, iAmount);
		ZENITH_ASSERT_TRUE(bFound, "a forced proc must lower enemy SpDefense");
		ZENITH_ASSERT_EQ(iAmount, -iMag);
		// The damaging hit precedes the secondary.
		ZENITH_ASSERT_TRUE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT), "expected a damaging hit");
		bTested = true;
	}
	ZENITH_ASSERT_TRUE(bTested, "no proc-forcing seed found");
}

// A rigged seed where the proc does NOT fire -> DAMAGE_DEALT only, no stat drop.
ZENITH_TEST(ZM_Battle, Secondary_ProcSuppressed_NoStatDrop)
{
	const u_int uChance = ZM_GetMoveData(ZM_MOVE_SALTSPRAY).m_uEffectChance;
	bool bTested = false;
	for (u_int64 ulSeed = 1ull; ulSeed <= 512ull && !bTested; ++ulSeed)
	{
		if (ZM_PredictProc(ulSeed, uChance)) { continue; }
		ZM_BattleState xState;
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_RunPlayerMove(ZM_ProcAttacker(), ZM_ProcDefender(), ulSeed, xState, xEvents);

		ZENITH_ASSERT_TRUE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT), "expected a damaging hit");
		ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_STAT_STAGE_CHANGED), "a suppressed proc applies no stage change");
		ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_ENEMY).Active().m_aiStage[ZM_BATTLE_STAT_SPDEFENSE], 0);
		bTested = true;
	}
	ZENITH_ASSERT_TRUE(bTested, "no proc-suppressing seed found");
}

// The proc draw is pulled AFTER crit + roll (chance<100): exactly three RandBelow
// calls (crit, roll, proc) are consumed, in that order, so the post-move RNG
// matches a mirror that drew the same three. Also confirms proc == prediction.
ZENITH_TEST(ZM_Battle, Secondary_DrawOrderCritRollProc)
{
	const u_int uChance = ZM_GetMoveData(ZM_MOVE_SALTSPRAY).m_uEffectChance;
	const u_int64 ulSeed = 0x1234ull;

	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunPlayerMove(ZM_ProcAttacker(), ZM_ProcDefender(), ulSeed, xState, xEvents);
	ZENITH_ASSERT_GT(xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP, 0u, "defender must survive so the proc draw is pulled");

	// Mirror the same three draws; the RNG cursors must then coincide.
	ZM_BattleRNG xMirror(ulSeed, 54ull);
	xMirror.Chance(1u, 24u);
	xMirror.RandRange(85u, 100u);
	const bool bPredictProc = (xMirror.RandBelow(100u) < uChance);
	ZENITH_ASSERT_EQ(xState.m_xRNG.Next(), xMirror.Next(),
		"exactly {crit, roll, proc} must be consumed, in order");
	ZENITH_ASSERT_TRUE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_STAT_STAGE_CHANGED) == bPredictProc,
		"observed proc must match the mirrored prediction");
}

// A chance>=100 secondary applies UNCONDITIONALLY with NO proc draw: only crit +
// roll are consumed (two draws), and the stat drop still lands. Close Bout is a
// chance-100 LOWER_DEFENSE physical.
ZENITH_TEST(ZM_Battle, Secondary_Chance100AppliesWithoutProcDraw)
{
	ZENITH_ASSERT_GE(ZM_GetMoveData(ZM_MOVE_CLOSEBOUT).m_uEffectChance, 100u, "test needs a chance>=100 secondary");
	const u_int64 ulSeed = 0x1234ull;

	const ZM_BattleMonsterSpec xPlayer = MakeSpecOverride(ZM_SPECIES_NIBBIN,    25u, ZM_MOVE_CLOSEBOUT, 80u, 40u,  40u, 40u,  40u, 100u);
	const ZM_BattleMonsterSpec xEnemy  = MakeSpecOverride(ZM_SPECIES_STRAYLING, 25u, ZM_MOVE_RAMBASH,  250u, 40u, 250u, 40u, 250u,  10u);

	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunPlayerMove(xPlayer, xEnemy, ulSeed, xState, xEvents);
	ZENITH_ASSERT_GT(xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP, 0u, "defender must survive");

	// Only crit + roll are drawn (no proc draw) -> mirror those two, cursors coincide.
	ZM_BattleRNG xMirror(ulSeed, 54ull);
	xMirror.Chance(1u, 24u);
	xMirror.RandRange(85u, 100u);
	ZENITH_ASSERT_EQ(xState.m_xRNG.Next(), xMirror.Next(), "a chance-100 secondary pulls NO proc draw");
	ZENITH_ASSERT_TRUE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_STAT_STAGE_CHANGED), "a chance-100 secondary always applies");
}

// A seed sweep over the proc gate: for every seed the observed stat-drop presence
// must equal the mirror's prediction (a mini-fuzz of the E3 draw), covering both
// procced and non-procced outcomes.
ZENITH_TEST(ZM_Battle, Secondary_ProcMatchesMirrorAcrossSeeds)
{
	const u_int uChance = ZM_GetMoveData(ZM_MOVE_SALTSPRAY).m_uEffectChance;
	bool bSawProc = false;
	bool bSawNoProc = false;
	for (u_int64 ulSeed = 1ull; ulSeed <= 120ull; ++ulSeed)
	{
		ZM_BattleState xState;
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_RunPlayerMove(ZM_ProcAttacker(), ZM_ProcDefender(), ulSeed, xState, xEvents);
		if (xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP == 0u) { continue; }   // (never, but guard)

		const bool bPredict = ZM_PredictProc(ulSeed, uChance);
		const bool bObserved = ZM_HasKind(xEvents, ZM_BATTLE_EVENT_STAT_STAGE_CHANGED);
		ZENITH_ASSERT_TRUE(bObserved == bPredict, "proc/no-proc must match the mirror (seed %llu)", (unsigned long long)ulSeed);
		if (bPredict) { bSawProc = true; } else { bSawNoProc = true; }
	}
	ZENITH_ASSERT_TRUE(bSawProc,   "sweep saw no proc");
	ZENITH_ASSERT_TRUE(bSawNoProc, "sweep saw no non-proc");
}

// Stat stages feed the damage path: with the SAME seed (identical crit/roll), a
// -1 attacker ATTACK stage lowers ApplyDamagingHit's output vs stage 0, and a +2
// stage raises it. Uses the executor's single damaging-hit site directly.
ZENITH_TEST(ZM_Battle, Stat_StageChangeFeedsDamage)
{
	const u_int64 ulSeed = 0x1234ull;
	const ZM_BattleMonsterSpec xPlayer = MakeSpecOverride(ZM_SPECIES_NIBBIN,    20u, ZM_MOVE_RAMBASH, 60u, 200u, 40u, 40u, 40u, 100u);
	const ZM_BattleMonsterSpec xEnemy  = MakeSpecOverride(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH, 250u, 40u, 120u, 40u, 120u, 10u);

	u_int uBase = 0u;
	u_int uLowered = 0u;
	u_int uRaised = 0u;
	{
		ZM_BattleState xState;
		BuildBattleState(xState, xPlayer, xEnemy, ulSeed, 54ull);
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
		uBase = ZM_MoveExecutor::ApplyDamagingHit(xCtx);
	}
	{
		ZM_BattleState xState;
		BuildBattleState(xState, xPlayer, xEnemy, ulSeed, 54ull);
		xState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[ZM_BATTLE_STAT_ATTACK] = -1;
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
		uLowered = ZM_MoveExecutor::ApplyDamagingHit(xCtx);
	}
	{
		ZM_BattleState xState;
		BuildBattleState(xState, xPlayer, xEnemy, ulSeed, 54ull);
		xState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[ZM_BATTLE_STAT_ATTACK] = +2;
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
		uRaised = ZM_MoveExecutor::ApplyDamagingHit(xCtx);
	}
	ZENITH_ASSERT_LT(uLowered, uBase, "a -1 ATTACK stage must reduce damage");
	ZENITH_ASSERT_GT(uRaised,  uBase, "a +2 ATTACK stage must increase damage");
}

// ============================================================================
// SC2 FIRST GOLDEN -- Scenario_LowerAttack_ExactStream. A STATUS LOWER_ATTACK
// (Warcry, mag 1) from a fast, bulky Nibbin into a stage-0 Strayling, then the
// enemy's Rambash reply (which already sees ATTACK at -1). Single turn, exact
// 9-event stream, derived by the offline oracle (seed 0x1234). The enemy's
// DAMAGE_DEALT amount (3) reflects the -1 ATTACK stage (stat stages feed damage).
// ============================================================================
ZENITH_TEST(ZM_Battle, Scenario_LowerAttack_ExactStream)
{
	ZM_BattleMonsterSpec axPlayer[1] = { MakeSpecOverride(ZM_SPECIES_NIBBIN,    10u, ZM_MOVE_WARCRY,  200u, 10u, 200u, 10u, 200u, 200u) };
	ZM_BattleMonsterSpec axEnemy[1]  = { MakeSpecOverride(ZM_SPECIES_STRAYLING, 10u, ZM_MOVE_RAMBASH,  45u, 60u,  40u, 35u,  40u,  55u) };

	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeTrainerConfig(), axPlayer, 1u, axEnemy, 1u, 0x1234ull, 54ull);
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
	xEngine.ResolveTurn();

	// Single non-terminal turn: player is fast + bulky, so it survives and the
	// battle is not over (no BATTLE_END).
	ZENITH_ASSERT_FALSE(xEngine.IsOver(), "single-turn golden must not end the battle");

	// --- offline-derived golden stream (9 events) ---
	Zenith_Vector<ZM_BattleEvent> xExpected;
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_BEGIN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_PLAYER, 0, ZM_MOVE_NONE, ZM_SPECIES_NIBBIN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_ENEMY, 0, ZM_MOVE_NONE, ZM_SPECIES_STRAYLING));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_BEGIN, ZM_SIDE_COUNT, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER, 0, ZM_MOVE_WARCRY));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_STAT_STAGE_CHANGED, ZM_SIDE_ENEMY, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, -1, (int)ZM_BATTLE_STAT_ATTACK));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_ENEMY, 0, ZM_MOVE_RAMBASH));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_PLAYER, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 3, 60));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_END, ZM_SIDE_COUNT, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));

	ZM_AssertStreamEquals(xExpected, xEngine.GetEvents(), "lowerAttack");

	// Corroborating post-conditions.
	ZENITH_ASSERT_EQ(xEngine.GetState().Side(ZM_SIDE_ENEMY).Active().m_aiStage[ZM_BATTLE_STAT_ATTACK], -1, "enemy ATTACK should be -1 after Warcry");
	ZENITH_ASSERT_EQ(xEngine.GetState().Side(ZM_SIDE_PLAYER).Active().m_uCurHP, 60u, "player should be at 60 HP (63 - 3)");
	ZENITH_ASSERT_TRUE(ZM_ValidateEventStream(xEngine.GetEvents(), xEngine, "lowerAttack"));
}

// ============================================================================
// SC2 REVIEW-FIX coverage (3 untested paths the reviewer flagged):
//  1. RAISE_CRIT honors the row magnitude (Killer Focus mag 2), capped at 3.
//  2. A multi-stat STATUS primary caps each stat SILENTLY; it emits ONE whole-move
//     MOVE_FAILED only when NOTHING moved -- never a "failed AND changed" stream.
//  3. The acc/eva stage delta is clamped to [-6,+6] before the accuracy fold.
// Each fix is on a path the 38 prior SC2 tests never reach (crit stage untouched,
// multi-stat starts at stage 0, |delta| <= 2), so all existing goldens are stable.
// ============================================================================
namespace
{
	// Count events of a given kind (whole-stream, order-independent).
	u_int ZM_CountKind(const Zenith_Vector<ZM_BattleEvent>& xEvents, ZM_BATTLE_EVENT eKind)
	{
		u_int uCount = 0u;
		for (u_int i = 0; i < xEvents.GetSize(); ++i)
		{
			if (xEvents.Get(i).m_eKind == eKind) { ++uCount; }
		}
		return uCount;
	}
}

// Fix 1. RAISE_CRIT adds the row magnitude to m_iCritStage (Killer Focus = +2), NOT a
// flat +1, capped at 3. A second use caps. No STAT_STAGE_CHANGED / MOVE_FAILED and no
// accuracy/crit/roll draw: the always-hits STATUS primary's stream is just [MOVE_USED].
ZENITH_TEST(ZM_Battle, RaiseCrit_HonorsMagnitude)
{
	// Killer Focus is the sole RAISE_CRIT row: STATUS, always-hits, magnitude 2.
	ZENITH_ASSERT_EQ((u_int)ZM_GetMoveData(ZM_MOVE_KILLERFOCUS).m_eEffect, (u_int)ZM_MOVE_EFFECT_RAISE_CRIT);
	ZENITH_ASSERT_EQ(ZM_GetMoveData(ZM_MOVE_KILLERFOCUS).m_iEffectMagnitude, 2, "test needs the mag-2 RAISE_CRIT row");

	ZM_BattleState xState;
	BuildBattleState(xState, MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_KILLERFOCUS),
		MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, 54ull);
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_PLAYER).Active().m_iCritStage, 0, "crit stage starts at 0");

	// First use: crit stage 0 -> 2 (honors magnitude 2). Stream = MOVE_USED only.
	Zenith_Vector<ZM_BattleEvent> xFirst;
	{
		ZM_MoveContext xCtx = MakeCtx(xState, xFirst, ZM_SIDE_PLAYER, 0u);
		ZM_MoveExecutor::Execute(xCtx);
	}
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_PLAYER).Active().m_iCritStage, 2, "mag-2 RAISE_CRIT must raise the crit stage to 2, not 1");
	ZENITH_ASSERT_EQ(xFirst.GetSize(), 1u, "a RAISE_CRIT primary emits only MOVE_USED");
	if (xFirst.GetSize() == 1u)
	{
		ZENITH_ASSERT_TRUE(xFirst.Get(0) == ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER, 0u, ZM_MOVE_KILLERFOCUS),
			"the only event is MOVE_USED(Killer Focus)");
	}
	ZENITH_ASSERT_FALSE(ZM_HasKind(xFirst, ZM_BATTLE_EVENT_STAT_STAGE_CHANGED), "RAISE_CRIT emits no STAT_STAGE_CHANGED");
	ZENITH_ASSERT_FALSE(ZM_HasKind(xFirst, ZM_BATTLE_EVENT_MOVE_FAILED), "RAISE_CRIT emits no MOVE_FAILED");

	// Second use: 2 + 2 = 4 -> capped at 3. Still MOVE_USED only.
	Zenith_Vector<ZM_BattleEvent> xSecond;
	{
		ZM_MoveContext xCtx = MakeCtx(xState, xSecond, ZM_SIDE_PLAYER, 0u);
		ZM_MoveExecutor::Execute(xCtx);
	}
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_PLAYER).Active().m_iCritStage, 3, "a second mag-2 RAISE_CRIT caps the crit stage at 3");
	ZENITH_ASSERT_EQ(xSecond.GetSize(), 1u, "a capped RAISE_CRIT primary still emits only MOVE_USED");
}

// Fix 1 (rate). At m_iCritStage == 2 the crit rate is 1/2 == Chance(1,2) == RandBelow(2):
// a seed whose first RandBelow(2)==0 crits, ==1 does not. Proven both directions on
// ApplyDamagingHit directly (Rambash's own crit stage 0, so the attacker counter
// decides), with a mirror RNG confirming exactly {crit=Chance(1,2), roll} are drawn.
ZENITH_TEST(ZM_Battle, RaiseCrit_Stage2AlwaysCritsRiggedElseHalf)
{
	const ZM_BattleMonsterSpec xPlayer = MakeSpecOverride(ZM_SPECIES_NIBBIN,    25u, ZM_MOVE_RAMBASH,  60u, 40u,  40u, 40u,  40u, 100u);
	const ZM_BattleMonsterSpec xEnemy  = MakeSpecOverride(ZM_SPECIES_STRAYLING, 25u, ZM_MOVE_RAMBASH, 250u, 40u, 250u, 40u, 250u,  10u);

	bool bSawCrit = false;
	bool bSawNoCrit = false;
	for (u_int64 ulSeed = 1ull; ulSeed <= 64ull && !(bSawCrit && bSawNoCrit); ++ulSeed)
	{
		// Mirror the stage-2 draw stream: crit = Chance(1,2), then roll = RandRange(85,100).
		ZM_BattleRNG xMirror(ulSeed, 54ull);
		const bool bPredictCrit = xMirror.Chance(1u, 2u);
		xMirror.RandRange(85u, 100u);

		ZM_BattleState xState;
		BuildBattleState(xState, xPlayer, xEnemy, ulSeed, 54ull);
		xState.Side(ZM_SIDE_PLAYER).Active().m_iCritStage = 2;
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
		ZM_MoveExecutor::ApplyDamagingHit(xCtx);

		ZENITH_ASSERT_GT(xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP, 0u, "defender must survive so the stream stays clean");
		// The crit decision matches Chance(1,2)...
		ZENITH_ASSERT_TRUE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_CRIT) == bPredictCrit,
			"stage-2 crit must equal Chance(1,2) (seed %llu)", (unsigned long long)ulSeed);
		// ...and exactly {crit, roll} are consumed -- proving the crit draw is RandBelow(2).
		ZENITH_ASSERT_EQ(xState.m_xRNG.Next(), xMirror.Next(), "stage-2 crit consumes exactly {Chance(1,2), roll}");

		if (bPredictCrit) { bSawCrit = true; } else { bSawNoCrit = true; }
	}
	ZENITH_ASSERT_TRUE(bSawCrit,   "no seed produced a stage-2 crit (rate should be ~1/2)");
	ZENITH_ASSERT_TRUE(bSawNoCrit, "no seed produced a stage-2 non-crit (rate should be ~1/2)");
}

// Fix 2 (partial cap). Bulk Stance = RAISE_ATTACK_DEFENSE (STATUS multi-stat, mag 1,
// self). Pre-pin ATTACK at +6: the other stat (DEFENSE) still changes, the capped
// ATTACK is SILENT, and NO MOVE_FAILED is emitted (no "failed AND changed" stream).
ZENITH_TEST(ZM_Battle, MultiStatPrimary_PartialCapNoMoveFailed)
{
	ZENITH_ASSERT_EQ((u_int)ZM_GetMoveData(ZM_MOVE_BULKSTANCE).m_eEffect, (u_int)ZM_MOVE_EFFECT_RAISE_ATTACK_DEFENSE);

	ZM_BattleState xState;
	BuildBattleState(xState, MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_BULKSTANCE),
		MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, 54ull);
	xState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[ZM_BATTLE_STAT_ATTACK] = iZM_MAX_STAGE;

	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);

	// The uncapped DEFENSE moved by the +1 magnitude.
	int iAmount = 0;
	const bool bDefChanged = ZM_FindStageChange(xEvents, ZM_SIDE_PLAYER, ZM_BATTLE_STAT_DEFENSE, iAmount);
	ZENITH_ASSERT_TRUE(bDefChanged, "the uncapped DEFENSE stat must still change");
	ZENITH_ASSERT_EQ(iAmount, 1, "DEFENSE moves by the +1 magnitude");
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[ZM_BATTLE_STAT_DEFENSE], 1);
	// The capped ATTACK is SILENT and held at +6.
	int iAtk = 0;
	ZENITH_ASSERT_FALSE(ZM_FindStageChange(xEvents, ZM_SIDE_PLAYER, ZM_BATTLE_STAT_ATTACK, iAtk), "a per-stat cap must be silent");
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[ZM_BATTLE_STAT_ATTACK], iZM_MAX_STAGE, "ATTACK stays pinned at +6");
	// Exactly one stat change, and crucially NO contradictory MOVE_FAILED.
	ZENITH_ASSERT_EQ(ZM_CountStageChanges(xEvents, ZM_SIDE_PLAYER), 1u, "only the uncapped stat changes");
	ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_FAILED), "a partial multi-stat cap must NOT emit MOVE_FAILED");
}

// Fix 2 (all capped). Bulk Stance with BOTH ATTACK and DEFENSE pre-pinned at +6 ->
// nothing can move -> exactly ONE whole-move MOVE_FAILED(STAT_MAXED) and no
// STAT_STAGE_CHANGED (not one MOVE_FAILED per capped stat).
ZENITH_TEST(ZM_Battle, MultiStatPrimary_AllCappedEmitsOneMoveFailed)
{
	ZM_BattleState xState;
	BuildBattleState(xState, MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_BULKSTANCE),
		MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, 54ull);
	xState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[ZM_BATTLE_STAT_ATTACK]  = iZM_MAX_STAGE;
	xState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[ZM_BATTLE_STAT_DEFENSE] = iZM_MAX_STAGE;

	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);

	ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_STAT_STAGE_CHANGED), "an all-capped multi-stat raise changes nothing");
	ZENITH_ASSERT_EQ(ZM_CountKind(xEvents, ZM_BATTLE_EVENT_MOVE_FAILED), 1u, "exactly ONE whole-move MOVE_FAILED when every stat is capped");
	const ZM_BattleEvent* pxFail = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_MOVE_FAILED);
	ZENITH_ASSERT_NOT_NULL(pxFail, "expected the whole-move MOVE_FAILED");
	if (pxFail != nullptr)
	{
		ZENITH_ASSERT_EQ((u_int)pxFail->m_iAux, (u_int)ZM_MOVE_FAIL_STAT_MAXED, "reason = STAT_MAXED");
		ZENITH_ASSERT_EQ(pxFail->m_uSide, (u_int)ZM_SIDE_PLAYER, "the whole-move failure is announced on the user (self) side");
	}
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[ZM_BATTLE_STAT_ATTACK],  iZM_MAX_STAGE, "ATTACK held at +6");
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[ZM_BATTLE_STAT_DEFENSE], iZM_MAX_STAGE, "DEFENSE held at +6");
}

// Fix 3 (acc/eva clamp). Rig attacker ACCURACY -3 and defender EVASION +6 -> raw delta
// -9, which MUST clamp to -6 before the fold. Torrent Cannon (base acc 80): the clamped
// effAcc is 20, whereas the raw -9 fold would give 14. On a boundary seed whose accuracy
// draw V lands in [14,20) the clamped fold HITS but the unclamped one would MISS -- so a
// HIT proves the delta was clamped to -6, not passed through at -9.
ZENITH_TEST(ZM_Battle, AccuracyStageDeltaClampedAtSix)
{
	const u_int uAcc = ZM_GetMoveData(ZM_MOVE_TORRENTCANNON).m_uAccuracy;   // 80
	const u_int uEffClamped   = ZM_ApplyStatStage(uAcc, iZM_MIN_STAGE);     // fold at -6 -> 20
	const u_int uEffUnclamped = ZM_ApplyStatStage(uAcc, -9);                // raw -9 fold  -> 14
	ZENITH_ASSERT_LT(uEffUnclamped, uEffClamped, "fixture must make the clamp observable");

	bool bTested = false;
	for (u_int64 ulSeed = 1ull; ulSeed <= 4096ull && !bTested; ++ulSeed)
	{
		// The executor's FIRST draw is RandBelow(100) (base acc 80 < 100). Find a seed
		// whose V straddles the two effAccs: HIT under clamped 20, MISS under raw 14.
		ZM_BattleRNG xMirror(ulSeed, 54ull);
		const u_int uV = xMirror.RandBelow(100u);
		if (!(uV >= uEffUnclamped && uV < uEffClamped)) { continue; }

		ZENITH_ASSERT_FALSE(ZM_ExecMiss(ulSeed, -3, +6),
			"a -9 raw delta must clamp to -6 (effAcc %u -> HIT), not use the raw -9 (effAcc %u -> MISS)",
			uEffClamped, uEffUnclamped);
		bTested = true;
	}
	ZENITH_ASSERT_TRUE(bTested, "no boundary seed found in [effUnclamped, effClamped)");
}
