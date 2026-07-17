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
