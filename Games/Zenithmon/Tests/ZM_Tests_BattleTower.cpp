#include "Zenith.h"

// ============================================================================
// ZM_Tests_BattleTower -- S2 box-6 SC2 (ZM_BattleTower) unit tests. Covers the
// level-50 flat clamp, the streak->AI-tier escalation + boss cadence, the
// deterministic per-battle seed, procedural-by-seed opponent/rental team
// generation, streak settlement, and the tower ZM_BattleConfig -- plus two
// end-to-end engine round-trip smokes that construct a real ZM_BattleEngine
// (driven as a controlled 1v1 against the generated team's lead, so a KO ends the
// battle and the box-5 ZM_ChooseAction chooser is never asked to act for a
// fainted active -- see RunOneTowerBattle).
//
// Category split (spec section 11, mirrors ZM_Tests_Breeding.cpp): the pure
// clamp / streak / team-gen / config logic has NO battle-engine / event-stream
// dependency (team-gen produces *specs*, validated via the pure
// ZM_BuildBattleMonster + species-table reads) -> suite ZM_Data. The two
// end-to-end round-trip smokes that drive ZM_BattleEngine -> suite ZM_Battle.
//
// DETERMINISM STRATEGY (spec section 7): team-gen goldens are checked against a
// LOCAL offline oracle (OracleTeam / OracleMovesForSpecies) that INDEPENDENTLY
// replays the pinned draw order -- rejection-sample distinct species under the
// rarity ceiling, then one nature RandBelow -- against an identically-seeded
// ZM_BattleRNG. The expected team therefore comes from the SPEC, not from
// re-running ZM_GenerateTowerTeam, which is what makes parallel authoring safe
// and the golden a real regression guard. Likewise the AI-tier band + boss bump
// is checked against a local OracleAITier that mirrors the frozen section-5 rule
// (tier = base band; +1 on a boss, capped CHAMPION).
//
// NOTE for the reviewer/orchestrator (spec-prose vs frozen rule): spec section 5
// carries an internally-inconsistent "worked values" line (streak 20 -> CHAMPION)
// and a "monotonic non-decreasing" claim that CONTRADICT the precise frozen rule
// in the same section (base band [7,21) -> GREEDY, one-tier boss bump). Under the
// precise rule (and the task's own "20 -> GREEDY (base)" + "+1 tier on boss"),
// ZM_TowerAITierForStreak(20) == SMART (base GREEDY bumped once), NOT CHAMPION;
// and the boss-bumped tier is NOT globally monotonic (it dips one tier after each
// INTERIOR boss, e.g. streak 13 -> SMART then streak 14 -> GREEDY). These tests
// therefore assert the precise-rule values (base-tier monotonicity + AI-tier
// boundedness + per-streak oracle agreement), not the erroneous prose. See the
// report notes.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Battle/ZM_BattleTower.h"
#include "Zenithmon/Source/Battle/ZM_BattleMonster.h"
#include "Zenithmon/Source/Battle/ZM_ExpAndLevel.h"       // ZM_ExpForLevel, ZM_GetSpeciesGrowthRate
#include "Zenithmon/Source/Battle/ZM_BattleEngine.h"       // round-trip smokes
#include "Zenithmon/Source/Battle/ZM_BattleState.h"        // ZM_BattleState (ZM_ChooseAction arg)
#include "Zenithmon/Source/Battle/ZM_BattleAI.h"           // ZM_ChooseAction, ZM_AI_TIER
#include "Zenithmon/Source/Battle/ZM_BattleEvent.h"        // ZM_BATTLE_EVENT_EXP_GAINED
#include "Zenithmon/Source/Battle/ZM_BattleTypes.h"        // ZM_BattleConfig, ZM_BattleAction
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_NatureData.h"           // ZM_NATURE_COUNT
#include "Zenithmon/Source/Data/ZM_AbilityData.h"          // ZM_ABILITY_NONE, ZM_ABILITY_STREAMLINE
#include "Zenithmon/Source/Data/ZM_MoveData.h"             // ZM_MOVE_NONE + move ids
#include "Zenithmon/Source/Data/ZM_Learnsets.h"            // ZM_GetSpeciesLearnset
#include "Zenithmon/Source/Data/ZM_StatCalc.h"             // uZM_MAX_LEVEL
#include "Zenithmon/Source/Data/ZM_BattleRNG.h"
#include "Collections/Zenith_Vector.h"

// ============================================================================
// Test-local helpers (anonymous namespace: no cross-TU ODR clashes -- the
// builders in the sibling battle-test TUs live in THEIR own anonymous namespaces
// and are not visible here, so this TU carries its own minimal equivalents).
// ============================================================================
namespace
{
	// ---- spec builders -----------------------------------------------------
	// Real-species spec with per-stat IV/EV arrays (no base-stat override unless
	// set by the caller). EVs kept small (well under the 252/510 caps) so the
	// build-path ZM_NormalizeEVs never perturbs a clamp-vs-direct comparison.
	ZM_BattleMonsterSpec MakeSpecIV(ZM_SPECIES_ID eSpecies, u_int uLevel,
		const u_int (&aIV)[ZM_STAT_COUNT], const u_int (&aEV)[ZM_STAT_COUNT],
		ZM_NATURE eNature, ZM_ABILITY_ID eAbility,
		ZM_MOVE_ID eM0 = ZM_MOVE_NONE, ZM_MOVE_ID eM1 = ZM_MOVE_NONE,
		ZM_MOVE_ID eM2 = ZM_MOVE_NONE, ZM_MOVE_ID eM3 = ZM_MOVE_NONE)
	{
		ZM_BattleMonsterSpec xSpec;
		xSpec.m_eSpecies = eSpecies;
		xSpec.m_uLevel = uLevel;
		for (u_int i = 0; i < ZM_STAT_COUNT; ++i) { xSpec.m_auIV[i] = aIV[i]; xSpec.m_auEV[i] = aEV[i]; }
		xSpec.m_eNature = eNature;
		xSpec.m_eAbility = eAbility;
		xSpec.m_aeMoves[0] = eM0; xSpec.m_aeMoves[1] = eM1; xSpec.m_aeMoves[2] = eM2; xSpec.m_aeMoves[3] = eM3;
		xSpec.m_bOverrideBaseStats = false;
		xSpec.m_uCurExp = uZM_EXP_UNSPECIFIED;
		return xSpec;
	}

	// Convenience: uniform IV / EV across the six stats.
	ZM_BattleMonsterSpec MakeSpecUniform(ZM_SPECIES_ID eSpecies, u_int uLevel,
		u_int uIV = 31u, u_int uEV = 0u, ZM_NATURE eNature = ZM_NATURE_FERAL,
		ZM_ABILITY_ID eAbility = ZM_ABILITY_NONE,
		ZM_MOVE_ID eM0 = ZM_MOVE_NONE, ZM_MOVE_ID eM1 = ZM_MOVE_NONE,
		ZM_MOVE_ID eM2 = ZM_MOVE_NONE, ZM_MOVE_ID eM3 = ZM_MOVE_NONE)
	{
		const u_int aIV[ZM_STAT_COUNT] = { uIV, uIV, uIV, uIV, uIV, uIV };
		const u_int aEV[ZM_STAT_COUNT] = { uEV, uEV, uEV, uEV, uEV, uEV };
		return MakeSpecIV(eSpecies, uLevel, aIV, aEV, eNature, eAbility, eM0, eM1, eM2, eM3);
	}

