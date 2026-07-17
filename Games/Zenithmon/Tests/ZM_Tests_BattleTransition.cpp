#include "Zenith.h"

// ============================================================================
// ZM_Tests_BattleTransition -- S5 item 3 (SC3b) unit gate for the
// ZM_BattleTransition component's PURE policy surface (category
// ZM_BattleTransition). The transition machine (component order 110) subscribes
// to ZM_OnWildEncounter, validates the dispatched payload, and picks the arena
// biome for the source scene. These cases pin the three PURE static helpers, so
// a drift in the payload-acceptance rules (species range, level band, which
// scene kinds may launch a battle) or the biome mapping is a boot-gate failure
// long before any scene loads.
//
// PURE / headless: no disk, no GPU, no entity creation. The ctor needs a
// Zenith_Entity& and OnStart/OnUpdate touch scene + event-dispatcher state, so
// NO ZM_BattleTransition instance is constructed here -- only the `static`
// policy helpers (IsEncounterPayloadValid / IsSceneEligibleForBattle /
// BiomeForScene) are exercised. The live-dispatcher wiring is the windowed
// ZM_BattleEncounterLatch_Test, not here.
//
// SENTINEL NOTE (load-bearing for cases 2 and 4): ZM_SpeciesData.h:342-343
// declares ZM_SPECIES_COUNT then ZM_SPECIES_NONE = ZM_SPECIES_COUNT -- the two
// SHARE a value. ZM_WorldSpec.h:54-55 has the same shape for ZM_SCENE_COUNT /
// ZM_SCENE_NONE. The `>= COUNT` range check is therefore the ONLY clause that
// rejects the NONE sentinel; a separate `!= NONE` clause would be dead code.
//   1. IsEncounterPayloadValid_AcceptsDawnmereFernfawn -- the golden payload.
//   2. IsEncounterPayloadValid_RejectsOutOfRangeSpecies -- range check covers NONE.
//   3. IsEncounterPayloadValid_RejectsZeroAndOverMaxLevel -- inclusive [1, 100].
//   4. IsEncounterPayloadValid_RejectsBattleAndNoneSourceScene -- scene gate applies.
//   5. IsSceneEligibleForBattle_RejectsFrontEndAndBattle -- the two ineligible kinds.
//   6. IsSceneEligibleForBattle_AcceptsTownRouteInterior -- the three eligible kinds.
//   7. BiomeForScene_EverySceneMapsInRange -- total function over ZM_SCENE_ID.
//   8. BiomeForScene_UnknownSceneDefaultsToMeadow -- out-of-range default.
//   9. BiomeForScene_DawnmereIsMeadow -- the pinned starter-town mapping.
//  10. ShouldAcceptBattleEnd_OnlyInBattle -- IN_BATTLE and nothing else.
//  11. OwnsFade_EveryNonIdleStateOwnsTheScreen -- fade ownership == non-idle.
//  12. IsOverworldPausedInState_MatchesPauseWindow -- the exact pause window.
//
// Cases 10-12 pin the state-machine PREDICATES the round trip is built on. They
// walk the whole ZM_BATTLE_TRANSITION_STATE enum rather than spot-checking, so
// INSERTING a state without classifying it is caught here: an unclassified state
// silently defaults to "not IN_BATTLE / not paused", which would either let item
// 4 end a battle that never started or leave the overworld ticking underneath a
// live battle.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Components/ZM_BattleTransition.h"

// ############################################################################
// 1. IsEncounterPayloadValid -- the golden Dawnmere/Fernfawn payload is accepted
// ############################################################################

// The exact payload the windowed latch test forces through the live dispatcher:
// an in-range species, a level inside [1, 100], and a TOWN source scene. If this
// case fails, the whole encounter->battle slice is dead at the first gate.
ZENITH_TEST(ZM_BattleTransition, IsEncounterPayloadValid_AcceptsDawnmereFernfawn)
{
	ZENITH_ASSERT_TRUE(
		ZM_BattleTransition::IsEncounterPayloadValid(
			ZM_SPECIES_FERNFAWN, 5u, ZM_SCENE_DAWNMERE),
		"the golden {FERNFAWN, level 5, DAWNMERE} encounter payload must be accepted");
}

