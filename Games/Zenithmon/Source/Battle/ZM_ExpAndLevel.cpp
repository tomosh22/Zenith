#include "Zenith.h"

#include "Zenithmon/Source/Battle/ZM_ExpAndLevel.h"
#include "Zenithmon/Source/Data/ZM_StatCalc.h"    // ZM_CalcStat + level/EV caps
#include "Zenithmon/Source/Data/ZM_Learnsets.h"   // ZM_GetSpeciesLearnset
#include "Zenithmon/Source/Data/ZM_MoveData.h"    // ZM_GetMoveData (learned-move PP)

// ============================================================================
// ZM_ExpAndLevel -- see the header banner. All functions are pure integer math;
// none draws RNG. The engine calls ZM_ApplyExpGain / ZM_AddEVYield behind the
// ZM_BattleConfig::m_bAwardExp gate; terminal settlement queues the pure ZM_Evolve.
// ============================================================================

namespace
{
	// Preserve the amount of HP already missing across a restat. Malformed/hostile
	// inputs cannot underflow: an originally living monster remains at least 1 HP,
	// and an over-full value is clamped to the new maximum.
	void g_CarryHPDelta(ZM_BattleMonster& xMon, u_int uOldMaxHP)
	{
		if (xMon.IsFainted())
		{
			return;
		}
		const u_int uNewMaxHP = xMon.m_auMaxStat[ZM_STAT_HP];
		const u_int64 ulMissing = uOldMaxHP > xMon.m_uCurHP
			? (u_int64)uOldMaxHP - (u_int64)xMon.m_uCurHP : 0ull;
		if (ulMissing >= (u_int64)uNewMaxHP)
		{
			xMon.m_uCurHP = uNewMaxHP > 0u ? 1u : 0u;
		}
		else
		{
			xMon.m_uCurHP = uNewMaxHP - (u_int)ulMissing;
		}
	}
}

u_int ZM_ExpForLevel(ZM_GROWTH_RATE eRate, u_int uLevel)
{
	Zenith_Assert(uLevel >= uZM_MIN_LEVEL && uLevel <= uZM_MAX_LEVEL,
		"ZM_ExpForLevel: level %u out of range [%u,%u]", uLevel, uZM_MIN_LEVEL, uZM_MAX_LEVEL);
	Zenith_Assert(eRate < ZM_GROWTH_COUNT, "ZM_ExpForLevel: growth rate %u out of range", (u_int)eRate);

	if (uLevel <= 1u)
	{
		return 0u;   // every curve starts at 0 exp for level 1 (special-cased)
	}

	// Signed 64-bit intermediates: 100^3 * 6 fits 32-bit, but MEDIUM_SLOW dips
	// negative near L1-2, so compute signed then clamp >= 0.
	const long long n  = (long long)uLevel;
	const long long n3 = n * n * n;
	long long iExp = 0;
	switch (eRate)
	{
	case ZM_GROWTH_FAST:        iExp = (4ll * n3) / 5ll;                                        break;
	case ZM_GROWTH_MEDIUM_FAST: iExp = n3;                                                      break;
	case ZM_GROWTH_MEDIUM_SLOW: iExp = (6ll * n3) / 5ll - 15ll * n * n + 100ll * n - 140ll;     break;
	case ZM_GROWTH_SLOW:        iExp = (5ll * n3) / 4ll;                                        break;
	default:                    iExp = 0;                                                       break;
	}
	if (iExp < 0)
	{
		iExp = 0;
	}
	return (u_int)iExp;
}

u_int ZM_LevelForExp(ZM_GROWTH_RATE eRate, u_int uExp)
{
	Zenith_Assert(eRate < ZM_GROWTH_COUNT, "ZM_LevelForExp: growth rate %u out of range", (u_int)eRate);

	// Simple deterministic 1..100 scan: the largest level whose threshold uExp meets.
	// The curves are monotonic, so this clamps to L1 for exp 0 and L100 above the total.
	u_int uLevel = uZM_MIN_LEVEL;
	for (u_int uL = uZM_MIN_LEVEL; uL <= uZM_MAX_LEVEL; ++uL)
	{
		if (ZM_ExpForLevel(eRate, uL) <= uExp)
		{
			uLevel = uL;
		}
	}
	return uLevel;
}

