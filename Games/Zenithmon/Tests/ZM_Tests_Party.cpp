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
#include "Zenithmon/Source/Party/ZM_BattleWriteBack.h"   // ZM_ApplyBattleResultToParty (win-only lead persist)
#include "Zenithmon/Source/Battle/ZM_BattleMonster.h"    // ZM_BuildBattleMonster, uZM_CURHP_UNSPECIFIED
#include "Zenithmon/Source/Battle/ZM_BattleDirectorCore.h" // ZM_BuildWildEnemySpec
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
		ZENITH_ASSERT_EQ(xActual.m_uFriendship, xExpected.m_uFriendship, "%s friendship", szLabel);
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
		for (u_int i = 0u; i < uZM_MONSTER_NICKNAME_CAPACITY; ++i)
		{
			ZENITH_ASSERT_EQ((u_int)(unsigned char)xActual.m_szNickname[i],
				(u_int)(unsigned char)xExpected.m_szNickname[i], "%s nickname byte %u", szLabel, i);
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

	// --- real-core drive helpers (SC5). Mirror ZM_Tests_BattleDirector's file-local
	// fixtures -- those copies are internal linkage there and not linkable across TUs. ---

	// A wild single-battle config: exp OFF (so a win never perturbs progression) with
	// flee permitted, so a RUN action resolves to a successful flee.
	ZM_BattleConfig MakeWildConfig()
	{
		ZM_BattleConfig xCfg;
		xCfg.m_bIsWild  = true;
		xCfg.m_bCanFlee = true;
		return xCfg;
	}

	ZM_BattleAction MakeMoveSlot0()
	{
		ZM_BattleAction xAction;
		xAction.m_eKind     = ZM_ACTION_MOVE;
		xAction.m_uMoveSlot = 0u;
		return xAction;
	}

	ZM_BattleAction MakeRunAction()
	{
		ZM_BattleAction xAction;
		xAction.m_eKind = ZM_ACTION_RUN;
		return xAction;
	}

	// Drive a real ZM_BattleDirectorCore to resolution under instant-battles, submitting
	// xPlayerAction on every AWAIT_INPUT. Bounded so a mis-specified fixture can never
	// hang; the caller asserts the resulting winner side.
	void DriveDirectorToEnd(ZM_BattleDirectorCore& xCore, const ZM_BattleAction& xPlayerAction)
	{
		u_int uIter = 0u;
		while (!xCore.ShouldRequestEnd() && uIter < 200u)
		{
			if (xCore.IsAwaitingInput())
			{
				xCore.SubmitPlayerAction(xPlayerAction);
			}
			xCore.Tick(0.0f);
			++uIter;
		}
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

// ############################################################################
// E. Battle result write-back to the party (S5 item 5 SC3)
//
// ZM_ApplyBattleResultToParty(party, leadSlot, winner, finalLead) is the win-only
// bridge that persists the post-battle lead back into the party. It writes the
// mutable progression (level / cumulative exp / EVs / moves+PP / curHP / status)
// into the lead slot ONLY when the player won, leaving identity untouched, and
// guards the empty / out-of-range cases.
// ############################################################################

// A PLAYER win persists the post-battle progression (level-up, new exp, trained
// EV, damaged HP, spent PP) into the party's lead slot, leaving identity intact.
ZENITH_TEST(ZM_Party, WriteBack_WinPersistsProgression)
{
	// A party holding a fresh L5 record at slot 0 (the battle lead).
	const ZM_Monster xLead = ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, 5u);
	ZM_Party xParty;
	ZENITH_ASSERT_TRUE(xParty.Add(xLead), "seed the lead at slot 0");

	// Identity fields a win must NEVER rewrite.
	const ZM_SPECIES_ID eSpeciesBefore = xParty.Get(0u).m_eSpecies;
	const ZM_NATURE     eNatureBefore  = xParty.Get(0u).m_eNature;
	const u_int         uIV0Before     = xParty.Get(0u).m_auIV[0];

	// Build the lead's battle monster, then apply a realistic post-win mutation:
	// it took damage, gained a level + exp, trained an EV, and spent PP.
	const ZM_GROWTH_RATE eRate = ZM_GetSpeciesGrowthRate(ZM_SPECIES_FERNFAWN);
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(ZM_MonsterToBattleSpec(xLead));
	xMon.m_uLevel = 6u;
	const u_int uNewExp = ZM_ExpForLevel(eRate, 6u) + 25u;
	xMon.m_uCurExp = uNewExp;
	const u_int uNewEV = xMon.m_auEV[ZM_STAT_HP] + 4u;
	xMon.m_auEV[ZM_STAT_HP] = uNewEV;
	xMon.m_uCurHP = 3u;
	ZENITH_ASSERT_GE(xMon.m_axMoves[0].m_uCurPP, 2u, "slot 0 has PP to spend");
	xMon.m_axMoves[0].m_uCurPP -= 2u;
	const u_int uSpentPP0 = xMon.m_axMoves[0].m_uCurPP;
	// Perturb an IDENTITY field on the battle monster (^1 stays in [0,31]) so the
	// identity-preservation assertion proves the write-back does NOT copy identity
	// -- rather than copying an identical value.
	xMon.m_auIV[0] = uIV0Before ^ 1u;

	// A PLAYER win writes the final lead back into the party's lead slot.
	ZM_ApplyBattleResultToParty(xParty, 0u, ZM_SIDE_PLAYER, xMon);

	const ZM_Monster& xAfter = xParty.Get(0u);
	ZENITH_ASSERT_EQ(xAfter.m_uLevel, 6u, "win persisted the level-up");
	ZENITH_ASSERT_EQ(xAfter.m_uCurrentExp, uNewExp, "win persisted the new exp total");
	ZENITH_ASSERT_EQ(xAfter.m_uCurrentHp, 3u, "win persisted the damaged HP");
	ZENITH_ASSERT_EQ(xAfter.m_axMoves[0].m_uCurPP, uSpentPP0, "win persisted the spent PP");
	ZENITH_ASSERT_EQ(xAfter.m_auEV[ZM_STAT_HP], uNewEV, "win persisted the trained EV");
	// Identity is immutable across a battle -- NOT copied from the (perturbed) battle monster.
	ZENITH_ASSERT_EQ((u_int)xAfter.m_eSpecies, (u_int)eSpeciesBefore, "species unchanged");
	ZENITH_ASSERT_EQ((u_int)xAfter.m_eNature,  (u_int)eNatureBefore,  "nature unchanged");
	ZENITH_ASSERT_EQ(xAfter.m_auIV[0], uIV0Before, "IVs are identity -- not overwritten by the battle monster");
}

// A non-win outcome (enemy win / draw) is a strict no-op: the lead record is left
// byte-for-byte unchanged, even though the battle monster would have changed it.
ZENITH_TEST(ZM_Party, WriteBack_NonWinIsNoOp)
{
	const ZM_Monster xLead = ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, 5u);
	ZM_Party xParty;
	xParty.Add(xLead);
	const ZM_Monster xBefore = xParty.Get(0u);   // exact snapshot before any call

	// A battle monster that WOULD rewrite everything IF a win wrote it back.
	const ZM_GROWTH_RATE eRate = ZM_GetSpeciesGrowthRate(ZM_SPECIES_FERNFAWN);
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(ZM_MonsterToBattleSpec(xLead));
	xMon.m_uLevel  = 6u;
	xMon.m_uCurExp = ZM_ExpForLevel(eRate, 6u) + 25u;
	xMon.m_uCurHP  = 1u;

	// Only a PLAYER win writes back: a loss (ENEMY) and a draw (COUNT) are no-ops.
	ZM_ApplyBattleResultToParty(xParty, 0u, ZM_SIDE_ENEMY, xMon);
	AssertRecordEq(xBefore, xParty.Get(0u), "after enemy-win (loss)");
	ZM_ApplyBattleResultToParty(xParty, 0u, ZM_SIDE_COUNT, xMon);
	AssertRecordEq(xBefore, xParty.Get(0u), "after draw");
}

