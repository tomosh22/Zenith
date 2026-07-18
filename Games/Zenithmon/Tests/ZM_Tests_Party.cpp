#include "Zenith.h"

// ============================================================================
// ZM_Tests_Party -- S5 item 5 SC1 unit tests for the PURE persistent player data
// model: ZM_Monster (record) / ZM_Party / ZM_GameState + the record<->battle
// conversions. All hermetic: no ECS, no scene, no graphics, no RNG, no I/O --
// only the pure S1 data formulas the model is built on. Category ZM_Party.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Party/ZM_Monster.h"
#include "Zenithmon/Source/Party/ZM_Party.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"
#include "Zenithmon/Source/Battle/ZM_BattleMonster.h"    // ZM_BuildBattleMonster
#include "Zenithmon/Source/Battle/ZM_ExpAndLevel.h"      // ZM_ExpForLevel, ZM_GetSpeciesGrowthRate
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"        // ZM_GetSpeciesBaseStats/Abilities
#include "Zenithmon/Source/Data/ZM_StatCalc.h"           // ZM_CalcStat, uZM_MAX_IV
#include "Zenithmon/Source/Data/ZM_MoveData.h"           // ZM_GetMoveData
#include "Zenithmon/Source/Data/ZM_Learnsets.h"          // ZM_GetSpeciesLearnset

namespace
{
	// Field-by-field equality of two persistent records (all persisted fields).
	void AssertRecordEq(const ZM_Monster& xExpected, const ZM_Monster& xActual, const char* szLabel)
	{
		ZENITH_ASSERT_EQ((u_int)xActual.m_eSpecies, (u_int)xExpected.m_eSpecies, "%s species", szLabel);
		ZENITH_ASSERT_EQ(xActual.m_uLevel,      xExpected.m_uLevel,      "%s level", szLabel);
		ZENITH_ASSERT_EQ(xActual.m_uCurrentExp, xExpected.m_uCurrentExp, "%s exp", szLabel);
		ZENITH_ASSERT_EQ((u_int)xActual.m_eNature,  (u_int)xExpected.m_eNature,  "%s nature", szLabel);
		ZENITH_ASSERT_EQ((u_int)xActual.m_eAbility, (u_int)xExpected.m_eAbility, "%s ability", szLabel);
		ZENITH_ASSERT_EQ((u_int)xActual.m_eStatus,  (u_int)xExpected.m_eStatus,  "%s status", szLabel);
		ZENITH_ASSERT_EQ(xActual.m_uCurrentHp,  xExpected.m_uCurrentHp,  "%s curHP", szLabel);
		ZENITH_ASSERT_EQ((u_int)xActual.m_eGender,  (u_int)xExpected.m_eGender,  "%s gender", szLabel);
		ZENITH_ASSERT_EQ(xActual.m_uFlags,      xExpected.m_uFlags,      "%s flags", szLabel);
		for (u_int i = 0u; i < ZM_STAT_COUNT; ++i)
		{
			ZENITH_ASSERT_EQ(xActual.m_auIV[i], xExpected.m_auIV[i], "%s IV %u", szLabel, i);
			ZENITH_ASSERT_EQ(xActual.m_auEV[i], xExpected.m_auEV[i], "%s EV %u", szLabel, i);
		}
		for (u_int i = 0u; i < uZM_MAX_MOVES; ++i)
		{
			ZENITH_ASSERT_EQ((u_int)xActual.m_axMoves[i].m_eMove, (u_int)xExpected.m_axMoves[i].m_eMove, "%s move %u", szLabel, i);
			ZENITH_ASSERT_EQ(xActual.m_axMoves[i].m_uCurPP, xExpected.m_axMoves[i].m_uCurPP, "%s curPP %u", szLabel, i);
			ZENITH_ASSERT_EQ(xActual.m_axMoves[i].m_uMaxPP, xExpected.m_axMoves[i].m_uMaxPP, "%s maxPP %u", szLabel, i);
		}
	}

	// Does a record's moveset contain a given move id?
	bool RecordHasMove(const ZM_Monster& xRec, ZM_MOVE_ID eMove)
	{
		for (u_int i = 0u; i < uZM_MAX_MOVES; ++i)
		{
			if (xRec.m_axMoves[i].m_eMove == eMove) { return true; }
		}
		return false;
	}
}