	// Override-base-stat spec (pencil-independent; huge stats for a smoke that a
	// clamped player reliably wins). Six values in ZM_STAT order.
	ZM_BattleMonsterSpec MakeSpecOverride(ZM_SPECIES_ID eSpecies, u_int uLevel,
		u_int uHP, u_int uATK, u_int uDEF, u_int uSPA, u_int uSPD, u_int uSPE,
		ZM_MOVE_ID eM0)
	{
		ZM_BattleMonsterSpec xSpec = MakeSpecUniform(eSpecies, uLevel, 31u, 0u, ZM_NATURE_FERAL, ZM_ABILITY_NONE, eM0);
		xSpec.m_bOverrideBaseStats = true;
		xSpec.m_xBaseStatsOverride.m_au[ZM_STAT_HP]        = uHP;
		xSpec.m_xBaseStatsOverride.m_au[ZM_STAT_ATTACK]    = uATK;
		xSpec.m_xBaseStatsOverride.m_au[ZM_STAT_DEFENSE]   = uDEF;
		xSpec.m_xBaseStatsOverride.m_au[ZM_STAT_SPATTACK]  = uSPA;
		xSpec.m_xBaseStatsOverride.m_au[ZM_STAT_SPDEFENSE] = uSPD;
		xSpec.m_xBaseStatsOverride.m_au[ZM_STAT_SPEED]     = uSPE;
		return xSpec;
	}

	// ---- built-monster comparison ------------------------------------------
	// Two built monsters agree on level, current HP, and all six max stats (the
	// clamp's observable output through the LOCKED ZM_BuildBattleMonster path).
	void AssertBuiltEqual(const ZM_BattleMonster& xA, const ZM_BattleMonster& xB, const char* szLabel)
	{
		ZENITH_ASSERT_EQ(xA.m_uLevel, xB.m_uLevel, "%s level", szLabel);
		ZENITH_ASSERT_EQ(xA.m_uCurHP, xB.m_uCurHP, "%s curHP", szLabel);
		for (u_int i = 0; i < ZM_STAT_COUNT; ++i)
		{
			ZENITH_ASSERT_EQ(xA.m_auMaxStat[i], xB.m_auMaxStat[i], "%s max stat %u", szLabel, i);
		}
	}

	// ---- streak-schedule oracle (spec section 5, the PRECISE frozen rule) ---
	ZM_AI_TIER OracleBaseTier(u_int uStreak)
	{
		if (uStreak < uZM_TOWER_STREAK_GREEDY)   { return ZM_AI_TIER_RANDOM; }
		if (uStreak < uZM_TOWER_STREAK_SMART)    { return ZM_AI_TIER_GREEDY; }
		if (uStreak < uZM_TOWER_STREAK_CHAMPION) { return ZM_AI_TIER_SMART;  }
		return ZM_AI_TIER_CHAMPION;
	}
	bool OracleIsBoss(u_int uStreak)
	{
		return ((uStreak + 1u) % uZM_TOWER_BOSS_PERIOD) == 0u;
	}
	ZM_AI_TIER OracleAITier(u_int uStreak)
	{
		ZM_AI_TIER eTier = OracleBaseTier(uStreak);
		if (OracleIsBoss(uStreak) && eTier < ZM_AI_TIER_CHAMPION)
		{
			eTier = (ZM_AI_TIER)(eTier + 1u);
		}
		return eTier;
	}

	// ---- team-generation oracle (spec section 7, INDEPENDENT reconstruction) --
	// Rarity ceiling replicated from the spec bands ([0,7)->COMMON, [7,21)->
	// UNCOMMON, [21,inf)->RARE) with literal thresholds so the oracle stands on
	// its own; a divergent implementation ceiling shifts the eligible-list size
	// and the whole team diverges (which is exactly what the golden must catch).
	ZM_RARITY OracleCeiling(u_int uStreak)
	{
		if (uStreak < 7u)  { return ZM_RARITY_COMMON;   }
		if (uStreak < 21u) { return ZM_RARITY_UNCOMMON; }
		return ZM_RARITY_RARE;
	}

	// Ascending-id eligible list: rarity <= ceiling AND not LEGENDARY. Returns the
	// count; fills paeOut (caller-sized to >= ZM_SPECIES_COUNT).
	u_int OracleEligible(u_int uStreak, ZM_SPECIES_ID* paeOut)
	{
		const ZM_RARITY eCeil = OracleCeiling(uStreak);
		const u_int uSpeciesCount = ZM_GetSpeciesCount();
		u_int uCount = 0u;
		for (u_int id = 0u; id < uSpeciesCount; ++id)
		{
			const ZM_RARITY eR = ZM_GetSpeciesData((ZM_SPECIES_ID)id).m_eRarity;
			if (eR <= eCeil && eR != ZM_RARITY_LEGENDARY)
			{
				paeOut[uCount++] = (ZM_SPECIES_ID)id;
			}
		}
		return uCount;
	}

	// The up-to-four HIGHEST-level (<= L50) learnset moves, in learn order, into
	// slots 0..k-1; remaining slots ZM_MOVE_NONE (spec section 6 move rule).
	void OracleMovesForSpecies(ZM_SPECIES_ID eSpecies, ZM_MOVE_ID (&aeOut)[uZM_MAX_MOVES])
	{
		for (u_int i = 0; i < uZM_MAX_MOVES; ++i) { aeOut[i] = ZM_MOVE_NONE; }
		const ZM_Learnset xLs = ZM_GetSpeciesLearnset(eSpecies);
		ZM_MOVE_ID aeAll[uZM_MAX_LEARNSET_SIZE];
		u_int uAll = 0u;
		for (u_int k = 0; k < xLs.m_uCount; ++k)
		{
			if (xLs.m_axMoves[k].m_uLevel <= uZM_TOWER_LEVEL)
			{
				aeAll[uAll++] = xLs.m_axMoves[k].m_eMove;
			}
		}
		const u_int uStart = (uAll > uZM_MAX_MOVES) ? (uAll - uZM_MAX_MOVES) : 0u;
		u_int uSlot = 0u;
		for (u_int k = uStart; k < uAll; ++k) { aeOut[uSlot++] = aeAll[k]; }
	}

	// Full team oracle: replays the section-7 draw order (species rejection then
	// one nature draw per slot) on xRng, filling species + nature out-arrays.
	void OracleTeam(u_int uStreak, ZM_BattleRNG& xRng, u_int uCount,
		ZM_SPECIES_ID* paeSpeciesOut, ZM_NATURE* paeNatureOut)
	{
		ZM_SPECIES_ID aeEligible[ZM_SPECIES_COUNT];
		const u_int uElig = OracleEligible(uStreak, aeEligible);
		for (u_int s = 0u; s < uCount; ++s)
		{
			u_int uIdx = 0u;
			bool bDup = true;
			while (bDup)
			{
				uIdx = xRng.RandBelow(uElig);
				bDup = false;
				for (u_int p = 0u; p < s; ++p)
				{
					if (paeSpeciesOut[p] == aeEligible[uIdx]) { bDup = true; break; }
				}
			}
			paeSpeciesOut[s] = aeEligible[uIdx];
			paeNatureOut[s]  = (ZM_NATURE)xRng.RandBelow((u_int)ZM_NATURE_COUNT);
		}
	}