// The write-back guards degenerate inputs: an empty party and an out-of-range lead
// slot both leave the party untouched and never index out of bounds.
ZENITH_TEST(ZM_Party, WriteBack_GuardsEmptyAndOutOfRange)
{
	const ZM_Monster xLead = ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, 5u);
	ZM_BattleMonster xMon = ZM_BuildBattleMonster(ZM_MonsterToBattleSpec(xLead));
	xMon.m_uLevel = 9u;
	xMon.m_uCurHP = 1u;

	// Empty party: a win write-back must guard the empty case (no crash, no write).
	ZM_Party xEmpty;
	ZM_ApplyBattleResultToParty(xEmpty, 0u, ZM_SIDE_PLAYER, xMon);
	ZENITH_ASSERT_EQ(xEmpty.Count(), 0u, "empty party stays empty");

	// One member + an out-of-range lead slot: guarded, no write to the real member.
	ZM_Party xParty;
	xParty.Add(xLead);
	const ZM_Monster xBefore = xParty.Get(0u);
	ZM_ApplyBattleResultToParty(xParty, 5u, ZM_SIDE_PLAYER, xMon);
	AssertRecordEq(xBefore, xParty.Get(0u), "out-of-range lead slot leaves the member untouched");
}

// SC3 spec-HP contract: ZM_BattleMonsterSpec::m_uCurHP round-trips a record's
// damaged HP through the build, clamps a specified value into [1, maxHP], and
// treats the uZM_CURHP_UNSPECIFIED sentinel as "full HP".
ZENITH_TEST(ZM_Party, SpecCurHP_RoundTripAndClamp)
{
	const ZM_Monster xRec = ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, 5u);
	const u_int uMaxHP = xRec.GetMaxHP();
	ZENITH_ASSERT_GT(uMaxHP, 1u, "max HP must exceed 1 so the clamp bands are distinct");

	// A record carrying damaged HP round-trips its exact curHP: record -> spec -> build.
	ZM_Monster xDamaged = xRec;
	xDamaged.m_uCurrentHp = uMaxHP / 2u;
	const ZM_BattleMonster xMonHalf = ZM_BuildBattleMonster(ZM_MonsterToBattleSpec(xDamaged));
	ZENITH_ASSERT_EQ(xMonHalf.m_uCurHP, uMaxHP / 2u, "damaged curHP carried through the spec");

	// Explicit spec clamps: build the spec, then override m_uCurHP to exercise the
	// [1, maxHP] clamp the builder applies for a specified (non-sentinel) value.
	ZM_BattleMonsterSpec xSpecLow = ZM_MonsterToBattleSpec(xRec);
	xSpecLow.m_uCurHP = 0u;
	ZENITH_ASSERT_EQ(ZM_BuildBattleMonster(xSpecLow).m_uCurHP, 1u, "a specified 0 clamps up to 1");

	ZM_BattleMonsterSpec xSpecHigh = ZM_MonsterToBattleSpec(xRec);
	xSpecHigh.m_uCurHP = 999999u;
	ZENITH_ASSERT_EQ(ZM_BuildBattleMonster(xSpecHigh).m_uCurHP, uMaxHP, "a specified over-max clamps down to maxHP");

	// The UNSPECIFIED sentinel means "full HP": ZM_BuildWildEnemySpec never sets
	// m_uCurHP, so it defaults to the sentinel and builds at full HP.
	const ZM_BattleMonsterSpec xWild = ZM_BuildWildEnemySpec(ZM_SPECIES_FERNFAWN, 5u);
	ZENITH_ASSERT_EQ((u_int)xWild.m_uCurHP, uZM_CURHP_UNSPECIFIED, "the wild spec leaves curHP at the full-HP sentinel");
	const ZM_BattleMonster xMonWild = ZM_BuildBattleMonster(xWild);
	ZENITH_ASSERT_EQ(xMonWild.m_uCurHP, xMonWild.m_auMaxStat[ZM_STAT_HP], "the sentinel builds at full HP");
}