// ############################################################################
// A. ZM_Monster record
// ############################################################################

// A default-constructed record is invalid (species NONE); a built one is valid.
ZENITH_TEST(ZM_Party, Monster_ValidityBySpeciesAndLevel)
{
	ZM_Monster xDefault;
	ZENITH_ASSERT_FALSE(xDefault.IsValid(), "default record (species NONE) should be invalid");

	const ZM_Monster xBuilt = ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, 5u);
	ZENITH_ASSERT_TRUE(xBuilt.IsValid(), "built Fernfawn L5 should be valid");
}

// ZM_BuildMonsterRecord produces a self-consistent full-health starter: correct
// level, exp at the curve floor, regular ability, a level-1 move, full HP.
ZENITH_TEST(ZM_Party, Monster_BuildRecordIsSelfConsistent)
{
	const ZM_Monster xRec = ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, 5u);

	ZENITH_ASSERT_EQ(xRec.m_uLevel, 5u);
	const ZM_GROWTH_RATE eRate = ZM_GetSpeciesGrowthRate(ZM_SPECIES_FERNFAWN);
	ZENITH_ASSERT_EQ(xRec.m_uCurrentExp, ZM_ExpForLevel(eRate, 5u), "exp must sit at the L5 curve floor");
	ZENITH_ASSERT_EQ((u_int)xRec.m_eAbility, (u_int)ZM_GetSpeciesAbilities(ZM_SPECIES_FERNFAWN).m_eRegular);
	ZENITH_ASSERT_EQ((u_int)xRec.m_eNature, (u_int)ZM_NATURE_FERAL);
	ZENITH_ASSERT_EQ(xRec.m_uCurrentHp, xRec.GetMaxHP(), "a fresh record is at full HP");
	ZENITH_ASSERT_FALSE(xRec.IsFainted());

	// The learnset's first entry is always the level-1 move; a L5 build retains it.
	const ZM_Learnset xLs = ZM_GetSpeciesLearnset(ZM_SPECIES_FERNFAWN);
	ZENITH_ASSERT_GT(xLs.m_uCount, 0u, "Fernfawn has a learnset");
	ZENITH_ASSERT_TRUE(RecordHasMove(xRec, xLs.m_axMoves[0].m_eMove), "record should carry the level-1 move");
	// The level-1 move slot is filled at full PP.
	ZENITH_ASSERT_NE((u_int)xRec.m_axMoves[0].m_eMove, (u_int)ZM_MOVE_NONE, "slot 0 must be filled");
	ZENITH_ASSERT_EQ(xRec.m_axMoves[0].m_uCurPP, xRec.m_axMoves[0].m_uMaxPP, "slot 0 at full PP");
}

// GetMaxHP mirrors the S1 HP formula exactly (base HP + IV/EV/level), and equals
// the HP a battle monster built from the same record starts at.
ZENITH_TEST(ZM_Party, Monster_MaxHPMatchesStatCalcAndBattleBuild)
{
	const ZM_Monster xRec = ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, 5u);

	const ZM_BaseStats xBase = ZM_GetSpeciesBaseStats(ZM_SPECIES_FERNFAWN);
	const u_int uExpectedHP = ZM_CalcStat(ZM_STAT_HP, xBase.m_au[ZM_STAT_HP],
		xRec.m_auIV[ZM_STAT_HP], xRec.m_auEV[ZM_STAT_HP], xRec.m_uLevel, xRec.m_eNature);
	ZENITH_ASSERT_EQ(xRec.GetMaxHP(), uExpectedHP, "GetMaxHP must match ZM_CalcStat");

	const ZM_BattleMonster xMon = ZM_BuildBattleMonster(ZM_MonsterToBattleSpec(xRec));
	ZENITH_ASSERT_EQ(xMon.m_auMaxStat[ZM_STAT_HP], xRec.GetMaxHP(), "battle build max HP must match the record");
	ZENITH_ASSERT_EQ(xMon.m_uCurHP, xRec.m_uCurrentHp, "battle build starts at the record's (full) HP");
}

