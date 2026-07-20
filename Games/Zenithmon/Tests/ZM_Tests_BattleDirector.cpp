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
#include "Zenithmon/Components/ZM_BattleDirector.h"
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

// ============================================================================
// SC3 additions -- the ECS-facing ZM_BattleDirector component's pure seams. These
// exercise the FROZEN static contract of ZM_BattleDirector (S5 item-4 SC3): the
// placeholder/config builders and the two guard predicates that gate its OnUpdate
// phase machine. They stay PURE (no ECS, no scene) -- the windowed round-trip
// gate (ZM_AutoTests_BattleDirector.cpp) proves the live drive.
// ============================================================================

// 10. The placeholder player spec the director hands the arena is deterministic:
//     byte-identical across calls on the fields that matter, a real (non-NONE)
//     species, and a filled slot-0 move. The exact species is an impl choice, so
//     it is asserted only non-NONE, never a specific value.
ZENITH_TEST(ZM_BattleDirector, Director_PlaceholderPlayerIsDeterministic)
{
	const ZM_BattleMonsterSpec xA = ZM_BattleDirector::BuildPlaceholderPlayerSpec();
	const ZM_BattleMonsterSpec xB = ZM_BattleDirector::BuildPlaceholderPlayerSpec();

	// A real, usable monster -- but the species is an impl choice, so assert only
	// non-NONE + a filled first move, never a value.
	ZENITH_ASSERT_NE((u_int)xA.m_eSpecies, (u_int)ZM_SPECIES_NONE,
		"the placeholder player must be a real species");
	ZENITH_ASSERT_NE((u_int)xA.m_aeMoves[0], (u_int)ZM_MOVE_NONE,
		"the placeholder player must have a real slot-0 move");

	// Deterministic across calls on every field that matters.
	ZENITH_ASSERT_EQ((u_int)xA.m_eSpecies, (u_int)xB.m_eSpecies, "species must be deterministic");
	ZENITH_ASSERT_EQ(xA.m_uLevel, xB.m_uLevel, "level must be deterministic");
	for (u_int i = 0u; i < uZM_MAX_MOVES; ++i)
	{
		ZENITH_ASSERT_EQ((u_int)xA.m_aeMoves[i], (u_int)xB.m_aeMoves[i],
			"move slot %u must be deterministic across calls", i);
	}
}

// 11. The director's battle is a wild encounter with exp OFF: the headless
//     AI-vs-AI battle must never perturb progression (zero-perturbation, ZM-D-032).
ZENITH_TEST(ZM_BattleDirector, Director_BattleConfigExpOff)
{
	const ZM_BattleConfig xCfg = ZM_BattleDirector::BuildBattleConfig();
	ZENITH_ASSERT_TRUE(xCfg.m_bIsWild, "the director's battle is a wild battle");
	ZENITH_ASSERT_FALSE(xCfg.m_bAwardExp, "the headless AI-vs-AI battle must not award exp");
}

// 11b. The director's wild battle permits BOTH catch and flee: the SC4 root menu adds
//      Catch (Fight/Catch/Run), so BuildBattleConfig() now sets m_bCanCatch = true (and
//      keeps m_bCanFlee = true for RUN). A sibling of the exp-off assert above so that
//      pin stays untouched.
ZENITH_TEST(ZM_BattleDirector, Director_BattleConfigCatchOn)
{
	const ZM_BattleConfig xCfg = ZM_BattleDirector::BuildBattleConfig();
	ZENITH_ASSERT_TRUE(xCfg.m_bCanCatch, "the director's wild battle permits catching (SC4)");
	ZENITH_ASSERT_TRUE(xCfg.m_bCanFlee, "the director's wild battle permits fleeing (SC5 RUN)");
}

