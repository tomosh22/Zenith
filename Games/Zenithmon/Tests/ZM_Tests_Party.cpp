#include "Zenith.h"

// ============================================================================
// ZM_Tests_Party -- S5 item 5 SC1 unit tests for the PURE persistent player data
// model: ZM_Monster (record) / ZM_Party / ZM_GameState + the record<->battle
// conversions. All hermetic: no ECS, no scene, no graphics, no RNG, no I/O --
// only the pure S1 data formulas the model is built on. Category ZM_Party.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "UnitTests/Zenith_AssertCapture.h"               // SC5: the trainer payout's totality proof
#include "DataStream/Zenith_DataStream.h"                // S8 G1-3: the badge/item save round trip
#include "Zenithmon/Source/Core/ZM_SaveSchema.h"          // S8 G1-3: ZM_SaveSchema::Write / ::Read
#include "Zenithmon/Source/Party/ZM_Monster.h"
#include "Zenithmon/Source/Party/ZM_Party.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"          // ZM_MakeNewGameState
#include "Zenithmon/Source/Party/ZM_StarterChoice.h"      // ZM_ApplyStarterChoice / ZM_STARTER_CHOICE_FERNFAWN
#include "Zenithmon/Source/Party/ZM_BattleWriteBack.h"   // ZM_ApplyBattleResultToParty (win-only lead persist)
#include "Zenithmon/Source/Battle/ZM_BattleMonster.h"    // ZM_BuildBattleMonster, uZM_CURHP_UNSPECIFIED
#include "Zenithmon/Source/Battle/ZM_BattleDirectorCore.h" // ZM_BuildWildEnemySpec
#include "Zenithmon/Source/Battle/ZM_ExpAndLevel.h"      // ZM_ExpForLevel, ZM_GetSpeciesGrowthRate
#include "Zenithmon/Source/Data/ZM_BadgeData.h"          // ZM_BADGE_BLOOM / ZM_BADGE_KILN (S8 G1-3 reward)
#include "Zenithmon/Source/Data/ZM_ItemData.h"           // ZM_ITEM_TM_VERDANTLASH (S8 G1-3 reward)
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"        // ZM_GetSpeciesBaseStats/Abilities
#include "Zenithmon/Source/Data/ZM_StoryFlags.h"         // ZM_SetStoryFlag / ZM_IsStoryFlagSet (SC5 trainer reward)
#include "Zenithmon/Source/Data/ZM_TrainerData.h"        // ZM_TRAINER_ID + the authored roster rows (SC5)
#include "Zenithmon/Source/Data/ZM_StatCalc.h"           // ZM_CalcStat, uZM_MAX_IV
#include "Zenithmon/Source/Data/ZM_MoveData.h"           // ZM_GetMoveData
#include "Zenithmon/Source/Data/ZM_Learnsets.h"          // ZM_GetSpeciesLearnset