ZM_GROWTH_RATE ZM_GetSpeciesGrowthRate(ZM_SPECIES_ID eId)
{
	// Rarer == slower; uses all four curves. Starters (RARE) land on MEDIUM_SLOW.
	switch (ZM_GetSpeciesData(eId).m_eRarity)
	{
	case ZM_RARITY_COMMON:    return ZM_GROWTH_FAST;
	case ZM_RARITY_UNCOMMON:  return ZM_GROWTH_MEDIUM_FAST;
	case ZM_RARITY_RARE:      return ZM_GROWTH_MEDIUM_SLOW;
	case ZM_RARITY_LEGENDARY: return ZM_GROWTH_SLOW;
	default:                  return ZM_GROWTH_MEDIUM_FAST;
	}
}

u_int ZM_GetSpeciesBaseExpYield(ZM_SPECIES_ID eId)
{
	// Derived from BST (monotonic along an evo chain, ZM-D-021); always >= 1.
	const u_int uYield = (ZM_GetSpeciesBaseStatTotal(eId) * 2u) / 5u;
	return uYield > 0u ? uYield : 1u;
}

ZM_BaseStats ZM_GetSpeciesEVYield(ZM_SPECIES_ID eId)
{
	const ZM_SpeciesData& xData = ZM_GetSpeciesData(eId);
	const ZM_BaseStats    xBase = ZM_GetSpeciesBaseStats(eId);

	// The species' single highest base stat; ties resolve to the lowest ZM_STAT index.
	u_int uMaxIdx = 0u;
	for (u_int i = 1u; i < ZM_STAT_COUNT; ++i)
	{
		if (xBase.m_au[i] > xBase.m_au[uMaxIdx])
		{
			uMaxIdx = i;
		}
	}

	// Stronger mons yield more: stage1->1, stage2->2, stage3->3, standalone-final->3.
	const bool  bSingleStageFinal = (xData.m_uEvoStage == 1u && xData.m_eEvolvesTo == ZM_SPECIES_NONE);
	const u_int uAmount           = bSingleStageFinal ? 3u : xData.m_uEvoStage;

	ZM_BaseStats xYield = {};
	xYield.m_au[uMaxIdx] = uAmount;
	return xYield;
}

u_int ZM_GetSpeciesEvolveLevel(ZM_SPECIES_ID eId)
{
	const ZM_SpeciesData& xData = ZM_GetSpeciesData(eId);
	if (xData.m_eEvolvesTo == ZM_SPECIES_NONE)
	{
		return 0u;   // final stage / single-stage: no level evolution
	}
	return (xData.m_uEvoStage == 1u) ? 16u : 36u;   // stage1->2 @16, stage2->3 @36
}

u_int ZM_CalcExpGain(ZM_SPECIES_ID eDefeated, u_int uDefLevel, bool bTrainer)
{
	// Gross classic singles award. Participation/share is applied independently by
	// the engine. Clamp hostile defeated levels before using wide intermediates.
	if (uDefLevel < uZM_MIN_LEVEL) { uDefLevel = uZM_MIN_LEVEL; }
	if (uDefLevel > uZM_MAX_LEVEL) { uDefLevel = uZM_MAX_LEVEL; }
	const u_int64 ulBase = (u_int64)ZM_GetSpeciesBaseExpYield(eDefeated);
	u_int64 ulExp = (ulBase * (u_int64)uDefLevel) / 7ull;
	if (bTrainer)
	{
		ulExp = (ulExp * 3ull) / 2ull;   // trainer x1.5, after the /7 divide
	}
	if (ulExp == 0ull)
	{
		ulExp = 1ull;
	}
	const u_int64 ulUIntMax = (u_int64)(~0u);
	return ulExp > ulUIntMax ? ~0u : (u_int)ulExp;
}

void ZM_NormalizeEVs(u_int (&auEV)[ZM_STAT_COUNT])
{
	// Canonical stat order owns deterministic overflow retention. Wide totals avoid
	// wrap even when every input is UINT_MAX.
	u_int64 ulTotal = 0ull;
	for (u_int i = 0u; i < ZM_STAT_COUNT; ++i)
	{
		u_int64 ulValue = (u_int64)auEV[i];
		if (ulValue > (u_int64)uZM_MAX_EV_PER_STAT)
		{
			ulValue = (u_int64)uZM_MAX_EV_PER_STAT;
		}
		const u_int64 ulRoom = ulTotal < (u_int64)uZM_MAX_EV_TOTAL
			? (u_int64)uZM_MAX_EV_TOTAL - ulTotal : 0ull;
		if (ulValue > ulRoom)
		{
			ulValue = ulRoom;
		}
		auEV[i] = (u_int)ulValue;
		ulTotal += ulValue;
	}
}