// ############################################################################
// F. Catch write-back to the game state (S5 item 5 SC4)
//
// ZM_ApplyCatchToGameState(gameState, bCaught, caughtMonster) is the catch bridge:
// on a successful catch it ALWAYS marks the caught species in the dex, and Adds the
// caught monster as a new party record WHEN the party is not full (box storage is
// S7). A failed catch is a strict no-op. Pure: no ECS, no scene, no RNG.
// ############################################################################

// A successful catch of a NEW species appends it to the party (as the record built
// from the caught battle monster) and marks it in the dex.
ZENITH_TEST(ZM_Party, Catch_AddsToPartyAndMarksDex)
{
	// A game state carrying a single Fernfawn L5 party lead.
	ZM_GameState xState;
	ZENITH_ASSERT_TRUE(xState.m_xParty.Add(ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, 5u)),
		"seed the lead at slot 0");
	ZENITH_ASSERT_EQ(xState.m_xParty.Count(), 1u, "one party member before the catch");
	ZENITH_ASSERT_FALSE(xState.IsCaught(ZM_SPECIES_KINDLET), "Kindlet is not caught before the catch");

	// The caught wild monster (a damaged/final battle instance in the shipped flow).
	const ZM_BattleMonster xCaught = ZM_BuildBattleMonster(ZM_BuildWildEnemySpec(ZM_SPECIES_KINDLET, 3u));

	ZM_ApplyCatchToGameState(xState, true, xCaught);

	// It joined the party as slot 1, exactly equal to the record the caught monster converts to.
	ZENITH_ASSERT_EQ(xState.m_xParty.Count(), 2u, "the caught monster joined the party");
	AssertRecordEq(ZM_MonsterFromBattleMonster(xCaught), xState.m_xParty.Get(1u),
		"the new party slot equals the record built from the caught battle monster");
	// The dex marks the species.
	ZENITH_ASSERT_TRUE(xState.IsCaught(ZM_SPECIES_KINDLET), "the caught species is marked in the dex");
}