// 11c. The core SURFACES the battle's can-catch rule to its presenter (Shortfalls 1.5
//      deferral (a)). The battle menu gates its Catch entry on this, so it must track
//      the config the battle was actually Begun with -- never a copy the UI keeps, and
//      never a constant. ZM_BattleTower already Begins battles with m_bCanCatch = false,
//      so the false case is a REAL reachable state, not a hypothetical.
ZENITH_TEST(ZM_BattleDirector, Director_CoreSurfacesCanCatchFromConfig)
{
	const ZM_BattleMonsterSpec xPlayer = ZM_BuildWildEnemySpec(ZM_SPECIES_FERNFAWN, 5u);
	const ZM_BattleMonsterSpec xEnemy  = ZM_BuildWildEnemySpec(ZM_SPECIES_KINDLET, 5u);

	// FAILS IF: IsCatchAllowed() is implemented as a constant / a UI-side copy rather
	// than a read-through to the engine's config -- a battle that never Begun has no
	// rules yet, and the fail-CLOSED answer is "no catching".
	ZM_BattleDirectorCore xNotStarted;
	ZENITH_ASSERT_FALSE(xNotStarted.IsCatchAllowed(),
		"a core that has not Begun a battle must not claim catching is allowed");

	// FAILS IF: IsCatchAllowed() stops reading m_bCanCatch (e.g. reads m_bIsWild, which
	// is TRUE in both configs below and would make both cases agree).
	ZM_BattleConfig xNoCatch = MakeWildConfig();   // MakeWildConfig leaves m_bCanCatch false
	ZENITH_ASSERT_FALSE(xNoCatch.m_bCanCatch, "fixture precondition: this config forbids catching");
	ZM_BattleDirectorCore xNoCatchCore;
	xNoCatchCore.Begin(&xPlayer, 1u, &xEnemy, 1u, xNoCatch, 0x5EED01ull, ZM_AI_TIER_GREEDY);
	ZENITH_ASSERT_FALSE(xNoCatchCore.IsCatchAllowed(),
		"a battle Begun with m_bCanCatch = false must report catching as disallowed");

	ZM_BattleConfig xCatch = MakeWildConfig();
	xCatch.m_bCanCatch = true;
	ZM_BattleDirectorCore xCatchCore;
	xCatchCore.Begin(&xPlayer, 1u, &xEnemy, 1u, xCatch, 0x5EED02ull, ZM_AI_TIER_GREEDY);
	ZENITH_ASSERT_TRUE(xCatchCore.IsCatchAllowed(),
		"a battle Begun with m_bCanCatch = true must report catching as allowed");
}

// 12. Setup is one-shot: it fires ONLY in WAIT_FOR_IN_BATTLE, ONLY once the
//     transition is in battle, and ONLY if not already set up. A wrong phase, a
//     not-yet-in-battle transition, or an already-set-up latch all suppress it.
ZENITH_TEST(ZM_BattleDirector, Director_SetupIsOneShot)
{
	ZENITH_ASSERT_TRUE(
		ZM_BattleDirector::ShouldRunSetup(ZM_BD_WAIT_FOR_IN_BATTLE, true, false),
		"first in-battle frame, not yet set up -> run setup");
	ZENITH_ASSERT_FALSE(
		ZM_BattleDirector::ShouldRunSetup(ZM_BD_WAIT_FOR_IN_BATTLE, true, true),
		"already set up -> never again (one-shot latch)");
	ZENITH_ASSERT_FALSE(
		ZM_BattleDirector::ShouldRunSetup(ZM_BD_WAIT_FOR_IN_BATTLE, false, false),
		"transition not yet in battle -> do not run setup");
	ZENITH_ASSERT_FALSE(
		ZM_BattleDirector::ShouldRunSetup(ZM_BD_RUNNING, true, false),
		"wrong phase (RUNNING) -> do not run setup");
}