void ZM_AddEVYield(u_int (&auEV)[ZM_STAT_COUNT], const ZM_BaseStats& xYield)
{
	// Defensively repair persisted/hostile input before adding. Yield is never
	// shared: every eligible recipient receives its species' full yield.
	ZM_NormalizeEVs(auEV);
	u_int64 ulTotal = 0ull;
	for (u_int i = 0u; i < ZM_STAT_COUNT; ++i)
	{
		ulTotal += (u_int64)auEV[i];
	}
	for (u_int i = 0u; i < ZM_STAT_COUNT; ++i)
	{
		u_int64 ulAdd = (u_int64)xYield.m_au[i];
		if (ulAdd == 0ull)
		{
			continue;
		}
		const u_int64 ulStatRoom = auEV[i] < uZM_MAX_EV_PER_STAT
			? (u_int64)uZM_MAX_EV_PER_STAT - (u_int64)auEV[i] : 0ull;
		if (ulAdd > ulStatRoom)
		{
			ulAdd = ulStatRoom;
		}
		const u_int64 ulTotalRoom = ulTotal < (u_int64)uZM_MAX_EV_TOTAL
			? (u_int64)uZM_MAX_EV_TOTAL - ulTotal : 0ull;
		if (ulAdd > ulTotalRoom)
		{
			ulAdd = ulTotalRoom;
		}
		auEV[i] += (u_int)ulAdd;
		ulTotal += ulAdd;
	}
}

ZM_SPECIES_ID ZM_CheckEvolveEligible(const ZM_BattleMonster& xMon)
{
	const u_int uEvoLevel = ZM_GetSpeciesEvolveLevel(xMon.m_eSpecies);
	if (uEvoLevel != 0u && xMon.m_uLevel >= uEvoLevel)
	{
		return ZM_GetSpeciesData(xMon.m_eSpecies).m_eEvolvesTo;
	}
	return ZM_SPECIES_NONE;
}

bool ZM_Evolve(ZM_BattleMonster& xMon)
{
	// Queue/levelled flags are battle-only settlement state. Calling the pure
	// mutation consumes that transient decision even when a stale queue is invalid.
	xMon.m_bEvolutionQueued = false;
	xMon.m_bLevelledThisBattle = false;
	const ZM_SPECIES_ID eTarget = ZM_CheckEvolveEligible(xMon);
	if (eTarget == ZM_SPECIES_NONE)
	{
		return false;   // not eligible -> no-op
	}

	// Evolution ALWAYS reads the new species' table (a pre-evo override never carries).
	xMon.m_eSpecies   = eTarget;
	xMon.m_xBaseStats = ZM_GetSpeciesBaseStats(eTarget);

	const u_int uOldMaxHP = xMon.m_auMaxStat[ZM_STAT_HP];
	for (u_int i = 0u; i < ZM_STAT_COUNT; ++i)
	{
		xMon.m_auMaxStat[i] = ZM_CalcStat((ZM_STAT)i, xMon.m_xBaseStats.m_au[i],
			xMon.m_auIV[i], xMon.m_auEV[i], xMon.m_uLevel, xMon.m_eNature);
	}
	g_CarryHPDelta(xMon, uOldMaxHP);
	return true;
}

namespace
{
	// True iff xMon already has eMove in any move slot (learn-time dedupe).
	bool g_MonKnowsMove(const ZM_BattleMonster& xMon, ZM_MOVE_ID eMove)
	{
		for (u_int i = 0u; i < uZM_MAX_MOVES; ++i)
		{
			if (xMon.m_axMoves[i].m_eMove == eMove)
			{
				return true;
			}
		}
		return false;
	}