// A failed catch (bCaught == false) is a strict no-op: no party growth, no dex mark.
ZENITH_TEST(ZM_Party, Catch_NotCaughtIsNoOp)
{
	ZM_GameState xState;
	xState.m_xParty.Add(ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, 5u));
	const u_int uCountBefore      = xState.m_xParty.Count();
	const ZM_Monster xLeadBefore = xState.m_xParty.Get(0u);
	const u_int uSeenBefore       = xState.GetSeenCount();
	const u_int uCaughtBefore     = xState.GetCaughtCount();
	const u_int uBoxCountBefore  = xState.m_xBoxes.Count();
	ZENITH_ASSERT_FALSE(xState.IsSeen(ZM_SPECIES_KINDLET), "Kindlet begins unseen");
	ZENITH_ASSERT_FALSE(xState.IsCaught(ZM_SPECIES_KINDLET), "Kindlet begins uncaught");
	ZENITH_ASSERT_EQ(uBoxCountBefore, 0u, "boxes begin empty");

	const ZM_BattleMonster xCaught = ZM_BuildBattleMonster(ZM_BuildWildEnemySpec(ZM_SPECIES_KINDLET, 3u));
	ZM_ApplyCatchToGameState(xState, false, xCaught);

	ZENITH_ASSERT_EQ(xState.m_xParty.Count(), uCountBefore, "a failed catch adds no party member");
	AssertRecordEq(xLeadBefore, xState.m_xParty.Get(0u), "a failed catch leaves the existing party record untouched");
	ZENITH_ASSERT_EQ(xState.GetSeenCount(), uSeenBefore, "a failed catch marks nothing in the seen dex");
	ZENITH_ASSERT_EQ(xState.GetCaughtCount(), uCaughtBefore, "a failed catch marks nothing in the dex");
	ZENITH_ASSERT_FALSE(xState.IsSeen(ZM_SPECIES_KINDLET), "the un-caught species stays unseen");
	ZENITH_ASSERT_FALSE(xState.IsCaught(ZM_SPECIES_KINDLET), "the un-caught species stays unmarked");
	ZENITH_ASSERT_EQ(xState.m_xBoxes.Count(), uBoxCountBefore, "a failed catch stores nothing in boxes");
	for (u_int uBox = 0u; uBox < uZM_BOX_COUNT; ++uBox)
	{
		for (u_int uSlot = 0u; uSlot < uZM_BOX_SLOTS_PER_BOX; ++uSlot)
		{
			ZENITH_ASSERT_TRUE(xState.m_xBoxes.TryGet(uBox, uSlot) == nullptr,
				"a failed catch leaves box %u slot %u empty", uBox, uSlot);
		}
	}
}