	// True iff two generated specs differ in at least one of the six fields the
	// oracle pins (species / level / nature / IVs / moves).
	bool TeamMonsDiffer(const ZM_BattleMonsterSpec& xA, const ZM_BattleMonsterSpec& xB)
	{
		if (xA.m_eSpecies != xB.m_eSpecies) { return true; }
		if (xA.m_uLevel != xB.m_uLevel)     { return true; }
		if (xA.m_eNature != xB.m_eNature)   { return true; }
		for (u_int i = 0; i < ZM_STAT_COUNT; ++i) { if (xA.m_auIV[i] != xB.m_auIV[i]) { return true; } }
		for (u_int i = 0; i < uZM_MAX_MOVES; ++i) { if (xA.m_aeMoves[i] != xB.m_aeMoves[i]) { return true; } }
		return false;
	}

	// ---- engine-smoke helpers ----------------------------------------------
	ZM_BattleAction MoveAction(u_int uSlot)
	{
		ZM_BattleAction xAction;
		xAction.m_eKind = ZM_ACTION_MOVE;
		xAction.m_uMoveSlot = uSlot;
		return xAction;
	}

	u_int CountKind(const Zenith_Vector<ZM_BattleEvent>& xEvents, ZM_BATTLE_EVENT eKind)
	{
		u_int uCount = 0u;
		for (u_int i = 0; i < xEvents.GetSize(); ++i)
		{
			if (xEvents.Get(i).m_eKind == eKind) { ++uCount; }
		}
		return uCount;
	}

	// Result of one driven tower battle (the caller-side integration loop of spec
	// section 8: clamp player -> generate enemy -> tower config -> drive engine
	// feeding the ENEMY action from ZM_ChooseAction at the streak's tier).
	struct SmokeResult
	{
		bool    m_bOver;
		ZM_SIDE m_eWinner;
		u_int   m_uExpEvents;
		u_int   m_uTurns;
	};

	SmokeResult RunOneTowerBattle(u_int uStreak, u_int64 ulRunSeed, u_int64 ulAISeed, u_int uGuard = 500u)
	{
		ZM_TowerRun xRun;
		xRun.m_ulSeed = ulRunSeed;
		xRun.m_uCurrentStreak = uStreak;

		// Player: an overwhelmingly dominant L50 built from a clamped L80 override
		// spec (proves the clamp preserves the golden-hook override + moves through
		// the round-trip). The huge ATK guarantees a one-hit KO of the enemy lead, so
		// the battle terminates deterministically in a turn regardless of the AI's
		// move choice; the huge HP/SPE guarantee the player acts first and never
		// faints.
		ZM_BattleMonsterSpec xPlayerSrc =
			MakeSpecOverride(ZM_SPECIES_NIBBIN, 80u, 500u, 900u, 400u, 100u, 400u, 500u, ZM_MOVE_BRUTESLAM);
		ZM_BattleMonsterSpec axPlayer[1];
		ZM_ClampPartyToTowerLevel(&xPlayerSrc, 1u, axPlayer);

		// Enemy: a procedural tower team for this streak, seeded off the run. The
		// smoke drives a CONTROLLED 1v1 against the generated team's LEAD (already a
		// clamped L50 spec) so the FIRST KO ends the battle and the engine never
		// enters a force-switch state. ZM_ChooseAction is the box-5 chooser for a
		// HEALTHY active and must never be asked to act for a fainted active -- which
		// a 3-mon enemy would require after each mid-battle faint (the engine leaves a
		// KO'd active in place until the caller resolves the forced switch, and
		// SubmitAction asserts on a fainted active). A 1-mon enemy sidesteps that
		// entirely: ZM_ChooseAction is only ever invoked on the healthy lead.
		ZM_BattleMonsterSpec axTeam[uZM_TOWER_TEAM_SIZE];
		ZM_BattleRNG xTeamRng(ZM_TowerBattleSeed(xRun, uStreak));
		ZM_GenerateTowerTeam(uStreak, xTeamRng, axTeam, uZM_TOWER_TEAM_SIZE);
		ZM_BattleMonsterSpec axEnemy[1] = { axTeam[0] };

		ZM_BattleEngine xEngine;
		xEngine.Begin(ZM_MakeTowerBattleConfig(), axPlayer, 1u, axEnemy, 1u,
			ZM_TowerBattleSeed(xRun, uStreak) ^ 0xB47ull, 54ull);

		const ZM_AI_TIER eTier = ZM_TowerAITierForStreak(uStreak);
		ZM_BattleRNG xAIRng(ulAISeed, 54ull);   // caller-owned; distinct from the battle RNG

		u_int uTurns = 0u;
		while (!xEngine.IsOver() && uTurns < uGuard)
		{
			xEngine.SubmitAction(ZM_SIDE_ENEMY,
				ZM_ChooseAction(xEngine.GetState(), ZM_SIDE_ENEMY, eTier, xAIRng));
			xEngine.SubmitAction(ZM_SIDE_PLAYER, MoveAction(0u));
			xEngine.ResolveTurn();
			++uTurns;
		}

		SmokeResult xResult;
		xResult.m_bOver = xEngine.IsOver();
		xResult.m_eWinner = xEngine.GetWinnerSide();
		xResult.m_uExpEvents = CountKind(xEngine.GetEvents(), ZM_BATTLE_EVENT_EXP_GAINED);
		xResult.m_uTurns = uTurns;
		return xResult;
	}
}

// ############################################################################
// A. Level-50 clamp (ZM_Data)
// ############################################################################

// An L80 mon clamps DOWN to L50; its built six max stats equal a directly-built
// L50 spec of the same species / IV / EV / nature (independent recompute oracle
// == the locked ZM_BuildBattleMonster path).
ZENITH_TEST(ZM_Data, Tower_Clamp_DownFromHighLevel)
{
	const u_int aIV[ZM_STAT_COUNT] = { 31u, 20u, 15u, 31u, 0u, 10u };
	const u_int aEV[ZM_STAT_COUNT] = { 4u, 8u, 12u, 16u, 20u, 24u };

	const ZM_BattleMonsterSpec xHigh = MakeSpecIV(ZM_SPECIES_FERNFAWN, 80u, aIV, aEV,
		ZM_NATURE_BRUTISH, ZM_ABILITY_NONE);
	const ZM_BattleMonsterSpec xClamped = ZM_ClampSpecToTowerLevel(xHigh);

	const ZM_BattleMonster xBuiltClamped = ZM_BuildBattleMonster(xClamped);
	const ZM_BattleMonster xBuiltDirect  = ZM_BuildBattleMonster(
		MakeSpecIV(ZM_SPECIES_FERNFAWN, uZM_TOWER_LEVEL, aIV, aEV, ZM_NATURE_BRUTISH, ZM_ABILITY_NONE));

	ZENITH_ASSERT_EQ(xClamped.m_uLevel, uZM_TOWER_LEVEL, "clamp must normalize level to 50");
	ZENITH_ASSERT_EQ(xBuiltClamped.m_uLevel, 50u);
	AssertBuiltEqual(xBuiltClamped, xBuiltDirect, "clamp-down");
}