// HealToFull restores curHP to max, every slot's PP to max, and clears status.
ZENITH_TEST(ZM_Party, Monster_HealToFullRestoresHpPpStatus)
{
	ZM_Monster xRec = ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, 5u);
	const u_int uMaxHP = xRec.GetMaxHP();
	const u_int uMaxPP0 = xRec.m_axMoves[0].m_uMaxPP;

	// Damage it: chip HP, spend PP, apply a major status.
	xRec.m_uCurrentHp = 1u;
	ZENITH_ASSERT_GT(uMaxPP0, 0u);
	xRec.m_axMoves[0].m_uCurPP = 0u;
	xRec.m_eStatus = ZM_MAJOR_STATUS_POISON;

	xRec.HealToFull();

	ZENITH_ASSERT_EQ(xRec.m_uCurrentHp, uMaxHP, "curHP restored to max");
	ZENITH_ASSERT_EQ(xRec.m_axMoves[0].m_uCurPP, uMaxPP0, "PP restored to max");
	ZENITH_ASSERT_EQ((u_int)xRec.m_eStatus, (u_int)ZM_MAJOR_STATUS_NONE, "status cleared");
}

// ############################################################################
// B. ZM_Party
// ############################################################################

// Add grows the count up to 6 and rejects the 7th (strict no-op), count stays 6.
ZENITH_TEST(ZM_Party, Party_AddUpToSixRejectsSeventh)
{
	ZM_Party xParty;
	ZENITH_ASSERT_EQ(xParty.Count(), 0u);
	ZENITH_ASSERT_TRUE(xParty.IsEmpty());

	const ZM_Monster xRec = ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, 5u);
	for (u_int i = 0u; i < uZM_MAX_PARTY_SIZE; ++i)
	{
		ZENITH_ASSERT_TRUE(xParty.Add(xRec), "add %u should succeed", i);
		ZENITH_ASSERT_EQ(xParty.Count(), i + 1u);
	}
	ZENITH_ASSERT_TRUE(xParty.IsFull());
	ZENITH_ASSERT_FALSE(xParty.Add(xRec), "the 7th add must be rejected");
	ZENITH_ASSERT_EQ(xParty.Count(), uZM_MAX_PARTY_SIZE, "count unchanged after a rejected add");
}

// LeadIndex/Lead skip fainted members; all-fainted falls back to slot 0.
ZENITH_TEST(ZM_Party, Party_GetLeadSkipsFainted)
{
	ZM_Party xParty;
	xParty.Add(ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, 5u));
	xParty.Add(ZM_BuildMonsterRecord(ZM_SPECIES_KINDLET, 5u));

	// Slot 0 fainted, slot 1 healthy -> lead is slot 1.
	xParty.Get(0u).m_uCurrentHp = 0u;
	ZENITH_ASSERT_EQ(xParty.LeadIndex(), 1u, "lead skips the fainted slot 0");
	ZENITH_ASSERT_EQ((u_int)xParty.Lead().m_eSpecies, (u_int)ZM_SPECIES_KINDLET);

	// Both fainted -> lead falls back to slot 0.
	xParty.Get(1u).m_uCurrentHp = 0u;
	ZENITH_ASSERT_EQ(xParty.LeadIndex(), 0u, "all fainted -> slot 0 fallback");
}

// AllFainted is true only when every member has curHP == 0.
ZENITH_TEST(ZM_Party, Party_AllFaintedOnlyWhenAllZeroHp)
{
	ZM_Party xParty;
	xParty.Add(ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, 5u));
	xParty.Add(ZM_BuildMonsterRecord(ZM_SPECIES_KINDLET, 5u));

	ZENITH_ASSERT_FALSE(xParty.AllFainted(), "both healthy");
	xParty.Get(0u).m_uCurrentHp = 0u;
	ZENITH_ASSERT_FALSE(xParty.AllFainted(), "one still standing");
	xParty.Get(1u).m_uCurrentHp = 0u;
	ZENITH_ASSERT_TRUE(xParty.AllFainted(), "every member fainted");
}