// Catch placement is party-first. A catch which fills the last party slot leaves
// boxes untouched; the next catch enters the first free slot of the first box.
// Both catches mark seen + caught, and the party never exceeds its fixed cap.
ZENITH_TEST(ZM_Party, Catch_FullPartyStoresFirstBoxAndMarksDex)
{
	ZM_GameState xState;
	for (u_int i = 0u; i < uZM_MAX_PARTY_SIZE - 1u; ++i)
	{
		ZENITH_ASSERT_TRUE(xState.m_xParty.Add(ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, 5u)),
			"seed party slot %u", i);
	}
	ZENITH_ASSERT_FALSE(xState.m_xParty.IsFull(), "one party slot remains before the first catch");
	ZENITH_ASSERT_EQ(xState.m_xBoxes.Count(), 0u, "boxes begin empty");
	ZENITH_ASSERT_FALSE(xState.IsCaught(ZM_SPECIES_KINDLET), "Kindlet is not caught before the catch");
	ZENITH_ASSERT_FALSE(xState.IsCaught(ZM_SPECIES_NIBBIN), "Nibbin is not caught before the catch");

	const ZM_BattleMonster xPartyCatch =
		ZM_BuildBattleMonster(ZM_BuildWildEnemySpec(ZM_SPECIES_KINDLET, 3u));
	ZM_ApplyCatchToGameState(xState, true, xPartyCatch);
	ZENITH_ASSERT_TRUE(xState.m_xParty.IsFull(), "the first catch fills the final party slot");
	ZENITH_ASSERT_EQ(xState.m_xParty.Count(), uZM_MAX_PARTY_SIZE, "party reaches exactly its cap");
	ZENITH_ASSERT_EQ(xState.m_xBoxes.Count(), 0u, "party-first placement leaves boxes untouched while space exists");
	AssertRecordEq(ZM_MonsterFromBattleMonster(xPartyCatch),
		xState.m_xParty.Get(uZM_MAX_PARTY_SIZE - 1u), "first catch occupies the final party slot");
	ZENITH_ASSERT_TRUE(xState.IsSeen(ZM_SPECIES_KINDLET), "the party catch marks Kindlet seen");
	ZENITH_ASSERT_TRUE(xState.IsCaught(ZM_SPECIES_KINDLET), "the party catch marks Kindlet caught");

	const ZM_BattleMonster xBoxCatch =
		ZM_BuildBattleMonster(ZM_BuildWildEnemySpec(ZM_SPECIES_NIBBIN, 4u));
	ZM_ApplyCatchToGameState(xState, true, xBoxCatch);
	ZENITH_ASSERT_EQ(xState.m_xParty.Count(), uZM_MAX_PARTY_SIZE, "the overflow catch does not grow the party");
	ZENITH_ASSERT_EQ(xState.m_xBoxes.Count(), 1u, "the overflow catch is stored exactly once");
	const ZM_Monster* pxStored = xState.m_xBoxes.TryGet(0u, 0u);
	ZENITH_ASSERT_TRUE(pxStored != nullptr, "the overflow catch occupies first box slot 0");
	if (pxStored != nullptr)
	{
		AssertRecordEq(ZM_MonsterFromBattleMonster(xBoxCatch), *pxStored,
			"the first box slot equals the caught battle monster record");
	}
	ZENITH_ASSERT_TRUE(xState.IsSeen(ZM_SPECIES_NIBBIN), "the boxed catch marks Nibbin seen");
	ZENITH_ASSERT_TRUE(xState.IsCaught(ZM_SPECIES_NIBBIN), "the boxed catch marks Nibbin caught");
}

// ############################################################################
// G. Loss -> whiteout + flee-HP-persist (S5 item 5 SC5)
//
// The loss/flee branches of the (ZM_GameState&, const ZM_BattleDirectorCore&) write-back
// overload, plus the vitals-only persist it uses on a flee:
//   * ZM_PersistBattleVitalsToRecord -- copies curHP + each move's curPP + status, NEVER
//     level/exp/EVs (the KEY flee-persist lock).
//   * the 3-way winner branch: PLAYER -> full write-back; ENEMY(loss) -> set
//     m_bPendingWhiteout (no heal); COUNT(flee) -> vitals-only persist (no progression).
//   * ZM_Party::HealAllFull -- the whiteout heal the manager runs (curHP/PP/status).
// All PURE: the core is driven headlessly under instant-battles; no ECS, no scene.
// ############################################################################

