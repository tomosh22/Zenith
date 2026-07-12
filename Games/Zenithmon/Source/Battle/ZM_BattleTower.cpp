#include "Zenith.h"

#include "Zenithmon/Source/Battle/ZM_BattleTower.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"   // ZM_GetSpeciesData, ZM_GetSpeciesCount, ZM_STAT_COUNT
#include "Zenithmon/Source/Data/ZM_Learnsets.h"     // ZM_GetSpeciesLearnset
#include "Zenithmon/Source/Data/ZM_MoveData.h"      // ZM_MOVE_NONE
#include "Zenithmon/Source/Data/ZM_NatureData.h"    // ZM_NATURE, ZM_NATURE_COUNT
#include "Zenithmon/Source/Data/ZM_AbilityData.h"   // ZM_ABILITY_NONE

// ============================================================================
// ZM_BattleTower implementation (S2 box 6 SC2). Pure, seeded, headless: PRODUCES
// tower setup (clamped L50 specs + AI tier + config) and SETTLES a streak; it
// never runs a battle. RNG enters ONLY ZM_GenerateTowerTeam (a caller-owned
// ZM_BattleRNG&, never the battle RNG), which draws in the EXACT order pinned by
// the spec (section 7): per slot -> species (rejection until distinct) -> nature.
// The L50 clamp mutates only the authoring SPEC (level = 50, exp = UNSPECIFIED),
// so the locked ZM_BuildBattleMonster recompute path is reused verbatim -- a mon
// over 50 clamps down, one under 50 scales up, and the built mon starts full HP.
// ============================================================================

namespace
{
	// Max IV -- strong tower mons (spec section 6). Matches the spec-default band.
	static const u_int uZM_TOWER_IV = 31u;

	// Fill paeMoves[0..uZM_MAX_MOVES) with the up-to-four HIGHEST-level learnset
	// moves learnable at/below L50 (NO RNG). Collect every eligible entry in learn
	// order; if more than four, keep the LAST four (highest-level); remaining slots
	// stay ZM_MOVE_NONE. Every species teaches a L1 move, so >= 1 slot is filled.
	void g_FillTowerMoves(ZM_SPECIES_ID eSpecies, ZM_MOVE_ID (&paeMoves)[uZM_MAX_MOVES])
	{
		for (u_int i = 0u; i < uZM_MAX_MOVES; ++i)
		{
			paeMoves[i] = ZM_MOVE_NONE;
		}

		const ZM_Learnset xLearnset = ZM_GetSpeciesLearnset(eSpecies);
		ZM_MOVE_ID aeEligible[uZM_MAX_LEARNSET_SIZE];
		u_int uEligible = 0u;
		for (u_int k = 0u; k < xLearnset.m_uCount; ++k)
		{
			if (xLearnset.m_axMoves[k].m_uLevel <= uZM_TOWER_LEVEL)
			{
				aeEligible[uEligible++] = xLearnset.m_axMoves[k].m_eMove;
			}
		}

		// Keep the last (highest-level) up-to-four eligible moves, in learn order.
		const u_int uKeep  = uEligible > uZM_MAX_MOVES ? uZM_MAX_MOVES : uEligible;
		const u_int uStart = uEligible - uKeep;
		for (u_int i = 0u; i < uKeep; ++i)
		{
			paeMoves[i] = aeEligible[uStart + i];
		}
	}
}

ZM_BattleMonsterSpec ZM_ClampSpecToTowerLevel(const ZM_BattleMonsterSpec& xSpec)
{
	// Clamp the authoring SPEC, not the built monster: reuse ZM_BuildBattleMonster's
	// locked recompute path (no new stat math). Species / IVs / EVs / nature /
	// ability / moves / base-stat override all survive the copy verbatim; only the
	// level (-> 50) and exp (-> UNSPECIFIED, so the build derives the L50 floor)
	// change. Total: the original m_uLevel is discarded, so any out-of-range input
	// is normalized to 50 and can never trip the build-time [1,100] assert.
	ZM_BattleMonsterSpec xOut = xSpec;
	xOut.m_uLevel  = uZM_TOWER_LEVEL;
	xOut.m_uCurExp = uZM_EXP_UNSPECIFIED;
	return xOut;
}