// ############################################################################
// 2. IsEncounterPayloadValid -- an out-of-range species is rejected
// ############################################################################

// ZM_SPECIES_NONE == ZM_SPECIES_COUNT, so the RANGE check (`>= ZM_SPECIES_COUNT`)
// is what rejects the sentinel -- there is deliberately no separate `!= NONE`
// clause, which would be dead code. The rejection must hold at every level, and
// any value past the end of the enum must fail the same way.
ZENITH_TEST(ZM_BattleTransition, IsEncounterPayloadValid_RejectsOutOfRangeSpecies)
{
	// The NONE sentinel is rejected BY THE RANGE CHECK (NONE == COUNT), at a low,
	// a mid, and the maximum legal level -- the species clause alone decides.
	ZENITH_ASSERT_FALSE(
		ZM_BattleTransition::IsEncounterPayloadValid(
			ZM_SPECIES_NONE, 1u, ZM_SCENE_DAWNMERE),
		"ZM_SPECIES_NONE (== ZM_SPECIES_COUNT) must fail the >= COUNT range check at level 1");
	ZENITH_ASSERT_FALSE(
		ZM_BattleTransition::IsEncounterPayloadValid(
			ZM_SPECIES_NONE, 5u, ZM_SCENE_DAWNMERE),
		"ZM_SPECIES_NONE (== ZM_SPECIES_COUNT) must fail the >= COUNT range check at level 5");
	ZENITH_ASSERT_FALSE(
		ZM_BattleTransition::IsEncounterPayloadValid(
			ZM_SPECIES_NONE, 100u, ZM_SCENE_DAWNMERE),
		"ZM_SPECIES_NONE (== ZM_SPECIES_COUNT) must fail the >= COUNT range check at level 100");

	// COUNT itself and anything beyond it are equally out of range.
	ZENITH_ASSERT_FALSE(
		ZM_BattleTransition::IsEncounterPayloadValid(
			ZM_SPECIES_COUNT, 5u, ZM_SCENE_DAWNMERE),
		"ZM_SPECIES_COUNT is not a species and must fail the range check");
	ZENITH_ASSERT_FALSE(
		ZM_BattleTransition::IsEncounterPayloadValid(
			static_cast<ZM_SPECIES_ID>(ZM_SPECIES_COUNT + 1u), 5u, ZM_SCENE_DAWNMERE),
		"a species id past the end of the enum must fail the range check");
}

// ############################################################################
// 3. IsEncounterPayloadValid -- the level band is inclusive [1, 100]
// ############################################################################

// Level 0 is not a legal creature level and 101 is past the cap, so both are
// rejected; the two boundary values 1 and 100 are INSIDE the band and must be
// accepted, which is what makes the comparison inclusive rather than strict.
ZENITH_TEST(ZM_BattleTransition, IsEncounterPayloadValid_RejectsZeroAndOverMaxLevel)
{
	ZENITH_ASSERT_FALSE(
		ZM_BattleTransition::IsEncounterPayloadValid(
			ZM_SPECIES_FERNFAWN, 0u, ZM_SCENE_DAWNMERE),
		"level 0 is below the band and must be rejected");
	ZENITH_ASSERT_FALSE(
		ZM_BattleTransition::IsEncounterPayloadValid(
			ZM_SPECIES_FERNFAWN, 101u, ZM_SCENE_DAWNMERE),
		"level 101 is above the cap and must be rejected");

	ZENITH_ASSERT_TRUE(
		ZM_BattleTransition::IsEncounterPayloadValid(
			ZM_SPECIES_FERNFAWN, 1u, ZM_SCENE_DAWNMERE),
		"level 1 is the inclusive lower bound and must be accepted");
	ZENITH_ASSERT_TRUE(
		ZM_BattleTransition::IsEncounterPayloadValid(
			ZM_SPECIES_FERNFAWN, 100u, ZM_SCENE_DAWNMERE),
		"level 100 is the inclusive upper bound and must be accepted");
}

