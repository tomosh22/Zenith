#pragma once

#include "Zenithmon/Source/Battle/ZM_BattleMonster.h"   // ZM_MoveSlot, ZM_BattleMonster(+Spec), and
                                                         // transitively ZM_SPECIES_ID / ZM_NATURE /
                                                         // ZM_ABILITY_ID / ZM_MOVE_ID / ZM_MAJOR_STATUS /
                                                         // ZM_GENDER / ZM_STAT_COUNT / uZM_MAX_MOVES

// ============================================================================
// ZM_Monster -- the PERSISTENT overworld/party monster record (S5 item 5 SC1).
// The bare name ZM_Monster was reserved for exactly this by ZM_BattleMonster.h.
//
// This is durable INSTANCE state (what a save persists), NOT the transient
// in-battle instance (that is ZM_BattleMonster). The field list mirrors the
// locked ZM_Monster save record in Docs/SaveFormat.md (Party module 1) so S7 can
// serialize it without re-shaping. SC1 is IN-MEMORY ONLY -- no ZM_DataStream
// Read/Write here (S7 owns disk).
//
// Everything derivable from the data tables (max stats, HP, exp<->level) is
// computed on demand via the S1 pure formulas (ZM_StatCalc / ZM_ExpAndLevel),
// never stored, so the record can never desync from the tables. 100% pure,
// headless, compiled in ALL configs (no ZENITH_TOOLS gate); no ECS, no graphics.
// ============================================================================

// Persistent flag bits (SaveFormat.md: bit 0 = IS_EGG, bit 1 = IS_SHINY). Both
// are reserved at item 5 (no egg/shiny gameplay yet); records write 0.
static const u_int uZM_MONSTER_FLAG_IS_EGG   = 1u << 0;
static const u_int uZM_MONSTER_FLAG_IS_SHINY = 1u << 1;

struct ZM_Monster
{
	ZM_SPECIES_ID   m_eSpecies    = ZM_SPECIES_NONE;
	u_int           m_uLevel      = 1u;
	u_int           m_uCurrentExp = 0u;                 // cumulative exp; consistent with level under the curve
	u_int           m_auIV[ZM_STAT_COUNT] = { 0u, 0u, 0u, 0u, 0u, 0u };   // 0..31 each; HP/Atk/Def/SpA/SpD/Spe
	u_int           m_auEV[ZM_STAT_COUNT] = { 0u, 0u, 0u, 0u, 0u, 0u };   // normalized (per-stat 252, total 510)
	ZM_NATURE       m_eNature     = ZM_NATURE_FERAL;
	ZM_ABILITY_ID   m_eAbility    = ZM_ABILITY_NONE;    // concrete ability (mirrors ZM_BattleMonsterSpec; see report note)
	ZM_MAJOR_STATUS m_eStatus     = ZM_MAJOR_STATUS_NONE;   // major status only; volatiles are battle-scoped, never persisted
	ZM_MoveSlot     m_axMoves[uZM_MAX_MOVES];           // id + current PP + max PP per slot (empty slots = ZM_MOVE_NONE)
	u_int           m_uCurrentHp  = 0u;                 // carries across battles; whiteout heals to full
	ZM_GENDER       m_eGender     = ZM_GENDER_GENDERLESS;
	u_int           m_uFlags      = 0u;                 // uZM_MONSTER_FLAG_* (all reserved at item 5)

	// A record is valid when its species is a real dex id and its level is in the
	// engine-wide [1,100] band.
	bool  IsValid() const;

	// The record's max HP, derived from base stats + IV/EV/level via ZM_StatCalc
	// (HP is nature-independent). Matches ZM_BuildBattleMonster's HP exactly for a
	// record holding clamped IVs + normalized EVs.
	u_int GetMaxHP() const;

	// curHP == 0 means fainted (derived, never stored -- no desync).
	bool  IsFainted() const { return m_uCurrentHp == 0u; }

	// Full heal: restore curHP to max, every move's PP to its max, and clear major
	// status. Used by the whiteout path (SC5) and any care-center heal later.
	void  HealToFull();
};

// Build a fresh, full-health persistent record for a species at a level: IV 31 /
// EV 0 / neutral (FERAL) nature / the species' REGULAR ability / a deterministic
// MALE gender (no RNG in SC1; gender is battle-inert) / exp at the level's curve
// floor / the up-to-four highest-level learnset moves learnable at/below uLevel
// (learn order; keep the LAST four) at full PP / curHP == max HP. Asserts level in
// [1,100]. This is the item-5 starter/test-monster factory.
ZM_Monster ZM_BuildMonsterRecord(ZM_SPECIES_ID eSpecies, u_int uLevel);

// --- pure conversions between the persistent record and the battle layer -----

// Persistent record -> battle authoring seed (the battle INPUT). Carries species,
// level, IVs, EVs, nature, ability, move ids, cumulative exp, gender, and (SC3) the
// record's damaged current HP via ZM_BattleMonsterSpec::m_uCurHP (clamped to
// [1, maxHP] at build; a full-health record passes its max HP through). PP is not
// carried -- moves rebuild at full PP from the ids (per-battle PP is not persisted).
ZM_BattleMonsterSpec ZM_MonsterToBattleSpec(const ZM_Monster& xRecord);

// Build a NEW persistent record from a battle instance (the caught-monster path,
// SC4): copies species/level/exp/IVs/EVs/nature/ability/gender + the mutable
// post-battle state (curHP, moves + PP, status).
ZM_Monster ZM_MonsterFromBattleMonster(const ZM_BattleMonster& xMon);

// Write the mutable post-battle state of a battle instance back into an EXISTING
// record (the lead write-back path, SC3): level, cumulative exp, EVs, moves + PP,
// curHP, and major status. Immutable identity (species, IVs, nature, ability,
// gender) is left untouched -- terminal evolution is deferred (D6).
void ZM_ApplyBattleMonsterToRecord(const ZM_BattleMonster& xMon, ZM_Monster& xRecordInOut);

// Persist ONLY the transient per-battle vitals of a battle instance back into an
// EXISTING record (the flee path, SC5): current HP, each move's current PP, and the
// major status. Deliberately copies NO progression -- level, cumulative exp, EVs,
// moves-learned, and identity are left untouched, because a successful flee awards
// no progression (unlike ZM_ApplyBattleMonsterToRecord, which carries the WIN's
// level/exp/EV gains). Pure.
void ZM_PersistBattleVitalsToRecord(const ZM_BattleMonster& xMon, ZM_Monster& xRecordInOut);