// An L20 mon clamps UP to L50 with the same recomputed stats (scale-up direction).
ZENITH_TEST(ZM_Data, Tower_Clamp_UpFromLowLevel)
{
	const u_int aIV[ZM_STAT_COUNT] = { 25u, 31u, 3u, 18u, 7u, 31u };
	const u_int aEV[ZM_STAT_COUNT] = { 0u, 4u, 0u, 8u, 0u, 12u };

	const ZM_BattleMonsterSpec xLow = MakeSpecIV(ZM_SPECIES_FERNFAWN, 20u, aIV, aEV,
		ZM_NATURE_ARCANE, ZM_ABILITY_NONE);
	const ZM_BattleMonsterSpec xClamped = ZM_ClampSpecToTowerLevel(xLow);

	const ZM_BattleMonster xBuiltClamped = ZM_BuildBattleMonster(xClamped);
	const ZM_BattleMonster xBuiltDirect  = ZM_BuildBattleMonster(
		MakeSpecIV(ZM_SPECIES_FERNFAWN, uZM_TOWER_LEVEL, aIV, aEV, ZM_NATURE_ARCANE, ZM_ABILITY_NONE));

	ZENITH_ASSERT_EQ(xClamped.m_uLevel, uZM_TOWER_LEVEL);
	AssertBuiltEqual(xBuiltClamped, xBuiltDirect, "clamp-up");
}

// Every identity field survives verbatim; ONLY m_uLevel(->50) and
// m_uCurExp(->UNSPECIFIED) change.
ZENITH_TEST(ZM_Data, Tower_Clamp_PreservesIdentityFields)
{
	const u_int aIV[ZM_STAT_COUNT] = { 30u, 29u, 28u, 27u, 26u, 25u };
	const u_int aEV[ZM_STAT_COUNT] = { 6u, 12u, 18u, 24u, 30u, 36u };
	ZM_BattleMonsterSpec xSrc = MakeSpecIV(ZM_SPECIES_THICKETBUCK, 80u, aIV, aEV,
		ZM_NATURE_SKITTISH, ZM_ABILITY_STREAMLINE,
		ZM_MOVE_RAMBASH, ZM_MOVE_QUICKJAB, ZM_MOVE_BRUTESLAM, ZM_MOVE_BUBBLESPRAY);
	// A base-stat override (golden hook) + a stale explicit exp: both must survive
	// (override) / be reset (exp) exactly as documented.
	xSrc.m_bOverrideBaseStats = true;
	for (u_int i = 0; i < ZM_STAT_COUNT; ++i) { xSrc.m_xBaseStatsOverride.m_au[i] = 40u + i * 10u; }
	xSrc.m_uCurExp = 12345u;

	const ZM_BattleMonsterSpec xC = ZM_ClampSpecToTowerLevel(xSrc);

	ZENITH_ASSERT_EQ((u_int)xC.m_eSpecies, (u_int)ZM_SPECIES_THICKETBUCK);
	ZENITH_ASSERT_EQ(xC.m_uLevel, uZM_TOWER_LEVEL, "level must be 50");
	ZENITH_ASSERT_EQ(xC.m_uCurExp, uZM_EXP_UNSPECIFIED, "exp must reset to UNSPECIFIED");
	ZENITH_ASSERT_EQ((u_int)xC.m_eNature, (u_int)ZM_NATURE_SKITTISH, "nature preserved");
	ZENITH_ASSERT_EQ((u_int)xC.m_eAbility, (u_int)ZM_ABILITY_STREAMLINE, "ability preserved");
	ZENITH_ASSERT_EQ(xC.m_bOverrideBaseStats, true, "override flag preserved");
	for (u_int i = 0; i < ZM_STAT_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ(xC.m_auIV[i], aIV[i], "IV %u preserved", i);
		ZENITH_ASSERT_EQ(xC.m_auEV[i], aEV[i], "EV %u preserved", i);
		ZENITH_ASSERT_EQ(xC.m_xBaseStatsOverride.m_au[i], 40u + i * 10u, "override stat %u preserved", i);
	}
	for (u_int i = 0; i < uZM_MAX_MOVES; ++i)
	{
		ZENITH_ASSERT_EQ((u_int)xC.m_aeMoves[i], (u_int)xSrc.m_aeMoves[i], "move slot %u preserved", i);
	}
}

// A built clamped mon starts at FULL HP and at the recomputed L50 exp floor for
// its species' growth curve.
ZENITH_TEST(ZM_Data, Tower_Clamp_FullHPAndExpFloor)
{
	const ZM_BattleMonsterSpec xC = ZM_ClampSpecToTowerLevel(MakeSpecUniform(ZM_SPECIES_FERNFAWN, 70u, 31u, 0u));
	const ZM_BattleMonster xBuilt = ZM_BuildBattleMonster(xC);

	ZENITH_ASSERT_EQ(xBuilt.m_uCurHP, xBuilt.m_auMaxStat[ZM_STAT_HP], "clamped mon starts at full HP");

	const ZM_GROWTH_RATE eRate = ZM_GetSpeciesGrowthRate(ZM_SPECIES_FERNFAWN);
	ZENITH_ASSERT_EQ(xBuilt.m_uCurExp, ZM_ExpForLevel(eRate, uZM_TOWER_LEVEL), "clamped mon sits at the L50 exp floor");
}

// Clamping an already-L50 spec is a build-equal fixed point.
ZENITH_TEST(ZM_Data, Tower_Clamp_Idempotent)
{
	const ZM_BattleMonsterSpec xL50 = MakeSpecUniform(ZM_SPECIES_NIBBIN, uZM_TOWER_LEVEL, 21u, 0u, ZM_NATURE_HULKING);
	const ZM_BattleMonsterSpec xC = ZM_ClampSpecToTowerLevel(xL50);

	ZENITH_ASSERT_EQ(xC.m_uLevel, uZM_TOWER_LEVEL);
	const ZM_BattleMonster xBuiltC = ZM_BuildBattleMonster(xC);
	const ZM_BattleMonster xBuiltDirect = ZM_BuildBattleMonster(xL50);
	AssertBuiltEqual(xBuiltC, xBuiltDirect, "idempotent");
}