// ############################################################################
// 4. IsEncounterPayloadValid -- the source-scene gate is applied
// ############################################################################

// The payload validator delegates its scene clause to IsSceneEligibleForBattle,
// so an otherwise-perfect species/level pair must still be rejected when it
// claims to come from the Battle scene (a battle cannot spawn over itself) or
// from the ZM_SCENE_NONE sentinel (which the >= COUNT check catches).
ZENITH_TEST(ZM_BattleTransition, IsEncounterPayloadValid_RejectsBattleAndNoneSourceScene)
{
	ZENITH_ASSERT_FALSE(
		ZM_BattleTransition::IsEncounterPayloadValid(
			ZM_SPECIES_FERNFAWN, 5u, ZM_SCENE_BATTLE),
		"a valid species/level from the BATTLE scene must still be rejected");
	ZENITH_ASSERT_FALSE(
		ZM_BattleTransition::IsEncounterPayloadValid(
			ZM_SPECIES_FERNFAWN, 5u, ZM_SCENE_NONE),
		"a valid species/level from the ZM_SCENE_NONE sentinel must still be rejected "
		"(NONE == ZM_SCENE_COUNT, caught by the range check)");
}

// ############################################################################
// 5. IsSceneEligibleForBattle -- FrontEnd and Battle are the ineligible kinds
// ############################################################################

// ZM_SCENE_KIND_BATTLE cannot spawn a battle over itself, and
// ZM_SCENE_KIND_FRONTEND has no world to pause, park a player in, or resume to.
// These are the only two kinds excluded by design.
ZENITH_TEST(ZM_BattleTransition, IsSceneEligibleForBattle_RejectsFrontEndAndBattle)
{
	ZENITH_ASSERT_FALSE(
		ZM_BattleTransition::IsSceneEligibleForBattle(ZM_SCENE_FRONTEND),
		"the FrontEnd has no world to pause or resume and is not battle-eligible");
	ZENITH_ASSERT_FALSE(
		ZM_BattleTransition::IsSceneEligibleForBattle(ZM_SCENE_BATTLE),
		"the Battle scene cannot spawn a battle over itself");
}

// ############################################################################
// 6. IsSceneEligibleForBattle -- town, route, and interior are all eligible
// ############################################################################

// One scene per remaining real kind that exists in the S1 proving set: Dawnmere
// (TOWN), Route 1 (ROUTE), and the Player's Home (INTERIOR). All three have a
// pausable world with a parkable player, so all three may launch a battle.
ZENITH_TEST(ZM_BattleTransition, IsSceneEligibleForBattle_AcceptsTownRouteInterior)
{
	ZENITH_ASSERT_TRUE(
		ZM_BattleTransition::IsSceneEligibleForBattle(ZM_SCENE_DAWNMERE),
		"Dawnmere (TOWN) is battle-eligible");
	ZENITH_ASSERT_TRUE(
		ZM_BattleTransition::IsSceneEligibleForBattle(ZM_SCENE_ROUTE1),
		"Route 1 (ROUTE) is battle-eligible");
	ZENITH_ASSERT_TRUE(
		ZM_BattleTransition::IsSceneEligibleForBattle(ZM_SCENE_PLAYERHOME),
		"the Player's Home (INTERIOR) is battle-eligible");
}

// ############################################################################
// 7. BiomeForScene -- a total function: every scene maps to a real biome
// ############################################################################

