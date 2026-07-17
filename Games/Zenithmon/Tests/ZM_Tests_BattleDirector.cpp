#include "Zenith.h"

// ============================================================================
// ZM_Tests_BattleDirector -- S5 item-4 unit tests for ZM_BattleDirectorCore, the
// pure, headless battle presenter (ZM-D-101). Everything here is PURE: no ECS, no
// scene, no graphics -- just the engine + the director's state machine + the
// event->op table. All fixtures are deterministic and hermetic (no baked assets),
// so no RequestSkip is needed.
//
// Coverage:
//   * ZM_MapEventToOp totality (every ZM_BATTLE_EVENT kind maps to a defined op;
//     framing kinds -> ZM_POP_NONE; the text/HP contracts)
//   * zm_instant_battles collapse (whole turn drains in one Tick) vs timed
//     advancement (a sub-op-duration Tick does NOT fully advance)
//   * ShouldRequestEnd latches at OVER after a full battle
//   * ZM_BuildWildEnemySpec learnset-derived moves
//   * AI non-perturbation: a director drive is byte-identical to a manual raw-
//     engine drive with the same seed / player picks / identically-seeded AI rng.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Battle/ZM_BattleDirectorCore.h"
#include "Zenithmon/Source/Battle/ZM_BattleEngine.h"
#include "Zenithmon/Source/Battle/ZM_BattleAI.h"
#include "Zenithmon/Source/Battle/ZM_BattleTypes.h"
#include "Zenithmon/Source/Data/ZM_BattleRNG.h"
#include "Zenithmon/Source/Data/ZM_Learnsets.h"
#include "Zenithmon/Source/Data/ZM_MoveData.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"

namespace
{
	// A wild single-battle config (exp off, so the driver exercises the box-1..3
	// event kinds without progression). Mirrors the SC-scoped wild encounter path.
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

	// A ZM_BATTLE_EVENT kind maps to ZM_POP_NONE iff it is a pure framing event.
	bool IsFramingKind(ZM_BATTLE_EVENT eKind)
	{
		return eKind == ZM_BATTLE_EVENT_BATTLE_BEGIN
		    || eKind == ZM_BATTLE_EVENT_TURN_BEGIN
		    || eKind == ZM_BATTLE_EVENT_TURN_END
		    || eKind == ZM_BATTLE_EVENT_EVOLUTION_QUEUED;
	}
}

// 1. The whole point: the event->op table is TOTAL. Every kind in [0, COUNT)
//    maps to a defined op; framing kinds are ZM_POP_NONE, everything else is not.
ZENITH_TEST(ZM_BattleDirector, MapEventToOp_TotalOverEveryKind)
{
	for (u_int u = 0u; u < (u_int)ZM_BATTLE_EVENT_COUNT; ++u)
	{
		ZM_BattleEvent x;
		x.m_eKind = (ZM_BATTLE_EVENT)u;
		const ZM_BattlePresentationOp xOp = ZM_MapEventToOp(x);

		ZENITH_ASSERT_LT((u_int)xOp.m_eOp, (u_int)ZM_POP_COUNT,
			"kind %u mapped to an out-of-range op", u);

		if (IsFramingKind((ZM_BATTLE_EVENT)u))
		{
			ZENITH_ASSERT_EQ((u_int)xOp.m_eOp, (u_int)ZM_POP_NONE,
				"framing kind %u should present nothing", u);
		}
		else
		{
			ZENITH_ASSERT_NE((u_int)xOp.m_eOp, (u_int)ZM_POP_NONE,
				"non-framing kind %u should present something", u);
		}
	}
}

// 2. Text contract: the log-line kinds carry text; a bare damage tween does not.
ZENITH_TEST(ZM_BattleDirector, MapEventToOp_TextKindsCarryALine)
{
	const ZM_BATTLE_EVENT aeTextKinds[] = {
		ZM_BATTLE_EVENT_MOVE_USED, ZM_BATTLE_EVENT_MOVE_MISSED,
		ZM_BATTLE_EVENT_BATTLE_END, ZM_BATTLE_EVENT_SWITCH_IN, ZM_BATTLE_EVENT_FAINT
	};
	for (ZM_BATTLE_EVENT eKind : aeTextKinds)
	{
		ZM_BattleEvent x; x.m_eKind = eKind;
		ZENITH_ASSERT_TRUE(ZM_MapEventToOp(x).m_bCarriesText,
			"kind %u should carry a log line", (u_int)eKind);
	}

	ZM_BattleEvent xDamage; xDamage.m_eKind = ZM_BATTLE_EVENT_DAMAGE_DEALT;
	ZENITH_ASSERT_FALSE(ZM_MapEventToOp(xDamage).m_bCarriesText,
		"DAMAGE_DEALT is a bare HP tween, no line");
}