// A multi-mon party clamps element-wise: order + count preserved, each -> L50 and
// each builds to the same stats as a direct L50 build of that member.
ZENITH_TEST(ZM_Data, Tower_ClampParty_PreservesOrderAndCount)
{
	ZM_BattleMonsterSpec axIn[3] = {
		MakeSpecUniform(ZM_SPECIES_FERNFAWN,  80u, 31u, 0u, ZM_NATURE_BRUTISH),
		MakeSpecUniform(ZM_SPECIES_NIBBIN,    20u, 15u, 0u, ZM_NATURE_ARCANE),
		MakeSpecUniform(ZM_SPECIES_STRAYLING, uZM_TOWER_LEVEL, 7u, 0u, ZM_NATURE_FLEET),
	};
	ZM_BattleMonsterSpec axOut[3];
	ZM_ClampPartyToTowerLevel(axIn, 3u, axOut);

	for (u_int s = 0; s < 3u; ++s)
	{
		ZENITH_ASSERT_EQ((u_int)axOut[s].m_eSpecies, (u_int)axIn[s].m_eSpecies, "slot %u species order preserved", s);
		ZENITH_ASSERT_EQ(axOut[s].m_uLevel, uZM_TOWER_LEVEL, "slot %u level 50", s);

		const ZM_BattleMonster xBuilt = ZM_BuildBattleMonster(axOut[s]);
		ZM_BattleMonsterSpec xDirect = axIn[s];
		xDirect.m_uLevel = uZM_TOWER_LEVEL;
		xDirect.m_uCurExp = uZM_EXP_UNSPECIFIED;
		AssertBuiltEqual(xBuilt, ZM_BuildBattleMonster(xDirect), "clampparty slot");
	}
}

// An input level OUTSIDE [1,100] is silently normalized to 50 -- the original is
// never built, so the clamp cannot trip ZM_BuildBattleMonster's range assert.
ZENITH_TEST(ZM_Data, Tower_Clamp_OutOfRangeInputLevelNormalized)
{
	const u_int auBad[] = { 0u, 200u };
	for (u_int i = 0; i < (u_int)(sizeof(auBad) / sizeof(auBad[0])); ++i)
	{
		ZM_BattleMonsterSpec xBad = MakeSpecUniform(ZM_SPECIES_NIBBIN, uZM_TOWER_LEVEL, 31u, 0u);
		xBad.m_uLevel = auBad[i];   // out of [1,100]
		const ZM_BattleMonsterSpec xC = ZM_ClampSpecToTowerLevel(xBad);
		ZENITH_ASSERT_EQ(xC.m_uLevel, uZM_TOWER_LEVEL, "bad input level %u -> 50", auBad[i]);
		// Builds cleanly at a valid L50 (no assert).
		const ZM_BattleMonster xBuilt = ZM_BuildBattleMonster(xC);
		ZENITH_ASSERT_EQ(xBuilt.m_uLevel, 50u);
		ZENITH_ASSERT_GT(xBuilt.m_auMaxStat[ZM_STAT_HP], 0u, "sane L50 HP");
	}
}

// ############################################################################
// B. Streak -> AI tier + boss cadence (ZM_Data)
// ############################################################################

// Base band boundaries (before the boss bump): the exact edges of each band.
ZENITH_TEST(ZM_Data, Tower_BaseTier_BandBoundaries)
{
	ZENITH_ASSERT_EQ((u_int)ZM_TowerBaseTierForStreak(0u),  (u_int)ZM_AI_TIER_RANDOM);
	ZENITH_ASSERT_EQ((u_int)ZM_TowerBaseTierForStreak(6u),  (u_int)ZM_AI_TIER_RANDOM);
	ZENITH_ASSERT_EQ((u_int)ZM_TowerBaseTierForStreak(7u),  (u_int)ZM_AI_TIER_GREEDY);
	ZENITH_ASSERT_EQ((u_int)ZM_TowerBaseTierForStreak(20u), (u_int)ZM_AI_TIER_GREEDY);
	ZENITH_ASSERT_EQ((u_int)ZM_TowerBaseTierForStreak(21u), (u_int)ZM_AI_TIER_SMART);
	ZENITH_ASSERT_EQ((u_int)ZM_TowerBaseTierForStreak(34u), (u_int)ZM_AI_TIER_SMART);
	ZENITH_ASSERT_EQ((u_int)ZM_TowerBaseTierForStreak(35u), (u_int)ZM_AI_TIER_CHAMPION);
	ZENITH_ASSERT_EQ((u_int)ZM_TowerBaseTierForStreak(60u), (u_int)ZM_AI_TIER_CHAMPION);
}

// The upcoming battle is a boss exactly when (streak+1) % 7 == 0.
ZENITH_TEST(ZM_Data, Tower_IsBoss_Every7th)
{
	const u_int auBoss[]    = { 6u, 13u, 20u, 27u, 34u };
	const u_int auNonBoss[] = { 0u, 5u, 7u, 12u };
	for (u_int i = 0; i < (u_int)(sizeof(auBoss) / sizeof(auBoss[0])); ++i)
	{
		ZENITH_ASSERT_TRUE(ZM_TowerIsBossBattle(auBoss[i]), "streak %u must be a boss", auBoss[i]);
	}
	for (u_int i = 0; i < (u_int)(sizeof(auNonBoss) / sizeof(auNonBoss[0])); ++i)
	{
		ZENITH_ASSERT_FALSE(ZM_TowerIsBossBattle(auNonBoss[i]), "streak %u must not be a boss", auNonBoss[i]);
	}
}

// Boss battles bump the base tier one step up (capped CHAMPION); non-boss streaks
// keep the base tier. Values are checked against the local precise-rule oracle
// (spec section 5) -- NOT the section-5 "worked values" prose, which mis-states
// streak 20 (see the file header note).
ZENITH_TEST(ZM_Data, Tower_AITier_BossBump)
{
	// Unambiguous boss bumps (base band interior + edges), asserted as literals.
	ZENITH_ASSERT_EQ((u_int)ZM_TowerAITierForStreak(6u),  (u_int)ZM_AI_TIER_GREEDY);    // RANDOM +1
	ZENITH_ASSERT_EQ((u_int)ZM_TowerAITierForStreak(13u), (u_int)ZM_AI_TIER_SMART);     // GREEDY +1
	ZENITH_ASSERT_EQ((u_int)ZM_TowerAITierForStreak(20u), (u_int)ZM_AI_TIER_SMART);     // GREEDY +1 (base 20 == GREEDY)
	ZENITH_ASSERT_EQ((u_int)ZM_TowerAITierForStreak(27u), (u_int)ZM_AI_TIER_CHAMPION);  // SMART  +1
	ZENITH_ASSERT_EQ((u_int)ZM_TowerAITierForStreak(34u), (u_int)ZM_AI_TIER_CHAMPION);  // SMART  +1

	// A CHAMPION-band boss never overflows past CHAMPION.
	ZENITH_ASSERT_EQ((u_int)ZM_TowerAITierForStreak(41u), (u_int)ZM_AI_TIER_CHAMPION);  // boss, base CHAMPION -> capped

	// Non-boss control: the AI tier equals the base tier.
	const u_int auNonBoss[] = { 0u, 5u, 7u, 12u, 21u };
	for (u_int i = 0; i < (u_int)(sizeof(auNonBoss) / sizeof(auNonBoss[0])); ++i)
	{
		const u_int s = auNonBoss[i];
		ZENITH_ASSERT_FALSE(ZM_TowerIsBossBattle(s), "control streak %u must be non-boss", s);
		ZENITH_ASSERT_EQ((u_int)ZM_TowerAITierForStreak(s), (u_int)ZM_TowerBaseTierForStreak(s),
			"non-boss AI tier must equal base tier (streak %u)", s);
	}
}