// 13. The end request is gated on the core actually resolving: the director only
//     asks the transition to end AFTER a real ZM_BattleDirectorCore reaches OVER,
//     and it latches (asks exactly once). Drives a REAL core to OVER under instant
//     mode, reusing the drive pattern above.
ZENITH_TEST(ZM_BattleDirector, Director_RequestsEndOnlyAfterResolved)
{
	ZM_SetInstantBattlesForTests(true);

	const ZM_BattleMonsterSpec xPlayer = ZM_BuildWildEnemySpec(ZM_SPECIES_FERNFAWN, 5u);
	const ZM_BattleMonsterSpec xEnemy  = ZM_BuildWildEnemySpec(ZM_SPECIES_KINDLET, 5u);
	const ZM_BattleConfig xCfg = MakeWildConfig();

	ZM_BattleDirectorCore xCore;
	xCore.Begin(&xPlayer, 1u, &xEnemy, 1u, xCfg, 0xD1EEC7ull, ZM_AI_TIER_GREEDY);

	// Before the core is OVER, the director must NOT request an end even in RUNNING:
	// the gate demands the core actually reports it should end.
	ZENITH_ASSERT_FALSE(xCore.ShouldRequestEnd(), "the core is not over immediately after Begin");
	ZENITH_ASSERT_FALSE(
		ZM_BattleDirector::ShouldRequestEndNow(ZM_BD_RUNNING, false, false),
		"RUNNING but the core has not yet ended -> must not request the end");

	// Drive the core to OVER (same bounded pattern as test 7 above).
	u_int uIter = 0u;
	while (!xCore.ShouldRequestEnd() && uIter < 200u)
	{
		if (xCore.IsAwaitingInput())
		{
			xCore.SubmitPlayerAction(MakeMoveSlot0());
		}
		xCore.Tick(0.0f);
		++uIter;
	}
	ZENITH_ASSERT_TRUE(xCore.ShouldRequestEnd(), "the driven core should reach the request-end latch");

	// Now the core says it should end: RUNNING + not yet requested -> request now.
	ZENITH_ASSERT_TRUE(
		ZM_BattleDirector::ShouldRequestEndNow(ZM_BD_RUNNING, true, false),
		"RUNNING + core over + not yet requested -> request the end now");
	// Latch: once requested, never again.
	ZENITH_ASSERT_FALSE(
		ZM_BattleDirector::ShouldRequestEndNow(ZM_BD_RUNNING, true, true),
		"already requested -> must not request the end again");
	// Phase guard: only RUNNING requests the end.
	ZENITH_ASSERT_FALSE(
		ZM_BattleDirector::ShouldRequestEndNow(ZM_BD_SETUP, true, false),
		"non-RUNNING phase (SETUP) -> must not request the end");

	ZM_SetInstantBattlesForTests(false);
}

// 14. The battle seed is a deterministic PURE function of the encounter payload, so a
//     windowed drive is reproducible; it is sensitive to BOTH the species and the
//     level, so distinct encounters derive distinct seeds (FNV-1a over both inputs).
ZENITH_TEST(ZM_BattleDirector, Director_BattleSeedIsDeterministic)
{
	const u_int64 ulA = ZM_BattleDirector::DeriveBattleSeed(ZM_SPECIES_FERNFAWN, 5u);
	ZENITH_ASSERT_EQ(ulA, ZM_BattleDirector::DeriveBattleSeed(ZM_SPECIES_FERNFAWN, 5u),
		"same (species, level) must derive the same seed");
	ZENITH_ASSERT_NE(ulA, ZM_BattleDirector::DeriveBattleSeed(ZM_SPECIES_KINDLET, 5u),
		"a different species must derive a different seed");
	ZENITH_ASSERT_NE(ulA, ZM_BattleDirector::DeriveBattleSeed(ZM_SPECIES_FERNFAWN, 6u),
		"a different level must derive a different seed");
}