// 3. HP contract: every HP-changing kind resolves to an HP tween op.
ZENITH_TEST(ZM_BattleDirector, MapEventToOp_HpKindsAreTweens)
{
	const ZM_BATTLE_EVENT aeHpKinds[] = {
		ZM_BATTLE_EVENT_DAMAGE_DEALT, ZM_BATTLE_EVENT_STATUS_DAMAGE,
		ZM_BATTLE_EVENT_HEAL, ZM_BATTLE_EVENT_DRAIN, ZM_BATTLE_EVENT_RECOIL,
		ZM_BATTLE_EVENT_WEATHER_DAMAGE
	};
	for (ZM_BATTLE_EVENT eKind : aeHpKinds)
	{
		ZM_BattleEvent x; x.m_eKind = eKind;
		ZENITH_ASSERT_EQ((u_int)ZM_MapEventToOp(x).m_eOp, (u_int)ZM_POP_HP_TWEEN,
			"kind %u should be an HP tween", (u_int)eKind);
	}
}

// 4. Instant mode collapses all timing: one Tick drains the intro; one Tick drains
//    a whole turn -- the machine is never left mid-turn in PLAYING_EVENTS.
ZENITH_TEST(ZM_BattleDirector, InstantBattles_DrainsWholeTurnInOneTick)
{
	ZM_SetInstantBattlesForTests(true);

	const ZM_BattleMonsterSpec xPlayer = ZM_BuildWildEnemySpec(ZM_SPECIES_FERNFAWN, 5u);
	const ZM_BattleMonsterSpec xEnemy  = ZM_BuildWildEnemySpec(ZM_SPECIES_KINDLET, 5u);
	const ZM_BattleConfig xCfg = MakeWildConfig();

	ZM_BattleDirectorCore xDirector;
	xDirector.Begin(&xPlayer, 1u, &xEnemy, 1u, xCfg, 0xC0FFEEull, ZM_AI_TIER_GREEDY);

	// Intro (BATTLE_BEGIN + both SWITCH_INs) drains in a single 0-second tick.
	xDirector.Tick(0.0f);
	ZENITH_ASSERT_EQ((u_int)xDirector.GetState(), (u_int)ZM_DIRECTOR_AWAIT_INPUT,
		"instant intro should drain in one Tick");

	// A full turn also drains in one tick: never stuck in PLAYING_EVENTS.
	xDirector.SubmitPlayerAction(MakeMoveSlot0());
	xDirector.Tick(0.0f);
	const bool bResolvedOrAwaiting =
		xDirector.GetState() == ZM_DIRECTOR_AWAIT_INPUT ||
		xDirector.GetState() == ZM_DIRECTOR_OVER;
	ZENITH_ASSERT_TRUE(bResolvedOrAwaiting,
		"instant turn should fully drain in one Tick (state %u)", (u_int)xDirector.GetState());

	ZM_SetInstantBattlesForTests(false);
}

// 5. Timed mode gates advancement: a sub-op-duration Tick leaves the machine mid-
//    intro in PLAYING_EVENTS; only enough accrued time drains the range.
ZENITH_TEST(ZM_BattleDirector, TimedBattles_AdvancesGraduallyNotInstant)
{
	ZM_SetInstantBattlesForTests(false);

	const ZM_BattleMonsterSpec xPlayer = ZM_BuildWildEnemySpec(ZM_SPECIES_FERNFAWN, 5u);
	const ZM_BattleMonsterSpec xEnemy  = ZM_BuildWildEnemySpec(ZM_SPECIES_KINDLET, 5u);
	const ZM_BattleConfig xCfg = MakeWildConfig();

	ZM_BattleDirectorCore xDirector;
	xDirector.Begin(&xPlayer, 1u, &xEnemy, 1u, xCfg, 0x1234ull, ZM_AI_TIER_GREEDY);

	// The intro contains a SWITCH_IN (0.4s), longer than this tiny tick, so a single
	// 0.01s Tick cannot fully drain the range.
	xDirector.Tick(0.01f);
	ZENITH_ASSERT_EQ((u_int)xDirector.GetState(), (u_int)ZM_DIRECTOR_PLAYING_EVENTS,
		"a sub-op-duration tick must not fully advance the intro");

	// A generous tick drains the rest.
	xDirector.Tick(100.0f);
	ZENITH_ASSERT_EQ((u_int)xDirector.GetState(), (u_int)ZM_DIRECTOR_AWAIT_INPUT,
		"a large tick should drain the intro to AWAIT_INPUT");
}

