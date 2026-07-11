#include "Zenith.h"

#include "Zenithmon/Source/Battle/ZM_BattleMonster.h"
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
		u_int uEV = xSpec.m_auEV[u];
		if (uEV > uZM_MAX_EV_PER_STAT) uEV = uZM_MAX_EV_PER_STAT;

		xMon.m_auIV[u] = uIV;
		xMon.m_auEV[u] = uEV;
		xMon.m_auMaxStat[u] = ZM_CalcStat((ZM_STAT)u, xBase.m_au[u], uIV, uEV, xSpec.m_uLevel, xSpec.m_eNature);
	}

	xMon.m_uCurHP = xMon.m_auMaxStat[ZM_STAT_HP];

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
