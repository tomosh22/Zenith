#include "Zenith.h"

#include "Zenithmon/Source/Battle/ZM_BattleMonster.h"
#include "Zenithmon/Source/Battle/ZM_ExpAndLevel.h"   // ZM_ExpForLevel, ZM_GetSpeciesGrowthRate (box 4 build fields)
#include "Zenithmon/Source/Data/ZM_StatCalc.h"   // ZM_CalcStat, IV/EV/level caps

// Derive an in-battle monster instance from its authoring spec. Pure integer
// math via the S1 stat formulas so a battle replays bit-for-bit from its seed.
ZM_BattleMonster ZM_BuildBattleMonster(const ZM_BattleMonsterSpec& xSpec)
{
	Zenith_Assert(xSpec.m_uLevel >= uZM_MIN_LEVEL && xSpec.m_uLevel <= uZM_MAX_LEVEL,
		"ZM_BuildBattleMonster: level %u out of range [%u,%u]", xSpec.m_uLevel, uZM_MIN_LEVEL, uZM_MAX_LEVEL);

	ZM_BattleMonster xMon;
	xMon.m_eSpecies = xSpec.m_eSpecies;
	xMon.m_uLevel   = xSpec.m_uLevel;
	xMon.m_eNature  = xSpec.m_eNature;
	xMon.m_eAbility = xSpec.m_eAbility;

	// Base stats: the golden hook overrides the ZM-D-021-derived table so a
	// pencil-verifiable golden survives a later base-stat re-tune.
	ZM_BaseStats xBase;
	if (xSpec.m_bOverrideBaseStats)
	{
		xBase = xSpec.m_xBaseStatsOverride;
	}
	else
	{
		xBase = ZM_GetSpeciesBaseStats(xSpec.m_eSpecies);
	}

	for (u_int u = 0; u < ZM_STAT_COUNT; ++u)
	{
		u_int uIV = xSpec.m_auIV[u];
		if (uIV > uZM_MAX_IV) uIV = uZM_MAX_IV;
		xMon.m_auIV[u] = uIV;
		xMon.m_auEV[u] = xSpec.m_auEV[u];
	}
	// Canonical HP/Atk/Def/SpA/SpD/Spe order decides which hostile excess is
	// retained: per-stat 252 first, then the total-510 room is consumed left-to-right.
	ZM_NormalizeEVs(xMon.m_auEV);
	for (u_int u = 0; u < ZM_STAT_COUNT; ++u)
	{
		xMon.m_auMaxStat[u] = ZM_CalcStat((ZM_STAT)u, xBase.m_au[u],
			xMon.m_auIV[u], xMon.m_auEV[u], xSpec.m_uLevel, xSpec.m_eNature);
	}

	xMon.m_uCurHP = xMon.m_auMaxStat[ZM_STAT_HP];

	// Box 4: stash the resolved base stats (the level-up recompute source, override-
	// aware) and normalize an explicitly supplied cumulative-exp value into the
	// declared level's band. Legacy callers omit it and receive the curve floor.
	xMon.m_xBaseStats = xBase;
	const ZM_GROWTH_RATE eRate = ZM_GetSpeciesGrowthRate(xSpec.m_eSpecies);
	const u_int uFloorExp = ZM_ExpForLevel(eRate, xSpec.m_uLevel);
	xMon.m_uCurExp = uFloorExp;
	if (xSpec.m_uCurExp != uZM_EXP_UNSPECIFIED && xSpec.m_uLevel < uZM_MAX_LEVEL)
	{
		const u_int uNextExp = ZM_ExpForLevel(eRate, xSpec.m_uLevel + 1u);
		const u_int uBandMax = uNextExp > uFloorExp ? uNextExp - 1u : uFloorExp;
		xMon.m_uCurExp = xSpec.m_uCurExp < uFloorExp ? uFloorExp : xSpec.m_uCurExp;
		if (xMon.m_uCurExp > uBandMax)
		{
			xMon.m_uCurExp = uBandMax;
		}
	}

	for (u_int u = 0; u < uZM_MAX_MOVES; ++u)
	{
		const ZM_MOVE_ID eMove = xSpec.m_aeMoves[u];
		xMon.m_axMoves[u].m_eMove = eMove;
		if (eMove != ZM_MOVE_NONE)
		{
			const u_int uPP = ZM_GetMoveData(eMove).m_uPP;
			xMon.m_axMoves[u].m_uCurPP = uPP;
			xMon.m_axMoves[u].m_uMaxPP = uPP;
		}
		else
		{
			xMon.m_axMoves[u].m_uCurPP = 0u;
			xMon.m_axMoves[u].m_uMaxPP = 0u;
		}
	}

	// status NONE, counters 0, mask 0, stages 0, crit stage 0 all come from the
	// struct defaults (xMon was default-constructed above).
	return xMon;
}

// True iff the monster's species carries eType in either type slot. Mirrors the
// ZM_StatusLogic file-static type read exactly; a ZM_TYPE_NONE second slot never
// equals a real type, so the equality check ignores empty slots on its own.
bool ZM_BattleMonsterHasType(const ZM_BattleMonster& xMon, ZM_TYPE eType)
{
	const ZM_SpeciesData& xSpecies = ZM_GetSpeciesData(xMon.m_eSpecies);
	return xSpecies.m_aeTypes[0] == eType || xSpecies.m_aeTypes[1] == eType;
}