// 15. The presented-event boundary the HUD scans (ZM_BattleDirectorCore::PresentedEventCount)
//     reaches the FULL event stream once the battle is OVER -- this is what lets the HUD show
//     the final line even under an instant drain (where CurrentEvent() is already null). A
//     resolved battle also emitted at least one event.
ZENITH_TEST(ZM_BattleDirector, Director_PresentedEventCountReachesEndAtOver)
{
	ZM_SetInstantBattlesForTests(true);

	const ZM_BattleMonsterSpec xPlayer = ZM_BuildWildEnemySpec(ZM_SPECIES_FERNFAWN, 5u);
	const ZM_BattleMonsterSpec xEnemy  = ZM_BuildWildEnemySpec(ZM_SPECIES_KINDLET, 5u);
	const ZM_BattleConfig xCfg = MakeWildConfig();

	ZM_BattleDirectorCore xCore;
	xCore.Begin(&xPlayer, 1u, &xEnemy, 1u, xCfg, 0xBEEF77ull, ZM_AI_TIER_GREEDY);

	u_int uIter = 0u;
	while (!xCore.ShouldRequestEnd() && uIter < 200u)
	{
		if (xCore.IsAwaitingInput())
		{
			xCore.SubmitPlayerAction(MakeMoveSlot0());
		}
		xCore.Tick(0.0f);
		++uIter;
	}
	ZENITH_ASSERT_TRUE(xCore.ShouldRequestEnd(), "the driven core should reach OVER");

	// At OVER every event has been presented -> the HUD's scan boundary spans the whole
	// stream, so the final (winner) line is reachable even after an instant drain.
	ZENITH_ASSERT_EQ(xCore.PresentedEventCount(), xCore.GetEngine().GetEventCount(),
		"at OVER, every event is presented (the HUD must see the final line)");
	ZENITH_ASSERT_TRUE(xCore.GetEngine().GetEventCount() >= 1u, "a resolved battle emitted events");

	ZM_SetInstantBattlesForTests(false);
}

// 16. In TIMED mode (zm_instant_battles OFF -- the DEFAULT shipping mode) the intro range
//     is NOT drained in Begin, so the core sits in PLAYING_EVENTS with the current op still
//     pending. PresentedEventCount() must INCLUDE that current op (the cursor+1 branch) so
//     the HUD's [0, PresentedEventCount()) scan can show the line being presented right now.
//     Every OTHER test runs under instant (which only ever hits the cursor/else branch), so
//     this is the sole lock on the timed-reveal path: dropping the +1 would return 0 here.
ZENITH_TEST(ZM_BattleDirector, Director_PresentedEventCountIncludesCurrentOpWhilePresenting)
{
	ZM_SetInstantBattlesForTests(false);   // timed: the intro is presented op-by-op, not drained

	const ZM_BattleMonsterSpec xPlayer = ZM_BuildWildEnemySpec(ZM_SPECIES_FERNFAWN, 5u);
	const ZM_BattleMonsterSpec xEnemy  = ZM_BuildWildEnemySpec(ZM_SPECIES_KINDLET, 5u);
	const ZM_BattleConfig xCfg = MakeWildConfig();

	ZM_BattleDirectorCore xCore;
	xCore.Begin(&xPlayer, 1u, &xEnemy, 1u, xCfg, 0xC0FFEEull, ZM_AI_TIER_GREEDY);

	// Begin enters PLAYING_EVENTS presenting the intro range [0, N) WITHOUT draining it.
	ZENITH_ASSERT_EQ((u_int)xCore.GetState(), (u_int)ZM_DIRECTOR_PLAYING_EVENTS,
		"timed Begin sits in PLAYING_EVENTS (the intro is not drained)");
	ZENITH_ASSERT_TRUE(xCore.GetEngine().GetEventCount() >= 1u, "the intro emitted events");
	// cursor is 0 and the current op is INCLUDED -> the count is 1, not 0.
	ZENITH_ASSERT_EQ(xCore.PresentedEventCount(), 1u,
		"the currently-presenting op is included (cursor+1) while PLAYING_EVENTS");
}