// 6. SubmitPlayerAction's precondition is the AWAIT_INPUT state. Asserting the
//    fault path would be fatal, so we verify the guard state non-fatally: right
//    after Begin the machine is presenting the intro, NOT awaiting input.
ZENITH_TEST(ZM_BattleDirector, SubmitPlayerAction_RejectedOutsideAwaitInput)
{
	const ZM_BattleMonsterSpec xPlayer = ZM_BuildWildEnemySpec(ZM_SPECIES_FERNFAWN, 5u);
	const ZM_BattleMonsterSpec xEnemy  = ZM_BuildWildEnemySpec(ZM_SPECIES_KINDLET, 5u);
	const ZM_BattleConfig xCfg = MakeWildConfig();

	ZM_BattleDirectorCore xDirector;
	xDirector.Begin(&xPlayer, 1u, &xEnemy, 1u, xCfg, 7u, ZM_AI_TIER_GREEDY);

	// Directly after Begin the intro is playing -> input is NOT accepted; a
	// SubmitPlayerAction here would trip the Zenith_Assert precondition.
	ZENITH_ASSERT_FALSE(xDirector.IsAwaitingInput(),
		"the intro is playing right after Begin; SubmitPlayerAction is rejected until AWAIT_INPUT");
	ZENITH_ASSERT_EQ((u_int)xDirector.GetState(), (u_int)ZM_DIRECTOR_PLAYING_EVENTS,
		"post-Begin state is PLAYING_EVENTS");
}

// 7. Driving a full battle to completion latches ShouldRequestEnd exactly at OVER,
//    with the engine reporting over.
ZENITH_TEST(ZM_BattleDirector, Resolution_SignalsRequestEndExactlyOnce)
{
	ZM_SetInstantBattlesForTests(true);

	const ZM_BattleMonsterSpec xPlayer = ZM_BuildWildEnemySpec(ZM_SPECIES_FERNFAWN, 5u);
	const ZM_BattleMonsterSpec xEnemy  = ZM_BuildWildEnemySpec(ZM_SPECIES_KINDLET, 5u);
	const ZM_BattleConfig xCfg = MakeWildConfig();

	ZM_BattleDirectorCore xDirector;
	xDirector.Begin(&xPlayer, 1u, &xEnemy, 1u, xCfg, 0xABCDEFull, ZM_AI_TIER_GREEDY);

	u_int uIter = 0u;
	while (!xDirector.ShouldRequestEnd() && uIter < 200u)
	{
		if (xDirector.IsAwaitingInput())
		{
			xDirector.SubmitPlayerAction(MakeMoveSlot0());
		}
		xDirector.Tick(0.0f);
		++uIter;
	}

	ZENITH_ASSERT_TRUE(xDirector.ShouldRequestEnd(), "battle should reach the request-end latch");
	ZENITH_ASSERT_TRUE(xDirector.IsOver(), "engine should report over at request-end");
	ZENITH_ASSERT_EQ((u_int)xDirector.GetState(), (u_int)ZM_DIRECTOR_OVER, "final state is OVER");

	ZM_SetInstantBattlesForTests(false);
}