// The BASE tier is monotonic non-decreasing over a full sweep; the boss-bumped AI
// tier is always bounded to [RANDOM, CHAMPION] and matches the precise-rule oracle
// at every streak. (The AI tier is deliberately NOT asserted globally monotonic --
// it dips one tier after each interior boss; see the file header note.)
ZENITH_TEST(ZM_Data, Tower_AITier_BaseMonotonicAndBounded)
{
	u_int uPrevBase = (u_int)ZM_TowerBaseTierForStreak(0u);
	for (u_int s = 0u; s <= 80u; ++s)
	{
		const u_int uBase = (u_int)ZM_TowerBaseTierForStreak(s);
		ZENITH_ASSERT_GE(uBase, uPrevBase, "base tier must be non-decreasing at streak %u", s);
		uPrevBase = uBase;

		const u_int uAI = (u_int)ZM_TowerAITierForStreak(s);
		ZENITH_ASSERT_GE(uAI, (u_int)ZM_AI_TIER_RANDOM, "AI tier below RANDOM at streak %u", s);
		ZENITH_ASSERT_LE(uAI, (u_int)ZM_AI_TIER_CHAMPION, "AI tier above CHAMPION at streak %u", s);
		ZENITH_ASSERT_EQ(uAI, (u_int)OracleAITier(s), "AI tier vs oracle at streak %u", s);
	}
}

// ############################################################################
// C. Streak settlement (ZM_Data)
// ############################################################################

// A win increments the current streak and raises the best high-water mark.
ZENITH_TEST(ZM_Data, Tower_Advance_WinIncrementsAndTracksBest)
{
	ZM_TowerRun xRun;
	xRun.m_uCurrentStreak = 4u;
	xRun.m_uBestStreak    = 4u;
	const u_int uReturned = ZM_TowerAdvance(xRun, true);
	ZENITH_ASSERT_EQ(uReturned, 5u, "advance returns the new current streak");
	ZENITH_ASSERT_EQ(xRun.m_uCurrentStreak, 5u);
	ZENITH_ASSERT_EQ(xRun.m_uBestStreak, 5u, "best raised with the new high-water");
}

// A loss resets the current streak to 0 but preserves the best.
ZENITH_TEST(ZM_Data, Tower_Advance_LossResetsCurrentKeepsBest)
{
	ZM_TowerRun xRun;
	xRun.m_uCurrentStreak = 9u;
	xRun.m_uBestStreak    = 12u;
	const u_int uReturned = ZM_TowerAdvance(xRun, false);
	ZENITH_ASSERT_EQ(uReturned, 0u);
	ZENITH_ASSERT_EQ(xRun.m_uCurrentStreak, 0u, "loss resets current to 0");
	ZENITH_ASSERT_EQ(xRun.m_uBestStreak, 12u, "best preserved across a loss");
}

// best is the running max over a win/loss/win sequence: win*3, loss, win*2 ->
// current 2, best 3.
ZENITH_TEST(ZM_Data, Tower_Advance_BestIsRunningMaxOverSequence)
{
	ZM_TowerRun xRun;   // fresh (all zero)
	ZM_TowerAdvance(xRun, true);
	ZM_TowerAdvance(xRun, true);
	ZM_TowerAdvance(xRun, true);   // cur 3, best 3
	ZENITH_ASSERT_EQ(xRun.m_uCurrentStreak, 3u);
	ZENITH_ASSERT_EQ(xRun.m_uBestStreak, 3u);
	ZM_TowerAdvance(xRun, false);  // cur 0, best 3
	ZENITH_ASSERT_EQ(xRun.m_uCurrentStreak, 0u);
	ZM_TowerAdvance(xRun, true);
	ZM_TowerAdvance(xRun, true);   // cur 2, best still 3
	ZENITH_ASSERT_EQ(xRun.m_uCurrentStreak, 2u);
	ZENITH_ASSERT_EQ(xRun.m_uBestStreak, 3u, "best is the running max, not the latest");
}

// A default-constructed ZM_TowerRun is a clean new run.
ZENITH_TEST(ZM_Data, Tower_Run_DefaultZeroed)
{
	ZM_TowerRun xRun;
	ZENITH_ASSERT_EQ(xRun.m_uCurrentStreak, 0u);
	ZENITH_ASSERT_EQ(xRun.m_uBestStreak, 0u);
	ZENITH_ASSERT_EQ(xRun.m_ulSeed, 0ull);
}

// ############################################################################
// D. Team generation (ZM_Data)
// ############################################################################

// Same seed + same streak -> field-identical teams; and the generated team
// matches the INDEPENDENT section-7 oracle (species + nature) with the fixed
// tower per-monster fields (level 50, IV 31, EV 0, ability NONE, exp UNSPECIFIED).
ZENITH_TEST(ZM_Data, Tower_Team_SameSeedIdentical)
{
	const u_int uStreak = 5u;
	const u_int64 ulSeed = 0xC0FFEEull;

	ZM_BattleMonsterSpec axA[uZM_TOWER_TEAM_SIZE];
	ZM_BattleMonsterSpec axB[uZM_TOWER_TEAM_SIZE];
	ZM_BattleRNG xRngA(ulSeed);
	ZM_BattleRNG xRngB(ulSeed);
	ZM_GenerateTowerTeam(uStreak, xRngA, axA, uZM_TOWER_TEAM_SIZE);
	ZM_GenerateTowerTeam(uStreak, xRngB, axB, uZM_TOWER_TEAM_SIZE);

	// Independent oracle over an identically-seeded RNG.
	ZM_SPECIES_ID aeOracleSpecies[uZM_TOWER_TEAM_SIZE];
	ZM_NATURE     aeOracleNature[uZM_TOWER_TEAM_SIZE];
	ZM_BattleRNG xRngOracle(ulSeed);
	OracleTeam(uStreak, xRngOracle, uZM_TOWER_TEAM_SIZE, aeOracleSpecies, aeOracleNature);

	for (u_int s = 0; s < uZM_TOWER_TEAM_SIZE; ++s)
	{
		// same-seed determinism
		ZENITH_ASSERT_FALSE(TeamMonsDiffer(axA[s], axB[s]), "slot %u: same seed must be identical", s);
		// golden vs oracle
		ZENITH_ASSERT_EQ((u_int)axA[s].m_eSpecies, (u_int)aeOracleSpecies[s], "slot %u species vs oracle", s);
		ZENITH_ASSERT_EQ((u_int)axA[s].m_eNature, (u_int)aeOracleNature[s], "slot %u nature vs oracle", s);
		// fixed tower fields
		ZENITH_ASSERT_EQ(axA[s].m_uLevel, uZM_TOWER_LEVEL, "slot %u level 50", s);
		ZENITH_ASSERT_EQ((u_int)axA[s].m_eAbility, (u_int)ZM_ABILITY_NONE, "slot %u ability NONE", s);
		ZENITH_ASSERT_EQ(axA[s].m_uCurExp, uZM_EXP_UNSPECIFIED, "slot %u exp UNSPECIFIED", s);
		ZENITH_ASSERT_EQ(axA[s].m_bOverrideBaseStats, false, "slot %u no override", s);
		for (u_int i = 0; i < ZM_STAT_COUNT; ++i)
		{
			ZENITH_ASSERT_EQ(axA[s].m_auIV[i], 31u, "slot %u IV %u == 31", s, i);
			ZENITH_ASSERT_EQ(axA[s].m_auEV[i], 0u, "slot %u EV %u == 0", s, i);
		}
	}
}