// The mapping table is indexed by ZM_SCENE_ID, so appending a scene to the world
// table without appending a row would either read past the table or return a
// garbage biome. Walking the whole enum pins BiomeForScene as TOTAL: every id in
// [0, ZM_SCENE_COUNT) yields a biome strictly inside [0, ZM_BATTLE_BIOME_COUNT).
ZENITH_TEST(ZM_BattleTransition, BiomeForScene_EverySceneMapsInRange)
{
	for (u_int u = 0; u < ZM_SCENE_COUNT; ++u)
	{
		const ZM_BATTLE_BIOME eBiome =
			ZM_BattleTransition::BiomeForScene(static_cast<ZM_SCENE_ID>(u));
		ZENITH_ASSERT_TRUE(eBiome < ZM_BATTLE_BIOME_COUNT,
			"every ZM_SCENE_ID must map to a biome inside [0, ZM_BATTLE_BIOME_COUNT)");
	}
}

// ############################################################################
// 8. BiomeForScene -- an unknown / out-of-range scene falls back to MEADOW
// ############################################################################

// ZM_SCENE_NONE == ZM_SCENE_COUNT, i.e. one past the last table row, so it takes
// the out-of-range branch. The fallback must be a real, renderable biome rather
// than an out-of-range read: an unmapped scene gets MEADOW dressing, never junk.
ZENITH_TEST(ZM_BattleTransition, BiomeForScene_UnknownSceneDefaultsToMeadow)
{
	ZENITH_ASSERT_EQ(
		(u_int)ZM_BattleTransition::BiomeForScene(ZM_SCENE_NONE),
		(u_int)ZM_BATTLE_BIOME_MEADOW,
		"the ZM_SCENE_NONE sentinel (== ZM_SCENE_COUNT) must fall back to MEADOW");
}

// ############################################################################
// 9. BiomeForScene -- Dawnmere is the pinned MEADOW mapping
// ############################################################################

// The starter town's mapping is the one the windowed slice actually exercises,
// so it is pinned explicitly rather than left to the out-of-range fallback: a
// row-order slip in the table that silently re-pointed Dawnmere at another
// biome would still pass case 7's in-range check but fails here.
ZENITH_TEST(ZM_BattleTransition, BiomeForScene_DawnmereIsMeadow)
{
	ZENITH_ASSERT_EQ(
		(u_int)ZM_BattleTransition::BiomeForScene(ZM_SCENE_DAWNMERE),
		(u_int)ZM_BATTLE_BIOME_MEADOW,
		"Dawnmere must dress the arena with the MEADOW set");
}

// ############################################################################
// 10. ShouldAcceptBattleEnd -- IN_BATTLE is the ONLY state that may be ended
// ############################################################################

// RequestBattleEnd() is the SOLE exit from IN_BATTLE -- there is deliberately no
// battle timer -- so this predicate is the whole gate protecting the round trip
// from item 4. Item 4 may only end a battle that is ACTUALLY RUNNING: accepting
// the request one state early (FADING_IN) would tear down an arena the player has
// not seen yet, and one state late (FADING_TO_OVERWORLD) would double-count the
// resume. The loop walks every enumerator so a newly inserted state is classified
// deliberately rather than defaulting to "not endable" unnoticed.
ZENITH_TEST(ZM_BattleTransition, ShouldAcceptBattleEnd_OnlyInBattle)
{
	for (u_int u = ZM_BATTLE_TRANSITION_IDLE;
		u <= ZM_BATTLE_TRANSITION_RESUME_FADING_IN;
		++u)
	{
		const ZM_BATTLE_TRANSITION_STATE eState =
			static_cast<ZM_BATTLE_TRANSITION_STATE>(u);
		const bool bExpected = (eState == ZM_BATTLE_TRANSITION_IN_BATTLE);
		ZENITH_ASSERT_EQ(
			(u_int)ZM_BattleTransition::ShouldAcceptBattleEnd(eState),
			(u_int)bExpected,
			"ShouldAcceptBattleEnd must be true for IN_BATTLE and false for every "
			"other state (state %u)", u);
	}
}

// ############################################################################
// 11. OwnsFade -- every non-idle state owns the screen
// ############################################################################