// ZM_PersistBattleVitalsToRecord copies the flee-carried vitals (curHP, per-move curPP,
// major status) but leaves the progression fields (level, cumulative exp, EVs) UNTOUCHED
// even when the battle monster carries a higher level/exp/EV. This is the flee-persist lock.
ZENITH_TEST(ZM_Party, Vitals_PersistCopiesHpPpStatusNotProgression)
{
	ZM_Monster xRec = ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, 5u);
	// Snapshot the progression fields a vitals persist must NEVER rewrite.
	const u_int uLevelBefore = xRec.m_uLevel;        // 5
	const u_int uExpBefore   = xRec.m_uCurrentExp;   // L5 curve floor
	const u_int uEVBefore    = xRec.m_auEV[ZM_STAT_HP];

	ZM_BattleMonster xMon = ZM_BuildBattleMonster(ZM_MonsterToBattleSpec(xRec));
	// Vitals to carry: low HP, a spent move, a major status.
	xMon.m_uCurHP = 3u;
	ZENITH_ASSERT_GE(xMon.m_axMoves[0].m_uMaxPP, 1u, "slot 0 has PP to spend");
	xMon.m_axMoves[0].m_uCurPP = xMon.m_axMoves[0].m_uMaxPP - 1u;
	const u_int uSpentPP0 = xMon.m_axMoves[0].m_uCurPP;
	xMon.m_eStatus = ZM_MAJOR_STATUS_BURN;
	// Progression a flee must IGNORE -- bumped ABOVE the record so an accidental
	// full write-back would be detectable (a vitals copy must not touch these).
	xMon.m_uLevel           = uLevelBefore + 3u;
	xMon.m_uCurExp          = uExpBefore + 4321u;
	xMon.m_auEV[ZM_STAT_HP] = uEVBefore + 12u;

	ZM_PersistBattleVitalsToRecord(xMon, xRec);

	// Vitals copied.
	ZENITH_ASSERT_EQ(xRec.m_uCurrentHp, 3u, "curHP persisted");
	ZENITH_ASSERT_EQ(xRec.m_axMoves[0].m_uCurPP, uSpentPP0, "spent PP persisted");
	ZENITH_ASSERT_EQ((u_int)xRec.m_eStatus, (u_int)ZM_MAJOR_STATUS_BURN, "major status persisted");
	// Progression untouched (a flee never levels / awards exp / trains EVs).
	ZENITH_ASSERT_EQ(xRec.m_uLevel, uLevelBefore, "level NOT persisted by a vitals copy");
	ZENITH_ASSERT_EQ(xRec.m_uCurrentExp, uExpBefore, "exp NOT persisted by a vitals copy");
	ZENITH_ASSERT_EQ(xRec.m_auEV[ZM_STAT_HP], uEVBefore, "EV NOT persisted by a vitals copy");
}

// A real LOSS (winner == ENEMY): the write-back overload latches m_bPendingWhiteout and
// does NOT touch the party -- in particular it does NOT heal (that is the manager's job).
// A pre-damaged lead therefore stays damaged, proving no heal happened in the write-back.
ZENITH_TEST(ZM_Party, WriteBack_LossSetsPendingWhiteout)
{
	ZM_SetInstantBattlesForTests(true);

	// A WEAK player vs a STRONG enemy so the enemy reliably KOs the player (the L2 player
	// can never faint the L60 enemy, and the enemy one-shots the L2 -> winner is ENEMY).
	const ZM_BattleMonsterSpec xPlayer = ZM_BuildWildEnemySpec(ZM_SPECIES_FERNFAWN, 2u);
	const ZM_BattleMonsterSpec xEnemy  = ZM_BuildWildEnemySpec(ZM_SPECIES_KINDLET, 60u);

	ZM_BattleDirectorCore xCore;
	xCore.Begin(&xPlayer, 1u, &xEnemy, 1u, MakeWildConfig(), 0x105Full, ZM_AI_TIER_GREEDY);
	DriveDirectorToEnd(xCore, MakeMoveSlot0());
	ZENITH_ASSERT_TRUE(xCore.ShouldRequestEnd(), "the loss drive should resolve the battle");
	ZENITH_ASSERT_EQ((u_int)xCore.GetWinner(), (u_int)ZM_SIDE_ENEMY,
		"a weak L2 player against a strong L60 enemy must lose (winner ENEMY)");

	// The persistent lead, pre-damaged so "unchanged" proves the write-back did NOT heal it.
	ZM_GameState xState;
	ZENITH_ASSERT_TRUE(xState.m_xParty.Add(ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, 2u)),
		"seed the lead at slot 0");
	xState.m_xParty.Get(0u).m_uCurrentHp = 1u;
	const u_int uHpBefore = xState.m_xParty.Get(0u).m_uCurrentHp;
	ZENITH_ASSERT_FALSE(xState.m_bPendingWhiteout, "no whiteout pending before the loss");

	ZM_ApplyBattleResultToParty(xState, xCore);

	ZENITH_ASSERT_TRUE(xState.m_bPendingWhiteout, "a loss latches the pending whiteout");
	ZENITH_ASSERT_EQ(xState.m_xParty.Get(0u).m_uCurrentHp, uHpBefore,
		"the write-back does NOT heal on a loss -- the pre-damaged lead stays damaged");

	ZM_SetInstantBattlesForTests(false);
}