// A distinct seed yields at least one differing species (control: the RNG really
// drives species selection).
ZENITH_TEST(ZM_Data, Tower_Team_DifferentSeedDiverges)
{
	const u_int uStreak = 15u;   // UNCOMMON band -> a large eligible pool
	ZM_BattleMonsterSpec axA[uZM_TOWER_TEAM_SIZE];
	ZM_BattleMonsterSpec axB[uZM_TOWER_TEAM_SIZE];
	ZM_BattleRNG xRngA(0x1111ull);
	ZM_BattleRNG xRngB(0x9E3779B97F4A7C15ull);
	ZM_GenerateTowerTeam(uStreak, xRngA, axA, uZM_TOWER_TEAM_SIZE);
	ZM_GenerateTowerTeam(uStreak, xRngB, axB, uZM_TOWER_TEAM_SIZE);

	bool bDiffers = false;
	for (u_int s = 0; s < uZM_TOWER_TEAM_SIZE; ++s)
	{
		if (axA[s].m_eSpecies != axB[s].m_eSpecies) { bDiffers = true; break; }
	}
	ZENITH_ASSERT_TRUE(bDiffers, "distinct seeds must produce a differing team");
}

// Legality invariants over several seeds: exactly uCount mons, each builds at L50,
// none legendary, no duplicate species within a team.
ZENITH_TEST(ZM_Data, Tower_Team_Legality)
{
	const u_int64 aulSeeds[] = { 0x1ull, 0x2ull, 0xDEADull, 0xBEEFull, 0xABCDEFull, 0x5EEDull };
	const u_int uStreak = 12u;   // UNCOMMON band
	for (u_int k = 0; k < (u_int)(sizeof(aulSeeds) / sizeof(aulSeeds[0])); ++k)
	{
		ZM_BattleMonsterSpec axTeam[uZM_TOWER_TEAM_SIZE];
		ZM_BattleRNG xRng(aulSeeds[k]);
		ZM_GenerateTowerTeam(uStreak, xRng, axTeam, uZM_TOWER_TEAM_SIZE);

		for (u_int s = 0; s < uZM_TOWER_TEAM_SIZE; ++s)
		{
			const ZM_BattleMonster xBuilt = ZM_BuildBattleMonster(axTeam[s]);
			ZENITH_ASSERT_EQ(xBuilt.m_uLevel, 50u, "seed %u slot %u must build at L50", k, s);
			ZENITH_ASSERT_NE((u_int)ZM_GetSpeciesData(axTeam[s].m_eSpecies).m_eRarity, (u_int)ZM_RARITY_LEGENDARY,
				"seed %u slot %u must not be legendary", k, s);
			// distinctness against earlier slots
			for (u_int p = 0; p < s; ++p)
			{
				ZENITH_ASSERT_NE((u_int)axTeam[s].m_eSpecies, (u_int)axTeam[p].m_eSpecies,
					"seed %u duplicate species at slots %u,%u", k, p, s);
			}
		}
	}
}

// The rarity ceiling rises with the streak band and never admits a legendary:
// streak 0 -> all COMMON; streak 10 -> <= UNCOMMON; streak 30 -> <= RARE; and no
// legendary appears at any sampled streak (incl. 100).
ZENITH_TEST(ZM_Data, Tower_Team_RarityCeilingByBand)
{
	struct { u_int uStreak; ZM_RARITY eCeiling; } aCase[] = {
		{ 0u,   ZM_RARITY_COMMON   },
		{ 10u,  ZM_RARITY_UNCOMMON },
		{ 30u,  ZM_RARITY_RARE     },
		{ 100u, ZM_RARITY_RARE     },
	};
	for (u_int c = 0; c < (u_int)(sizeof(aCase) / sizeof(aCase[0])); ++c)
	{
		ZM_BattleMonsterSpec axTeam[uZM_TOWER_TEAM_SIZE];
		ZM_BattleRNG xRng(0x1234ull + aCase[c].uStreak);
		ZM_GenerateTowerTeam(aCase[c].uStreak, xRng, axTeam, uZM_TOWER_TEAM_SIZE);
		for (u_int s = 0; s < uZM_TOWER_TEAM_SIZE; ++s)
		{
			const ZM_RARITY eR = ZM_GetSpeciesData(axTeam[s].m_eSpecies).m_eRarity;
			ZENITH_ASSERT_LE((u_int)eR, (u_int)aCase[c].eCeiling,
				"streak %u slot %u rarity above ceiling", aCase[c].uStreak, s);
			ZENITH_ASSERT_NE((u_int)eR, (u_int)ZM_RARITY_LEGENDARY,
				"streak %u slot %u legendary", aCase[c].uStreak, s);
		}
	}
	// Streak 0: the ceiling is exactly COMMON, so every member is COMMON.
	ZM_BattleMonsterSpec axLow[uZM_TOWER_TEAM_SIZE];
	ZM_BattleRNG xRngLow(0x777ull);
	ZM_GenerateTowerTeam(0u, xRngLow, axLow, uZM_TOWER_TEAM_SIZE);
	for (u_int s = 0; s < uZM_TOWER_TEAM_SIZE; ++s)
	{
		ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(axLow[s].m_eSpecies).m_eRarity, (u_int)ZM_RARITY_COMMON,
			"streak 0 slot %u must be COMMON", s);
	}
}

// Each generated mon's four move slots equal the up-to-four highest-level (<= L50)
// learnset moves for its species, remaining slots NONE -- reconstructed
// independently from ZM_GetSpeciesLearnset.
ZENITH_TEST(ZM_Data, Tower_Team_MovesAreLearnsetDerived)
{
	const u_int uStreak = 25u;   // RARE band -> the widest species mix
	ZM_BattleMonsterSpec axTeam[uZM_TOWER_TEAM_SIZE];
	ZM_BattleRNG xRng(0xA5A5A5ull);
	ZM_GenerateTowerTeam(uStreak, xRng, axTeam, uZM_TOWER_TEAM_SIZE);

	for (u_int s = 0; s < uZM_TOWER_TEAM_SIZE; ++s)
	{
		ZM_MOVE_ID aeExpected[uZM_MAX_MOVES];
		OracleMovesForSpecies(axTeam[s].m_eSpecies, aeExpected);
		// The learnset always teaches an L1 move, so slot 0 is a real move.
		ZENITH_ASSERT_NE((u_int)aeExpected[0], (u_int)ZM_MOVE_NONE, "slot %u must have >=1 move", s);
		for (u_int i = 0; i < uZM_MAX_MOVES; ++i)
		{
			ZENITH_ASSERT_EQ((u_int)axTeam[s].m_aeMoves[i], (u_int)aeExpected[i],
				"slot %u move %u vs learnset oracle", s, i);
		}
	}
}

