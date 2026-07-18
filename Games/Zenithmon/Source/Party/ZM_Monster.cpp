#include "Zenith.h"

#include "Zenithmon/Source/Party/ZM_Monster.h"
#include "Zenithmon/Source/Data/ZM_StatCalc.h"        // ZM_CalcStat, uZM_MIN_LEVEL/uZM_MAX_LEVEL, uZM_MAX_IV
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"     // ZM_GetSpeciesBaseStats, ZM_GetSpeciesAbilities
#include "Zenithmon/Source/Data/ZM_MoveData.h"         // ZM_GetMoveData (base PP)
#include "Zenithmon/Source/Data/ZM_Learnsets.h"        // ZM_GetSpeciesLearnset
#include "Zenithmon/Source/Battle/ZM_ExpAndLevel.h"    // ZM_GetSpeciesGrowthRate, ZM_ExpForLevel

// ============================================================================
// ZM_Monster -- persistent record helpers. Pure integer math + table reads only;
// no RNG, no ECS, no I/O. Kept byte-consistent with ZM_BuildBattleMonster so the
// record<->battle round-trip is exact for a full-health member.
// ============================================================================

bool ZM_Monster::IsValid() const
{
	return (u_int)m_eSpecies < (u_int)ZM_SPECIES_COUNT
		&& m_uLevel >= uZM_MIN_LEVEL
		&& m_uLevel <= uZM_MAX_LEVEL;
}

u_int ZM_Monster::GetMaxHP() const
{
	Zenith_Assert((u_int)m_eSpecies < (u_int)ZM_SPECIES_COUNT,
		"ZM_Monster::GetMaxHP: invalid species %u", (u_int)m_eSpecies);
	const ZM_BaseStats xBase = ZM_GetSpeciesBaseStats(m_eSpecies);
	// Normalize a LOCAL copy of the EVs (252/stat + 510 total caps) BEFORE the HP calc,
	// exactly as ZM_BuildBattleMonster does -- so the record's max HP equals its in-battle
	// max HP even for an over-cap record (a corrupt S7 save or a hand-written EV path).
	// Without this, an above-cap EV array would make HP jump on entering/leaving battle.
	u_int auNormalizedEV[ZM_STAT_COUNT];
	for (u_int u = 0u; u < (u_int)ZM_STAT_COUNT; ++u)
	{
		auNormalizedEV[u] = m_auEV[u];
	}
	ZM_NormalizeEVs(auNormalizedEV);
	// HP is nature-independent; ZM_CalcStat routes HP through the HP formula.
	return ZM_CalcStat(ZM_STAT_HP, xBase.m_au[ZM_STAT_HP],
		m_auIV[ZM_STAT_HP], auNormalizedEV[ZM_STAT_HP], m_uLevel, m_eNature);
}

void ZM_Monster::HealToFull()
{
	m_uCurrentHp = GetMaxHP();
	for (u_int u = 0u; u < uZM_MAX_MOVES; ++u)
	{
		m_axMoves[u].m_uCurPP = m_axMoves[u].m_uMaxPP;
	}
	m_eStatus = ZM_MAJOR_STATUS_NONE;
}

