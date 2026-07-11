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
#include "Zenithmon/Source/Battle/ZM_StatusLogic.h"
#include "Zenithmon/Source/Battle/ZM_CatchCalc.h"
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
		case ZM_BATTLE_EVENT_NO_PP:            return "NO_PP";
		case ZM_BATTLE_EVENT_MOVE_FAILED:      return "MOVE_FAILED";
		case ZM_BATTLE_EVENT_STATUS_APPLIED:   return "STATUS_APPLIED";
		case ZM_BATTLE_EVENT_STATUS_DAMAGE:    return "STATUS_DAMAGE";
		case ZM_BATTLE_EVENT_STATUS_CURED:     return "STATUS_CURED";
		case ZM_BATTLE_EVENT_STAT_STAGE_CHANGED:return "STAT_STAGE_CHANGED";
		case ZM_BATTLE_EVENT_VOLATILE_APPLIED: return "VOLATILE_APPLIED";
		case ZM_BATTLE_EVENT_VOLATILE_ENDED:   return "VOLATILE_ENDED";
		case ZM_BATTLE_EVENT_FLINCH:           return "FLINCH";
		case ZM_BATTLE_EVENT_HEAL:             return "HEAL";
		case ZM_BATTLE_EVENT_DRAIN:            return "DRAIN";
		case ZM_BATTLE_EVENT_RECOIL:           return "RECOIL";
		case ZM_BATTLE_EVENT_MULTI_HIT:        return "MULTI_HIT";
		case ZM_BATTLE_EVENT_CATCH_SHAKE:      return "CATCH_SHAKE";
		case ZM_BATTLE_EVENT_CATCH_RESULT:     return "CATCH_RESULT";
		case ZM_BATTLE_EVENT_FLEE:             return "FLEE";
		case ZM_BATTLE_EVENT_FLEE_FAILED:      return "FLEE_FAILED";
		default:                              return "?";
		}
	}

	// Compact one-line stringification of a ZM_BattleEvent (all 8 scalar fields).
	void ZM_BattleEventToString(const ZM_BattleEvent& xEvent, char* acOut, size_t uSize)
	{
		snprintf(acOut, uSize, "[%s side=%u slot=%u move=%u species=%u amt=%d aux=%d tag=%d]",
			ZM_EventKindName(xEvent.m_eKind), xEvent.m_uSide, xEvent.m_uSlot,
			xEvent.m_uMoveId, xEvent.m_uSpeciesId, xEvent.m_iAmount, xEvent.m_iAux,
			xEvent.m_iTag);
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
				eKind == ZM_BATTLE_EVENT_FAINT       || eKind == ZM_BATTLE_EVENT_NO_PP         ||
				eKind == ZM_BATTLE_EVENT_MOVE_FAILED || eKind == ZM_BATTLE_EVENT_STATUS_APPLIED ||
				eKind == ZM_BATTLE_EVENT_STATUS_DAMAGE || eKind == ZM_BATTLE_EVENT_STATUS_CURED ||
				eKind == ZM_BATTLE_EVENT_STAT_STAGE_CHANGED ||
				eKind == ZM_BATTLE_EVENT_VOLATILE_APPLIED || eKind == ZM_BATTLE_EVENT_VOLATILE_ENDED ||
				eKind == ZM_BATTLE_EVENT_FLINCH      || eKind == ZM_BATTLE_EVENT_HEAL          ||
				eKind == ZM_BATTLE_EVENT_DRAIN       || eKind == ZM_BATTLE_EVENT_RECOIL        ||
				eKind == ZM_BATTLE_EVENT_MULTI_HIT   ||
				eKind == ZM_BATTLE_EVENT_CATCH_SHAKE || eKind == ZM_BATTLE_EVENT_CATCH_RESULT  ||
				eKind == ZM_BATTLE_EVENT_FLEE        || eKind == ZM_BATTLE_EVENT_FLEE_FAILED;

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
			case ZM_BATTLE_EVENT_STATUS_DAMAGE:
			{
				// Rule 5: direct/status damage amount >= 0 and remaining HP in [0, maxHP].
				if (x.m_iAmount < 0) { ZM_VALIDATE_FAIL(i, "rule5: damage amount < 0"); }
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
				// Rule 7: at most once per (side,slot); preceded by direct/status/recoil
				// damage with remaining HP 0 for the same monster. Leech Seed's optional
				// DRAIN follows the FAINT, so STATUS_DAMAGE remains immediately prior.
				if (aabFainted[x.m_uSide][x.m_uSlot]) { ZM_VALIDATE_FAIL(i, "rule7: duplicate FAINT for the same monster"); }
				if (i == 0u) { ZM_VALIDATE_FAIL(i, "rule7: FAINT with no preceding DAMAGE_DEALT"); }
				const ZM_BattleEvent& p = xEvents.Get(i - 1u);
				const bool bDamageKind = p.m_eKind == ZM_BATTLE_EVENT_DAMAGE_DEALT ||
					p.m_eKind == ZM_BATTLE_EVENT_STATUS_DAMAGE || p.m_eKind == ZM_BATTLE_EVENT_RECOIL;
				const bool bPrevOk = bDamageKind &&
				                     p.m_uSide == x.m_uSide && p.m_uSlot == x.m_uSlot && p.m_iAux == 0;
				if (!bPrevOk) { ZM_VALIDATE_FAIL(i, "rule7: FAINT not preceded by same-monster damage(remaining==0)"); }
				aabFainted[x.m_uSide][x.m_uSlot] = true;
				break;
			}

			// Rule 9 (SC6): CATCH_SHAKE index in [1,4]; CATCH_RESULT amount in {0,1}
			// with a shake count in [0,4]; FLEE/FLEE_FAILED carry no damage. These
			// only appear inside a turn (rule 3 already forbids them outside one).
			case ZM_BATTLE_EVENT_CATCH_SHAKE:
				if (x.m_iAmount < 1 || x.m_iAmount > 4) { ZM_VALIDATE_FAIL(i, "rule9: CATCH_SHAKE index not in [1,4]"); }
				break;

			case ZM_BATTLE_EVENT_CATCH_RESULT:
				if (x.m_iAmount != 0 && x.m_iAmount != 1) { ZM_VALIDATE_FAIL(i, "rule9: CATCH_RESULT amount not in {0,1}"); }
				if (x.m_iAux < 0 || x.m_iAux > 4)         { ZM_VALIDATE_FAIL(i, "rule9: CATCH_RESULT shake count not in [0,4]"); }
				break;

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
// 14. Fuzz_Soak_2000Battles_Invariants -- the S2 battle-engine soak (TestPlan
//     5.2). 2000 seeded 1v1 battles (mono-type species, glass-cannon FIRE move ->
//     never immune -> guaranteed KO termination), each capped at 500 turns. Half
//     are WILD, where the player periodically attempts a catch (incl. the
//     guaranteed PRIMEORB) or a run, so the SC6 pre-move path is exercised across
//     the fuzz. Per battle assert termination and, throughout, HP/PP/stage bounds,
//     no fainted actor acts, and full stream well-formedness. Voluntary switching
//     is covered by the scripted Engine_Switch_* tests -- box 2 has no forced
//     replacement on faint, which a 1v1 soak deliberately sidesteps.
// ============================================================================
ZENITH_TEST(ZM_Battle, Fuzz_Soak_2000Battles_Invariants)
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
	static const ZM_ITEM_ID aeBalls[] = { ZM_ITEM_CATCHORB, ZM_ITEM_GREATORB, ZM_ITEM_ULTRAORB, ZM_ITEM_PRIMEORB };

	for (u_int uBattle = 0; uBattle < 2000u; ++uBattle)
	{
		// Deterministic per-battle selection RNG (independent of the engine RNG).
		ZM_BattleRNG xSel(0x5EED0000ull + uBattle, 54ull);
		const ZM_SPECIES_ID ePlayer = aeMono[xSel.RandBelow(uMonoCount)];
		const ZM_SPECIES_ID eEnemy   = aeMono[xSel.RandBelow(uMonoCount)];
		const u_int uPlayerLvl = 10u + xSel.RandBelow(21u);   // 10..30 (bounds HP)
		const u_int uEnemyLvl  = 10u + xSel.RandBelow(21u);
		const u_int uPlayerSpe = 40u + xSel.RandBelow(60u);   // varied speed (incl. ties)
		const u_int uEnemySpe  = 40u + xSel.RandBelow(60u);
		const bool  bWild      = (uBattle & 1u) != 0u;         // half wild, half trainer

		ZM_BattleConfig xCfg;
		xCfg.m_bIsWild = bWild; xCfg.m_bCanCatch = bWild; xCfg.m_bCanFlee = bWild; xCfg.m_uLevelCap = 0u;

		// Glass-cannon SPECIAL profile: high SpA, low HP/SpDef -> fast KO, well
		// within PP. Flarelash (FIRE special, acc 100, no secondary) never hits an
		// immunity, so the enemy (which always attacks) always deals >=1 damage ->
		// the battle must terminate even if the player only catches / runs.
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
			const ZM_BattleMonster& xActP = xEngine.GetState().Side(ZM_SIDE_PLAYER).m_xParty.Get(0);
			const ZM_BattleMonster& xActE = xEngine.GetState().Side(ZM_SIDE_ENEMY).m_xParty.Get(0);
			// The enemy always attacks; never submit into a spent slot (glass-cannon
			// KOs long before PP runs out, but guard defensively).
			if (xActE.m_axMoves[uSlot].m_uCurPP == 0u) { break; }

			// Player: mostly MOVE; in a wild battle, occasionally catch (random ball,
			// incl. the guaranteed PRIMEORB) or run -> exercises SC6 in the fuzz.
			ZM_BattleAction xPlayerAction = MoveAction(uSlot);
			bool bPlayerMoves = true;
			if (bWild)
			{
				const u_int uPick = xSel.RandBelow(10u);
				if (uPick == 0u)
				{
					xPlayerAction.m_eKind = ZM_ACTION_ITEM;
					xPlayerAction.m_eItem = aeBalls[xSel.RandBelow(4u)];
					bPlayerMoves = false;
				}
				else if (uPick == 1u)
				{
					xPlayerAction.m_eKind = ZM_ACTION_RUN;
					bPlayerMoves = false;
				}
			}
			if (bPlayerMoves && xActP.m_axMoves[uSlot].m_uCurPP == 0u) { break; }

			xEngine.SubmitAction(ZM_SIDE_PLAYER, xPlayerAction);
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
		ZENITH_ASSERT_TRUE(ZM_ValidateEventStream(xEngine.GetEvents(), xEngine, "soak"),
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

// ============================================================================
// S2 box-2 SC3 -- delivery variants (MULTI_HIT/DOUBLE_HIT/RECOIL/DRAIN/HEAL_HALF/
// FIXED_LEVEL/HALVE_HP/OHKO) + field/screen/hazard SETTERS (state-only, NO event)
// + the bScreen damage reduction (DecisionLog ZM-D-033). Each golden/number is
// cross-checked by the offline oracle (scratchpad/zm_battle_ref.py), whose box-1
// 27-event stream re-derives byte-identical after the SC3 execute_move dispatch
// (the oracle's own gate).
//
// SHARED EVENT CONTRACT (the encodings these tests assert; the orchestrator
// reconciles any that the built engine disagrees with):
//  * MULTI_HIT:  side/slot = the ATTACKER (a summary of the attacker's move, like
//    MOVE_USED/RECOIL/DRAIN -- DAMAGE_DEALT already carries the defender); m_iAmount
//    = hits LANDED. It trails the per-hit DAMAGE_DEALT run (one MULTI_HIT after the
//    last hit); the hit-run stops early on a faint (m_iAmount then < rolled count).
//  * RECOIL:     side/slot = the ATTACKER; m_iAmount = floor(dmgDealt*mag/100);
//    m_iAux = attacker remaining HP after the recoil.
//  * DRAIN:      side/slot = the ATTACKER; m_iAmount = floor(dmgDealt*mag/100)
//    healed; m_iAux = attacker new HP. (Assumed event kind == DRAIN for the drain
//    heal -- see the SC3 report note; the orchestrator reconciles vs HEAL.)
//  * HEAL:       side/slot = the USER; m_iAmount = floor(maxHP*mag/100); m_iAux =
//    new HP.
//  * FIXED_LEVEL dmg == attacker level; HALVE_HP dmg == floor(defCurHP/2) (min 1);
//    OHKO dmg == defender current HP, or MOVE_FAILED(ZM_MOVE_FAIL_OHKO_FAILED) if
//    defender level > attacker level. All THREE draw NO crit/roll.
//  * WEATHER_*/SCREEN_*/HAZARD_SPIKES are STATE-ONLY: asserted via GetState()
//    (m_xField.m_eWeather / side.m_auScreenTurns[..]==5 / m_uHazardSpikeLayers),
//    they emit NO event, and screens/hazards land on the USER / OPPONENT side
//    respectively. Screen reduction is asserted via a HALVED DAMAGE_DEALT.
//
// Discipline (ZM-D-033): the ONE full-stream golden here uses a battle with NO
// weather/screen set (box 3 will add weather/screen events + countdown and must
// not shift these box-2 goldens).
// ============================================================================
namespace
{
	// SC3 stream rule: a MULTI_HIT event's m_iAmount must equal the number of
	// DAMAGE_DEALT events in the contiguous hit-run immediately before it (CRIT/
	// SUPER/NOT may interleave; the run is bounded by the opening MOVE_USED).
	bool ZM_MultiHitAmountMatchesRun(const Zenith_Vector<ZM_BattleEvent>& xEvents)
	{
		for (u_int i = 0; i < xEvents.GetSize(); ++i)
		{
			if (xEvents.Get(i).m_eKind != ZM_BATTLE_EVENT_MULTI_HIT) { continue; }
			u_int uRun = 0u;
			for (int j = (int)i - 1; j >= 0; --j)
			{
				const ZM_BATTLE_EVENT e = xEvents.Get((u_int)j).m_eKind;
				if (e == ZM_BATTLE_EVENT_MOVE_USED) { break; }
				if (e == ZM_BATTLE_EVENT_DAMAGE_DEALT) { ++uRun; }
			}
			if ((int)uRun != xEvents.Get(i).m_iAmount) { return false; }
		}
		return true;
	}

	// Fast Nibbin holding a multi-hit move with LOW ATK/SpA (weak hits) + a very
	// bulky defender that survives a full 5-hit run -- so hits-landed == rolled
	// count and the DAMAGE_DEALT run is unbroken (count-distribution fixtures).
	ZM_BattleMonsterSpec ZM_MultiHitAttacker(ZM_MOVE_ID eMove)
	{
		return MakeSpecOverride(ZM_SPECIES_NIBBIN, 10u, eMove, 200u, 30u, 200u, 30u, 200u, 200u);
	}
	ZM_BattleMonsterSpec ZM_MultiHitBulkyDefender()
	{
		return MakeSpecOverride(ZM_SPECIES_STRAYLING, 10u, ZM_MOVE_RAMBASH, 250u, 10u, 250u, 10u, 250u, 10u);
	}

	// Execute an acc-100 multi-hit move over a bulky defender at ulSeed (so the
	// FIRST draw is the RandBelow(8) count); assert the DAMAGE_DEALT run + the
	// MULTI_HIT amount both equal uHits, and the amount matches the run.
	void ZM_CheckMultiHitCount(ZM_MOVE_ID eMove, u_int64 ulSeed, u_int uHits)
	{
		ZM_BattleState xState;
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_RunPlayerMove(ZM_MultiHitAttacker(eMove), ZM_MultiHitBulkyDefender(), ulSeed, xState, xEvents);
		ZENITH_ASSERT_GT(xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP, 0u, "bulky defender must survive the full hit run");
		ZENITH_ASSERT_EQ(ZM_CountKind(xEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT), uHits, "DAMAGE_DEALT run != expected hits");
		const ZM_BattleEvent* pxMH = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_MULTI_HIT);
		ZENITH_ASSERT_NOT_NULL(pxMH, "a multi-hit move emits a MULTI_HIT event");
		// (The MULTI_HIT side=ATTACKER encoding is pinned once, in the golden below.)
		if (pxMH != nullptr) { ZENITH_ASSERT_EQ((u_int)pxMH->m_iAmount, uHits, "MULTI_HIT amount != expected hits"); }
		ZENITH_ASSERT_TRUE(ZM_MultiHitAmountMatchesRun(xEvents), "MULTI_HIT amount must equal the preceding DAMAGE_DEALT run");
	}
}

// ============================================================================
// SC3 FIRST GOLDEN -- Scenario_MultiHitTwoHits_ExactStream. THORNVOLLEY (GRASS
// PHYSICAL power25 acc100 MULTI_HIT, no STAB vs a NORMAL target) from a fast,
// bulky Nibbin that rolls a 2-hit count, then Strayling's Rambash reply. Single
// non-terminal turn, exact 11-event stream, derived by the offline oracle at
// seed 0x1 -- the ONLY full-stream SC3 golden, and it sets NO weather/screen.
// Stream: [..., MOVE_USED, DAMAGE_DEALT, DAMAGE_DEALT, MULTI_HIT(2), ...].
// ============================================================================
ZENITH_TEST(ZM_Battle, Scenario_MultiHitTwoHits_ExactStream)
{
	ZM_BattleMonsterSpec axPlayer[1] = { MakeSpecOverride(ZM_SPECIES_NIBBIN,    10u, ZM_MOVE_THORNVOLLEY, 200u, 30u, 200u, 10u, 200u, 200u) };
	ZM_BattleMonsterSpec axEnemy[1]  = { MakeSpecOverride(ZM_SPECIES_STRAYLING, 10u, ZM_MOVE_RAMBASH,     200u, 10u,  40u, 10u,  40u,  55u) };

	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeTrainerConfig(), axPlayer, 1u, axEnemy, 1u, 0x1ull, 54ull);
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
	xEngine.ResolveTurn();

	ZENITH_ASSERT_FALSE(xEngine.IsOver(), "single-turn multi-hit golden must not end the battle");

	// --- offline-derived golden stream (11 events, seed 0x1) ---
	Zenith_Vector<ZM_BattleEvent> xExpected;
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_BEGIN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_PLAYER, 0, ZM_MOVE_NONE, ZM_SPECIES_NIBBIN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_ENEMY, 0, ZM_MOVE_NONE, ZM_SPECIES_STRAYLING));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_BEGIN, ZM_SIDE_COUNT, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER, 0, ZM_MOVE_THORNVOLLEY));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_ENEMY, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 4, 59));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_ENEMY, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 3, 56));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MULTI_HIT, ZM_SIDE_PLAYER, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 2));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_ENEMY, 0, ZM_MOVE_RAMBASH));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_PLAYER, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 3, 60));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_END, ZM_SIDE_COUNT, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));

	ZM_AssertStreamEquals(xExpected, xEngine.GetEvents(), "multiHitTwoHits");
	ZENITH_ASSERT_TRUE(ZM_MultiHitAmountMatchesRun(xEngine.GetEvents()), "MULTI_HIT amount must equal the two-hit DAMAGE_DEALT run");
	ZENITH_ASSERT_TRUE(ZM_ValidateEventStream(xEngine.GetEvents(), xEngine, "multiHitTwoHits"));
}

// ---- MULTI_HIT count distribution across all 8 RandBelow(8) draw values -------
// Each rigged seed makes THORNVOLLEY's FIRST draw (the count) hit a fixed value;
// the mapping {0,1,2->2; 3,4,5->3; 6->4; 7->5} is asserted via the hit count.
ZENITH_TEST(ZM_Battle, MultiHit_Draw0_TwoHits)   { ZM_CheckMultiHitCount(ZM_MOVE_THORNVOLLEY, 6ull,  2u); }
ZENITH_TEST(ZM_Battle, MultiHit_Draw1_TwoHits)   { ZM_CheckMultiHitCount(ZM_MOVE_THORNVOLLEY, 1ull,  2u); }
ZENITH_TEST(ZM_Battle, MultiHit_Draw2_TwoHits)   { ZM_CheckMultiHitCount(ZM_MOVE_THORNVOLLEY, 14ull, 2u); }
ZENITH_TEST(ZM_Battle, MultiHit_Draw3_ThreeHits) { ZM_CheckMultiHitCount(ZM_MOVE_THORNVOLLEY, 7ull,  3u); }
ZENITH_TEST(ZM_Battle, MultiHit_Draw4_ThreeHits) { ZM_CheckMultiHitCount(ZM_MOVE_THORNVOLLEY, 2ull,  3u); }
ZENITH_TEST(ZM_Battle, MultiHit_Draw5_ThreeHits) { ZM_CheckMultiHitCount(ZM_MOVE_THORNVOLLEY, 3ull,  3u); }
ZENITH_TEST(ZM_Battle, MultiHit_Draw6_FourHits)  { ZM_CheckMultiHitCount(ZM_MOVE_THORNVOLLEY, 15ull, 4u); }
ZENITH_TEST(ZM_Battle, MultiHit_Draw7_FiveHits)  { ZM_CheckMultiHitCount(ZM_MOVE_THORNVOLLEY, 4ull,  5u); }

// A SPECIAL multi-hit move (Sleet Volley, ICE) uses the same count machinery.
ZENITH_TEST(ZM_Battle, MultiHit_SleetVolley_SpecialVariety)
{
	ZM_CheckMultiHitCount(ZM_MOVE_SLEETVOLLEY, 15ull, 4u);   // draw 6 -> 4 hits
}

// The hit-run stops on a faint: seed 4 rolls a 5-hit count, but a 1-HP defender
// faints on the first hit -> exactly one DAMAGE_DEALT, one FAINT, MULTI_HIT(1).
ZENITH_TEST(ZM_Battle, MultiHit_BreaksOnFaint)
{
	ZM_BattleState xState;
	BuildBattleState(xState, ZM_MultiHitAttacker(ZM_MOVE_THORNVOLLEY), ZM_MultiHitBulkyDefender(), 4ull, 54ull);
	xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP = 1u;   // dies on the first hit

	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);

	// The rolled count was 5 (seed 4), but the run broke on the faint after hit 1.
	ZENITH_ASSERT_EQ(ZM_CountKind(xEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT), 1u, "break-on-faint: only the killing hit lands");
	ZENITH_ASSERT_TRUE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_FAINT), "the defender faints");
	const ZM_BattleEvent* pxMH = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_MULTI_HIT);
	ZENITH_ASSERT_NOT_NULL(pxMH, "a MULTI_HIT still reports the hits landed");
	if (pxMH != nullptr) { ZENITH_ASSERT_EQ((u_int)pxMH->m_iAmount, 1u, "MULTI_HIT amount == hits landed (1), not the rolled 5"); }
	ZENITH_ASSERT_TRUE(ZM_MultiHitAmountMatchesRun(xEvents), "MULTI_HIT amount must equal the (single) DAMAGE_DEALT run");
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP, 0u, "defender is fainted");
}

// The MULTI_HIT event sits immediately after the last DAMAGE_DEALT of the run and
// carries the run length (seed 7 -> 3-hit count).
ZENITH_TEST(ZM_Battle, MultiHit_AmountMatchesRunOrder)
{
	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunPlayerMove(ZM_MultiHitAttacker(ZM_MOVE_THORNVOLLEY), ZM_MultiHitBulkyDefender(), 7ull, xState, xEvents);

	// Locate the MULTI_HIT and the last DAMAGE_DEALT before it.
	int iMH = -1;
	for (u_int i = 0; i < xEvents.GetSize(); ++i)
	{
		if (xEvents.Get(i).m_eKind == ZM_BATTLE_EVENT_MULTI_HIT) { iMH = (int)i; break; }
	}
	ZENITH_ASSERT_TRUE(iMH > 0, "MULTI_HIT must exist and follow the hit run");
	if (iMH > 0)
	{
		ZENITH_ASSERT_EQ((u_int)xEvents.Get((u_int)iMH - 1u).m_eKind, (u_int)ZM_BATTLE_EVENT_DAMAGE_DEALT, "MULTI_HIT immediately follows the last hit");
		ZENITH_ASSERT_EQ((u_int)xEvents.Get((u_int)iMH).m_iAmount, ZM_CountKind(xEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT), "MULTI_HIT amount == DAMAGE_DEALT count");
		ZENITH_ASSERT_EQ((u_int)xEvents.Get((u_int)iMH).m_iAmount, 3u, "seed 7 rolls a 3-hit count");
	}
}

// ============================================================================
// DOUBLE_HIT -- fixed 2 hits, NO count draw. Onetwo (BRAWL) into a mono-FIRE
// defender (BRAWL neutral -> no SUPER noise) so the stream is a clean 2-hit run.
// ============================================================================
namespace
{
	ZM_BattleMonsterSpec ZM_DoubleHitAttacker(ZM_MOVE_ID eMove)
	{
		return MakeSpecOverride(ZM_SPECIES_NIBBIN, 10u, eMove, 200u, 30u, 200u, 30u, 200u, 200u);
	}
	ZM_BattleMonsterSpec ZM_DoubleHitBulkyFire()   // Kindlet is mono-FIRE
	{
		return MakeSpecOverride(ZM_SPECIES_KINDLET, 10u, ZM_MOVE_RAMBASH, 250u, 10u, 250u, 10u, 250u, 10u);
	}
}

ZENITH_TEST(ZM_Battle, DoubleHit_OneTwo_FixedTwoHits)
{
	ZENITH_ASSERT_EQ((u_int)ZM_GetMoveData(ZM_MOVE_ONETWO).m_eEffect, (u_int)ZM_MOVE_EFFECT_DOUBLE_HIT);

	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunPlayerMove(ZM_DoubleHitAttacker(ZM_MOVE_ONETWO), ZM_DoubleHitBulkyFire(), 0x1234ull, xState, xEvents);

	ZENITH_ASSERT_GT(xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP, 0u, "bulky defender must survive both hits");
	ZENITH_ASSERT_EQ(ZM_CountKind(xEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT), 2u, "double-hit lands exactly two hits");
	const ZM_BattleEvent* pxMH = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_MULTI_HIT);
	ZENITH_ASSERT_NOT_NULL(pxMH, "double-hit emits a MULTI_HIT event");
	if (pxMH != nullptr) { ZENITH_ASSERT_EQ((u_int)pxMH->m_iAmount, 2u, "double-hit MULTI_HIT amount is 2"); }
	ZENITH_ASSERT_TRUE(ZM_MultiHitAmountMatchesRun(xEvents), "MULTI_HIT amount must equal the two-hit run");
}

// Proves DOUBLE_HIT pulls NO count draw: the ONLY draws are (crit, roll) x2. A
// mirror RNG that draws exactly that must then coincide with the state RNG.
ZENITH_TEST(ZM_Battle, DoubleHit_NoCountDraw)
{
	const u_int64 ulSeed = 0x1234ull;
	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunPlayerMove(ZM_DoubleHitAttacker(ZM_MOVE_ONETWO), ZM_DoubleHitBulkyFire(), ulSeed, xState, xEvents);
	ZENITH_ASSERT_GT(xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP, 0u, "both hits must land (survivor) so all 4 draws are pulled");
	ZENITH_ASSERT_EQ(ZM_CountKind(xEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT), 2u, "two hits");

	// Onetwo is acc 100 (no accuracy draw); a count draw would desynchronize this.
	ZM_BattleRNG xMirror(ulSeed, 54ull);
	xMirror.Chance(1u, 24u);       xMirror.RandRange(85u, 100u);   // hit 1: crit, roll
	xMirror.Chance(1u, 24u);       xMirror.RandRange(85u, 100u);   // hit 2: crit, roll
	ZENITH_ASSERT_EQ(xState.m_xRNG.Next(), xMirror.Next(), "double-hit consumes exactly {crit,roll}x2 -- no RandBelow(8) count draw");
}

// A second DOUBLE_HIT move (Twin Fang, DRAKE, acc 95). Seed 1 lands the accuracy
// roll; the fixed two hits still land.
ZENITH_TEST(ZM_Battle, DoubleHit_TwinFang_Variety)
{
	ZENITH_ASSERT_EQ((u_int)ZM_GetMoveData(ZM_MOVE_TWINFANG).m_eEffect, (u_int)ZM_MOVE_EFFECT_DOUBLE_HIT);

	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	// Strayling is mono-NORMAL; DRAKE is neutral vs NORMAL.
	ZM_RunPlayerMove(ZM_DoubleHitAttacker(ZM_MOVE_TWINFANG), ZM_MultiHitBulkyDefender(), 1ull, xState, xEvents);

	ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_MISSED), "seed 1 must connect (acc 95)");
	ZENITH_ASSERT_EQ(ZM_CountKind(xEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT), 2u, "twin fang lands two hits");
	const ZM_BattleEvent* pxMH = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_MULTI_HIT);
	ZENITH_ASSERT_NOT_NULL(pxMH, "twin fang emits MULTI_HIT");
	if (pxMH != nullptr) { ZENITH_ASSERT_EQ((u_int)pxMH->m_iAmount, 2u, "twin fang MULTI_HIT amount is 2"); }
}

// ============================================================================
// RECOIL -- self-damage = floor(dmgDealt*mag/100); RECOIL(amount, aux=atkHP).
// Bulky defender survives so DAMAGE_DEALT (raw) == HP lost == the recoil base.
// ============================================================================
namespace
{
	void ZM_CheckRecoil(ZM_MOVE_ID eMove, u_int uExpectMag, u_int64 ulSeed)
	{
		const int iMag = ZM_GetMoveData(eMove).m_iEffectMagnitude;
		ZENITH_ASSERT_EQ((u_int)iMag, uExpectMag, "recoil fixture: unexpected magnitude for move %u", (u_int)eMove);

		ZM_BattleState xState;
		BuildBattleState(xState,
			MakeSpecOverride(ZM_SPECIES_NIBBIN,    20u, eMove,           100u, 60u,  40u, 60u,  40u, 100u),
			MakeSpecOverride(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH, 250u, 10u, 250u, 10u, 250u,  10u), ulSeed, 54ull);
		const u_int uAtkMax = xState.Side(ZM_SIDE_PLAYER).Active().m_auMaxStat[ZM_STAT_HP];

		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
		ZM_MoveExecutor::Execute(xCtx);

		ZENITH_ASSERT_GT(xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP, 0u, "defender must survive so recoil keys off the actual damage");
		const ZM_BattleEvent* pxDmg = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT);
		const ZM_BattleEvent* pxRec = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_RECOIL);
		ZENITH_ASSERT_NOT_NULL(pxDmg, "a recoil move deals damage");
		ZENITH_ASSERT_NOT_NULL(pxRec, "a recoil move emits RECOIL");
		if (pxDmg != nullptr && pxRec != nullptr)
		{
			const int iRecoil = (pxDmg->m_iAmount * iMag) / 100;
			ZENITH_ASSERT_EQ(pxRec->m_iAmount, iRecoil, "RECOIL amount != floor(dmg*mag/100)");
			ZENITH_ASSERT_EQ(pxRec->m_uSide, (u_int)ZM_SIDE_PLAYER, "RECOIL is on the attacker");
			ZENITH_ASSERT_EQ((u_int)pxRec->m_iAux, uAtkMax - (u_int)iRecoil, "RECOIL aux != attacker remaining HP");
			ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_PLAYER).Active().m_uCurHP, uAtkMax - (u_int)iRecoil, "attacker HP not reduced by recoil");
		}
	}
}

ZENITH_TEST(ZM_Battle, Recoil_RecklessRush_Mag33)
{
	ZM_CheckRecoil(ZM_MOVE_RECKLESSRUSH, 33u, 0x1234ull);   // NORMAL, STAB
}
ZENITH_TEST(ZM_Battle, Recoil_Galvano_Mag25)
{
	ZM_CheckRecoil(ZM_MOVE_GALVANORUSH, 25u, 0x1234ull);    // ELECTRIC, no STAB
}
ZENITH_TEST(ZM_Battle, Recoil_BraveBeak_Mag33)
{
	ZM_CheckRecoil(ZM_MOVE_BRAVEBEAK, 33u, 0x1234ull);      // SKY, acc 100
}

// ============================================================================
// DRAIN -- heal = floor(dmgDealt*mag/100); DRAIN(amount, aux=atk new HP). The
// attacker is pre-damaged so the heal is observable and uncapped.
// ============================================================================
namespace
{
	void ZM_CheckDrain(ZM_MOVE_ID eMove, u_int64 ulSeed)
	{
		const int iMag = ZM_GetMoveData(eMove).m_iEffectMagnitude;
		ZM_BattleState xState;
		BuildBattleState(xState,
			MakeSpecOverride(ZM_SPECIES_NIBBIN,    20u, eMove,           200u, 60u,  40u, 90u,  40u, 100u),
			MakeSpecOverride(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH, 250u, 10u, 250u, 10u, 250u,  10u), ulSeed, 54ull);
		const u_int uAtkMax = xState.Side(ZM_SIDE_PLAYER).Active().m_auMaxStat[ZM_STAT_HP];
		const u_int uPre = 5u;   // pre-damaged, well below max
		xState.Side(ZM_SIDE_PLAYER).Active().m_uCurHP = uPre;

		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
		ZM_MoveExecutor::Execute(xCtx);

		ZENITH_ASSERT_GT(xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP, 0u, "defender must survive so drain keys off the actual damage");
		const ZM_BattleEvent* pxDmg = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT);
		const ZM_BattleEvent* pxDrain = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_DRAIN);
		ZENITH_ASSERT_NOT_NULL(pxDmg, "a drain move deals damage");
		ZENITH_ASSERT_NOT_NULL(pxDrain, "a drain move emits DRAIN");
		if (pxDmg != nullptr && pxDrain != nullptr)
		{
			const u_int uHeal = ((u_int)pxDmg->m_iAmount * (u_int)iMag) / 100u;
			ZENITH_ASSERT_GT(uHeal, 0u, "drain fixture must produce a nonzero heal");
			const u_int uNew = (uPre + uHeal > uAtkMax) ? uAtkMax : (uPre + uHeal);
			ZENITH_ASSERT_EQ(pxDrain->m_uSide, (u_int)ZM_SIDE_PLAYER, "DRAIN is on the attacker");
			ZENITH_ASSERT_EQ((u_int)pxDrain->m_iAmount, uHeal, "DRAIN amount != floor(dmg*mag/100)");
			ZENITH_ASSERT_EQ((u_int)pxDrain->m_iAux, uNew, "DRAIN aux != attacker new HP");
			ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_PLAYER).Active().m_uCurHP, uNew, "attacker HP not increased by drain");
		}
	}
}

ZENITH_TEST(ZM_Battle, Drain_Sapdraw_Special)
{
	ZM_CheckDrain(ZM_MOVE_SAPDRAW, 0x1234ull);    // GRASS SPECIAL, mag 50
}
ZENITH_TEST(ZM_Battle, Drain_LeechBite_Physical)
{
	ZM_CheckDrain(ZM_MOVE_LEECHBITE, 0x1234ull);  // VENOM PHYSICAL, mag 50
}
ZENITH_TEST(ZM_Battle, Drain_VeinTap_Variety)
{
	ZM_CheckDrain(ZM_MOVE_VEINTAP, 0x1234ull);    // SWARM PHYSICAL, mag 50
}

// Drain heal caps at max HP: at max-1 HP a positive heal leaves exactly max.
ZENITH_TEST(ZM_Battle, Drain_CappedAtMax)
{
	ZM_BattleState xState;
	BuildBattleState(xState,
		MakeSpecOverride(ZM_SPECIES_NIBBIN,    20u, ZM_MOVE_SAPDRAW, 200u, 60u,  40u, 90u,  40u, 100u),
		MakeSpecOverride(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH, 250u, 10u, 250u, 10u, 250u,  10u), 0x1234ull, 54ull);
	const u_int uAtkMax = xState.Side(ZM_SIDE_PLAYER).Active().m_auMaxStat[ZM_STAT_HP];
	xState.Side(ZM_SIDE_PLAYER).Active().m_uCurHP = uAtkMax - 1u;   // one below full

	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);

	const ZM_BattleEvent* pxDrain = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_DRAIN);
	ZENITH_ASSERT_NOT_NULL(pxDrain, "a drain move emits DRAIN even when it overheals");
	if (pxDrain != nullptr) { ZENITH_ASSERT_EQ((u_int)pxDrain->m_iAux, uAtkMax, "DRAIN aux caps at max HP"); }
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_PLAYER).Active().m_uCurHP, uAtkMax, "attacker HP caps at max");
}

// ============================================================================
// HEAL_HALF -- a STATUS self-heal: HEAL(amount=floor(maxHP*mag/100), aux=new HP),
// 0 RNG draws. The user is pre-damaged so the heal is observable and uncapped.
// ============================================================================
ZENITH_TEST(ZM_Battle, HealHalf_Repose)
{
	ZM_BattleState xState;
	BuildBattleState(xState, MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_REPOSE),
		MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, 54ull);
	const int iMag = ZM_GetMoveData(ZM_MOVE_REPOSE).m_iEffectMagnitude;   // 50
	const u_int uMax = xState.Side(ZM_SIDE_PLAYER).Active().m_auMaxStat[ZM_STAT_HP];
	xState.Side(ZM_SIDE_PLAYER).Active().m_uCurHP = 1u;

	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);

	const u_int uHeal = (uMax * (u_int)iMag) / 100u;
	const u_int uNew = (1u + uHeal > uMax) ? uMax : (1u + uHeal);
	const ZM_BattleEvent* pxHeal = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_HEAL);
	ZENITH_ASSERT_NOT_NULL(pxHeal, "Repose emits HEAL");
	if (pxHeal != nullptr)
	{
		ZENITH_ASSERT_EQ(pxHeal->m_uSide, (u_int)ZM_SIDE_PLAYER, "HEAL is on the user");
		ZENITH_ASSERT_EQ((u_int)pxHeal->m_iAmount, uHeal, "HEAL amount != floor(maxHP*mag/100)");
		ZENITH_ASSERT_EQ((u_int)pxHeal->m_iAux, uNew, "HEAL aux != new HP");
	}
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_PLAYER).Active().m_uCurHP, uNew, "user HP not healed");
}

ZENITH_TEST(ZM_Battle, HealHalf_CappedAtMax)
{
	ZM_BattleState xState;
	BuildBattleState(xState, MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_REPOSE),
		MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, 54ull);
	const u_int uMax = xState.Side(ZM_SIDE_PLAYER).Active().m_auMaxStat[ZM_STAT_HP];
	xState.Side(ZM_SIDE_PLAYER).Active().m_uCurHP = uMax - 1u;

	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);

	const ZM_BattleEvent* pxHeal = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_HEAL);
	ZENITH_ASSERT_NOT_NULL(pxHeal, "Repose emits HEAL even when it overheals");
	if (pxHeal != nullptr) { ZENITH_ASSERT_EQ((u_int)pxHeal->m_iAux, uMax, "HEAL aux caps at max HP"); }
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_PLAYER).Active().m_uCurHP, uMax, "user HP caps at max");
}

// ============================================================================
// FIXED_LEVEL -- dmg == attacker level, NO crit/roll/effectiveness. The defender
// must not be type-immune (Mindshear is MIND vs NORMAL; Nightbrand is PHANTOM,
// which is immune to NORMAL, so it targets a mono-FIRE Kindlet).
// ============================================================================
namespace
{
	void ZM_CheckFixedLevel(ZM_MOVE_ID eMove, ZM_SPECIES_ID eDefSpecies, u_int uLevel, u_int64 ulSeed)
	{
		ZM_BattleState xState;
		BuildBattleState(xState,
			MakeSpec(ZM_SPECIES_NIBBIN, uLevel, eMove),
			MakeSpecOverride(eDefSpecies, uLevel, ZM_MOVE_RAMBASH, 200u, 10u, 200u, 10u, 200u, 10u), ulSeed, 54ull);

		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
		ZM_MoveExecutor::Execute(xCtx);

		const ZM_BattleEvent* pxDmg = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT);
		ZENITH_ASSERT_NOT_NULL(pxDmg, "fixed-level deals damage");
		if (pxDmg != nullptr)
		{
			ZENITH_ASSERT_EQ((u_int)pxDmg->m_iAmount, uLevel, "fixed-level damage != attacker level");
			ZENITH_ASSERT_EQ(pxDmg->m_uSide, (u_int)ZM_SIDE_ENEMY, "fixed-level hits the defender");
		}
		ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_CRIT), "fixed damage never crits");
		ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_SUPER_EFFECTIVE), "fixed damage has no effectiveness");
		ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_NOT_EFFECTIVE), "fixed damage has no effectiveness");
		ZENITH_ASSERT_GT(xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP, 0u, "level-worth of fixed damage leaves the bulky defender alive");
	}
}

ZENITH_TEST(ZM_Battle, FixedLevel_Mindshear_DamageEqualsLevel)
{
	ZM_CheckFixedLevel(ZM_MOVE_MINDSHEAR, ZM_SPECIES_STRAYLING, 20u, 0x1234ull);   // MIND vs NORMAL
}
ZENITH_TEST(ZM_Battle, FixedLevel_NightBrand_DamageEqualsLevel)
{
	ZM_CheckFixedLevel(ZM_MOVE_NIGHTBRAND, ZM_SPECIES_KINDLET, 25u, 0x1234ull);    // PHANTOM vs FIRE
}

// Fixed-level (acc 100) pulls NO RNG draw at all: the state RNG is byte-identical
// to a fresh mirror on the same seed.
ZENITH_TEST(ZM_Battle, FixedLevel_NoCritRollDraw)
{
	const u_int64 ulSeed = 0x1234ull;
	ZM_BattleState xState;
	BuildBattleState(xState, MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_MINDSHEAR),
		MakeSpecOverride(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH, 200u, 10u, 200u, 10u, 200u, 10u), ulSeed, 54ull);

	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);

	ZM_BattleRNG xMirror(ulSeed, 54ull);
	ZENITH_ASSERT_EQ(xState.m_xRNG.Next(), xMirror.Next(), "fixed-level (acc 100) consumes 0 RNG draws");
}

// ============================================================================
// HALVE_HP -- dmg == floor(defCurHP/2) (min 1), NO crit. Gnashdown is acc 90, so
// seed 1 is used (it lands the accuracy roll).
// ============================================================================
ZENITH_TEST(ZM_Battle, HalveHP_Gnashdown_HalvesDefenderHP)
{
	ZM_BattleState xState;
	BuildBattleState(xState,
		MakeSpecOverride(ZM_SPECIES_NIBBIN,    20u, ZM_MOVE_GNASHDOWN, 60u, 60u,  40u, 40u,  40u, 100u),
		MakeSpecOverride(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH,  200u, 10u, 200u, 10u, 200u,  10u), 1ull, 54ull);
	const u_int uDefCur = xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP;

	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);

	ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_MISSED), "seed 1 must connect (acc 90)");
	const ZM_BattleEvent* pxDmg = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT);
	ZENITH_ASSERT_NOT_NULL(pxDmg, "halve-hp deals damage");
	if (pxDmg != nullptr)
	{
		const u_int uExpect = (uDefCur / 2u < 1u) ? 1u : (uDefCur / 2u);
		ZENITH_ASSERT_EQ((u_int)pxDmg->m_iAmount, uExpect, "halve-hp damage != floor(defHP/2) (min 1)");
		ZENITH_ASSERT_EQ((u_int)pxDmg->m_iAux, uDefCur - uExpect, "halve-hp leaves ceil(defHP/2)");
	}
	ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_CRIT), "halve-hp never crits");
}

// The min-1 floor: at 1 HP, floor(1/2)=0 clamps up to 1 -> the defender faints.
ZENITH_TEST(ZM_Battle, HalveHP_MinOne)
{
	ZM_BattleState xState;
	BuildBattleState(xState,
		MakeSpecOverride(ZM_SPECIES_NIBBIN,    20u, ZM_MOVE_GNASHDOWN, 60u, 60u,  40u, 40u,  40u, 100u),
		MakeSpecOverride(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH,  200u, 10u, 200u, 10u, 200u,  10u), 1ull, 54ull);
	xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP = 1u;

	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);

	ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_MISSED), "seed 1 must connect (acc 90)");
	const ZM_BattleEvent* pxDmg = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT);
	ZENITH_ASSERT_NOT_NULL(pxDmg, "halve-hp deals damage");
	if (pxDmg != nullptr) { ZENITH_ASSERT_EQ((u_int)pxDmg->m_iAmount, 1u, "halve-hp of 1 HP clamps up to 1"); }
	ZENITH_ASSERT_TRUE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_FAINT), "the 1-HP defender faints");
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP, 0u);
}

// ============================================================================
// OHKO -- dmg == defender current HP (full KO), or MOVE_FAILED when the defender
// out-levels the attacker. Fissure Crack is acc 30; seed 2 lands the roll.
// ============================================================================
ZENITH_TEST(ZM_Battle, Ohko_FullKO)
{
	// Attacker L30 >= defender L20 -> succeeds.
	ZM_BattleState xState;
	BuildBattleState(xState,
		MakeSpecOverride(ZM_SPECIES_NIBBIN,    30u, ZM_MOVE_FISSURECRACK, 200u, 60u,  40u, 40u,  40u, 100u),
		MakeSpecOverride(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH,      200u, 10u, 200u, 10u, 200u,  10u), 2ull, 54ull);
	const u_int uDefCur = xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP;

	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);

	ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_MISSED), "seed 2 must connect (acc 30)");
	const ZM_BattleEvent* pxDmg = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT);
	ZENITH_ASSERT_NOT_NULL(pxDmg, "ohko deals damage on a hit");
	if (pxDmg != nullptr)
	{
		ZENITH_ASSERT_EQ((u_int)pxDmg->m_iAmount, uDefCur, "ohko damage != defender current HP");
		ZENITH_ASSERT_EQ((u_int)pxDmg->m_iAux, 0u, "ohko leaves 0 HP");
	}
	ZENITH_ASSERT_TRUE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_FAINT), "ohko faints the defender");
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP, 0u);
	ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_CRIT), "ohko never crits");
}

// OHKO into a HIGHER-level target fails with MOVE_FAILED(OHKO_FAILED); no damage.
// (ZM_MOVE_FAIL_OHKO_FAILED is appended to ZM_MOVE_FAIL_REASON by the SC3
// implementer -- see the SC3 report note.)
ZENITH_TEST(ZM_Battle, Ohko_FailsVsHigherLevel)
{
	// Attacker L20 < defender L30 -> fails. Seed 2 connects the accuracy roll, so
	// the failure is the level check (not a miss), whatever the check order.
	ZM_BattleState xState;
	BuildBattleState(xState,
		MakeSpecOverride(ZM_SPECIES_NIBBIN,    20u, ZM_MOVE_FISSURECRACK, 200u, 60u,  40u, 40u,  40u, 100u),
		MakeSpecOverride(ZM_SPECIES_STRAYLING, 30u, ZM_MOVE_RAMBASH,      200u, 10u, 200u, 10u, 200u,  10u), 2ull, 54ull);
	const u_int uDefFull = xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP;

	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);

	const ZM_BattleEvent* pxFail = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_MOVE_FAILED);
	ZENITH_ASSERT_NOT_NULL(pxFail, "OHKO into a higher-level target must MOVE_FAILED");
	if (pxFail != nullptr) { ZENITH_ASSERT_EQ((u_int)pxFail->m_iAux, (u_int)ZM_MOVE_FAIL_OHKO_FAILED, "MOVE_FAILED reason must be OHKO_FAILED"); }
	ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT), "a failed OHKO deals no damage");
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP, uDefFull, "the target's HP is untouched");
}

// OHKO draws only its accuracy roll (acc 30) -- NO crit/roll. After a connecting
// hit the state RNG matches a mirror that drew exactly one RandBelow(100).
ZENITH_TEST(ZM_Battle, Ohko_NoCritRollDraw)
{
	const u_int64 ulSeed = 2ull;
	ZM_BattleState xState;
	BuildBattleState(xState,
		MakeSpecOverride(ZM_SPECIES_NIBBIN,    30u, ZM_MOVE_FISSURECRACK, 200u, 60u,  40u, 40u,  40u, 100u),
		MakeSpecOverride(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH,      200u, 10u, 200u, 10u, 200u,  10u), ulSeed, 54ull);

	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);
	ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_MISSED), "seed 2 must connect (acc 30)");

	ZM_BattleRNG xMirror(ulSeed, 54ull);
	xMirror.RandBelow(100u);   // the sole accuracy draw
	ZENITH_ASSERT_EQ(xState.m_xRNG.Next(), xMirror.Next(), "OHKO consumes only its accuracy draw -- no crit/roll");
}

// ============================================================================
// WEATHER setters -- STATE-ONLY (m_xField.m_eWeather), NO event, 0 RNG draws.
// ============================================================================
namespace
{
	void ZM_CheckWeatherSetter(ZM_MOVE_ID eMove, ZM_WEATHER eExpected)
	{
		ZM_BattleState xState;
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_RunPlayerMove(MakeSpec(ZM_SPECIES_NIBBIN, 20u, eMove),
			MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, xState, xEvents);
		ZENITH_ASSERT_EQ((u_int)xState.m_xField.m_eWeather, (u_int)eExpected, "weather field not set");
		ZENITH_ASSERT_EQ(ZM_CountKind(xEvents, ZM_BATTLE_EVENT_WEATHER_CHANGED), 0u, "box-2 weather setter emits NO event");
	}
}

ZENITH_TEST(ZM_Battle, Weather_Sunflare_SetsSun)   { ZM_CheckWeatherSetter(ZM_MOVE_SUNFLARE, ZM_WEATHER_SUN); }
ZENITH_TEST(ZM_Battle, Weather_Raincall_SetsRain)  { ZM_CheckWeatherSetter(ZM_MOVE_RAINCALL, ZM_WEATHER_RAIN); }
ZENITH_TEST(ZM_Battle, Weather_Sandstir_SetsSand)  { ZM_CheckWeatherSetter(ZM_MOVE_SANDSTIR, ZM_WEATHER_SAND); }
ZENITH_TEST(ZM_Battle, Weather_Snowveil_SetsSnow)  { ZM_CheckWeatherSetter(ZM_MOVE_SNOWVEIL, ZM_WEATHER_SNOW); }

// The whole stream of a weather setter is just [MOVE_USED] -- no weather event.
ZENITH_TEST(ZM_Battle, Weather_SetterEmitsOnlyMoveUsed)
{
	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunPlayerMove(MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_SUNFLARE),
		MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, xState, xEvents);
	ZENITH_ASSERT_EQ(xEvents.GetSize(), 1u, "a weather setter emits only MOVE_USED");
	if (xEvents.GetSize() == 1u)
	{
		ZENITH_ASSERT_TRUE(xEvents.Get(0) == ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER, 0u, ZM_MOVE_SUNFLARE),
			"the only event is MOVE_USED(Sunflare)");
	}
	ZENITH_ASSERT_EQ((u_int)xState.m_xField.m_eWeather, (u_int)ZM_WEATHER_SUN);
}

// A later weather setter OVERWRITES the field weather (Sun -> Rain).
ZENITH_TEST(ZM_Battle, Weather_OverwritesPrevious)
{
	ZM_BattleMonsterSpec xAtk = MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_SUNFLARE);
	xAtk.m_aeMoves[1] = ZM_MOVE_RAINCALL;
	ZM_BattleState xState;
	BuildBattleState(xState, xAtk, MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, 54ull);

	{ Zenith_Vector<ZM_BattleEvent> xE; ZM_MoveContext xC = MakeCtx(xState, xE, ZM_SIDE_PLAYER, 0u); ZM_MoveExecutor::Execute(xC); }
	ZENITH_ASSERT_EQ((u_int)xState.m_xField.m_eWeather, (u_int)ZM_WEATHER_SUN, "Sunflare sets Sun");
	{ Zenith_Vector<ZM_BattleEvent> xE; ZM_MoveContext xC = MakeCtx(xState, xE, ZM_SIDE_PLAYER, 1u); ZM_MoveExecutor::Execute(xC); }
	ZENITH_ASSERT_EQ((u_int)xState.m_xField.m_eWeather, (u_int)ZM_WEATHER_RAIN, "Raincall overwrites Sun with Rain");
}

// ============================================================================
// SCREEN setters -- land on the USER's side, STATE-ONLY (m_auScreenTurns==5), no
// event.
// ============================================================================
ZENITH_TEST(ZM_Battle, Screen_AegisWall_SetsPhysicalOnUserSide)
{
	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunPlayerMove(MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_AEGISWALL),
		MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, xState, xEvents);

	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_PLAYER).m_auScreenTurns[ZM_SCREEN_PHYSICAL], 5u, "physical screen set to 5 on the user side");
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_PLAYER).m_auScreenTurns[ZM_SCREEN_SPECIAL], 0u, "special screen untouched");
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_ENEMY).m_auScreenTurns[ZM_SCREEN_PHYSICAL], 0u, "the opponent's side is unaffected");
	ZENITH_ASSERT_EQ(ZM_CountKind(xEvents, ZM_BATTLE_EVENT_SCREEN_SET), 0u, "box-2 screen setter emits NO event");
}

ZENITH_TEST(ZM_Battle, Screen_LumenWall_SetsSpecialOnUserSide)
{
	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunPlayerMove(MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_LUMENWALL),
		MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, xState, xEvents);

	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_PLAYER).m_auScreenTurns[ZM_SCREEN_SPECIAL], 5u, "special screen set to 5 on the user side");
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_PLAYER).m_auScreenTurns[ZM_SCREEN_PHYSICAL], 0u, "physical screen untouched");
	ZENITH_ASSERT_EQ(ZM_CountKind(xEvents, ZM_BATTLE_EVENT_SCREEN_SET), 0u, "box-2 screen setter emits NO event");
}

// ============================================================================
// SCREEN damage reduction (bScreen) -- asserted via a HALVED DAMAGE_DEALT, never
// an event. Compared with/without the screen at the SAME seed (identical crit/
// roll). Rambash (physical) into the enemy's physical screen. Non-crit seed 1
// halves; crit seed 19 bypasses; a wrong-type (special) screen does nothing.
// ============================================================================
namespace
{
	// Run one ApplyDamagingHit of Rambash (player, physical) into a bulky enemy,
	// optionally with a screen active on the enemy side; return the raw damage and
	// (via out-param) whether the hit crit.
	u_int ZM_ApplyHitWithScreen(u_int64 ulSeed, int iScreenSlot, bool* pbCritOut)
	{
		ZM_BattleState xState;
		BuildBattleState(xState,
			MakeSpecOverride(ZM_SPECIES_NIBBIN,    20u, ZM_MOVE_RAMBASH, 200u, 60u, 40u, 40u, 40u, 100u),
			MakeSpecOverride(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH, 250u, 10u, 40u, 10u, 40u,  10u), ulSeed, 54ull);
		if (iScreenSlot >= 0) { xState.Side(ZM_SIDE_ENEMY).m_auScreenTurns[iScreenSlot] = 5u; }
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
		const u_int uDmg = ZM_MoveExecutor::ApplyDamagingHit(xCtx);
		if (pbCritOut != nullptr) { *pbCritOut = ZM_HasKind(xEvents, ZM_BATTLE_EVENT_CRIT); }
		return uDmg;
	}
}

// Non-crit seed 1: a matching physical screen halves the physical hit.
ZENITH_TEST(ZM_Battle, Screen_HalvesMatchingPhysicalDamage)
{
	bool bCritOff = true;
	const u_int uOff = ZM_ApplyHitWithScreen(1ull, -1, &bCritOff);
	const u_int uOn  = ZM_ApplyHitWithScreen(1ull, (int)ZM_SCREEN_PHYSICAL, nullptr);
	ZENITH_ASSERT_FALSE(bCritOff, "seed 1's first draw must be a non-crit for the halving to apply");
	ZENITH_ASSERT_GE(uOff, 2u, "need >= 2 base damage to observe halving");
	ZENITH_ASSERT_EQ(uOn, uOff / 2u, "a matching physical screen halves a non-crit physical hit");
}

// Crit seed 19: a crit BYPASSES the screen (screen-on == screen-off == crit dmg).
ZENITH_TEST(ZM_Battle, Screen_CritBypasses)
{
	bool bCritOff = false;
	const u_int uOff = ZM_ApplyHitWithScreen(19ull, -1, &bCritOff);
	const u_int uOn  = ZM_ApplyHitWithScreen(19ull, (int)ZM_SCREEN_PHYSICAL, nullptr);
	ZENITH_ASSERT_TRUE(bCritOff, "seed 19's first draw must be a crit for this test");
	ZENITH_ASSERT_EQ(uOn, uOff, "a crit bypasses the physical screen (crit-through-screen == crit-no-screen)");
}

// Isolation: a SPECIAL screen does not reduce a PHYSICAL hit (non-crit seed 1).
ZENITH_TEST(ZM_Battle, Screen_WrongTypeDoesNotHalve)
{
	const u_int uOff = ZM_ApplyHitWithScreen(1ull, -1, nullptr);
	const u_int uOn  = ZM_ApplyHitWithScreen(1ull, (int)ZM_SCREEN_SPECIAL, nullptr);
	ZENITH_ASSERT_EQ(uOn, uOff, "a special screen must NOT reduce a physical hit");
}

// ============================================================================
// HAZARD spikes -- land on the OPPONENT's side, cap at 3 layers, emit NO event.
// ============================================================================
ZENITH_TEST(ZM_Battle, Hazard_Caltrops_LayersCapAtThree)
{
	ZM_BattleState xState;
	BuildBattleState(xState, MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_CALTROPS),
		MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, 54ull);

	const u_int auExpect[5] = { 1u, 2u, 3u, 3u, 3u };
	for (u_int i = 0; i < 5u; ++i)
	{
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
		ZM_MoveExecutor::Execute(xCtx);
		ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_ENEMY).m_uHazardSpikeLayers, auExpect[i], "hazard layer count wrong after use %u", i + 1u);
	}
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_PLAYER).m_uHazardSpikeLayers, 0u, "the user's own side gets no spikes");
}

// A hazard setter is state-only: its whole stream is just [MOVE_USED].
ZENITH_TEST(ZM_Battle, Hazard_Caltrops_EmitsOnlyMoveUsed)
{
	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunPlayerMove(MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_CALTROPS),
		MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, xState, xEvents);
	ZENITH_ASSERT_EQ(xEvents.GetSize(), 1u, "a hazard setter emits only MOVE_USED");
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_ENEMY).m_uHazardSpikeLayers, 1u, "one layer is laid on the opponent's side");
}

// ============================================================================
// S2 box-2 SC4 -- ZM_StatusLogic majors (6) + burn/paralysis + the m_iTag POD
// append (DecisionLog ZM-D-033; box-2 plan section 3a). Every golden/number is
// cross-checked by the offline oracle (scratchpad/zm_battle_ref.py), whose box-1
// 27-event stream re-derives byte-identical WITH the appended 8th tag field == 0.
//
// SHARED EVENT ENCODINGS (the executor + ZM_StatusLogic emit these exactly):
//  * STATUS_APPLIED: m_uSide/m_uSlot = TARGET;  m_iAux = (int)ZM_MAJOR_STATUS applied.
//  * STATUS_DAMAGE:  m_uSide/m_uSlot = VICTIM;   m_iAmount = chip; m_iAux = HP
//                    remaining; **m_iTag = (int)source ZM_MAJOR_STATUS** (8th field).
//  * STATUS_CURED:   m_uSide/m_uSlot = TARGET;   m_iAux = (int)status cured.
//  * MOVE_FAILED reasons (m_iAux): ZM_MOVE_FAIL_FROZEN / ASLEEP / FULLY_PARALYZED
//    / STATUS_BLOCKED (appended after OHKO_FAILED).
//  * Q3: an action-time gate cancel (freeze/sleep/full-para) spends NO PP and emits
//    NO MOVE_USED -- a frozen mon's turn stream is just [MOVE_FAILED(FROZEN)].
// ============================================================================
namespace
{
	// Rig the player-active major status on a bare 1v1 state, then execute the
	// player's slot-0 move DIRECTLY (no engine turn loop) -- for chips/gates that
	// live in the engine, use ZM_BeginWithPlayerStatus below instead.

	// Execute a can-miss STATUS-category primary as a SURE HIT: boost the user's
	// ACCURACY stage to +6 so effAcc >= 100 (no accuracy draw, deterministic hit),
	// optionally pre-set the enemy's major status (already-statused block), then run
	// slot 0. Non-sleep statuses draw NOTHING here, so the stream is exactly
	// [MOVE_USED, STATUS_APPLIED] (apply) or [MOVE_USED, MOVE_FAILED] (blocked).
	void ZM_RunStatusPrimary(const ZM_BattleMonsterSpec& xPlayer, const ZM_BattleMonsterSpec& xEnemy,
		u_int64 ulSeed, ZM_MAJOR_STATUS eEnemyPreStatus, ZM_BattleState& xStateOut,
		Zenith_Vector<ZM_BattleEvent>& xEventsOut)
	{
		BuildBattleState(xStateOut, xPlayer, xEnemy, ulSeed, 54ull);
		xStateOut.Side(ZM_SIDE_PLAYER).Active().m_aiStage[ZM_BATTLE_STAT_ACCURACY] = iZM_MAX_STAGE;
		if (eEnemyPreStatus != ZM_MAJOR_STATUS_NONE)
		{
			xStateOut.Side(ZM_SIDE_ENEMY).Active().m_eStatus = eEnemyPreStatus;
		}
		ZM_MoveContext xCtx = MakeCtx(xStateOut, xEventsOut, ZM_SIDE_PLAYER, 0u);
		ZM_MoveExecutor::Execute(xCtx);
	}

	// A STATUS-category primary that APPLIES a major -> exactly [MOVE_USED(move),
	// STATUS_APPLIED(enemy, status)]; enemy holds the status; no MOVE_FAILED.
	void ZM_CheckStatusPrimaryApply(ZM_MOVE_ID eMove, ZM_SPECIES_ID eEnemySpecies, ZM_MAJOR_STATUS eStatus)
	{
		ZM_BattleState xState;
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_RunStatusPrimary(MakeSpec(ZM_SPECIES_NIBBIN, 20u, eMove),
			MakeSpec(eEnemySpecies, 20u, ZM_MOVE_RAMBASH), 0x1234ull, ZM_MAJOR_STATUS_NONE, xState, xEvents);

		ZENITH_ASSERT_EQ((u_int)xState.Side(ZM_SIDE_ENEMY).Active().m_eStatus, (u_int)eStatus, "enemy should hold the applied status");
		ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_FAILED), "a successful apply emits no MOVE_FAILED");
		const ZM_BattleEvent* pxApplied = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_STATUS_APPLIED);
		ZENITH_ASSERT_NOT_NULL(pxApplied, "a status primary that lands emits STATUS_APPLIED");
		if (pxApplied != nullptr)
		{
			ZENITH_ASSERT_EQ(pxApplied->m_uSide, (u_int)ZM_SIDE_ENEMY, "STATUS_APPLIED targets the enemy (the TARGET)");
			ZENITH_ASSERT_EQ(pxApplied->m_uSlot, 0u, "STATUS_APPLIED slot == target active slot");
			ZENITH_ASSERT_EQ((u_int)pxApplied->m_iAux, (u_int)eStatus, "STATUS_APPLIED m_iAux == (int)applied status");
			ZENITH_ASSERT_EQ(pxApplied->m_iTag, 0, "STATUS_APPLIED leaves the appended m_iTag at 0");
		}
	}

	// A STATUS-category primary that is BLOCKED (type-immune OR already-statused) ->
	// MOVE_FAILED(STATUS_BLOCKED), no STATUS_APPLIED, enemy status unchanged.
	void ZM_CheckStatusPrimaryBlocked(ZM_MOVE_ID eMove, ZM_SPECIES_ID eEnemySpecies,
		ZM_MAJOR_STATUS eEnemyPreStatus)
	{
		ZM_BattleState xState;
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_RunStatusPrimary(MakeSpec(ZM_SPECIES_NIBBIN, 20u, eMove),
			MakeSpec(eEnemySpecies, 20u, ZM_MOVE_RAMBASH), 0x1234ull, eEnemyPreStatus, xState, xEvents);

		ZENITH_ASSERT_EQ((u_int)xState.Side(ZM_SIDE_ENEMY).Active().m_eStatus, (u_int)eEnemyPreStatus, "a blocked apply must leave the enemy's status unchanged");
		ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_STATUS_APPLIED), "a blocked primary emits no STATUS_APPLIED");
		const ZM_BattleEvent* pxFail = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_MOVE_FAILED);
		ZENITH_ASSERT_NOT_NULL(pxFail, "a blocked status PRIMARY emits MOVE_FAILED(STATUS_BLOCKED)");
		if (pxFail != nullptr)
		{
			ZENITH_ASSERT_EQ((u_int)pxFail->m_iAux, (u_int)ZM_MOVE_FAIL_STATUS_BLOCKED, "MOVE_FAILED reason == STATUS_BLOCKED");
		}
	}

	// Begin a 1v1 battle and rig the player-active major status + counter BEFORE
	// resolving -- the box-1-safe way to reach the engine's action-time gate + the
	// end-of-turn chip (both live in the engine, not the isolated executor).
	void ZM_BeginWithPlayerStatus(ZM_BattleEngine& xEngine, const ZM_BattleMonsterSpec& xPlayer,
		const ZM_BattleMonsterSpec& xEnemy, u_int64 ulSeed, ZM_MAJOR_STATUS eStatus, u_int uCounter)
	{
		ZM_BattleMonsterSpec axP[1] = { xPlayer };
		ZM_BattleMonsterSpec axE[1] = { xEnemy };
		xEngine.Begin(MakeTrainerConfig(), axP, 1u, axE, 1u, ulSeed, 54ull);
		ZM_BattleMonster& xMon = xEngine.GetStateMutable().Side(ZM_SIDE_PLAYER).Active();
		xMon.m_eStatus = eStatus;
		xMon.m_uStatusCounter = uCounter;
	}

	// Does any MOVE_USED for the given side appear in the stream?
	bool ZM_HasMoveUsedForSide(const Zenith_Vector<ZM_BattleEvent>& xEvents, ZM_SIDE eSide)
	{
		for (u_int i = 0; i < xEvents.GetSize(); ++i)
		{
			const ZM_BattleEvent& x = xEvents.Get(i);
			if (x.m_eKind == ZM_BATTLE_EVENT_MOVE_USED && x.m_uSide == (u_int)eSide) { return true; }
		}
		return false;
	}

	// First MOVE_FAILED for a side (or nullptr).
	const ZM_BattleEvent* ZM_FindMoveFailedForSide(const Zenith_Vector<ZM_BattleEvent>& xEvents, ZM_SIDE eSide)
	{
		for (u_int i = 0; i < xEvents.GetSize(); ++i)
		{
			const ZM_BattleEvent& x = xEvents.Get(i);
			if (x.m_eKind == ZM_BATTLE_EVENT_MOVE_FAILED && x.m_uSide == (u_int)eSide) { return &xEvents.Get(i); }
		}
		return nullptr;
	}

	// A very fast Rambash player (acts first even under the paralysis 1/4 cut) vs a
	// bulky enemy holding a self-move (Whetclaw) so the enemy never damages the
	// player -- used to isolate the player's action-time gate.
	ZM_BattleMonsterSpec ZM_GateFastPlayer(ZM_MOVE_ID eMove)
	{
		return MakeSpecOverride(ZM_SPECIES_NIBBIN, 50u, eMove, 200u, 40u, 200u, 40u, 200u, 200u);
	}
	ZM_BattleMonsterSpec ZM_GateBulkyEnemy()
	{
		return MakeSpecOverride(ZM_SPECIES_STRAYLING, 50u, ZM_MOVE_WHETCLAW, 250u, 10u, 250u, 10u, 250u, 10u);
	}
}

// ---- APPLY: a primary STATUS move (or freeze secondary) -> STATUS_APPLIED ------
ZENITH_TEST(ZM_Battle, Burn_Apply_Hexflame)
{
	ZM_CheckStatusPrimaryApply(ZM_MOVE_HEXFLAME, ZM_SPECIES_STRAYLING, ZM_MAJOR_STATUS_BURN);
}
ZENITH_TEST(ZM_Battle, Paralyze_Apply_StaticSnare)
{
	ZM_CheckStatusPrimaryApply(ZM_MOVE_STATICSNARE, ZM_SPECIES_STRAYLING, ZM_MAJOR_STATUS_PARALYSIS);
}
ZENITH_TEST(ZM_Battle, Poison_Apply_VenomCoat)
{
	ZM_CheckStatusPrimaryApply(ZM_MOVE_VENOMCOAT, ZM_SPECIES_STRAYLING, ZM_MAJOR_STATUS_POISON);
}
ZENITH_TEST(ZM_Battle, Toxic_Apply_Blightdose)
{
	ZM_CheckStatusPrimaryApply(ZM_MOVE_BLIGHTDOSE, ZM_SPECIES_STRAYLING, ZM_MAJOR_STATUS_TOXIC);
}
ZENITH_TEST(ZM_Battle, Sleep_Apply_Lullaby)
{
	ZM_CheckStatusPrimaryApply(ZM_MOVE_LULLABY, ZM_SPECIES_STRAYLING, ZM_MAJOR_STATUS_SLEEP);
}

// TOXIC apply seeds the ramp counter at 1 (no draw); SLEEP apply draws a duration
// RandRange(1,3) -> counter in [1,3]. (The oracle confirms these apply-time draws.)
ZENITH_TEST(ZM_Battle, Toxic_Apply_CounterStartsAtOne)
{
	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunStatusPrimary(MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_BLIGHTDOSE),
		MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, ZM_MAJOR_STATUS_NONE, xState, xEvents);
	const ZM_BattleMonster& xEnemy = xState.Side(ZM_SIDE_ENEMY).Active();
	ZENITH_ASSERT_EQ((u_int)xEnemy.m_eStatus, (u_int)ZM_MAJOR_STATUS_TOXIC);
	ZENITH_ASSERT_EQ(xEnemy.m_uStatusCounter, 1u, "toxic ramp counter starts at 1");
}
ZENITH_TEST(ZM_Battle, Sleep_Apply_DrawsDurationInRange)
{
	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunStatusPrimary(MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_LULLABY),
		MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, ZM_MAJOR_STATUS_NONE, xState, xEvents);
	const ZM_BattleMonster& xEnemy = xState.Side(ZM_SIDE_ENEMY).Active();
	ZENITH_ASSERT_EQ((u_int)xEnemy.m_eStatus, (u_int)ZM_MAJOR_STATUS_SLEEP);
	ZENITH_ASSERT_GE(xEnemy.m_uStatusCounter, 1u, "sleep duration >= 1");
	ZENITH_ASSERT_LE(xEnemy.m_uStatusCounter, 3u, "sleep duration <= 3 (RandRange(1,3))");
}

// FREEZE has NO status-category carrier: its apply vehicle is a damaging FREEZE
// secondary (Frostnip, chance 10). Seed 15 forces the proc (oracle-derived); a
// bulky non-ICE defender survives so the freeze is observable.
ZENITH_TEST(ZM_Battle, Freeze_Apply_FrostnipSecondary)
{
	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunPlayerMove(MakeSpecOverride(ZM_SPECIES_NIBBIN,    20u, ZM_MOVE_FROSTNIP, 80u, 40u, 40u, 10u,  40u, 60u),
		MakeSpecOverride(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH,  250u, 10u, 40u, 10u, 250u, 10u), 15ull, xState, xEvents);

	ZENITH_ASSERT_GT(xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP, 0u, "defender must survive to observe the freeze");
	ZENITH_ASSERT_EQ((u_int)xState.Side(ZM_SIDE_ENEMY).Active().m_eStatus, (u_int)ZM_MAJOR_STATUS_FREEZE, "the freeze secondary should proc + apply at seed 15");
	const ZM_BattleEvent* pxApplied = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_STATUS_APPLIED);
	ZENITH_ASSERT_NOT_NULL(pxApplied, "a landing freeze secondary emits STATUS_APPLIED");
	if (pxApplied != nullptr) { ZENITH_ASSERT_EQ((u_int)pxApplied->m_iAux, (u_int)ZM_MAJOR_STATUS_FREEZE); }
	// The secondary runs AFTER the damaging hit.
	ZENITH_ASSERT_TRUE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT), "Frostnip is a damaging move");
}

// A BURN secondary on a damaging fire move lands too (Emberclaw into a non-FIRE
// target, seed 15 forces the proc).
ZENITH_TEST(ZM_Battle, Burn_Apply_EmberclawSecondary)
{
	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunPlayerMove(MakeSpecOverride(ZM_SPECIES_NIBBIN,    20u, ZM_MOVE_EMBERCLAW, 80u, 30u,  40u, 40u, 40u, 60u),
		MakeSpecOverride(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH,  250u, 10u, 250u, 10u, 40u, 10u), 15ull, xState, xEvents);
	ZENITH_ASSERT_GT(xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP, 0u, "defender must survive to observe the burn");
	ZENITH_ASSERT_EQ((u_int)xState.Side(ZM_SIDE_ENEMY).Active().m_eStatus, (u_int)ZM_MAJOR_STATUS_BURN);
	ZENITH_ASSERT_TRUE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_STATUS_APPLIED));
}

// ---- BLOCK: type-immune (primary MOVE_FAILED(STATUS_BLOCKED)) ------------------
// BURN blocked on FIRE, POISON/TOXIC blocked on VENOM|IRON, FREEZE blocked on ICE.
ZENITH_TEST(ZM_Battle, Burn_BlockedOnFire_Primary)
{
	ZM_CheckStatusPrimaryBlocked(ZM_MOVE_HEXFLAME, ZM_SPECIES_KINDLET, ZM_MAJOR_STATUS_NONE);
}
ZENITH_TEST(ZM_Battle, Poison_BlockedOnVenom_Primary)
{
	ZM_CheckStatusPrimaryBlocked(ZM_MOVE_VENOMCOAT, ZM_SPECIES_OOZEL, ZM_MAJOR_STATUS_NONE);
}
ZENITH_TEST(ZM_Battle, Poison_BlockedOnIron_Primary)
{
	ZM_CheckStatusPrimaryBlocked(ZM_MOVE_VENOMCOAT, ZM_SPECIES_SLAGLET, ZM_MAJOR_STATUS_NONE);
}
ZENITH_TEST(ZM_Battle, Toxic_BlockedOnVenom_Primary)
{
	ZM_CheckStatusPrimaryBlocked(ZM_MOVE_BLIGHTDOSE, ZM_SPECIES_OOZEL, ZM_MAJOR_STATUS_NONE);
}

// ---- BLOCK: type-immune SECONDARY is SILENT (no MOVE_FAILED, no STATUS_APPLIED) -
// Paired with the *_Apply_*Secondary tests above: SAME move + SAME seed 15 (which
// forces the proc); into an IMMUNE-typed target the proc'd status is blocked and
// the move stays SILENT -- it still deals its (reduced) damage.
ZENITH_TEST(ZM_Battle, Burn_SecondarySilentOnFire)
{
	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunPlayerMove(MakeSpecOverride(ZM_SPECIES_NIBBIN,   20u, ZM_MOVE_EMBERCLAW, 80u, 30u,  40u, 40u, 40u, 60u),
		MakeSpecOverride(ZM_SPECIES_KINDLET, 20u, ZM_MOVE_RAMBASH,  250u, 10u, 250u, 10u, 40u, 10u), 15ull, xState, xEvents);
	ZENITH_ASSERT_GT(xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP, 0u, "the FIRE defender must survive");
	ZENITH_ASSERT_EQ((u_int)xState.Side(ZM_SIDE_ENEMY).Active().m_eStatus, (u_int)ZM_MAJOR_STATUS_NONE, "burn is blocked on a FIRE target");
	ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_STATUS_APPLIED), "a blocked secondary is silent");
	ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_FAILED), "a blocked SECONDARY emits no MOVE_FAILED");
	ZENITH_ASSERT_TRUE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT), "the move still connects (FIRE vs FIRE = 0.5x, not immune)");
}
ZENITH_TEST(ZM_Battle, Freeze_SecondarySilentOnIce)
{
	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunPlayerMove(MakeSpecOverride(ZM_SPECIES_NIBBIN,   20u, ZM_MOVE_FROSTNIP, 80u, 40u, 40u, 10u,  40u, 60u),
		MakeSpecOverride(ZM_SPECIES_FRISKET, 20u, ZM_MOVE_RAMBASH, 250u, 10u, 40u, 10u, 250u, 10u), 15ull, xState, xEvents);
	ZENITH_ASSERT_GT(xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP, 0u, "the ICE defender must survive");
	ZENITH_ASSERT_EQ((u_int)xState.Side(ZM_SIDE_ENEMY).Active().m_eStatus, (u_int)ZM_MAJOR_STATUS_NONE, "freeze is blocked on an ICE target");
	ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_STATUS_APPLIED), "a blocked secondary is silent");
	ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_FAILED), "a blocked SECONDARY emits no MOVE_FAILED");
}

// ---- No type block for SLEEP / PARALYSIS (they apply to any type) --------------
ZENITH_TEST(ZM_Battle, Paralyze_NotTypeBlocked)
{
	ZM_CheckStatusPrimaryApply(ZM_MOVE_STATICSNARE, ZM_SPECIES_SPARKIT, ZM_MAJOR_STATUS_PARALYSIS);
}
ZENITH_TEST(ZM_Battle, Sleep_NotTypeBlocked)
{
	ZM_CheckStatusPrimaryApply(ZM_MOVE_LULLABY, ZM_SPECIES_SPARKIT, ZM_MAJOR_STATUS_SLEEP);
}

// ---- BLOCK: already-statused (any existing major blocks a new one) -------------
// Enemy is a plain NORMAL (no type immunity), pre-loaded with a DIFFERENT major.
ZENITH_TEST(ZM_Battle, Burn_BlockedWhenAlreadyStatused)
{
	ZM_CheckStatusPrimaryBlocked(ZM_MOVE_HEXFLAME, ZM_SPECIES_STRAYLING, ZM_MAJOR_STATUS_PARALYSIS);
}
ZENITH_TEST(ZM_Battle, Paralyze_BlockedWhenAlreadyStatused)
{
	ZM_CheckStatusPrimaryBlocked(ZM_MOVE_STATICSNARE, ZM_SPECIES_STRAYLING, ZM_MAJOR_STATUS_BURN);
}
ZENITH_TEST(ZM_Battle, Poison_BlockedWhenAlreadyStatused)
{
	ZM_CheckStatusPrimaryBlocked(ZM_MOVE_VENOMCOAT, ZM_SPECIES_STRAYLING, ZM_MAJOR_STATUS_SLEEP);
}
ZENITH_TEST(ZM_Battle, Toxic_BlockedWhenAlreadyStatused)
{
	ZM_CheckStatusPrimaryBlocked(ZM_MOVE_BLIGHTDOSE, ZM_SPECIES_STRAYLING, ZM_MAJOR_STATUS_BURN);
}
ZENITH_TEST(ZM_Battle, Sleep_BlockedWhenAlreadyStatused)
{
	ZM_CheckStatusPrimaryBlocked(ZM_MOVE_LULLABY, ZM_SPECIES_STRAYLING, ZM_MAJOR_STATUS_POISON);
}

// ============================================================================
// ACTION-TIME GATES (freeze/sleep/paralysis) -- run in the engine BEFORE PP +
// MOVE_USED. A cancel spends NO PP and emits NO MOVE_USED (Q3). Gate rolls:
// FREEZE thaw RandBelow(100)<20; PARALYSIS full-para RandBelow(100)<25 (both the
// FIRST RNG pull, so seed_low=2 -> v=4 fires, seed_high=1 -> v=77 does not --
// oracle-derived). SLEEP is counter-driven (0 draws): decrement-then-check.
// ============================================================================

// FROZEN + stays frozen (seed_high=1): stream is just [MOVE_FAILED(FROZEN)] for
// the player -- NO MOVE_USED, PP untouched, still frozen (Q3).
ZENITH_TEST(ZM_Battle, Freeze_Gate_StaysFrozen_NoMoveUsed)
{
	ZM_BattleEngine xEngine;
	ZM_BeginWithPlayerStatus(xEngine, ZM_GateFastPlayer(ZM_MOVE_RAMBASH), ZM_GateBulkyEnemy(),
		1ull, ZM_MAJOR_STATUS_FREEZE, 0u);
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
	xEngine.ResolveTurn();

	const Zenith_Vector<ZM_BattleEvent>& xEvents = xEngine.GetEvents();
	ZENITH_ASSERT_FALSE(ZM_HasMoveUsedForSide(xEvents, ZM_SIDE_PLAYER), "Q3: a frozen (stay) turn emits NO MOVE_USED for the player");
	const ZM_BattleEvent* pxFail = ZM_FindMoveFailedForSide(xEvents, ZM_SIDE_PLAYER);
	ZENITH_ASSERT_NOT_NULL(pxFail, "a stay-frozen turn emits MOVE_FAILED(FROZEN)");
	if (pxFail != nullptr) { ZENITH_ASSERT_EQ((u_int)pxFail->m_iAux, (u_int)ZM_MOVE_FAIL_FROZEN, "reason == FROZEN"); }
	const ZM_BattleMonster& xPlayer = xEngine.GetState().Side(ZM_SIDE_PLAYER).Active();
	ZENITH_ASSERT_EQ((u_int)xPlayer.m_eStatus, (u_int)ZM_MAJOR_STATUS_FREEZE, "still frozen after a stay");
	ZENITH_ASSERT_EQ(xPlayer.m_axMoves[0].m_uCurPP, xPlayer.m_axMoves[0].m_uMaxPP, "Q3: a gate cancel spends NO PP");
}

// FROZEN + thaws (seed_low=2, v=4<20): STATUS_CURED(FREEZE), then the mon acts
// (MOVE_USED present), status cleared, PP spent.
ZENITH_TEST(ZM_Battle, Freeze_Gate_ThawsAndActs)
{
	ZM_BattleEngine xEngine;
	ZM_BeginWithPlayerStatus(xEngine, ZM_GateFastPlayer(ZM_MOVE_RAMBASH), ZM_GateBulkyEnemy(),
		2ull, ZM_MAJOR_STATUS_FREEZE, 0u);
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
	xEngine.ResolveTurn();

	const Zenith_Vector<ZM_BattleEvent>& xEvents = xEngine.GetEvents();
	const ZM_BattleEvent* pxCured = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_STATUS_CURED);
	ZENITH_ASSERT_NOT_NULL(pxCured, "a thaw emits STATUS_CURED(FREEZE)");
	if (pxCured != nullptr)
	{
		ZENITH_ASSERT_EQ(pxCured->m_uSide, (u_int)ZM_SIDE_PLAYER);
		ZENITH_ASSERT_EQ((u_int)pxCured->m_iAux, (u_int)ZM_MAJOR_STATUS_FREEZE, "cured status == FREEZE");
	}
	ZENITH_ASSERT_TRUE(ZM_HasMoveUsedForSide(xEvents, ZM_SIDE_PLAYER), "a thawed mon proceeds to use its move");
	const ZM_BattleMonster& xPlayer = xEngine.GetState().Side(ZM_SIDE_PLAYER).Active();
	ZENITH_ASSERT_EQ((u_int)xPlayer.m_eStatus, (u_int)ZM_MAJOR_STATUS_NONE, "thaw clears freeze");
	ZENITH_ASSERT_EQ(xPlayer.m_axMoves[0].m_uCurPP, xPlayer.m_axMoves[0].m_uMaxPP - 1u, "a mon that acts spends 1 PP");
}

// ASLEEP + stays asleep (counter 2 -> decrement to 1 > 0): MOVE_FAILED(ASLEEP),
// no MOVE_USED, PP untouched, counter now 1.
ZENITH_TEST(ZM_Battle, Sleep_Gate_StaysAsleep_CounterDecrements)
{
	ZM_BattleEngine xEngine;
	ZM_BeginWithPlayerStatus(xEngine, ZM_GateFastPlayer(ZM_MOVE_RAMBASH), ZM_GateBulkyEnemy(),
		1ull, ZM_MAJOR_STATUS_SLEEP, 2u);
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
	xEngine.ResolveTurn();

	const Zenith_Vector<ZM_BattleEvent>& xEvents = xEngine.GetEvents();
	ZENITH_ASSERT_FALSE(ZM_HasMoveUsedForSide(xEvents, ZM_SIDE_PLAYER), "Q3: an asleep turn emits NO MOVE_USED");
	const ZM_BattleEvent* pxFail = ZM_FindMoveFailedForSide(xEvents, ZM_SIDE_PLAYER);
	ZENITH_ASSERT_NOT_NULL(pxFail, "a stay-asleep turn emits MOVE_FAILED(ASLEEP)");
	if (pxFail != nullptr) { ZENITH_ASSERT_EQ((u_int)pxFail->m_iAux, (u_int)ZM_MOVE_FAIL_ASLEEP, "reason == ASLEEP"); }
	const ZM_BattleMonster& xPlayer = xEngine.GetState().Side(ZM_SIDE_PLAYER).Active();
	ZENITH_ASSERT_EQ((u_int)xPlayer.m_eStatus, (u_int)ZM_MAJOR_STATUS_SLEEP, "still asleep");
	ZENITH_ASSERT_EQ(xPlayer.m_uStatusCounter, 1u, "decrement-then-check: 2 -> 1");
	ZENITH_ASSERT_EQ(xPlayer.m_axMoves[0].m_uCurPP, xPlayer.m_axMoves[0].m_uMaxPP, "Q3: no PP spent");
}

// ASLEEP + wakes (counter 1 -> decrement to 0): STATUS_CURED(SLEEP) + acts.
ZENITH_TEST(ZM_Battle, Sleep_Gate_WakesOnCounterZero)
{
	ZM_BattleEngine xEngine;
	ZM_BeginWithPlayerStatus(xEngine, ZM_GateFastPlayer(ZM_MOVE_RAMBASH), ZM_GateBulkyEnemy(),
		1ull, ZM_MAJOR_STATUS_SLEEP, 1u);
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
	xEngine.ResolveTurn();

	const Zenith_Vector<ZM_BattleEvent>& xEvents = xEngine.GetEvents();
	const ZM_BattleEvent* pxCured = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_STATUS_CURED);
	ZENITH_ASSERT_NOT_NULL(pxCured, "waking emits STATUS_CURED(SLEEP)");
	if (pxCured != nullptr) { ZENITH_ASSERT_EQ((u_int)pxCured->m_iAux, (u_int)ZM_MAJOR_STATUS_SLEEP); }
	ZENITH_ASSERT_TRUE(ZM_HasMoveUsedForSide(xEvents, ZM_SIDE_PLAYER), "a woken mon proceeds to use its move");
	ZENITH_ASSERT_EQ((u_int)xEngine.GetState().Side(ZM_SIDE_PLAYER).Active().m_eStatus, (u_int)ZM_MAJOR_STATUS_NONE, "wake clears sleep");
}

// PARALYSIS + fully paralyzed (seed_low=2, v=4<25): MOVE_FAILED(FULLY_PARALYZED),
// no MOVE_USED, PP untouched. Player is fast enough to act first even at 1/4 speed.
ZENITH_TEST(ZM_Battle, Paralysis_Gate_FullyParalyzed_NoMoveUsed)
{
	ZM_BattleEngine xEngine;
	ZM_BeginWithPlayerStatus(xEngine, ZM_GateFastPlayer(ZM_MOVE_RAMBASH), ZM_GateBulkyEnemy(),
		2ull, ZM_MAJOR_STATUS_PARALYSIS, 0u);
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
	xEngine.ResolveTurn();

	const Zenith_Vector<ZM_BattleEvent>& xEvents = xEngine.GetEvents();
	ZENITH_ASSERT_FALSE(ZM_HasMoveUsedForSide(xEvents, ZM_SIDE_PLAYER), "Q3: a full-para turn emits NO MOVE_USED");
	const ZM_BattleEvent* pxFail = ZM_FindMoveFailedForSide(xEvents, ZM_SIDE_PLAYER);
	ZENITH_ASSERT_NOT_NULL(pxFail, "a full-para turn emits MOVE_FAILED(FULLY_PARALYZED)");
	if (pxFail != nullptr) { ZENITH_ASSERT_EQ((u_int)pxFail->m_iAux, (u_int)ZM_MOVE_FAIL_FULLY_PARALYZED, "reason == FULLY_PARALYZED"); }
	const ZM_BattleMonster& xPlayer = xEngine.GetState().Side(ZM_SIDE_PLAYER).Active();
	ZENITH_ASSERT_EQ((u_int)xPlayer.m_eStatus, (u_int)ZM_MAJOR_STATUS_PARALYSIS, "paralysis persists (permanent until cured)");
	ZENITH_ASSERT_EQ(xPlayer.m_axMoves[0].m_uCurPP, xPlayer.m_axMoves[0].m_uMaxPP, "Q3: no PP spent");
}

// PARALYSIS + acts (seed_high=1, v=77>=25): MOVE_USED present, no MOVE_FAILED, PP spent.
ZENITH_TEST(ZM_Battle, Paralysis_Gate_ActsWhenNotFullyParalyzed)
{
	ZM_BattleEngine xEngine;
	ZM_BeginWithPlayerStatus(xEngine, ZM_GateFastPlayer(ZM_MOVE_RAMBASH), ZM_GateBulkyEnemy(),
		1ull, ZM_MAJOR_STATUS_PARALYSIS, 0u);
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
	xEngine.ResolveTurn();

	const Zenith_Vector<ZM_BattleEvent>& xEvents = xEngine.GetEvents();
	ZENITH_ASSERT_TRUE(ZM_HasMoveUsedForSide(xEvents, ZM_SIDE_PLAYER), "a paralyzed mon that passes the gate still acts");
	ZENITH_ASSERT_NULL(ZM_FindMoveFailedForSide(xEvents, ZM_SIDE_PLAYER), "no MOVE_FAILED when the para roll passes");
	const ZM_BattleMonster& xPlayer = xEngine.GetState().Side(ZM_SIDE_PLAYER).Active();
	ZENITH_ASSERT_EQ(xPlayer.m_axMoves[0].m_uCurPP, xPlayer.m_axMoves[0].m_uMaxPP - 1u, "a mon that acts spends 1 PP");
}

// ============================================================================
// END-OF-TURN CHIPS (POISON/BURN=maxHP/8, TOXIC ramp=maxHP*n/16). Chip target =
// the player; both mons hold a SELF-move (Whetclaw) so the player takes NO move
// damage and remaining HP == maxHP - accumulated chips. Nibbin base HP 100 @L30
// -> maxHP 109 (oracle): burn/poison chip 13 (rem 96); toxic ramp 6/13/20.
// ============================================================================
namespace
{
	ZM_BattleMonsterSpec ZM_ChipPlayer()   { return MakeSpecOverride(ZM_SPECIES_NIBBIN,    30u, ZM_MOVE_WHETCLAW, 100u, 40u, 40u, 40u, 40u, 60u); }
	ZM_BattleMonsterSpec ZM_ChipEnemy()    { return MakeSpecOverride(ZM_SPECIES_STRAYLING, 30u, ZM_MOVE_WHETCLAW, 100u, 40u, 40u, 40u, 40u, 40u); }

	// First player-side STATUS_DAMAGE in the stream (or nullptr).
	const ZM_BattleEvent* ZM_FindStatusDamageForSide(const Zenith_Vector<ZM_BattleEvent>& xEvents, ZM_SIDE eSide)
	{
		for (u_int i = 0; i < xEvents.GetSize(); ++i)
		{
			const ZM_BattleEvent& x = xEvents.Get(i);
			if (x.m_eKind == ZM_BATTLE_EVENT_STATUS_DAMAGE && x.m_uSide == (u_int)eSide) { return &xEvents.Get(i); }
		}
		return nullptr;
	}
}

ZENITH_TEST(ZM_Battle, Burn_TurnEndChip_MaxHpOverEight)
{
	ZM_BattleEngine xEngine;
	ZM_BeginWithPlayerStatus(xEngine, ZM_ChipPlayer(), ZM_ChipEnemy(), 0x1234ull, ZM_MAJOR_STATUS_BURN, 0u);
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
	xEngine.ResolveTurn();

	const ZM_BattleEvent* pxChip = ZM_FindStatusDamageForSide(xEngine.GetEvents(), ZM_SIDE_PLAYER);
	ZENITH_ASSERT_NOT_NULL(pxChip, "a burned mon takes an end-of-turn STATUS_DAMAGE chip");
	if (pxChip != nullptr)
	{
		ZENITH_ASSERT_EQ(pxChip->m_iAmount, 13, "burn chip == maxHP/8 == 109/8 == 13");
		ZENITH_ASSERT_EQ(pxChip->m_iAux, 96, "remaining HP == 109 - 13");
		ZENITH_ASSERT_EQ(pxChip->m_iTag, (int)ZM_MAJOR_STATUS_BURN, "STATUS_DAMAGE m_iTag (8th field) == source status BURN");
	}
}

ZENITH_TEST(ZM_Battle, Poison_TurnEndChip_MaxHpOverEight)
{
	ZM_BattleEngine xEngine;
	ZM_BeginWithPlayerStatus(xEngine, ZM_ChipPlayer(), ZM_ChipEnemy(), 0x1234ull, ZM_MAJOR_STATUS_POISON, 0u);
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
	xEngine.ResolveTurn();

	const ZM_BattleEvent* pxChip = ZM_FindStatusDamageForSide(xEngine.GetEvents(), ZM_SIDE_PLAYER);
	ZENITH_ASSERT_NOT_NULL(pxChip, "a poisoned mon takes an end-of-turn STATUS_DAMAGE chip");
	if (pxChip != nullptr)
	{
		ZENITH_ASSERT_EQ(pxChip->m_iAmount, 13, "poison chip == maxHP/8 == 13");
		ZENITH_ASSERT_EQ(pxChip->m_iAux, 96, "remaining HP == 96");
		ZENITH_ASSERT_EQ(pxChip->m_iTag, (int)ZM_MAJOR_STATUS_POISON, "m_iTag == POISON");
	}
}

// TOXIC ramp over 3 turns: maxHP*1/16, *2/16, *3/16 -> 6, 13, 20 (Nibbin maxHP 109).
ZENITH_TEST(ZM_Battle, Toxic_TurnEndChip_RampsOverThreeTurns)
{
	ZM_BattleEngine xEngine;
	ZM_BeginWithPlayerStatus(xEngine, ZM_ChipPlayer(), ZM_ChipEnemy(), 0x1234ull, ZM_MAJOR_STATUS_TOXIC, 1u);

	const int aiExpectChip[3] = { 6, 13, 20 };
	const int aiExpectRem[3]  = { 103, 90, 70 };
	u_int uSeen = 0u;
	for (u_int uTurn = 0; uTurn < 3u; ++uTurn)
	{
		const u_int uBefore = xEngine.GetEventCount();
		xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
		xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
		xEngine.ResolveTurn();

		// The STATUS_DAMAGE appended THIS turn (scan only the new tail).
		const ZM_BattleEvent* pxChip = nullptr;
		for (u_int i = uBefore; i < xEngine.GetEventCount(); ++i)
		{
			const ZM_BattleEvent& x = xEngine.GetEvent(i);
			if (x.m_eKind == ZM_BATTLE_EVENT_STATUS_DAMAGE && x.m_uSide == (u_int)ZM_SIDE_PLAYER) { pxChip = &xEngine.GetEvent(i); }
		}
		ZENITH_ASSERT_NOT_NULL(pxChip, "toxic chips every turn");
		if (pxChip != nullptr)
		{
			ZENITH_ASSERT_EQ(pxChip->m_iAmount, aiExpectChip[uTurn], "toxic ramp chip turn %u", uTurn + 1u);
			ZENITH_ASSERT_EQ(pxChip->m_iAux, aiExpectRem[uTurn], "toxic remaining HP turn %u", uTurn + 1u);
			ZENITH_ASSERT_EQ(pxChip->m_iTag, (int)ZM_MAJOR_STATUS_TOXIC, "m_iTag == TOXIC");
			++uSeen;
		}
	}
	ZENITH_ASSERT_EQ(uSeen, 3u, "three toxic chips over three turns");
	ZENITH_ASSERT_EQ(xEngine.GetState().Side(ZM_SIDE_PLAYER).Active().m_uStatusCounter, 4u, "ramp counter 1->4 after three chips");
}

// FREEZE / SLEEP / PARALYSIS never chip at end of turn.
ZENITH_TEST(ZM_Battle, Freeze_NoTurnEndChip)
{
	ZM_BattleEngine xEngine;
	ZM_BeginWithPlayerStatus(xEngine, ZM_ChipPlayer(), ZM_ChipEnemy(), 0x1234ull, ZM_MAJOR_STATUS_FREEZE, 0u);
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
	xEngine.ResolveTurn();
	ZENITH_ASSERT_EQ(ZM_CountKind(xEngine.GetEvents(), ZM_BATTLE_EVENT_STATUS_DAMAGE), 0u, "freeze does not chip");
}
ZENITH_TEST(ZM_Battle, Sleep_NoTurnEndChip)
{
	ZM_BattleEngine xEngine;
	ZM_BeginWithPlayerStatus(xEngine, ZM_ChipPlayer(), ZM_ChipEnemy(), 0x1234ull, ZM_MAJOR_STATUS_SLEEP, 5u);
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
	xEngine.ResolveTurn();
	ZENITH_ASSERT_EQ(ZM_CountKind(xEngine.GetEvents(), ZM_BATTLE_EVENT_STATUS_DAMAGE), 0u, "sleep does not chip");
}
ZENITH_TEST(ZM_Battle, Paralysis_NoTurnEndChip)
{
	ZM_BattleEngine xEngine;
	ZM_BeginWithPlayerStatus(xEngine, ZM_ChipPlayer(), ZM_ChipEnemy(), 0x1234ull, ZM_MAJOR_STATUS_PARALYSIS, 0u);
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
	xEngine.ResolveTurn();
	ZENITH_ASSERT_EQ(ZM_CountKind(xEngine.GetEvents(), ZM_BATTLE_EVENT_STATUS_DAMAGE), 0u, "paralysis does not chip");
}

// ============================================================================
// CURE paths -- CURE_STATUS (self) / HEAL_BELL (party-wide) / REST.
// ============================================================================
namespace
{
	// Rig the player's major status on a bare state, then execute the player's
	// self-targeted cure move (acc 0 -> always hits, no accuracy draw).
	void ZM_CheckCureStatus(ZM_MAJOR_STATUS ePreStatus)
	{
		ZM_BattleState xState;
		BuildBattleState(xState, MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_SHAKEOFF),
			MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, 54ull);
		xState.Side(ZM_SIDE_PLAYER).Active().m_eStatus = ePreStatus;

		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
		ZM_MoveExecutor::Execute(xCtx);

		ZENITH_ASSERT_EQ((u_int)xState.Side(ZM_SIDE_PLAYER).Active().m_eStatus, (u_int)ZM_MAJOR_STATUS_NONE, "CURE_STATUS clears the user's major");
		const ZM_BattleEvent* pxCured = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_STATUS_CURED);
		ZENITH_ASSERT_NOT_NULL(pxCured, "CURE_STATUS emits STATUS_CURED");
		if (pxCured != nullptr)
		{
			ZENITH_ASSERT_EQ(pxCured->m_uSide, (u_int)ZM_SIDE_PLAYER, "STATUS_CURED targets the user");
			ZENITH_ASSERT_EQ((u_int)pxCured->m_iAux, (u_int)ePreStatus, "STATUS_CURED m_iAux == the cured status");
		}
	}
}

ZENITH_TEST(ZM_Battle, CureStatus_ShakeOff_CuresBurn)
{
	ZM_CheckCureStatus(ZM_MAJOR_STATUS_BURN);
}
ZENITH_TEST(ZM_Battle, CureStatus_ShakeOff_CuresParalysis)
{
	ZM_CheckCureStatus(ZM_MAJOR_STATUS_PARALYSIS);
}

// HEAL_BELL (Chime Cure) cures the WHOLE party -- build a 2-mon player party, both
// statused, execute Chime Cure -> both cleared.
ZENITH_TEST(ZM_Battle, HealBell_ChimeCure_CuresWholeParty)
{
	ZM_BattleState xState;
	ZM_BattleSide& xPlayer = xState.Side(ZM_SIDE_PLAYER);
	ZM_BattleSide& xEnemy  = xState.Side(ZM_SIDE_ENEMY);
	xPlayer.m_xParty.Clear();
	xEnemy.m_xParty.Clear();
	xPlayer.m_xParty.PushBack(ZM_BuildBattleMonster(MakeSpec(ZM_SPECIES_NIBBIN,    20u, ZM_MOVE_CHIMECURE)));
	xPlayer.m_xParty.PushBack(ZM_BuildBattleMonster(MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH)));
	xEnemy.m_xParty.PushBack(ZM_BuildBattleMonster(MakeSpec(ZM_SPECIES_STRAYLING,  20u, ZM_MOVE_RAMBASH)));
	xPlayer.m_uActiveSlot = 0u;
	xEnemy.m_uActiveSlot  = 0u;
	xState.m_xField = ZM_FieldState();
	xState.m_xRNG.Seed(0x1234ull, 54ull);
	xPlayer.m_xParty.Get(0).m_eStatus = ZM_MAJOR_STATUS_BURN;
	xPlayer.m_xParty.Get(1).m_eStatus = ZM_MAJOR_STATUS_POISON;

	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);

	// The party-wide proof is that the RESERVE member's state goes NONE too.
	ZENITH_ASSERT_EQ((u_int)xPlayer.m_xParty.Get(0).m_eStatus, (u_int)ZM_MAJOR_STATUS_NONE, "HEAL_BELL cures the active party member");
	ZENITH_ASSERT_EQ((u_int)xPlayer.m_xParty.Get(1).m_eStatus, (u_int)ZM_MAJOR_STATUS_NONE, "HEAL_BELL cures the RESERVE party member (party-wide)");
	ZENITH_ASSERT_TRUE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_STATUS_CURED), "HEAL_BELL emits STATUS_CURED");
}

// REST: full heal + self-SLEEP counter 2 (fixed, no duration draw). Tested on a
// non-statused, damaged mon so there is no prior-status cure to reason about.
ZENITH_TEST(ZM_Battle, Rest_SlumberHeal_FullHealAndSelfSleep)
{
	ZM_BattleState xState;
	BuildBattleState(xState, MakeSpecOverride(ZM_SPECIES_NIBBIN, 30u, ZM_MOVE_SLUMBERHEAL, 100u, 40u, 40u, 40u, 40u, 60u),
		MakeSpec(ZM_SPECIES_STRAYLING, 30u, ZM_MOVE_RAMBASH), 0x1234ull, 54ull);
	ZM_BattleMonster& xPlayer = xState.Side(ZM_SIDE_PLAYER).Active();
	const u_int uMaxHP = xPlayer.m_auMaxStat[ZM_STAT_HP];
	xPlayer.m_uCurHP = uMaxHP / 2u;   // damaged

	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);

	ZENITH_ASSERT_EQ(xPlayer.m_uCurHP, uMaxHP, "REST heals to full HP");
	ZENITH_ASSERT_EQ((u_int)xPlayer.m_eStatus, (u_int)ZM_MAJOR_STATUS_SLEEP, "REST self-inflicts SLEEP");
	ZENITH_ASSERT_EQ(xPlayer.m_uStatusCounter, 2u, "REST sets the sleep counter to 2");
	ZENITH_ASSERT_TRUE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_HEAL), "REST emits a HEAL");
	const ZM_BattleEvent* pxApplied = ZM_FindKind(xEvents, ZM_BATTLE_EVENT_STATUS_APPLIED);
	ZENITH_ASSERT_NOT_NULL(pxApplied, "REST emits STATUS_APPLIED(SLEEP)");
	if (pxApplied != nullptr) { ZENITH_ASSERT_EQ((u_int)pxApplied->m_iAux, (u_int)ZM_MAJOR_STATUS_SLEEP); }
}

// ============================================================================
// BURN damage reduction (bBurnedPhysical) + PARALYSIS speed cut, isolated.
// ============================================================================
namespace
{
	// One ApplyDamagingHit at ulSeed with the attacker optionally BURNED; returns
	// the raw damage. Same seed -> identical crit/roll, so burned == floor(unburned/2).
	u_int ZM_ApplyHitBurned(ZM_MOVE_ID eMove, u_int64 ulSeed, bool bBurned)
	{
		ZM_BattleState xState;
		BuildBattleState(xState,
			MakeSpecOverride(ZM_SPECIES_NIBBIN,    20u, eMove,            200u, 60u, 40u, 60u, 40u, 100u),
			MakeSpecOverride(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH,  250u, 10u, 40u, 10u, 40u,  10u), ulSeed, 54ull);
		if (bBurned) { xState.Side(ZM_SIDE_PLAYER).Active().m_eStatus = ZM_MAJOR_STATUS_BURN; }
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
		return ZM_MoveExecutor::ApplyDamagingHit(xCtx);
	}
}

// A burned attacker's PHYSICAL hit is halved (seed 1 non-crit, oracle: 18 -> 9).
ZENITH_TEST(ZM_Battle, BurnDamage_HalvesPhysical_SameSeed)
{
	const u_int uUnburned = ZM_ApplyHitBurned(ZM_MOVE_RAMBASH, 1ull, false);
	const u_int uBurned   = ZM_ApplyHitBurned(ZM_MOVE_RAMBASH, 1ull, true);
	ZENITH_ASSERT_GE(uUnburned, 2u, "need >= 2 base damage to observe halving");
	ZENITH_ASSERT_EQ(uBurned, uUnburned / 2u, "a burned physical hit deals floor(unburned/2)");
}

// A burned attacker's SPECIAL hit is UNAFFECTED (bBurnedPhysical is physical-only).
ZENITH_TEST(ZM_Battle, BurnDamage_SpecialUnaffected)
{
	const u_int uUnburned = ZM_ApplyHitBurned(ZM_MOVE_FLARELASH, 1ull, false);
	const u_int uBurned   = ZM_ApplyHitBurned(ZM_MOVE_FLARELASH, 1ull, true);
	ZENITH_ASSERT_GE(uUnburned, 2u, "sanity: non-trivial special damage");
	ZENITH_ASSERT_EQ(uBurned, uUnburned, "burn does NOT reduce a special hit");
}

// Paralysis' 1/4 effective-speed cut flips turn order: a mon that is faster when
// healthy moves SECOND once paralyzed. Player speed stat ~80, enemy ~50: 80 > 50
// (player first) but 80/4 = 20 < 50 (enemy first). Observed via the first MOVE_USED.
ZENITH_TEST(ZM_Battle, ParalysisSpeed_QuarterCutFlipsTurnOrder)
{
	const ZM_BattleMonsterSpec xPlayer = MakeSpecOverride(ZM_SPECIES_NIBBIN,    50u, ZM_MOVE_RAMBASH, 250u, 10u, 250u, 10u, 250u, 60u);
	const ZM_BattleMonsterSpec xEnemy  = MakeSpecOverride(ZM_SPECIES_STRAYLING, 50u, ZM_MOVE_RAMBASH, 250u, 10u, 250u, 10u, 250u, 30u);

	// Control: healthy player is faster -> acts first.
	{
		ZM_BattleMonsterSpec axP[1] = { xPlayer };
		ZM_BattleMonsterSpec axE[1] = { xEnemy };
		ZM_BattleEngine xEngine;
		xEngine.Begin(MakeTrainerConfig(), axP, 1u, axE, 1u, 1ull, 54ull);
		xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
		xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
		xEngine.ResolveTurn();
		ZENITH_ASSERT_EQ(FirstActorSide(xEngine), (u_int)ZM_SIDE_PLAYER, "healthy: faster player acts first");
	}
	// Paralyzed: player's effective speed is quartered -> enemy acts first.
	{
		ZM_BattleEngine xEngine;
		ZM_BeginWithPlayerStatus(xEngine, xPlayer, xEnemy, 1ull, ZM_MAJOR_STATUS_PARALYSIS, 0u);
		xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
		xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
		xEngine.ResolveTurn();
		ZENITH_ASSERT_EQ(FirstActorSide(xEngine), (u_int)ZM_SIDE_ENEMY, "paralyzed: the 1/4 speed cut makes the enemy act first");
	}
}

// ============================================================================
// The m_iTag POD append + a status-free regression guard.
// ============================================================================

// m_iTag is the appended 8th field: it defaults 0 (so 7-arg box-1 events still
// compare equal), a trailing ZM_MakeEvent arg sets it, and it participates in ==.
ZENITH_TEST(ZM_Battle, Event_TagFieldAppendedDefaultsZero)
{
	static_assert(std::is_trivially_copyable_v<ZM_BattleEvent>, "ZM_BattleEvent stays trivially copyable after the m_iTag append");

	// A default-built event and a box-1-style 7-arg event both leave m_iTag 0.
	const ZM_BattleEvent xDef = ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_BEGIN);
	ZENITH_ASSERT_EQ(xDef.m_iTag, 0, "m_iTag defaults to 0");
	const ZM_BattleEvent x7 = ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_ENEMY, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 10, 22);
	const ZM_BattleEvent x8 = ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_ENEMY, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 10, 22, 0);
	ZENITH_ASSERT_TRUE(x7 == x8, "a 7-arg event equals the same event with an explicit tag 0 (box-1 goldens stay equal)");

	// A STATUS_DAMAGE carries the source status in the 8th arg; tag participates in ==.
	const ZM_BattleEvent xTagged = ZM_MakeEvent(ZM_BATTLE_EVENT_STATUS_DAMAGE, ZM_SIDE_ENEMY, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 10, 59, (int)ZM_MAJOR_STATUS_BURN);
	ZENITH_ASSERT_EQ(xTagged.m_iTag, (int)ZM_MAJOR_STATUS_BURN, "the trailing arg sets m_iTag");
	{ ZM_BattleEvent x = xTagged; x.m_iTag = 0; ZENITH_ASSERT_FALSE(x == xTagged, "m_iTag differences break equality"); }
}

// A status-free control battle is byte-identical to box 1: no STATUS_* events ever
// fire and EVERY event leaves the appended m_iTag at 0 (the append is transparent).
ZENITH_TEST(ZM_Battle, StatusFree_NoStatusEventsAndTagZero)
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
	ZENITH_ASSERT_TRUE(xEngine.IsOver(), "the status-free control battle terminates");

	const Zenith_Vector<ZM_BattleEvent>& xEvents = xEngine.GetEvents();
	ZENITH_ASSERT_EQ(ZM_CountKind(xEvents, ZM_BATTLE_EVENT_STATUS_APPLIED), 0u, "no STATUS_APPLIED in a status-free battle");
	ZENITH_ASSERT_EQ(ZM_CountKind(xEvents, ZM_BATTLE_EVENT_STATUS_DAMAGE), 0u, "no STATUS_DAMAGE in a status-free battle");
	ZENITH_ASSERT_EQ(ZM_CountKind(xEvents, ZM_BATTLE_EVENT_STATUS_CURED), 0u, "no STATUS_CURED in a status-free battle");
	for (u_int i = 0; i < xEvents.GetSize(); ++i)
	{
		ZENITH_ASSERT_EQ(xEvents.Get(i).m_iTag, 0, "every status-free event leaves m_iTag at 0 (box-1 goldens unshifted)");
	}
}

// ============================================================================
// A few extra coverage tests (mini-goldens + multi-turn + no-op edges).
// ============================================================================

// A non-sleep STATUS primary that lands is EXACTLY [MOVE_USED, STATUS_APPLIED]:
// accuracy is boosted so it never draws, and burn/para/poison/toxic draw nothing
// at apply time. (SLEEP is excluded here -- it also draws a duration.)
ZENITH_TEST(ZM_Battle, Burn_Apply_Hexflame_ExactTwoEventStream)
{
	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunStatusPrimary(MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_HEXFLAME),
		MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, ZM_MAJOR_STATUS_NONE, xState, xEvents);

	Zenith_Vector<ZM_BattleEvent> xExpected;
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER, 0u, ZM_MOVE_HEXFLAME));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_STATUS_APPLIED, ZM_SIDE_ENEMY, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, (int)ZM_MAJOR_STATUS_BURN));
	ZM_AssertStreamEquals(xExpected, xEvents, "burnApplyExact");
}

// A damaging status secondary runs AFTER the hit: DAMAGE_DEALT precedes STATUS_APPLIED.
ZENITH_TEST(ZM_Battle, Secondary_StatusAppliedAfterDamage)
{
	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunPlayerMove(MakeSpecOverride(ZM_SPECIES_NIBBIN,    20u, ZM_MOVE_EMBERCLAW, 80u, 30u,  40u, 40u, 40u, 60u),
		MakeSpecOverride(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH,  250u, 10u, 250u, 10u, 40u, 10u), 15ull, xState, xEvents);

	int iDmg = -1;
	int iApplied = -1;
	for (u_int i = 0; i < xEvents.GetSize(); ++i)
	{
		if (xEvents.Get(i).m_eKind == ZM_BATTLE_EVENT_DAMAGE_DEALT && iDmg < 0)         { iDmg = (int)i; }
		if (xEvents.Get(i).m_eKind == ZM_BATTLE_EVENT_STATUS_APPLIED && iApplied < 0)   { iApplied = (int)i; }
	}
	ZENITH_ASSERT_GE(iDmg, 0, "the secondary carrier deals damage");
	ZENITH_ASSERT_GE(iApplied, 0, "the burn secondary applied at seed 15");
	ZENITH_ASSERT_LT(iDmg, iApplied, "DAMAGE_DEALT must precede the STATUS_APPLIED secondary");
}

// SLEEP over three turns from counter 3: asleep (turns 1,2), wakes on turn 3.
ZENITH_TEST(ZM_Battle, Sleep_Gate_ThreeTurnWake)
{
	ZM_BattleEngine xEngine;
	ZM_BeginWithPlayerStatus(xEngine, ZM_GateFastPlayer(ZM_MOVE_RAMBASH), ZM_GateBulkyEnemy(),
		1ull, ZM_MAJOR_STATUS_SLEEP, 3u);

	// Turns 1 & 2: still asleep (MOVE_FAILED(ASLEEP), no player MOVE_USED).
	for (u_int uTurn = 0; uTurn < 2u; ++uTurn)
	{
		const u_int uBefore = xEngine.GetEventCount();
		xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
		xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
		xEngine.ResolveTurn();
		bool bFailed = false;
		bool bPlayerMoved = false;
		for (u_int i = uBefore; i < xEngine.GetEventCount(); ++i)
		{
			const ZM_BattleEvent& x = xEngine.GetEvent(i);
			if (x.m_eKind == ZM_BATTLE_EVENT_MOVE_FAILED && x.m_uSide == (u_int)ZM_SIDE_PLAYER && (u_int)x.m_iAux == (u_int)ZM_MOVE_FAIL_ASLEEP) { bFailed = true; }
			if (x.m_eKind == ZM_BATTLE_EVENT_MOVE_USED && x.m_uSide == (u_int)ZM_SIDE_PLAYER) { bPlayerMoved = true; }
		}
		ZENITH_ASSERT_TRUE(bFailed, "asleep on turn %u", uTurn + 1u);
		ZENITH_ASSERT_FALSE(bPlayerMoved, "no MOVE_USED while asleep on turn %u", uTurn + 1u);
	}
	// Turn 3: wakes and acts.
	{
		const u_int uBefore = xEngine.GetEventCount();
		xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
		xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
		xEngine.ResolveTurn();
		bool bCured = false;
		bool bPlayerMoved = false;
		for (u_int i = uBefore; i < xEngine.GetEventCount(); ++i)
		{
			const ZM_BattleEvent& x = xEngine.GetEvent(i);
			if (x.m_eKind == ZM_BATTLE_EVENT_STATUS_CURED && x.m_uSide == (u_int)ZM_SIDE_PLAYER) { bCured = true; }
			if (x.m_eKind == ZM_BATTLE_EVENT_MOVE_USED && x.m_uSide == (u_int)ZM_SIDE_PLAYER)    { bPlayerMoved = true; }
		}
		ZENITH_ASSERT_TRUE(bCured, "STATUS_CURED(SLEEP) on the waking turn");
		ZENITH_ASSERT_TRUE(bPlayerMoved, "acts on the waking turn");
	}
	ZENITH_ASSERT_EQ((u_int)xEngine.GetState().Side(ZM_SIDE_PLAYER).Active().m_eStatus, (u_int)ZM_MAJOR_STATUS_NONE, "awake after three turns");
}

// CURE_STATUS on a HEALTHY mon is a no-op: status stays NONE, no STATUS_CURED.
ZENITH_TEST(ZM_Battle, CureStatus_ShakeOff_NoOpWhenHealthy)
{
	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunPlayerMove(MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_SHAKEOFF),
		MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH), 0x1234ull, xState, xEvents);
	ZENITH_ASSERT_EQ((u_int)xState.Side(ZM_SIDE_PLAYER).Active().m_eStatus, (u_int)ZM_MAJOR_STATUS_NONE);
	ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_STATUS_CURED), "curing a healthy mon emits no STATUS_CURED");
}

// ============================================================================
// SC4 FIRST GOLDEN -- Scenario_BurnSecondaryThenChip_ExactStream. A FIRE PHYSICAL
// move with a burn secondary (Emberclaw, rigged to proc at seed 0x1234) from a
// fast Kindlet into a bulky NORMAL Strayling, then Strayling's Rambash reply
// (halved -- the enemy is now BURNED), then the enemy's end-of-turn burn chip.
// Exact 11-event stream, derived by the offline oracle. Enemy maxHP 84 -> chip 10.
// The whole stream carries m_iTag 0 EXCEPT the STATUS_DAMAGE, whose 8th field is
// (int)ZM_MAJOR_STATUS_BURN.
// ============================================================================
ZENITH_TEST(ZM_Battle, Scenario_BurnSecondaryThenChip_ExactStream)
{
	ZM_BattleMonsterSpec axPlayer[1] = { MakeSpecOverride(ZM_SPECIES_KINDLET,   20u, ZM_MOVE_EMBERCLAW, 50u, 55u, 45u, 60u, 50u, 90u) };
	ZM_BattleMonsterSpec axEnemy[1]  = { MakeSpecOverride(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH,  120u, 40u, 90u, 35u, 60u, 40u) };

	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeTrainerConfig(), axPlayer, 1u, axEnemy, 1u, 0x1234ull, 54ull);
	xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xEngine.SubmitAction(ZM_SIDE_ENEMY,  MoveAction(0u));
	xEngine.ResolveTurn();

	ZENITH_ASSERT_FALSE(xEngine.IsOver(), "single-turn burn golden must not end the battle");

	// --- offline-derived golden stream (11 events, seed 0x1234) ---
	Zenith_Vector<ZM_BattleEvent> xExpected;
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_BEGIN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_PLAYER, 0, ZM_MOVE_NONE, ZM_SPECIES_KINDLET));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_ENEMY, 0, ZM_MOVE_NONE, ZM_SPECIES_STRAYLING));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_BEGIN, ZM_SIDE_COUNT, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER, 0, ZM_MOVE_EMBERCLAW));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_ENEMY, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 15, 69));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_STATUS_APPLIED, ZM_SIDE_ENEMY, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, (int)ZM_MAJOR_STATUS_BURN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_ENEMY, 0, ZM_MOVE_RAMBASH));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_PLAYER, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 6, 50));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_STATUS_DAMAGE, ZM_SIDE_ENEMY, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 10, 59, (int)ZM_MAJOR_STATUS_BURN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_END, ZM_SIDE_COUNT, 0, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));

	ZM_AssertStreamEquals(xExpected, xEngine.GetEvents(), "burnSecondaryThenChip");

	// Corroborating post-conditions: enemy is BURNED and took the chip; the enemy's
	// own physical reply was HALVED by its fresh burn (bBurnedPhysical, 6 HP off 56).
	ZENITH_ASSERT_EQ((u_int)xEngine.GetState().Side(ZM_SIDE_ENEMY).Active().m_eStatus, (u_int)ZM_MAJOR_STATUS_BURN, "enemy ends the turn burned");
	ZENITH_ASSERT_EQ(xEngine.GetState().Side(ZM_SIDE_ENEMY).Active().m_uCurHP, 59u, "enemy HP 84 -> 69 (Emberclaw) -> 59 (burn chip)");
	ZENITH_ASSERT_EQ(xEngine.GetState().Side(ZM_SIDE_PLAYER).Active().m_uCurHP, 50u, "player HP 56 -> 50 (halved burned Rambash)");
	ZENITH_ASSERT_TRUE(ZM_ValidateEventStream(xEngine.GetEvents(), xEngine, "burnSecondaryThenChip"));
}

// ============================================================================
// S2 box-2 SC5 -- the ten battle-local volatiles, Endure, Swagger, forced
// switching, and the fully locked G1-G6 / M0-M6 ordering contract. These tests
// intentionally arrive RED before the SC5 implementation. Event encodings,
// guarded draws, and switch-reset semantics are the binding synthesis contract.
// ============================================================================
namespace
{
	int ZM_SC5FindEvent(const Zenith_Vector<ZM_BattleEvent>& xEvents,
		ZM_BATTLE_EVENT eKind, ZM_SIDE eSide = ZM_SIDE_COUNT, u_int uStart = 0u)
	{
		for (u_int u = uStart; u < xEvents.GetSize(); ++u)
		{
			const ZM_BattleEvent& xEvent = xEvents.Get(u);
			if (xEvent.m_eKind == eKind &&
				(eSide == ZM_SIDE_COUNT || xEvent.m_uSide == (u_int)eSide))
			{
				return (int)u;
			}
		}
		return -1;
	}

	const ZM_BattleEvent* ZM_SC5FindEventPtr(const Zenith_Vector<ZM_BattleEvent>& xEvents,
		ZM_BATTLE_EVENT eKind, ZM_SIDE eSide = ZM_SIDE_COUNT, u_int uStart = 0u)
	{
		const int iIndex = ZM_SC5FindEvent(xEvents, eKind, eSide, uStart);
		return iIndex >= 0 ? &xEvents.Get((u_int)iIndex) : nullptr;
	}

	u_int ZM_SC5CountForSide(const Zenith_Vector<ZM_BattleEvent>& xEvents,
		ZM_BATTLE_EVENT eKind, ZM_SIDE eSide)
	{
		u_int uCount = 0u;
		for (u_int u = 0u; u < xEvents.GetSize(); ++u)
		{
			const ZM_BattleEvent& xEvent = xEvents.Get(u);
			if (xEvent.m_eKind == eKind && xEvent.m_uSide == (u_int)eSide)
			{
				++uCount;
			}
		}
		return uCount;
	}

	bool ZM_SC5HasVolatile(const ZM_BattleMonster& xMon, ZM_VOLATILE eVolatile)
	{
		return (xMon.m_uVolatileMask & (u_int)eVolatile) != 0u;
	}

	void ZM_SC5RigVolatile(ZM_BattleMonster& xMon, ZM_VOLATILE eVolatile,
		u_int uTurns = 0u, ZM_SIDE eLeechSource = ZM_SIDE_COUNT)
	{
		xMon.m_uVolatileMask |= (u_int)eVolatile;
		switch (eVolatile)
		{
		case ZM_VOLATILE_CONFUSED:   xMon.m_uConfuseTurns = uTurns; break;
		case ZM_VOLATILE_TRAP:       xMon.m_uTrapTurns = uTurns; break;
		case ZM_VOLATILE_TAUNT:      xMon.m_uTauntTurns = uTurns; break;
		case ZM_VOLATILE_LOCK:       xMon.m_uLockTurns = uTurns; break;
		case ZM_VOLATILE_LEECH_SEED: xMon.m_eLeechSourceSide = eLeechSource; break;
		default: break;
		}
	}

	ZM_BattleMonsterSpec ZM_SC5Spec(ZM_SPECIES_ID eSpecies, ZM_MOVE_ID eMove0,
		ZM_MOVE_ID eMove1 = ZM_MOVE_NONE, u_int uSpeedBase = 60u, u_int uHPBase = 120u)
	{
		ZM_BattleMonsterSpec xSpec = MakeSpecOverride(eSpecies, 50u, eMove0,
			uHPBase, 60u, 80u, 60u, 80u, uSpeedBase);
		xSpec.m_aeMoves[1] = eMove1;
		return xSpec;
	}

	void ZM_SC5ResolveMoveTurn(ZM_BattleEngine& xEngine, u_int uPlayerSlot = 0u,
		u_int uEnemySlot = 0u)
	{
		xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(uPlayerSlot));
		xEngine.SubmitAction(ZM_SIDE_ENEMY, MoveAction(uEnemySlot));
		xEngine.ResolveTurn();
	}

	void ZM_SC5AssertRNGCursor(ZM_BattleState& xState, ZM_BattleRNG& xMirror,
		const char* szReason)
	{
		ZENITH_ASSERT_EQ(xState.m_xRNG.Next(), xMirror.Next(), "%s", szReason);
	}

	u_int ZM_SC5ExpectedConfusionDamage(const ZM_BattleMonster& xMon, u_int uRoll)
	{
		ZM_DamageInput xIn;
		xIn.uLevel = xMon.m_uLevel;
		xIn.uPower = 40u;
		xIn.uAttack = ZM_ApplyStatStage(xMon.m_auMaxStat[ZM_STAT_ATTACK],
			xMon.m_aiStage[ZM_BATTLE_STAT_ATTACK]);
		xIn.uDefense = ZM_ApplyStatStage(xMon.m_auMaxStat[ZM_STAT_DEFENSE],
			xMon.m_aiStage[ZM_BATTLE_STAT_DEFENSE]);
		xIn.bStab = false;
		xIn.uEffectivenessPercent = 100u;
		xIn.bCrit = false;
		xIn.uRandomPercent = uRoll;
		xIn.bBurnedPhysical = xMon.m_eStatus == ZM_MAJOR_STATUS_BURN;
		return ZM_CalcDamage(xIn);
	}

	u_int64 ZM_SC5FindSecondarySeed(ZM_MOVE_ID eMove, bool bWantProc)
	{
		const u_int uChance = ZM_GetMoveData(eMove).m_uEffectChance;
		for (u_int64 ulSeed = 1ull; ulSeed <= 4096ull; ++ulSeed)
		{
			ZM_BattleRNG xMirror(ulSeed, 54ull);
			xMirror.Chance(1u, 24u);
			xMirror.RandRange(85u, 100u);
			const bool bProc = xMirror.RandBelow(100u) < uChance;
			if (bProc == bWantProc) { return ulSeed; }
		}
		return 0ull;
	}

	u_int64 ZM_SC5FindGateSeed(bool bWantBelow33)
	{
		for (u_int64 ulSeed = 1ull; ulSeed <= 4096ull; ++ulSeed)
		{
			ZM_BattleRNG xMirror(ulSeed, 54ull);
			const bool bBelow = xMirror.RandBelow(100u) < 33u;
			if (bBelow == bWantBelow33) { return ulSeed; }
		}
		return 0ull;
	}

	void ZM_SC5AssertAppliedEncoding(const ZM_BattleEvent& xEvent, ZM_SIDE eSide,
		ZM_VOLATILE eVolatile, u_int uDuration, int iTag = 0)
	{
		ZENITH_ASSERT_EQ((u_int)xEvent.m_eKind, (u_int)ZM_BATTLE_EVENT_VOLATILE_APPLIED);
		ZENITH_ASSERT_EQ(xEvent.m_uSide, (u_int)eSide);
		ZENITH_ASSERT_EQ(xEvent.m_uSlot, 0u);
		ZENITH_ASSERT_EQ(xEvent.m_iAmount, (int)uDuration);
		ZENITH_ASSERT_EQ(xEvent.m_iAux, (int)eVolatile);
		ZENITH_ASSERT_EQ(xEvent.m_iTag, iTag);
	}
}

ZENITH_TEST(ZM_Battle, Volatile_DefaultStateIsClear)
{
	const ZM_BattleMonster xMon = ZM_BuildBattleMonster(ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH));
	ZENITH_ASSERT_EQ(xMon.m_uVolatileMask, (u_int)ZM_VOLATILE_NONE);
	ZENITH_ASSERT_EQ(xMon.m_uConfuseTurns, 0u);
	ZENITH_ASSERT_EQ(xMon.m_uTrapTurns, 0u);
	ZENITH_ASSERT_EQ(xMon.m_uTauntTurns, 0u);
	ZENITH_ASSERT_EQ(xMon.m_uLockTurns, 0u);
	ZENITH_ASSERT_EQ(xMon.m_uChargeMoveSlot, uZM_MAX_MOVES);
	ZENITH_ASSERT_EQ(xMon.m_uLockMoveSlot, uZM_MAX_MOVES);
	ZENITH_ASSERT_EQ((u_int)xMon.m_eLeechSourceSide, (u_int)ZM_SIDE_COUNT);
	ZENITH_ASSERT_FALSE(xMon.m_bEndureThisTurn);
}

ZENITH_TEST(ZM_Battle, Volatile_AppliedAndEndedEventEncodingFields)
{
	ZM_BattleState xState;
	BuildBattleState(xState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_BEWILDER),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), 0x1234ull, 54ull);
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_BattleRNG xMirror(0x1234ull, 54ull);
	const u_int uDuration = xMirror.RandRange(1u, 4u);
	ZENITH_ASSERT_TRUE(ZM_StatusLogic::ApplyVolatile(xCtx, ZM_SIDE_ENEMY, ZM_VOLATILE_CONFUSED));
	ZENITH_ASSERT_EQ(xEvents.GetSize(), 1u);
	if (xEvents.GetSize() == 1u)
	{
		ZM_SC5AssertAppliedEncoding(xEvents.Get(0), ZM_SIDE_ENEMY, ZM_VOLATILE_CONFUSED, uDuration);
	}
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_ENEMY).Active().m_uConfuseTurns, uDuration);
	ZM_SC5AssertRNGCursor(xState, xMirror, "confusion apply consumes exactly its duration draw");
	ZENITH_ASSERT_TRUE(ZM_StatusLogic::EndVolatile(xState, xEvents, ZM_SIDE_ENEMY, ZM_VOLATILE_CONFUSED));
	ZENITH_ASSERT_EQ(xEvents.GetSize(), 2u);
	if (xEvents.GetSize() == 2u)
	{
		const ZM_BattleEvent& xEnd = xEvents.Get(1);
		ZENITH_ASSERT_EQ((u_int)xEnd.m_eKind, (u_int)ZM_BATTLE_EVENT_VOLATILE_ENDED);
		ZENITH_ASSERT_EQ(xEnd.m_uSide, (u_int)ZM_SIDE_ENEMY);
		ZENITH_ASSERT_EQ(xEnd.m_iAmount, 0);
		ZENITH_ASSERT_EQ(xEnd.m_iAux, (int)ZM_VOLATILE_CONFUSED);
		ZENITH_ASSERT_EQ(xEnd.m_iTag, 0);
	}
	ZENITH_ASSERT_FALSE(ZM_StatusLogic::EndVolatile(xState, xEvents, ZM_SIDE_ENEMY, ZM_VOLATILE_CONFUSED));
	ZENITH_ASSERT_EQ(xEvents.GetSize(), 2u, "ending an absent volatile is idempotent and silent");
}

ZENITH_TEST(ZM_Battle, Volatile_CountersAreIndependentFromMajorStatusCounter)
{
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH));
	xMon.m_eStatus = ZM_MAJOR_STATUS_TOXIC;
	xMon.m_uStatusCounter = 7u;
	ZM_SC5RigVolatile(xMon, ZM_VOLATILE_CONFUSED, 4u);
	ZM_SC5RigVolatile(xMon, ZM_VOLATILE_TRAP, 5u);
	ZM_SC5RigVolatile(xMon, ZM_VOLATILE_TAUNT, 3u);
	ZM_SC5RigVolatile(xMon, ZM_VOLATILE_LOCK, 2u);
	ZENITH_ASSERT_EQ(xMon.m_uStatusCounter, 7u);
	ZENITH_ASSERT_EQ(xMon.m_uConfuseTurns, 4u);
	ZENITH_ASSERT_EQ(xMon.m_uTrapTurns, 5u);
	ZENITH_ASSERT_EQ(xMon.m_uTauntTurns, 3u);
	ZENITH_ASSERT_EQ(xMon.m_uLockTurns, 2u);
	ZENITH_ASSERT_TRUE(ZM_SC5HasVolatile(xMon, ZM_VOLATILE_CONFUSED));
	ZENITH_ASSERT_TRUE(ZM_SC5HasVolatile(xMon, ZM_VOLATILE_TRAP));
	ZENITH_ASSERT_TRUE(ZM_SC5HasVolatile(xMon, ZM_VOLATILE_TAUNT));
	ZENITH_ASSERT_TRUE(ZM_SC5HasVolatile(xMon, ZM_VOLATILE_LOCK));
}

ZENITH_TEST(ZM_Battle, Volatile_DamageTagsUseDisjointDomain)
{
	ZENITH_ASSERT_EQ(ZM_VolatileDamageTag(ZM_VOLATILE_LEECH_SEED),
		iZM_STATUS_DAMAGE_TAG_VOLATILE_BASE | (int)ZM_VOLATILE_LEECH_SEED);
	ZENITH_ASSERT_EQ(ZM_VolatileDamageTag(ZM_VOLATILE_TRAP),
		iZM_STATUS_DAMAGE_TAG_VOLATILE_BASE | (int)ZM_VOLATILE_TRAP);
	ZENITH_ASSERT_TRUE(ZM_VolatileDamageTag(ZM_VOLATILE_LEECH_SEED) != (int)ZM_MAJOR_STATUS_BURN,
		"volatile Leech tag must not collide with the existing positive major tags");
	ZENITH_ASSERT_TRUE(ZM_VolatileDamageTag(ZM_VOLATILE_TRAP) != (int)ZM_MAJOR_STATUS_TOXIC,
		"volatile Trap tag must not collide with a major tag");
}

ZENITH_TEST(ZM_Battle, Confuse_PrimaryBewilder_AppliesDurationAndExactDraw)
{
	const u_int64 ulSeed = 0x1234ull;
	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunPlayerMove(ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_BEWILDER),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), ulSeed, xState, xEvents);
	ZM_BattleRNG xMirror(ulSeed, 54ull);
	const u_int uDuration = xMirror.RandRange(1u, 4u);
	const ZM_BattleMonster& xDef = xState.Side(ZM_SIDE_ENEMY).Active();
	ZENITH_ASSERT_TRUE(ZM_SC5HasVolatile(xDef, ZM_VOLATILE_CONFUSED));
	ZENITH_ASSERT_EQ(xDef.m_uConfuseTurns, uDuration);
	const ZM_BattleEvent* pxApplied = ZM_SC5FindEventPtr(xEvents, ZM_BATTLE_EVENT_VOLATILE_APPLIED, ZM_SIDE_ENEMY);
	ZENITH_ASSERT_NOT_NULL(pxApplied);
	if (pxApplied != nullptr) { ZM_SC5AssertAppliedEncoding(*pxApplied, ZM_SIDE_ENEMY, ZM_VOLATILE_CONFUSED, uDuration); }
	ZM_SC5AssertRNGCursor(xState, xMirror, "Bewilder draws only the successful confusion duration");
}

ZENITH_TEST(ZM_Battle, Confuse_PrimaryAlreadyConfused_BlockedNoDurationDraw)
{
	const u_int64 ulSeed = 0x1234ull;
	ZM_BattleState xState;
	BuildBattleState(xState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_BEWILDER),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), ulSeed, 54ull);
	ZM_BattleMonster& xDef = xState.Side(ZM_SIDE_ENEMY).Active();
	ZM_SC5RigVolatile(xDef, ZM_VOLATILE_CONFUSED, 2u);
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	const u_int uPP = xCtx.Atk().m_axMoves[0].m_uCurPP;
	ZM_MoveExecutor::Execute(xCtx);
	ZENITH_ASSERT_EQ(xCtx.Atk().m_axMoves[0].m_uCurPP, uPP - 1u);
	ZENITH_ASSERT_TRUE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_USED));
	ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_VOLATILE_APPLIED));
	const ZM_BattleEvent* pxFail = ZM_SC5FindEventPtr(xEvents, ZM_BATTLE_EVENT_MOVE_FAILED, ZM_SIDE_ENEMY);
	ZENITH_ASSERT_NOT_NULL(pxFail);
	if (pxFail != nullptr) { ZENITH_ASSERT_EQ(pxFail->m_iAux, (int)ZM_MOVE_FAIL_VOLATILE_BLOCKED); }
	ZENITH_ASSERT_EQ(xDef.m_uConfuseTurns, 2u, "blocked confusion does not refresh duration");
	ZM_BattleRNG xMirror(ulSeed, 54ull);
	ZM_SC5AssertRNGCursor(xState, xMirror, "blocked confusion consumes no duration draw");
}

ZENITH_TEST(ZM_Battle, Confuse_SecondaryProcAndSuppressionFollowExactDrawStream)
{
	for (u_int uCase = 0u; uCase < 2u; ++uCase)
	{
		const bool bWantProc = uCase == 0u;
		const u_int64 ulSeed = ZM_SC5FindSecondarySeed(ZM_MOVE_DAZZLEPULSE, bWantProc);
		ZENITH_ASSERT_GT(ulSeed, 0ull);
		ZM_BattleState xState;
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_RunPlayerMove(ZM_SC5Spec(ZM_SPECIES_FAYLING, ZM_MOVE_DAZZLEPULSE, ZM_MOVE_NONE, 120u),
			ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH, ZM_MOVE_NONE, 10u, 250u),
			ulSeed, xState, xEvents);
		ZM_BattleRNG xMirror(ulSeed, 54ull);
		xMirror.Chance(1u, 24u);
		xMirror.RandRange(85u, 100u);
		const bool bProc = xMirror.RandBelow(100u) < ZM_GetMoveData(ZM_MOVE_DAZZLEPULSE).m_uEffectChance;
		ZENITH_ASSERT_TRUE(bProc == bWantProc);
		if (bProc)
		{
			const u_int uDuration = xMirror.RandRange(1u, 4u);
			ZENITH_ASSERT_TRUE(ZM_SC5HasVolatile(xState.Side(ZM_SIDE_ENEMY).Active(), ZM_VOLATILE_CONFUSED));
			ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_ENEMY).Active().m_uConfuseTurns, uDuration);
			ZENITH_ASSERT_LT(ZM_SC5FindEvent(xEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT),
				ZM_SC5FindEvent(xEvents, ZM_BATTLE_EVENT_VOLATILE_APPLIED));
		}
		else
		{
			ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xState.Side(ZM_SIDE_ENEMY).Active(), ZM_VOLATILE_CONFUSED));
			ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_VOLATILE_APPLIED));
		}
		ZM_SC5AssertRNGCursor(xState, xMirror, "confusion secondary draws crit, roll, proc, then guarded duration");
	}

	// An already-confused target suppresses the entire damaging-secondary branch:
	// the connected hit draws only crit+roll, with no proc gate or duration draw.
	const u_int64 ulBlockedSeed = 0x1234ull;
	ZM_BattleState xBlockedState;
	BuildBattleState(xBlockedState,
		ZM_SC5Spec(ZM_SPECIES_FAYLING, ZM_MOVE_DAZZLEPULSE, ZM_MOVE_NONE, 120u),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH, ZM_MOVE_NONE, 10u, 250u),
		ulBlockedSeed, 54ull);
	ZM_BattleMonster& xBlockedTarget = xBlockedState.Side(ZM_SIDE_ENEMY).Active();
	ZM_SC5RigVolatile(xBlockedTarget, ZM_VOLATILE_CONFUSED, 3u);
	Zenith_Vector<ZM_BattleEvent> xBlockedEvents;
	ZM_MoveContext xBlockedCtx = MakeCtx(xBlockedState, xBlockedEvents, ZM_SIDE_PLAYER, 0u);
	ZM_BattleRNG xBlockedMirror(ulBlockedSeed, 54ull);
	xBlockedMirror.Chance(1u, 24u);
	xBlockedMirror.RandRange(85u, 100u);
	ZM_MoveExecutor::Execute(xBlockedCtx);
	ZENITH_ASSERT_TRUE(ZM_HasKind(xBlockedEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT));
	ZENITH_ASSERT_FALSE(ZM_HasKind(xBlockedEvents, ZM_BATTLE_EVENT_VOLATILE_APPLIED));
	ZENITH_ASSERT_FALSE(ZM_HasKind(xBlockedEvents, ZM_BATTLE_EVENT_VOLATILE_ENDED));
	ZENITH_ASSERT_FALSE(ZM_HasKind(xBlockedEvents, ZM_BATTLE_EVENT_MOVE_FAILED));
	ZENITH_ASSERT_TRUE(ZM_SC5HasVolatile(xBlockedTarget, ZM_VOLATILE_CONFUSED));
	ZENITH_ASSERT_EQ(xBlockedTarget.m_uConfuseTurns, 3u,
		"an existing confusion is neither refreshed nor decremented by the incoming secondary");
	ZM_SC5AssertRNGCursor(xBlockedState, xBlockedMirror,
		"blocked confusion secondary consumes only ordinary crit+roll draws");
}

ZENITH_TEST(ZM_Battle, Confuse_GateSelfHit_NoPPNoMoveUsed_ExactDamageAndCursor)
{
	const u_int64 ulSeed = ZM_SC5FindGateSeed(true);
	ZM_BattleState xState;
	BuildBattleState(xState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_WHETCLAW), ulSeed, 54ull);
	ZM_BattleMonster& xAtk = xState.Side(ZM_SIDE_PLAYER).Active();
	ZM_SC5RigVolatile(xAtk, ZM_VOLATILE_CONFUSED, 2u);
	xAtk.m_aiStage[ZM_BATTLE_STAT_ATTACK] = 2;
	xAtk.m_aiStage[ZM_BATTLE_STAT_DEFENSE] = -1;
	const u_int uPP = xAtk.m_axMoves[0].m_uCurPP;
	const u_int uBeforeHP = xAtk.m_uCurHP;
	ZM_BattleRNG xMirror(ulSeed, 54ull);
	ZENITH_ASSERT_LT(xMirror.RandBelow(100u), 33u);
	const u_int uRoll = xMirror.RandRange(85u, 100u);
	const u_int uExpected = ZM_SC5ExpectedConfusionDamage(xAtk, uRoll);
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);
	ZENITH_ASSERT_EQ(xAtk.m_axMoves[0].m_uCurPP, uPP, "G5 cancellation spends no PP");
	ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_USED));
	ZENITH_ASSERT_EQ(xAtk.m_uConfuseTurns, 1u, "one attempted G5 action consumes one turn");
	const ZM_BattleEvent* pxFail = ZM_SC5FindEventPtr(xEvents, ZM_BATTLE_EVENT_MOVE_FAILED, ZM_SIDE_PLAYER);
	const ZM_BattleEvent* pxDmg = ZM_SC5FindEventPtr(xEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_PLAYER);
	ZENITH_ASSERT_NOT_NULL(pxFail);
	ZENITH_ASSERT_NOT_NULL(pxDmg);
	if (pxFail != nullptr) { ZENITH_ASSERT_EQ(pxFail->m_iAux, (int)ZM_MOVE_FAIL_CONFUSED); }
	if (pxDmg != nullptr)
	{
		ZENITH_ASSERT_EQ(pxDmg->m_iAmount, (int)uExpected);
		ZENITH_ASSERT_EQ(pxDmg->m_iAux, (int)(uExpected >= uBeforeHP ? 0u : uBeforeHP - uExpected));
		ZENITH_ASSERT_EQ(pxDmg->m_iTag, ZM_VolatileDamageTag(ZM_VOLATILE_CONFUSED));
	}
	ZENITH_ASSERT_LT(ZM_SC5FindEvent(xEvents, ZM_BATTLE_EVENT_MOVE_FAILED),
		ZM_SC5FindEvent(xEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT));
	ZM_SC5AssertRNGCursor(xState, xMirror, "self-hit consumes exactly confuse gate + damage roll");
}

ZENITH_TEST(ZM_Battle, Confuse_GatePass_ActsThenExpiresOnce)
{
	const u_int64 ulSeed = ZM_SC5FindGateSeed(false);
	ZM_BattleState xState;
	BuildBattleState(xState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_WHETCLAW, ZM_MOVE_NONE, 10u, 250u), ulSeed, 54ull);
	ZM_BattleMonster& xAtk = xState.Side(ZM_SIDE_PLAYER).Active();
	ZM_SC5RigVolatile(xAtk, ZM_VOLATILE_CONFUSED, 1u);
	ZM_BattleRNG xMirror(ulSeed, 54ull);
	ZENITH_ASSERT_GE(xMirror.RandBelow(100u), 33u);
	xMirror.Chance(1u, 24u);
	xMirror.RandRange(85u, 100u);
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);
	ZENITH_ASSERT_TRUE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_USED));
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xAtk, ZM_VOLATILE_CONFUSED));
	ZENITH_ASSERT_EQ(xAtk.m_uConfuseTurns, 0u);
	ZENITH_ASSERT_EQ(ZM_CountKind(xEvents, ZM_BATTLE_EVENT_VOLATILE_ENDED), 1u);
	const int iEnded = ZM_SC5FindEvent(xEvents, ZM_BATTLE_EVENT_VOLATILE_ENDED,
		ZM_SIDE_PLAYER);
	const int iUsed = ZM_SC5FindEvent(xEvents, ZM_BATTLE_EVENT_MOVE_USED,
		ZM_SIDE_PLAYER);
	ZENITH_ASSERT_GE(iEnded, 0);
	ZENITH_ASSERT_GE(iUsed, 0);
	ZENITH_ASSERT_LT(iEnded, iUsed,
		"G5 expiry clears confusion and emits ENDED before the move reaches M1");
	if (iEnded >= 0)
	{
		const ZM_BattleEvent& xEnded = xEvents.Get((u_int)iEnded);
		ZENITH_ASSERT_EQ(xEnded.m_iAmount, 0);
		ZENITH_ASSERT_EQ(xEnded.m_iAux, (int)ZM_VOLATILE_CONFUSED);
		ZENITH_ASSERT_EQ(xEnded.m_iTag, 0);
	}
	ZM_SC5AssertRNGCursor(xState, xMirror, "confusion pass precedes the ordinary crit/roll stream");
}

ZENITH_TEST(ZM_Battle, Confuse_BurnHalvesSelfHitAndCanFaint)
{
	const u_int64 ulSeed = ZM_SC5FindGateSeed(true);
	u_int auDamage[2] = {};
	for (u_int uCase = 0u; uCase < 2u; ++uCase)
	{
		ZM_BattleState xState;
		BuildBattleState(xState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH),
			ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_WHETCLAW), ulSeed, 54ull);
		ZM_BattleMonster& xAtk = xState.Side(ZM_SIDE_PLAYER).Active();
		ZM_SC5RigVolatile(xAtk, ZM_VOLATILE_CONFUSED, 2u);
		if (uCase == 1u) { xAtk.m_eStatus = ZM_MAJOR_STATUS_BURN; }
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
		ZM_MoveExecutor::Execute(xCtx);
		const ZM_BattleEvent* pxDmg = ZM_SC5FindEventPtr(xEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_PLAYER);
		ZENITH_ASSERT_NOT_NULL(pxDmg);
		if (pxDmg != nullptr) { auDamage[uCase] = (u_int)pxDmg->m_iAmount; }
	}
	ZENITH_ASSERT_EQ(auDamage[1], auDamage[0] / 2u, "burn halves the typeless physical confusion self-hit");

	ZM_BattleState xKOState;
	BuildBattleState(xKOState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_WHETCLAW), ulSeed, 54ull);
	ZM_BattleMonster& xKOAtk = xKOState.Side(ZM_SIDE_PLAYER).Active();
	ZM_SC5RigVolatile(xKOAtk, ZM_VOLATILE_CONFUSED, 1u);
	xKOAtk.m_uCurHP = 1u;
	Zenith_Vector<ZM_BattleEvent> xKOEvents;
	ZM_MoveContext xKOCtx = MakeCtx(xKOState, xKOEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xKOCtx);
	ZENITH_ASSERT_EQ(ZM_CountKind(xKOEvents, ZM_BATTLE_EVENT_FAINT), 1u);
	const int iFail = ZM_SC5FindEvent(xKOEvents, ZM_BATTLE_EVENT_MOVE_FAILED,
		ZM_SIDE_PLAYER);
	const int iDamage = ZM_SC5FindEvent(xKOEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT,
		ZM_SIDE_PLAYER);
	const int iFaint = ZM_SC5FindEvent(xKOEvents, ZM_BATTLE_EVENT_FAINT,
		ZM_SIDE_PLAYER);
	const int iEnded = ZM_SC5FindEvent(xKOEvents, ZM_BATTLE_EVENT_VOLATILE_ENDED,
		ZM_SIDE_PLAYER);
	ZENITH_ASSERT_GE(iFail, 0);
	ZENITH_ASSERT_GE(iDamage, 0);
	ZENITH_ASSERT_GE(iFaint, 0);
	ZENITH_ASSERT_GE(iEnded, 0);
	ZENITH_ASSERT_LT(iFail, iDamage);
	ZENITH_ASSERT_LT(iDamage, iFaint);
	ZENITH_ASSERT_LT(iFaint, iEnded,
		"an expiring self-hit announces damage/faint before clearing confusion");
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xKOAtk, ZM_VOLATILE_CONFUSED));
	ZENITH_ASSERT_EQ(xKOAtk.m_uConfuseTurns, 0u);
	if (iEnded >= 0)
	{
		ZENITH_ASSERT_EQ(xKOEvents.Get((u_int)iEnded).m_iAux,
			(int)ZM_VOLATILE_CONFUSED);
	}
}

ZENITH_TEST(ZM_Battle, Flinch_SecondaryProcTimingAndGateCancellation)
{
	const u_int64 ulSeed = ZM_SC5FindSecondarySeed(ZM_MOVE_SKULLKNOCK, true);
	ZM_BattleMonsterSpec axPlayer[1] = { ZM_SC5Spec(ZM_SPECIES_KINDLET, ZM_MOVE_SKULLKNOCK, ZM_MOVE_NONE, 120u) };
	ZM_BattleMonsterSpec axEnemy[1] = { ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH, ZM_MOVE_NONE, 10u, 250u) };
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeTrainerConfig(), axPlayer, 1u, axEnemy, 1u, ulSeed, 54ull);
	const u_int uEnemyPP = xEngine.GetState().Side(ZM_SIDE_ENEMY).Active().m_axMoves[0].m_uCurPP;
	ZM_SC5ResolveMoveTurn(xEngine);
	const Zenith_Vector<ZM_BattleEvent>& xEvents = xEngine.GetEvents();
	ZENITH_ASSERT_TRUE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_VOLATILE_APPLIED));
	ZENITH_ASSERT_EQ(ZM_SC5CountForSide(xEvents, ZM_BATTLE_EVENT_FLINCH, ZM_SIDE_ENEMY), 1u);
	ZENITH_ASSERT_EQ(ZM_SC5CountForSide(xEvents, ZM_BATTLE_EVENT_MOVE_FAILED, ZM_SIDE_ENEMY), 0u,
		"FLINCH is the sole G4 cancellation event");
	ZENITH_ASSERT_EQ(ZM_SC5CountForSide(xEvents, ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_ENEMY), 0u);
	ZENITH_ASSERT_EQ(xEngine.GetState().Side(ZM_SIDE_ENEMY).Active().m_axMoves[0].m_uCurPP, uEnemyPP);
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xEngine.GetState().Side(ZM_SIDE_ENEMY).Active(), ZM_VOLATILE_FLINCH));
	ZENITH_ASSERT_LT(ZM_SC5FindEvent(xEvents, ZM_BATTLE_EVENT_FLINCH, ZM_SIDE_ENEMY),
		ZM_SC5FindEvent(xEvents, ZM_BATTLE_EVENT_VOLATILE_ENDED, ZM_SIDE_ENEMY));
}

ZENITH_TEST(ZM_Battle, Flinch_SlowerCarrierExpiresAndNeverStealsNextTurn)
{
	// The faster enemy consumes crit+roll before the slower carrier's
	// crit+roll+secondary stream, so search the real full-turn draw prefix.
	u_int64 ulSeed = 0ull;
	for (u_int64 ulTry = 1ull; ulTry <= 4096ull; ++ulTry)
	{
		ZM_BattleRNG xMirror(ulTry, 54ull);
		xMirror.Chance(1u, 24u);       // faster enemy crit
		xMirror.RandRange(85u, 100u);  // faster enemy damage roll
		xMirror.Chance(1u, 24u);       // slower Skull Knock crit
		xMirror.RandRange(85u, 100u);  // slower Skull Knock damage roll
		if (xMirror.RandBelow(100u) < ZM_GetMoveData(ZM_MOVE_SKULLKNOCK).m_uEffectChance)
		{
			ulSeed = ulTry;
			break;
		}
	}
	ZENITH_ASSERT_GT(ulSeed, 0ull);
	ZM_BattleRNG xTurnMirror(ulSeed, 54ull);
	xTurnMirror.Chance(1u, 24u);
	xTurnMirror.RandRange(85u, 100u);
	xTurnMirror.Chance(1u, 24u);
	xTurnMirror.RandRange(85u, 100u);
	ZENITH_ASSERT_LT(xTurnMirror.RandBelow(100u),
		ZM_GetMoveData(ZM_MOVE_SKULLKNOCK).m_uEffectChance,
		"the selected full-turn seed deterministically procs the slower carrier's Flinch");
	ZM_BattleMonsterSpec axPlayer[1] = { ZM_SC5Spec(ZM_SPECIES_KINDLET, ZM_MOVE_SKULLKNOCK, ZM_MOVE_NONE, 10u, 250u) };
	ZM_BattleMonsterSpec axEnemy[1] = { ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH, ZM_MOVE_NONE, 120u, 250u) };
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeTrainerConfig(), axPlayer, 1u, axEnemy, 1u, ulSeed, 54ull);
	ZM_SC5ResolveMoveTurn(xEngine);
	const Zenith_Vector<ZM_BattleEvent>& xEvents = xEngine.GetEvents();
	ZENITH_ASSERT_TRUE(ZM_SC5CountForSide(xEvents, ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_ENEMY) >= 1u,
		"the faster target already acted before the slow flinch carrier");
	const int iPlayerHit = ZM_SC5FindEvent(xEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_ENEMY);
	const int iApplied = ZM_SC5FindEvent(xEvents, ZM_BATTLE_EVENT_VOLATILE_APPLIED, ZM_SIDE_ENEMY);
	const int iEnded = ZM_SC5FindEvent(xEvents, ZM_BATTLE_EVENT_VOLATILE_ENDED, ZM_SIDE_ENEMY);
	ZENITH_ASSERT_EQ(ZM_SC5CountForSide(xEvents, ZM_BATTLE_EVENT_VOLATILE_APPLIED,
		ZM_SIDE_ENEMY), 1u);
	ZENITH_ASSERT_EQ(ZM_SC5CountForSide(xEvents, ZM_BATTLE_EVENT_VOLATILE_ENDED,
		ZM_SIDE_ENEMY), 1u);
	ZENITH_ASSERT_GE(iPlayerHit, 0);
	ZENITH_ASSERT_GE(iApplied, 0);
	ZENITH_ASSERT_GE(iEnded, 0);
	ZENITH_ASSERT_LT(iPlayerHit, iApplied);
	ZENITH_ASSERT_LT(iApplied, iEnded, "late Flinch applies after the hit, then expires at that EOT");
	if (iApplied >= 0)
	{
		ZM_SC5AssertAppliedEncoding(xEvents.Get((u_int)iApplied), ZM_SIDE_ENEMY,
			ZM_VOLATILE_FLINCH, 0u);
	}
	if (iEnded >= 0)
	{
		const ZM_BattleEvent& xEnded = xEvents.Get((u_int)iEnded);
		ZENITH_ASSERT_EQ(xEnded.m_iAmount, 0);
		ZENITH_ASSERT_EQ(xEnded.m_iAux, (int)ZM_VOLATILE_FLINCH);
		ZENITH_ASSERT_EQ(xEnded.m_iTag, 0);
	}
	ZM_SC5AssertRNGCursor(xEngine.GetStateMutable(), xTurnMirror,
		"the slower-carrier turn consumes the exact two-hit plus proc stream");
	ZENITH_ASSERT_EQ(ZM_SC5CountForSide(xEvents, ZM_BATTLE_EVENT_FLINCH, ZM_SIDE_ENEMY), 0u,
		"a target that already acted never reaches a same-turn G4");
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xEngine.GetState().Side(ZM_SIDE_ENEMY).Active(), ZM_VOLATILE_FLINCH));
	const u_int uBeforeEvents = xEngine.GetEventCount();
	const u_int uBefore = ZM_SC5CountForSide(xEvents, ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_ENEMY);
	const u_int uBeforeFlinch = ZM_SC5CountForSide(xEvents, ZM_BATTLE_EVENT_FLINCH, ZM_SIDE_ENEMY);
	ZM_SC5ResolveMoveTurn(xEngine);
	ZENITH_ASSERT_GT(ZM_SC5CountForSide(xEvents, ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_ENEMY), uBefore,
		"a late flinch must not carry into the next turn");
	ZENITH_ASSERT_GE(ZM_SC5FindEvent(xEvents, ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_ENEMY, uBeforeEvents), 0);
	ZENITH_ASSERT_EQ(ZM_SC5CountForSide(xEvents, ZM_BATTLE_EVENT_FLINCH, ZM_SIDE_ENEMY), uBeforeFlinch,
		"the next turn has no stale G4 cancellation");
}

ZENITH_TEST(ZM_Battle, Flinch_GateUsesDedicatedEventsAndNoRNG)
{
	const u_int64 ulSeed = 0x1234ull;
	ZM_BattleState xState;
	BuildBattleState(xState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_WHETCLAW), ulSeed, 54ull);
	ZM_BattleMonster& xAtk = xState.Side(ZM_SIDE_PLAYER).Active();
	ZM_SC5RigVolatile(xAtk, ZM_VOLATILE_FLINCH);
	const u_int uPP = xAtk.m_axMoves[0].m_uCurPP;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);
	ZENITH_ASSERT_EQ(xAtk.m_axMoves[0].m_uCurPP, uPP);
	ZENITH_ASSERT_EQ(xEvents.GetSize(), 2u);
	if (xEvents.GetSize() == 2u)
	{
		ZENITH_ASSERT_EQ((u_int)xEvents.Get(0).m_eKind, (u_int)ZM_BATTLE_EVENT_FLINCH);
		ZENITH_ASSERT_EQ((u_int)xEvents.Get(1).m_eKind, (u_int)ZM_BATTLE_EVENT_VOLATILE_ENDED);
		ZENITH_ASSERT_EQ(xEvents.Get(1).m_iAux, (int)ZM_VOLATILE_FLINCH);
	}
	ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_FAILED));
	ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_USED));
	ZM_BattleRNG xMirror(ulSeed, 54ull);
	ZM_SC5AssertRNGCursor(xState, xMirror, "G4 flinch consumes no RNG");
}

ZENITH_TEST(ZM_Battle, Scenario_FastFlinchCancelsReply_ExactStream)
{
	// Independent pencil/oracle fixture: level-50 neutral stats from base 1 give
	// Atk=Def=21 and HP=76. Seed 0x1234 is non-crit, roll 88, proc<30:
	// floor(((22*70*21/21)/50+2)*88/100) = 28 raw damage.
	ZM_BattleMonsterSpec axPlayer[1] = {
		MakeSpecOverride(ZM_SPECIES_KINDLET, 50u, ZM_MOVE_SKULLKNOCK, 1u, 1u, 1u, 1u, 1u, 100u) };
	ZM_BattleMonsterSpec axEnemy[1] = {
		MakeSpecOverride(ZM_SPECIES_STRAYLING, 50u, ZM_MOVE_WHETCLAW, 1u, 1u, 1u, 1u, 1u, 1u) };
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeTrainerConfig(), axPlayer, 1u, axEnemy, 1u, 0x1234ull, 54ull);
	ZM_SC5ResolveMoveTurn(xEngine);
	Zenith_Vector<ZM_BattleEvent> xExpected;
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_BEGIN));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_PLAYER, 0u, ZM_MOVE_NONE, ZM_SPECIES_KINDLET));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_ENEMY, 0u, ZM_MOVE_NONE, ZM_SPECIES_STRAYLING));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_BEGIN, ZM_SIDE_COUNT, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER, 0u, ZM_MOVE_SKULLKNOCK));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_ENEMY, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 28, 48));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_VOLATILE_APPLIED, ZM_SIDE_ENEMY, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, (int)ZM_VOLATILE_FLINCH));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_FLINCH, ZM_SIDE_ENEMY, 0u));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_VOLATILE_ENDED, ZM_SIDE_ENEMY, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, (int)ZM_VOLATILE_FLINCH));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_END, ZM_SIDE_COUNT, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 1));
	ZM_AssertStreamEquals(xExpected, xEngine.GetEvents(), "fastFlinchCancelsReply");
}

ZENITH_TEST(ZM_Battle, LeechSeed_ApplyDuplicateGrassBlockAndSourceEncoding)
{
	const u_int64 ulSeed = 0x1234ull;
	ZM_BattleState xState;
	BuildBattleState(xState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_SEEDLEECH),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), ulSeed, 54ull);
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZENITH_ASSERT_TRUE(ZM_StatusLogic::CanApplyVolatile(xCtx.Def(), ZM_VOLATILE_LEECH_SEED));
	ZENITH_ASSERT_TRUE(ZM_StatusLogic::ApplyVolatile(xCtx, ZM_SIDE_ENEMY,
		ZM_VOLATILE_LEECH_SEED, ZM_SIDE_PLAYER));
	const ZM_BattleMonster& xDef = xState.Side(ZM_SIDE_ENEMY).Active();
	ZENITH_ASSERT_TRUE(ZM_SC5HasVolatile(xDef, ZM_VOLATILE_LEECH_SEED));
	ZENITH_ASSERT_EQ((u_int)xDef.m_eLeechSourceSide, (u_int)ZM_SIDE_PLAYER);
	ZENITH_ASSERT_EQ(xEvents.GetSize(), 1u);
	if (xEvents.GetSize() == 1u)
	{
		ZM_SC5AssertAppliedEncoding(xEvents.Get(0), ZM_SIDE_ENEMY, ZM_VOLATILE_LEECH_SEED,
			0u, (int)ZM_SIDE_PLAYER + 1);
	}
	ZENITH_ASSERT_FALSE(ZM_StatusLogic::CanApplyVolatile(xDef, ZM_VOLATILE_LEECH_SEED));
	ZENITH_ASSERT_FALSE(ZM_StatusLogic::ApplyVolatile(xCtx, ZM_SIDE_ENEMY,
		ZM_VOLATILE_LEECH_SEED, ZM_SIDE_PLAYER));
	ZENITH_ASSERT_EQ(xEvents.GetSize(), 1u, "duplicate seed application is silent in the centralized helper");
	ZM_BattleRNG xMirror(ulSeed, 54ull);
	ZM_SC5AssertRNGCursor(xState, xMirror, "Leech Seed apply/block consumes no RNG");

	ZM_BattleState xGrassState;
	BuildBattleState(xGrassState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_SEEDLEECH),
		ZM_SC5Spec(ZM_SPECIES_FERNFAWN, ZM_MOVE_RAMBASH), ulSeed, 54ull);
	ZENITH_ASSERT_FALSE(ZM_StatusLogic::CanApplyVolatile(
		xGrassState.Side(ZM_SIDE_ENEMY).Active(), ZM_VOLATILE_LEECH_SEED),
		"GRASS targets are immune to Leech Seed");
}

ZENITH_TEST(ZM_Battle, LeechSeed_PrimaryMissAndBlockEvents)
{
	// Find a seed whose first draw misses Seed Leech's 90 accuracy.
	u_int64 ulMissSeed = 0ull;
	for (u_int64 ulSeed = 1ull; ulSeed <= 4096ull; ++ulSeed)
	{
		ZM_BattleRNG xMirror(ulSeed, 54ull);
		if (xMirror.RandBelow(100u) >= 90u) { ulMissSeed = ulSeed; break; }
	}
	ZENITH_ASSERT_GT(ulMissSeed, 0ull);
	ZM_BattleState xMissState;
	Zenith_Vector<ZM_BattleEvent> xMissEvents;
	ZM_RunPlayerMove(ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_SEEDLEECH),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), ulMissSeed, xMissState, xMissEvents);
	ZENITH_ASSERT_TRUE(ZM_HasKind(xMissEvents, ZM_BATTLE_EVENT_MOVE_MISSED));
	ZENITH_ASSERT_FALSE(ZM_HasKind(xMissEvents, ZM_BATTLE_EVENT_VOLATILE_APPLIED));
	ZM_BattleRNG xMissMirror(ulMissSeed, 54ull);
	xMissMirror.RandBelow(100u);
	ZM_SC5AssertRNGCursor(xMissState, xMissMirror, "a Leech Seed miss consumes accuracy only");

	// Boost accuracy so the GRASS block itself is isolated and deterministic.
	ZM_BattleState xBlockState;
	BuildBattleState(xBlockState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_SEEDLEECH),
		ZM_SC5Spec(ZM_SPECIES_FERNFAWN, ZM_MOVE_RAMBASH), 1ull, 54ull);
	xBlockState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[ZM_BATTLE_STAT_ACCURACY] = iZM_MAX_STAGE;
	Zenith_Vector<ZM_BattleEvent> xBlockEvents;
	ZM_MoveContext xBlockCtx = MakeCtx(xBlockState, xBlockEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xBlockCtx);
	const ZM_BattleEvent* pxFail = ZM_SC5FindEventPtr(xBlockEvents,
		ZM_BATTLE_EVENT_MOVE_FAILED, ZM_SIDE_ENEMY);
	ZENITH_ASSERT_NOT_NULL(pxFail);
	if (pxFail != nullptr) { ZENITH_ASSERT_EQ(pxFail->m_iAux, (int)ZM_MOVE_FAIL_VOLATILE_BLOCKED); }
	ZENITH_ASSERT_FALSE(ZM_HasKind(xBlockEvents, ZM_BATTLE_EVENT_VOLATILE_APPLIED));
}

ZENITH_TEST(ZM_Battle, LeechSeed_EOT_ChipDrainCapAndSourceSwitchRedirect)
{
	ZM_BattleMonsterSpec axPlayer[2] = {
		ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH),
		ZM_SC5Spec(ZM_SPECIES_KINDLET, ZM_MOVE_RAMBASH) };
	ZM_BattleMonsterSpec axEnemy[1] = {
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH) };
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeTrainerConfig(), axPlayer, 2u, axEnemy, 1u, 1ull, 54ull);
	ZM_BattleState& xState = xEngine.GetStateMutable();
	ZM_BattleSide& xPlayer = xState.Side(ZM_SIDE_PLAYER);
	ZM_BattleSide& xEnemy = xState.Side(ZM_SIDE_ENEMY);
	ZM_BattleMonster& xVictim = xEnemy.Active();
	ZM_SC5RigVolatile(xVictim, ZM_VOLATILE_LEECH_SEED, 0u, ZM_SIDE_PLAYER);
	const u_int uScheduled = xVictim.m_auMaxStat[ZM_STAT_HP] / 8u > 0u
		? xVictim.m_auMaxStat[ZM_STAT_HP] / 8u : 1u;
	xVictim.m_uCurHP = uScheduled - 1u;
	xPlayer.m_xParty.Get(0).m_uCurHP = 1u;
	xPlayer.m_xParty.Get(1).m_uCurHP = xPlayer.m_xParty.Get(1).m_auMaxStat[ZM_STAT_HP] - 2u;
	ZENITH_ASSERT_TRUE(xEngine.DoSwitch(ZM_SIDE_PLAYER, 1u));
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_StatusLogic::EndOfTurn(xState, xEvents, ZM_SIDE_ENEMY);
	const ZM_BattleEvent* pxDamage = ZM_SC5FindEventPtr(xEvents, ZM_BATTLE_EVENT_STATUS_DAMAGE, ZM_SIDE_ENEMY);
	const ZM_BattleEvent* pxDrain = ZM_SC5FindEventPtr(xEvents, ZM_BATTLE_EVENT_DRAIN, ZM_SIDE_PLAYER);
	ZENITH_ASSERT_NOT_NULL(pxDamage);
	ZENITH_ASSERT_NOT_NULL(pxDrain);
	if (pxDamage != nullptr)
	{
		ZENITH_ASSERT_EQ(pxDamage->m_iAmount, (int)uScheduled, "Leech reports raw scheduled chip");
		ZENITH_ASSERT_EQ(pxDamage->m_iAux, 0);
		ZENITH_ASSERT_EQ(pxDamage->m_iTag, ZM_VolatileDamageTag(ZM_VOLATILE_LEECH_SEED));
	}
	if (pxDrain != nullptr)
	{
		ZENITH_ASSERT_EQ(pxDrain->m_uSlot, 1u, "source switching redirects healing to the new active");
		ZENITH_ASSERT_EQ(pxDrain->m_iAmount, 2, "DRAIN amount is actual restored HP, capped by missing HP");
		ZENITH_ASSERT_EQ(pxDrain->m_iAux,
			(int)xPlayer.m_xParty.Get(1).m_auMaxStat[ZM_STAT_HP]);
	}
	ZENITH_ASSERT_LT(ZM_SC5FindEvent(xEvents, ZM_BATTLE_EVENT_STATUS_DAMAGE),
		ZM_SC5FindEvent(xEvents, ZM_BATTLE_EVENT_FAINT));
	ZENITH_ASSERT_LT(ZM_SC5FindEvent(xEvents, ZM_BATTLE_EVENT_FAINT),
		ZM_SC5FindEvent(xEvents, ZM_BATTLE_EVENT_DRAIN));

	// A living full-HP source still reports a zero-amount DRAIN; a source side
	// with no living active still takes the victim chip but emits no DRAIN.
	ZM_BattleState xFullState;
	BuildBattleState(xFullState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), 1ull, 54ull);
	ZM_SC5RigVolatile(xFullState.Side(ZM_SIDE_ENEMY).Active(),
		ZM_VOLATILE_LEECH_SEED, 0u, ZM_SIDE_PLAYER);
	Zenith_Vector<ZM_BattleEvent> xFullEvents;
	ZM_StatusLogic::EndOfTurn(xFullState, xFullEvents, ZM_SIDE_ENEMY);
	const ZM_BattleEvent* pxFullDrain = ZM_SC5FindEventPtr(xFullEvents,
		ZM_BATTLE_EVENT_DRAIN, ZM_SIDE_PLAYER);
	ZENITH_ASSERT_NOT_NULL(pxFullDrain);
	if (pxFullDrain != nullptr)
	{
		ZENITH_ASSERT_EQ(pxFullDrain->m_iAmount, 0);
		ZENITH_ASSERT_EQ(pxFullDrain->m_iAux,
			(int)xFullState.Side(ZM_SIDE_PLAYER).Active().m_auMaxStat[ZM_STAT_HP]);
	}

	ZM_BattleState xNoSourceState;
	BuildBattleState(xNoSourceState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), 1ull, 54ull);
	xNoSourceState.Side(ZM_SIDE_PLAYER).Active().m_uCurHP = 0u;
	ZM_SC5RigVolatile(xNoSourceState.Side(ZM_SIDE_ENEMY).Active(),
		ZM_VOLATILE_LEECH_SEED, 0u, ZM_SIDE_PLAYER);
	Zenith_Vector<ZM_BattleEvent> xNoSourceEvents;
	ZM_StatusLogic::EndOfTurn(xNoSourceState, xNoSourceEvents, ZM_SIDE_ENEMY);
	ZENITH_ASSERT_TRUE(ZM_HasKind(xNoSourceEvents, ZM_BATTLE_EVENT_STATUS_DAMAGE));
	ZENITH_ASSERT_FALSE(ZM_HasKind(xNoSourceEvents, ZM_BATTLE_EVENT_DRAIN));
}

ZENITH_TEST(ZM_Battle, LeechSeed_DoSwitchClearsWithoutEndedEvent)
{
	ZM_BattleMonsterSpec axPlayer[1] = { ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH) };
	ZM_BattleMonsterSpec axEnemy[2] = {
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH),
		ZM_SC5Spec(ZM_SPECIES_KINDLET, ZM_MOVE_RAMBASH) };
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeTrainerConfig(), axPlayer, 1u, axEnemy, 2u, 1ull, 54ull);
	ZM_BattleMonster& xOutgoing = xEngine.GetStateMutable().Side(ZM_SIDE_ENEMY).Active();
	ZM_SC5RigVolatile(xOutgoing, ZM_VOLATILE_LEECH_SEED, 0u, ZM_SIDE_PLAYER);
	const u_int uBefore = xEngine.GetEventCount();
	ZENITH_ASSERT_TRUE(xEngine.DoSwitch(ZM_SIDE_ENEMY, 1u));
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xOutgoing, ZM_VOLATILE_LEECH_SEED));
	ZENITH_ASSERT_EQ((u_int)xOutgoing.m_eLeechSourceSide, (u_int)ZM_SIDE_COUNT);
	for (u_int u = uBefore; u < xEngine.GetEventCount(); ++u)
	{
		ZENITH_ASSERT_TRUE(xEngine.GetEvent(u).m_eKind != ZM_BATTLE_EVENT_VOLATILE_ENDED,
			"switch cleanup is silent; SWITCH_IN is the only switch event");
	}
}

ZENITH_TEST(ZM_Battle, Trap_DamagingCarrier_DrawOrderApplyAndDuplicateBlock)
{
	// Find a base-accuracy hit seed for Maelstrom Coil (85).
	u_int64 ulSeed = 0ull;
	for (u_int64 ulTry = 1ull; ulTry <= 4096ull; ++ulTry)
	{
		ZM_BattleRNG xMirror(ulTry, 54ull);
		if (xMirror.RandBelow(100u) < 85u) { ulSeed = ulTry; break; }
	}
	ZENITH_ASSERT_GT(ulSeed, 0ull);
	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunPlayerMove(ZM_SC5Spec(ZM_SPECIES_FINLET, ZM_MOVE_MAELSTROMCOIL, ZM_MOVE_NONE, 120u),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH, ZM_MOVE_NONE, 10u, 250u),
		ulSeed, xState, xEvents);
	ZM_BattleRNG xMirror(ulSeed, 54ull);
	xMirror.RandBelow(100u);          // M5 accuracy
	xMirror.Chance(1u, 24u);         // crit
	xMirror.RandRange(85u, 100u);    // damage roll
	const u_int uDuration = xMirror.RandRange(4u, 5u);
	const ZM_BattleMonster& xDef = xState.Side(ZM_SIDE_ENEMY).Active();
	ZENITH_ASSERT_TRUE(ZM_SC5HasVolatile(xDef, ZM_VOLATILE_TRAP));
	ZENITH_ASSERT_EQ(xDef.m_uTrapTurns, uDuration);
	const ZM_BattleEvent* pxApplied = ZM_SC5FindEventPtr(xEvents,
		ZM_BATTLE_EVENT_VOLATILE_APPLIED, ZM_SIDE_ENEMY);
	ZENITH_ASSERT_NOT_NULL(pxApplied);
	if (pxApplied != nullptr) { ZM_SC5AssertAppliedEncoding(*pxApplied, ZM_SIDE_ENEMY, ZM_VOLATILE_TRAP, uDuration); }
	ZM_SC5AssertRNGCursor(xState, xMirror, "Trap draws accuracy, crit, roll, then guarded duration");

	// Central duplicate application is silent and does not refresh/draw.
	Zenith_Vector<ZM_BattleEvent> xDuplicateEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xDuplicateEvents, ZM_SIDE_PLAYER, 0u);
	ZENITH_ASSERT_FALSE(ZM_StatusLogic::ApplyVolatile(xCtx, ZM_SIDE_ENEMY, ZM_VOLATILE_TRAP));
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_ENEMY).Active().m_uTrapTurns, uDuration);
	ZENITH_ASSERT_EQ(xDuplicateEvents.GetSize(), 0u);
	ZM_SC5AssertRNGCursor(xState, xMirror, "duplicate Trap consumes no duration draw");
}

ZENITH_TEST(ZM_Battle, Trap_MissAndFaintedTargetDoNotApplyOrDrawDuration)
{
	u_int64 ulMissSeed = 0ull;
	for (u_int64 ulTry = 1ull; ulTry <= 4096ull; ++ulTry)
	{
		ZM_BattleRNG xMirror(ulTry, 54ull);
		if (xMirror.RandBelow(100u) >= 85u) { ulMissSeed = ulTry; break; }
	}
	ZM_BattleState xMissState;
	Zenith_Vector<ZM_BattleEvent> xMissEvents;
	ZM_RunPlayerMove(ZM_SC5Spec(ZM_SPECIES_FINLET, ZM_MOVE_MAELSTROMCOIL),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), ulMissSeed, xMissState, xMissEvents);
	ZENITH_ASSERT_FALSE(ZM_HasKind(xMissEvents, ZM_BATTLE_EVENT_VOLATILE_APPLIED));
	ZM_BattleRNG xMissMirror(ulMissSeed, 54ull);
	xMissMirror.RandBelow(100u);
	ZM_SC5AssertRNGCursor(xMissState, xMissMirror, "missed Trap carrier draws accuracy only");

	ZM_BattleState xKOState;
	BuildBattleState(xKOState, ZM_SC5Spec(ZM_SPECIES_FINLET, ZM_MOVE_MAELSTROMCOIL),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), 1ull, 54ull);
	xKOState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP = 1u;
	xKOState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[ZM_BATTLE_STAT_ACCURACY] = iZM_MAX_STAGE;
	Zenith_Vector<ZM_BattleEvent> xKOEvents;
	ZM_MoveContext xKOCtx = MakeCtx(xKOState, xKOEvents, ZM_SIDE_PLAYER, 0u);
	ZM_BattleRNG xKOMirror(1ull, 54ull);
	const u_int uKOAccuracy = ZM_ApplyStatStage(
		ZM_GetMoveData(ZM_MOVE_MAELSTROMCOIL).m_uAccuracy, iZM_MAX_STAGE);
	ZENITH_ASSERT_LT(xKOMirror.RandBelow(100u), uKOAccuracy,
		"the mirrored +6-accuracy Trap carrier draw must connect before testing KO suppression");
	xKOMirror.Chance(1u, 24u);
	xKOMirror.RandRange(85u, 100u);
	ZM_MoveExecutor::Execute(xKOCtx);
	ZENITH_ASSERT_TRUE(xKOState.Side(ZM_SIDE_ENEMY).Active().IsFainted());
	ZENITH_ASSERT_FALSE(ZM_HasKind(xKOEvents, ZM_BATTLE_EVENT_MOVE_MISSED));
	ZENITH_ASSERT_TRUE(ZM_HasKind(xKOEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT));
	ZENITH_ASSERT_FALSE(ZM_HasKind(xKOEvents, ZM_BATTLE_EVENT_VOLATILE_APPLIED));
	ZM_SC5AssertRNGCursor(xKOState, xKOMirror, "a KO carrier does not draw/apply Trap duration");
}

ZENITH_TEST(ZM_Battle, Trap_EOT_TicksApplicationTurnExpiresAndFaintsOnce)
{
	ZM_BattleState xState;
	BuildBattleState(xState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), 1ull, 54ull);
	ZM_BattleMonster& xVictim = xState.Side(ZM_SIDE_ENEMY).Active();
	ZM_SC5RigVolatile(xVictim, ZM_VOLATILE_TRAP, 1u);
	const u_int uScheduled = xVictim.m_auMaxStat[ZM_STAT_HP] / 8u > 0u
		? xVictim.m_auMaxStat[ZM_STAT_HP] / 8u : 1u;
	xVictim.m_uCurHP = uScheduled + 1u;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_StatusLogic::EndOfTurn(xState, xEvents, ZM_SIDE_ENEMY);
	ZENITH_ASSERT_EQ(xVictim.m_uCurHP, 1u);
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xVictim, ZM_VOLATILE_TRAP));
	const ZM_BattleEvent* pxDamage = ZM_SC5FindEventPtr(xEvents, ZM_BATTLE_EVENT_STATUS_DAMAGE);
	const ZM_BattleEvent* pxEnd = ZM_SC5FindEventPtr(xEvents, ZM_BATTLE_EVENT_VOLATILE_ENDED);
	ZENITH_ASSERT_NOT_NULL(pxDamage);
	ZENITH_ASSERT_NOT_NULL(pxEnd);
	if (pxDamage != nullptr) { ZENITH_ASSERT_EQ(pxDamage->m_iTag, ZM_VolatileDamageTag(ZM_VOLATILE_TRAP)); }
	ZENITH_ASSERT_LT(ZM_SC5FindEvent(xEvents, ZM_BATTLE_EVENT_STATUS_DAMAGE),
		ZM_SC5FindEvent(xEvents, ZM_BATTLE_EVENT_VOLATILE_ENDED));

	ZM_BattleState xKOState;
	BuildBattleState(xKOState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), 1ull, 54ull);
	ZM_BattleMonster& xKOVictim = xKOState.Side(ZM_SIDE_ENEMY).Active();
	ZM_SC5RigVolatile(xKOVictim, ZM_VOLATILE_TRAP, 3u);
	xKOVictim.m_uCurHP = 1u;
	Zenith_Vector<ZM_BattleEvent> xKOEvents;
	ZM_StatusLogic::EndOfTurn(xKOState, xKOEvents, ZM_SIDE_ENEMY);
	ZENITH_ASSERT_EQ(ZM_CountKind(xKOEvents, ZM_BATTLE_EVENT_FAINT), 1u);
	ZENITH_ASSERT_FALSE(ZM_HasKind(xKOEvents, ZM_BATTLE_EVENT_VOLATILE_ENDED),
		"a KO stops later per-side volatile cleanup events");
}

ZENITH_TEST(ZM_Battle, Scenario_LeechTrapEOT_ExactStream)
{
	// Independent arithmetic golden: max HP 76, victim 30 HP, source 70/76.
	// Leech schedules 9 -> victim 21, restores 6 to cap; Trap schedules 9 -> 12,
	// then its final counter expires.
	ZM_BattleState xState;
	BuildBattleState(xState,
		MakeSpecOverride(ZM_SPECIES_KINDLET, 50u, ZM_MOVE_RAMBASH, 1u, 1u, 1u, 1u, 1u, 2u),
		MakeSpecOverride(ZM_SPECIES_STRAYLING, 50u, ZM_MOVE_RAMBASH, 1u, 1u, 1u, 1u, 1u, 1u),
		1ull, 54ull);
	ZM_BattleMonster& xSource = xState.Side(ZM_SIDE_PLAYER).Active();
	ZM_BattleMonster& xVictim = xState.Side(ZM_SIDE_ENEMY).Active();
	xSource.m_uCurHP = 70u;
	xVictim.m_uCurHP = 30u;
	ZM_SC5RigVolatile(xVictim, ZM_VOLATILE_LEECH_SEED, 0u, ZM_SIDE_PLAYER);
	ZM_SC5RigVolatile(xVictim, ZM_VOLATILE_TRAP, 1u);
	Zenith_Vector<ZM_BattleEvent> xActual;
	ZM_StatusLogic::EndOfTurn(xState, xActual, ZM_SIDE_ENEMY);
	Zenith_Vector<ZM_BattleEvent> xExpected;
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_STATUS_DAMAGE, ZM_SIDE_ENEMY, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 9, 21, ZM_VolatileDamageTag(ZM_VOLATILE_LEECH_SEED)));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_DRAIN, ZM_SIDE_PLAYER, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 6, 76));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_STATUS_DAMAGE, ZM_SIDE_ENEMY, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 9, 12, ZM_VolatileDamageTag(ZM_VOLATILE_TRAP)));
	xExpected.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_VOLATILE_ENDED, ZM_SIDE_ENEMY, 0u,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, (int)ZM_VOLATILE_TRAP));
	ZM_AssertStreamEquals(xExpected, xActual, "leechTrapEOT");
}

ZENITH_TEST(ZM_Battle, Taunt_ApplyM0BlockDamageBypassAndExpiry)
{
	ZM_BattleState xState;
	BuildBattleState(xState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_GOAD),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_WHETCLAW, ZM_MOVE_RAMBASH), 1ull, 54ull);
	Zenith_Vector<ZM_BattleEvent> xApplyEvents;
	ZM_MoveContext xApplyCtx = MakeCtx(xState, xApplyEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xApplyCtx);
	ZM_BattleMonster& xTaunted = xState.Side(ZM_SIDE_ENEMY).Active();
	ZENITH_ASSERT_TRUE(ZM_SC5HasVolatile(xTaunted, ZM_VOLATILE_TAUNT));
	ZENITH_ASSERT_EQ(xTaunted.m_uTauntTurns, 3u);
	const ZM_BattleEvent* pxApplied = ZM_SC5FindEventPtr(xApplyEvents,
		ZM_BATTLE_EVENT_VOLATILE_APPLIED, ZM_SIDE_ENEMY);
	ZENITH_ASSERT_NOT_NULL(pxApplied);
	if (pxApplied != nullptr) { ZM_SC5AssertAppliedEncoding(*pxApplied, ZM_SIDE_ENEMY, ZM_VOLATILE_TAUNT, 3u); }

	// M0 blocks the status slot before PP/MOVE_USED and consumes no RNG.
	Zenith_Vector<ZM_BattleEvent> xBlockEvents;
	ZM_MoveContext xBlockCtx = MakeCtx(xState, xBlockEvents, ZM_SIDE_ENEMY, 0u);
	const u_int uStatusPP = xTaunted.m_axMoves[0].m_uCurPP;
	ZM_BattleRNG xMirror = xState.m_xRNG;
	ZM_MoveExecutor::Execute(xBlockCtx);
	ZENITH_ASSERT_EQ(xTaunted.m_axMoves[0].m_uCurPP, uStatusPP);
	ZENITH_ASSERT_FALSE(ZM_HasKind(xBlockEvents, ZM_BATTLE_EVENT_MOVE_USED));
	const ZM_BattleEvent* pxFail = ZM_SC5FindEventPtr(xBlockEvents,
		ZM_BATTLE_EVENT_MOVE_FAILED, ZM_SIDE_ENEMY);
	ZENITH_ASSERT_NOT_NULL(pxFail);
	if (pxFail != nullptr) { ZENITH_ASSERT_EQ(pxFail->m_iAux, (int)ZM_MOVE_FAIL_TAUNTED); }
	ZM_SC5AssertRNGCursor(xState, xMirror, "M0 Taunt consumes no RNG");

	// A damaging slot bypasses M0 and spends PP/acts.
	Zenith_Vector<ZM_BattleEvent> xDamageEvents;
	ZM_MoveContext xDamageCtx = MakeCtx(xState, xDamageEvents, ZM_SIDE_ENEMY, 1u);
	const u_int uDamagePP = xTaunted.m_axMoves[1].m_uCurPP;
	ZM_MoveExecutor::Execute(xDamageCtx);
	ZENITH_ASSERT_EQ(xTaunted.m_axMoves[1].m_uCurPP, uDamagePP - 1u);
	ZENITH_ASSERT_TRUE(ZM_HasKind(xDamageEvents, ZM_BATTLE_EVENT_MOVE_USED));

	// Application-turn EOT decrements 3->2; two more EOT passes expire exactly once.
	Zenith_Vector<ZM_BattleEvent> xEOT;
	ZM_StatusLogic::EndOfTurn(xState, xEOT, ZM_SIDE_ENEMY);
	ZENITH_ASSERT_EQ(xTaunted.m_uTauntTurns, 2u);
	ZM_StatusLogic::EndOfTurn(xState, xEOT, ZM_SIDE_ENEMY);
	ZENITH_ASSERT_EQ(xTaunted.m_uTauntTurns, 1u);
	ZM_StatusLogic::EndOfTurn(xState, xEOT, ZM_SIDE_ENEMY);
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xTaunted, ZM_VOLATILE_TAUNT));
	ZENITH_ASSERT_EQ(ZM_CountKind(xEOT, ZM_BATTLE_EVENT_VOLATILE_ENDED), 1u);
}

ZENITH_TEST(ZM_Battle, Protect_M2InterceptsOpponentMovesBeforeAllDraws)
{
	ZM_BattleMonsterSpec axPlayer[1] = {
		ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_BULWARK, ZM_MOVE_NONE, 10u, 250u) };
	ZM_BattleMonsterSpec axEnemy[1] = {
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_TORRENTCANNON, ZM_MOVE_NONE, 120u, 250u) };
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeTrainerConfig(), axPlayer, 1u, axEnemy, 1u, 0x1234ull, 54ull);
	const u_int uEnemyPP = xEngine.GetState().Side(ZM_SIDE_ENEMY).Active().m_axMoves[0].m_uCurPP;
	ZM_BattleRNG xMirror(0x1234ull, 54ull);
	ZM_SC5ResolveMoveTurn(xEngine);
	const Zenith_Vector<ZM_BattleEvent>& xEvents = xEngine.GetEvents();
	ZENITH_ASSERT_TRUE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_VOLATILE_APPLIED));
	const ZM_BattleEvent* pxFail = ZM_SC5FindEventPtr(xEvents,
		ZM_BATTLE_EVENT_MOVE_FAILED, ZM_SIDE_PLAYER);
	ZENITH_ASSERT_NOT_NULL(pxFail);
	if (pxFail != nullptr) { ZENITH_ASSERT_EQ(pxFail->m_iAux, (int)ZM_MOVE_FAIL_PROTECTED); }
	ZENITH_ASSERT_EQ(xEngine.GetState().Side(ZM_SIDE_ENEMY).Active().m_axMoves[0].m_uCurPP, uEnemyPP - 1u);
	ZENITH_ASSERT_EQ(ZM_SC5CountForSide(xEvents, ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_ENEMY), 1u);
	ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_MISSED));
	ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT));
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xEngine.GetState().Side(ZM_SIDE_PLAYER).Active(), ZM_VOLATILE_PROTECT));
	ZENITH_ASSERT_EQ(ZM_CountKind(xEvents, ZM_BATTLE_EVENT_VOLATILE_ENDED), 1u,
		"Protect expires at the same turn EOT");

	// Consecutive uses refresh normally: there is no repeat-use decay or RNG.
	const u_int uSecondStart = xEngine.GetEventCount();
	ZM_SC5ResolveMoveTurn(xEngine);
	const ZM_BattleEvent* pxSecondFail = ZM_SC5FindEventPtr(xEvents,
		ZM_BATTLE_EVENT_MOVE_FAILED, ZM_SIDE_PLAYER, uSecondStart);
	ZENITH_ASSERT_NOT_NULL(pxSecondFail);
	if (pxSecondFail != nullptr) { ZENITH_ASSERT_EQ(pxSecondFail->m_iAux, (int)ZM_MOVE_FAIL_PROTECTED); }
	ZENITH_ASSERT_EQ(ZM_CountKind(xEvents, ZM_BATTLE_EVENT_VOLATILE_APPLIED), 2u);
	ZENITH_ASSERT_EQ(ZM_CountKind(xEvents, ZM_BATTLE_EVENT_VOLATILE_ENDED), 2u);
	ZM_BattleState& xMutable = xEngine.GetStateMutable();
	ZM_SC5AssertRNGCursor(xMutable, xMirror,
		"M2 Protect and consecutive refreshes consume no accuracy/crit/repeat-use draws");

	// Protect is the sole refreshable volatile. Two central applications in the
	// same turn each announce APPLIED, with no intermediate end, failure, or draw.
	ZM_BattleState xRefreshState;
	BuildBattleState(xRefreshState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_BULWARK),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), 1ull, 54ull);
	Zenith_Vector<ZM_BattleEvent> xRefreshEvents;
	ZM_MoveContext xRefreshCtx = MakeCtx(xRefreshState, xRefreshEvents, ZM_SIDE_PLAYER, 0u);
	ZM_BattleRNG xRefreshMirror(1ull, 54ull);
	ZENITH_ASSERT_TRUE(ZM_StatusLogic::ApplyVolatile(
		xRefreshCtx, ZM_SIDE_PLAYER, ZM_VOLATILE_PROTECT));
	ZENITH_ASSERT_TRUE(ZM_StatusLogic::ApplyVolatile(
		xRefreshCtx, ZM_SIDE_PLAYER, ZM_VOLATILE_PROTECT));
	ZENITH_ASSERT_TRUE(ZM_SC5HasVolatile(
		xRefreshState.Side(ZM_SIDE_PLAYER).Active(), ZM_VOLATILE_PROTECT));
	ZENITH_ASSERT_EQ(xRefreshEvents.GetSize(), 2u);
	ZENITH_ASSERT_EQ(ZM_CountKind(xRefreshEvents, ZM_BATTLE_EVENT_VOLATILE_APPLIED), 2u);
	ZENITH_ASSERT_EQ(ZM_CountKind(xRefreshEvents, ZM_BATTLE_EVENT_VOLATILE_ENDED), 0u);
	ZENITH_ASSERT_EQ(ZM_CountKind(xRefreshEvents, ZM_BATTLE_EVENT_MOVE_FAILED), 0u);
	if (xRefreshEvents.GetSize() == 2u)
	{
		ZM_SC5AssertAppliedEncoding(xRefreshEvents.Get(0u), ZM_SIDE_PLAYER,
			ZM_VOLATILE_PROTECT, 0u);
		ZM_SC5AssertAppliedEncoding(xRefreshEvents.Get(1u), ZM_SIDE_PLAYER,
			ZM_VOLATILE_PROTECT, 0u);
	}
	ZM_SC5AssertRNGCursor(xRefreshState, xRefreshMirror,
		"same-turn Protect refresh emits twice without a draw");

	ZM_StatusLogic::EndOfTurn(xRefreshState, xRefreshEvents, ZM_SIDE_PLAYER);
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(
		xRefreshState.Side(ZM_SIDE_PLAYER).Active(), ZM_VOLATILE_PROTECT));
	ZENITH_ASSERT_EQ(xRefreshEvents.GetSize(), 3u);
	ZENITH_ASSERT_EQ(ZM_CountKind(xRefreshEvents, ZM_BATTLE_EVENT_VOLATILE_APPLIED), 2u);
	ZENITH_ASSERT_EQ(ZM_CountKind(xRefreshEvents, ZM_BATTLE_EVENT_VOLATILE_ENDED), 1u);
	ZENITH_ASSERT_EQ(ZM_CountKind(xRefreshEvents, ZM_BATTLE_EVENT_MOVE_FAILED), 0u);
	if (xRefreshEvents.GetSize() == 3u)
	{
		const ZM_BattleEvent& xEnded = xRefreshEvents.Get(2u);
		ZENITH_ASSERT_EQ((u_int)xEnded.m_eKind,
			(u_int)ZM_BATTLE_EVENT_VOLATILE_ENDED);
		ZENITH_ASSERT_EQ(xEnded.m_uSide, (u_int)ZM_SIDE_PLAYER);
		ZENITH_ASSERT_EQ(xEnded.m_iAux, (int)ZM_VOLATILE_PROTECT);
		ZENITH_ASSERT_EQ(xEnded.m_iTag, 0);
	}
	ZM_SC5AssertRNGCursor(xRefreshState, xRefreshMirror,
		"Protect's single EOT expiry also consumes no draw");
}

ZENITH_TEST(ZM_Battle, Protect_OnlyInterceptsOpponentTargetAndPreventsMultiHitCountDraw)
{
	ZM_BattleState xState;
	BuildBattleState(xState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_WHETCLAW),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), 1ull, 54ull);
	ZM_SC5RigVolatile(xState.Side(ZM_SIDE_ENEMY).Active(), ZM_VOLATILE_PROTECT);
	Zenith_Vector<ZM_BattleEvent> xSelfEvents;
	ZM_MoveContext xSelfCtx = MakeCtx(xState, xSelfEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xSelfCtx);
	ZENITH_ASSERT_TRUE(ZM_HasKind(xSelfEvents, ZM_BATTLE_EVENT_STAT_STAGE_CHANGED),
		"a self-target status move bypasses the opponent's Protect");
	ZENITH_ASSERT_FALSE(ZM_HasKind(xSelfEvents, ZM_BATTLE_EVENT_MOVE_FAILED));

	// An opponent-target STATUS move is intercepted at M2 after PP/MOVE_USED but
	// before both its M5 accuracy draw and its sleep-duration draw.
	ZM_BattleState xStatusState;
	BuildBattleState(xStatusState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_DROWSESPORE),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), 1ull, 54ull);
	ZM_SC5RigVolatile(xStatusState.Side(ZM_SIDE_ENEMY).Active(), ZM_VOLATILE_PROTECT);
	const u_int uStatusPP = xStatusState.Side(ZM_SIDE_PLAYER).Active().m_axMoves[0].m_uCurPP;
	Zenith_Vector<ZM_BattleEvent> xStatusEvents;
	ZM_MoveContext xStatusCtx = MakeCtx(xStatusState, xStatusEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xStatusCtx);
	ZENITH_ASSERT_EQ(xStatusState.Side(ZM_SIDE_PLAYER).Active().m_axMoves[0].m_uCurPP, uStatusPP - 1u);
	ZENITH_ASSERT_TRUE(ZM_HasKind(xStatusEvents, ZM_BATTLE_EVENT_MOVE_USED));
	ZENITH_ASSERT_FALSE(ZM_HasKind(xStatusEvents, ZM_BATTLE_EVENT_MOVE_MISSED));
	ZENITH_ASSERT_FALSE(ZM_HasKind(xStatusEvents, ZM_BATTLE_EVENT_STATUS_APPLIED));
	ZENITH_ASSERT_EQ((u_int)xStatusState.Side(ZM_SIDE_ENEMY).Active().m_eStatus,
		(u_int)ZM_MAJOR_STATUS_NONE);
	const ZM_BattleEvent* pxStatusFail = ZM_SC5FindEventPtr(xStatusEvents,
		ZM_BATTLE_EVENT_MOVE_FAILED, ZM_SIDE_ENEMY);
	ZENITH_ASSERT_NOT_NULL(pxStatusFail);
	if (pxStatusFail != nullptr) { ZENITH_ASSERT_EQ(pxStatusFail->m_iAux, (int)ZM_MOVE_FAIL_PROTECTED); }
	ZM_BattleRNG xStatusMirror(1ull, 54ull);
	ZM_SC5AssertRNGCursor(xStatusState, xStatusMirror,
		"Protect intercepts opponent STATUS before accuracy and status-duration draws");

	// FIELD-target STATUS bypasses the opponent's Protect just like SELF target.
	ZM_BattleState xFieldState;
	BuildBattleState(xFieldState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAINCALL),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), 1ull, 54ull);
	ZM_SC5RigVolatile(xFieldState.Side(ZM_SIDE_ENEMY).Active(), ZM_VOLATILE_PROTECT);
	Zenith_Vector<ZM_BattleEvent> xFieldEvents;
	ZM_MoveContext xFieldCtx = MakeCtx(xFieldState, xFieldEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xFieldCtx);
	ZENITH_ASSERT_EQ((u_int)xFieldState.m_xField.m_eWeather, (u_int)ZM_WEATHER_RAIN);
	ZENITH_ASSERT_FALSE(ZM_HasKind(xFieldEvents, ZM_BATTLE_EVENT_MOVE_FAILED));

	ZM_BattleState xMultiState;
	BuildBattleState(xMultiState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_THORNVOLLEY),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), 1ull, 54ull);
	ZM_SC5RigVolatile(xMultiState.Side(ZM_SIDE_ENEMY).Active(), ZM_VOLATILE_PROTECT);
	Zenith_Vector<ZM_BattleEvent> xMultiEvents;
	ZM_MoveContext xMultiCtx = MakeCtx(xMultiState, xMultiEvents, ZM_SIDE_PLAYER, 0u);
	ZM_BattleRNG xMirror(1ull, 54ull);
	ZM_MoveExecutor::Execute(xMultiCtx);
	ZENITH_ASSERT_FALSE(ZM_HasKind(xMultiEvents, ZM_BATTLE_EVENT_MULTI_HIT));
	ZENITH_ASSERT_FALSE(ZM_HasKind(xMultiEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT));
	ZM_SC5AssertRNGCursor(xMultiState, xMirror, "Protect intercepts before MULTI_HIT count/crit/roll");
}

ZENITH_TEST(ZM_Battle, Endure_IsSeparateSilentGuardAndClearsAtEOT)
{
	ZM_BattleMonsterSpec axPlayer[1] = {
		ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_BRACE, ZM_MOVE_NONE, 10u, 50u) };
	ZM_BattleMonsterSpec axEnemy[1] = {
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_COMETDASH, ZM_MOVE_NONE, 120u, 250u) };
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeTrainerConfig(), axPlayer, 1u, axEnemy, 1u, 1ull, 54ull);
	xEngine.GetStateMutable().Side(ZM_SIDE_PLAYER).Active().m_uCurHP = 1u;
	ZM_SC5ResolveMoveTurn(xEngine);
	const ZM_BattleMonster& xPlayer = xEngine.GetState().Side(ZM_SIDE_PLAYER).Active();
	ZENITH_ASSERT_EQ(xPlayer.m_uCurHP, 1u);
	ZENITH_ASSERT_FALSE(xPlayer.m_bEndureThisTurn, "Endure clears silently at EOT");
	ZENITH_ASSERT_EQ(ZM_CountKind(xEngine.GetEvents(), ZM_BATTLE_EVENT_VOLATILE_APPLIED), 0u,
		"Endure is not an 11th volatile");
	ZENITH_ASSERT_EQ(ZM_CountKind(xEngine.GetEvents(), ZM_BATTLE_EVENT_VOLATILE_ENDED), 0u);
	ZENITH_ASSERT_EQ(ZM_CountKind(xEngine.GetEvents(), ZM_BATTLE_EVENT_FAINT), 0u);
	const ZM_BattleEvent* pxDamage = ZM_SC5FindEventPtr(xEngine.GetEvents(),
		ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_PLAYER);
	ZENITH_ASSERT_NOT_NULL(pxDamage);
	if (pxDamage != nullptr)
	{
		ZENITH_ASSERT_GT(pxDamage->m_iAmount, 1, "raw lethal damage remains reported");
		ZENITH_ASSERT_EQ(pxDamage->m_iAux, 1, "Endure clamps remaining HP to one");
	}
}

ZENITH_TEST(ZM_Battle, Endure_ClampsMultiHitFixedAndOHKOButNotSelfOrEOTDamage)
{
	const ZM_MOVE_ID aeDirectMoves[] = { ZM_MOVE_THORNVOLLEY, ZM_MOVE_MINDSHEAR, ZM_MOVE_FISSURECRACK };
	const u_int64 aulConnectingSeeds[] = { 2ull, 2ull, 1ull };
	for (u_int u = 0u; u < 3u; ++u)
	{
		ZM_BattleState xState;
		BuildBattleState(xState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, aeDirectMoves[u]),
			ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), aulConnectingSeeds[u], 54ull);
		if (aeDirectMoves[u] == ZM_MOVE_FISSURECRACK)
		{
			// The shared +6 stat-stage fold is x4: base 30 becomes 120. M5 still
			// consumes its draw because the move's base accuracy is below 100.
			xState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[ZM_BATTLE_STAT_ACCURACY] = iZM_MAX_STAGE;
			const u_int uOHKOAccuracy = ZM_ApplyStatStage(
				ZM_GetMoveData(ZM_MOVE_FISSURECRACK).m_uAccuracy, iZM_MAX_STAGE);
			ZENITH_ASSERT_EQ(uOHKOAccuracy, 120u);
			ZM_BattleRNG xHitMirror(aulConnectingSeeds[u], 54ull);
			ZENITH_ASSERT_LT(xHitMirror.RandBelow(100u), uOHKOAccuracy,
				"the mirrored boosted OHKO accuracy draw must connect");
		}
		ZM_BattleMonster& xDef = xState.Side(ZM_SIDE_ENEMY).Active();
		xDef.m_uCurHP = 1u;
		xDef.m_bEndureThisTurn = true;
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
		ZM_MoveExecutor::Execute(xCtx);
		ZENITH_ASSERT_TRUE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_USED));
		ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_MISSED));
		ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_FAILED));
		u_int uDamageCount = 0u;
		for (u_int uEvent = 0u; uEvent < xEvents.GetSize(); ++uEvent)
		{
			const ZM_BattleEvent& xEvent = xEvents.Get(uEvent);
			if (xEvent.m_eKind != ZM_BATTLE_EVENT_DAMAGE_DEALT) { continue; }
			++uDamageCount;
			ZENITH_ASSERT_GT(xEvent.m_iAmount, 0,
				"every connected direct delivery reports positive raw damage");
			ZENITH_ASSERT_EQ(xEvent.m_iAux, 1,
				"Endure reports one remaining HP for every delivered hit");
			if (aeDirectMoves[u] == ZM_MOVE_MINDSHEAR)
			{
				ZENITH_ASSERT_EQ(xEvent.m_iAmount,
					(int)xState.Side(ZM_SIDE_PLAYER).Active().m_uLevel,
					"FIXED_LEVEL retains its exact raw damage under Endure");
			}
			else if (aeDirectMoves[u] == ZM_MOVE_FISSURECRACK)
			{
				ZENITH_ASSERT_EQ(xEvent.m_iAmount, 1,
					"OHKO reports the defender's pre-hit HP as raw damage");
			}
		}
		ZENITH_ASSERT_GT(uDamageCount, 0u, "direct Endure fixture must connect and deal damage");
		if (aeDirectMoves[u] == ZM_MOVE_THORNVOLLEY)
		{
			const ZM_BattleEvent* pxMulti = ZM_SC5FindEventPtr(xEvents,
				ZM_BATTLE_EVENT_MULTI_HIT, ZM_SIDE_PLAYER);
			ZENITH_ASSERT_NOT_NULL(pxMulti);
			if (pxMulti != nullptr)
			{
				ZENITH_ASSERT_EQ(pxMulti->m_iAmount, (int)uDamageCount,
					"MULTI_HIT landed count matches the Endure-clamped damage groups");
			}
		}
		else
		{
			ZENITH_ASSERT_EQ(uDamageCount, 1u,
				"fixed and OHKO delivery each emit exactly one damage group");
		}
		ZENITH_ASSERT_EQ(xDef.m_uCurHP, 1u, "Endure clamps every direct move-delivery kind");
		ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_FAINT));
	}

	// Confusion self-hit is explicitly outside Endure.
	const u_int64 ulSelfSeed = ZM_SC5FindGateSeed(true);
	ZM_BattleState xSelfState;
	BuildBattleState(xSelfState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_WHETCLAW), ulSelfSeed, 54ull);
	ZM_BattleMonster& xSelf = xSelfState.Side(ZM_SIDE_PLAYER).Active();
	xSelf.m_uCurHP = 1u;
	xSelf.m_bEndureThisTurn = true;
	ZM_SC5RigVolatile(xSelf, ZM_VOLATILE_CONFUSED, 2u);
	Zenith_Vector<ZM_BattleEvent> xSelfEvents;
	ZM_MoveContext xSelfCtx = MakeCtx(xSelfState, xSelfEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xSelfCtx);
	ZENITH_ASSERT_TRUE(xSelf.IsFainted());

	// Recoil is self-inflicted after a connected hit and is likewise outside Endure.
	ZM_BattleState xRecoilState;
	BuildBattleState(xRecoilState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RECKLESSRUSH),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH, ZM_MOVE_NONE, 60u, 300u), 1ull, 54ull);
	ZM_BattleMonster& xRecoilUser = xRecoilState.Side(ZM_SIDE_PLAYER).Active();
	xRecoilUser.m_uCurHP = 1u;
	xRecoilUser.m_bEndureThisTurn = true;
	Zenith_Vector<ZM_BattleEvent> xRecoilEvents;
	ZM_MoveContext xRecoilCtx = MakeCtx(xRecoilState, xRecoilEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xRecoilCtx);
	ZENITH_ASSERT_TRUE(ZM_HasKind(xRecoilEvents, ZM_BATTLE_EVENT_RECOIL));
	ZENITH_ASSERT_TRUE(xRecoilUser.IsFainted(), "Endure never clamps recoil");

	// Major/EOT chip is also outside Endure.
	ZM_BattleState xEOTState;
	BuildBattleState(xEOTState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), 1ull, 54ull);
	ZM_BattleMonster& xEOT = xEOTState.Side(ZM_SIDE_PLAYER).Active();
	xEOT.m_uCurHP = 1u;
	xEOT.m_bEndureThisTurn = true;
	xEOT.m_eStatus = ZM_MAJOR_STATUS_POISON;
	Zenith_Vector<ZM_BattleEvent> xEOTEvents;
	ZM_StatusLogic::EndOfTurn(xEOTState, xEOTEvents, ZM_SIDE_PLAYER);
	ZENITH_ASSERT_TRUE(xEOT.IsFainted());
}

ZENITH_TEST(ZM_Battle, Charge_FirstTurnNoAccuracy_ReleaseForcedNoSecondPP)
{
	ZM_BattleMonsterSpec axPlayer[1] = {
		ZM_SC5Spec(ZM_SPECIES_KINDLET, ZM_MOVE_HEATSHIMMER, ZM_MOVE_RAMBASH, 120u, 250u) };
	ZM_BattleMonsterSpec axEnemy[1] = {
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_WHETCLAW, ZM_MOVE_NONE, 10u, 250u) };
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeTrainerConfig(), axPlayer, 1u, axEnemy, 1u, 0x1234ull, 54ull);
	const u_int uStartPP = xEngine.GetState().Side(ZM_SIDE_PLAYER).Active().m_axMoves[0].m_uCurPP;
	ZM_SC5ResolveMoveTurn(xEngine, 0u, 0u);
	ZM_BattleMonster& xPlayer = xEngine.GetStateMutable().Side(ZM_SIDE_PLAYER).Active();
	ZENITH_ASSERT_TRUE(ZM_SC5HasVolatile(xPlayer, ZM_VOLATILE_CHARGE));
	ZENITH_ASSERT_EQ(xPlayer.m_uChargeMoveSlot, 0u);
	ZENITH_ASSERT_EQ(xPlayer.m_axMoves[0].m_uCurPP, uStartPP - 1u);
	ZENITH_ASSERT_EQ(ZM_SC5CountForSide(xEngine.GetEvents(), ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_ENEMY), 0u,
		"charge turn returns before accuracy/crit/damage");
	const ZM_BattleEvent* pxApply = ZM_SC5FindEventPtr(xEngine.GetEvents(),
		ZM_BATTLE_EVENT_VOLATILE_APPLIED, ZM_SIDE_PLAYER);
	ZENITH_ASSERT_NOT_NULL(pxApply);
	if (pxApply != nullptr) { ZM_SC5AssertAppliedEncoding(*pxApply, ZM_SIDE_PLAYER, ZM_VOLATILE_CHARGE, 0u); }

	// A pending release is legal at 0 PP, forces stored slot 0 over submitted slot 1,
	// announces the move again, but does not spend PP a second time.
	xPlayer.m_axMoves[0].m_uCurPP = 0u;
	const u_int uBeforeEvents = xEngine.GetEventCount();
	ZM_SC5ResolveMoveTurn(xEngine, 1u, 0u);
	ZENITH_ASSERT_EQ(xPlayer.m_axMoves[0].m_uCurPP, 0u);
	ZENITH_ASSERT_EQ(ZM_SC5CountForSide(xEngine.GetEvents(), ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER), 2u);
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xPlayer, ZM_VOLATILE_CHARGE));
	ZENITH_ASSERT_EQ(xPlayer.m_uChargeMoveSlot, uZM_MAX_MOVES);
	const int iUsed = ZM_SC5FindEvent(xEngine.GetEvents(), ZM_BATTLE_EVENT_MOVE_USED,
		ZM_SIDE_PLAYER, uBeforeEvents);
	const int iEnd = ZM_SC5FindEvent(xEngine.GetEvents(), ZM_BATTLE_EVENT_VOLATILE_ENDED,
		ZM_SIDE_PLAYER, uBeforeEvents);
	const int iDamage = ZM_SC5FindEvent(xEngine.GetEvents(), ZM_BATTLE_EVENT_DAMAGE_DEALT,
		ZM_SIDE_ENEMY, uBeforeEvents);
	ZENITH_ASSERT_GE(iUsed, 0);
	ZENITH_ASSERT_GE(iEnd, 0);
	ZENITH_ASSERT_GE(iDamage, 0);
	ZENITH_ASSERT_LT(iUsed, iEnd);
	ZENITH_ASSERT_LT(iEnd, iDamage, "release clears/ends charge after MOVE_USED but before hit resolution");

	// A release intercepted at M2 is still consumed: MOVE_USED, then clear/end,
	// then PROTECTED, with neither second PP nor any hit draw.
	ZM_BattleState xProtectedState;
	BuildBattleState(xProtectedState, ZM_SC5Spec(ZM_SPECIES_KINDLET, ZM_MOVE_HEATSHIMMER),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), 1ull, 54ull);
	ZM_BattleMonster& xProtectedUser = xProtectedState.Side(ZM_SIDE_PLAYER).Active();
	ZM_SC5RigVolatile(xProtectedUser, ZM_VOLATILE_CHARGE);
	xProtectedUser.m_uChargeMoveSlot = 0u;
	ZM_SC5RigVolatile(xProtectedState.Side(ZM_SIDE_ENEMY).Active(), ZM_VOLATILE_PROTECT);
	const u_int uProtectedPP = xProtectedUser.m_axMoves[0].m_uCurPP;
	Zenith_Vector<ZM_BattleEvent> xProtectedEvents;
	ZM_MoveContext xProtectedCtx = MakeCtx(xProtectedState, xProtectedEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xProtectedCtx);
	const int iProtectedUsed = ZM_SC5FindEvent(xProtectedEvents, ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER);
	const int iProtectedEnd = ZM_SC5FindEvent(xProtectedEvents, ZM_BATTLE_EVENT_VOLATILE_ENDED, ZM_SIDE_PLAYER);
	const int iProtectedFail = ZM_SC5FindEvent(xProtectedEvents, ZM_BATTLE_EVENT_MOVE_FAILED, ZM_SIDE_ENEMY);
	ZENITH_ASSERT_GE(iProtectedUsed, 0);
	ZENITH_ASSERT_GE(iProtectedEnd, 0);
	ZENITH_ASSERT_GE(iProtectedFail, 0);
	ZENITH_ASSERT_LT(iProtectedUsed, iProtectedEnd);
	ZENITH_ASSERT_LT(iProtectedEnd, iProtectedFail);
	if (iProtectedFail >= 0)
	{
		ZENITH_ASSERT_EQ(xProtectedEvents.Get((u_int)iProtectedFail).m_iAux, (int)ZM_MOVE_FAIL_PROTECTED);
	}
	ZENITH_ASSERT_EQ(xProtectedUser.m_axMoves[0].m_uCurPP, uProtectedPP);
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xProtectedUser, ZM_VOLATILE_CHARGE));
	ZENITH_ASSERT_FALSE(ZM_HasKind(xProtectedEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT));
	ZM_BattleRNG xProtectedMirror(1ull, 54ull);
	ZM_SC5AssertRNGCursor(xProtectedState, xProtectedMirror, "protected release consumes no hit draw");
}

ZENITH_TEST(ZM_Battle, Charge_GateCancellationClearsChargeAndSemiAfterFailure)
{
	ZM_BattleState xState;
	BuildBattleState(xState, ZM_SC5Spec(ZM_SPECIES_KINDLET, ZM_MOVE_UNDERDELVE),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_WHETCLAW), 1ull, 54ull);
	ZM_BattleMonster& xAtk = xState.Side(ZM_SIDE_PLAYER).Active();
	ZM_SC5RigVolatile(xAtk, ZM_VOLATILE_CHARGE);
	ZM_SC5RigVolatile(xAtk, ZM_VOLATILE_SEMI_INVULN);
	xAtk.m_uChargeMoveSlot = 0u;
	xAtk.m_eStatus = ZM_MAJOR_STATUS_SLEEP;
	xAtk.m_uStatusCounter = 2u;
	const u_int uPP = xAtk.m_axMoves[0].m_uCurPP;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);
	ZENITH_ASSERT_EQ(xAtk.m_axMoves[0].m_uCurPP, uPP);
	ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_USED));
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xAtk, ZM_VOLATILE_CHARGE));
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xAtk, ZM_VOLATILE_SEMI_INVULN));
	ZENITH_ASSERT_EQ(xAtk.m_uChargeMoveSlot, uZM_MAX_MOVES);
	ZENITH_ASSERT_EQ(xEvents.GetSize(), 3u,
		"G cancellation emits exactly failure, SEMI ended, then CHARGE ended");
	if (xEvents.GetSize() == 3u)
	{
		const ZM_BattleEvent& xFail = xEvents.Get(0u);
		const ZM_BattleEvent& xSemiEnd = xEvents.Get(1u);
		const ZM_BattleEvent& xChargeEnd = xEvents.Get(2u);
		ZENITH_ASSERT_EQ((u_int)xFail.m_eKind, (u_int)ZM_BATTLE_EVENT_MOVE_FAILED);
		ZENITH_ASSERT_EQ(xFail.m_uSide, (u_int)ZM_SIDE_PLAYER);
		ZENITH_ASSERT_EQ(xFail.m_iAux, (int)ZM_MOVE_FAIL_ASLEEP);
		ZENITH_ASSERT_EQ((u_int)xSemiEnd.m_eKind, (u_int)ZM_BATTLE_EVENT_VOLATILE_ENDED);
		ZENITH_ASSERT_EQ(xSemiEnd.m_uSide, (u_int)ZM_SIDE_PLAYER);
		ZENITH_ASSERT_EQ(xSemiEnd.m_iAmount, 0);
		ZENITH_ASSERT_EQ(xSemiEnd.m_iAux, (int)ZM_VOLATILE_SEMI_INVULN);
		ZENITH_ASSERT_EQ(xSemiEnd.m_iTag, 0);
		ZENITH_ASSERT_EQ((u_int)xChargeEnd.m_eKind, (u_int)ZM_BATTLE_EVENT_VOLATILE_ENDED);
		ZENITH_ASSERT_EQ(xChargeEnd.m_uSide, (u_int)ZM_SIDE_PLAYER);
		ZENITH_ASSERT_EQ(xChargeEnd.m_iAmount, 0);
		ZENITH_ASSERT_EQ(xChargeEnd.m_iAux, (int)ZM_VOLATILE_CHARGE);
		ZENITH_ASSERT_EQ(xChargeEnd.m_iTag, 0);
	}
}

ZENITH_TEST(ZM_Battle, SemiInvuln_SlowerEntryThenM3InterceptAndRelease)
{
	ZM_BattleMonsterSpec axPlayer[1] = {
		ZM_SC5Spec(ZM_SPECIES_BURRIT, ZM_MOVE_UNDERDELVE, ZM_MOVE_NONE, 10u, 250u) };
	ZM_BattleMonsterSpec axEnemy[1] = {
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_TORRENTCANNON, ZM_MOVE_NONE, 120u, 250u) };
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeTrainerConfig(), axPlayer, 1u, axEnemy, 1u, 1ull, 54ull);
	ZM_BattleRNG xMirror(1ull, 54ull);
	// Turn 1: faster enemy can hit before the slower user becomes semi-invulnerable.
	xMirror.RandBelow(100u);          // Torrent Cannon accuracy
	xMirror.Chance(1u, 24u);
	xMirror.RandRange(85u, 100u);
	ZM_SC5ResolveMoveTurn(xEngine);
	ZM_BattleMonster& xPlayer = xEngine.GetStateMutable().Side(ZM_SIDE_PLAYER).Active();
	ZENITH_ASSERT_TRUE(ZM_SC5HasVolatile(xPlayer, ZM_VOLATILE_CHARGE));
	ZENITH_ASSERT_TRUE(ZM_SC5HasVolatile(xPlayer, ZM_VOLATILE_SEMI_INVULN));
	ZENITH_ASSERT_TRUE(ZM_SC5CountForSide(xEngine.GetEvents(), ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_PLAYER) >= 1u);
	// EOT must not count down charge/semi.
	ZENITH_ASSERT_TRUE(ZM_SC5HasVolatile(xPlayer, ZM_VOLATILE_SEMI_INVULN));

	const u_int uBefore = xEngine.GetEventCount();
	const u_int uEnemyPP = xEngine.GetState().Side(ZM_SIDE_ENEMY).Active().m_axMoves[0].m_uCurPP;
	ZM_SC5ResolveMoveTurn(xEngine);
	// Turn 2: faster enemy announces/spends, but M3 sees vanished target before accuracy.
	const ZM_BattleEvent* pxFail = ZM_SC5FindEventPtr(xEngine.GetEvents(),
		ZM_BATTLE_EVENT_MOVE_FAILED, ZM_SIDE_PLAYER, uBefore);
	ZENITH_ASSERT_NOT_NULL(pxFail);
	if (pxFail != nullptr) { ZENITH_ASSERT_EQ(pxFail->m_iAux, (int)ZM_MOVE_FAIL_SEMI_INVULNERABLE); }
	ZENITH_ASSERT_EQ(xEngine.GetState().Side(ZM_SIDE_ENEMY).Active().m_axMoves[0].m_uCurPP, uEnemyPP - 1u);
	// Player then releases: no second PP, ends both states before its normal hit draws.
	xMirror.Chance(1u, 24u);
	xMirror.RandRange(85u, 100u);
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xPlayer, ZM_VOLATILE_CHARGE));
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xPlayer, ZM_VOLATILE_SEMI_INVULN));
	ZM_SC5AssertRNGCursor(xEngine.GetStateMutable(), xMirror,
		"semi intercept adds no draw; release consumes only its crit/roll");
}

ZENITH_TEST(ZM_Battle, Protect_M2PrecedesSemiInvulnerableM3)
{
	ZM_BattleState xState;
	BuildBattleState(xState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_WHETCLAW), 1ull, 54ull);
	ZM_BattleMonster& xDef = xState.Side(ZM_SIDE_ENEMY).Active();
	ZM_SC5RigVolatile(xDef, ZM_VOLATILE_PROTECT);
	ZM_SC5RigVolatile(xDef, ZM_VOLATILE_SEMI_INVULN);
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);
	const ZM_BattleEvent* pxFail = ZM_SC5FindEventPtr(xEvents,
		ZM_BATTLE_EVENT_MOVE_FAILED, ZM_SIDE_ENEMY);
	ZENITH_ASSERT_NOT_NULL(pxFail);
	if (pxFail != nullptr) { ZENITH_ASSERT_EQ(pxFail->m_iAux, (int)ZM_MOVE_FAIL_PROTECTED); }
	ZM_BattleRNG xMirror(1ull, 54ull);
	ZM_SC5AssertRNGCursor(xState, xMirror, "M2 Protect cancels before M3 and all hit draws");
}

ZENITH_TEST(ZM_Battle, Recharge_EstablishesOnConnectingHitAndKOOnly)
{
	const u_int64 ulSeed = 1ull; // known 90-accuracy hit in the existing SC3 fixtures
	for (u_int uCase = 0u; uCase < 2u; ++uCase)
	{
		ZM_BattleState xState;
		BuildBattleState(xState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_TITANBEAM),
			ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH, ZM_MOVE_NONE, 10u, 250u),
			ulSeed, 54ull);
		if (uCase == 1u) { xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP = 1u; }
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
		ZM_MoveExecutor::Execute(xCtx);
		ZENITH_ASSERT_TRUE(ZM_SC5HasVolatile(xState.Side(ZM_SIDE_PLAYER).Active(), ZM_VOLATILE_RECHARGE));
		const int iDamage = ZM_SC5FindEvent(xEvents, ZM_BATTLE_EVENT_DAMAGE_DEALT);
		const int iApply = ZM_SC5FindEvent(xEvents, ZM_BATTLE_EVENT_VOLATILE_APPLIED, ZM_SIDE_PLAYER);
		ZENITH_ASSERT_GE(iDamage, 0);
		ZENITH_ASSERT_GE(iApply, 0);
		ZENITH_ASSERT_LT(iDamage, iApply);
	}

	// Protect is a failed connection, so it never establishes recharge and pulls no hit draws.
	ZM_BattleState xProtectState;
	BuildBattleState(xProtectState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_TITANBEAM),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), ulSeed, 54ull);
	ZM_SC5RigVolatile(xProtectState.Side(ZM_SIDE_ENEMY).Active(), ZM_VOLATILE_PROTECT);
	Zenith_Vector<ZM_BattleEvent> xProtectEvents;
	ZM_MoveContext xProtectCtx = MakeCtx(xProtectState, xProtectEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xProtectCtx);
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xProtectState.Side(ZM_SIDE_PLAYER).Active(), ZM_VOLATILE_RECHARGE));
	ZM_BattleRNG xProtectMirror(ulSeed, 54ull);
	ZM_SC5AssertRNGCursor(xProtectState, xProtectMirror, "protected recharge move establishes nothing and draws nothing");

	// Miss and type immunity are the other non-connections and likewise never
	// establish Recharge or consume post-accuracy hit draws.
	u_int64 ulMissSeed = 0ull;
	for (u_int64 ulTry = 1ull; ulTry <= 4096ull; ++ulTry)
	{
		ZM_BattleRNG xTry(ulTry, 54ull);
		if (xTry.RandBelow(100u) >= 90u) { ulMissSeed = ulTry; break; }
	}
	ZENITH_ASSERT_GT(ulMissSeed, 0ull);
	ZM_BattleState xMissState;
	BuildBattleState(xMissState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_TITANBEAM),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), ulMissSeed, 54ull);
	Zenith_Vector<ZM_BattleEvent> xMissEvents;
	ZM_MoveContext xMissCtx = MakeCtx(xMissState, xMissEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xMissCtx);
	ZENITH_ASSERT_TRUE(ZM_HasKind(xMissEvents, ZM_BATTLE_EVENT_MOVE_MISSED));
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xMissState.Side(ZM_SIDE_PLAYER).Active(), ZM_VOLATILE_RECHARGE));
	ZM_BattleRNG xMissMirror(ulMissSeed, 54ull);
	ZENITH_ASSERT_GE(xMissMirror.RandBelow(100u),
		ZM_GetMoveData(ZM_MOVE_TITANBEAM).m_uAccuracy,
		"the mirrored Recharge fixture draw is deterministically a miss");
	ZM_SC5AssertRNGCursor(xMissState, xMissMirror, "missed recharge move consumes accuracy only");

	ZM_BattleState xImmuneState;
	BuildBattleState(xImmuneState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_TITANBEAM),
		ZM_SC5Spec(ZM_SPECIES_WISPET, ZM_MOVE_HEXBOLT), ulSeed, 54ull);
	Zenith_Vector<ZM_BattleEvent> xImmuneEvents;
	ZM_MoveContext xImmuneCtx = MakeCtx(xImmuneState, xImmuneEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xImmuneCtx);
	ZENITH_ASSERT_TRUE(ZM_HasKind(xImmuneEvents, ZM_BATTLE_EVENT_IMMUNE));
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xImmuneState.Side(ZM_SIDE_PLAYER).Active(), ZM_VOLATILE_RECHARGE));
	ZM_BattleRNG xImmuneMirror(ulSeed, 54ull);
	xImmuneMirror.RandBelow(100u);
	ZM_SC5AssertRNGCursor(xImmuneState, xImmuneMirror, "immune recharge move consumes accuracy only");
}

ZENITH_TEST(ZM_Battle, Recharge_G1CancelsEndsBeforeFreezeWithoutDraw)
{
	ZM_BattleState xState;
	BuildBattleState(xState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_WHETCLAW), 1ull, 54ull);
	ZM_BattleMonster& xAtk = xState.Side(ZM_SIDE_PLAYER).Active();
	ZM_SC5RigVolatile(xAtk, ZM_VOLATILE_RECHARGE);
	xAtk.m_eStatus = ZM_MAJOR_STATUS_FREEZE;
	const u_int uPP = xAtk.m_axMoves[0].m_uCurPP;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);
	ZENITH_ASSERT_EQ(xAtk.m_axMoves[0].m_uCurPP, uPP);
	ZENITH_ASSERT_EQ((u_int)xAtk.m_eStatus, (u_int)ZM_MAJOR_STATUS_FREEZE,
		"G1 prevents G2 mutation/draw");
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xAtk, ZM_VOLATILE_RECHARGE));
	ZENITH_ASSERT_EQ(xEvents.GetSize(), 2u);
	if (xEvents.GetSize() == 2u)
	{
		ZENITH_ASSERT_EQ((u_int)xEvents.Get(0).m_eKind, (u_int)ZM_BATTLE_EVENT_MOVE_FAILED);
		ZENITH_ASSERT_EQ(xEvents.Get(0).m_iAux, (int)ZM_MOVE_FAIL_RECHARGE);
		ZENITH_ASSERT_EQ((u_int)xEvents.Get(1).m_eKind, (u_int)ZM_BATTLE_EVENT_VOLATILE_ENDED);
		ZENITH_ASSERT_EQ(xEvents.Get(1).m_iAux, (int)ZM_VOLATILE_RECHARGE);
	}
	ZM_BattleRNG xMirror(1ull, 54ull);
	ZM_SC5AssertRNGCursor(xState, xMirror, "G1 Recharge consumes no draw and stops G2");
}

ZENITH_TEST(ZM_Battle, LockIn_EstablishesIncludingKOWithGuardedDuration)
{
	const u_int64 ulSeed = 0x1234ull;
	for (u_int uCase = 0u; uCase < 2u; ++uCase)
	{
		ZM_BattleState xState;
		BuildBattleState(xState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_BERSERKSPIN),
			ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH, ZM_MOVE_NONE, 10u, 300u),
			ulSeed, 54ull);
		if (uCase == 1u) { xState.Side(ZM_SIDE_ENEMY).Active().m_uCurHP = 1u; }
		ZM_BattleRNG xMirror(ulSeed, 54ull);
		xMirror.Chance(1u, 24u);
		xMirror.RandRange(85u, 100u);
		const u_int uTotalUses = xMirror.RandRange(2u, 3u);
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
		ZM_MoveExecutor::Execute(xCtx);
		const ZM_BattleMonster& xAtk = xState.Side(ZM_SIDE_PLAYER).Active();
		ZENITH_ASSERT_TRUE(ZM_SC5HasVolatile(xAtk, ZM_VOLATILE_LOCK));
		ZENITH_ASSERT_EQ(xAtk.m_uLockTurns, uTotalUses - 1u,
			"establishment counts use one and stores only future uses");
		ZENITH_ASSERT_EQ(xAtk.m_uLockMoveSlot, 0u);
		const ZM_BattleEvent* pxApply = ZM_SC5FindEventPtr(xEvents,
			ZM_BATTLE_EVENT_VOLATILE_APPLIED, ZM_SIDE_PLAYER);
		ZENITH_ASSERT_NOT_NULL(pxApply);
		if (pxApply != nullptr) { ZM_SC5AssertAppliedEncoding(*pxApply, ZM_SIDE_PLAYER, ZM_VOLATILE_LOCK, uTotalUses); }
		ZM_SC5AssertRNGCursor(xState, xMirror, "Lock draws duration only after its connecting hit");
	}

	// A 100-accuracy LOCK_IN move stopped by M6 immunity neither establishes
	// lock nor draws crit, roll, or duration.
	ZM_BattleState xImmuneState;
	BuildBattleState(xImmuneState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_BERSERKSPIN),
		ZM_SC5Spec(ZM_SPECIES_WISPET, ZM_MOVE_HEXBOLT), ulSeed, 54ull);
	Zenith_Vector<ZM_BattleEvent> xImmuneEvents;
	ZM_MoveContext xImmuneCtx = MakeCtx(xImmuneState, xImmuneEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xImmuneCtx);
	ZENITH_ASSERT_TRUE(ZM_HasKind(xImmuneEvents, ZM_BATTLE_EVENT_IMMUNE));
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xImmuneState.Side(ZM_SIDE_PLAYER).Active(), ZM_VOLATILE_LOCK));
	ZM_BattleRNG xImmuneMirror(ulSeed, 54ull);
	ZM_SC5AssertRNGCursor(xImmuneState, xImmuneMirror, "immune LOCK_IN consumes no guarded draws");

	// M2 Protect is another non-connection: a LOCK_IN move spends PP/announces,
	// but establishes no lock and never reaches crit, roll, or duration draws.
	ZM_BattleState xProtectState;
	BuildBattleState(xProtectState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_BERSERKSPIN),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), ulSeed, 54ull);
	ZM_SC5RigVolatile(xProtectState.Side(ZM_SIDE_ENEMY).Active(), ZM_VOLATILE_PROTECT);
	Zenith_Vector<ZM_BattleEvent> xProtectEvents;
	ZM_MoveContext xProtectCtx = MakeCtx(xProtectState, xProtectEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xProtectCtx);
	const ZM_BattleMonster& xProtectUser = xProtectState.Side(ZM_SIDE_PLAYER).Active();
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xProtectUser, ZM_VOLATILE_LOCK));
	ZENITH_ASSERT_EQ(xProtectUser.m_uLockTurns, 0u);
	ZENITH_ASSERT_EQ(xProtectUser.m_uLockMoveSlot, uZM_MAX_MOVES);
	ZENITH_ASSERT_FALSE(ZM_HasKind(xProtectEvents, ZM_BATTLE_EVENT_VOLATILE_APPLIED));
	ZENITH_ASSERT_EQ(xProtectEvents.GetSize(), 2u,
		"protected LOCK_IN emits only MOVE_USED then PROTECTED");
	if (xProtectEvents.GetSize() == 2u)
	{
		ZENITH_ASSERT_EQ((u_int)xProtectEvents.Get(0u).m_eKind,
			(u_int)ZM_BATTLE_EVENT_MOVE_USED);
		ZENITH_ASSERT_EQ((u_int)xProtectEvents.Get(1u).m_eKind,
			(u_int)ZM_BATTLE_EVENT_MOVE_FAILED);
		ZENITH_ASSERT_EQ(xProtectEvents.Get(1u).m_iAux, (int)ZM_MOVE_FAIL_PROTECTED);
	}
	ZM_BattleRNG xProtectMirror(ulSeed, 54ull);
	ZM_SC5AssertRNGCursor(xProtectState, xProtectMirror,
		"protected LOCK_IN establishes nothing and draws no duration");
}

ZENITH_TEST(ZM_Battle, LockIn_ForcesStoredSlotConsumesUsesAndEndsAtZeroPP)
{
	u_int64 ulSeed = 0ull;
	for (u_int64 ulTry = 1ull; ulTry <= 4096ull; ++ulTry)
	{
		ZM_BattleRNG xMirror(ulTry, 54ull);
		xMirror.Chance(1u, 24u);
		xMirror.RandRange(85u, 100u);
		if (xMirror.RandRange(2u, 3u) == 3u)
		{
			ulSeed = ulTry;
			break;
		}
	}
	ZENITH_ASSERT_GT(ulSeed, 0ull, "fixture requires two future locked uses");
	ZM_BattleMonsterSpec axPlayer[1] = {
		ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_BERSERKSPIN, ZM_MOVE_RAMBASH, 120u, 300u) };
	ZM_BattleMonsterSpec axEnemy[1] = {
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_WHETCLAW, ZM_MOVE_NONE, 10u, 400u) };
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeTrainerConfig(), axPlayer, 1u, axEnemy, 1u, ulSeed, 54ull);
	ZM_SC5ResolveMoveTurn(xEngine, 0u, 0u);
	ZM_BattleMonster& xPlayer = xEngine.GetStateMutable().Side(ZM_SIDE_PLAYER).Active();
	ZENITH_ASSERT_TRUE(ZM_SC5HasVolatile(xPlayer, ZM_VOLATILE_LOCK));
	ZENITH_ASSERT_EQ(xPlayer.m_uLockTurns, 2u, "three total uses store exactly two future uses");
	const u_int uRambashPP = xPlayer.m_axMoves[1].m_uCurPP;
	xPlayer.m_axMoves[0].m_uCurPP = 1u;
	xPlayer.m_iCritStage = 3;
	const u_int uBeforeUsed = ZM_SC5CountForSide(xEngine.GetEvents(), ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER);
	const u_int uBeforeEvents = xEngine.GetEventCount();
	ZM_SC5ResolveMoveTurn(xEngine, 1u, 0u);
	ZENITH_ASSERT_EQ(xEngine.GetEventCount(), uBeforeEvents + 8u,
		"the zero-PP lock lifecycle has one exact, non-conditional turn stream");
	if (xEngine.GetEventCount() == uBeforeEvents + 8u)
	{
		const ZM_BattleEvent& xTurnBegin = xEngine.GetEvent(uBeforeEvents + 0u);
		const ZM_BattleEvent& xForcedUse = xEngine.GetEvent(uBeforeEvents + 1u);
		const ZM_BattleEvent& xCrit = xEngine.GetEvent(uBeforeEvents + 2u);
		const ZM_BattleEvent& xDamage = xEngine.GetEvent(uBeforeEvents + 3u);
		const ZM_BattleEvent& xLockEnd = xEngine.GetEvent(uBeforeEvents + 4u);
		const ZM_BattleEvent& xEnemyUse = xEngine.GetEvent(uBeforeEvents + 5u);
		const ZM_BattleEvent& xEnemyStage = xEngine.GetEvent(uBeforeEvents + 6u);
		const ZM_BattleEvent& xTurnEnd = xEngine.GetEvent(uBeforeEvents + 7u);
		ZENITH_ASSERT_EQ((u_int)xTurnBegin.m_eKind, (u_int)ZM_BATTLE_EVENT_TURN_BEGIN);
		ZENITH_ASSERT_EQ((u_int)xForcedUse.m_eKind, (u_int)ZM_BATTLE_EVENT_MOVE_USED);
		ZENITH_ASSERT_EQ(xForcedUse.m_uSide, (u_int)ZM_SIDE_PLAYER);
		ZENITH_ASSERT_EQ(xForcedUse.m_uMoveId, (u_int)ZM_MOVE_BERSERKSPIN);
		ZENITH_ASSERT_EQ((u_int)xCrit.m_eKind, (u_int)ZM_BATTLE_EVENT_CRIT);
		ZENITH_ASSERT_EQ((u_int)xDamage.m_eKind, (u_int)ZM_BATTLE_EVENT_DAMAGE_DEALT);
		ZENITH_ASSERT_EQ((u_int)xLockEnd.m_eKind, (u_int)ZM_BATTLE_EVENT_VOLATILE_ENDED);
		ZENITH_ASSERT_EQ(xLockEnd.m_uSide, (u_int)ZM_SIDE_PLAYER);
		ZENITH_ASSERT_EQ(xLockEnd.m_iAux, (int)ZM_VOLATILE_LOCK);
		ZENITH_ASSERT_EQ((u_int)xEnemyUse.m_eKind, (u_int)ZM_BATTLE_EVENT_MOVE_USED);
		ZENITH_ASSERT_EQ(xEnemyUse.m_uSide, (u_int)ZM_SIDE_ENEMY);
		ZENITH_ASSERT_EQ((u_int)xEnemyStage.m_eKind,
			(u_int)ZM_BATTLE_EVENT_STAT_STAGE_CHANGED);
		ZENITH_ASSERT_EQ((u_int)xTurnEnd.m_eKind, (u_int)ZM_BATTLE_EVENT_TURN_END);
	}
	ZENITH_ASSERT_EQ(xPlayer.m_axMoves[1].m_uCurPP, uRambashPP, "submitted other slot is ignored while locked");
	ZENITH_ASSERT_EQ(xPlayer.m_axMoves[0].m_uCurPP, 0u, "the forced stored-slot use spends its final PP");
	ZENITH_ASSERT_EQ(ZM_SC5CountForSide(xEngine.GetEvents(), ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER), uBeforeUsed + 1u);
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xPlayer, ZM_VOLATILE_LOCK));
	ZENITH_ASSERT_EQ(xPlayer.m_uLockTurns, 0u);
	ZENITH_ASSERT_EQ(xPlayer.m_uLockMoveSlot, uZM_MAX_MOVES);
	ZENITH_ASSERT_FALSE(ZM_HasKind(xEngine.GetEvents(), ZM_BATTLE_EVENT_NO_PP));
	ZENITH_ASSERT_EQ(ZM_CountKind(xEngine.GetEvents(), ZM_BATTLE_EVENT_VOLATILE_ENDED), 1u);
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xPlayer, ZM_VOLATILE_CONFUSED),
		"lock expiry never adds post-lock confusion");
	for (u_int u = uBeforeEvents; u < xEngine.GetEventCount(); ++u)
	{
		const ZM_BattleEvent& xEvent = xEngine.GetEvent(u);
		ZENITH_ASSERT_FALSE(xEvent.m_eKind == ZM_BATTLE_EVENT_VOLATILE_APPLIED &&
			xEvent.m_iAux == (int)ZM_VOLATILE_CONFUSED,
			"lock expiry emits no confusion application");
	}

	// Natural counter expiry is independent of PP exhaustion. Pin a two-use
	// duration (the establishing hit plus one future forced stored-slot use).
	u_int64 ulNaturalSeed = 0ull;
	for (u_int64 ulTry = 1ull; ulTry <= 4096ull; ++ulTry)
	{
		ZM_BattleRNG xNaturalTry(ulTry, 54ull);
		xNaturalTry.Chance(1u, 24u);
		xNaturalTry.RandRange(85u, 100u);
		if (xNaturalTry.RandRange(2u, 3u) == 2u)
		{
			ulNaturalSeed = ulTry;
			break;
		}
	}
	ZENITH_ASSERT_GT(ulNaturalSeed, 0ull,
		"fixture requires exactly one future locked use");
	ZM_BattleMonsterSpec axNaturalPlayer[1] = {
		ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_BERSERKSPIN,
			ZM_MOVE_RAMBASH, 120u, 300u) };
	ZM_BattleMonsterSpec axNaturalEnemy[1] = {
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_WHETCLAW,
			ZM_MOVE_NONE, 10u, 800u) };
	ZM_BattleEngine xNaturalEngine;
	xNaturalEngine.Begin(MakeTrainerConfig(), axNaturalPlayer, 1u,
		axNaturalEnemy, 1u, ulNaturalSeed, 54ull);
	ZM_SC5ResolveMoveTurn(xNaturalEngine, 0u, 0u);
	ZM_BattleMonster& xNaturalPlayer =
		xNaturalEngine.GetStateMutable().Side(ZM_SIDE_PLAYER).Active();
	ZENITH_ASSERT_TRUE(ZM_SC5HasVolatile(xNaturalPlayer, ZM_VOLATILE_LOCK));
	ZENITH_ASSERT_EQ(xNaturalPlayer.m_uLockTurns, 1u);
	ZENITH_ASSERT_EQ(xNaturalPlayer.m_uLockMoveSlot, 0u);
	const ZM_BattleEvent* pxNaturalApply = ZM_SC5FindEventPtr(
		xNaturalEngine.GetEvents(), ZM_BATTLE_EVENT_VOLATILE_APPLIED, ZM_SIDE_PLAYER);
	ZENITH_ASSERT_NOT_NULL(pxNaturalApply);
	if (pxNaturalApply != nullptr)
	{
		ZM_SC5AssertAppliedEncoding(*pxNaturalApply, ZM_SIDE_PLAYER,
			ZM_VOLATILE_LOCK, 2u);
	}

	const u_int uNaturalLockPP = xNaturalPlayer.m_axMoves[0].m_uCurPP;
	const u_int uNaturalOtherPP = xNaturalPlayer.m_axMoves[1].m_uCurPP;
	const u_int uNaturalStart = xNaturalEngine.GetEventCount();
	const u_int uNaturalUses = ZM_SC5CountForSide(xNaturalEngine.GetEvents(),
		ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER);
	const u_int uNaturalEnds = ZM_CountKind(xNaturalEngine.GetEvents(),
		ZM_BATTLE_EVENT_VOLATILE_ENDED);
	ZENITH_ASSERT_EQ(uNaturalEnds, 0u);
	ZENITH_ASSERT_GT(uNaturalLockPP, 1u,
		"natural expiry fixture must retain PP after its final forced use");
	ZM_SC5ResolveMoveTurn(xNaturalEngine, 1u, 0u);
	ZENITH_ASSERT_EQ(xNaturalPlayer.m_axMoves[0].m_uCurPP, uNaturalLockPP - 1u);
	ZENITH_ASSERT_GT(xNaturalPlayer.m_axMoves[0].m_uCurPP, 0u);
	ZENITH_ASSERT_EQ(xNaturalPlayer.m_axMoves[1].m_uCurPP, uNaturalOtherPP,
		"the submitted alternate slot is ignored for the one future locked use");
	ZENITH_ASSERT_EQ(ZM_SC5CountForSide(xNaturalEngine.GetEvents(),
		ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_PLAYER), uNaturalUses + 1u);
	const ZM_BattleEvent* pxNaturalUse = ZM_SC5FindEventPtr(
		xNaturalEngine.GetEvents(), ZM_BATTLE_EVENT_MOVE_USED,
		ZM_SIDE_PLAYER, uNaturalStart);
	ZENITH_ASSERT_NOT_NULL(pxNaturalUse);
	if (pxNaturalUse != nullptr)
	{
		ZENITH_ASSERT_EQ(pxNaturalUse->m_uMoveId, (u_int)ZM_MOVE_BERSERKSPIN);
	}
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xNaturalPlayer, ZM_VOLATILE_LOCK));
	ZENITH_ASSERT_EQ(xNaturalPlayer.m_uLockTurns, 0u);
	ZENITH_ASSERT_EQ(xNaturalPlayer.m_uLockMoveSlot, uZM_MAX_MOVES);
	ZENITH_ASSERT_FALSE(ZM_HasKind(xNaturalEngine.GetEvents(), ZM_BATTLE_EVENT_NO_PP));
	ZENITH_ASSERT_EQ(ZM_CountKind(xNaturalEngine.GetEvents(),
		ZM_BATTLE_EVENT_VOLATILE_ENDED), 1u);
	const ZM_BattleEvent* pxNaturalEnd = ZM_SC5FindEventPtr(
		xNaturalEngine.GetEvents(), ZM_BATTLE_EVENT_VOLATILE_ENDED,
		ZM_SIDE_PLAYER, uNaturalStart);
	ZENITH_ASSERT_NOT_NULL(pxNaturalEnd);
	if (pxNaturalEnd != nullptr)
	{
		ZENITH_ASSERT_EQ(pxNaturalEnd->m_iAux, (int)ZM_VOLATILE_LOCK);
	}
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xNaturalPlayer, ZM_VOLATILE_CONFUSED));
	for (u_int u = uNaturalStart; u < xNaturalEngine.GetEventCount(); ++u)
	{
		const ZM_BattleEvent& xEvent = xNaturalEngine.GetEvent(u);
		ZENITH_ASSERT_FALSE(xEvent.m_eKind == ZM_BATTLE_EVENT_VOLATILE_APPLIED &&
			xEvent.m_iAux == (int)ZM_VOLATILE_CONFUSED,
			"natural lock expiry emits no post-lock confusion");
	}
}

ZENITH_TEST(ZM_Battle, LockIn_GateCancellationConsumesNoUseOrPP)
{
	ZM_BattleState xState;
	BuildBattleState(xState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_BERSERKSPIN),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_WHETCLAW), 1ull, 54ull);
	ZM_BattleMonster& xAtk = xState.Side(ZM_SIDE_PLAYER).Active();
	ZM_SC5RigVolatile(xAtk, ZM_VOLATILE_LOCK, 2u);
	xAtk.m_uLockMoveSlot = 0u;
	ZM_SC5RigVolatile(xAtk, ZM_VOLATILE_FLINCH);
	const u_int uPP = xAtk.m_axMoves[0].m_uCurPP;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);
	ZENITH_ASSERT_EQ(xAtk.m_axMoves[0].m_uCurPP, uPP);
	ZENITH_ASSERT_EQ(xAtk.m_uLockTurns, 2u);
	ZENITH_ASSERT_TRUE(ZM_SC5HasVolatile(xAtk, ZM_VOLATILE_LOCK));
	ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_USED));
}

ZENITH_TEST(ZM_Battle, Swagger_HitOrdersStageBeforeNewConfusionAndExactDraws)
{
	// Bravado accuracy 85, then no stat draw, then guarded confusion duration.
	const u_int uAccuracy = ZM_GetMoveData(ZM_MOVE_BRAVADO).m_uAccuracy;
	u_int64 ulHitSeed = 0ull;
	for (u_int64 ulTry = 1ull; ulTry <= 4096ull; ++ulTry)
	{
		ZM_BattleRNG xMirror(ulTry, 54ull);
		if (xMirror.RandBelow(100u) < uAccuracy) { ulHitSeed = ulTry; break; }
	}
	ZENITH_ASSERT_GT(ulHitSeed, 0ull);
	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunPlayerMove(ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_BRAVADO),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), ulHitSeed, xState, xEvents);
	ZM_BattleRNG xMirror(ulHitSeed, 54ull);
	ZENITH_ASSERT_LT(xMirror.RandBelow(100u), uAccuracy,
		"the mirrored Swagger hit fixture must connect");
	const u_int uDuration = xMirror.RandRange(1u, 4u);
	const ZM_BattleMonster& xTarget = xState.Side(ZM_SIDE_ENEMY).Active();
	ZENITH_ASSERT_EQ(xTarget.m_aiStage[ZM_BATTLE_STAT_ATTACK], 2);
	ZENITH_ASSERT_TRUE(ZM_SC5HasVolatile(xTarget, ZM_VOLATILE_CONFUSED));
	ZENITH_ASSERT_EQ(xTarget.m_uConfuseTurns, uDuration);
	const int iStage = ZM_SC5FindEvent(xEvents, ZM_BATTLE_EVENT_STAT_STAGE_CHANGED, ZM_SIDE_ENEMY);
	const int iApply = ZM_SC5FindEvent(xEvents, ZM_BATTLE_EVENT_VOLATILE_APPLIED, ZM_SIDE_ENEMY);
	ZENITH_ASSERT_GE(iStage, 0);
	ZENITH_ASSERT_GE(iApply, 0);
	ZENITH_ASSERT_LT(iStage, iApply);
	ZM_SC5AssertRNGCursor(xState, xMirror, "Swagger draws accuracy then new-confusion duration only");
}

ZENITH_TEST(ZM_Battle, Swagger_ComponentSuccessAndBothBlockedRules)
{
	const u_int uBoostedAccuracy = ZM_ApplyStatStage(
		ZM_GetMoveData(ZM_MOVE_BRAVADO).m_uAccuracy, iZM_MAX_STAGE);
	// Attack maxed, confusion new: confusion succeeds; no STAT_MAXED failure.
	ZM_BattleState xConfuseState;
	BuildBattleState(xConfuseState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_BRAVADO),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), 1ull, 54ull);
	xConfuseState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[ZM_BATTLE_STAT_ACCURACY] = iZM_MAX_STAGE;
	xConfuseState.Side(ZM_SIDE_ENEMY).Active().m_aiStage[ZM_BATTLE_STAT_ATTACK] = iZM_MAX_STAGE;
	Zenith_Vector<ZM_BattleEvent> xConfuseEvents;
	ZM_MoveContext xConfuseCtx = MakeCtx(xConfuseState, xConfuseEvents, ZM_SIDE_PLAYER, 0u);
	ZM_BattleRNG xConfuseMirror(1ull, 54ull);
	ZENITH_ASSERT_LT(xConfuseMirror.RandBelow(100u), uBoostedAccuracy,
		"the mirrored +6-accuracy Swagger draw must connect");
	const u_int uConfuseDuration = xConfuseMirror.RandRange(1u, 4u);
	ZM_MoveExecutor::Execute(xConfuseCtx);
	ZENITH_ASSERT_TRUE(ZM_SC5HasVolatile(xConfuseState.Side(ZM_SIDE_ENEMY).Active(), ZM_VOLATILE_CONFUSED));
	ZENITH_ASSERT_EQ(xConfuseState.Side(ZM_SIDE_ENEMY).Active().m_uConfuseTurns, uConfuseDuration);
	ZENITH_ASSERT_FALSE(ZM_HasKind(xConfuseEvents, ZM_BATTLE_EVENT_MOVE_FAILED));
	ZM_SC5AssertRNGCursor(xConfuseState, xConfuseMirror,
		"+6-accuracy Swagger still draws accuracy, then new-confusion duration");

	// Confusion already present, attack can rise: stat succeeds, duration not refreshed/drawn.
	ZM_BattleState xRaiseState;
	BuildBattleState(xRaiseState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_BRAVADO),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), 1ull, 54ull);
	xRaiseState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[ZM_BATTLE_STAT_ACCURACY] = iZM_MAX_STAGE;
	ZM_BattleMonster& xRaiseTarget = xRaiseState.Side(ZM_SIDE_ENEMY).Active();
	ZM_SC5RigVolatile(xRaiseTarget, ZM_VOLATILE_CONFUSED, 3u);
	Zenith_Vector<ZM_BattleEvent> xRaiseEvents;
	ZM_MoveContext xRaiseCtx = MakeCtx(xRaiseState, xRaiseEvents, ZM_SIDE_PLAYER, 0u);
	ZM_BattleRNG xRaiseMirror(1ull, 54ull);
	ZENITH_ASSERT_LT(xRaiseMirror.RandBelow(100u), uBoostedAccuracy,
		"the already-confused Swagger fixture must still connect");
	ZM_MoveExecutor::Execute(xRaiseCtx);
	ZENITH_ASSERT_EQ(xRaiseTarget.m_aiStage[ZM_BATTLE_STAT_ATTACK], 2);
	ZENITH_ASSERT_EQ(xRaiseTarget.m_uConfuseTurns, 3u);
	ZENITH_ASSERT_TRUE(ZM_HasKind(xRaiseEvents, ZM_BATTLE_EVENT_STAT_STAGE_CHANGED));
	ZENITH_ASSERT_FALSE(ZM_HasKind(xRaiseEvents, ZM_BATTLE_EVENT_MOVE_FAILED));
	ZM_SC5AssertRNGCursor(xRaiseState, xRaiseMirror,
		"already-confused +6-accuracy Swagger draws accuracy but no duration");

	// Both components blocked: exactly one target-locus VOLATILE_BLOCKED.
	ZM_BattleState xBlockedState;
	BuildBattleState(xBlockedState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_BRAVADO),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), 1ull, 54ull);
	xBlockedState.Side(ZM_SIDE_PLAYER).Active().m_aiStage[ZM_BATTLE_STAT_ACCURACY] = iZM_MAX_STAGE;
	ZM_BattleMonster& xBlocked = xBlockedState.Side(ZM_SIDE_ENEMY).Active();
	xBlocked.m_aiStage[ZM_BATTLE_STAT_ATTACK] = iZM_MAX_STAGE;
	ZM_SC5RigVolatile(xBlocked, ZM_VOLATILE_CONFUSED, 2u);
	Zenith_Vector<ZM_BattleEvent> xBlockedEvents;
	ZM_MoveContext xBlockedCtx = MakeCtx(xBlockedState, xBlockedEvents, ZM_SIDE_PLAYER, 0u);
	ZM_BattleRNG xBlockedMirror(1ull, 54ull);
	ZENITH_ASSERT_LT(xBlockedMirror.RandBelow(100u), uBoostedAccuracy,
		"the both-blocked Swagger fixture must first connect");
	ZM_MoveExecutor::Execute(xBlockedCtx);
	ZENITH_ASSERT_EQ(ZM_CountKind(xBlockedEvents, ZM_BATTLE_EVENT_MOVE_FAILED), 1u);
	const ZM_BattleEvent* pxFail = ZM_SC5FindEventPtr(xBlockedEvents,
		ZM_BATTLE_EVENT_MOVE_FAILED, ZM_SIDE_ENEMY);
	ZENITH_ASSERT_NOT_NULL(pxFail);
	if (pxFail != nullptr) { ZENITH_ASSERT_EQ(pxFail->m_iAux, (int)ZM_MOVE_FAIL_VOLATILE_BLOCKED); }
	ZM_SC5AssertRNGCursor(xBlockedState, xBlockedMirror,
		"both-blocked +6-accuracy Swagger draws accuracy only");
}

ZENITH_TEST(ZM_Battle, Swagger_MissAppliesNothingAndDrawsAccuracyOnly)
{
	const u_int uAccuracy = ZM_GetMoveData(ZM_MOVE_BRAVADO).m_uAccuracy;
	u_int64 ulMissSeed = 0ull;
	for (u_int64 ulTry = 1ull; ulTry <= 4096ull; ++ulTry)
	{
		ZM_BattleRNG xMirror(ulTry, 54ull);
		if (xMirror.RandBelow(100u) >= uAccuracy) { ulMissSeed = ulTry; break; }
	}
	ZENITH_ASSERT_GT(ulMissSeed, 0ull);
	ZM_BattleState xState;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_RunPlayerMove(ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_BRAVADO),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), ulMissSeed, xState, xEvents);
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_ENEMY).Active().m_aiStage[ZM_BATTLE_STAT_ATTACK], 0);
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xState.Side(ZM_SIDE_ENEMY).Active(), ZM_VOLATILE_CONFUSED));
	ZENITH_ASSERT_TRUE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_MISSED));
	ZM_BattleRNG xMirror(ulMissSeed, 54ull);
	ZENITH_ASSERT_GE(xMirror.RandBelow(100u), uAccuracy,
		"the mirrored Swagger miss fixture must miss");
	ZM_SC5AssertRNGCursor(xState, xMirror, "missed Swagger draws accuracy only");
}

ZENITH_TEST(ZM_Battle, DoSwitch_ValidAndInvalidDestinationContract)
{
	ZM_BattleMonsterSpec axPlayer[1] = { ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH) };
	ZM_BattleMonsterSpec axEnemy[3] = {
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH),
		ZM_SC5Spec(ZM_SPECIES_KINDLET, ZM_MOVE_RAMBASH),
		ZM_SC5Spec(ZM_SPECIES_FERNFAWN, ZM_MOVE_RAMBASH) };
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeTrainerConfig(), axPlayer, 1u, axEnemy, 3u, 1ull, 54ull);
	const u_int uHeaderCount = xEngine.GetEventCount();
	ZENITH_ASSERT_FALSE(xEngine.DoSwitch(ZM_SIDE_COUNT, 0u),
		"the first invalid-side enum boundary is rejected before Side() indexing");
	ZENITH_ASSERT_FALSE(xEngine.DoSwitch(ZM_SIDE_ENEMY, 0u), "current slot is invalid");
	ZENITH_ASSERT_FALSE(xEngine.DoSwitch(ZM_SIDE_ENEMY, 3u), "out-of-range slot is invalid");
	xEngine.GetStateMutable().Side(ZM_SIDE_ENEMY).m_xParty.Get(1).m_uCurHP = 0u;
	ZENITH_ASSERT_FALSE(xEngine.DoSwitch(ZM_SIDE_ENEMY, 1u), "fainted destination is invalid");
	ZENITH_ASSERT_EQ(xEngine.GetEventCount(), uHeaderCount, "invalid switches emit nothing");
	ZENITH_ASSERT_TRUE(xEngine.DoSwitch(ZM_SIDE_ENEMY, 2u));
	ZENITH_ASSERT_EQ(xEngine.GetState().Side(ZM_SIDE_ENEMY).m_uActiveSlot, 2u);
	ZENITH_ASSERT_EQ(xEngine.GetEventCount(), uHeaderCount + 1u);
	const ZM_BattleEvent& xSwitch = xEngine.GetEvent(uHeaderCount);
	ZENITH_ASSERT_EQ((u_int)xSwitch.m_eKind, (u_int)ZM_BATTLE_EVENT_SWITCH_IN);
	ZENITH_ASSERT_EQ(xSwitch.m_uSide, (u_int)ZM_SIDE_ENEMY);
	ZENITH_ASSERT_EQ(xSwitch.m_uSlot, 2u);
	ZENITH_ASSERT_EQ(xSwitch.m_uSpeciesId, (u_int)ZM_SPECIES_FERNFAWN);
}

ZENITH_TEST(ZM_Battle, DoSwitch_ResetOutgoingTransientsPreservePersistentState)
{
	ZM_BattleMonsterSpec axPlayer[1] = { ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH) };
	ZM_BattleMonsterSpec axEnemy[2] = {
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH),
		ZM_SC5Spec(ZM_SPECIES_KINDLET, ZM_MOVE_RAMBASH) };
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeTrainerConfig(), axPlayer, 1u, axEnemy, 2u, 1ull, 54ull);
	ZM_BattleSide& xSide = xEngine.GetStateMutable().Side(ZM_SIDE_ENEMY);
	ZM_BattleMonster& xOutgoing = xSide.Active();
	const ZM_SPECIES_ID eSpecies = xOutgoing.m_eSpecies;
	const ZM_ABILITY_ID eAbility = xOutgoing.m_eAbility;
	const u_int uHP = xOutgoing.m_uCurHP - 1u;
	xOutgoing.m_uCurHP = uHP;
	xOutgoing.m_eStatus = ZM_MAJOR_STATUS_TOXIC;
	xOutgoing.m_uStatusCounter = 7u;
	const u_int uPP = --xOutgoing.m_axMoves[0].m_uCurPP;
	xOutgoing.m_uVolatileMask = (u_int)ZM_VOLATILE_CONFUSED | (u_int)ZM_VOLATILE_FLINCH |
		(u_int)ZM_VOLATILE_LEECH_SEED | (u_int)ZM_VOLATILE_PROTECT | (u_int)ZM_VOLATILE_CHARGE |
		(u_int)ZM_VOLATILE_SEMI_INVULN | (u_int)ZM_VOLATILE_RECHARGE | (u_int)ZM_VOLATILE_LOCK |
		(u_int)ZM_VOLATILE_TRAP | (u_int)ZM_VOLATILE_TAUNT;
	xOutgoing.m_uConfuseTurns = 4u;
	xOutgoing.m_uTrapTurns = 5u;
	xOutgoing.m_uTauntTurns = 3u;
	xOutgoing.m_uLockTurns = 2u;
	xOutgoing.m_uChargeMoveSlot = 0u;
	xOutgoing.m_uLockMoveSlot = 0u;
	xOutgoing.m_eLeechSourceSide = ZM_SIDE_PLAYER;
	xOutgoing.m_bEndureThisTurn = true;
	for (u_int u = 0u; u < ZM_BATTLE_STAT_COUNT; ++u) { xOutgoing.m_aiStage[u] = (u & 1u) ? -3 : 4; }
	xOutgoing.m_iCritStage = 3;
	ZM_BattleMonster& xIncoming = xSide.m_xParty.Get(1);
	xIncoming.m_eStatus = ZM_MAJOR_STATUS_BURN;
	xIncoming.m_uStatusCounter = 9u;
	xIncoming.m_uCurHP -= 2u;
	const u_int uIncomingHP = xIncoming.m_uCurHP;
	const u_int uBefore = xEngine.GetEventCount();
	ZENITH_ASSERT_TRUE(xEngine.DoSwitch(ZM_SIDE_ENEMY, 1u));
	ZENITH_ASSERT_EQ(xOutgoing.m_uVolatileMask, (u_int)ZM_VOLATILE_NONE);
	ZENITH_ASSERT_EQ(xOutgoing.m_uConfuseTurns, 0u);
	ZENITH_ASSERT_EQ(xOutgoing.m_uTrapTurns, 0u);
	ZENITH_ASSERT_EQ(xOutgoing.m_uTauntTurns, 0u);
	ZENITH_ASSERT_EQ(xOutgoing.m_uLockTurns, 0u);
	ZENITH_ASSERT_EQ(xOutgoing.m_uChargeMoveSlot, uZM_MAX_MOVES);
	ZENITH_ASSERT_EQ(xOutgoing.m_uLockMoveSlot, uZM_MAX_MOVES);
	ZENITH_ASSERT_EQ((u_int)xOutgoing.m_eLeechSourceSide, (u_int)ZM_SIDE_COUNT);
	ZENITH_ASSERT_FALSE(xOutgoing.m_bEndureThisTurn);
	for (u_int u = 0u; u < ZM_BATTLE_STAT_COUNT; ++u) { ZENITH_ASSERT_EQ(xOutgoing.m_aiStage[u], 0); }
	ZENITH_ASSERT_EQ(xOutgoing.m_iCritStage, 0);
	ZENITH_ASSERT_EQ((u_int)xOutgoing.m_eStatus, (u_int)ZM_MAJOR_STATUS_TOXIC);
	ZENITH_ASSERT_EQ(xOutgoing.m_uStatusCounter, 7u);
	ZENITH_ASSERT_EQ(xOutgoing.m_uCurHP, uHP);
	ZENITH_ASSERT_EQ(xOutgoing.m_axMoves[0].m_uCurPP, uPP);
	ZENITH_ASSERT_EQ((u_int)xOutgoing.m_eSpecies, (u_int)eSpecies);
	ZENITH_ASSERT_EQ((u_int)xOutgoing.m_eAbility, (u_int)eAbility);
	ZENITH_ASSERT_EQ((u_int)xIncoming.m_eStatus, (u_int)ZM_MAJOR_STATUS_BURN);
	ZENITH_ASSERT_EQ(xIncoming.m_uStatusCounter, 9u);
	ZENITH_ASSERT_EQ(xIncoming.m_uCurHP, uIncomingHP);
	ZENITH_ASSERT_EQ(xEngine.GetEventCount(), uBefore + 1u, "switch cleanup emits only SWITCH_IN");
}

ZENITH_TEST(ZM_Battle, ForceSwitch_PrimaryLowestEligibleNoTargetAndNoRNG)
{
	ZM_BattleMonsterSpec axPlayer[1] = {
		ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_BELLOW, ZM_MOVE_NONE, 10u, 250u) };
	ZM_BattleMonsterSpec axEnemy[3] = {
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_WHETCLAW, ZM_MOVE_NONE, 120u, 250u),
		ZM_SC5Spec(ZM_SPECIES_KINDLET, ZM_MOVE_RAMBASH),
		ZM_SC5Spec(ZM_SPECIES_FERNFAWN, ZM_MOVE_RAMBASH) };
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeTrainerConfig(), axPlayer, 1u, axEnemy, 3u, 1ull, 54ull);
	ZM_BattleRNG xMirror(1ull, 54ull);
	ZM_SC5ResolveMoveTurn(xEngine);
	ZENITH_ASSERT_EQ(xEngine.GetState().Side(ZM_SIDE_ENEMY).m_uActiveSlot, 1u,
		"FORCE_SWITCH chooses lowest-index eligible bench");
	ZM_SC5AssertRNGCursor(xEngine.GetStateMutable(), xMirror, "self setup + deterministic forced switch consume no RNG");

	ZM_BattleMonsterSpec axSoloEnemy[1] = {
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_WHETCLAW, ZM_MOVE_NONE, 120u, 250u) };
	ZM_BattleEngine xNoTarget;
	xNoTarget.Begin(MakeTrainerConfig(), axPlayer, 1u, axSoloEnemy, 1u, 1ull, 54ull);
	ZM_SC5ResolveMoveTurn(xNoTarget);
	const ZM_BattleEvent* pxFail = ZM_SC5FindEventPtr(xNoTarget.GetEvents(),
		ZM_BATTLE_EVENT_MOVE_FAILED, ZM_SIDE_ENEMY);
	ZENITH_ASSERT_NOT_NULL(pxFail);
	if (pxFail != nullptr) { ZENITH_ASSERT_EQ(pxFail->m_iAux, (int)ZM_MOVE_FAIL_NO_SWITCH_TARGET); }
}

ZENITH_TEST(ZM_Battle, ForceSwitch_DamagingCarrierDamageThenSwitchButNotOnKO)
{
	const u_int64 ulSeed = 1ull;
	for (u_int uCase = 0u; uCase < 2u; ++uCase)
	{
		ZM_BattleMonsterSpec axPlayer[1] = {
			ZM_SC5Spec(ZM_SPECIES_FINLET, ZM_MOVE_UNDERTOW, ZM_MOVE_NONE, 10u, 250u) };
		ZM_BattleMonsterSpec axEnemy[2] = {
			ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_WHETCLAW, ZM_MOVE_NONE, 120u, 300u),
			ZM_SC5Spec(ZM_SPECIES_KINDLET, ZM_MOVE_RAMBASH) };
		ZM_BattleEngine xEngine;
		xEngine.Begin(MakeTrainerConfig(), axPlayer, 1u, axEnemy, 2u, ulSeed, 54ull);
		if (uCase == 1u) { xEngine.GetStateMutable().Side(ZM_SIDE_ENEMY).Active().m_uCurHP = 1u; }
		ZM_SC5RigVolatile(xEngine.GetStateMutable().Side(ZM_SIDE_ENEMY).Active(), ZM_VOLATILE_TRAP, 5u);
		ZM_SC5ResolveMoveTurn(xEngine);
		const int iDamage = ZM_SC5FindEvent(xEngine.GetEvents(), ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_SIDE_ENEMY);
		const int iSwitch = ZM_SC5FindEvent(xEngine.GetEvents(), ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_ENEMY, 3u);
		ZENITH_ASSERT_GE(iDamage, 0);
		if (uCase == 0u)
		{
			ZENITH_ASSERT_GE(iSwitch, 0);
			ZENITH_ASSERT_LT(iDamage, iSwitch);
			ZENITH_ASSERT_EQ(xEngine.GetState().Side(ZM_SIDE_ENEMY).m_uActiveSlot, 1u);
		}
		else
		{
			ZENITH_ASSERT_TRUE(iSwitch < 0, "damaging FORCE_SWITCH never switches a KO target");
			ZENITH_ASSERT_EQ(xEngine.GetState().Side(ZM_SIDE_ENEMY).m_uActiveSlot, 0u);
		}
	}

	// Unavailable damaging secondary phasing is silent: damage remains, there is
	// no NO_SWITCH_TARGET failure, and the only SWITCH_INs are Begin's headers.
	ZM_BattleMonsterSpec axSoloPlayer[1] = {
		ZM_SC5Spec(ZM_SPECIES_FINLET, ZM_MOVE_UNDERTOW, ZM_MOVE_NONE, 120u, 250u) };
	ZM_BattleMonsterSpec axSoloEnemy[1] = {
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_WHETCLAW, ZM_MOVE_NONE, 10u, 300u) };
	ZM_BattleEngine xSolo;
	xSolo.Begin(MakeTrainerConfig(), axSoloPlayer, 1u, axSoloEnemy, 1u, ulSeed, 54ull);
	ZM_BattleRNG xSoloMirror(ulSeed, 54ull);
	xSoloMirror.Chance(1u, 24u);
	xSoloMirror.RandRange(85u, 100u);
	ZM_SC5ResolveMoveTurn(xSolo);
	ZENITH_ASSERT_TRUE(ZM_HasKind(xSolo.GetEvents(), ZM_BATTLE_EVENT_DAMAGE_DEALT));
	ZENITH_ASSERT_EQ(ZM_CountKind(xSolo.GetEvents(), ZM_BATTLE_EVENT_SWITCH_IN), 2u);
	ZENITH_ASSERT_FALSE(ZM_HasKind(xSolo.GetEvents(), ZM_BATTLE_EVENT_MOVE_FAILED));
	ZENITH_ASSERT_EQ(xSolo.GetState().Side(ZM_SIDE_ENEMY).m_uActiveSlot, 0u);
	ZM_SC5AssertRNGCursor(xSolo.GetStateMutable(), xSoloMirror,
		"no-bench damaging FORCE_SWITCH consumes only ordinary crit+roll draws");
}

ZENITH_TEST(ZM_Battle, ForceSwitch_FirstMoverSkipsQueuedOldMonsterAction)
{
	// Equal -6 FORCE_SWITCH priorities; faster player forces enemy before its queued
	// Bellow. The incoming slot must not execute the outgoing slot's queued action.
	ZM_BattleMonsterSpec axPlayer[2] = {
		ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_BELLOW, ZM_MOVE_NONE, 120u, 250u),
		ZM_SC5Spec(ZM_SPECIES_KINDLET, ZM_MOVE_RAMBASH) };
	ZM_BattleMonsterSpec axEnemy[2] = {
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_BELLOW, ZM_MOVE_NONE, 10u, 250u),
		ZM_SC5Spec(ZM_SPECIES_FERNFAWN, ZM_MOVE_RAMBASH) };
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeTrainerConfig(), axPlayer, 2u, axEnemy, 2u, 1ull, 54ull);
	ZM_SC5ResolveMoveTurn(xEngine);
	ZENITH_ASSERT_EQ(xEngine.GetState().Side(ZM_SIDE_ENEMY).m_uActiveSlot, 1u);
	ZENITH_ASSERT_EQ(ZM_SC5CountForSide(xEngine.GetEvents(), ZM_BATTLE_EVENT_MOVE_USED, ZM_SIDE_ENEMY), 0u,
		"the queued old-mon action is skipped after first mover force-switches eSecond");
}

ZENITH_TEST(ZM_Battle, EOT_FixedSideAndPerSideOrderWithCleanup)
{
	// Exercise the complete engine path first. Enemy is faster and therefore acts
	// first, but ResolveTurn's EOT phase must still tick PLAYER before ENEMY.
	ZM_BattleMonsterSpec axPlayer[1] = {
		ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_WHETCLAW, ZM_MOVE_NONE, 10u, 250u) };
	ZM_BattleMonsterSpec axEnemy[1] = {
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_WHETCLAW, ZM_MOVE_NONE, 120u, 250u) };
	ZM_BattleEngine xEngine;
	xEngine.Begin(MakeTrainerConfig(), axPlayer, 1u, axEnemy, 1u, 1ull, 54ull);
	xEngine.GetStateMutable().Side(ZM_SIDE_PLAYER).Active().m_eStatus = ZM_MAJOR_STATUS_POISON;
	xEngine.GetStateMutable().Side(ZM_SIDE_ENEMY).Active().m_eStatus = ZM_MAJOR_STATUS_POISON;
	const u_int uTurnStart = xEngine.GetEventCount();
	ZM_SC5ResolveMoveTurn(xEngine);
	const Zenith_Vector<ZM_BattleEvent>& xTurnEvents = xEngine.GetEvents();
	const int iEnemyMove = ZM_SC5FindEvent(xTurnEvents, ZM_BATTLE_EVENT_MOVE_USED,
		ZM_SIDE_ENEMY, uTurnStart);
	const int iPlayerMove = ZM_SC5FindEvent(xTurnEvents, ZM_BATTLE_EVENT_MOVE_USED,
		ZM_SIDE_PLAYER, uTurnStart);
	const int iPlayerEOT = ZM_SC5FindEvent(xTurnEvents, ZM_BATTLE_EVENT_STATUS_DAMAGE,
		ZM_SIDE_PLAYER, uTurnStart);
	const int iEnemyEOT = ZM_SC5FindEvent(xTurnEvents, ZM_BATTLE_EVENT_STATUS_DAMAGE,
		ZM_SIDE_ENEMY, uTurnStart);
	const int iTurnEnd = ZM_SC5FindEvent(xTurnEvents, ZM_BATTLE_EVENT_TURN_END,
		ZM_SIDE_COUNT, uTurnStart);
	ZENITH_ASSERT_GE(iEnemyMove, 0);
	ZENITH_ASSERT_GE(iPlayerMove, 0);
	ZENITH_ASSERT_GE(iPlayerEOT, 0);
	ZENITH_ASSERT_GE(iEnemyEOT, 0);
	ZENITH_ASSERT_GE(iTurnEnd, 0);
	ZENITH_ASSERT_LT(iEnemyMove, iPlayerMove, "fixture proves ENEMY was the faster mover");
	ZENITH_ASSERT_LT(iPlayerMove, iPlayerEOT);
	ZENITH_ASSERT_LT(iPlayerEOT, iEnemyEOT,
		"ResolveTurn fixes EOT side order to PLAYER then ENEMY independent of move order");
	ZENITH_ASSERT_LT(iEnemyEOT, iTurnEnd);

	// Exercise the per-side EOT sub-order and cleanup slots directly.
	ZM_BattleState xState;
	BuildBattleState(xState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH, ZM_MOVE_NONE, 10u, 250u),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH, ZM_MOVE_NONE, 120u, 250u), 1ull, 54ull);
	for (u_int uSide = 0u; uSide < ZM_SIDE_COUNT; ++uSide)
	{
		ZM_BattleMonster& xMon = xState.Side((ZM_SIDE)uSide).Active();
		xMon.m_eStatus = ZM_MAJOR_STATUS_POISON;
		ZM_SC5RigVolatile(xMon, ZM_VOLATILE_LEECH_SEED, 0u,
			uSide == ZM_SIDE_PLAYER ? ZM_SIDE_ENEMY : ZM_SIDE_PLAYER);
		ZM_SC5RigVolatile(xMon, ZM_VOLATILE_TRAP, 2u);
		ZM_SC5RigVolatile(xMon, ZM_VOLATILE_FLINCH);
		ZM_SC5RigVolatile(xMon, ZM_VOLATILE_PROTECT);
		ZM_SC5RigVolatile(xMon, ZM_VOLATILE_TAUNT, 1u);
	}
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_StatusLogic::EndOfTurn(xState, xEvents, ZM_SIDE_PLAYER);
	ZM_StatusLogic::EndOfTurn(xState, xEvents, ZM_SIDE_ENEMY);
	const int iPlayerFirst = ZM_SC5FindEvent(xEvents, ZM_BATTLE_EVENT_STATUS_DAMAGE, ZM_SIDE_PLAYER);
	const int iEnemyFirst = ZM_SC5FindEvent(xEvents, ZM_BATTLE_EVENT_STATUS_DAMAGE, ZM_SIDE_ENEMY);
	ZENITH_ASSERT_GE(iPlayerFirst, 0);
	ZENITH_ASSERT_GE(iEnemyFirst, 0);
	ZENITH_ASSERT_LT(iPlayerFirst, iEnemyFirst, "EOT is fixed PLAYER then ENEMY, not speed ordered");

	// For player: major, leech, trap, then cleanup FLINCH->PROTECT->TAUNT.
	int iMajor = -1;
	int iLeech = -1;
	int iTrap = -1;
	int iFlinchEnd = -1;
	int iProtectEnd = -1;
	int iTauntEnd = -1;
	for (u_int u = 0u; u < xEvents.GetSize(); ++u)
	{
		const ZM_BattleEvent& xEvent = xEvents.Get(u);
		if (xEvent.m_uSide != (u_int)ZM_SIDE_PLAYER) { continue; }
		if (xEvent.m_eKind == ZM_BATTLE_EVENT_STATUS_DAMAGE)
		{
			if (xEvent.m_iTag == (int)ZM_MAJOR_STATUS_POISON) { iMajor = (int)u; }
			else if (xEvent.m_iTag == ZM_VolatileDamageTag(ZM_VOLATILE_LEECH_SEED)) { iLeech = (int)u; }
			else if (xEvent.m_iTag == ZM_VolatileDamageTag(ZM_VOLATILE_TRAP)) { iTrap = (int)u; }
		}
		else if (xEvent.m_eKind == ZM_BATTLE_EVENT_VOLATILE_ENDED)
		{
			if (xEvent.m_iAux == (int)ZM_VOLATILE_FLINCH) { iFlinchEnd = (int)u; }
			else if (xEvent.m_iAux == (int)ZM_VOLATILE_PROTECT) { iProtectEnd = (int)u; }
			else if (xEvent.m_iAux == (int)ZM_VOLATILE_TAUNT) { iTauntEnd = (int)u; }
		}
	}
	ZENITH_ASSERT_LT(iMajor, iLeech);
	ZENITH_ASSERT_LT(iLeech, iTrap);
	ZENITH_ASSERT_LT(iTrap, iFlinchEnd);
	ZENITH_ASSERT_LT(iFlinchEnd, iProtectEnd);
	ZENITH_ASSERT_LT(iProtectEnd, iTauntEnd);
}

ZENITH_TEST(ZM_Battle, EOT_KOSkipsLaterChipsButClearsOneTurnState)
{
	ZM_BattleState xState;
	BuildBattleState(xState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), 1ull, 54ull);
	ZM_BattleMonster& xMon = xState.Side(ZM_SIDE_PLAYER).Active();
	xMon.m_uCurHP = 1u;
	xMon.m_eStatus = ZM_MAJOR_STATUS_POISON;
	ZM_SC5RigVolatile(xMon, ZM_VOLATILE_LEECH_SEED, 0u, ZM_SIDE_ENEMY);
	ZM_SC5RigVolatile(xMon, ZM_VOLATILE_TRAP, 2u);
	ZM_SC5RigVolatile(xMon, ZM_VOLATILE_FLINCH);
	ZM_SC5RigVolatile(xMon, ZM_VOLATILE_PROTECT);
	xMon.m_bEndureThisTurn = true;
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_StatusLogic::EndOfTurn(xState, xEvents, ZM_SIDE_PLAYER);
	ZENITH_ASSERT_TRUE(xMon.IsFainted());
	ZENITH_ASSERT_EQ(ZM_CountKind(xEvents, ZM_BATTLE_EVENT_STATUS_DAMAGE), 1u,
		"major KO skips Leech and Trap chips");
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xMon, ZM_VOLATILE_FLINCH));
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xMon, ZM_VOLATILE_PROTECT));
	ZENITH_ASSERT_FALSE(xMon.m_bEndureThisTurn, "KO still clears one-turn silent state");
}

ZENITH_TEST(ZM_Battle, GateOrder_G2G3G4G5G6FirstCancelStopsLaterStateAndDraws)
{
	// G2 freeze cancel before flinch/confusion. Seed 1 is the SC4 stay-frozen seed.
	{
		ZM_BattleState xState;
		BuildBattleState(xState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH),
			ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_WHETCLAW), 1ull, 54ull);
		ZM_BattleMonster& xAtk = xState.Side(ZM_SIDE_PLAYER).Active();
		xAtk.m_eStatus = ZM_MAJOR_STATUS_FREEZE;
		ZM_SC5RigVolatile(xAtk, ZM_VOLATILE_FLINCH);
		ZM_SC5RigVolatile(xAtk, ZM_VOLATILE_CONFUSED, 3u);
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
		ZM_MoveExecutor::Execute(xCtx);
		ZENITH_ASSERT_TRUE(ZM_SC5HasVolatile(xAtk, ZM_VOLATILE_FLINCH));
		ZENITH_ASSERT_EQ(xAtk.m_uConfuseTurns, 3u);
		ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_FLINCH));
		ZM_BattleRNG xMirror(1ull, 54ull);
		xMirror.RandBelow(100u);
		ZM_SC5AssertRNGCursor(xState, xMirror, "G2 cancel stops G4/G5");
	}
	// G3 sleep cancel before flinch/confusion.
	{
		ZM_BattleState xState;
		BuildBattleState(xState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH),
			ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_WHETCLAW), 1ull, 54ull);
		ZM_BattleMonster& xAtk = xState.Side(ZM_SIDE_PLAYER).Active();
		xAtk.m_eStatus = ZM_MAJOR_STATUS_SLEEP;
		xAtk.m_uStatusCounter = 2u;
		ZM_SC5RigVolatile(xAtk, ZM_VOLATILE_FLINCH);
		ZM_SC5RigVolatile(xAtk, ZM_VOLATILE_CONFUSED, 3u);
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
		ZM_MoveExecutor::Execute(xCtx);
		ZENITH_ASSERT_EQ(xAtk.m_uStatusCounter, 1u);
		ZENITH_ASSERT_TRUE(ZM_SC5HasVolatile(xAtk, ZM_VOLATILE_FLINCH));
		ZENITH_ASSERT_EQ(xAtk.m_uConfuseTurns, 3u);
		ZM_BattleRNG xMirror(1ull, 54ull);
		ZM_SC5AssertRNGCursor(xState, xMirror, "G3 cancel draws nothing and stops G4/G5");
	}
	// G4 flinch before G5 confusion and G6 paralysis.
	{
		ZM_BattleState xState;
		BuildBattleState(xState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH),
			ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_WHETCLAW), 1ull, 54ull);
		ZM_BattleMonster& xAtk = xState.Side(ZM_SIDE_PLAYER).Active();
		xAtk.m_eStatus = ZM_MAJOR_STATUS_PARALYSIS;
		ZM_SC5RigVolatile(xAtk, ZM_VOLATILE_FLINCH);
		ZM_SC5RigVolatile(xAtk, ZM_VOLATILE_CONFUSED, 3u);
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
		ZM_MoveExecutor::Execute(xCtx);
		ZENITH_ASSERT_EQ(xAtk.m_uConfuseTurns, 3u);
		ZM_BattleRNG xMirror(1ull, 54ull);
		ZM_SC5AssertRNGCursor(xState, xMirror, "G4 stops G5/G6 with zero draws");
	}
}

ZENITH_TEST(ZM_Battle, GateOrder_ThawWakeContinueToFlinch)
{
	// SC4 seed 2 thaws. The later G4 must then consume/cancel.
	for (u_int uCase = 0u; uCase < 2u; ++uCase)
	{
		ZM_BattleState xState;
		BuildBattleState(xState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH),
			ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_WHETCLAW), 2ull, 54ull);
		ZM_BattleMonster& xAtk = xState.Side(ZM_SIDE_PLAYER).Active();
		if (uCase == 0u) { xAtk.m_eStatus = ZM_MAJOR_STATUS_FREEZE; }
		else { xAtk.m_eStatus = ZM_MAJOR_STATUS_SLEEP; xAtk.m_uStatusCounter = 1u; }
		ZM_SC5RigVolatile(xAtk, ZM_VOLATILE_FLINCH);
		const u_int uPP = xAtk.m_axMoves[0].m_uCurPP;
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
		ZM_MoveExecutor::Execute(xCtx);
		ZENITH_ASSERT_EQ((u_int)xAtk.m_eStatus, (u_int)ZM_MAJOR_STATUS_NONE);
		ZENITH_ASSERT_TRUE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_STATUS_CURED));
		ZENITH_ASSERT_TRUE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_FLINCH));
		ZENITH_ASSERT_FALSE(ZM_HasKind(xEvents, ZM_BATTLE_EVENT_MOVE_USED));
		ZENITH_ASSERT_EQ(xAtk.m_axMoves[0].m_uCurPP, uPP);
	}
}

ZENITH_TEST(ZM_Battle, GateOrder_G5BeforeG6_SelfHitStopsOrPassDrawsParalysis)
{
	// Self-hit: G6 must not draw.
	{
		const u_int64 ulSeed = ZM_SC5FindGateSeed(true);
		ZM_BattleState xState;
		BuildBattleState(xState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH),
			ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_WHETCLAW), ulSeed, 54ull);
		ZM_BattleMonster& xAtk = xState.Side(ZM_SIDE_PLAYER).Active();
		xAtk.m_eStatus = ZM_MAJOR_STATUS_PARALYSIS;
		ZM_SC5RigVolatile(xAtk, ZM_VOLATILE_CONFUSED, 2u);
		ZM_BattleRNG xMirror(ulSeed, 54ull);
		xMirror.RandBelow(100u);
		xMirror.RandRange(85u, 100u);
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
		ZM_MoveExecutor::Execute(xCtx);
		ZM_SC5AssertRNGCursor(xState, xMirror, "G5 self-hit stops before G6");
	}
	// Find a seed where confusion passes, then paralysis cancels: exactly two gate draws.
	{
		u_int64 ulSeed = 0ull;
		for (u_int64 ulTry = 1ull; ulTry <= 4096ull; ++ulTry)
		{
			ZM_BattleRNG xMirror(ulTry, 54ull);
			if (xMirror.RandBelow(100u) >= 33u && xMirror.RandBelow(100u) < 25u)
			{
				ulSeed = ulTry;
				break;
			}
		}
		ZM_BattleState xState;
		BuildBattleState(xState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH),
			ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_WHETCLAW), ulSeed, 54ull);
		ZM_BattleMonster& xAtk = xState.Side(ZM_SIDE_PLAYER).Active();
		xAtk.m_eStatus = ZM_MAJOR_STATUS_PARALYSIS;
		ZM_SC5RigVolatile(xAtk, ZM_VOLATILE_CONFUSED, 2u);
		Zenith_Vector<ZM_BattleEvent> xEvents;
		ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
		ZM_MoveExecutor::Execute(xCtx);
		const ZM_BattleEvent* pxFail = ZM_SC5FindEventPtr(xEvents,
			ZM_BATTLE_EVENT_MOVE_FAILED, ZM_SIDE_PLAYER);
		ZENITH_ASSERT_NOT_NULL(pxFail);
		if (pxFail != nullptr) { ZENITH_ASSERT_EQ(pxFail->m_iAux, (int)ZM_MOVE_FAIL_FULLY_PARALYZED); }
		ZM_BattleRNG xMirror(ulSeed, 54ull);
		xMirror.RandBelow(100u);
		xMirror.RandBelow(100u);
		ZM_SC5AssertRNGCursor(xState, xMirror, "G5 pass then G6 consumes exactly two gate draws");
	}
}

ZENITH_TEST(ZM_Battle, MoveOrder_GCancellationThenM0TauntAndM3BeforeM4)
{
	// G4 cancels before M0 can inspect Taunt.
	ZM_BattleState xGateState;
	BuildBattleState(xGateState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_WHETCLAW),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), 1ull, 54ull);
	ZM_BattleMonster& xGateAtk = xGateState.Side(ZM_SIDE_PLAYER).Active();
	ZM_SC5RigVolatile(xGateAtk, ZM_VOLATILE_FLINCH);
	ZM_SC5RigVolatile(xGateAtk, ZM_VOLATILE_TAUNT, 3u);
	Zenith_Vector<ZM_BattleEvent> xGateEvents;
	ZM_MoveContext xGateCtx = MakeCtx(xGateState, xGateEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xGateCtx);
	ZENITH_ASSERT_TRUE(ZM_HasKind(xGateEvents, ZM_BATTLE_EVENT_FLINCH));
	ZENITH_ASSERT_EQ(xGateAtk.m_uTauntTurns, 3u);
	ZENITH_ASSERT_FALSE(ZM_HasKind(xGateEvents, ZM_BATTLE_EVENT_MOVE_FAILED));

	// M3 target vanished cancels before the attacker's first-turn charge M4.
	ZM_BattleState xMState;
	BuildBattleState(xMState, ZM_SC5Spec(ZM_SPECIES_KINDLET, ZM_MOVE_HEATSHIMMER),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), 1ull, 54ull);
	ZM_SC5RigVolatile(xMState.Side(ZM_SIDE_ENEMY).Active(), ZM_VOLATILE_SEMI_INVULN);
	Zenith_Vector<ZM_BattleEvent> xMEvents;
	ZM_MoveContext xMCtx = MakeCtx(xMState, xMEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xMCtx);
	const ZM_BattleEvent* pxFail = ZM_SC5FindEventPtr(xMEvents,
		ZM_BATTLE_EVENT_MOVE_FAILED, ZM_SIDE_ENEMY);
	ZENITH_ASSERT_NOT_NULL(pxFail);
	if (pxFail != nullptr) { ZENITH_ASSERT_EQ(pxFail->m_iAux, (int)ZM_MOVE_FAIL_SEMI_INVULNERABLE); }
	ZENITH_ASSERT_FALSE(ZM_SC5HasVolatile(xMState.Side(ZM_SIDE_PLAYER).Active(), ZM_VOLATILE_CHARGE));
	ZM_BattleRNG xMirror(1ull, 54ull);
	ZM_SC5AssertRNGCursor(xMState, xMirror, "M3 cancels before M4 and M5");
}

ZENITH_TEST(ZM_Battle, MoveOrder_M4ChargeBeforeM5AccuracyAndM5BeforeM6Immunity)
{
	// Highstoop can miss (95), but first-turn semi/charge returns at M4 without draw.
	ZM_BattleState xChargeState;
	BuildBattleState(xChargeState, ZM_SC5Spec(ZM_SPECIES_SQUALLET, ZM_MOVE_HIGHSTOOP),
		ZM_SC5Spec(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH), 1ull, 54ull);
	Zenith_Vector<ZM_BattleEvent> xChargeEvents;
	ZM_MoveContext xChargeCtx = MakeCtx(xChargeState, xChargeEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xChargeCtx);
	ZM_BattleRNG xChargeMirror(1ull, 54ull);
	ZM_SC5AssertRNGCursor(xChargeState, xChargeMirror, "M4 first turn precedes M5 accuracy");

	// Gnash Down can miss (90) and NORMAL is immune into PHANTOM. A connecting
	// seed consumes accuracy, then M6 emits IMMUNE with no crit/effect draws.
	u_int64 ulHitSeed = 0ull;
	for (u_int64 ulTry = 1ull; ulTry <= 4096ull; ++ulTry)
	{
		ZM_BattleRNG xMirror(ulTry, 54ull);
		if (xMirror.RandBelow(100u) < 90u) { ulHitSeed = ulTry; break; }
	}
	ZM_BattleState xImmuneState;
	BuildBattleState(xImmuneState, ZM_SC5Spec(ZM_SPECIES_NIBBIN, ZM_MOVE_GNASHDOWN),
		ZM_SC5Spec(ZM_SPECIES_WISPET, ZM_MOVE_HEXBOLT), ulHitSeed, 54ull);
	Zenith_Vector<ZM_BattleEvent> xImmuneEvents;
	ZM_MoveContext xImmuneCtx = MakeCtx(xImmuneState, xImmuneEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xImmuneCtx);
	ZENITH_ASSERT_TRUE(ZM_HasKind(xImmuneEvents, ZM_BATTLE_EVENT_IMMUNE));
	ZM_BattleRNG xImmuneMirror(ulHitSeed, 54ull);
	xImmuneMirror.RandBelow(100u);
	ZM_SC5AssertRNGCursor(xImmuneState, xImmuneMirror, "M5 accuracy precedes M6 immunity; M6 stops later draws");
}

ZENITH_TEST(ZM_Battle, VolatileFree_SC5AddsNoEventsDrawsOrState)
{
	ZM_BattleState xState;
	BuildBattleState(xState, MakeScenarioNibbin(), MakeScenarioStrayling(), 0x1234ull, 54ull);
	ZM_BattleRNG xMirror(0x1234ull, 54ull);
	xMirror.Chance(1u, 24u);
	xMirror.RandRange(85u, 100u);
	Zenith_Vector<ZM_BattleEvent> xEvents;
	ZM_MoveContext xCtx = MakeCtx(xState, xEvents, ZM_SIDE_PLAYER, 0u);
	ZM_MoveExecutor::Execute(xCtx);
	ZENITH_ASSERT_EQ(ZM_CountKind(xEvents, ZM_BATTLE_EVENT_VOLATILE_APPLIED), 0u);
	ZENITH_ASSERT_EQ(ZM_CountKind(xEvents, ZM_BATTLE_EVENT_VOLATILE_ENDED), 0u);
	ZENITH_ASSERT_EQ(ZM_CountKind(xEvents, ZM_BATTLE_EVENT_FLINCH), 0u);
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_PLAYER).Active().m_uVolatileMask, (u_int)ZM_VOLATILE_NONE);
	ZENITH_ASSERT_EQ(xState.Side(ZM_SIDE_ENEMY).Active().m_uVolatileMask, (u_int)ZM_VOLATILE_NONE);
	ZM_SC5AssertRNGCursor(xState, xMirror, "volatile-free old path consumes the exact SC1 crit/roll stream");
}

// ============================================================================
// S2 box-2 SC6 -- ZM_CatchCalc (Gen-III/IV four-shake capture + flee odds) and
// the engine pre-move SWITCH / ITEM(catch) / RUN actions (DecisionLog ZM-D-033
// SC6 + ZM-D-035). Expected catch 'a'/'b' values and per-seed shake/flee outcomes
// are derived by the offline oracle scratchpad/zm_catch_ref.py (an independent
// PCG32 + integer-math reimplementation), not echoed from the engine. Guaranteed
// captures / guaranteed flees draw nothing, so their outcomes are seed-independent.
// ============================================================================
namespace
{
	ZM_BattleConfig SC6_WildConfig()
	{
		ZM_BattleConfig xCfg;
		xCfg.m_bIsWild = true; xCfg.m_bCanCatch = true; xCfg.m_bCanFlee = true; xCfg.m_uLevelCap = 0u;
		return xCfg;
	}
	ZM_BattleAction SC6_Switch(u_int uSlot) { ZM_BattleAction x; x.m_eKind = ZM_ACTION_SWITCH; x.m_uSwitchSlot = uSlot; return x; }
	ZM_BattleAction SC6_Item(ZM_ITEM_ID eItem) { ZM_BattleAction x; x.m_eKind = ZM_ACTION_ITEM; x.m_eItem = eItem; return x; }
	ZM_BattleAction SC6_Run() { ZM_BattleAction x; x.m_eKind = ZM_ACTION_RUN; return x; }

	u_int SC6_Count(const Zenith_Vector<ZM_BattleEvent>& xEv, ZM_BATTLE_EVENT eKind)
	{
		u_int uN = 0u;
		for (u_int i = 0; i < xEv.GetSize(); ++i) { if (xEv.Get(i).m_eKind == eKind) { ++uN; } }
		return uN;
	}
	u_int SC6_FirstIndex(const Zenith_Vector<ZM_BattleEvent>& xEv, ZM_BATTLE_EVENT eKind)
	{
		for (u_int i = 0; i < xEv.GetSize(); ++i) { if (xEv.Get(i).m_eKind == eKind) { return i; } }
		return xEv.GetSize();
	}
	const ZM_BattleEvent* SC6_Find(const Zenith_Vector<ZM_BattleEvent>& xEv, ZM_BATTLE_EVENT eKind)
	{
		const u_int uIdx = SC6_FirstIndex(xEv, eKind);
		return (uIdx < xEv.GetSize()) ? &xEv.Get(uIdx) : nullptr;
	}
}

// --- ZM_CatchCalc pure math (independent of RNG / the engine) ----------------
ZENITH_TEST(ZM_Battle, CatchCalc_BaseCatchRate_Rarities)
{
	ZENITH_ASSERT_EQ(ZM_CatchCalc::BaseCatchRate(ZM_RARITY_COMMON),    190u, "COMMON base rate");
	ZENITH_ASSERT_EQ(ZM_CatchCalc::BaseCatchRate(ZM_RARITY_UNCOMMON),  120u, "UNCOMMON base rate");
	ZENITH_ASSERT_EQ(ZM_CatchCalc::BaseCatchRate(ZM_RARITY_RARE),       45u, "RARE base rate");
	ZENITH_ASSERT_EQ(ZM_CatchCalc::BaseCatchRate(ZM_RARITY_LEGENDARY),   3u, "LEGENDARY base rate");
}

ZENITH_TEST(ZM_Battle, CatchCalc_StatusModifier_Multipliers)
{
	u_int uNum = 0u, uDen = 0u;
	ZM_CatchCalc::StatusModifier(ZM_MAJOR_STATUS_NONE, uNum, uDen);
	ZENITH_ASSERT_TRUE(uNum == 1u && uDen == 1u, "NONE -> x1");
	ZM_CatchCalc::StatusModifier(ZM_MAJOR_STATUS_SLEEP, uNum, uDen);
	ZENITH_ASSERT_TRUE(uNum == 5u && uDen == 2u, "SLEEP -> x5/2");
	ZM_CatchCalc::StatusModifier(ZM_MAJOR_STATUS_FREEZE, uNum, uDen);
	ZENITH_ASSERT_TRUE(uNum == 5u && uDen == 2u, "FREEZE -> x5/2");
	ZM_CatchCalc::StatusModifier(ZM_MAJOR_STATUS_PARALYSIS, uNum, uDen);
	ZENITH_ASSERT_TRUE(uNum == 3u && uDen == 2u, "PARALYSIS -> x3/2");
	ZM_CatchCalc::StatusModifier(ZM_MAJOR_STATUS_POISON, uNum, uDen);
	ZENITH_ASSERT_TRUE(uNum == 3u && uDen == 2u, "POISON -> x3/2");
	ZM_CatchCalc::StatusModifier(ZM_MAJOR_STATUS_TOXIC, uNum, uDen);
	ZENITH_ASSERT_TRUE(uNum == 3u && uDen == 2u, "TOXIC -> x3/2");
	ZM_CatchCalc::StatusModifier(ZM_MAJOR_STATUS_BURN, uNum, uDen);
	ZENITH_ASSERT_TRUE(uNum == 3u && uDen == 2u, "BURN -> x3/2");
}

ZENITH_TEST(ZM_Battle, CatchCalc_BallCatchParam_And_Guaranteed)
{
	ZENITH_ASSERT_EQ(ZM_CatchCalc::BallCatchParam(ZM_ITEM_CATCHORB), 10u, "Catch Orb x10");
	ZENITH_ASSERT_EQ(ZM_CatchCalc::BallCatchParam(ZM_ITEM_GREATORB), 15u, "Great Orb x15");
	ZENITH_ASSERT_EQ(ZM_CatchCalc::BallCatchParam(ZM_ITEM_ULTRAORB), 20u, "Ultra Orb x20");
	ZENITH_ASSERT_EQ(ZM_CatchCalc::BallCatchParam(ZM_ITEM_PRIMEORB), 255u, "Prime Orb sentinel 255");
	ZENITH_ASSERT_EQ(ZM_CatchCalc::BallCatchParam(ZM_ITEM_SALVE), 0u, "non-ball -> 0");
	ZENITH_ASSERT_TRUE(ZM_CatchCalc::IsGuaranteedBall(ZM_ITEM_PRIMEORB), "Prime Orb is guaranteed");
	ZENITH_ASSERT_TRUE(!ZM_CatchCalc::IsGuaranteedBall(ZM_ITEM_CATCHORB), "Catch Orb is not guaranteed");
	ZENITH_ASSERT_TRUE(!ZM_CatchCalc::IsGuaranteedBall(ZM_ITEM_ULTRAORB), "Ultra Orb is not guaranteed");
}

ZENITH_TEST(ZM_Battle, CatchCalc_ModifiedCatchValue_Vectors)
{
	// Oracle-derived a (zm_catch_ref.py). maxHP 100 throughout.
	ZENITH_ASSERT_EQ(ZM_CatchCalc::ModifiedCatchValue(100u, 100u, 190u, 10u, 1u, 1u),  63u, "COMMON full CATCHORB -> 63");
	ZENITH_ASSERT_EQ(ZM_CatchCalc::ModifiedCatchValue(100u,   1u, 190u, 10u, 1u, 1u), 188u, "COMMON 1HP CATCHORB -> 188");
	ZENITH_ASSERT_EQ(ZM_CatchCalc::ModifiedCatchValue(100u,   1u, 190u, 20u, 1u, 1u), 377u, "COMMON 1HP ULTRAORB -> 377");
	ZENITH_ASSERT_EQ(ZM_CatchCalc::ModifiedCatchValue(100u,   1u, 190u, 10u, 5u, 2u), 470u, "COMMON 1HP CATCHORB asleep -> 470");
	ZENITH_ASSERT_EQ(ZM_CatchCalc::ModifiedCatchValue(100u, 100u,   3u, 10u, 1u, 1u),   1u, "LEGENDARY full CATCHORB -> 1");
	ZENITH_ASSERT_EQ(ZM_CatchCalc::ModifiedCatchValue(100u,  50u, 120u, 15u, 1u, 1u), 120u, "UNCOMMON half GREATORB -> 120");
	ZENITH_ASSERT_EQ(ZM_CatchCalc::ModifiedCatchValue(0u,     0u, 190u, 10u, 1u, 1u),   1u, "maxHP 0 guarded to 1");
}

ZENITH_TEST(ZM_Battle, CatchCalc_ShakeProbability_Vectors)
{
	ZENITH_ASSERT_EQ(ZM_CatchCalc::ShakeProbability(63u),  47661u, "b(a=63)");
	ZENITH_ASSERT_EQ(ZM_CatchCalc::ShakeProbability(188u), 61680u, "b(a=188)");
	ZENITH_ASSERT_EQ(ZM_CatchCalc::ShakeProbability(1u),   16643u, "b(a=1)");
	ZENITH_ASSERT_EQ(ZM_CatchCalc::ShakeProbability(120u), 55187u, "b(a=120)");
	ZENITH_ASSERT_EQ(ZM_CatchCalc::ShakeProbability(255u), 65536u, "a>=255 -> sentinel");
	ZENITH_ASSERT_EQ(ZM_CatchCalc::ShakeProbability(500u), 65536u, "a>>255 -> sentinel");
}

// --- ZM_CatchCalc::Roll (rigged seeds; oracle-derived outcomes) --------------
ZENITH_TEST(ZM_Battle, CatchCalc_Roll_NonGuaranteedCaught)
{
	// COMMON species, base HP 25 @ L50 -> maxHP 100; curHP 1 -> a=188, b=61680.
	// Seed(2,54): four shakes all pass (oracle) -> caught.
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(
		MakeSpecOverride(ZM_SPECIES_NIBBIN, 50u, ZM_MOVE_RAMBASH, 25u, 40u, 40u, 40u, 40u, 40u));
	xMon.m_uCurHP = 1u;
	ZM_BattleRNG xRng(2ull, 54ull);
	const ZM_CatchResult xRes = ZM_CatchCalc::Roll(xMon, ZM_ITEM_CATCHORB, xRng);
	ZENITH_ASSERT_EQ(xRes.m_uCatchValueA, 188u, "a == 188 (maxHP 100, curHP 1, COMMON, CATCHORB)");
	ZENITH_ASSERT_TRUE(xRes.m_bCaught, "seed 2 -> caught");
	ZENITH_ASSERT_EQ(xRes.m_uShakes, 4u, "caught == 4 shakes");
}

ZENITH_TEST(ZM_Battle, CatchCalc_Roll_NonGuaranteedEscaped)
{
	// Same a=188/b=61680 target; Seed(5,54)'s first draw (62793) >= b -> 0 shakes.
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(
		MakeSpecOverride(ZM_SPECIES_NIBBIN, 50u, ZM_MOVE_RAMBASH, 25u, 40u, 40u, 40u, 40u, 40u));
	xMon.m_uCurHP = 1u;
	ZM_BattleRNG xRng(5ull, 54ull);
	const ZM_CatchResult xRes = ZM_CatchCalc::Roll(xMon, ZM_ITEM_CATCHORB, xRng);
	ZENITH_ASSERT_EQ(xRes.m_uCatchValueA, 188u, "a == 188");
	ZENITH_ASSERT_TRUE(!xRes.m_bCaught, "seed 5 -> escaped");
	ZENITH_ASSERT_EQ(xRes.m_uShakes, 0u, "escaped on the first shake");
}

ZENITH_TEST(ZM_Battle, CatchCalc_Roll_GuaranteedByValue)
{
	// ULTRAORB on a 1-HP COMMON -> a=377 >= 255 -> guaranteed, no draws.
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(
		MakeSpecOverride(ZM_SPECIES_NIBBIN, 50u, ZM_MOVE_RAMBASH, 25u, 40u, 40u, 40u, 40u, 40u));
	xMon.m_uCurHP = 1u;
	ZM_BattleRNG xRng(999ull, 54ull);   // seed irrelevant: no draws
	const ZM_CatchResult xRes = ZM_CatchCalc::Roll(xMon, ZM_ITEM_ULTRAORB, xRng);
	ZENITH_ASSERT_EQ(xRes.m_uCatchValueA, 377u, "a == 377");
	ZENITH_ASSERT_TRUE(xRes.m_bCaught, "a>=255 -> caught");
	ZENITH_ASSERT_EQ(xRes.m_uShakes, 4u, "guaranteed -> 4 shakes shown");
}

ZENITH_TEST(ZM_Battle, CatchCalc_Roll_GuaranteedByBall)
{
	// PRIMEORB (param 255) -> guaranteed regardless of HP / rarity, no draws.
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(MakeSpec(ZM_SPECIES_ZENARIS, 50u, ZM_MOVE_RAMBASH));
	ZM_BattleRNG xRng(1ull, 54ull);
	const ZM_CatchResult xRes = ZM_CatchCalc::Roll(xMon, ZM_ITEM_PRIMEORB, xRng);
	ZENITH_ASSERT_EQ(xRes.m_uCatchValueA, 255u, "guaranteed ball reports a==255");
	ZENITH_ASSERT_TRUE(xRes.m_bCaught, "PRIMEORB -> caught");
	ZENITH_ASSERT_EQ(xRes.m_uShakes, 4u, "4 shakes");
}

ZENITH_TEST(ZM_Battle, CatchCalc_Roll_LegendaryFullHpEscapes)
{
	// Full-HP LEGENDARY with a basic ball -> a=1, b=16643. Seed(1,54) first draw
	// (56745) >= b -> escape on shake 1 (oracle).
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(MakeSpec(ZM_SPECIES_ZENARIS, 50u, ZM_MOVE_RAMBASH));
	ZM_BattleRNG xRng(1ull, 54ull);
	const ZM_CatchResult xRes = ZM_CatchCalc::Roll(xMon, ZM_ITEM_CATCHORB, xRng);
	ZENITH_ASSERT_EQ(xRes.m_uCatchValueA, 1u, "full-HP legendary -> a==1");
	ZENITH_ASSERT_TRUE(!xRes.m_bCaught, "seed 1 -> escaped");
	ZENITH_ASSERT_EQ(xRes.m_uShakes, 0u, "escaped on the first shake");
}

// --- flee odds (pure) --------------------------------------------------------
ZENITH_TEST(ZM_Battle, FleeOdds_Vectors)
{
	ZENITH_ASSERT_TRUE(ZM_ComputeFleeOdds(100u, 10u, 1u).m_bGuaranteed, "faster -> guaranteed");
	ZENITH_ASSERT_TRUE(ZM_ComputeFleeOdds(50u, 50u, 1u).m_bGuaranteed, "equal speed -> guaranteed");
	const ZM_FleeOdds x1 = ZM_ComputeFleeOdds(10u, 100u, 1u);
	ZENITH_ASSERT_TRUE(!x1.m_bGuaranteed, "slower -> not guaranteed");
	ZENITH_ASSERT_EQ(x1.m_uThreshold, 42u, "f(10,100,1) == 42");
	ZENITH_ASSERT_EQ(ZM_ComputeFleeOdds(10u, 100u, 2u).m_uThreshold, 72u, "f ramps +30 per attempt");
	ZENITH_ASSERT_EQ(ZM_ComputeFleeOdds(30u, 60u, 1u).m_uThreshold, 94u, "f(30,60,1) == 94");
}

ZENITH_TEST(ZM_Battle, RollFlee_GuaranteedAndComputed)
{
	ZM_BattleRNG xRngA(123ull, 54ull);
	ZENITH_ASSERT_TRUE(ZM_RollFlee(100u, 10u, 1u, xRngA), "faster always flees (no draw)");
	// Seed(15,54) first %256 = 246; f(slow,fast,1) <= 157 -> 246 >= f -> fails.
	ZM_BattleRNG xRngB(15ull, 54ull);
	ZENITH_ASSERT_TRUE(!ZM_RollFlee(10u, 100u, 1u, xRngB), "seed 15 -> flee fails");
	// Seed(4,54) first %256 = 127; f(10,100,4)=132 -> 127 < 132 -> flees.
	ZM_BattleRNG xRngC(4ull, 54ull);
	ZENITH_ASSERT_TRUE(ZM_RollFlee(10u, 100u, 4u, xRngC), "seed 4, attempt 4 -> flee succeeds");
}

// --- engine pre-move ITEM (catch) --------------------------------------------
ZENITH_TEST(ZM_Battle, Engine_Catch_GuaranteedBall_EndsBattle)
{
	ZM_BattleMonsterSpec axP[1] = { MakeSpec(ZM_SPECIES_NIBBIN, 20u, ZM_MOVE_RAMBASH) };
	ZM_BattleMonsterSpec axE[1] = { MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH) };
	ZM_BattleEngine xEng;
	xEng.Begin(SC6_WildConfig(), axP, 1u, axE, 1u, 0x1234ull, 54ull);
	xEng.SubmitAction(ZM_SIDE_PLAYER, SC6_Item(ZM_ITEM_PRIMEORB));
	xEng.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xEng.ResolveTurn();

	const Zenith_Vector<ZM_BattleEvent>& xEv = xEng.GetEvents();
	ZENITH_ASSERT_TRUE(xEng.IsOver(), "guaranteed catch ends the battle");
	ZENITH_ASSERT_EQ((u_int)xEng.GetWinnerSide(), (u_int)ZM_SIDE_PLAYER, "catcher wins");
	ZENITH_ASSERT_EQ(SC6_Count(xEv, ZM_BATTLE_EVENT_CATCH_SHAKE), 4u, "guaranteed catch shows 4 shakes");
	ZENITH_ASSERT_EQ(SC6_Count(xEv, ZM_BATTLE_EVENT_MOVE_USED), 0u, "catch ends before any move");
	const ZM_BattleEvent* pRes = SC6_Find(xEv, ZM_BATTLE_EVENT_CATCH_RESULT);
	ZENITH_ASSERT_TRUE(pRes != nullptr, "CATCH_RESULT emitted");
	ZENITH_ASSERT_EQ(pRes->m_uSide, (u_int)ZM_SIDE_ENEMY, "target side is the enemy");
	ZENITH_ASSERT_EQ(pRes->m_iAmount, 1, "caught");
	ZENITH_ASSERT_EQ(pRes->m_iAux, 4, "shake count 4");
	ZENITH_ASSERT_EQ(pRes->m_iTag, (int)ZM_ITEM_PRIMEORB, "ball id carried in tag");
	const ZM_BattleEvent& xLast = xEv.Get(xEv.GetSize() - 1u);
	ZENITH_ASSERT_EQ((u_int)xLast.m_eKind, (u_int)ZM_BATTLE_EVENT_BATTLE_END, "ends with BATTLE_END");
	ZENITH_ASSERT_EQ(xLast.m_iAmount, (int)ZM_SIDE_PLAYER, "winner == player");
	ZENITH_ASSERT_TRUE(ZM_ValidateEventStream(xEv, xEng, "catch_guaranteed"), "well-formed");
}

ZENITH_TEST(ZM_Battle, Engine_Catch_NonGuaranteed_FourShakes)
{
	// Enemy base HP 25 @ L50 -> maxHP 100; curHP rigged to 1 -> a=188. Seed 2 -> caught.
	ZM_BattleMonsterSpec axP[1] = { MakeSpec(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH) };
	ZM_BattleMonsterSpec axE[1] = { MakeSpecOverride(ZM_SPECIES_NIBBIN, 50u, ZM_MOVE_RAMBASH, 25u, 40u, 40u, 40u, 40u, 40u) };
	ZM_BattleEngine xEng;
	xEng.Begin(SC6_WildConfig(), axP, 1u, axE, 1u, 2ull, 54ull);
	xEng.GetStateMutable().Side(ZM_SIDE_ENEMY).Active().m_uCurHP = 1u;
	xEng.SubmitAction(ZM_SIDE_PLAYER, SC6_Item(ZM_ITEM_CATCHORB));
	xEng.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xEng.ResolveTurn();

	const Zenith_Vector<ZM_BattleEvent>& xEv = xEng.GetEvents();
	ZENITH_ASSERT_TRUE(xEng.IsOver(), "non-guaranteed 4-shake catch ends the battle");
	ZENITH_ASSERT_EQ((u_int)xEng.GetWinnerSide(), (u_int)ZM_SIDE_PLAYER, "catcher wins");
	ZENITH_ASSERT_EQ(SC6_Count(xEv, ZM_BATTLE_EVENT_CATCH_SHAKE), 4u, "four wobbles");
	const ZM_BattleEvent* pRes = SC6_Find(xEv, ZM_BATTLE_EVENT_CATCH_RESULT);
	ZENITH_ASSERT_TRUE(pRes != nullptr && pRes->m_iAmount == 1 && pRes->m_iAux == 4, "caught in 4 shakes");
	ZENITH_ASSERT_TRUE(ZM_ValidateEventStream(xEv, xEng, "catch_4shake"), "well-formed");
}

ZENITH_TEST(ZM_Battle, Engine_Catch_Escape_TurnContinues)
{
	// Full-HP LEGENDARY enemy + CATCHORB -> a=1. Seed 1 -> escape 0 shakes; the wild
	// enemy (low level, weak) then attacks the bulky player, which survives.
	ZM_BattleMonsterSpec axP[1] = { MakeSpecOverride(ZM_SPECIES_NIBBIN, 30u, ZM_MOVE_RAMBASH, 250u, 40u, 250u, 40u, 250u, 100u) };
	ZM_BattleMonsterSpec axE[1] = { MakeSpec(ZM_SPECIES_ZENARIS, 15u, ZM_MOVE_RAMBASH) };
	ZM_BattleEngine xEng;
	xEng.Begin(SC6_WildConfig(), axP, 1u, axE, 1u, 1ull, 54ull);
	xEng.SubmitAction(ZM_SIDE_PLAYER, SC6_Item(ZM_ITEM_CATCHORB));
	xEng.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xEng.ResolveTurn();

	const Zenith_Vector<ZM_BattleEvent>& xEv = xEng.GetEvents();
	ZENITH_ASSERT_EQ(SC6_Count(xEv, ZM_BATTLE_EVENT_CATCH_SHAKE), 0u, "0 shakes on escape");
	const ZM_BattleEvent* pRes = SC6_Find(xEv, ZM_BATTLE_EVENT_CATCH_RESULT);
	ZENITH_ASSERT_TRUE(pRes != nullptr && pRes->m_iAmount == 0 && pRes->m_iAux == 0, "escaped, 0 shakes");
	// The turn continued: the wild enemy took its move AFTER the failed catch.
	const u_int uResIdx = SC6_FirstIndex(xEv, ZM_BATTLE_EVENT_CATCH_RESULT);
	const u_int uMoveIdx = SC6_FirstIndex(xEv, ZM_BATTLE_EVENT_MOVE_USED);
	ZENITH_ASSERT_TRUE(uMoveIdx < xEv.GetSize() && uMoveIdx > uResIdx, "enemy moved after the failed catch");
	ZENITH_ASSERT_TRUE(!xEng.IsOver(), "battle continues after a failed catch");
	ZENITH_ASSERT_TRUE(ZM_ValidateEventStream(xEv, xEng, "catch_escape"), "well-formed");
}

// --- engine pre-move RUN -----------------------------------------------------
ZENITH_TEST(ZM_Battle, Engine_Run_Guaranteed_EndsBattle)
{
	// Fast player vs slow enemy -> guaranteed flee (no draw).
	ZM_BattleMonsterSpec axP[1] = { MakeSpecOverride(ZM_SPECIES_NIBBIN, 30u, ZM_MOVE_RAMBASH, 60u, 40u, 40u, 40u, 40u, 200u) };
	ZM_BattleMonsterSpec axE[1] = { MakeSpecOverride(ZM_SPECIES_STRAYLING, 30u, ZM_MOVE_RAMBASH, 60u, 40u, 40u, 40u, 40u, 10u) };
	ZM_BattleEngine xEng;
	xEng.Begin(SC6_WildConfig(), axP, 1u, axE, 1u, 0x777ull, 54ull);
	xEng.SubmitAction(ZM_SIDE_PLAYER, SC6_Run());
	xEng.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xEng.ResolveTurn();

	const Zenith_Vector<ZM_BattleEvent>& xEv = xEng.GetEvents();
	ZENITH_ASSERT_TRUE(xEng.IsOver(), "guaranteed flee ends the battle");
	ZENITH_ASSERT_EQ((u_int)xEng.GetWinnerSide(), (u_int)ZM_SIDE_COUNT, "flee has no winner");
	ZENITH_ASSERT_EQ(SC6_Count(xEv, ZM_BATTLE_EVENT_FLEE), 1u, "one FLEE event");
	ZENITH_ASSERT_EQ(SC6_Count(xEv, ZM_BATTLE_EVENT_MOVE_USED), 0u, "flee ends before the enemy moves");
	const ZM_BattleEvent* pFlee = SC6_Find(xEv, ZM_BATTLE_EVENT_FLEE);
	ZENITH_ASSERT_TRUE(pFlee != nullptr && pFlee->m_uSide == (u_int)ZM_SIDE_PLAYER, "player fled");
	const ZM_BattleEvent& xLast = xEv.Get(xEv.GetSize() - 1u);
	ZENITH_ASSERT_EQ((u_int)xLast.m_eKind, (u_int)ZM_BATTLE_EVENT_BATTLE_END, "ends with BATTLE_END");
	ZENITH_ASSERT_EQ(xLast.m_iAmount, (int)ZM_SIDE_COUNT, "winner == none");
	ZENITH_ASSERT_TRUE(ZM_ValidateEventStream(xEv, xEng, "flee_guaranteed"), "well-formed");
}

ZENITH_TEST(ZM_Battle, Engine_Run_Failed_TurnContinues)
{
	// Slow, bulky player vs fast weak enemy. Seed 15's first %256 (246) exceeds any
	// attempt-1 threshold (<=157) -> FLEE_FAILED; the enemy then attacks + player lives.
	ZM_BattleMonsterSpec axP[1] = { MakeSpecOverride(ZM_SPECIES_NIBBIN, 30u, ZM_MOVE_RAMBASH, 250u, 40u, 250u, 40u, 250u, 10u) };
	ZM_BattleMonsterSpec axE[1] = { MakeSpecOverride(ZM_SPECIES_STRAYLING, 15u, ZM_MOVE_RAMBASH, 40u, 40u, 40u, 40u, 40u, 200u) };
	ZM_BattleEngine xEng;
	xEng.Begin(SC6_WildConfig(), axP, 1u, axE, 1u, 15ull, 54ull);
	xEng.SubmitAction(ZM_SIDE_PLAYER, SC6_Run());
	xEng.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xEng.ResolveTurn();

	const Zenith_Vector<ZM_BattleEvent>& xEv = xEng.GetEvents();
	ZENITH_ASSERT_EQ(SC6_Count(xEv, ZM_BATTLE_EVENT_FLEE), 0u, "no successful FLEE");
	ZENITH_ASSERT_EQ(SC6_Count(xEv, ZM_BATTLE_EVENT_FLEE_FAILED), 1u, "one FLEE_FAILED");
	const u_int uFailIdx = SC6_FirstIndex(xEv, ZM_BATTLE_EVENT_FLEE_FAILED);
	const u_int uMoveIdx = SC6_FirstIndex(xEv, ZM_BATTLE_EVENT_MOVE_USED);
	ZENITH_ASSERT_TRUE(uMoveIdx < xEv.GetSize() && uMoveIdx > uFailIdx, "enemy moved after the failed run");
	ZENITH_ASSERT_TRUE(!xEng.IsOver(), "battle continues after a failed run");
	ZENITH_ASSERT_TRUE(ZM_ValidateEventStream(xEv, xEng, "flee_failed"), "well-formed");
}

// --- engine pre-move SWITCH --------------------------------------------------
ZENITH_TEST(ZM_Battle, Engine_Switch_Voluntary_BringsInBenchMon)
{
	ZM_BattleMonsterSpec axP[2] = {
		MakeSpecOverride(ZM_SPECIES_NIBBIN,    30u, ZM_MOVE_RAMBASH, 60u, 40u, 40u, 40u, 40u, 100u),
		MakeSpecOverride(ZM_SPECIES_STRAYLING, 30u, ZM_MOVE_RAMBASH, 250u, 40u, 250u, 40u, 250u, 100u),
	};
	ZM_BattleMonsterSpec axE[1] = { MakeSpec(ZM_SPECIES_WISPET, 15u, ZM_MOVE_RAMBASH) };
	ZM_BattleEngine xEng;
	xEng.Begin(MakeTrainerConfig(), axP, 2u, axE, 1u, 0x2468ull, 54ull);
	xEng.SubmitAction(ZM_SIDE_PLAYER, SC6_Switch(1u));
	xEng.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xEng.ResolveTurn();

	const Zenith_Vector<ZM_BattleEvent>& xEv = xEng.GetEvents();
	ZENITH_ASSERT_EQ(xEng.GetState().Side(ZM_SIDE_PLAYER).m_uActiveSlot, 1u, "active is now slot 1");
	// 2 initial SWITCH_INs at Begin + 1 for the voluntary switch.
	ZENITH_ASSERT_EQ(SC6_Count(xEv, ZM_BATTLE_EVENT_SWITCH_IN), 3u, "one voluntary SWITCH_IN");
	ZENITH_ASSERT_EQ(SC6_Count(xEv, ZM_BATTLE_EVENT_MOVE_FAILED), 0u, "a legal switch does not fail");
	// The switched-in mon is the enemy's move target -> enemy MOVE_USED after the switch.
	ZENITH_ASSERT_TRUE(SC6_Count(xEv, ZM_BATTLE_EVENT_MOVE_USED) >= 1u, "enemy took its move");
	ZENITH_ASSERT_TRUE(ZM_ValidateEventStream(xEv, xEng, "switch_ok"), "well-formed");
}

ZENITH_TEST(ZM_Battle, Engine_Switch_Trapped_Fails)
{
	ZM_BattleMonsterSpec axP[2] = {
		MakeSpecOverride(ZM_SPECIES_NIBBIN,    30u, ZM_MOVE_RAMBASH, 250u, 40u, 250u, 40u, 250u, 100u),
		MakeSpec(ZM_SPECIES_STRAYLING, 30u, ZM_MOVE_RAMBASH),
	};
	ZM_BattleMonsterSpec axE[1] = { MakeSpec(ZM_SPECIES_WISPET, 15u, ZM_MOVE_RAMBASH) };
	ZM_BattleEngine xEng;
	xEng.Begin(MakeTrainerConfig(), axP, 2u, axE, 1u, 0x2468ull, 54ull);
	// A valid TRAP state carries both the bit and a live counter (as ApplyVolatile sets).
	xEng.GetStateMutable().Side(ZM_SIDE_PLAYER).Active().m_uVolatileMask |= (u_int)ZM_VOLATILE_TRAP;
	xEng.GetStateMutable().Side(ZM_SIDE_PLAYER).Active().m_uTrapTurns = 3u;
	xEng.SubmitAction(ZM_SIDE_PLAYER, SC6_Switch(1u));
	xEng.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xEng.ResolveTurn();

	const Zenith_Vector<ZM_BattleEvent>& xEv = xEng.GetEvents();
	ZENITH_ASSERT_EQ(xEng.GetState().Side(ZM_SIDE_PLAYER).m_uActiveSlot, 0u, "trapped -> no switch");
	ZENITH_ASSERT_EQ(SC6_Count(xEv, ZM_BATTLE_EVENT_SWITCH_IN), 2u, "no extra SWITCH_IN");
	const ZM_BattleEvent* pFail = SC6_Find(xEv, ZM_BATTLE_EVENT_MOVE_FAILED);
	ZENITH_ASSERT_TRUE(pFail != nullptr && pFail->m_iAux == (int)ZM_MOVE_FAIL_TRAPPED, "MOVE_FAILED(TRAPPED)");
	ZENITH_ASSERT_TRUE(ZM_ValidateEventStream(xEv, xEng, "switch_trapped"), "well-formed");
}

ZENITH_TEST(ZM_Battle, Engine_Switch_InvalidTarget_Fails)
{
	ZM_BattleMonsterSpec axP[2] = {
		MakeSpecOverride(ZM_SPECIES_NIBBIN,    30u, ZM_MOVE_RAMBASH, 250u, 40u, 250u, 40u, 250u, 100u),
		MakeSpec(ZM_SPECIES_STRAYLING, 30u, ZM_MOVE_RAMBASH),
	};
	ZM_BattleMonsterSpec axE[1] = { MakeSpec(ZM_SPECIES_WISPET, 15u, ZM_MOVE_RAMBASH) };
	ZM_BattleEngine xEng;
	xEng.Begin(MakeTrainerConfig(), axP, 2u, axE, 1u, 0x2468ull, 54ull);
	xEng.GetStateMutable().Side(ZM_SIDE_PLAYER).m_xParty.Get(1u).m_uCurHP = 0u;   // bench slot fainted
	xEng.SubmitAction(ZM_SIDE_PLAYER, SC6_Switch(1u));
	xEng.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xEng.ResolveTurn();

	const Zenith_Vector<ZM_BattleEvent>& xEv = xEng.GetEvents();
	ZENITH_ASSERT_EQ(xEng.GetState().Side(ZM_SIDE_PLAYER).m_uActiveSlot, 0u, "fainted target -> no switch");
	ZENITH_ASSERT_EQ(SC6_Count(xEv, ZM_BATTLE_EVENT_SWITCH_IN), 2u, "no extra SWITCH_IN");
	const ZM_BattleEvent* pFail = SC6_Find(xEv, ZM_BATTLE_EVENT_MOVE_FAILED);
	ZENITH_ASSERT_TRUE(pFail != nullptr && pFail->m_iAux == (int)ZM_MOVE_FAIL_NO_SWITCH_TARGET, "MOVE_FAILED(NO_SWITCH_TARGET)");
	ZENITH_ASSERT_TRUE(ZM_ValidateEventStream(xEv, xEng, "switch_invalid"), "well-formed");
}

// --- byte-identity: a both-MOVE turn is unaffected by config / the pre-move phase
ZENITH_TEST(ZM_Battle, Engine_MoveOnly_WildConfigMatchesTrainer)
{
	ZM_BattleMonsterSpec axP[1] = { MakeSpecOverride(ZM_SPECIES_NIBBIN,    20u, ZM_MOVE_RAMBASH, 200u, 60u, 60u, 40u, 60u, 100u) };
	ZM_BattleMonsterSpec axE[1] = { MakeSpecOverride(ZM_SPECIES_STRAYLING, 20u, ZM_MOVE_RAMBASH, 200u, 60u, 60u, 40u, 60u, 100u) };

	ZM_BattleEngine xTrainer;
	xTrainer.Begin(MakeTrainerConfig(), axP, 1u, axE, 1u, 0xABCDull, 54ull);
	xTrainer.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xTrainer.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xTrainer.ResolveTurn();

	ZM_BattleEngine xWild;
	xWild.Begin(SC6_WildConfig(), axP, 1u, axE, 1u, 0xABCDull, 54ull);
	xWild.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
	xWild.SubmitAction(ZM_SIDE_ENEMY, MoveAction(0u));
	xWild.ResolveTurn();

	const Zenith_Vector<ZM_BattleEvent>& xT = xTrainer.GetEvents();
	const Zenith_Vector<ZM_BattleEvent>& xW = xWild.GetEvents();
	bool bEqual = (xT.GetSize() == xW.GetSize());
	for (u_int i = 0; bEqual && i < xT.GetSize(); ++i) { bEqual = (xT.Get(i) == xW.Get(i)); }
	if (!bEqual) { ZM_LogTwoStreams(xT, xW); }
	ZENITH_ASSERT_TRUE(bEqual, "a both-MOVE turn is byte-identical regardless of wild/trainer config");
	ZENITH_ASSERT_EQ(SC6_Count(xW, ZM_BATTLE_EVENT_CATCH_SHAKE), 0u, "no catch events in a move-only turn");
	ZENITH_ASSERT_EQ(SC6_Count(xW, ZM_BATTLE_EVENT_FLEE), 0u, "no flee events in a move-only turn");
}