void ZM_ClampPartyToTowerLevel(const ZM_BattleMonsterSpec* paxIn, u_int uCount,
	ZM_BattleMonsterSpec* paxOut)
{
	Zenith_Assert(paxIn != nullptr, "ZM_ClampPartyToTowerLevel: null input array");
	Zenith_Assert(paxOut != nullptr, "ZM_ClampPartyToTowerLevel: null output array");
	for (u_int i = 0u; i < uCount; ++i)
	{
		paxOut[i] = ZM_ClampSpecToTowerLevel(paxIn[i]);
	}
}

bool ZM_TowerIsBossBattle(u_int uStreak)
{
	// The upcoming battle is 1-indexed n = uStreak + 1; a boss every 7th (GDD 7.1).
	return ((uStreak + 1u) % uZM_TOWER_BOSS_PERIOD) == 0u;
}

ZM_AI_TIER ZM_TowerBaseTierForStreak(u_int uStreak)
{
	// Monotonic band lookup (tested at every boundary). Thresholds are S11-tunable.
	if (uStreak >= uZM_TOWER_STREAK_CHAMPION) { return ZM_AI_TIER_CHAMPION; }
	if (uStreak >= uZM_TOWER_STREAK_SMART)    { return ZM_AI_TIER_SMART; }
	if (uStreak >= uZM_TOWER_STREAK_GREEDY)   { return ZM_AI_TIER_GREEDY; }
	return ZM_AI_TIER_RANDOM;
}

ZM_AI_TIER ZM_TowerAITierForStreak(u_int uStreak)
{
	ZM_AI_TIER eTier = ZM_TowerBaseTierForStreak(uStreak);
	// Boss bump: one tier up, capped at CHAMPION so we never reach COUNT / NONE.
	if (ZM_TowerIsBossBattle(uStreak) && eTier < ZM_AI_TIER_CHAMPION)
	{
		eTier = (ZM_AI_TIER)((u_int)eTier + 1u);
	}
	return eTier;
}

u_int64 ZM_TowerBattleSeed(const ZM_TowerRun& xRun, u_int uStreak)
{
	// Weyl step on the run seed: distinct per streak, reproducible per (run, streak).
	static const u_int64 ulWEYL_ODD = 0x9E3779B97F4A7C15ull;
	return xRun.m_ulSeed + ulWEYL_ODD * (u_int64)(uStreak + 1u);
}

ZM_RARITY ZM_TowerMaxRarityForStreak(u_int uStreak)
{
	// Rarity ceiling rises with the band, never LEGENDARY (Scope: legendaries gate
	// the post-game story). [0,7) COMMON / [7,21) UNCOMMON / [21,inf) RARE.
	if (uStreak >= uZM_TOWER_STREAK_SMART)  { return ZM_RARITY_RARE; }
	if (uStreak >= uZM_TOWER_STREAK_GREEDY) { return ZM_RARITY_UNCOMMON; }
	return ZM_RARITY_COMMON;
}