	// Teach every learnset move whose level == the mon's (just-incremented) level.
	// A move already known is skipped silently; a move with no free slot SKIPS
	// (ZM-D-043 4-move-overflow ruling: headless, no auto-replace, no event). Each
	// learned move fills the first empty slot with PP from the move table and emits
	// MOVE_LEARNED(dest slot in m_iAmount).
	void g_LearnLevelUpMoves(ZM_BattleMonster& xMon, Zenith_Vector<ZM_BattleEvent>& xEvents,
		ZM_SIDE eSide, u_int uSlot)
	{
		const ZM_Learnset xLearnset = ZM_GetSpeciesLearnset(xMon.m_eSpecies);
		for (u_int k = 0u; k < xLearnset.m_uCount; ++k)
		{
			if (xLearnset.m_axMoves[k].m_uLevel != xMon.m_uLevel)
			{
				continue;
			}
			const ZM_MOVE_ID eMove = xLearnset.m_axMoves[k].m_eMove;
			if (g_MonKnowsMove(xMon, eMove))
			{
				continue;   // dedupe: already known, no slot change, no event
			}

			u_int uDest = uZM_MAX_MOVES;
			for (u_int i = 0u; i < uZM_MAX_MOVES; ++i)
			{
				if (xMon.m_axMoves[i].m_eMove == ZM_MOVE_NONE)
				{
					uDest = i;
					break;
				}
			}
			if (uDest >= uZM_MAX_MOVES)
			{
				continue;   // 4-move overflow: SKIP (no event, no auto-replace)
			}

			const u_int uPP = ZM_GetMoveData(eMove).m_uPP;
			xMon.m_axMoves[uDest].m_eMove  = eMove;
			xMon.m_axMoves[uDest].m_uCurPP = uPP;
			xMon.m_axMoves[uDest].m_uMaxPP = uPP;
			xEvents.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_LEARNED, eSide, uSlot,
				(u_int)eMove, (u_int)xMon.m_eSpecies, (int)uDest, 0));
		}
	}
}

u_int ZM_ApplyExpGain(ZM_BattleMonster& xMon, u_int uExpGain,
	Zenith_Vector<ZM_BattleEvent>& xEvents, ZM_SIDE eSide, u_int uSlot, u_int uMaxLevel)
{
	if (uExpGain == 0u)
	{
		return 0u;
	}
	if (uMaxLevel == 0u) { uMaxLevel = uZM_MAX_LEVEL; }
	if (uMaxLevel > uZM_MAX_LEVEL) { uMaxLevel = uZM_MAX_LEVEL; }
	const u_int uEffectiveLevel = xMon.m_uLevel < uZM_MIN_LEVEL
		? uZM_MIN_LEVEL : xMon.m_uLevel;
	if (uEffectiveLevel >= uMaxLevel || uEffectiveLevel > uZM_MAX_LEVEL)
	{
		return 0u;   // already at the cap: no exp, no events at all
	}

	const ZM_GROWTH_RATE eRate = ZM_GetSpeciesGrowthRate(xMon.m_eSpecies);
	const u_int uCapExp = ZM_ExpForLevel(eRate, uMaxLevel);
	const u_int uFloorExp = ZM_ExpForLevel(eRate, uEffectiveLevel);
	const u_int uStartExp = xMon.m_uCurExp < uFloorExp ? uFloorExp : xMon.m_uCurExp;
	if (uStartExp >= uCapExp)
	{
		return 0u;
	}
	const u_int64 ulRoom = (u_int64)uCapExp - (u_int64)uStartExp;
	const u_int uCredited = (u_int64)uExpGain > ulRoom ? (u_int)ulRoom : uExpGain;
	if (uCredited == 0u)
	{
		return 0u;
	}
	xMon.m_uCurExp = uStartExp + uCredited;   // proven <= cap; cannot wrap
	xMon.m_uLevel = uEffectiveLevel;

	xEvents.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_EXP_GAINED, eSide, uSlot,
		ZM_MOVE_NONE, (u_int)xMon.m_eSpecies, (int)uCredited, (int)xMon.m_uCurExp));

	u_int uGained = 0u;
	while (xMon.m_uLevel < uMaxLevel
		&& xMon.m_uCurExp >= ZM_ExpForLevel(eRate, xMon.m_uLevel + 1u))
	{
		++xMon.m_uLevel;
		++uGained;
		xMon.m_bLevelledThisBattle = true;

		// Recompute all six max stats from the STORED base stats (override-aware),
		// NOT the species table, then carry the HP delta (fainted mons never heal).
		const u_int uOldMaxHP = xMon.m_auMaxStat[ZM_STAT_HP];
		for (u_int i = 0u; i < ZM_STAT_COUNT; ++i)
		{
			xMon.m_auMaxStat[i] = ZM_CalcStat((ZM_STAT)i, xMon.m_xBaseStats.m_au[i],
				xMon.m_auIV[i], xMon.m_auEV[i], xMon.m_uLevel, xMon.m_eNature);
		}
		const u_int uNewMaxHP = xMon.m_auMaxStat[ZM_STAT_HP];
		g_CarryHPDelta(xMon, uOldMaxHP);

		xEvents.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_LEVEL_UP, eSide, uSlot,
			ZM_MOVE_NONE, (u_int)xMon.m_eSpecies, (int)xMon.m_uLevel, (int)uNewMaxHP));

		g_LearnLevelUpMoves(xMon, xEvents, eSide, uSlot);
	}
	return uGained;
}