// 8. ZM_BuildWildEnemySpec derives its moves from the species learnset (moves
//    learnable at/below the level), with a filled slot 0 (every species has a L1 move).
ZENITH_TEST(ZM_BattleDirector, BuildWildEnemySpec_DerivesLearnsetMoves)
{
	const ZM_BattleMonsterSpec xSpec = ZM_BuildWildEnemySpec(ZM_SPECIES_FERNFAWN, 5u);

	ZENITH_ASSERT_EQ((u_int)xSpec.m_eSpecies, (u_int)ZM_SPECIES_FERNFAWN, "species preserved");
	ZENITH_ASSERT_EQ(xSpec.m_uLevel, 5u, "level preserved");
	ZENITH_ASSERT_NE((u_int)xSpec.m_aeMoves[0], (u_int)ZM_MOVE_NONE, "slot 0 filled (L1 move)");

	// Every non-NONE move must be one the learnset teaches at level <= 5.
	const ZM_Learnset xLearnset = ZM_GetSpeciesLearnset(ZM_SPECIES_FERNFAWN);
	for (u_int i = 0u; i < uZM_MAX_MOVES; ++i)
	{
		if (xSpec.m_aeMoves[i] == ZM_MOVE_NONE)
		{
			continue;
		}
		bool bFound = false;
		for (u_int k = 0u; k < xLearnset.m_uCount; ++k)
		{
			if (xLearnset.m_axMoves[k].m_eMove == xSpec.m_aeMoves[i] &&
				xLearnset.m_axMoves[k].m_uLevel <= 5u)
			{
				bFound = true;
				break;
			}
		}
		ZENITH_ASSERT_TRUE(bFound,
			"move slot %u must be a learnset entry at level <= 5", i);
	}
}

// 9. Non-perturbation proof: a director drive produces a byte-identical engine
//    event stream to a hand drive of a RAW engine with the SAME seed, the SAME
//    player MOVE-slot-0 each turn, and enemy actions from an AI rng seeded EXACTLY
//    as the director seeds it. Uses RANDOM tier so the AI rng actually draws.
ZENITH_TEST(ZM_BattleDirector, AiRngUnperturbing_DirectorDriveMatchesManualDrive)
{
	ZM_SetInstantBattlesForTests(true);

	const ZM_BattleMonsterSpec xPlayer = ZM_BuildWildEnemySpec(ZM_SPECIES_FERNFAWN, 5u);
	const ZM_BattleMonsterSpec xEnemy  = ZM_BuildWildEnemySpec(ZM_SPECIES_KINDLET, 5u);
	const ZM_BattleConfig xCfg = MakeWildConfig();
	const u_int64 ulSeed = 0x5EED1234ull;
	const ZM_AI_TIER eTier = ZM_AI_TIER_RANDOM;

	// --- director drive ---
	ZM_BattleDirectorCore xDirector;
	xDirector.Begin(&xPlayer, 1u, &xEnemy, 1u, xCfg, ulSeed, eTier);
	u_int uIter = 0u;
	while (!xDirector.ShouldRequestEnd() && uIter < 200u)
	{
		if (xDirector.IsAwaitingInput())
		{
			xDirector.SubmitPlayerAction(MakeMoveSlot0());
		}
		xDirector.Tick(0.0f);
		++uIter;
	}

	// --- manual raw-engine drive, mirroring the director exactly ---
	ZM_BattleEngine xManual;
	xManual.Begin(xCfg, &xPlayer, 1u, &xEnemy, 1u, ulSeed);
	ZM_BattleRNG xManualAiRng(ZM_DeriveAiRngSeed(ulSeed));
	u_int uManualIter = 0u;
	while (!xManual.IsOver() && uManualIter < 200u)
	{
		const ZM_BattleAction xEnemyAction = ZM_ChooseAction(xManual.GetState(), ZM_SIDE_ENEMY, eTier, xManualAiRng);
		xManual.SubmitAction(ZM_SIDE_PLAYER, MakeMoveSlot0());
		xManual.SubmitAction(ZM_SIDE_ENEMY, xEnemyAction);
		xManual.ResolveTurn();
		++uManualIter;
	}

	// Byte-identical event streams via the defaulted ZM_BattleEvent operator==.
	const ZM_BattleEngine& xDirEngine = xDirector.GetEngine();
	ZENITH_ASSERT_TRUE(xDirEngine.IsOver(), "director battle should have ended");
	ZENITH_ASSERT_TRUE(xManual.IsOver(), "manual battle should have ended");
	ZENITH_ASSERT_EQ(xDirEngine.GetEventCount(), xManual.GetEventCount(),
		"director and manual drives must emit the same number of events");

	const u_int uCount = xDirEngine.GetEventCount() < xManual.GetEventCount()
		? xDirEngine.GetEventCount() : xManual.GetEventCount();
	for (u_int i = 0u; i < uCount; ++i)
	{
		ZENITH_ASSERT_TRUE(xDirEngine.GetEvent(i) == xManual.GetEvent(i),
			"event %u differs between director and manual drive", i);
	}

	ZM_SetInstantBattlesForTests(false);
}