// OwnsFade is what OnUpdate consults when the persistent root has LOST its
// authored BattleFade overlay: an owning state is slammed opaque so a broken
// overlay can never reveal a half-built arena or a half-restored overworld. IDLE
// is the only state with nothing to hide, so it must NOT claim the fade -- were
// it to, an idle machine would blacken the screen for a fault it is not part of.
ZENITH_TEST(ZM_BattleTransition, OwnsFade_EveryNonIdleStateOwnsTheScreen)
{
	for (u_int u = ZM_BATTLE_TRANSITION_IDLE;
		u <= ZM_BATTLE_TRANSITION_RESUME_FADING_IN;
		++u)
	{
		const ZM_BATTLE_TRANSITION_STATE eState =
			static_cast<ZM_BATTLE_TRANSITION_STATE>(u);
		const bool bExpected = (eState != ZM_BATTLE_TRANSITION_IDLE);
		ZENITH_ASSERT_EQ(
			(u_int)ZM_BattleTransition::OwnsFade(eState),
			(u_int)bExpected,
			"every non-idle state owns the fade; IDLE owns nothing (state %u)", u);
	}
}

// ############################################################################
// 12. IsOverworldPausedInState -- the pause window is exactly ENTERING..RESUMING
// ############################################################################

// The pause window is bounded on BOTH sides for concrete reasons. It opens no
// earlier than ENTERING because FADING_OUT and WAITING_FOR_SCENE still need the
// live overworld -- the player is parked from the active scene during ENTERING,
// and a scene paused before that would never dispatch. It closes at RESUMING
// (which unpauses) rather than at RESUME_FADING_IN, so the world is already
// ticking again by the time the screen starts to reveal it. IDLE is trivially
// unpaused. Each state is named explicitly rather than derived, so this case
// disagrees with the implementation on drift instead of restating it.
ZENITH_TEST(ZM_BattleTransition, IsOverworldPausedInState_MatchesPauseWindow)
{
	ZENITH_ASSERT_FALSE(
		ZM_BattleTransition::IsOverworldPausedInState(ZM_BATTLE_TRANSITION_IDLE),
		"IDLE owns no overworld and must not report it paused");
	ZENITH_ASSERT_FALSE(
		ZM_BattleTransition::IsOverworldPausedInState(ZM_BATTLE_TRANSITION_FADING_OUT),
		"FADING_OUT still runs the live overworld under the fade");
	ZENITH_ASSERT_FALSE(
		ZM_BattleTransition::IsOverworldPausedInState(
			ZM_BATTLE_TRANSITION_WAITING_FOR_SCENE),
		"WAITING_FOR_SCENE has not yet parked the player, so the overworld must tick");
	ZENITH_ASSERT_FALSE(
		ZM_BattleTransition::IsOverworldPausedInState(
			ZM_BATTLE_TRANSITION_RESUME_FADING_IN),
		"RESUMING already unpaused the overworld before the reveal fade");

	ZENITH_ASSERT_TRUE(
		ZM_BattleTransition::IsOverworldPausedInState(ZM_BATTLE_TRANSITION_ENTERING),
		"ENTERING pauses the overworld once the player is parked");
	ZENITH_ASSERT_TRUE(
		ZM_BattleTransition::IsOverworldPausedInState(ZM_BATTLE_TRANSITION_FADING_IN),
		"FADING_IN reveals the battle with the overworld paused");
	ZENITH_ASSERT_TRUE(
		ZM_BattleTransition::IsOverworldPausedInState(ZM_BATTLE_TRANSITION_IN_BATTLE),
		"IN_BATTLE must never tick the overworld underneath the battle");
	ZENITH_ASSERT_TRUE(
		ZM_BattleTransition::IsOverworldPausedInState(
			ZM_BATTLE_TRANSITION_FADING_TO_OVERWORLD),
		"FADING_TO_OVERWORLD is still inside the pause window");
	ZENITH_ASSERT_TRUE(
		ZM_BattleTransition::IsOverworldPausedInState(ZM_BATTLE_TRANSITION_RESUMING),
		"RESUMING is the state that performs the unpause, so it is still in-window");
}