// HealAllFull restores curHP + PP for every member.
ZENITH_TEST(ZM_Party, Party_HealAllFullRestoresEveryMember)
{
	ZM_Party xParty;
	xParty.Add(ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, 5u));
	xParty.Add(ZM_BuildMonsterRecord(ZM_SPECIES_KINDLET, 5u));

	const u_int uMaxHP0 = xParty.Get(0u).GetMaxHP();
	const u_int uMaxHP1 = xParty.Get(1u).GetMaxHP();
	const u_int uMaxPP1 = xParty.Get(1u).m_axMoves[0].m_uMaxPP;

	xParty.Get(0u).m_uCurrentHp = 0u;                 // fainted
	xParty.Get(1u).m_uCurrentHp = 2u;                 // chipped
	xParty.Get(1u).m_axMoves[0].m_uCurPP = 0u;        // spent

	xParty.HealAllFull();

	ZENITH_ASSERT_EQ(xParty.Get(0u).m_uCurrentHp, uMaxHP0, "member 0 fully healed");
	ZENITH_ASSERT_EQ(xParty.Get(1u).m_uCurrentHp, uMaxHP1, "member 1 fully healed");
	ZENITH_ASSERT_EQ(xParty.Get(1u).m_axMoves[0].m_uCurPP, uMaxPP1, "member 1 PP restored");
	ZENITH_ASSERT_FALSE(xParty.AllFainted(), "nobody fainted after a heal");
}

// ############################################################################
// C. ZM_GameState (caught set + starter)
// ############################################################################

// MarkCaught / IsCaught / GetCaughtCount, with idempotent marking.
ZENITH_TEST(ZM_Party, GameState_CaughtSetMarkQueryCountIdempotent)
{
	ZM_GameState xState;
	ZENITH_ASSERT_EQ(xState.GetCaughtCount(), 0u);
	ZENITH_ASSERT_FALSE(xState.IsCaught(ZM_SPECIES_FERNFAWN));

	xState.MarkCaught(ZM_SPECIES_FERNFAWN);
	ZENITH_ASSERT_TRUE(xState.IsCaught(ZM_SPECIES_FERNFAWN));
	ZENITH_ASSERT_EQ(xState.GetCaughtCount(), 1u);

	// Marking the same species again does not double-count.
	xState.MarkCaught(ZM_SPECIES_FERNFAWN);
	ZENITH_ASSERT_EQ(xState.GetCaughtCount(), 1u, "idempotent mark");

	xState.MarkCaught(ZM_SPECIES_KINDLET);
	ZENITH_ASSERT_EQ(xState.GetCaughtCount(), 2u);
	ZENITH_ASSERT_FALSE(xState.IsCaught(ZM_SPECIES_SYLVASTAG), "unmarked species not caught");

	// Out-of-range / NONE ids are ignored, never index out of bounds.
	xState.MarkCaught(ZM_SPECIES_NONE);
	ZENITH_ASSERT_FALSE(xState.IsCaught(ZM_SPECIES_NONE));
	ZENITH_ASSERT_EQ(xState.GetCaughtCount(), 2u, "NONE is ignored");
}

// The starter GameState is exactly one valid Fernfawn L5 with a level-1 move,
// its species marked caught, and no pending whiteout.
ZENITH_TEST(ZM_Party, GameState_MakeStarterIsSingleValidStarter)
{
	const ZM_GameState xState = ZM_MakeStarterGameState();

	ZENITH_ASSERT_EQ(xState.m_xParty.Count(), 1u, "exactly one starter");
	const ZM_Monster& xLead = xState.m_xParty.Get(0u);
	ZENITH_ASSERT_TRUE(xLead.IsValid());
	ZENITH_ASSERT_EQ((u_int)xLead.m_eSpecies, (u_int)ZM_SPECIES_FERNFAWN);
	ZENITH_ASSERT_EQ(xLead.m_uLevel, 5u);
	ZENITH_ASSERT_FALSE(xLead.IsFainted(), "starter at full health");

	const ZM_Learnset xLs = ZM_GetSpeciesLearnset(ZM_SPECIES_FERNFAWN);
	ZENITH_ASSERT_TRUE(RecordHasMove(xLead, xLs.m_axMoves[0].m_eMove), "starter has its level-1 move");

	ZENITH_ASSERT_TRUE(xState.IsCaught(ZM_SPECIES_FERNFAWN), "starter species is marked caught");
	ZENITH_ASSERT_EQ(xState.GetCaughtCount(), 1u);
	ZENITH_ASSERT_FALSE(xState.m_bPendingWhiteout, "no whiteout pending at start");
}