// A real PLAYER win: the write-back overload never latches the whiteout (that is loss-only),
// and it writes the resolved lead back into the party (identity intact, level not regressed).
ZENITH_TEST(ZM_Party, WriteBack_WinDoesNotSetWhiteout)
{
	ZM_SetInstantBattlesForTests(true);

	// A STRONG player vs a WEAK enemy: the L20 player one-shots the L2 enemy (winner PLAYER).
	const ZM_BattleMonsterSpec xPlayer = ZM_BuildWildEnemySpec(ZM_SPECIES_FERNFAWN, 20u);
	const ZM_BattleMonsterSpec xEnemy  = ZM_BuildWildEnemySpec(ZM_SPECIES_KINDLET, 2u);

	ZM_BattleDirectorCore xCore;
	xCore.Begin(&xPlayer, 1u, &xEnemy, 1u, MakeWildConfig(), 0x2A22ull, ZM_AI_TIER_GREEDY);
	DriveDirectorToEnd(xCore, MakeMoveSlot0());
	ZENITH_ASSERT_TRUE(xCore.ShouldRequestEnd(), "the win drive should resolve the battle");
	ZENITH_ASSERT_EQ((u_int)xCore.GetWinner(), (u_int)ZM_SIDE_PLAYER,
		"a strong L20 player against a weak L2 enemy must win (winner PLAYER)");

	ZM_GameState xState;
	ZENITH_ASSERT_TRUE(xState.m_xParty.Add(ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, 20u)),
		"seed the lead at slot 0");
	const u_int uLevelBefore = xState.m_xParty.Get(0u).m_uLevel;   // 20

	ZM_ApplyBattleResultToParty(xState, xCore);

	ZENITH_ASSERT_FALSE(xState.m_bPendingWhiteout, "a win must NEVER latch the whiteout");
	// The win branch wrote the resolved lead back: identity is intact and the level never
	// regressed (exp is OFF for this config, so the level stays put rather than rising).
	ZENITH_ASSERT_EQ((u_int)xState.m_xParty.Get(0u).m_eSpecies, (u_int)ZM_SPECIES_FERNFAWN,
		"identity (species) survives the win write-back");
	ZENITH_ASSERT_GE(xState.m_xParty.Get(0u).m_uLevel, uLevelBefore, "the win never regresses the lead's level");

	ZM_SetInstantBattlesForTests(false);
}

// A real FLEE (winner == COUNT): the write-back overload never latches the whiteout and
// never writes progression -- the lead's level + cumulative exp are left exactly as they
// were (only the vitals-only persist runs, which this fixture's distinct-level lead exposes).
ZENITH_TEST(ZM_Party, WriteBack_FleeDoesNotSetWhiteoutOrProgress)
{
	ZM_SetInstantBattlesForTests(true);

	// The faster L5 player flees the weak L2 enemy every time (guaranteed flee).
	const ZM_BattleMonsterSpec xPlayer = ZM_BuildWildEnemySpec(ZM_SPECIES_FERNFAWN, 5u);
	const ZM_BattleMonsterSpec xEnemy  = ZM_BuildWildEnemySpec(ZM_SPECIES_KINDLET, 2u);

	ZM_BattleDirectorCore xCore;
	xCore.Begin(&xPlayer, 1u, &xEnemy, 1u, MakeWildConfig(), 0xF1EEull, ZM_AI_TIER_GREEDY);
	DriveDirectorToEnd(xCore, MakeRunAction());
	ZENITH_ASSERT_TRUE(xCore.ShouldRequestEnd(), "the flee drive should resolve the battle");
	ZENITH_ASSERT_EQ((u_int)xCore.GetWinner(), (u_int)ZM_SIDE_COUNT,
		"a successful flee ends with no winner (COUNT)");

	// A lead at a DISTINCT level (6) from the battle monster (5): a buggy full write-back
	// would overwrite it to 5, so "unchanged" is a decisive lock, not vacuous.
	ZM_GameState xState;
	ZENITH_ASSERT_TRUE(xState.m_xParty.Add(ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, 6u)),
		"seed a distinct-level lead at slot 0");
	const u_int uLevelBefore = xState.m_xParty.Get(0u).m_uLevel;      // 6
	const u_int uExpBefore   = xState.m_xParty.Get(0u).m_uCurrentExp; // L6 floor

	ZM_ApplyBattleResultToParty(xState, xCore);

	ZENITH_ASSERT_FALSE(xState.m_bPendingWhiteout, "a flee must NEVER latch the whiteout");
	ZENITH_ASSERT_EQ(xState.m_xParty.Get(0u).m_uLevel, uLevelBefore, "a flee never levels the lead");
	ZENITH_ASSERT_EQ(xState.m_xParty.Get(0u).m_uCurrentExp, uExpBefore, "a flee never awards exp");

	ZM_SetInstantBattlesForTests(false);
}