void ZM_GenerateTowerTeam(u_int uStreak, ZM_BattleRNG& xRng,
	ZM_BattleMonsterSpec* paxOut, u_int uCount)
{
	Zenith_Assert(paxOut != nullptr, "ZM_GenerateTowerTeam: null output array");
	Zenith_Assert(uCount <= uZM_TOWER_TEAM_SIZE,
		"ZM_GenerateTowerTeam: uCount %u exceeds the tower team size %u", uCount, uZM_TOWER_TEAM_SIZE);

	// --- Build the eligible-species list (NO draws): ascending ZM_SPECIES_ID order,
	// include if rarity <= ceiling AND rarity != LEGENDARY. Both this impl and any
	// Test-Author oracle rebuild this identical list. ---
	const ZM_RARITY eCeiling = ZM_TowerMaxRarityForStreak(uStreak);
	ZM_SPECIES_ID aeEligible[ZM_SPECIES_COUNT];
	u_int uEligible = 0u;
	const u_int uSpeciesCount = ZM_GetSpeciesCount();
	for (u_int i = 0u; i < uSpeciesCount; ++i)
	{
		const ZM_SPECIES_ID eId     = (ZM_SPECIES_ID)i;
		const ZM_RARITY     eRarity = ZM_GetSpeciesData(eId).m_eRarity;
		if (eRarity != ZM_RARITY_LEGENDARY && eRarity <= eCeiling)
		{
			aeEligible[uEligible++] = eId;
		}
	}
	Zenith_Assert(uEligible >= uCount,
		"ZM_GenerateTowerTeam: eligible pool (%u) smaller than team size (%u)", uEligible, uCount);

	// --- Per slot, in ascending order: draw a DISTINCT species (rejection over the
	// eligible list), then exactly one nature draw AFTER the species is fixed. IVs
	// (all 31), EVs (0), moves (learnset-derived) and ability (NONE) draw nothing.
	// This is the pinned spec-section-7 draw order. ---
	for (u_int s = 0u; s < uCount; ++s)
	{
		ZM_SPECIES_ID eSpecies = ZM_SPECIES_NONE;
		for (;;)
		{
			const u_int         uIdx  = xRng.RandBelow(uEligible);
			const ZM_SPECIES_ID eCand = aeEligible[uIdx];
			bool bTaken = false;
			for (u_int j = 0u; j < s; ++j)
			{
				if (paxOut[j].m_eSpecies == eCand)
				{
					bTaken = true;
					break;
				}
			}
			if (!bTaken)
			{
				eSpecies = eCand;
				break;
			}
		}
		const ZM_NATURE eNature = (ZM_NATURE)xRng.RandBelow((u_int)ZM_NATURE_COUNT);

		// Assemble the clamped L50 spec (level 50 + exp UNSPECIFIED == the clamp form).
		ZM_BattleMonsterSpec xSpec = {};
		xSpec.m_eSpecies = eSpecies;
		xSpec.m_uLevel   = uZM_TOWER_LEVEL;
		for (u_int i = 0u; i < ZM_STAT_COUNT; ++i)
		{
			xSpec.m_auIV[i] = uZM_TOWER_IV;
			xSpec.m_auEV[i] = 0u;
		}
		xSpec.m_eNature           = eNature;
		xSpec.m_eAbility          = ZM_ABILITY_NONE;
		xSpec.m_bOverrideBaseStats = false;
		xSpec.m_uCurExp           = uZM_EXP_UNSPECIFIED;
		g_FillTowerMoves(eSpecies, xSpec.m_aeMoves);
		paxOut[s] = xSpec;
	}
}

u_int ZM_TowerAdvance(ZM_TowerRun& xRun, bool bPlayerWon)
{
	if (bPlayerWon)
	{
		++xRun.m_uCurrentStreak;
		if (xRun.m_uCurrentStreak > xRun.m_uBestStreak)
		{
			xRun.m_uBestStreak = xRun.m_uCurrentStreak;   // running high-water mark
		}
	}
	else
	{
		xRun.m_uCurrentStreak = 0u;   // loss resets current; best is preserved
	}
	return xRun.m_uCurrentStreak;
}

ZM_BattleConfig ZM_MakeTowerBattleConfig()
{
	ZM_BattleConfig xConfig = {};
	xConfig.m_uLevelCap        = uZM_TOWER_LEVEL;   // belt-and-braces exp cap if awards ever enabled
	xConfig.m_bIsWild          = false;
	xConfig.m_bCanCatch        = false;
	xConfig.m_bCanFlee         = false;
	xConfig.m_bIsTrainerBattle = true;
	xConfig.m_bAwardExp        = false;             // flat-50 facility grants no levels
	return xConfig;
}
