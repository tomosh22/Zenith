#pragma once

#include "Zenithmon/Source/Battle/ZM_BattleTypes.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"   // ZM_SPECIES_ID, ZM_STAT, ZM_STAT_COUNT, ZM_BaseStats
#include "Zenithmon/Source/Data/ZM_MoveData.h"       // ZM_MOVE_ID, ZM_MOVE_NONE
#include "Zenithmon/Source/Data/ZM_NatureData.h"     // ZM_NATURE
#include "Zenithmon/Source/Data/ZM_AbilityData.h"    // ZM_ABILITY_ID

// ============================================================================
// ZM_BattleMonster -- the mutable in-battle monster instance (S2 box 1). Named
// ZM_BattleMonster (not ZM_Monster) to reserve the bare name for a future
// persistent overworld/party monster (box 4/6). ZM_BattleMonsterSpec is the
// serializable authoring seed; ZM_BuildBattleMonster derives an instance from
// it via the S1 stat formulas. Everything overridable so a golden fixes every
// input; the base-stat override survives the ZM-D-021 base-stat re-tune.
// ============================================================================

struct ZM_MoveSlot
{
	ZM_MOVE_ID m_eMove = ZM_MOVE_NONE;
	u_int      m_uCurPP = 0u;
	u_int      m_uMaxPP = 0u;
};

// The serializable authoring seed. Tests, box-4 party->battle, box-6 breeding all
// produce one of these. Everything overridable so a golden fixes every input.
struct ZM_BattleMonsterSpec
{
	ZM_SPECIES_ID m_eSpecies = ZM_SPECIES_NONE;
	u_int         m_uLevel   = 50u;
	u_int         m_auIV[ZM_STAT_COUNT] = { 31u,31u,31u,31u,31u,31u };
	u_int         m_auEV[ZM_STAT_COUNT] = { 0u,0u,0u,0u,0u,0u };
	ZM_NATURE     m_eNature  = ZM_NATURE_FERAL;      // neutral default
	ZM_ABILITY_ID m_eAbility = ZM_ABILITY_NONE;      // box 3 fills real defaults
	ZM_MOVE_ID    m_aeMoves[uZM_MAX_MOVES] = { ZM_MOVE_NONE, ZM_MOVE_NONE, ZM_MOVE_NONE, ZM_MOVE_NONE };
	// --- TEST/GOLDEN HOOK ---
	bool          m_bOverrideBaseStats = false;      // if true, use m_xBaseStatsOverride not the
	ZM_BaseStats  m_xBaseStatsOverride  = {};        // ZM-D-021-derived table -> golden survives re-tune
};

struct ZM_BattleMonster
{
	// build inputs (persist; box 4 mutates level/EV/exp)
	ZM_SPECIES_ID   m_eSpecies = ZM_SPECIES_NONE;
	u_int           m_uLevel   = 1u;
	u_int           m_auIV[ZM_STAT_COUNT] = {};
	u_int           m_auEV[ZM_STAT_COUNT] = {};
	ZM_NATURE       m_eNature  = ZM_NATURE_FERAL;
	ZM_ABILITY_ID   m_eAbility = ZM_ABILITY_NONE;    // inert in box 1; box-3 hook subject
	// derived once at construction
	u_int           m_auMaxStat[ZM_STAT_COUNT] = {}; // index ZM_STAT_HP == max HP
	// mutable battle state
	u_int           m_uCurHP   = 0u;
	ZM_MAJOR_STATUS m_eStatus  = ZM_MAJOR_STATUS_NONE;   // box 2
	u_int           m_uStatusCounter = 0u;               // sleep turns / toxic ramp (box 2)
	u_int           m_uVolatileMask  = ZM_VOLATILE_NONE;  // box 2
	ZM_MoveSlot     m_axMoves[uZM_MAX_MOVES];
	int             m_aiStage[ZM_BATTLE_STAT_COUNT] = {}; // each in [-6,+6]; box 1 all 0

	bool IsFainted() const { return m_uCurHP == 0u; }     // derived, never stored (no desync)
};

// Construct from species+level; overridable via the spec. Asserts level in [1,100].
ZM_BattleMonster ZM_BuildBattleMonster(const ZM_BattleMonsterSpec& xSpec);