#include <vector>                                         // S8 G1-3: byte snapshot for the save round trip

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

	// The exact composition production ships (ZM_GameStateManager's three seed
	// sites): a new game plus the Fernfawn grant. This is what the deleted
	// starter seed produced field for field, so every unit routed through it keeps
	// the fixture it was written against.
	//
	// GameState_NewGamePlusFernfawnIsSingleValidStarter deliberately does NOT use
	// it: that unit IS the composition's contract and may not be routed through the
	// helper it exists to pin -- a helper that silently stopped granting anything
	// would then take the unit down with it and the unit would be proving nothing.
	//
	// ★ BY REFERENCE, NOT A RETURNING FACTORY, AND THAT IS THE STACK BUDGET TALKING.
	// A `ZM_GameState MakeFernfawn...()` holds one MORE live ZM_GameState at the
	// deepest point of every call: under /Od NRVO does not fire, so the helper's named
	// local is copied into the caller's slot while ZM_MakeNewGameState's own local is
	// still alive. ZM_GameState is dominated by ZM_BoxStorage, so that third copy is
	// tens of KB against this exe's 1 MB main-thread reserve -- the same arithmetic
	// that overflowed __chkstk in Tests/ZM_AutoTests_SaveContinue.cpp (see the note
	// above SCSeedFernfawnStarter there, whose shape this deliberately mirrors).
	// Written this way the peak matches the single-factory call it replaces.
	void SeedFernfawnStarterFixture(ZM_GameState& xStateOut)
	{
		xStateOut = ZM_MakeNewGameState();
		ZM_ApplyStarterChoice(xStateOut, ZM_STARTER_CHOICE_FERNFAWN);
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

// The composition every production seed site ships -- a new game plus the
// Fernfawn grant -- is exactly one valid Fernfawn L5 with a level-1 move, its
// species marked caught, and no pending whiteout.
//
// Spelled INLINE rather than through SeedFernfawnStarterFixture(): this unit is
// the composition's contract, so routing it through the helper would make it
// restate the helper instead of pinning the two production calls.
ZENITH_TEST(ZM_Party, GameState_NewGamePlusFernfawnIsSingleValidStarter)
{
	ZM_GameState xState = ZM_MakeNewGameState();
	ZM_ApplyStarterChoice(xState, ZM_STARTER_CHOICE_FERNFAWN);

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

// ############################################################################
// H. Trainer reward write-back (S7 item 3 SC5)
//
// ZM_ApplyTrainerResultToGameState -- the trainer half of resolved-battle
// persistence. WIN-ONLY, and the win test is NOT re-implemented: it routes
// through the SAME shipped ZM_ClassifyBattleResult that section G pins, so the
// trainer arm and the wild arm can never drift about what "the player won" means.
//
// What these cases pin is the ROUTING of the prize, not AddMoney's saturation
// (ZM_Bag / Money_AddSaturatesAtTheCapWithoutWrapping already owns that) and not
// the flag codec (ZM_Story / Accessor_SetThenIsSetRoundTripsEveryRegisteredFlag
// owns that):
//   * a win credits exactly the ROW's prize and sets the ROW's defeat flag;
//   * a loss / draw / flee pays NOTHING and never touches m_bPendingWhiteout --
//     ZM_ApplyBattleResultToParty stays the SINGLE owner of that latch;
//   * a second win is IDEMPOTENT on the flag and NOT on the money (SC5's
//     deliberate answer -- "pay once" is a caller-side gate, and ZM-D-135 forbids
//     a "trainers already paid" member on the frozen ZM_GameState);
//   * a ZM_STORY_FLAG_NONE row still pays and writes no bit at all;
//   * an unregistered id is a TOTAL, SILENT no-op.
// All PURE: a game state, a roster id, and two primitives. No ECS, no scene.
// ############################################################################

// The headline case: beating the rival credits the authored prize and sets the
// authored defeat flag, and does nothing else.
ZENITH_TEST(ZM_Party, TrainerReward_WinCreditsThePrizeAndSetsTheDefeatFlag)
{
	ZM_GameState xState;
	SeedFernfawnStarterFixture(xState);
	const ZM_TrainerData& xRow = ZM_GetTrainerData(ZM_TRAINER_RIVAL_VESPER);
	const u_int uMoneyBefore = xState.m_uMoney;
	const u_int uPartyBefore = xState.m_xParty.Count();
	const u_int uBadgeCountBefore = xState.GetBadgeCount();
	const u_int uBagStacksBefore = xState.m_xBag.TotalStackCount();

	// ANTI-VACUITY, before anything is applied: an already-set flag would make the
	// transition meaningless, and a purse near the cap would make the delta a
	// saturation artefact rather than the routed prize.
	ZENITH_ASSERT_FALSE(ZM_IsStoryFlagSet(xState, ZM_STORY_FLAG_RIVAL1_DEFEATED),
		"the starter state already has the rival flag set -- the transition is vacuous");
	ZENITH_ASSERT_LT(uMoneyBefore + xRow.m_uPrizeMoney, uZM_MONEY_CAP,
		"the starter purse plus the prize is at/over the cap -- the credit delta would be "
		"a saturation artefact rather than the routed prize");

	const ZM_TrainerRewardResult xResult = ZM_ApplyTrainerResultToGameState(
		xState, ZM_TRAINER_RIVAL_VESPER, ZM_SIDE_PLAYER, false);

	ZENITH_ASSERT_TRUE(xResult.m_bApplied, "a PLAYER win over a registered trainer must pay out");
	ZENITH_ASSERT_EQ(xResult.m_uMoneyCredited, xRow.m_uPrizeMoney,
		"the credited amount is not the row's prize");
	ZENITH_ASSERT_EQ(xResult.m_uMoneyCredited, 500u,
		"the rival's authored prize is 500 -- either the row or the routing changed");
	ZENITH_ASSERT_EQ(xState.m_uMoney, uMoneyBefore + 500u, "the purse did not gain exactly the prize");
	ZENITH_ASSERT_TRUE(xResult.m_bFlagNewlySet, "the rival's defeat flag went 0 -> 1 and must be reported");
	ZENITH_ASSERT_TRUE(ZM_IsStoryFlagSet(xState, ZM_STORY_FLAG_RIVAL1_DEFEATED),
		"beating the rival must set ZM_STORY_FLAG_RIVAL1_DEFEATED");
	ZENITH_ASSERT_FALSE(xState.m_bPendingWhiteout,
		"a WIN must never latch a whiteout -- this helper is not the latch owner at all");
	ZENITH_ASSERT_EQ(xState.m_xParty.Count(), uPartyBefore,
		"the trainer payout touched a monster record -- it owns money and flags ONLY");

	// S8 G1-3: Vesper's row carries ZM_BADGE_NONE / ZM_ITEM_NONE -- confirm the
	// reward columns are a genuine TOTAL no-op for a row that names neither, not
	// merely untested. A helper that awarded SOME badge or item regardless of the
	// row would only be caught here and at the by-row fixture below.
	ZENITH_ASSERT_FALSE(xResult.m_bBadgeNewlyAwarded, "Vesper's row carries no badge reward");
	ZENITH_ASSERT_FALSE(xResult.m_bItemGranted, "Vesper's row carries no item reward");
	ZENITH_ASSERT_EQ(xState.GetBadgeCount(), uBadgeCountBefore, "a rival win must not award any badge");
	ZENITH_ASSERT_EQ(xState.m_xBag.TotalStackCount(), uBagStacksBefore,
		"a rival win must not add any bag stack");
}

// Fail closed on every outcome that is not an outright player win, and leave the
// loss half entirely to the shipped ZM_ApplyBattleResultToParty whiteout latch.
ZENITH_TEST(ZM_Party, TrainerReward_LossAndDrawPayNothingAndLeaveTheWhiteoutToTheExistingPath)
{
	struct Shape
	{
		ZM_SIDE     m_eWinner;
		bool        m_bLeadFainted;
		const char* m_szLabel;
	};
	const Shape axShapes[3] =
	{
		{ ZM_SIDE_ENEMY, true,  "an ENEMY win (a loss)" },
		{ ZM_SIDE_COUNT, true,  "a COUNT draw/double-KO that wiped the lead" },
		{ ZM_SIDE_COUNT, false, "a COUNT flee/draw with a live lead" },
	};

	ZM_GameState xState;
	for (u_int u = 0u; u < 3u; ++u)
	{
		SeedFernfawnStarterFixture(xState);
		const u_int uMoneyBefore = xState.m_uMoney;

		const ZM_TrainerRewardResult xResult = ZM_ApplyTrainerResultToGameState(
			xState, ZM_TRAINER_RIVAL_VESPER, axShapes[u].m_eWinner, axShapes[u].m_bLeadFainted);

		ZENITH_ASSERT_FALSE(xResult.m_bApplied, "%s must not pay out", axShapes[u].m_szLabel);
		ZENITH_ASSERT_EQ(xResult.m_uMoneyCredited, 0u, "%s credited money", axShapes[u].m_szLabel);
		ZENITH_ASSERT_EQ(xState.m_uMoney, uMoneyBefore, "%s moved the purse", axShapes[u].m_szLabel);
		ZENITH_ASSERT_FALSE(xResult.m_bFlagNewlySet, "%s reported a flag write", axShapes[u].m_szLabel);
		ZENITH_ASSERT_FALSE(ZM_IsStoryFlagSet(xState, ZM_STORY_FLAG_RIVAL1_DEFEATED),
			"%s set the rival's defeat flag", axShapes[u].m_szLabel);
		ZENITH_ASSERT_FALSE(xState.m_bPendingWhiteout,
			"%s latched m_bPendingWhiteout -- ZM_ApplyBattleResultToParty is its SINGLE owner",
			axShapes[u].m_szLabel);
	}

	// State the hand-off explicitly rather than leaving it implied: the two shapes
	// that wipe the party are WHITEOUTs, and the shipped write-back is what latches
	// them. The trainer helper deliberately duplicates none of that.
	ZENITH_ASSERT_EQ((u_int)ZM_ClassifyBattleResult(ZM_SIDE_ENEMY, true), (u_int)ZM_BRA_WHITEOUT,
		"an ENEMY win is a WHITEOUT owned by ZM_ApplyBattleResultToParty");
	ZENITH_ASSERT_EQ((u_int)ZM_ClassifyBattleResult(ZM_SIDE_COUNT, true), (u_int)ZM_BRA_WHITEOUT,
		"a COUNT draw with a fainted lead is a WHITEOUT owned by ZM_ApplyBattleResultToParty");
}

// The deliberate asymmetry: ZM_SetStoryFlag is idempotent, AddMoney is not. SC5's
// answer is that "pay once" is a CALLER-side gate (SC6's sight FSM ANDs in "not
// yet defeated") because ZM-D-135 forbids a "trainers already paid" member on the
// frozen ZM_GameState. m_bFlagNewlySet is the lever that gate will read.
ZENITH_TEST(ZM_Party, TrainerReward_SecondWinIsFlagIdempotentAndPaysAgain)
{
	ZM_GameState xState;
	SeedFernfawnStarterFixture(xState);
	const u_int uMoneyBefore = xState.m_uMoney;
	ZENITH_ASSERT_LT(uMoneyBefore + 1000u, uZM_MONEY_CAP,
		"two prizes must fit under the cap or the second credit is a saturation artefact");

	const ZM_TrainerRewardResult xFirst = ZM_ApplyTrainerResultToGameState(
		xState, ZM_TRAINER_RIVAL_VESPER, ZM_SIDE_PLAYER, false);
	ZENITH_ASSERT_TRUE(xFirst.m_bFlagNewlySet, "the FIRST win must report a 0 -> 1 flag transition");
	ZENITH_ASSERT_EQ(xFirst.m_uMoneyCredited, 500u, "the first win must credit the authored prize");

	const ZM_TrainerRewardResult xSecond = ZM_ApplyTrainerResultToGameState(
		xState, ZM_TRAINER_RIVAL_VESPER, ZM_SIDE_PLAYER, false);

	ZENITH_ASSERT_TRUE(xSecond.m_bApplied, "a second win over the same trainer still applies");
	ZENITH_ASSERT_EQ(xSecond.m_uMoneyCredited, 500u,
		"the money is NOT idempotent by design -- the re-engagement gate is the caller's");
	ZENITH_ASSERT_EQ(xState.m_uMoney, uMoneyBefore + 1000u, "two wins must credit two prizes");
	ZENITH_ASSERT_FALSE(xSecond.m_bFlagNewlySet,
		"the flag write is idempotent, so the SECOND win set nothing NEW -- this false is "
		"exactly the lever SC6's 'never re-spot a beaten trainer' guard reads");
	ZENITH_ASSERT_TRUE(ZM_IsStoryFlagSet(xState, ZM_STORY_FLAG_RIVAL1_DEFEATED),
		"the flag is still SET after the second win -- idempotent, never toggled");
}

// The credit is REPORTED as the observed purse delta, not as the row's prize, so
// a saturated credit reports what actually landed. Story progress is NEVER
// coupled to the money landing.
ZENITH_TEST(ZM_Party, TrainerReward_MoneyAtTheCapCreditsNothingAndStillSetsTheFlag)
{
	ZM_GameState xState;

	// Case A -- exactly at the cap: nothing lands, the flag still does.
	SeedFernfawnStarterFixture(xState);
	xState.m_uMoney = uZM_MONEY_CAP;
	{
		const ZM_TrainerRewardResult xResult = ZM_ApplyTrainerResultToGameState(
			xState, ZM_TRAINER_RIVAL_VESPER, ZM_SIDE_PLAYER, false);
		ZENITH_ASSERT_EQ(xResult.m_uMoneyCredited, 0u,
			"a purse at the cap has no headroom, so NOTHING was credited");
		ZENITH_ASSERT_EQ(xState.m_uMoney, uZM_MONEY_CAP, "the capped purse saturated, never wrapped");
		ZENITH_ASSERT_TRUE(xResult.m_bApplied, "a win still APPLIED even though no money landed");
		ZENITH_ASSERT_TRUE(xResult.m_bFlagNewlySet, "the defeat flag is never coupled to the money");
		ZENITH_ASSERT_TRUE(ZM_IsStoryFlagSet(xState, ZM_STORY_FLAG_RIVAL1_DEFEATED),
			"story progress must survive a saturated prize");
	}

	// Case B -- partial headroom: exactly the headroom lands, not the whole prize.
	SeedFernfawnStarterFixture(xState);
	xState.m_uMoney = uZM_MONEY_CAP - 100u;
	{
		const ZM_TrainerRewardResult xResult = ZM_ApplyTrainerResultToGameState(
			xState, ZM_TRAINER_RIVAL_VESPER, ZM_SIDE_PLAYER, false);
		ZENITH_ASSERT_EQ(xResult.m_uMoneyCredited, 100u,
			"the report must be the OBSERVED delta (100), not the row's prize (500)");
		ZENITH_ASSERT_EQ(xState.m_uMoney, uZM_MONEY_CAP, "the partial credit lands exactly at the cap");
	}

	// Case C -- an imported over-cap purse (only reachable through a hand-edited
	// save; module 7 restores the full uint32). AddMoney credits nothing there.
	SeedFernfawnStarterFixture(xState);
	xState.m_uMoney = uZM_MONEY_CAP + 5000u;
	{
		const ZM_TrainerRewardResult xResult = ZM_ApplyTrainerResultToGameState(
			xState, ZM_TRAINER_RIVAL_VESPER, ZM_SIDE_PLAYER, false);
		ZENITH_ASSERT_EQ(xResult.m_uMoneyCredited, 0u, "an over-cap purse credits nothing");
		ZENITH_ASSERT_EQ(xState.m_uMoney, uZM_MONEY_CAP + 5000u,
			"an over-cap balance is preserved byte-for-byte, never clamped down");
	}
}

// ZM_TRAINER_ROUTE1_RAMBLER carries ZM_STORY_FLAG_NONE -- the arm no other roster
// row exercises. The prize is still paid; ZM_SetStoryFlag returns false with no
// mutation and no log, so there is deliberately NO 'NONE' branch in the helper.
ZENITH_TEST(ZM_Party, TrainerReward_UnflaggedRowStillPaysAndWritesNoFlag)
{
	// ANTI-VACUITY: if this row ever GAINS a flag, this case stops testing the
	// unflagged arm and must be re-pointed rather than silently passing.
	ZENITH_ASSERT_EQ((u_int)ZM_GetTrainerData(ZM_TRAINER_ROUTE1_RAMBLER).m_eDefeatFlag,
		(u_int)ZM_STORY_FLAG_NONE,
		"the rambler no longer carries ZM_STORY_FLAG_NONE -- this case no longer covers "
		"the unflagged arm");

	ZM_GameState xState;
	SeedFernfawnStarterFixture(xState);
	const u_int uMoneyBefore = xState.m_uMoney;

	const ZM_TrainerRewardResult xResult = ZM_ApplyTrainerResultToGameState(
		xState, ZM_TRAINER_ROUTE1_RAMBLER, ZM_SIDE_PLAYER, false);

	ZENITH_ASSERT_TRUE(xResult.m_bApplied, "an unflagged trainer still pays out on a win");
	ZENITH_ASSERT_EQ(xResult.m_uMoneyCredited, 120u, "the rambler's authored prize is 120");
	ZENITH_ASSERT_EQ(xState.m_uMoney, uMoneyBefore + 120u, "the purse did not gain exactly the prize");
	ZENITH_ASSERT_FALSE(xResult.m_bFlagNewlySet, "a ZM_STORY_FLAG_NONE row writes no flag");

	// NO bit at all -- not merely "not the rival's". A helper that substituted a
	// hard-coded flag for the row's would be caught here and nowhere else.
	for (u_int u = 0u; u < (u_int)ZM_STORY_FLAG_COUNT; ++u)
	{
		ZENITH_ASSERT_FALSE(ZM_IsStoryFlagSet(xState, (ZM_STORY_FLAG_ID)u),
			"an unflagged trainer's payout set story flag %u", u);
	}
}

// TOTAL and SILENT for every id the roster does not name -- including on the WIN
// outcome, the one that would otherwise pay. The registered guard runs BEFORE
// ZM_GetTrainerData precisely because that accessor logs a non-fatal
// Zenith_Error on an unregistered id, and a total no-op must be silent.
ZENITH_TEST(ZM_Party, TrainerReward_UnregisteredTrainerIsATotalSilentNoOp)
{
	const ZM_TRAINER_ID aeUnregistered[3] =
	{
		ZM_TRAINER_NONE,
		(ZM_TRAINER_ID)ZM_TRAINER_COUNT,
		(ZM_TRAINER_ID)77u
	};

	ZM_GameState xState;
	SeedFernfawnStarterFixture(xState);
	const u_int uMoneyBefore = xState.m_uMoney;

	// NO ZENITH_ASSERT_* inside the capture scope (it swallows framework failures),
	// and the hit count is copied out BEFORE the closing brace (the dtor restores
	// the previous count).
	ZM_TrainerRewardResult axResults[3];
	u_int uHits = 0u;
	{
		Zenith_AssertCaptureScope xCapture;
		for (u_int u = 0u; u < 3u; ++u)
		{
			axResults[u] = ZM_ApplyTrainerResultToGameState(
				xState, aeUnregistered[u], ZM_SIDE_PLAYER, false);
		}
		uHits = (u_int)xCapture.GetHitCount();
	}

	ZENITH_ASSERT_EQ(uHits, 0u,
		"the trainer payout asserted on an unregistered id -- Zenith_Assert breaks in "
		"EVERY config, so this would kill the whole boot unit run");

	for (u_int u = 0u; u < 3u; ++u)
	{
		ZENITH_ASSERT_FALSE(axResults[u].m_bApplied,
			"unregistered id %u reported an applied payout", (u_int)aeUnregistered[u]);
		ZENITH_ASSERT_EQ(axResults[u].m_uMoneyCredited, 0u,
			"unregistered id %u credited money", (u_int)aeUnregistered[u]);
		ZENITH_ASSERT_FALSE(axResults[u].m_bFlagNewlySet,
			"unregistered id %u reported a flag write", (u_int)aeUnregistered[u]);
	}
	ZENITH_ASSERT_EQ(xState.m_uMoney, uMoneyBefore, "an unregistered id moved the purse");
	for (u_int u = 0u; u < (u_int)ZM_STORY_FLAG_COUNT; ++u)
	{
		ZENITH_ASSERT_FALSE(ZM_IsStoryFlagSet(xState, (ZM_STORY_FLAG_ID)u),
			"an unregistered id's payout set story flag %u", u);
	}
}

// ############################################################################
// I. Gym-leader reward: badge + take-home item on a win (S8 G1-3, ZM-70)
//
// ZM_TrainerRewardResult::m_bBadgeNewlyAwarded / m_bItemGranted extend the SAME
// by-id/by-row primitive section H already pins for money+flag; the classify /
// registered / fail-closed / totality arms proven there are NOT re-proven here.
// ZM_TRAINER_GYM1_FENNA is the first (and, today, only) row whose m_eBadgeReward /
// m_eItemReward are not both NONE, so she is what exercises the PRODUCTION content;
// the by-row unit below exercises the MECHANISM independent of her, per
// ZM-D-208 Ruling 2's reasoning applied to the reward half (ZM-D-209).
// ############################################################################

// The headline case: beating Fenna credits her prize, sets her flag, AND awards the
// Bloom Badge + grants Verdant Lash -- all four in one call, all read off her row.
ZENITH_TEST(ZM_Party, TrainerReward_BeatingFennaAwardsTheBloomBadgeAndGrantsVerdantLash)
{
	ZM_GameState xState;
	SeedFernfawnStarterFixture(xState);
	ZENITH_ASSERT_FALSE(xState.HasBadge((u_int)ZM_BADGE_BLOOM),
		"the starter state already has the Bloom Badge -- the transition is vacuous");
	ZENITH_ASSERT_FALSE(xState.m_xBag.Has(ZM_ITEM_TM_VERDANTLASH),
		"the starter bag already carries Verdant Lash -- the transition is vacuous");
	ZENITH_ASSERT_LT(xState.m_uMoney + 2600u, uZM_MONEY_CAP,
		"the starter purse plus Fenna's prize is at/over the cap -- the credit delta would be "
		"a saturation artefact rather than the routed prize");

	const ZM_TrainerRewardResult xResult = ZM_ApplyTrainerResultToGameState(
		xState, ZM_TRAINER_GYM1_FENNA, ZM_SIDE_PLAYER, false);

	ZENITH_ASSERT_TRUE(xResult.m_bApplied, "a PLAYER win over Fenna must pay out");
	ZENITH_ASSERT_EQ(xResult.m_uMoneyCredited, 2600u, "Fenna's authored prize is 2600");
	ZENITH_ASSERT_TRUE(xResult.m_bFlagNewlySet, "beating Fenna must set ZM_STORY_FLAG_GYM1_DEFEATED");
	ZENITH_ASSERT_TRUE(ZM_IsStoryFlagSet(xState, ZM_STORY_FLAG_GYM1_DEFEATED),
		"ZM_STORY_FLAG_GYM1_DEFEATED must actually be set");

	ZENITH_ASSERT_TRUE(xResult.m_bBadgeNewlyAwarded, "beating Fenna must newly award a badge");
	ZENITH_ASSERT_TRUE(xState.HasBadge((u_int)ZM_BADGE_BLOOM), "the Bloom Badge must be held");
	ZENITH_ASSERT_EQ(xState.GetBadgeCount(), 1u, "exactly one badge after the first gym win");

	ZENITH_ASSERT_TRUE(xResult.m_bItemGranted, "beating Fenna must grant an item");
	ZENITH_ASSERT_TRUE(xState.m_xBag.Has(ZM_ITEM_TM_VERDANTLASH), "Verdant Lash must be in the bag");
	ZENITH_ASSERT_EQ(xState.m_xBag.GetCount(ZM_ITEM_TM_VERDANTLASH), 1u,
		"exactly one Verdant Lash after the first win");
}

// ZM-D-208 Ruling 2's reasoning, applied to the reward half: a helper keyed on
// ZM_TRAINER_ID can only be exercised through the shipped roster, so its coverage
// dies the moment content changes. This drives the badge+item grant off a
// HAND-BUILT row that is not Fenna's and carries NEITHER her badge NOR her item,
// proving the mechanism reads BOTH rewards OFF THE ROW rather than off a
// hardcoded trainer id -- and that fail-closed still holds through the by-row
// seam. (ZM-70 review correction N7: the item used to reuse Fenna's own TM, which
// could not distinguish row-driven from hard-coded the way the badge check
// already did -- ZM_ITEM_TM_TITANBEAM closes that gap for free.)
ZENITH_TEST(ZM_Party, TrainerReward_ByRowPrimitiveGrantsWhateverTheRowNames)
{
	// Deliberately NOT Fenna: m_eId borrows the rambler's (irrelevant to this
	// primitive, which never inspects m_eId), the badge is Gym 2's, not Gym 1's,
	// and the item is a DIFFERENT TM -- not Verdant Lash, which is what
	// TrainerReward_BeatingFennaAwardsTheBloomBadgeAndGrantsVerdantLash above
	// already exercises as PRODUCTION content.
	const ZM_TrainerData xFixtureRow =
	{
		ZM_TRAINER_ROUTE1_RAMBLER, "fixture leader", nullptr, 0u, 0u,
		ZM_STORY_FLAG_NONE, ZM_AI_TIER_NONE,
		nullptr, 0u,
		ZM_BADGE_KILN, ZM_ITEM_TM_TITANBEAM
	};

	ZM_GameState xState;

	// A WIN grants exactly what the row names.
	SeedFernfawnStarterFixture(xState);
	ZENITH_ASSERT_FALSE(xState.HasBadge((u_int)ZM_BADGE_KILN),
		"the starter state already has the Kiln badge -- the transition is vacuous");
	ZENITH_ASSERT_FALSE(xState.m_xBag.Has(ZM_ITEM_TM_TITANBEAM),
		"the starter bag already carries TM Titan Beam -- the transition is vacuous");
	{
		const ZM_TrainerRewardResult xResult = ZM_ApplyTrainerResultToGameState(
			xState, xFixtureRow, ZM_SIDE_PLAYER, false);
		ZENITH_ASSERT_TRUE(xResult.m_bApplied, "a by-row PLAYER win must apply");
		ZENITH_ASSERT_TRUE(xResult.m_bBadgeNewlyAwarded, "the row's badge must be newly awarded");
		ZENITH_ASSERT_TRUE(xResult.m_bItemGranted, "the row's item must be granted");
		ZENITH_ASSERT_TRUE(xState.HasBadge((u_int)ZM_BADGE_KILN),
			"the by-row primitive must award the badge THE ROW NAMES, not a hardcoded one");
		ZENITH_ASSERT_FALSE(xState.HasBadge((u_int)ZM_BADGE_BLOOM),
			"a fixture row unrelated to Fenna must never award HER badge");
		ZENITH_ASSERT_TRUE(xState.m_xBag.Has(ZM_ITEM_TM_TITANBEAM),
			"the by-row primitive must grant the item THE ROW NAMES, not a hardcoded one");
		ZENITH_ASSERT_FALSE(xState.m_xBag.Has(ZM_ITEM_TM_VERDANTLASH),
			"a fixture row unrelated to Fenna must never grant HER item -- proves this reads "
			"the item off the row rather than off the TM Fenna happens to award");
	}

	// Fail-closed still holds through the by-row seam: a non-win outcome grants
	// nothing, exactly as section H already pins for money+flag via the by-id path.
	SeedFernfawnStarterFixture(xState);
	{
		const ZM_TrainerRewardResult xLossResult = ZM_ApplyTrainerResultToGameState(
			xState, xFixtureRow, ZM_SIDE_ENEMY, true);
		ZENITH_ASSERT_FALSE(xLossResult.m_bApplied, "a loss through the by-row seam must not apply");
		ZENITH_ASSERT_FALSE(xLossResult.m_bBadgeNewlyAwarded, "a loss must not report a badge award");
		ZENITH_ASSERT_FALSE(xLossResult.m_bItemGranted, "a loss must not report an item grant");
		ZENITH_ASSERT_FALSE(xState.HasBadge((u_int)ZM_BADGE_KILN), "a loss must not award a badge");
		ZENITH_ASSERT_FALSE(xState.m_xBag.Has(ZM_ITEM_TM_TITANBEAM), "a loss must not grant an item");
	}
}

// DoD: "Awarding twice does not double-grant, and the badge count is right after a
// repeat win." AwardBadge ORs a single bit, so the badge is STRUCTURALLY idempotent
// -- no dedup logic exists or is needed. The item ALSO does not double-grant (ZM-70
// review correction: BLOCKING), but by a DIFFERENT mechanism than the badge: it is
// gated on ZM_Bag::Has before the Add, because the bag is ALREADY the record of
// "does the player own this" -- see ZM_TrainerRewardResult's header comment. The
// item does NOT follow the prize money's shape: money still pays again because
// ZM-D-135 forbids the GameState member that would record "already paid", and no
// such member is needed here, so ZM-D-135 never governs this column at all. Both
// halves are PROVEN here, not merely assumed.
ZENITH_TEST(ZM_Party, TrainerReward_SecondWinDoesNotDoubleAwardTheBadge)
{
	ZM_GameState xState;
	SeedFernfawnStarterFixture(xState);

	const ZM_TrainerRewardResult xFirst = ZM_ApplyTrainerResultToGameState(
		xState, ZM_TRAINER_GYM1_FENNA, ZM_SIDE_PLAYER, false);
	ZENITH_ASSERT_TRUE(xFirst.m_bBadgeNewlyAwarded, "the FIRST win must newly award the Bloom Badge");
	ZENITH_ASSERT_EQ(xState.GetBadgeCount(), 1u, "one badge after the first win");
	ZENITH_ASSERT_TRUE(xFirst.m_bItemGranted, "the FIRST win must grant Verdant Lash");
	ZENITH_ASSERT_EQ(xState.m_xBag.GetCount(ZM_ITEM_TM_VERDANTLASH), 1u, "one TM copy after the first win");

	const ZM_TrainerRewardResult xSecond = ZM_ApplyTrainerResultToGameState(
		xState, ZM_TRAINER_GYM1_FENNA, ZM_SIDE_PLAYER, false);

	ZENITH_ASSERT_TRUE(xSecond.m_bApplied, "a second win over the same trainer still applies");
	ZENITH_ASSERT_FALSE(xSecond.m_bBadgeNewlyAwarded,
		"the badge is idempotent -- AwardBadge ORs a bit, so the SECOND win must report no NEW badge");
	ZENITH_ASSERT_TRUE(xState.HasBadge((u_int)ZM_BADGE_BLOOM), "the badge is still held after the second win");
	ZENITH_ASSERT_EQ(xState.GetBadgeCount(), 1u,
		"the badge count must still be exactly 1 after a repeat win -- awarding twice must not double-grant");

	// The item: OBSERVED and asserted, not assumed. It DEDUPLICATES -- unlike the
	// prize money -- because m_xBag.Has() already answers "does the player own
	// this" with zero new state, so nothing needs a ZM-D-135-forbidden member to
	// gate it. A second copy would exist purely to be sold: Verdant Lash (like
	// every TM) is m_bConsumable == false, so ZM_ShopSell converting the duplicate
	// to its m_uSellPrice is its ONLY use -- a farmable prize the per-trainer
	// defeat flag exists to forbid.
	ZENITH_ASSERT_FALSE(xSecond.m_bItemGranted, "the second win must NOT grant a second copy of the item");
	ZENITH_ASSERT_TRUE(xState.m_xBag.Has(ZM_ITEM_TM_VERDANTLASH), "the first copy must still be held");
	ZENITH_ASSERT_EQ(xState.m_xBag.GetCount(ZM_ITEM_TM_VERDANTLASH), 1u,
		"two wins must NOT stack two TM copies -- the bag itself is the dedupe record");
}

// DoD: "Both survive a save/load ROUND TRIP, proven by reading them back off a
// reloaded save rather than off the mutated object." Every assertion below reads
// xReloaded -- xState, the object the reward actually mutated, is never consulted
// again after the encode -- so a reward that never reached the wire cannot pass by
// accident. No new save module and no schema change: badges are module 5, the bag
// is module 6 (Source/Core/ZM_SaveSchema.cpp), both already wired before this ticket.
ZENITH_TEST(ZM_Party, TrainerReward_BadgeAndItemSurviveASaveLoadRoundTrip)
{
	ZM_GameState xState;
	SeedFernfawnStarterFixture(xState);

	const ZM_TrainerRewardResult xResult = ZM_ApplyTrainerResultToGameState(
		xState, ZM_TRAINER_GYM1_FENNA, ZM_SIDE_PLAYER, false);
	ZENITH_ASSERT_TRUE(xResult.m_bBadgeNewlyAwarded, "beating Fenna must newly award the Bloom Badge");
	ZENITH_ASSERT_TRUE(xResult.m_bItemGranted, "beating Fenna must grant Verdant Lash into the bag");

	Zenith_DataStream xWriteStream;
	const Zenith_Status xWriteStatus = ZM_SaveSchema::Write(xState, xWriteStream);
	ZENITH_ASSERT_TRUE(xWriteStatus.IsOk(), "encoding the post-reward state failed (error %u)",
		(u_int)xWriteStatus.Error());
	if (!xWriteStatus.IsOk()) { return; }

	// Snapshot the encoded bytes into an owned buffer -- ZM_SaveSchema::Read takes a
	// stream over a caller-owned byte range, mirroring Tests/ZM_Tests_SaveSchema.cpp's
	// own Encode/Decode idiom for the identical mechanism.
	const uint8_t* pEncoded = (const uint8_t*)xWriteStream.GetData();
	const std::vector<uint8_t> xBytes(pEncoded, pEncoded + xWriteStream.GetCursor());

	ZM_GameState xReloaded;
	Zenith_DataStream xReadStream((void*)xBytes.data(), xBytes.size());
	const Zenith_Status xReadStatus = ZM_SaveSchema::Read(xReadStream, xBytes.size(), xReloaded);
	ZENITH_ASSERT_TRUE(xReadStatus.IsOk(), "decoding the post-reward state failed (error %u)",
		(u_int)xReadStatus.Error());
	if (!xReadStatus.IsOk()) { return; }

	ZENITH_ASSERT_TRUE(xReloaded.HasBadge((u_int)ZM_BADGE_BLOOM),
		"the Bloom Badge did not survive a save/load round trip");
	ZENITH_ASSERT_EQ(xReloaded.GetBadgeCount(), 1u, "the reloaded badge count is wrong");
	ZENITH_ASSERT_TRUE(xReloaded.m_xBag.Has(ZM_ITEM_TM_VERDANTLASH),
		"Verdant Lash did not survive a save/load round trip");
	ZENITH_ASSERT_EQ(xReloaded.m_xBag.GetCount(ZM_ITEM_TM_VERDANTLASH), 1u,
		"the reloaded Verdant Lash count is wrong");
}