ZM_Monster ZM_BuildMonsterRecord(ZM_SPECIES_ID eSpecies, u_int uLevel)
{
	Zenith_Assert(uLevel >= uZM_MIN_LEVEL && uLevel <= uZM_MAX_LEVEL,
		"ZM_BuildMonsterRecord: level %u out of range [%u,%u]", uLevel, uZM_MIN_LEVEL, uZM_MAX_LEVEL);
	Zenith_Assert((u_int)eSpecies < (u_int)ZM_SPECIES_COUNT,
		"ZM_BuildMonsterRecord: invalid species %u", (u_int)eSpecies);

	ZM_Monster xRec;
	xRec.m_eSpecies = eSpecies;
	xRec.m_uLevel   = uLevel;
	for (u_int u = 0u; u < ZM_STAT_COUNT; ++u)
	{
		xRec.m_auIV[u] = uZM_MAX_IV;   // "perfect" IV default (mirrors ZM_BuildWildEnemySpec)
		xRec.m_auEV[u] = 0u;
	}
	xRec.m_eNature  = ZM_NATURE_FERAL;                                 // neutral
	xRec.m_eAbility = ZM_GetSpeciesAbilities(eSpecies).m_eRegular;     // slot-0 (regular) ability
	xRec.m_eGender  = ZM_GENDER_MALE;   // deterministic default (SC1 has no RNG); gender is battle-inert
	xRec.m_eStatus  = ZM_MAJOR_STATUS_NONE;
	xRec.m_uFlags   = 0u;

	// Exp at the level's curve floor (level<->exp consistency invariant).
	const ZM_GROWTH_RATE eRate = ZM_GetSpeciesGrowthRate(eSpecies);
	xRec.m_uCurrentExp = ZM_ExpForLevel(eRate, uLevel);

	// Moves: the up-to-four highest-level learnset entries learnable at/below the
	// level, in learn order, keeping the LAST four when there are more.
	const ZM_Learnset xLs = ZM_GetSpeciesLearnset(eSpecies);
	ZM_MOVE_ID aeMoves[uZM_MAX_MOVES] = { ZM_MOVE_NONE, ZM_MOVE_NONE, ZM_MOVE_NONE, ZM_MOVE_NONE };
	u_int uFilled = 0u;
	for (u_int k = 0u; k < xLs.m_uCount; ++k)
	{
		if (xLs.m_axMoves[k].m_uLevel > uLevel) { continue; }
		if (uFilled < uZM_MAX_MOVES)
		{
			aeMoves[uFilled++] = xLs.m_axMoves[k].m_eMove;
		}
		else
		{
			for (u_int j = 1u; j < uZM_MAX_MOVES; ++j) { aeMoves[j - 1u] = aeMoves[j]; }
			aeMoves[uZM_MAX_MOVES - 1u] = xLs.m_axMoves[k].m_eMove;
		}
	}
	for (u_int u = 0u; u < uZM_MAX_MOVES; ++u)
	{
		xRec.m_axMoves[u].m_eMove = aeMoves[u];
		if (aeMoves[u] != ZM_MOVE_NONE)
		{
			const u_int uPP = ZM_GetMoveData(aeMoves[u]).m_uPP;
			xRec.m_axMoves[u].m_uCurPP = uPP;
			xRec.m_axMoves[u].m_uMaxPP = uPP;
		}
		else
		{
			xRec.m_axMoves[u].m_uCurPP = 0u;
			xRec.m_axMoves[u].m_uMaxPP = 0u;
		}
	}

	xRec.m_uCurrentHp = xRec.GetMaxHP();   // full health
	return xRec;
}

ZM_BattleMonsterSpec ZM_MonsterToBattleSpec(const ZM_Monster& xRecord)
{
	ZM_BattleMonsterSpec xSpec;
	xSpec.m_eSpecies = xRecord.m_eSpecies;
	xSpec.m_uLevel   = xRecord.m_uLevel;
	for (u_int u = 0u; u < ZM_STAT_COUNT; ++u)
	{
		xSpec.m_auIV[u] = xRecord.m_auIV[u];
		xSpec.m_auEV[u] = xRecord.m_auEV[u];
	}
	xSpec.m_eNature  = xRecord.m_eNature;
	xSpec.m_eAbility = xRecord.m_eAbility;
	for (u_int u = 0u; u < uZM_MAX_MOVES; ++u)
	{
		xSpec.m_aeMoves[u] = xRecord.m_axMoves[u].m_eMove;
	}
	xSpec.m_uCurExp = xRecord.m_uCurrentExp;
	xSpec.m_eGender = xRecord.m_eGender;
	// m_bOverrideBaseStats stays false: real-species stats via the table.
	return xSpec;
}

void ZM_ApplyBattleMonsterToRecord(const ZM_BattleMonster& xMon, ZM_Monster& xRecordInOut)
{
	xRecordInOut.m_uLevel      = xMon.m_uLevel;
	xRecordInOut.m_uCurrentExp = xMon.m_uCurExp;
	for (u_int u = 0u; u < ZM_STAT_COUNT; ++u)
	{
		xRecordInOut.m_auEV[u] = xMon.m_auEV[u];   // the exp path accrues EVs
	}
	for (u_int u = 0u; u < uZM_MAX_MOVES; ++u)
	{
		xRecordInOut.m_axMoves[u] = xMon.m_axMoves[u];   // learned moves + current/max PP
	}
	xRecordInOut.m_uCurrentHp = xMon.m_uCurHP;
	xRecordInOut.m_eStatus    = xMon.m_eStatus;
	// species / IVs / nature / ability / gender are immutable across a battle
	// (terminal evolution is deferred -- D6), so they are deliberately not copied.
}

ZM_Monster ZM_MonsterFromBattleMonster(const ZM_BattleMonster& xMon)
{
	ZM_Monster xRec;
	xRec.m_eSpecies = xMon.m_eSpecies;
	for (u_int u = 0u; u < ZM_STAT_COUNT; ++u)
	{
		xRec.m_auIV[u] = xMon.m_auIV[u];
	}
	xRec.m_eNature  = xMon.m_eNature;
	xRec.m_eAbility = xMon.m_eAbility;
	xRec.m_eGender  = xMon.m_eGender;
	xRec.m_uFlags   = 0u;
	// level / exp / EVs / moves+PP / curHP / status.
	ZM_ApplyBattleMonsterToRecord(xMon, xRec);
	return xRec;
}