// ZM_SIDE_COUNT is returned for BOTH a successful flee AND a DRAW/double-KO. The classifier
// must NOT treat a draw that fainted the lead as a flee (which would persist a 0-HP lead and
// SKIP the whiteout, stranding the player with a wiped party). A real flee leaves the lead
// alive; a COUNT-with-fainted-lead is a party wipe -> WHITEOUT, same as an ENEMY loss.
ZENITH_TEST(ZM_Party, WriteBack_ClassifyDrawVsFleeVsLoss)
{
	ZENITH_ASSERT_EQ((u_int)ZM_ClassifyBattleResult(ZM_SIDE_PLAYER, false),
		(u_int)ZM_BRA_WRITE_BACK_WIN, "PLAYER win -> write-back");
	ZENITH_ASSERT_EQ((u_int)ZM_ClassifyBattleResult(ZM_SIDE_ENEMY, false),
		(u_int)ZM_BRA_WHITEOUT, "ENEMY win -> whiteout");
	ZENITH_ASSERT_EQ((u_int)ZM_ClassifyBattleResult(ZM_SIDE_ENEMY, true),
		(u_int)ZM_BRA_WHITEOUT, "ENEMY win (lead fainted) -> whiteout");
	ZENITH_ASSERT_EQ((u_int)ZM_ClassifyBattleResult(ZM_SIDE_COUNT, false),
		(u_int)ZM_BRA_PERSIST_VITALS, "COUNT with a LIVE lead is a real flee -> persist vitals");
	ZENITH_ASSERT_EQ((u_int)ZM_ClassifyBattleResult(ZM_SIDE_COUNT, true),
		(u_int)ZM_BRA_WHITEOUT, "COUNT with a FAINTED lead is a DRAW/double-KO wipe -> whiteout, NOT a flee");
}

// The whiteout heal the manager runs on a loss: HealAllFull restores EVERY member's curHP
// to max, every move's PP to max, AND clears major status (the status dimension the S5-SC1
// Party_HealAllFullRestoresEveryMember unit does not cover -- this is the loss-specific case).
ZENITH_TEST(ZM_Party, Party_HealAllFullFromLossRestoresEveryMember)
{
	ZM_Party xParty;
	xParty.Add(ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, 5u));
	xParty.Add(ZM_BuildMonsterRecord(ZM_SPECIES_KINDLET, 5u));

	const u_int uMaxHP0 = xParty.Get(0u).GetMaxHP();
	const u_int uMaxHP1 = xParty.Get(1u).GetMaxHP();
	const u_int uMaxPP0 = xParty.Get(0u).m_axMoves[0].m_uMaxPP;
	const u_int uMaxPP1 = xParty.Get(1u).m_axMoves[0].m_uMaxPP;

	// A battle loss leaves the party damaged: chipped/fainted HP, spent PP, and -- the
	// loss-specific dimension -- a lingering major status on each member.
	xParty.Get(0u).m_uCurrentHp = 1u;
	xParty.Get(0u).m_axMoves[0].m_uCurPP = 0u;
	xParty.Get(0u).m_eStatus = ZM_MAJOR_STATUS_BURN;
	xParty.Get(1u).m_uCurrentHp = 0u;                        // fainted
	xParty.Get(1u).m_axMoves[0].m_uCurPP = 0u;
	xParty.Get(1u).m_eStatus = ZM_MAJOR_STATUS_PARALYSIS;

	xParty.HealAllFull();

	ZENITH_ASSERT_EQ(xParty.Get(0u).m_uCurrentHp, uMaxHP0, "member 0 HP restored to max");
	ZENITH_ASSERT_EQ(xParty.Get(1u).m_uCurrentHp, uMaxHP1, "member 1 HP restored to max");
	ZENITH_ASSERT_EQ(xParty.Get(0u).m_axMoves[0].m_uCurPP, uMaxPP0, "member 0 PP restored to max");
	ZENITH_ASSERT_EQ(xParty.Get(1u).m_axMoves[0].m_uCurPP, uMaxPP1, "member 1 PP restored to max");
	ZENITH_ASSERT_EQ((u_int)xParty.Get(0u).m_eStatus, (u_int)ZM_MAJOR_STATUS_NONE, "member 0 status cleared");
	ZENITH_ASSERT_EQ((u_int)xParty.Get(1u).m_eStatus, (u_int)ZM_MAJOR_STATUS_NONE, "member 1 status cleared");
	ZENITH_ASSERT_FALSE(xParty.AllFainted(), "nobody fainted after a full heal");
}