// ############################################################################
// D. Record <-> battle conversions
// ############################################################################

// A full-health record survives record -> spec -> ZM_BuildBattleMonster ->
// ZM_MonsterFromBattleMonster identically (species/level/exp/IVs/EVs/nature/
// ability/gender/moves+PP/curHP all preserved).
ZENITH_TEST(ZM_Party, Convert_RecordSpecBuildRoundTripIsExact)
{
	const ZM_Monster xRec = ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, 5u);

	const ZM_BattleMonsterSpec xSpec = ZM_MonsterToBattleSpec(xRec);
	const ZM_BattleMonster     xMon  = ZM_BuildBattleMonster(xSpec);

	// Spec carries the identity fields.
	ZENITH_ASSERT_EQ((u_int)xMon.m_eSpecies, (u_int)xRec.m_eSpecies, "species preserved");
	ZENITH_ASSERT_EQ(xMon.m_uLevel, xRec.m_uLevel, "level preserved");
	ZENITH_ASSERT_EQ(xMon.m_uCurHP, xRec.m_uCurrentHp, "current HP preserved (full-health record)");
	for (u_int i = 0u; i < uZM_MAX_MOVES; ++i)
	{
		ZENITH_ASSERT_EQ((u_int)xMon.m_axMoves[i].m_eMove, (u_int)xRec.m_axMoves[i].m_eMove, "move %u preserved", i);
	}

	// Rebuild a record from the battle monster: identical to the original.
	const ZM_Monster xBack = ZM_MonsterFromBattleMonster(xMon);
	AssertRecordEq(xRec, xBack, "roundtrip");
}

// ZM_MonsterFromBattleMonster copies a caught battle instance faithfully -- incl.
// its DAMAGED current HP and a spent move (the caught-monster path).
ZENITH_TEST(ZM_Party, Convert_MonsterFromBattleMonsterFaithful)
{
	const ZM_Monster xSeed = ZM_BuildMonsterRecord(ZM_SPECIES_KINDLET, 8u);
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(ZM_MonsterToBattleSpec(xSeed));

	// Simulate a captured, damaged wild monster.
	xMon.m_uCurHP = 4u;
	xMon.m_axMoves[0].m_uCurPP = xMon.m_axMoves[0].m_uMaxPP - 1u;
	xMon.m_eStatus = ZM_MAJOR_STATUS_PARALYSIS;

	const ZM_Monster xRec = ZM_MonsterFromBattleMonster(xMon);

	ZENITH_ASSERT_TRUE(xRec.IsValid());
	ZENITH_ASSERT_EQ((u_int)xRec.m_eSpecies, (u_int)xMon.m_eSpecies);
	ZENITH_ASSERT_EQ(xRec.m_uLevel, xMon.m_uLevel);
	ZENITH_ASSERT_EQ(xRec.m_uCurrentExp, xMon.m_uCurExp);
	ZENITH_ASSERT_EQ(xRec.m_uCurrentHp, 4u, "damaged HP carried into the record");
	ZENITH_ASSERT_EQ((u_int)xRec.m_eStatus, (u_int)ZM_MAJOR_STATUS_PARALYSIS);
	ZENITH_ASSERT_EQ(xRec.m_axMoves[0].m_uCurPP, xMon.m_axMoves[0].m_uCurPP, "spent PP carried");
	for (u_int i = 0u; i < ZM_STAT_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ(xRec.m_auIV[i], xMon.m_auIV[i], "IV %u", i);
		ZENITH_ASSERT_EQ(xRec.m_auEV[i], xMon.m_auEV[i], "EV %u", i);
	}
}