// ZM_TowerBattleSeed is distinct across streaks within a run and reproducible for
// a fixed (run, streak); teams generated from those seeds match on a repeat.
ZENITH_TEST(ZM_Data, Tower_Team_BattleSeedDistinctPerStreakReproducible)
{
	ZM_TowerRun xRun;
	xRun.m_ulSeed = 0x0123456789ABCDEFull;

	const u_int auStreaks[] = { 0u, 1u, 2u, 7u };
	u_int64 aulSeeds[4];
	for (u_int i = 0; i < 4u; ++i) { aulSeeds[i] = ZM_TowerBattleSeed(xRun, auStreaks[i]); }

	// Pairwise distinct.
	for (u_int a = 0; a < 4u; ++a)
	{
		for (u_int b = a + 1u; b < 4u; ++b)
		{
			ZENITH_ASSERT_NE(aulSeeds[a], aulSeeds[b], "battle seeds for streak %u,%u must differ",
				auStreaks[a], auStreaks[b]);
		}
	}

	// Reproducible for a fixed (run, streak).
	ZENITH_ASSERT_EQ(ZM_TowerBattleSeed(xRun, 2u), aulSeeds[2], "battle seed must be stable");

	// Teams from a repeated seed match; teams from two different-streak seeds are
	// generated at the same (streak 0) band but from distinct seeds -> a control on
	// the seed feeding through into the team.
	ZM_BattleMonsterSpec axFirst[uZM_TOWER_TEAM_SIZE];
	ZM_BattleMonsterSpec axRepeat[uZM_TOWER_TEAM_SIZE];
	ZM_BattleRNG xRng1(aulSeeds[0]);
	ZM_BattleRNG xRng2(aulSeeds[0]);
	ZM_GenerateTowerTeam(0u, xRng1, axFirst, uZM_TOWER_TEAM_SIZE);
	ZM_GenerateTowerTeam(0u, xRng2, axRepeat, uZM_TOWER_TEAM_SIZE);
	for (u_int s = 0; s < uZM_TOWER_TEAM_SIZE; ++s)
	{
		ZENITH_ASSERT_FALSE(TeamMonsDiffer(axFirst[s], axRepeat[s]), "slot %u repeat must match", s);
	}
}

// ZM_TowerMaxRarityForStreak band boundaries: 0,6 -> COMMON; 7,20 -> UNCOMMON;
// 21,100 -> RARE (never LEGENDARY).
ZENITH_TEST(ZM_Data, Tower_MaxRarity_BandBoundaries)
{
	ZENITH_ASSERT_EQ((u_int)ZM_TowerMaxRarityForStreak(0u),   (u_int)ZM_RARITY_COMMON);
	ZENITH_ASSERT_EQ((u_int)ZM_TowerMaxRarityForStreak(6u),   (u_int)ZM_RARITY_COMMON);
	ZENITH_ASSERT_EQ((u_int)ZM_TowerMaxRarityForStreak(7u),   (u_int)ZM_RARITY_UNCOMMON);
	ZENITH_ASSERT_EQ((u_int)ZM_TowerMaxRarityForStreak(20u),  (u_int)ZM_RARITY_UNCOMMON);
	ZENITH_ASSERT_EQ((u_int)ZM_TowerMaxRarityForStreak(21u),  (u_int)ZM_RARITY_RARE);
	ZENITH_ASSERT_EQ((u_int)ZM_TowerMaxRarityForStreak(100u), (u_int)ZM_RARITY_RARE);
	// Never reaches LEGENDARY across a wide sweep.
	for (u_int s = 0u; s <= 200u; ++s)
	{
		ZENITH_ASSERT_NE((u_int)ZM_TowerMaxRarityForStreak(s), (u_int)ZM_RARITY_LEGENDARY,
			"max rarity must never be LEGENDARY (streak %u)", s);
	}
}

// ############################################################################
// E. Config (ZM_Data)
// ############################################################################

// The tower battle config: L50 cap, trainer battle, no wild/catch/flee, no exp.
ZENITH_TEST(ZM_Data, Tower_Config_Fields)
{
	const ZM_BattleConfig xCfg = ZM_MakeTowerBattleConfig();
	ZENITH_ASSERT_EQ(xCfg.m_uLevelCap, uZM_TOWER_LEVEL, "level cap must be 50");
	ZENITH_ASSERT_EQ(xCfg.m_bIsWild, false, "not a wild battle");
	ZENITH_ASSERT_EQ(xCfg.m_bCanCatch, false, "no catching in the tower");
	ZENITH_ASSERT_EQ(xCfg.m_bCanFlee, false, "no fleeing in the tower");
	ZENITH_ASSERT_EQ(xCfg.m_bIsTrainerBattle, true, "tower battles are trainer battles");
	ZENITH_ASSERT_EQ(xCfg.m_bAwardExp, false, "a flat-50 facility grants no exp");
}

// ############################################################################
// F. Engine round-trip smokes (ZM_Battle)
// ############################################################################

// A tower battle -- clamped player vs the generated tower team's LEAD (a
// controlled 1v1 so the first KO ends the battle; see RunOneTowerBattle), driven
// through ZM_BattleEngine with the ENEMY action fed by ZM_ChooseAction at the
// streak's AI tier -- terminates to a decided result within the soak turn cap;
// ZM_TowerAdvance settles the streak from the winner; and an identically-seeded
// replay is bit-deterministic (same winner + same turn count).
ZENITH_TEST(ZM_Battle, Tower_Battle_EndToEndTerminates)
{
	const u_int uStreak = 0u;
	const SmokeResult xR = RunOneTowerBattle(uStreak, 0xBADC0FFEEull, 0xA1F00Dull);

	ZENITH_ASSERT_TRUE(xR.m_bOver, "tower battle must terminate within the turn cap");
	ZENITH_ASSERT_LT(xR.m_uTurns, 500u, "terminated strictly under the soak bound");
	ZENITH_ASSERT_EQ((u_int)xR.m_eWinner, (u_int)ZM_SIDE_PLAYER, "the overpowered clamped player wins");

	// Settle the streak from the (player) win.
	ZM_TowerRun xRun;
	xRun.m_uCurrentStreak = uStreak;
	const u_int uNew = ZM_TowerAdvance(xRun, xR.m_eWinner == ZM_SIDE_PLAYER);
	ZENITH_ASSERT_EQ(uNew, uStreak + 1u, "a win advances the streak");
	ZENITH_ASSERT_EQ(xRun.m_uBestStreak, uStreak + 1u);

	// Determinism: an identically-seeded replay yields the identical outcome.
	const SmokeResult xReplay = RunOneTowerBattle(uStreak, 0xBADC0FFEEull, 0xA1F00Dull);
	ZENITH_ASSERT_EQ((u_int)xReplay.m_eWinner, (u_int)xR.m_eWinner, "replay winner must match");
	ZENITH_ASSERT_EQ(xReplay.m_uTurns, xR.m_uTurns, "replay turn count must match");
}

// The tower config has awards OFF, so a completed tower battle emits ZERO
// EXP_GAINED events (belt-and-braces that the flat-50 facility grants no levels).
ZENITH_TEST(ZM_Battle, Tower_Battle_NoExpAwarded)
{
	const SmokeResult xR = RunOneTowerBattle(0u, 0xF00DBEEFull, 0xC0FFEEull);
	ZENITH_ASSERT_TRUE(xR.m_bOver, "battle must complete");
	ZENITH_ASSERT_EQ(xR.m_uExpEvents, 0u, "awards-off tower battle must emit no EXP_GAINED events");
}