// ZM_ApplyBattleMonsterToRecord writes the mutable post-battle state (HP/exp/
// level/moves+PP/status) back into an EXISTING record, leaving identity untouched.
ZENITH_TEST(ZM_Party, Convert_ApplyBattleMonsterToRecordWritesBack)
{
	ZM_Monster xRec = ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, 5u);
	const ZM_SPECIES_ID eSpeciesBefore = xRec.m_eSpecies;
	const ZM_ABILITY_ID eAbilityBefore = xRec.m_eAbility;
	const u_int         uIV0Before     = xRec.m_auIV[0];

	ZM_BattleMonster xMon = ZM_BuildBattleMonster(ZM_MonsterToBattleSpec(xRec));

	// Post-battle mutation: took damage, gained a level + exp, spent PP.
	const ZM_GROWTH_RATE eRate = ZM_GetSpeciesGrowthRate(ZM_SPECIES_FERNFAWN);
	xMon.m_uCurHP = 3u;
	xMon.m_uLevel = 6u;
	xMon.m_uCurExp = ZM_ExpForLevel(eRate, 6u) + 10u;
	xMon.m_axMoves[0].m_uCurPP = xMon.m_axMoves[0].m_uMaxPP - 2u;
	// Perturb an IDENTITY field on the battle monster to a value DIFFERENT from the
	// record (^1 stays in [0,31]) -- so the identity-preservation assertion actually
	// proves the write-back does NOT copy identity, rather than copying an identical
	// value. Guards the SC3 lead-persist contract against an accidental identity copy.
	xMon.m_auIV[0] = uIV0Before ^ 1u;

	ZM_ApplyBattleMonsterToRecord(xMon, xRec);

	ZENITH_ASSERT_EQ(xRec.m_uCurrentHp, 3u, "HP written back");
	ZENITH_ASSERT_EQ(xRec.m_uLevel, 6u, "level written back");
	ZENITH_ASSERT_EQ(xRec.m_uCurrentExp, ZM_ExpForLevel(eRate, 6u) + 10u, "exp written back");
	ZENITH_ASSERT_EQ(xRec.m_axMoves[0].m_uCurPP, xMon.m_axMoves[0].m_uCurPP, "PP written back");
	// Identity is immutable across a battle -- NOT copied from the (perturbed) battle monster.
	ZENITH_ASSERT_EQ((u_int)xRec.m_eSpecies, (u_int)eSpeciesBefore, "species unchanged");
	ZENITH_ASSERT_EQ((u_int)xRec.m_eAbility, (u_int)eAbilityBefore, "ability unchanged");
	ZENITH_ASSERT_EQ(xRec.m_auIV[0], uIV0Before, "IVs are identity -- not overwritten by the battle monster");
}

// Determinism: the same record produces a byte-consistent battle spec every time.
ZENITH_TEST(ZM_Party, Convert_MonsterToBattleSpecIsDeterministic)
{
	const ZM_Monster xRec = ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, 5u);

	const ZM_BattleMonsterSpec xA = ZM_MonsterToBattleSpec(xRec);
	const ZM_BattleMonsterSpec xB = ZM_MonsterToBattleSpec(xRec);

	ZENITH_ASSERT_EQ((u_int)xA.m_eSpecies, (u_int)xB.m_eSpecies);
	ZENITH_ASSERT_EQ(xA.m_uLevel, xB.m_uLevel);
	ZENITH_ASSERT_EQ(xA.m_uCurExp, xB.m_uCurExp);
	ZENITH_ASSERT_EQ((u_int)xA.m_eNature, (u_int)xB.m_eNature);
	ZENITH_ASSERT_EQ((u_int)xA.m_eAbility, (u_int)xB.m_eAbility);
	ZENITH_ASSERT_EQ((u_int)xA.m_eGender, (u_int)xB.m_eGender);
	for (u_int i = 0u; i < ZM_STAT_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ(xA.m_auIV[i], xB.m_auIV[i], "IV %u", i);
		ZENITH_ASSERT_EQ(xA.m_auEV[i], xB.m_auEV[i], "EV %u", i);
	}
	for (u_int i = 0u; i < uZM_MAX_MOVES; ++i)
	{
		ZENITH_ASSERT_EQ((u_int)xA.m_aeMoves[i], (u_int)xB.m_aeMoves[i], "move %u", i);
	}
}
